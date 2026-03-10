#pragma once

#include "esp_err.h"
#include "sensors/temperature.h"
#include "sensors/current.h"
#include "sensors/mpu6050.h"

/** Aggregated snapshot of all sensor readings for one transmission cycle. */
typedef struct {
    temperature_reading_t temperature;
    current_reading_t     current;
    mpu6050_reading_t     accelerometer;
    uint32_t              reed_pulse_count;
    bool                  reed_state;           /* true=open, false=closed */
    int64_t               timestamp_ms;         /* ms since boot            */
    int                   signal_rssi_dbm;
} sensor_data_t;

/**
 * @brief Verify modem is online and reachable.
 *        Call once after sim7670x_init().
 */
esp_err_t portal_init(void);

/**
 * @brief Serialise sensor_data_t to JSON and HTTP POST to CONFIG_PORTAL_URL.
 *
 * JSON schema (used by the browser portal):
 * {
 *   "device_id": "ESP32-S3-001",
 *   "timestamp": <ms>,
 *   "temperature": { "value": <°C>, "valid": <bool> },
 *   "current":  { "irms": <A>, "power": <W>, "energy": <kWh>, "valid": <bool> },
 *   "accelerometer": { "ax":.., "ay":.., "az":..,
 *                      "gx":.., "gy":.., "gz":.., "valid": <bool> },
 *   "reed": { "pulses": <n>, "state": <bool> },
 *   "rssi_dbm": <int>
 * }
 */
esp_err_t portal_send(const sensor_data_t *data);
