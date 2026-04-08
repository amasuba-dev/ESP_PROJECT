#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "esp_sleep.h"
#include "nvs_flash.h"
#include "driver/i2c_master.h"
#include "driver/gpio.h"
#include "esp_adc/adc_oneshot.h"
#include <string.h>
#include <stdio.h>

#include "sensors/temperature.h"
#include "sensors/current.h"
#include "sensors/mpu6050.h"
#include "sensors/reed_switch.h"
#include "sensors/ds18b20.h"
#include "sensors/phototransistor.h"
#include "modem/sim7670x.h"
#include "portal/portal.h"
#include "sdkconfig.h"

static const char *TAG = "app_main";
static bool s_modem_ready = false;

/* ── SAST schedule ──────────────────────────────────────────────────────────
 * SAST = UTC+2.  Modem provides UTC via AT+CCLK.
 * Wake:  06:00 SAST = hour 6  in SAST
 * Sleep: 18:00 SAST = hour 18 in SAST
 * ─────────────────────────────────────────────────────────────────────────*/
#define SAST_OFFSET_H 2
#define DAY_START_H 6 /* wake  at 06:00 SAST */
#define DAY_END_H 18  /* sleep at 18:00 SAST */

typedef struct
{
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
    if (!p)
        return t;
    /* format: +CCLK: "26/03/10,14:20:05+08"  (yy/MM/dd,hh:mm:ss±tz) */
    int yy, mo, dd, hh, mm, ss, tz = 0;
    if (sscanf(p, "+CCLK: \"%d/%d/%d,%d:%d:%d%d\"",
               &yy, &mo, &dd, &hh, &mm, &ss, &tz) >= 6)
    {
        /* tz is in quarter-hours; subtract to get UTC */
        int total_min = hh * 60 + mm - (tz * 15);
        if (total_min < 0)
            total_min += 1440;
        if (total_min >= 1440)
            total_min -= 1440;
        t.hour = total_min / 60;
        t.min = total_min % 60;
        t.sec = ss;
        t.year = 2000 + yy;
        t.month = mo;
        t.day = dd;
        t.valid = true;
    }
    return t;
}

/** Returns true if current SAST hour is within daytime [DAY_START_H, DAY_END_H). */
#if CONFIG_SLEEP_ENABLED
static bool is_daytime(const utc_time_t *utc)
{
    if (!utc->valid)
        return true; /* operate normally if time unknown */
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
    if (!utc->valid)
    {
        /* Fallback: sleep 12 hours */
        return 12ULL * 3600ULL;
    }
    int sast_hour = (utc->hour + SAST_OFFSET_H) % 24;
    int sast_min = utc->min;
    int sast_sec = utc->sec;

    /* Current seconds since midnight SAST */
    int now_s = sast_hour * 3600 + sast_min * 60 + sast_sec;

    /* Target: next 06:00 SAST = 6*3600 seconds */
    int target_s = DAY_START_H * 3600;

    int diff_s = target_s - now_s;
    if (diff_s <= 0)
        diff_s += 86400; /* wrap to next day */
    return (uint64_t)diff_s;
}
#endif /* CONFIG_SLEEP_ENABLED */

/* ── Reed switch ISR ────────────────────────────────────────────────────── */
static void on_reed_event(bool state, void *arg)
{
    /* ISR context — minimal work */
    (void)arg;
    (void)state;
}

/* ── Read all sensors into data struct ─────────────────────────────────── */
static void read_all_sensors(sensor_data_t *data)
{
    data->timestamp_ms = esp_timer_get_time() / 1000LL;
    temperature_read(&data->temperature);
    current_read(&data->current);
    mpu6050_read(&data->accelerometer);
    ds18b20_read(&data->ds18b20);
    phototransistor_read(&data->phototransistor);
    data->reed_pulse_count = reed_switch_get_pulse_count();
    data->reed_state = reed_switch_get_state();
    if (s_modem_ready)
    {
        sim7670x_get_signal_quality(&data->signal_rssi_dbm);
    }
    else
    {
        data->signal_rssi_dbm = -999;
    }
}

