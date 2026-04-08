/*
 * Phototransistor (BPW85C) — ADC driver for ESP-IDF
 *
 * Ported from the Arduino NodeMCU code where this sensor connected to the
 * Energy Monitor Shield V2 terminal X2 (analog input).
 *
 * The BPW85C phototransistor outputs collector current proportional to
 * incident light.  With a load resistor a voltage appears at the ADC pin:
 * higher voltage → more light.
 */

#include "phototransistor.h"
#include "esp_log.h"
#include "sdkconfig.h"
#include "esp_adc/adc_cali.h"
#include "esp_adc/adc_cali_scheme.h"

#define ADC_MAX_VAL 4095.0f
#define VREF_V 3.1f /* ESP32-S3 ATTEN_DB_12 nominal full-scale */

static const char *TAG = "phototransistor";

static adc_oneshot_unit_handle_t s_adc;
static adc_cali_handle_t s_cali_handle = NULL;

esp_err_t phototransistor_init(adc_oneshot_unit_handle_t adc_handle)
{
    s_adc = adc_handle;

    adc_oneshot_chan_cfg_t cfg = {
        .bitwidth = ADC_BITWIDTH_12,
        .atten = ADC_ATTEN_DB_12,
    };
    esp_err_t ret = adc_oneshot_config_channel(s_adc, CONFIG_PHOTO_ADC_CHANNEL, &cfg);
    if (ret != ESP_OK)
        return ret;

    /* Attempt calibration (ESP32-S3 supports curve fitting) */
    adc_cali_curve_fitting_config_t cali_cfg = {
        .unit_id = ADC_UNIT_1,
        .chan = CONFIG_PHOTO_ADC_CHANNEL,
        .atten = ADC_ATTEN_DB_12,
        .bitwidth = ADC_BITWIDTH_12,
    };
    if (adc_cali_create_scheme_curve_fitting(&cali_cfg, &s_cali_handle) == ESP_OK)
    {
        ESP_LOGI(TAG, "ADC calibration enabled (curve fitting)");
    }
    else
    {
        ESP_LOGW(TAG, "ADC calibration unavailable; using nominal %.1fV reference", VREF_V);
        s_cali_handle = NULL;
    }

    ESP_LOGI(TAG, "Phototransistor (BPW85C) on ADC1_CH%d ready", CONFIG_PHOTO_ADC_CHANNEL);
    return ESP_OK;
}

esp_err_t phototransistor_read(phototransistor_reading_t *reading)
{
    int raw = 0;
    esp_err_t ret = adc_oneshot_read(s_adc, CONFIG_PHOTO_ADC_CHANNEL, &raw);
    if (ret != ESP_OK)
    {
        reading->valid = false;
        return ret;
    }

    reading->raw = raw;

    if (s_cali_handle)
    {
        int mv = 0;
        adc_cali_raw_to_voltage(s_cali_handle, raw, &mv);
        reading->voltage_v = mv / 1000.0f;
    }
    else
    {
        reading->voltage_v = (raw / ADC_MAX_VAL) * VREF_V;
    }

    reading->valid = true;

    ESP_LOGD(TAG, "Photo: raw=%d  V=%.3f", raw, reading->voltage_v);
    return ESP_OK;
}
