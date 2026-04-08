#pragma once

#include "esp_err.h"
#include <stdbool.h>

typedef struct
{
    float temperature_c;
    bool valid;
} ds18b20_reading_t;

/**
 * @brief Initialise DS18B20 1-Wire temperature sensor.
 *        Uses GPIO bit-bang with open-drain + internal pull-up.
 *        For long wires (>1 m) add an external 4.7 kΩ pull-up to 3.3 V.
 * @param gpio_num  GPIO connected to DS18B20 data line.
 * @return ESP_OK on success, ESP_ERR_NOT_FOUND if no device on the bus.
 */
esp_err_t ds18b20_init(int gpio_num);

/**
 * @brief Read temperature from DS18B20 (12-bit, 0.0625 °C resolution).
 *        Blocks ~750 ms for conversion (yields to RTOS scheduler).
 *        CRC-8 validated; reading->valid = false on CRC mismatch.
 */
esp_err_t ds18b20_read(ds18b20_reading_t *reading);
