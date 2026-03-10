#include "portal.h"
#include "modem/sim7670x.h"
#include "esp_log.h"
#include <stdio.h>
#include <string.h>
#include "sdkconfig.h"

#define JSON_BUF  1024
#define RESP_BUF  512

static const char *TAG = "portal";

esp_err_t portal_init(void)
{
    if (!sim7670x_wait_ready(30000)) {
        ESP_LOGE(TAG, "Modem not ready – check wiring and SIM card");
        return ESP_FAIL;
    }
    ESP_LOGI(TAG, "Portal initialised. Endpoint: %s", CONFIG_PORTAL_URL);
    return ESP_OK;
}

esp_err_t portal_send(const sensor_data_t *data)
{
    char json[JSON_BUF];
    char resp[RESP_BUF];

    /* Build JSON payload -------------------------------------------------- */
    int n = snprintf(json, sizeof(json),
        "{"
          "\"device_id\":\"%s\","
          "\"timestamp\":%lld,"
          "\"temperature\":{\"value\":%.2f,\"valid\":%s},"
          "\"current\":{"
            "\"irms\":%.3f,"
            "\"power\":%.1f,"
            "\"energy\":%.4f,"
            "\"valid\":%s"
          "},"
          "\"accelerometer\":{"
            "\"ax\":%.3f,\"ay\":%.3f,\"az\":%.3f,"
            "\"gx\":%.2f,\"gy\":%.2f,\"gz\":%.2f,"
            "\"valid\":%s"
          "},"
          "\"reed\":{\"pulses\":%lu,\"state\":%s},"
          "\"rssi_dbm\":%d"
        "}",
        CONFIG_DEVICE_ID,
        (long long)data->timestamp_ms,
        data->temperature.temperature_c,
        data->temperature.valid ? "true" : "false",
        data->current.irms_a,
        data->current.power_w,
        data->current.energy_kwh,
        data->current.valid ? "true" : "false",
        data->accelerometer.accel_x,
        data->accelerometer.accel_y,
        data->accelerometer.accel_z,
        data->accelerometer.gyro_x,
        data->accelerometer.gyro_y,
        data->accelerometer.gyro_z,
        data->accelerometer.valid ? "true" : "false",
        (unsigned long)data->reed_pulse_count,
        data->reed_state ? "true" : "false",
        data->signal_rssi_dbm
    );

    if (n < 0 || n >= (int)sizeof(json)) {
        ESP_LOGE(TAG, "JSON buffer overflow");
        return ESP_ERR_NO_MEM;
    }

    ESP_LOGD(TAG, "POST → %s", json);
    return sim7670x_http_post(CONFIG_PORTAL_URL, json, resp, sizeof(resp));
}
