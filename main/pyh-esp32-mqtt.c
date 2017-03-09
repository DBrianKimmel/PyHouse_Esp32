/*
 * esp32-mqtt.c
 *
 *  Created on: Jan 26, 2017
 *      Author: briank
 */

/**
 * @file pyh_esp32_mqtt.c
 * @brief Provide the MQTT functionality for the PyHouse Esp32 package.
 *
 * Set up the data for MQTT:
 * 		Broker Info:
 * 			Host, Port, Username(opt), Password(opt)
 * 		Last Will info
 * 			Topic, Message
 * 		Client Info:
 * 			ClientId, Subscribe topic,
 * First we must Initialize this package.
 * Then we
 */

#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <string.h>

#include "esp_system.h"
#include "esp_log.h"
#include "esp_event_loop.h"

#include "freertos/event_groups.h"

#include "mqtt.h"

#include "pyh-esp32-mqtt.h"

#include "sdkconfig.h"

#define MQTT_QOS_0 0
#define MQTT_RETAIN_0 0
// Ideally the numeric portion would be a serial number of this ESP32
// The "PyH-Esp32" portion is requires by the rest of the PyHouse system and identifies that this package is in use.
#define PYH_MQTT_CLIENT_ID "PyH-Esp32-001"

static const char *TAG = "PyH_Mqtt      ";

/* FreeRTOS event group to signal when we are connected & ready to make a request */
static EventGroupHandle_t l_mqtt_event_group;
const int MQTT_CONNECTED_BIT = BIT0;
const int MQTT_SUBSCRIBED_BIT = BIT1;



QueueHandle_t  				g_QueueHandle;

/*
 *
 */
Ringbuff_t   g_RingBufer;

/*
 * The payload portion of a MQTT message
 */
mqtt_message_t   g_Payload = {
//	.PayloadData = 			(uint8_t*)"",
//	.PayloadLength = 		0,
};

/**
 *
 */
BrokerConfig_t   g_Broker = {
//	.Host =             	CONFIG_MQTT_HOST_NAME,
//	.Port =             	CONFIG_MQTT_HOST_PORT,
//	.ClientId =	        	PYH_MQTT_CLIENT_ID,
//	.Username =         	CONFIG_MQTT_HOST_USERNAME,
//	.Password =         	CONFIG_MQTT_HOST_PASSWORD,
};

/**
 *
 */
Callback_t   g_CallBack = {
//	.connected_cb =			*cb_connected,
//	.disconnected_cb =		*cb_disconnected,
//	.reconnect_cb =			*cb_reconnected,
//	.subscribe_cb =			*cb_subscribe,
//	.publish_cb =			*cb_publish,
//	.data_cb =				*cb_data,

};

/**
 * Mqtt Packet = message
 */
PacketInfo_t   g_Packet = {
//	.PacketType =					0,
//	.PacketId =						0,
//	.PacketStart =					0,
//	.PacketTopic =					(uint8_t*)"",
//	.PacketTopic_length = 			0,
//	.PacketPayload =				(uint8_t*)"",
//	.PacketPayload_length = 		0,
//	.PacketPayload_offset = 		0,
//	.Packet_length = 				0,
//	.PacketBuffer =					(uint8_t*)0,
//	.PacketBuffer_length =			0,
};

/**
 *
 */
State_t   g_State = {
//	.message_length =				0,
//	.message_length_read =			0,
//	.outbound_message =				&g_Payload,
//	.pending_msg_id = 				0,
//	.pending_msg_type =				0,
//	.pending_publish_qos =			0,
};
/*
 *
 */
Buffers_t   g_Buffers = {
//	.in_buffer =					g_BufferIn,
//	.out_buffer =					g_BufferOut,
//	.in_buffer_length =				sizeof(g_BufferIn),
//	.out_buffer_length =			sizeof(g_BufferOut),
};

/*
 * Information about this MQTT client
 */
Client_t   g_Client;
















