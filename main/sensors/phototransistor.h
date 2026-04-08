#pragma once

#include "esp_err.h"
#include "esp_adc/adc_oneshot.h"
#include <stdbool.h>

typedef struct
{
    int raw;         /* Raw ADC value (0–4095)       */
    float voltage_v; /* Calibrated voltage (V)       */
    bool valid;
} phototransistor_reading_t;

/**
 * @brief Initialise phototransistor (BPW85C) ADC channel.
 *        Connected via Energy Monitor Shield terminal X2.
 * @param adc_handle  Shared ADC1 handle created by app_main.
 */
esp_err_t phototransistor_init(adc_oneshot_unit_handle_t adc_handle);

/**
 * @brief Read phototransistor analog value.
 *        Higher voltage = more light.
 */
esp_err_t phototransistor_read(phototransistor_reading_t *reading);
