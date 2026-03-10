#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include "esp_log.h"
#include "esp_event.h"
#include "mqtt_client.h"
#include "driver/gpio.h"
#include "driver/ledc.h"
#include "esp_adc/adc_oneshot.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "MQTT_LAB";


#define IS_DEVICE_1 1

//same pins on both esp
#define GPIO_LED_A   GPIO_NUM_20
#define GPIO_LED_B   GPIO_NUM_21
#define GPIO_BUTTON_A GPIO_NUM_15
#define GPIO_BUTTON_B GPIO_NUM_3
#define ADC_CHAN      ADC_CHANNEL_2 

#define TEAM_ID "teamWhopper"
#define BASE_TOPIC "ibero/ei2/" TEAM_ID "/"


#if IS_DEVICE_1
    //ESP 1 publish buttons and pot
    #define PUB_BTN_A BASE_TOPIC "button1"
    #define PUB_BTN_B BASE_TOPIC "button2"
    #define PUB_POT   BASE_TOPIC "pot1"
    // And sub to ESP 2
    #define SUB_BTN_A BASE_TOPIC "button3"
    #define SUB_BTN_B BASE_TOPIC "button4"
    #define SUB_POT   BASE_TOPIC "pot2"
#else
    //ESP 2 publish buttons and pot2
    #define PUB_BTN_A BASE_TOPIC "button3"
    #define PUB_BTN_B BASE_TOPIC "button4"
    #define PUB_POT   BASE_TOPIC "pot2"
    // And subscribe to ESP1
    #define SUB_BTN_A BASE_TOPIC "button1"
    #define SUB_BTN_B BASE_TOPIC "button2"
    #define SUB_POT   BASE_TOPIC "pot1"
#endif

static esp_mqtt_client_handle_t client = NULL;
static adc_oneshot_unit_handle_t adc1_handle;


static int led_a_state = 0;
static int led_b_state = 0; 
static int current_pwm_duty = 0; 


// Update pwm in the leds
static void update_leds() {
    int duty_a = led_a_state ? current_pwm_duty : 0;
    int duty_b = led_b_state ? current_pwm_duty : 0;

    ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0, duty_a);
    ledc_update_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0);

    ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_1, duty_b);
    ledc_update_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_1);
}


//Reading and publishing

static void publisher_task(void *pvParameters)
{
    int last_button_a = 1;
    int last_button_b = 1;
    int last_pot_8bit = -1;

    while (1)
    {
        // pull up
        int button_a = gpio_get_level(GPIO_BUTTON_A);
        int button_b = gpio_get_level(GPIO_BUTTON_B);

        // detect button A
        if (button_a == 0 && last_button_a == 1) {
            esp_mqtt_client_publish(client, PUB_BTN_A, "TOGGLE", 0, 0, 0);
        }
        last_button_a = button_a;

        // detect button B
        if (button_b == 0 && last_button_b == 1) {
            esp_mqtt_client_publish(client, PUB_BTN_B, "TOGGLE", 0, 0, 0);
        }
        last_button_b = button_b;

        // read ADC
        int adc_raw;
        adc_oneshot_read(adc1_handle, ADC_CHAN, &adc_raw);
        int pot_8bit = (adc_raw * 255) / 4095; 

        // Publishing
        if (abs(pot_8bit - last_pot_8bit) > 2) {
            char msg[10];
            sprintf(msg, "%d", pot_8bit);
            esp_mqtt_client_publish(client, PUB_POT, msg, 0, 0, 0);
            last_pot_8bit = pot_8bit;
        }

        vTaskDelay(pdMS_TO_TICKS(50)); 
    }
}

//mqtt handler

