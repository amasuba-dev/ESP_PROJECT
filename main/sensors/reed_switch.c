#include "reed_switch.h"
#include "driver/gpio.h"
#include "esp_timer.h"
#include "esp_log.h"
#include <stdatomic.h>

#define DEBOUNCE_US     20000   /* 20 ms debounce window */

static const char *TAG = "reed_switch";

static int           s_gpio;
static reed_cb_t     s_cb;
static void         *s_cb_arg;
static atomic_uint   s_pulse_count;
static volatile bool s_state;

static void IRAM_ATTR gpio_isr_handler(void *arg)
{
    static int64_t last_us = 0;
    int64_t now = esp_timer_get_time();

    if ((now - last_us) < DEBOUNCE_US) return;
    last_us = now;

    s_state = (bool)gpio_get_level(s_gpio); /* true=open, false=closed */
    if (!s_state) {
        /* Falling edge = switch closed → count one pulse */
        atomic_fetch_add_explicit(&s_pulse_count, 1, memory_order_relaxed);
    }
    if (s_cb) s_cb(s_state, s_cb_arg);
}

esp_err_t reed_switch_init(int gpio_num, reed_cb_t callback, void *arg)
{
    s_gpio   = gpio_num;
    s_cb     = callback;
    s_cb_arg = arg;
    atomic_store(&s_pulse_count, 0);

    gpio_config_t io_cfg = {
        .pin_bit_mask = (1ULL << gpio_num),
        .mode         = GPIO_MODE_INPUT,
        .pull_up_en   = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type    = GPIO_INTR_ANYEDGE,
    };
    ESP_ERROR_CHECK(gpio_config(&io_cfg));

    /* gpio_install_isr_service may already be called; ignore re-install error */
    esp_err_t ret = gpio_install_isr_service(0);
    if (ret != ESP_OK && ret != ESP_ERR_INVALID_STATE) {
        return ret;
    }

    ESP_ERROR_CHECK(gpio_isr_handler_add(gpio_num, gpio_isr_handler, NULL));

    s_state = (bool)gpio_get_level(gpio_num);
    ESP_LOGI(TAG, "Reed switch on GPIO%d, initial=%s",
             gpio_num, s_state ? "OPEN" : "CLOSED");
    return ESP_OK;
}

bool reed_switch_get_state(void)
{
    return s_state;
}

uint32_t reed_switch_get_pulse_count(void)
{
    return atomic_load(&s_pulse_count);
}

void reed_switch_reset_pulse_count(void)
{
    atomic_store(&s_pulse_count, 0);
}
