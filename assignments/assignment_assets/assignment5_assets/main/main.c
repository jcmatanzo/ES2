// main.c (ESP32-C6 / ESP-IDF)
// Main entry point: NVS init → Wi-Fi STA connect → MQTT start

#include <stdio.h>
#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_err.h"
#include "esp_log.h"
#include "nvs_flash.h"

#include "wifi_sta.h"
#include "mqtt_client_app.h"

static const char *TAG = "APP_MAIN";

#ifndef WIFI_SSID
#define WIFI_SSID "JCMP01"
#endif

#ifndef WIFI_PASS
#define WIFI_PASS "wgvo2919"
#endif

#ifndef CONFIG_MQTT_BROKER_URI
#define CONFIG_MQTT_BROKER_URI "mqtt://test.mosquitto.org:1883"
#endif

#define LED_GPIO 8;


void app_main(void)
{
    
    ESP_LOGI(TAG, "Booting...");

    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ESP_ERROR_CHECK(nvs_flash_init());
    } else {
        ESP_ERROR_CHECK(ret);
    }

    const char *ssid =
    #ifdef CONFIG_WIFI_SSID
        CONFIG_WIFI_SSID;
    #else
        WIFI_SSID;
    #endif

    const char *pass =
    #ifdef CONFIG_WIFI_PASSWORD
        CONFIG_WIFI_PASSWORD;
    #else
        WIFI_PASS;
    #endif

    ESP_LOGI(TAG, "Connecting to Wi-Fi SSID: %s", ssid);

    esp_err_t wifi_err = wifi_sta_connect(ssid, pass, 30000);
    if (wifi_err != ESP_OK) {
        ESP_LOGE(TAG, "Wi-Fi connect failed: %s (0x%x)", esp_err_to_name(wifi_err), wifi_err);

        while (1) {
            vTaskDelay(pdMS_TO_TICKS(1000));
        }
    }

    ESP_LOGI(TAG, "Wi-Fi connected!");

    ESP_LOGI(TAG, "Starting MQTT client: %s", CONFIG_MQTT_BROKER_URI);
    ESP_ERROR_CHECK(mqtt_app_start(CONFIG_MQTT_BROKER_URI));

    while (1) {
        ESP_LOGI(TAG, "System running...");
        vTaskDelay(pdMS_TO_TICKS(5000));
    }
}