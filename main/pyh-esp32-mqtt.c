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
#include <string.h>
#include "esp_log.h"

#include "mqtt.h"
#include "pyh-esp32-mqtt.h"
#include "sdkconfig.h"

static const char *TAG = "PyH_Mqtt      ";

Client_t   g_ClientPtr;


/*
 * Second stage - this will connect to the broker and begin the communication.
 */
void pyh_mqtt_start(){
	ESP_LOGI(TAG, " 41 Mqtt Starting.");
	char l_will_topic[32];
	char l_will_message[32];
	char l_subscribe[32];
	int l_qos = 0;

	snprintf(l_will_topic, sizeof(l_will_topic), "pyhouse/%s/lwt", CONFIG_PYHOUSE_HOUSE_NAME);
	snprintf(l_will_message, sizeof(l_will_message),  "%s Offline", CONFIG_MQTT_CLIENT_ID);
	snprintf(l_subscribe, sizeof(l_subscribe), "pyhouse/%s/#", CONFIG_PYHOUSE_HOUSE_NAME);
	ESP_ERROR_CHECK(Mqtt_start(&g_ClientPtr, l_will_topic, l_will_message));
	ESP_LOGI(TAG, " 47 Mqtt Started.\n");
}

/*
 * first stage - this will allocate memory and setup preparations.
 */
void pyh_mqtt_init() {
	ESP_LOGI(TAG, " 54 PyH-Mqtt Init - Begin - Client:%p", &g_ClientPtr);
	ESP_ERROR_CHECK(Mqtt_init(&g_ClientPtr));
	ESP_LOGI(TAG, " 56 Mqtt Initialized.\n");
}

// ### END DBK
