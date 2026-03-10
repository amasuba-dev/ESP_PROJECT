#include "sim7670x.h"
#include "driver/uart.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <string.h>
#include <stdio.h>
#include "sdkconfig.h"

#define RX_BUF_SIZE     4096
#define AT_TIMEOUT_MS   10000
#define HTTP_TIMEOUT_MS 30000

static const char *TAG = "sim7670x";

/* ── Internal AT command helper ─────────────────────────────────────────── */

/**
 * Send an AT command and wait until 'expect' string or "ERROR" appears in the
 * response, or the timeout expires.
 *
 * @return true  if 'expect' was found in the response.
 *         false if "ERROR" was received or timeout elapsed.
 */
static bool at_cmd(const char *cmd, const char *expect,
                   char *resp, size_t resp_len, uint32_t timeout_ms)
{
    uart_flush_input(CONFIG_SIM7670X_UART_PORT);

    if (cmd && *cmd) {
        uart_write_bytes(CONFIG_SIM7670X_UART_PORT, cmd, strlen(cmd));
        uart_write_bytes(CONFIG_SIM7670X_UART_PORT, "\r\n", 2);
    }

    if (!expect) return true;

    char    buf[512] = {0};
    size_t  pos      = 0;
    TickType_t deadline = xTaskGetTickCount() + pdMS_TO_TICKS(timeout_ms);

    while (xTaskGetTickCount() < deadline) {
        uint8_t ch;
        if (uart_read_bytes(CONFIG_SIM7670X_UART_PORT, &ch, 1,
                            pdMS_TO_TICKS(100)) > 0) {
            if (pos < sizeof(buf) - 1) buf[pos++] = (char)ch;
        }
        if (strstr(buf, expect)) {
            if (resp && resp_len) {
                strncpy(resp, buf, resp_len - 1);
                resp[resp_len - 1] = '\0';
            }
            return true;
        }
        if (strstr(buf, "ERROR")) {
            ESP_LOGW(TAG, "ERROR response for: %s", cmd ? cmd : "(data)");
            return false;
        }
    }
    ESP_LOGW(TAG, "Timeout waiting for '%s' (cmd='%s')", expect, cmd ? cmd : "");
    return false;
}

/* ── Public API ──────────────────────────────────────────────────────────── */

esp_err_t sim7670x_init(void)
{
    uart_config_t cfg = {
        .baud_rate  = CONFIG_SIM7670X_BAUD_RATE,
        .data_bits  = UART_DATA_8_BITS,
        .parity     = UART_PARITY_DISABLE,
        .stop_bits  = UART_STOP_BITS_1,
        .flow_ctrl  = UART_HW_FLOWCTRL_DISABLE,
        .source_clk = UART_SCLK_DEFAULT,
    };
    ESP_ERROR_CHECK(uart_driver_install(CONFIG_SIM7670X_UART_PORT,
                                        RX_BUF_SIZE, 0, 0, NULL, 0));
    ESP_ERROR_CHECK(uart_param_config(CONFIG_SIM7670X_UART_PORT, &cfg));
    ESP_ERROR_CHECK(uart_set_pin(CONFIG_SIM7670X_UART_PORT,
                                 CONFIG_SIM7670X_TX_GPIO,
                                 CONFIG_SIM7670X_RX_GPIO,
                                 UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE));

    ESP_LOGI(TAG, "SIM7670X on UART%d  TX=%d  RX=%d  baud=%d",
             CONFIG_SIM7670X_UART_PORT,
             CONFIG_SIM7670X_TX_GPIO, CONFIG_SIM7670X_RX_GPIO,
             CONFIG_SIM7670X_BAUD_RATE);
    return ESP_OK;
}

bool sim7670x_wait_ready(uint32_t timeout_ms)
{
    TickType_t deadline = xTaskGetTickCount() + pdMS_TO_TICKS(timeout_ms);
    while (xTaskGetTickCount() < deadline) {
        if (at_cmd("AT", "OK", NULL, 0, 1000)) {
            /* Disable echo for cleaner parsing */
            at_cmd("ATE0", "OK", NULL, 0, 1000);
            ESP_LOGI(TAG, "Modem ready");
            return true;
        }
        vTaskDelay(pdMS_TO_TICKS(500));
    }
    ESP_LOGE(TAG, "Modem not responding after %lu ms", (unsigned long)timeout_ms);
    return false;
}

esp_err_t sim7670x_get_signal_quality(int *rssi_dbm)
{
    char resp[128] = {0};
    if (!at_cmd("AT+CSQ", "+CSQ:", resp, sizeof(resp), AT_TIMEOUT_MS)) {
        *rssi_dbm = -999;
        return ESP_FAIL;
    }
    /* +CSQ: <rssi>,<ber>  where rssi 0-31, 99=unknown */
    int csq = 99;
    char *p = strstr(resp, "+CSQ:");
    if (p) sscanf(p, "+CSQ: %d", &csq);
    *rssi_dbm = (csq == 99) ? -999 : (-113 + csq * 2);
    return ESP_OK;
}

esp_err_t sim7670x_http_post(const char *url, const char *json,
                              char *resp_buf, size_t resp_len)
{
    char cmd[300];

    /* Activate data bearer (PDP context 0) */
    at_cmd("AT+CNACT=0,1", "+APP PDP: 0,ACTIVE", NULL, 0, 15000);

    /* Initialise HTTP stack */
    if (!at_cmd("AT+HTTPINIT", "OK", NULL, 0, AT_TIMEOUT_MS)) {
        ESP_LOGE(TAG, "HTTPINIT failed");
        return ESP_FAIL;
    }

    /* Set URL */
    snprintf(cmd, sizeof(cmd), "AT+HTTPPARA=\"URL\",\"%s\"", url);
    if (!at_cmd(cmd, "OK", NULL, 0, AT_TIMEOUT_MS)) goto term;

    /* Set content-type */
    if (!at_cmd("AT+HTTPPARA=\"CONTENT\",\"application/json\"",
                "OK", NULL, 0, AT_TIMEOUT_MS)) goto term;

    /* Upload body */
    size_t len = strlen(json);
    snprintf(cmd, sizeof(cmd), "AT+HTTPDATA=%d,5000", (int)len);
    if (!at_cmd(cmd, "DOWNLOAD", NULL, 0, AT_TIMEOUT_MS)) goto term;

    uart_write_bytes(CONFIG_SIM7670X_UART_PORT, json, len);
    at_cmd(NULL, "OK", NULL, 0, 5000);   /* wait for data accepted */

    /* Execute POST (action=1) */
    char http_resp[128] = {0};
    if (!at_cmd("AT+HTTPACTION=1", "+HTTPACTION: 1,200",
                http_resp, sizeof(http_resp), HTTP_TIMEOUT_MS)) {
        ESP_LOGW(TAG, "HTTP POST response: %s", http_resp);
    }

    /* Optionally read response body */
    if (resp_buf && resp_len) {
        at_cmd("AT+HTTPREAD=0,512", "OK", resp_buf, (int)resp_len, AT_TIMEOUT_MS);
    }

term:
    at_cmd("AT+HTTPTERM", "OK", NULL, 0, AT_TIMEOUT_MS);
    return ESP_OK;
}
