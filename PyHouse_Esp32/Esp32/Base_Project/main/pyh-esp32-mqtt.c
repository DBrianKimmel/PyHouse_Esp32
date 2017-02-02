/*
 * esp32-mqtt.c
 *
 *  Created on: Jan 26, 2017
 *      Author: briank
 */

#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <string.h>

#include "soc/rtc_cntl_reg.h"

#include "esp_system.h"
#include "esp_log.h"
#include "esp_event_loop.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "freertos/queue.h"
#include "freertos/event_groups.h"

#include "mqtt.h"

#include "sdkconfig.h"

#define MQTT_QOS_0 0
#define MQTT_RETAIN_0 0
#define MQTT_CLIENT_ID "PyH-00001"

static const char *TAG = "PyH_Mqtt";

/* FreeRTOS event group to signal when we are connected & ready to make a request */
static EventGroupHandle_t l_mqtt_event_group;
const int MQTT_CONNECTED_BIT = BIT0;
const int MQTT_SUBSCRIBED_BIT = BIT1;

void cb_connected(void *self, void *params) {
	ESP_LOGI(TAG, "Connected Callback\n");
	xEventGroupSetBits(l_mqtt_event_group, MQTT_CONNECTED_BIT);
	mqtt_client *client = (mqtt_client *) self;
	mqtt_subscribe(client, "pyhouse/#", MQTT_QOS_0);
	mqtt_publish(client, "pyhouse/Esp32", "Hello from Esp32 development.", 29, MQTT_QOS_0, MQTT_RETAIN_0);
}

void cb_disconnected(void *self, void *params) {
	ESP_LOGI(TAG, "DisConnected Callback\n");
	xEventGroupClearBits(l_mqtt_event_group, MQTT_CONNECTED_BIT);
}

void cb_reconnected(void *self, void *params) {
	ESP_LOGI(TAG, "ReConnect Callback\n");
}

void cb_subscribe(void *self, void *params) {
	ESP_LOGI(TAG, "Subscribe callback ok, test publish msg\n");
	xEventGroupSetBits(l_mqtt_event_group, MQTT_SUBSCRIBED_BIT);
	mqtt_client *client = (mqtt_client *) self;
	mqtt_publish(client, "/test", "abcde", 5, 0, 0);
}

void cb_publish(void *self, void *params) {
	ESP_LOGI(TAG, "Publish Callback\n");
}

void cb_data(void *self, void *params) {
	ESP_LOGI(TAG, "Data Callback\n");
	mqtt_client *client = (mqtt_client *) self;
	mqtt_event_data_t *event_data = (mqtt_event_data_t *) params;

	if (event_data->data_offset == 0) {
		char *topic = malloc(event_data->topic_length + 1);
		memcpy(topic, event_data->topic, event_data->topic_length);
		topic[event_data->topic_length] = 0;
		ESP_LOGI(TAG, "[APP] Publish topic: %s\n", topic);
		free(topic);
	}
	// char *data = malloc(event_data->data_length + 1);
	// memcpy(data, event_data->data, event_data->data_length);
	// data[event_data->data_length] = 0;
	ESP_LOGI(TAG, "[APP] Publish data[%d/%d bytes]\n",
			event_data->data_length + event_data->data_offset,
			event_data->data_total_length);
	// data);
	// free(data);
}

mqtt_settings l_settings = {
		.host = CONFIG_MQTT_HOST_NAME,
		.port = CONFIG_MQTT_HOST_PORT,
		.client_id = MQTT_CLIENT_ID,
		.username = "user",
		.password = "pass",
		.clean_session = 0, .keepalive = 120,
		.lwt_topic = "/lwt", .lwt_msg = "offline", .lwt_qos = MQTT_QOS_0, .lwt_retain = MQTT_RETAIN_0,
		.connected_cb = cb_connected,
		.disconnected_cb = cb_disconnected,
		.reconnect_cb = cb_reconnected,
		.subscribe_cb = cb_subscribe,
		.publish_cb = cb_publish,
		.data_cb = cb_data,
};

void pyh_mqtt_start(){
	ESP_LOGI(TAG, "Mqtt Starting.");
	mqtt_start(&l_settings);
	ESP_LOGI(TAG, "Mqtt Started.\n");
}

static esp_err_t mqtt_event_handler(void *ctx, system_event_t *p_event) {
    switch(p_event->event_id) {
    case SYSTEM_EVENT_STA_START:
        break;
    case SYSTEM_EVENT_STA_GOT_IP:
        mqtt_start(&l_settings);
        break;
    case SYSTEM_EVENT_STA_DISCONNECTED:
        //mqtt_stop();
        break;
    default:
        break;
    }
    return ESP_OK;
}

void pyh_mqtt_init() {
	ESP_LOGI(TAG, "Mqtt Initializing");
	l_mqtt_event_group = xEventGroupCreate();
	ESP_LOGI(TAG, "Mqtt Initialized.\n");
}
// ### END DBK
