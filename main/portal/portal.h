#pragma once

#include "esp_err.h"
#include <stdbool.h>
#include "sensors/temperature.h"
#include "sensors/current.h"
#include "sensors/mpu6050.h"
#include "sensors/ds18b20.h"
#include "sensors/phototransistor.h"

/** Lifecycle event tag included in every JSON POST. */
typedef enum
{
    PORTAL_EVENT_DATA = 0,  /**< Regular measurement during day mode   */
    PORTAL_EVENT_WAKE = 1,  /**< First POST after waking from deep sleep */
    PORTAL_EVENT_SLEEP = 2, /**< Last POST before entering deep sleep   */
} portal_event_t;

/** Aggregated snapshot of all sensor readings for one transmission cycle. */
typedef struct
{
    temperature_reading_t temperature;
    current_reading_t current;
    mpu6050_reading_t accelerometer;
    ds18b20_reading_t ds18b20;
    phototransistor_reading_t phototransistor;
    uint32_t reed_pulse_count;
    bool reed_state;      /* true=open, false=closed */
    int64_t timestamp_ms; /* ms since boot            */
    int signal_rssi_dbm;
    portal_event_t event; /* data / wake / sleep      */
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
