#pragma once

#include "esp_err.h"
#include "esp_adc/adc_oneshot.h"

typedef struct {
    float irms_a;       /* RMS current (A)          */
    float power_w;      /* Apparent power (W)       */
    float energy_kwh;   /* Accumulated energy (kWh) */
    bool  valid;
} current_reading_t;

/**
 * @brief Initialise current sensor ADC channel.
 *        SCT013-030-1V connected to Energy Monitor Shield A0.
 * @param adc_handle  Shared ADC1 handle created by app_main.
 */
esp_err_t current_init(adc_oneshot_unit_handle_t adc_handle);

/** @brief Sample 1480 ADC points and calculate RMS current. */
esp_err_t current_read(current_reading_t *reading);

/** @brief Reset the accumulated energy counter. */
void      current_reset_energy(void);
