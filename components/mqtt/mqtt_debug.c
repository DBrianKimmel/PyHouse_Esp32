/*
 * mwtt_debug.c
 *
 *  Created on: Feb 24, 2017
 *      Author: briank
 */

#include "esp_log.h"
#include "esp_err.h"

#include "mqtt.h"
#include "mqtt_message.h"

static const char *TAG = "MqttDebug";

void print_client(Client_t *p_client) {
	ESP_LOGD(TAG, "ClientDebug - Client:%p", p_client);
	ESP_LOGD(TAG, "ClientDebug - State:%p; Buffers:%p", p_client->State, p_client->Buffers);
	ESP_LOGD(TAG, "ClientDebug - Broker:%p; Will:%p", p_client->Broker, p_client->Will);
	ESP_LOGD(TAG, "ClientDebug - Cb:%p; Packet:%p", p_client->Cb, p_client->Packet);
	ESP_LOGD(TAG, "ClientDebug - SendingQueue:%p; RingBuff:%p", p_client->xSendingQueue, p_client->send_rb);
}

// ### END DBK
