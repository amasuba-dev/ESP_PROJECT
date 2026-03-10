#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "esp_sleep.h"
#include "nvs_flash.h"
#include "driver/i2c_master.h"
#include "esp_adc/adc_oneshot.h"
#include <string.h>
#include <stdio.h>

#include "sensors/temperature.h"
#include "sensors/current.h"
#include "sensors/mpu6050.h"
#include "sensors/reed_switch.h"
#include "modem/sim7670x.h"
#include "portal/portal.h"
#include "sdkconfig.h"

static const char *TAG = "app_main";

/* ── SAST schedule ──────────────────────────────────────────────────────────
 * SAST = UTC+2.  Modem provides UTC via AT+CCLK.
 * Wake:  06:00 SAST = hour 6  in SAST
 * Sleep: 18:00 SAST = hour 18 in SAST
 * ─────────────────────────────────────────────────────────────────────────*/
#define SAST_OFFSET_H   2
#define DAY_START_H     6    /* wake  at 06:00 SAST */
#define DAY_END_H       18   /* sleep at 18:00 SAST */

typedef struct {
    int year, month, day, hour, min, sec;
    bool valid;
} utc_time_t;

/**
 * Parse AT+CCLK response: +CCLK: "yy/MM/dd,hh:mm:ss±zz"
 * Fills UTC time (modem usually reports local + offset; we use the raw
 * hh:mm:ss field and subtract the TZ offset the modem embedded).
 * Simplification: configure modem APN with UTC (AT+CTZU=1) so the modem
 * stores UTC directly, removing the need for manual offset parsing.
 */
static utc_time_t parse_cclk(const char *resp)
{
    utc_time_t t = {0};
    const char *p = strstr(resp, "+CCLK:");
    if (!p) return t;
    /* format: +CCLK: "26/03/10,14:20:05+08"  (yy/MM/dd,hh:mm:ss±tz) */
    int yy, mo, dd, hh, mm, ss, tz = 0;
    if (sscanf(p, "+CCLK: \"%d/%d/%d,%d:%d:%d%d\"",
               &yy, &mo, &dd, &hh, &mm, &ss, &tz) >= 6) {
        /* tz is in quarter-hours; subtract to get UTC */
        int total_min = hh * 60 + mm - (tz * 15);
        if (total_min < 0)        total_min += 1440;
        if (total_min >= 1440)    total_min -= 1440;
        t.hour  = total_min / 60;
        t.min   = total_min % 60;
        t.sec   = ss;
        t.year  = 2000 + yy;
        t.month = mo;
        t.day   = dd;
        t.valid = true;
    }
    return t;
}

/** Returns true if current SAST hour is within daytime [DAY_START_H, DAY_END_H). */
static bool is_daytime(const utc_time_t *utc)
{
    if (!utc->valid) return true;   /* operate normally if time unknown */
    int sast_hour = (utc->hour + SAST_OFFSET_H) % 24;
    return (sast_hour >= DAY_START_H && sast_hour < DAY_END_H);
}

/**
 * Calculate deep-sleep duration in microseconds to reach next 06:00 SAST.
 * Called just before sleep at 18:00 SAST, so sleep = 12 h exactly.
 * We compute the precise remaining seconds to be safe against boot delay.
 */
static uint64_t seconds_until_wake(const utc_time_t *utc)
{
    if (!utc->valid) {
        /* Fallback: sleep 12 hours */
        return 12ULL * 3600ULL;
    }
    int sast_hour = (utc->hour + SAST_OFFSET_H) % 24;
    int sast_min  = utc->min;
    int sast_sec  = utc->sec;

    /* Current seconds since midnight SAST */
    int now_s = sast_hour * 3600 + sast_min * 60 + sast_sec;

    /* Target: next 06:00 SAST = 6*3600 seconds */
    int target_s = DAY_START_H * 3600;

    int diff_s = target_s - now_s;
    if (diff_s <= 0) diff_s += 86400;   /* wrap to next day */
    return (uint64_t)diff_s;
}

/* ── Reed switch ISR ────────────────────────────────────────────────────── */
static void on_reed_event(bool state, void *arg)
{
    /* ISR context — minimal work */
    (void)arg; (void)state;
}

/* ── Read all sensors into data struct ─────────────────────────────────── */
static void read_all_sensors(sensor_data_t *data)
{
    data->timestamp_ms = esp_timer_get_time() / 1000LL;
    temperature_read(&data->temperature);
    current_read(&data->current);
    mpu6050_read(&data->accelerometer);
    data->reed_pulse_count = reed_switch_get_pulse_count();
    data->reed_state       = reed_switch_get_state();
    sim7670x_get_signal_quality(&data->signal_rssi_dbm);
}

