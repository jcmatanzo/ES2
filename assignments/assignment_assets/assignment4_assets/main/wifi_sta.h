#pragma once

#include <stdbool.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Initialize Wi-Fi STA and block until connected (got IP) or timeout/fail.
 *
 * @param ssid  Wi-Fi SSID (null-terminated string)
 * @param pass  Wi-Fi password (null-terminated string)
 * @return ESP_OK on success (IP acquired), otherwise an error code.
 */
esp_err_t wifi_sta_connect(const char *ssid, const char *pass, uint32_t timeout_ms);

#ifdef __cplusplus
}
#endif