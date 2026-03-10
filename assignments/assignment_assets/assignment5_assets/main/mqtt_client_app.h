#pragma once

#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Start MQTT client (non-blocking). Requires Wi-Fi already connected.
 *
 * @param broker_uri Example: "mqtt://192.168.1.10:1883"
 * @return ESP_OK if started, otherwise error.
 */
esp_err_t mqtt_app_start(const char *broker_uri);

/**
 * @brief Publish convenience wrapper (optional).
 *
 * @param topic Topic string
 * @param payload Null-terminated payload (UTF-8)
 * @param qos 0,1,2
 * @param retain 0/1
 * @return message_id (>=0) or -1 on error
 */
int mqtt_app_publish(const char *topic, const char *payload, int qos, int retain);

#ifdef __cplusplus
}
#endif