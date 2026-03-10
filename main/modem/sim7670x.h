#pragma once

#include "esp_err.h"
#include <stdbool.h>
#include <stddef.h>

/**
 * @brief Initialise the UART port connected to the SIM7670X modem.
 *        Pins and baud rate are set via menuconfig (CONFIG_SIM7670X_*).
 */
esp_err_t sim7670x_init(void);

/**
 * @brief Poll the modem with "AT" until it responds "OK".
 * @param timeout_ms  Total wait time in milliseconds.
 * @return true if modem is responsive within the timeout.
 */
bool sim7670x_wait_ready(uint32_t timeout_ms);

/**
 * @brief Perform an HTTP POST with a JSON body.
 *        Activates bearer, initialises HTTP stack, sends data, reads response.
 * @param url       Full URL (http://...).
 * @param json      NULL-terminated JSON string.
 * @param resp_buf  Buffer for server response text, or NULL to discard.
 * @param resp_len  Size of resp_buf.
 */
esp_err_t sim7670x_http_post(const char *url, const char *json,
                              char *resp_buf, size_t resp_len);

/**
 * @brief Read signal quality.
 * @param rssi_dbm  Filled with RSSI in dBm (-113 to -51), or -999 if unknown.
 */
esp_err_t sim7670x_get_signal_quality(int *rssi_dbm);