/* ── app_main ───────────────────────────────────────────────────────────── */
void app_main(void)
{
    ESP_LOGI(TAG, "=== ESP Project starting (device: %s) ===", CONFIG_DEVICE_ID);

    /* ── NVS ──────────────────────────────────────────────────────────── */
    esp_err_t nvs = nvs_flash_init();
    if (nvs == ESP_ERR_NVS_NO_FREE_PAGES || nvs == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ESP_ERROR_CHECK(nvs_flash_init());
    }

    /* ── Shared ADC1 ──────────────────────────────────────────────────── */
    adc_oneshot_unit_handle_t adc1;
    adc_oneshot_unit_init_cfg_t adc_cfg = { .unit_id = ADC_UNIT_1 };
    ESP_ERROR_CHECK(adc_oneshot_new_unit(&adc_cfg, &adc1));

    /* ── Shared I2C bus ───────────────────────────────────────────────── */
    i2c_master_bus_config_t i2c_cfg = {
        .i2c_port                     = I2C_NUM_0,
        .sda_io_num                   = CONFIG_I2C_SDA_GPIO,
        .scl_io_num                   = CONFIG_I2C_SCL_GPIO,
        .clk_source                   = I2C_CLK_SRC_DEFAULT,
        .glitch_ignore_cnt            = 7,
        .flags.enable_internal_pullup = true,
    };
    i2c_master_bus_handle_t i2c_bus;
    ESP_ERROR_CHECK(i2c_new_master_bus(&i2c_cfg, &i2c_bus));

    /* ── Sensors ──────────────────────────────────────────────────────── */
    ESP_ERROR_CHECK(temperature_init(adc1));
    ESP_ERROR_CHECK(current_init(adc1));
    ESP_ERROR_CHECK(mpu6050_init(i2c_bus));
    ESP_ERROR_CHECK(reed_switch_init(CONFIG_REED_SWITCH_GPIO, on_reed_event, NULL));

    /* ── 4G modem + portal ────────────────────────────────────────────── */
    ESP_ERROR_CHECK(sim7670x_init());
    ESP_ERROR_CHECK(portal_init());

    ESP_LOGI(TAG, "All components ready. Interval: %d ms", CONFIG_MEASURE_INTERVAL_MS);

    /* ── Get network time for scheduling ─────────────────────────────── */
    char time_resp[128] = {0};
    sim7670x_get_network_time(time_resp, sizeof(time_resp));
    utc_time_t utc = parse_cclk(time_resp);
    if (utc.valid) {
        int sast_h = (utc.hour + SAST_OFFSET_H) % 24;
        ESP_LOGI(TAG, "Network time: UTC %02d:%02d:%02d  →  SAST %02d:%02d:%02d",
                 utc.hour, utc.min, utc.sec, sast_h, utc.min, utc.sec);
    } else {
        ESP_LOGW(TAG, "Network time unavailable – running without sleep scheduling");
    }

    /* ── Wake-up status message ──────────────────────────────────────── */
    sensor_data_t data = {0};
    read_all_sensors(&data);
    data.event = PORTAL_EVENT_WAKE;
    portal_send(&data);
    reed_switch_reset_pulse_count();

    /* ── Main daytime loop ───────────────────────────────────────────── */
    while (1) {
        /* Refresh time every cycle for accurate sleep decision */
        memset(time_resp, 0, sizeof(time_resp));
        sim7670x_get_network_time(time_resp, sizeof(time_resp));
        utc = parse_cclk(time_resp);

        if (!is_daytime(&utc)) {
            /* ── Send sleep status, then enter deep sleep ──────────── */
            ESP_LOGI(TAG, "18:00 SAST reached – sending sleep update");
            memset(&data, 0, sizeof(data));
            read_all_sensors(&data);
            data.event = PORTAL_EVENT_SLEEP;
            portal_send(&data);

            uint64_t sleep_s  = seconds_until_wake(&utc);
            uint64_t sleep_us = sleep_s * 1000000ULL;
            ESP_LOGI(TAG, "Entering deep sleep for %llu s (wake at 06:00 SAST)",
                     (unsigned long long)sleep_s);

            esp_sleep_enable_timer_wakeup(sleep_us);
            esp_deep_sleep_start();
            /* ── Does not return ── */
        }

        /* ── Daytime measurement ───────────────────────────────────── */
        memset(&data, 0, sizeof(data));
        read_all_sensors(&data);
        data.event = PORTAL_EVENT_DATA;

        int sast_h = utc.valid ? (utc.hour + SAST_OFFSET_H) % 24 : -1;
        ESP_LOGI(TAG,
                 "[SAST ~%02d:%02d]  T=%.2f°C  I=%.3fA  P=%.1fW  "
                 "E=%.4fkWh  Reed=%lu  RSSI=%ddBm",
                 sast_h, utc.min,
                 data.temperature.temperature_c,
                 data.current.irms_a,
                 data.current.power_w,
                 data.current.energy_kwh,
                 (unsigned long)data.reed_pulse_count,
                 data.signal_rssi_dbm);

        esp_err_t ret = portal_send(&data);
        if (ret != ESP_OK) {
            ESP_LOGW(TAG, "Portal send failed – will retry next cycle");
        }

        vTaskDelay(pdMS_TO_TICKS(CONFIG_MEASURE_INTERVAL_MS));
    }
}
