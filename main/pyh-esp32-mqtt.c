/*
 * esp32-mqtt.c
 *
 *  Created on: Jan 26, 2017
 *      Author: briank
 */

/**
 * @file pyh_esp32_mqtt.c
 * @brief Provide the Mqtt functionality for the PyHouse Esp32 package.
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
//#include "freertos/task.h"
//#include "freertos/semphr.h"
#include "freertos/queue.h"
#include "freertos/event_groups.h"

#include "mqtt.h"

#include "pyh-esp32-mqtt.h"

#include "sdkconfig.h"

#define MQTT_QOS_0 0
#define MQTT_RETAIN_0 0
#define MQTT_CLIENT_ID "PyH-00001"

static const char *TAG = "PyH_Mqtt";

/* FreeRTOS event group to signal when we are connected & ready to make a request */
static EventGroupHandle_t l_mqtt_event_group;
const int MQTT_CONNECTED_BIT = BIT0;
const int MQTT_SUBSCRIBED_BIT = BIT1;

static uint8_t 				g_BufferIn[1024];
static uint8_t 				g_BufferOut[1024];
QueueHandle_t  				g_QueueHandle;


/*
 *
 */
RINGBUF g_RingBufer = {
};

/*
 * The payload portion of a MQTT message
 */
mqtt_message_t g_Payload = {
	.PayloadData = 			(uint8_t*)"",
	.PayloadLength = 		0,
};

/*
 *
 */
mqtt_connect_info_t g_ConnectInfo = {
		.client_id =		"",
		.will_topic =		"",
		.will_message =		"",
		.keepalive =		0,
		.will_qos =			0,
		.will_retain =		0,
		.clean_session = 	0,
};

/**
 *
 */
BrokerConfig g_Broker = {
		.Host =             CONFIG_MQTT_HOST_NAME,
		.Port =             CONFIG_MQTT_HOST_PORT,
		.Client_id =        MQTT_CLIENT_ID,
		.Username =         CONFIG_MQTT_HOST_USERNAME,
		.Password =         CONFIG_MQTT_HOST_PASSWORD,
};

/**
 *
 */
Callback g_CallBack = {
		.connected_cb =		*cb_connected,
		.disconnected_cb =	*cb_disconnected,
		.reconnect_cb =		*cb_reconnected,
		.subscribe_cb =		*cb_subscribe,
		.publish_cb =		*cb_publish,
		.data_cb =			*cb_data,

};

/**
 *
 */
LastWill g_LastWill = {
		.Will_topic =		"pyhouse/lwt",
		.Will_msg =			"offline",
		.Will_qos =			MQTT_QOS_0,
		.Will_retain =		MQTT_RETAIN_0,
		.clean_session =	0,
};

/**
 *
 */
MessageInfo g_MessageInfo = {
	.MessageType =					0,
	.MessageTopic =					"",
	.MessageTopic_length = 			0,
	.MessagePayload =				"",
	.MessagePayload_length = 		0,
	.MessagePayload_offset = 		0,
	.Message_total_length = 		0,
};

/**
 *
 */
State g_State = {
		.in_buffer =			g_BufferIn,
		.out_buffer =			g_BufferOut,
		.in_buffer_length =		1024,
		.out_buffer_length =	1024,
		.message_length =		0,
		.message_length_read =	0,
		.outbound_message =		&g_Payload,
		.mqtt_connection =		&g_ConnectInfo,
		.pending_msg_id = 		0,
		.pending_msg_type =		0,
		.pending_publish_qos =	0,
};

/*
 * Information about this MQTT client
 */
Client g_Client  = {
	.Settings =         		&g_Settings,
	.State =					&g_State,
	.Broker =					&g_Broker,
	.Will =						&g_LastWill,
	.Cb = 						&g_CallBack,
//	.Connection_info =			&g_ConnectInfo,
	.xSendingQueue = 			&g_QueueHandle,
//	.send_rb = 					&g_RingBufer,
	.Keep_alive =				0,
	.Socket =					0,
};





void cb_connected(void *p_self, void *p_params) {
	ESP_LOGI(TAG, "Connected Callback\n");
	xEventGroupSetBits(l_mqtt_event_group, MQTT_CONNECTED_BIT);
	Client *l_client = (Client *) p_self;
	mqtt_subscribe(l_client, "pyhouse/#", MQTT_QOS_0);
	mqtt_publish(l_client, "pyhouse/Esp32", "Hello from Esp32 development.", 29, MQTT_QOS_0, MQTT_RETAIN_0);
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
//	mqtt_client *client = (mqtt_client *) self;
//	mqtt_publish(client, "/test", "abcde", 5, 0, 0);
}

void cb_publish(void *self, void *params) {
	ESP_LOGI(TAG, "Publish Callback\n");
}

void cb_data(void *self, void *params) {
	ESP_LOGI(TAG, "Data Callback\n");
//	mqtt_client *client = (mqtt_client *) self;
//	mqtt_event_data_t *event_data = (mqtt_event_data_t *) params;

//	if (event_data->data_offset == 0) {
//		char *topic = malloc(event_data->topic_length + 1);
//		memcpy(topic, event_data->topic, event_data->topic_length);
//		topic[event_data->topic_length] = 0;
//		ESP_LOGI(TAG, "[APP] Publish topic: %s\n", topic);
//		free(topic);
//	}
	// char *data = malloc(event_data->data_length + 1);
	// memcpy(data, event_data->data, event_data->data_length);
	// data[event_data->data_length] = 0;
//	ESP_LOGI(TAG, "[APP] Publish data[%d/%d bytes]\n",
//			event_data->data_length + event_data->data_offset,
//			event_data->data_total_length);
	// data);
	// free(data);
}


void pyh_mqtt_start(){
	ESP_LOGI(TAG, "Mqtt Starting.");
	ESP_ERROR_CHECK(mqtt_start(&g_Client));
	// mqtt_task(&g_Client);
	// ESP_ERROR_CHECK(mqtt_connect(&g_Client));
	ESP_LOGI(TAG, "Mqtt Started.\n");
}

static esp_err_t mqtt_event_handler(void *ctx, system_event_t *p_event) {
    switch(p_event->event_id) {
    case SYSTEM_EVENT_STA_START:
        break;
    case SYSTEM_EVENT_STA_GOT_IP:
        mqtt_start(&g_Client);
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
	ESP_ERROR_CHECK(mqtt_init(&g_Client))
	ESP_LOGI(TAG, "Mqtt Initialized.\n");
}

// ### END DBK