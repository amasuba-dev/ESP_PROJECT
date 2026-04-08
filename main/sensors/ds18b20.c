/*
 * DS18B20 1-Wire Temperature Sensor — GPIO bit-bang driver for ESP-IDF
 *
 * Ported from the Arduino NodeMCU code that used OneWire + DallasTemperature
 * libraries.  This implementation runs directly on the ESP32-S3 WaveShare
 * using open-drain GPIO with internal pull-up.
 *
 * Wiring:  Yellow (Data) → GPIO + optional 4.7 kΩ to 3.3 V
 *          Red (VCC)     → 3.3 V
 *          Black (GND)   → GND
 */

#include "ds18b20.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "esp_attr.h"
#include "esp_rom_sys.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "ds18b20";

/* 1-Wire ROM commands */
#define OW_CMD_SKIP_ROM 0xCC
#define OW_CMD_CONVERT_T 0x44
#define OW_CMD_READ_SCRATCH 0xBE

static int s_pin = -1;
static portMUX_TYPE s_mux = portMUX_INITIALIZER_UNLOCKED;

/* ── Low-level 1-Wire open-drain helpers ────────────────────────────────
 * Pin is configured as INPUT_OUTPUT_OD with internal pull-up.
 *   set_level(0)  → drive bus LOW
 *   set_level(1)  → release bus (pull-up drives HIGH)
 *   get_level()   → sample bus state
 * Timing uses esp_rom_delay_us() (busy-wait, µs precision).
 * Critical sections disable interrupts so timing is not perturbed.
 * ─────────────────────────────────────────────────────────────────────── */

/**
 * Issue a reset pulse and check for presence pulse.
 * @return true if a device responded with a presence pulse.
 */
static bool IRAM_ATTR ow_reset(void)
{
    bool presence;
    portENTER_CRITICAL(&s_mux);

    gpio_set_level(s_pin, 0); /* pull LOW */
    esp_rom_delay_us(480);
    gpio_set_level(s_pin, 1); /* release */
    esp_rom_delay_us(70);
    presence = (gpio_get_level(s_pin) == 0);

    portEXIT_CRITICAL(&s_mux);

    esp_rom_delay_us(410); /* complete reset time-slot */
    return presence;
}

/** Write a single bit (LSB first). */
static void IRAM_ATTR ow_write_bit(int bit)
{
    portENTER_CRITICAL(&s_mux);
    gpio_set_level(s_pin, 0); /* start slot */
    if (bit)
    {
        esp_rom_delay_us(6);
        gpio_set_level(s_pin, 1); /* release early for '1' */
        portEXIT_CRITICAL(&s_mux);
        esp_rom_delay_us(64);
    }
    else
    {
        esp_rom_delay_us(60);     /* hold low for '0' */
        gpio_set_level(s_pin, 1); /* release */
        portEXIT_CRITICAL(&s_mux);
        esp_rom_delay_us(10);
    }
}

/** Read a single bit. */
static int IRAM_ATTR ow_read_bit(void)
{
    int bit;
    portENTER_CRITICAL(&s_mux);

    gpio_set_level(s_pin, 0); /* start slot */
    esp_rom_delay_us(6);
    gpio_set_level(s_pin, 1); /* release */
    esp_rom_delay_us(9);
    bit = gpio_get_level(s_pin); /* sample */

    portEXIT_CRITICAL(&s_mux);
    esp_rom_delay_us(55); /* complete time-slot */
    return bit;
}

static void ow_write_byte(uint8_t byte)
{
    for (int i = 0; i < 8; i++)
    {
        ow_write_bit(byte & 0x01);
        byte >>= 1;
    }
}

static uint8_t ow_read_byte(void)
{
    uint8_t byte = 0;
    for (int i = 0; i < 8; i++)
    {
        byte |= (ow_read_bit() << i);
    }
    return byte;
}

/* ── CRC-8 (Maxim/Dallas, polynomial 0x8C reflected) ───────────────── */
static uint8_t crc8(const uint8_t *data, int len)
{
    uint8_t crc = 0;
    for (int i = 0; i < len; i++)
    {
        uint8_t byte = data[i];
        for (int j = 0; j < 8; j++)
        {
            uint8_t mix = (crc ^ byte) & 0x01;
            crc >>= 1;
            if (mix)
                crc ^= 0x8C;
            byte >>= 1;
        }
    }
    return crc;
}

/* ── Public API ──────────────────────────────────────────────────────── */

esp_err_t ds18b20_init(int gpio_num)
{
    s_pin = gpio_num;

    /* Open-drain + internal pull-up — no direction change needed at runtime */
    gpio_config_t io_cfg = {
        .pin_bit_mask = (1ULL << gpio_num),
        .mode = GPIO_MODE_INPUT_OUTPUT_OD,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    esp_err_t ret = gpio_config(&io_cfg);
    if (ret != ESP_OK)
        return ret;

    gpio_set_level(gpio_num, 1);   /* release bus */
    vTaskDelay(pdMS_TO_TICKS(50)); /* let pull-up stabilise */

    if (!ow_reset())
    {
        ESP_LOGW(TAG, "No DS18B20 detected on GPIO%d – check wiring", gpio_num);
        ESP_LOGW(TAG, "  Yellow (Data) → GPIO%d, Red → 3V3, Black → GND", gpio_num);
        ESP_LOGW(TAG, "  For long wires add 4.7 kΩ pull-up to 3V3");
        return ESP_ERR_NOT_FOUND;
    }

    /* Perform a dummy conversion so the sensor is primed */
    ow_write_byte(OW_CMD_SKIP_ROM);
    ow_write_byte(OW_CMD_CONVERT_T);
    vTaskDelay(pdMS_TO_TICKS(750));

    ESP_LOGI(TAG, "DS18B20 initialised on GPIO%d (12-bit, 0.0625 °C)", gpio_num);
    return ESP_OK;
}

esp_err_t ds18b20_read(ds18b20_reading_t *reading)
{
    reading->valid = false;

    /* Start conversion: Skip ROM (assumes single device) + Convert T */
    if (!ow_reset())
    {
        ESP_LOGW(TAG, "DS18B20 not responding");
        return ESP_ERR_NOT_FOUND;
    }
    ow_write_byte(OW_CMD_SKIP_ROM);
    ow_write_byte(OW_CMD_CONVERT_T);

    /* 12-bit conversion takes up to 750 ms — yield to scheduler */
    vTaskDelay(pdMS_TO_TICKS(750));

    /* Read scratchpad (9 bytes) */
    if (!ow_reset())
    {
        return ESP_ERR_NOT_FOUND;
    }
    ow_write_byte(OW_CMD_SKIP_ROM);
    ow_write_byte(OW_CMD_READ_SCRATCH);

    uint8_t scratch[9];
    for (int i = 0; i < 9; i++)
    {
        scratch[i] = ow_read_byte();
    }

    /* Validate CRC */
    uint8_t calc_crc = crc8(scratch, 8);
    if (calc_crc != scratch[8])
    {
        ESP_LOGW(TAG, "CRC mismatch (calc 0x%02X, got 0x%02X)", calc_crc, scratch[8]);
        return ESP_ERR_INVALID_CRC;
    }

    /* Convert 12-bit raw → °C  (DS18B20 resolution: 0.0625 °C per LSB) */
    int16_t raw = (int16_t)((scratch[1] << 8) | scratch[0]);
    reading->temperature_c = raw / 16.0f;
    reading->valid = true;

    ESP_LOGD(TAG, "DS18B20: %.2f °C (raw=0x%04X)", reading->temperature_c, (uint16_t)raw);
    return ESP_OK;
}