void cb_connected(void *p_self, void *p_params) {
	ESP_LOGI(TAG, "170 Connected Callback\n");
	xEventGroupSetBits(l_mqtt_event_group, MQTT_CONNECTED_BIT);
	Client_t *l_client = (Client_t *) p_self;
	mqtt_subscribe(l_client, "pyhouse/#", MQTT_QOS_0);
	mqtt_publish(l_client, "pyhouse/Esp32", "Hello from Esp32 development.", 29, MQTT_QOS_0, MQTT_RETAIN_0);
}



void cb_disconnected(void *p_self, void *p_params) {
	ESP_LOGI(TAG, "DisConnected Callback\n");
	xEventGroupClearBits(l_mqtt_event_group, MQTT_CONNECTED_BIT);
}



void cb_reconnected(void *p_self, void *p_params) {
	ESP_LOGI(TAG, "ReConnect Callback\n");
}



void cb_subscribe(void *p_self, void *p_params) {
	ESP_LOGI(TAG, "Subscribe callback ok, test publish msg\n");
	xEventGroupSetBits(l_mqtt_event_group, MQTT_SUBSCRIBED_BIT);
	Client_t *l_client = (Client_t *) p_self;
	mqtt_publish(l_client, "/test", "abcde", 5, 0, 0);
}



void cb_publish(void *p_self, void *p_params) {
	ESP_LOGI(TAG, "Publish Callback\n");
}



void cb_data(void *p_self, void *p_params) {
	ESP_LOGI(TAG, "206 Data Callback\n");
	Client_t *l_client = (Client_t *) p_self;
//	mqtt_event_data_t *event_data = (mqtt_event_data_t *) p_params;

//	if (event_data->data_offset == 0) {
//		char *topic = malloc(event_data->topic_length + 1);
//		memcpy(topic, event_data->topic, event_data->topic_length);
//		topic[event_data->topic_length] = 0;
//		ESP_LOGI(TAG, "[APP] Publish topic: %s\n", topic);
//		free(topic);
//	}
//	char *data = malloc(event_data->data_length + 1);
//	memcpy(data, event_data->data, event_data->data_length);
//	data[event_data->data_length] = 0;
//	ESP_LOGI(TAG, "[APP] Publish data[%d/%d bytes]\n",
//			event_data->data_length + event_data->data_offset,
//			event_data->data_total_length);
	// data);
	// free(data);
}


static esp_err_t mqtt_event_handler(void *ctx, system_event_t *p_event) {
    switch(p_event->event_id) {
    case SYSTEM_EVENT_STA_START:
        break;
    case SYSTEM_EVENT_STA_GOT_IP:
        Mqtt_start(&g_Client, "a", "b");
        break;
    case SYSTEM_EVENT_STA_DISCONNECTED:
        //mqtt_stop();
        break;
    default:
        break;
    }
    return ESP_OK;
}

/*
 * Second stage - this will connect to the broker and begin the communication.
 */
void pyh_mqtt_start(){
	ESP_LOGI(TAG, "Mqtt Starting.");
	char l_will_topic[32];
	char l_will_message[32];
	snprintf(l_will_topic, sizeof(l_will_topic), "pyhouse/%s/lwt", CONFIG_PYHOUSE_HOUSE_NAME);
	snprintf(l_will_message, sizeof(l_will_message),  "%s Offline", CONFIG_MQTT_CLIENT_ID);
	ESP_ERROR_CHECK(Mqtt_start(&g_Client, l_will_topic, l_will_message));
	// mqtt_task(&g_Client);
	// ESP_ERROR_CHECK(mqtt_connect(&g_Client));
	ESP_LOGI(TAG, "Mqtt Started.\n");
}

/*
 * first stage - this will allocate memory and setup preparations.
 */
void pyh_mqtt_init() {
	ESP_LOGI(TAG, "Mqtt Initializing");
	l_mqtt_event_group = xEventGroupCreate();
	ESP_ERROR_CHECK(Mqtt_init(& g_Client));
	ESP_LOGI(TAG, "Mqtt Initialized.\n");
}

// ### END DBK