static void mqtt_event_handler(void *handler_args, esp_event_base_t base,
                               int32_t event_id, void *event_data)
{
    esp_mqtt_event_handle_t event = event_data;

    switch (event_id)
    {
    case MQTT_EVENT_CONNECTED:
        ESP_LOGI(TAG, "MQTT connected");
        // subscribing to the other esp topics
        esp_mqtt_client_subscribe(client, SUB_BTN_A, 0);
        esp_mqtt_client_subscribe(client, SUB_BTN_B, 0);
        esp_mqtt_client_subscribe(client, SUB_POT, 0);
        break;

    case MQTT_EVENT_DATA:
    {
        //Check if is the remote A
        if (event->topic_len == strlen(SUB_BTN_A) &&
            memcmp(event->topic, SUB_BTN_A, event->topic_len) == 0)
        {
            if (event->data_len >= 6 && memcmp(event->data, "TOGGLE", 6) == 0) {
                led_a_state = !led_a_state; 
                update_leds();
                ESP_LOGI(TAG, "LED A state toggled to: %d", led_a_state);
            }
        }
        // check if is the remote B
        else if (event->topic_len == strlen(SUB_BTN_B) &&
                 memcmp(event->topic, SUB_BTN_B, event->topic_len) == 0)
        {
            if (event->data_len >= 6 && memcmp(event->data, "TOGGLE", 6) == 0) {
                led_b_state = !led_b_state;
                update_leds();
                ESP_LOGI(TAG, "LED B state toggled to: %d", led_b_state);
            }
        }
        // Check pot
        else if (event->topic_len == strlen(SUB_POT) &&
                 memcmp(event->topic, SUB_POT, event->topic_len) == 0)
        {
            char data_str[10];
            int len = event->data_len < 9 ? event->data_len : 9;
            memcpy(data_str, event->data, len);
            data_str[len] = '\0';

            current_pwm_duty = atoi(data_str);
            update_leds();
        }
        break;
    }
    default:
        break;
    }
}



esp_err_t mqtt_app_start(const char *broker_uri)
{
   
    gpio_config_t led_conf = {
        .pin_bit_mask = (1ULL << GPIO_LED_A) | (1ULL << GPIO_LED_B),
        .mode = GPIO_MODE_OUTPUT};
    gpio_config(&led_conf);

   
    gpio_config_t button_conf = {
        .pin_bit_mask = (1ULL << GPIO_BUTTON_A) | (1ULL << GPIO_BUTTON_B),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE};
    gpio_config(&button_conf);

    //8bit configurated pwm
    ledc_timer_config_t ledc_timer = {
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .timer_num = LEDC_TIMER_0,
        .duty_resolution = LEDC_TIMER_8_BIT, 
        .freq_hz = 5000};
    ledc_timer_config(&ledc_timer);

    ledc_channel_config_t ledc_channel1 = {
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .channel = LEDC_CHANNEL_0,
        .timer_sel = LEDC_TIMER_0,
        .gpio_num = GPIO_LED_A,
        .duty = 0};
    ledc_channel_config(&ledc_channel1);

    ledc_channel_config_t ledc_channel2 = {
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .channel = LEDC_CHANNEL_1,
        .timer_sel = LEDC_TIMER_0,
        .gpio_num = GPIO_LED_B,
        .duty = 0};
    ledc_channel_config(&ledc_channel2);

    //ADC
    adc_oneshot_unit_init_cfg_t init_config1 = {
        .unit_id = ADC_UNIT_1};
    adc_oneshot_new_unit(&init_config1, &adc1_handle);

    adc_oneshot_chan_cfg_t config = {
        .bitwidth = ADC_BITWIDTH_DEFAULT,
        .atten = ADC_ATTEN_DB_12};
    adc_oneshot_config_channel(adc1_handle, ADC_CHAN, &config);

    // MQTT
    esp_mqtt_client_config_t cfg = {
        .broker.address.uri = broker_uri};
    client = esp_mqtt_client_init(&cfg);

    esp_mqtt_client_register_event(client, ESP_EVENT_ANY_ID, mqtt_event_handler, NULL);
    esp_mqtt_client_start(client);

    xTaskCreate(publisher_task, "publisher", 4096, NULL, 5, NULL);

    return ESP_OK;
}