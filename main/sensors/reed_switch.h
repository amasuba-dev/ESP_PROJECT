#pragma once

#include "esp_err.h"
#include <stdbool.h>
#include <stdint.h>

/**
 * @brief Callback invoked from ISR context on any edge.
 * @param state  true = switch OPEN, false = switch CLOSED (active-low).
 * @param arg    User-supplied argument passed to reed_switch_init().
 */
typedef void (*reed_cb_t)(bool state, void *arg);

/**
 * @brief Configure a GPIO pin for reed switch input with debounce.
 *        Internal pull-up is enabled; switch should connect GPIO to GND.
 * @param gpio_num  GPIO number (set via CONFIG_REED_SWITCH_GPIO).
 * @param callback  Optional ISR-context callback, or NULL.
 * @param arg       Argument forwarded to callback.
 */
esp_err_t reed_switch_init(int gpio_num, reed_cb_t callback, void *arg);

/** @brief Return current reed switch state (true=open, false=closed). */
bool     reed_switch_get_state(void);

/** @brief Return total number of CLOSED transitions since init or last reset. */
uint32_t reed_switch_get_pulse_count(void);

/** @brief Reset pulse counter to zero. */
void     reed_switch_reset_pulse_count(void);
