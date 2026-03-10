#include "temperature.h"
#include "esp_log.h"
#include <math.h>
#include "sdkconfig.h"

/*
 * HFT 105.5/2.7 thermistor calibration constants.
 * Update these from the sensor datasheet once available.
 *
 *   TEMP_BETA      – Beta coefficient (K).  Typical NTC range: 3000–4500.
 *   TEMP_NOMINAL_R – Resistance at TEMP_NOMINAL_C (Ω).
 *   SERIES_R       – Value of the series resistor in the voltage divider (Ω).
 *
 * Wiring (voltage divider):
 *   3.3V ── SERIES_R ──┬── ADC pin
 *                      └── Thermistor ── GND
 */
#define TEMP_BETA           3950.0f
#define TEMP_NOMINAL_R      10000.0f
#define TEMP_NOMINAL_C      25.0f
#define SERIES_R            10000.0f

/* ESP32-S3 ADC with ATTEN_DB_12: full-scale ≈ 3.1 V */
#define ADC_MAX_VAL         4095.0f
#define VREF_V              3.1f

static const char *TAG = "temperature";
static adc_oneshot_unit_handle_t s_adc;

esp_err_t temperature_init(adc_oneshot_unit_handle_t adc_handle)
{
    s_adc = adc_handle;
    adc_oneshot_chan_cfg_t cfg = {
        .bitwidth = ADC_BITWIDTH_12,
        .atten    = ADC_ATTEN_DB_12,
    };
    return adc_oneshot_config_channel(s_adc, CONFIG_TEMP_ADC_CHANNEL, &cfg);
}

esp_err_t temperature_read(temperature_reading_t *reading)
{
    int raw = 0;
    esp_err_t ret = adc_oneshot_read(s_adc, CONFIG_TEMP_ADC_CHANNEL, &raw);
    if (ret != ESP_OK) {
        reading->valid = false;
        return ret;
    }

    /* Voltage at ADC pin */
    float v = (raw / ADC_MAX_VAL) * VREF_V;

    /* Prevent divide-by-zero near full-scale */
    if (v >= VREF_V) {
        reading->valid = false;
        return ESP_ERR_INVALID_RESPONSE;
    }

    /* Thermistor resistance from voltage divider */
    float r = SERIES_R * v / (VREF_V - v);

    /* Beta equation: 1/T = 1/T0 + (1/B)*ln(R/R0) */
    float inv_t = (1.0f / (TEMP_NOMINAL_C + 273.15f))
                + (1.0f / TEMP_BETA) * logf(r / TEMP_NOMINAL_R);

    reading->temperature_c = (1.0f / inv_t) - 273.15f;
    reading->valid = true;

    ESP_LOGD(TAG, "raw=%d  R=%.0f Ω  T=%.2f °C", raw, r, reading->temperature_c);
    return ESP_OK;
}
