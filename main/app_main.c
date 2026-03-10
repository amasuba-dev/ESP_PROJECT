#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "nvs_flash.h"
#include "driver/i2c_master.h"
#include "esp_adc/adc_oneshot.h"

#include "sensors/temperature.h"
#include "sensors/current.h"
#include "sensors/mpu6050.h"
#include "sensors/reed_switch.h"
#include "modem/sim7670x.h"
#include "portal/portal.h"
#include "sdkconfig.h"

static const char *TAG = "app_main";

static void on_reed_event(bool state, void *arg)
{
    /* Called from ISR – keep it short */
    (void)arg;
    /* state: true=open, false=closed */
}

void app_main(void)
{
    ESP_LOGI(TAG, "=== ESP Project starting (device: %s) ===", CONFIG_DEVICE_ID);

    /* ── NVS ───────────────────────────────────────────────────────────── */
    esp_err_t nvs = nvs_flash_init();
    if (nvs == ESP_ERR_NVS_NO_FREE_PAGES || nvs == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ESP_ERROR_CHECK(nvs_flash_init());
    }

    /* ── Shared ADC1 handle (temperature + current sensors) ────────────── */
    adc_oneshot_unit_handle_t adc1;
    adc_oneshot_unit_init_cfg_t adc_cfg = { .unit_id = ADC_UNIT_1 };
    ESP_ERROR_CHECK(adc_oneshot_new_unit(&adc_cfg, &adc1));

    /* ── Shared I2C bus (MPU-6050, OLED if added later) ────────────────── */
    i2c_master_bus_config_t i2c_cfg = {
        .i2c_port               = I2C_NUM_0,
        .sda_io_num             = CONFIG_I2C_SDA_GPIO,
        .scl_io_num             = CONFIG_I2C_SCL_GPIO,
        .clk_source             = I2C_CLK_SRC_DEFAULT,
        .glitch_ignore_cnt      = 7,
        .flags.enable_internal_pullup = true,
    };
    i2c_master_bus_handle_t i2c_bus;
    ESP_ERROR_CHECK(i2c_new_master_bus(&i2c_cfg, &i2c_bus));

    /* ── Sensors ────────────────────────────────────────────────────────── */
    ESP_ERROR_CHECK(temperature_init(adc1));
    ESP_ERROR_CHECK(current_init(adc1));
    ESP_ERROR_CHECK(mpu6050_init(i2c_bus));
    ESP_ERROR_CHECK(reed_switch_init(CONFIG_REED_SWITCH_GPIO, on_reed_event, NULL));

    /* ── 4G modem + portal ──────────────────────────────────────────────── */
    ESP_ERROR_CHECK(sim7670x_init());
    ESP_ERROR_CHECK(portal_init());

    ESP_LOGI(TAG, "All components ready. Measurement interval: %d ms",
             CONFIG_MEASURE_INTERVAL_MS);

    /* ── Main measurement loop ──────────────────────────────────────────── */
    while (1) {
        sensor_data_t data = {0};
        data.timestamp_ms = esp_timer_get_time() / 1000LL;

        temperature_read(&data.temperature);
        current_read(&data.current);
        mpu6050_read(&data.accelerometer);
        data.reed_pulse_count = reed_switch_get_pulse_count();
        data.reed_state       = reed_switch_get_state();
        sim7670x_get_signal_quality(&data.signal_rssi_dbm);

        ESP_LOGI(TAG,
                 "T=%.2f°C  I=%.3fA  P=%.1fW  E=%.4fkWh  "
                 "Reed=%lu pulses  RSSI=%ddBm",
                 data.temperature.temperature_c,
                 data.current.irms_a,
                 data.current.power_w,
                 data.current.energy_kwh,
                 (unsigned long)data.reed_pulse_count,
                 data.signal_rssi_dbm);

        esp_err_t ret = portal_send(&data);
        if (ret != ESP_OK) {
            ESP_LOGW(TAG, "Portal send failed (will retry next cycle)");
        }

        vTaskDelay(pdMS_TO_TICKS(CONFIG_MEASURE_INTERVAL_MS));
    }
}
