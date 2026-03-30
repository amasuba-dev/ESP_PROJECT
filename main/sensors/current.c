#include "current.h"
#include "esp_log.h"
#include "esp_timer.h"
#include <math.h>
#include "sdkconfig.h"
#include "esp_adc/adc_cali.h"
#include "esp_adc/adc_cali_scheme.h"

/*
 * SCT013-030-1V calibration
 * ─────────────────────────
 * This model has an internal burden resistor: 30 A → 1 V peak output.
 * Calibration constant = 30 A / 1 V = 30.0 A/V.
 * Tune CURRENT_CAL empirically with a known resistive load.
 *
 * Hardware note (Energy Monitor Shield A0 → ESP32-S3 ADC pin):
 *   The shield biases the AC signal at VCC/2 so the ADC sees 0–3.3 V.
 *   If your bias circuit is different, adjust VREF_V accordingly.
 */
#define CURRENT_CAL     30.0f       /* A/V – adjust per calibration     */
#define NOISE_FLOOR_A   0.05f       /* Readings below this → 0 A        */
#define SAMPLES         1480        /* ~1 full 50 Hz cycle at ADC rate  */
#define ADC_MAX_VAL     4095.0f
#define VREF_V          3.1f        /* ESP32-S3 ATTEN_DB_12 full scale  */

static const char *TAG = "current";

static adc_oneshot_unit_handle_t s_adc;
static adc_cali_handle_t         s_cali_handle = NULL;
static float   s_energy_kwh = 0.0f;
static int64_t s_last_us    = 0;

esp_err_t current_init(adc_oneshot_unit_handle_t adc_handle)
{
    s_adc = adc_handle;
    adc_oneshot_chan_cfg_t cfg = {
        .bitwidth = ADC_BITWIDTH_12,
        .atten    = ADC_ATTEN_DB_12,
    };
    s_last_us = esp_timer_get_time();
    esp_err_t ret = adc_oneshot_config_channel(s_adc, CONFIG_CURRENT_ADC_CHANNEL, &cfg);
    if (ret != ESP_OK) return ret;

    /* Attempt chip-specific ADC calibration (ESP32-S3 supports curve fitting) */
    adc_cali_curve_fitting_config_t cali_cfg = {
        .unit_id  = ADC_UNIT_1,
        .chan     = CONFIG_CURRENT_ADC_CHANNEL,
        .atten    = ADC_ATTEN_DB_12,
        .bitwidth = ADC_BITWIDTH_12,
    };
    if (adc_cali_create_scheme_curve_fitting(&cali_cfg, &s_cali_handle) == ESP_OK) {
        ESP_LOGI(TAG, "ADC calibration enabled (curve fitting)");
    } else {
        ESP_LOGW(TAG, "ADC calibration unavailable; using nominal %.1fV reference", VREF_V);
        s_cali_handle = NULL;
    }
    return ESP_OK;
}

esp_err_t current_read(current_reading_t *reading)
{
    /*
     * Single-pass EmonLib-style algorithm:
     *   offset tracks the DC mid-point with a slow low-pass filter.
     *   filtered = sample − offset  →  centred AC waveform.
     *   RMS = sqrt( mean(filtered²) )
     */
    static double offset = ADC_MAX_VAL / 2.0;

    double sum_sq = 0.0;

    for (int i = 0; i < SAMPLES; i++) {
        int raw = 0;
        esp_err_t ret = adc_oneshot_read(s_adc, CONFIG_CURRENT_ADC_CHANNEL, &raw);
        if (ret != ESP_OK) {
            reading->valid = false;
            return ret;
        }
        /* Drift DC offset slowly (1024-sample time constant) */
        offset += ((double)raw - offset) / 1024.0;
        double filtered = (double)raw - offset;
        sum_sq += filtered * filtered;
    }

    double raw_rms = sqrt(sum_sq / SAMPLES);
    float vrms;
    if (s_cali_handle) {
        /*
         * Convert (midpoint ± raw_rms) through the calibration curve to get
         * the true voltage swing, then halve it to obtain the RMS voltage.
         * This correctly handles ADC non-linearity near the operating point.
         */
        int mid    = (int)(offset + 0.5);
        int hi_raw = mid + (int)(raw_rms + 0.5);
        int lo_raw = mid - (int)(raw_rms + 0.5);
        if (hi_raw > (int)ADC_MAX_VAL) hi_raw = (int)ADC_MAX_VAL;
        if (lo_raw < 0)               lo_raw = 0;
        int hi_mv = 0, lo_mv = 0;
        adc_cali_raw_to_voltage(s_cali_handle, hi_raw, &hi_mv);
        adc_cali_raw_to_voltage(s_cali_handle, lo_raw, &lo_mv);
        vrms = (float)(hi_mv - lo_mv) / 2000.0f;  /* mV half-span → V RMS */
    } else {
        vrms = (float)(raw_rms / ADC_MAX_VAL) * VREF_V;
    }
    float irms = vrms * CURRENT_CAL;
    if (irms < NOISE_FLOOR_A) irms = 0.0f;

    reading->irms_a  = irms;
    reading->power_w = irms * (float)CONFIG_SUPPLY_VOLTAGE;

    /* Accumulate energy */
    int64_t now_us = esp_timer_get_time();
    float   dt_h   = (float)(now_us - s_last_us) / 3600000000.0f;
    s_energy_kwh  += reading->power_w * dt_h / 1000.0f;
    s_last_us      = now_us;

    reading->energy_kwh = s_energy_kwh;
    reading->valid = true;

    ESP_LOGD(TAG, "Irms=%.3f A  P=%.1f W  E=%.4f kWh",
             irms, reading->power_w, s_energy_kwh);
    return ESP_OK;
}

void current_reset_energy(void)
{
    s_energy_kwh = 0.0f;
    s_last_us    = esp_timer_get_time();
}
