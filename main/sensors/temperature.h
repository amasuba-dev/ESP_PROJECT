#pragma once

#include "esp_err.h"
#include "esp_adc/adc_oneshot.h"

typedef struct {
    float temperature_c;
    bool  valid;
} temperature_reading_t;

/**
 * @brief Initialise the temperature sensor ADC channel.
 * @param adc_handle  Shared ADC1 handle created by app_main.
 */
esp_err_t temperature_init(adc_oneshot_unit_handle_t adc_handle);

/**
 * @brief Read temperature from HFT 105.5/2.7 thermistor.
 *        Adjust TEMP_BETA and TEMP_NOMINAL_R in temperature.c
 *        to match the sensor's datasheet values.
 */
esp_err_t temperature_read(temperature_reading_t *reading);
