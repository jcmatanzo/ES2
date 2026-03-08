// main.c (ESP32-C6 / ESP-IDF)
// Main entry point: NVS init -> Wi-Fi STA connect -> MQTT start -> FreeRTOS Tasks

#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_err.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "driver/ledc.h" 
#include "esp_adc/adc_oneshot.h"

#include "wifi_sta.h"
#include "mqtt_client_app.h"

#define MOTOR_GPIO 13
#define TEMP_ADC_CHAN ADC_CHANNEL_4
#define HUM_ADC_CHAN  ADC_CHANNEL_5

static const char *TAG = "APP_MAIN";

static int current_motor_state = 0;
static int current_motor_speed = 0;

static adc_oneshot_unit_handle_t adc1_handle;

extern void mqtt_publish_sensor_data(int is_temp, int value);

static void motor_init(void) {
    ledc_timer_config_t ledc_timer = {
        .speed_mode       = LEDC_LOW_SPEED_MODE,
        .timer_num        = LEDC_TIMER_0,
        .duty_resolution  = LEDC_TIMER_8_BIT,
        .freq_hz          = 1000,
        .clk_cfg          = LEDC_AUTO_CLK
    };
    ESP_ERROR_CHECK(ledc_timer_config(&ledc_timer));

    ledc_channel_config_t ledc_channel = {
        .speed_mode     = LEDC_LOW_SPEED_MODE,
        .channel        = LEDC_CHANNEL_0,
        .timer_sel      = LEDC_TIMER_0,
        .intr_type      = LEDC_INTR_DISABLE,
        .gpio_num       = MOTOR_GPIO,
        .duty           = 0,
        .hpoint         = 0
    };
    ESP_ERROR_CHECK(ledc_channel_config(&ledc_channel));
    ESP_LOGI(TAG, "Motor initialized on GPIO %d", MOTOR_GPIO);
}

static void analog_init(void) {
    adc_oneshot_unit_init_cfg_t init_config1 = {
        .unit_id = ADC_UNIT_1,
    };
    ESP_ERROR_CHECK(adc_oneshot_new_unit(&init_config1, &adc1_handle));
    adc_oneshot_chan_cfg_t config = {
        .bitwidth = ADC_BITWIDTH_DEFAULT,
        .atten = ADC_ATTEN_DB_12, 
    };
    ESP_ERROR_CHECK(adc_oneshot_config_channel(adc1_handle, TEMP_ADC_CHAN, &config));
    ESP_ERROR_CHECK(adc_oneshot_config_channel(adc1_handle, HUM_ADC_CHAN, &config));
    
    ESP_LOGI(TAG, "ADC initialized on Channels 4 (GPIO4) and 5 (GPIO5)");
}

void update_motor_from_mqtt(int is_speed, int value) {
    if (is_speed) {
        if (value < 0) value = 0;
        if (value > 255) value = 255;
        current_motor_speed = value;
        ESP_LOGI(TAG, "Command Received: Speed set to %d", current_motor_speed);
    } else {
        current_motor_state = (value != 0) ? 1 : 0;
        ESP_LOGI(TAG, "Command Received: Motor state set to %d", current_motor_state);
    }

    int duty = (current_motor_state == 1) ? current_motor_speed : 0;
    ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0, duty);
    ledc_update_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0);
}

static void sensor_publish_task(void *pvParameters) {
    int raw_temp = 0;
    int raw_hum = 0;
    
    while (1) {
        vTaskDelay(pdMS_TO_TICKS(5000));
        
        ESP_ERROR_CHECK(adc_oneshot_read(adc1_handle, TEMP_ADC_CHAN, &raw_temp));
        ESP_ERROR_CHECK(adc_oneshot_read(adc1_handle, HUM_ADC_CHAN, &raw_hum));
        
        int temp_mapped = (raw_temp * 50) / 4095;
        int hum_mapped = (raw_hum * 100) / 4095;
        
        ESP_LOGI(TAG, "Publishing -> Raw T: %d (%dC) | Raw H: %d (%d%%)", 
                 raw_temp, temp_mapped, raw_hum, hum_mapped);
        
        mqtt_publish_sensor_data(1, temp_mapped);
        mqtt_publish_sensor_data(0, hum_mapped);
    }
}

#ifndef WIFI_SSID
#define WIFI_SSID "JCMP01"
#endif

#ifndef WIFI_PASS
#define WIFI_PASS "wgvo2919"
#endif

#ifndef CONFIG_MQTT_BROKER_URI
#define CONFIG_MQTT_BROKER_URI "mqtt://test.mosquitto.org:1883"
#endif

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

    ESP_LOGI(TAG, "Connecting to Wi-Fi SSID: %s", WIFI_SSID);

    esp_err_t wifi_err = wifi_sta_connect(WIFI_SSID, WIFI_PASS, 30000); 
    if (wifi_err != ESP_OK) {
        ESP_LOGE(TAG, "Wi-Fi connect failed: %s (0x%x)", esp_err_to_name(wifi_err), wifi_err);
        while (1) { vTaskDelay(pdMS_TO_TICKS(1000)); }
    }
    ESP_LOGI(TAG, "Wi-Fi connected!");
    
    motor_init();
    analog_init();

    ESP_LOGI(TAG, "Starting MQTT client: %s", CONFIG_MQTT_BROKER_URI);
    ESP_ERROR_CHECK(mqtt_app_start(CONFIG_MQTT_BROKER_URI));

    xTaskCreate(sensor_publish_task, "sensor_pub", 2048, NULL, 5, NULL);

    while (1) {
        vTaskDelay(pdMS_TO_TICKS(10000));
    }
}