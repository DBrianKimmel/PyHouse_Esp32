/*
 * mwtt_debug.c
 *
 *  Created on: Feb 24, 2017
 *      Author: briank
 */

#include "esp_log.h"
 //#include "esp_err.h"

#include "mqtt.h"
#include "mqtt_debug.h"
//#include "mqtt_message.h"

static const char *TAG = "MqttDebug";

void print_client(Client_t *p_client) {
	ESP_LOGD(TAG, "ClientDebug - Client:%p", p_client);
	ESP_LOGD(TAG, "ClientDebug - State:%p; Buffers:%p", p_client->State, p_client->Buffers);
	ESP_LOGD(TAG, "ClientDebug - Broker:%p; Will:%p", p_client->Broker, p_client->Will);
	ESP_LOGD(TAG, "ClientDebug - Cb:%p; Packet:%p", p_client->Cb, p_client->Packet);
	ESP_LOGD(TAG, "ClientDebug - SendingQueue:%p; RingBuff:%p", p_client->xSendingQueue, p_client->send_rb);
}

void print_packet(PacketInfo_t *p_packet) {
	ESP_LOGD(TAG, "PacketDebug - Packet:%p;  Type:%d;  Id:%d", p_packet, p_packet->PacketType, p_packet->PacketId);
	ESP_LOGD(TAG, "PacketDebug - BuffersLen:%d;  Buffer:%p", p_packet->PacketBuffer_length, p_packet->PacketBuffer);
	ESP_LOGD(TAG, "PacketDebug - TopicLen:%d;  Topic:%p", p_packet->PacketTopic_length, p_packet->PacketTopic);
	ESP_LOGD(TAG, "PacketDebug - PayloadLen:%d;  PayloadOffset:%d;  Payload:%p", p_packet->PacketPayload_length, p_packet->PacketPayload_offset, p_packet->PacketPayload);
//	ESP_LOGD(TAG, "PacketDebug - SendingQueue:%p;  RingBuff:%p", p_packet->xSendingQueue, p_packet->send_rb);
}

// ### END DBK