/* ── app_main ───────────────────────────────────────────────────────────── */
void app_main(void)
{
    ESP_LOGI(TAG, "=== ESP Project starting (device: %s) ===", CONFIG_DEVICE_ID);

    /* ── Wake-up cause ────────────────────────────────────────────────── */
    esp_sleep_wakeup_cause_t wakeup_cause = esp_sleep_get_wakeup_cause();
    switch (wakeup_cause)
    {
    case ESP_SLEEP_WAKEUP_TIMER:
        ESP_LOGI(TAG, "Wake-up cause: scheduled timer (06:00 SAST)");
        break;
    case ESP_SLEEP_WAKEUP_EXT1:
        ESP_LOGI(TAG, "Wake-up cause: reed switch (ext1 GPIO event)");
        break;
    default:
        ESP_LOGI(TAG, "Wake-up cause: power-on / reset");
        break;
    }

    /* ── NVS ──────────────────────────────────────────────────────────── */
    esp_err_t nvs = nvs_flash_init();
    if (nvs == ESP_ERR_NVS_NO_FREE_PAGES || nvs == ESP_ERR_NVS_NEW_VERSION_FOUND)
    {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ESP_ERROR_CHECK(nvs_flash_init());
    }

    /* ── Shared ADC1 ──────────────────────────────────────────────────── */
    adc_oneshot_unit_handle_t adc1;
    adc_oneshot_unit_init_cfg_t adc_cfg = {.unit_id = ADC_UNIT_1};
    ESP_ERROR_CHECK(adc_oneshot_new_unit(&adc_cfg, &adc1));

    /* ── Shared I2C bus ───────────────────────────────────────────────── */
    i2c_master_bus_config_t i2c_bus_cfg = {
        .i2c_port = I2C_NUM_0,
        .sda_io_num = CONFIG_I2C_SDA_GPIO,
        .scl_io_num = CONFIG_I2C_SCL_GPIO,
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .glitch_ignore_cnt = 7,
        .flags.enable_internal_pullup = true,
    };
    i2c_master_bus_handle_t i2c_bus;
    ESP_ERROR_CHECK(i2c_new_master_bus(&i2c_bus_cfg, &i2c_bus));

    /* ── Wait for sensors to power up (MPU6050 etc.) ────────────────── */
    ESP_LOGI(TAG, "Waiting 1 s for sensors to power up...");
    vTaskDelay(pdMS_TO_TICKS(1000));

    /* ── Sensors ──────────────────────────────────────────────────────── */
    ESP_ERROR_CHECK(temperature_init(adc1));
    ESP_ERROR_CHECK(current_init(adc1));

    /* Warm up CT sensor – first ADC reads are unreliable (as in Arduino code) */
    ESP_LOGI(TAG, "Warming up CT sensor...");
    {
        current_reading_t discard;
        for (int i = 0; i < 5; i++)
        {
            current_read(&discard);
            vTaskDelay(pdMS_TO_TICKS(100));
        }
    }
    ESP_LOGI(TAG, "CT sensor ready");

    esp_err_t mpu_ret = mpu6050_init(i2c_bus);
    if (mpu_ret != ESP_OK)
    {
        ESP_LOGW(TAG, "MPU6050 init failed – accelerometer/gyroscope data will be unavailable");
    }
    ESP_ERROR_CHECK(reed_switch_init(CONFIG_REED_SWITCH_GPIO, on_reed_event, NULL));

    /* ── DS18B20 temperature sensor (1-Wire) ─────────────────────────── */
    esp_err_t ds_ret = ds18b20_init(CONFIG_DS18B20_GPIO);
    if (ds_ret != ESP_OK)
    {
        ESP_LOGW(TAG, "DS18B20 init failed – 1-Wire temperature will be unavailable");
    }

    /* ── Phototransistor (BPW85C via ADC) ─────────────────────────────── */
    ESP_ERROR_CHECK(phototransistor_init(adc1));

    /* ── 4G modem + portal ────────────────────────────────────────────── */
    esp_err_t modem_ret = sim7670x_init();
    if (modem_ret == ESP_OK)
    {
        esp_err_t portal_ret = portal_init();
        s_modem_ready = (portal_ret == ESP_OK);
        if (!s_modem_ready)
        {
            ESP_LOGW(TAG, "Portal init failed; continuing without modem uplink");
        }
    }
    else
    {
        s_modem_ready = false;
        ESP_LOGW(TAG, "Modem init failed; continuing without modem uplink");
    }

    ESP_LOGI(TAG, "All components ready. Interval: %d ms", CONFIG_MEASURE_INTERVAL_MS);

    /* ── Get network time for scheduling ─────────────────────────────── */
    char time_resp[128] = {0};
    if (s_modem_ready)
    {
        sim7670x_get_network_time(time_resp, sizeof(time_resp));
    }
    utc_time_t utc = parse_cclk(time_resp);
    if (utc.valid)
    {
        int sast_h = (utc.hour + SAST_OFFSET_H) % 24;
        ESP_LOGI(TAG, "Network time: UTC %02d:%02d:%02d  →  SAST %02d:%02d:%02d",
                 utc.hour, utc.min, utc.sec, sast_h, utc.min, utc.sec);
    }
    else
    {
        ESP_LOGW(TAG, "Network time unavailable – running without sleep scheduling");
    }

    /* ── Wake-up status message ──────────────────────────────────────── */
    sensor_data_t data = {0};
    read_all_sensors(&data);
    data.event = PORTAL_EVENT_WAKE;
    if (s_modem_ready)
    {
        portal_send(&data);
    }
    reed_switch_reset_pulse_count();

    /* ── Initial delay to stabilize ──────────────────────────────────── */
    ESP_LOGI(TAG, "Waiting 10 seconds before starting main loop...");
    vTaskDelay(pdMS_TO_TICKS(10000));

    /* ── Main daytime loop ───────────────────────────────────────────── */
    while (1)
    {

        /* Refresh time every cycle for accurate sleep decision */
        memset(time_resp, 0, sizeof(time_resp));
        if (s_modem_ready)
        {
            sim7670x_get_network_time(time_resp, sizeof(time_resp));
        }
        utc = parse_cclk(time_resp);

        /* ── Sleep scheduling ─────────────────────────────────────────
         * When CONFIG_SLEEP_ENABLED: enter deep sleep at 18:00 SAST,
         * wake at 06:00 SAST via timer. Reed switch ext1 can wake early.
         * ──────────────────────────────────────────────────────────── */
#if CONFIG_SLEEP_ENABLED
        if (!is_daytime(&utc))
        {
            ESP_LOGI(TAG, "18:00 SAST reached – sending sleep update");
            memset(&data, 0, sizeof(data));
            read_all_sensors(&data);
            data.event = PORTAL_EVENT_SLEEP;
            if (s_modem_ready)
            {
                portal_send(&data);
            }

            uint64_t sleep_s = seconds_until_wake(&utc);
            uint64_t sleep_us = sleep_s * 1000000ULL;
            ESP_LOGI(TAG, "Entering deep sleep for %llu s (wake at 06:00 SAST)",
                     (unsigned long long)sleep_s);

            /* Timer wake: scheduled 06:00 SAST alarm */
            esp_sleep_enable_timer_wakeup(sleep_us);

#if CONFIG_SLEEP_WAKE_ON_REED
            /* Ext1 wake: reed switch GPIO (active-low → wake on LOW level).
             * Skip if pin is already LOW to avoid immediate re-wake. */
            if (gpio_get_level(CONFIG_REED_SWITCH_GPIO) == 1)
            {
                esp_sleep_enable_ext1_wakeup(1ULL << CONFIG_REED_SWITCH_GPIO,
                                             ESP_EXT1_WAKEUP_ALL_LOW);
                ESP_LOGI(TAG, "Ext1 wake enabled on GPIO%d (reed switch)", CONFIG_REED_SWITCH_GPIO);
            }
            else
            {
                ESP_LOGW(TAG, "Reed switch already LOW – ext1 wake skipped to avoid boot loop");
            }
#endif /* CONFIG_SLEEP_WAKE_ON_REED */

            esp_deep_sleep_start();
            /* Does not return – device restarts from app_main on wake */
        }
#endif /* CONFIG_SLEEP_ENABLED */

        /* ── Measurement ───────────────────────────────────────────── */
        memset(&data, 0, sizeof(data));
        read_all_sensors(&data);
        data.event = PORTAL_EVENT_DATA;

        int sast_h = utc.valid ? (utc.hour + SAST_OFFSET_H) % 24 : -1;

        ESP_LOGI(TAG, "========================================");
        ESP_LOGI(TAG, "  SENSOR READINGS  [SAST ~%02d:%02d]", sast_h, utc.min);
        ESP_LOGI(TAG, "========================================");

        ESP_LOGI(TAG, "--- TEMPERATURE ---");
        ESP_LOGI(TAG, "  Value : %.2f C", data.temperature.temperature_c);
        ESP_LOGI(TAG, "  Valid : %s", data.temperature.valid ? "YES" : "NO");

        ESP_LOGI(TAG, "--- CURRENT SENSOR (SCT013) ---");
        ESP_LOGI(TAG, "  Irms    : %.3f A", data.current.irms_a);
        ESP_LOGI(TAG, "  Power   : %.1f W", data.current.power_w);
        ESP_LOGI(TAG, "  Energy  : %.4f kWh", data.current.energy_kwh);
        ESP_LOGI(TAG, "  Valid   : %s", data.current.valid ? "YES" : "NO");

        ESP_LOGI(TAG, "--- ACCELEROMETER (MPU6050) ---");
        ESP_LOGI(TAG, "  Accel X : %+.3f m/s2", data.accelerometer.accel_x);
        ESP_LOGI(TAG, "  Accel Y : %+.3f m/s2", data.accelerometer.accel_y);
        ESP_LOGI(TAG, "  Accel Z : %+.3f m/s2", data.accelerometer.accel_z);

        ESP_LOGI(TAG, "--- GYROSCOPE (MPU6050) ---");
        ESP_LOGI(TAG, "  Gyro  X : %+.2f deg/s", data.accelerometer.gyro_x);
        ESP_LOGI(TAG, "  Gyro  Y : %+.2f deg/s", data.accelerometer.gyro_y);
        ESP_LOGI(TAG, "  Gyro  Z : %+.2f deg/s", data.accelerometer.gyro_z);
        ESP_LOGI(TAG, "  MPU Temp: %.1f C", data.accelerometer.temperature_c);
        ESP_LOGI(TAG, "  Valid   : %s", data.accelerometer.valid ? "YES" : "NO");

        ESP_LOGI(TAG, "--- REED SWITCH ---");
        ESP_LOGI(TAG, "  State  : %s", data.reed_state ? "OPEN" : "CLOSED");
        ESP_LOGI(TAG, "  Pulses : %lu", (unsigned long)data.reed_pulse_count);

        ESP_LOGI(TAG, "--- DS18B20 TEMPERATURE ---");
        ESP_LOGI(TAG, "  Value : %.2f C  (%.2f F)",
                 data.ds18b20.temperature_c,
                 data.ds18b20.temperature_c * 9.0f / 5.0f + 32.0f);
        ESP_LOGI(TAG, "  Valid : %s", data.ds18b20.valid ? "YES" : "NO");

        ESP_LOGI(TAG, "--- PHOTOTRANSISTOR (BPW85C) ---");
        ESP_LOGI(TAG, "  Raw ADC : %d", data.phototransistor.raw);
        ESP_LOGI(TAG, "  Voltage : %.3f V", data.phototransistor.voltage_v);
        ESP_LOGI(TAG, "  Valid   : %s", data.phototransistor.valid ? "YES" : "NO");

        ESP_LOGI(TAG, "--- 4G SIGNAL ---");
        ESP_LOGI(TAG, "  RSSI : %d dBm  (%s)",
                 data.signal_rssi_dbm,
                 data.signal_rssi_dbm > -80 ? "EXCELLENT" : data.signal_rssi_dbm > -90 ? "GOOD"
                                                        : data.signal_rssi_dbm > -100  ? "FAIR"
                                                                                       : "POOR");
        ESP_LOGI(TAG, "========================================");

        if (s_modem_ready)
        {
            esp_err_t ret = portal_send(&data);
            if (ret != ESP_OK)
            {
                ESP_LOGW(TAG, "Portal send failed – will retry next cycle");
            }
        }
        else
        {
            ESP_LOGW(TAG, "Modem/portal unavailable; skipping uplink this cycle");
        }

        ESP_LOGI(TAG, "Next reading in %d ms...\n", CONFIG_MEASURE_INTERVAL_MS);
        vTaskDelay(pdMS_TO_TICKS(CONFIG_MEASURE_INTERVAL_MS));
    }
}
