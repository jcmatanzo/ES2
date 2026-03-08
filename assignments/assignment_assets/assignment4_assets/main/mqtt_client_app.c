#include <string.h>
#include <stdlib.h>
#include "esp_log.h"
#include "mqtt_client.h"

static const char *TAG = "MQTT_LAB";

#define TEAM_ID     "teamWhopper"
#define DEVICE_ID   "c6_01"
#define TOPIC_MOTOR "ibero/ei2/" TEAM_ID "/" DEVICE_ID "/cmd/motor"
#define TOPIC_SPEED "ibero/ei2/" TEAM_ID "/" DEVICE_ID "/cmd/speed"
#define TOPIC_TEMP  "ibero/ei2/" TEAM_ID "/" DEVICE_ID "/read/temp"
#define TOPIC_HUM   "ibero/ei2/" TEAM_ID "/" DEVICE_ID "/read/hum"

static esp_mqtt_client_handle_t client = NULL;

extern void update_motor_from_mqtt(int is_speed, int value);

static void mqtt_event_handler(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data)
{
    esp_mqtt_event_handle_t event = (esp_mqtt_event_handle_t) event_data;

    switch ((esp_mqtt_event_id_t)event_id) {

    case MQTT_EVENT_CONNECTED:
        ESP_LOGI(TAG, "MQTT connected");
        esp_mqtt_client_subscribe(client, TOPIC_MOTOR, 0);
        esp_mqtt_client_subscribe(client, TOPIC_SPEED, 0);
        break;

    case MQTT_EVENT_DISCONNECTED:
        ESP_LOGW(TAG, "MQTT disconnected");
        break;

    case MQTT_EVENT_DATA:
    
        if (strncmp(event->topic, TOPIC_MOTOR, event->topic_len) == 0) {
            char val_str[16] = {0};
            snprintf(val_str, sizeof(val_str), "%.*s", event->data_len, event->data);
            int state = atoi(val_str);
            update_motor_from_mqtt(0, state);
        } 
        else if (strncmp(event->topic, TOPIC_SPEED, event->topic_len) == 0) {
            char val_str[16] = {0};
            snprintf(val_str, sizeof(val_str), "%.*s", event->data_len, event->data);
            int speed = atoi(val_str);
            update_motor_from_mqtt(1, speed);
        }
        break;

    case MQTT_EVENT_ERROR:
        ESP_LOGE(TAG, "MQTT error");
        break;

    default:
        break;
    }
}

void mqtt_publish_sensor_data(int is_temp, int value)
{
    if (client == NULL) return;
    
    char payload[16];
    snprintf(payload, sizeof(payload), "%d", value);
    
    if (is_temp) {
        esp_mqtt_client_publish(client, TOPIC_TEMP, payload, 0, 0, 0);
    } else {
        esp_mqtt_client_publish(client, TOPIC_HUM, payload, 0, 0, 0);
    }
}

esp_err_t mqtt_app_start(const char *broker_uri)
{
    esp_mqtt_client_config_t cfg = {
        .broker.address.uri = broker_uri,   
        .session.keepalive = 30,
    };

    client = esp_mqtt_client_init(&cfg);
    esp_mqtt_client_register_event(client, ESP_EVENT_ANY_ID, mqtt_event_handler, NULL);
    esp_mqtt_client_start(client);
    
    return ESP_OK;
}