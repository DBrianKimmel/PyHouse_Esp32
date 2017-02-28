/*
 * mwtt_debug.c
 *
 *  Created on: Feb 24, 2017
 *      Author: briank
 */

#include <stdio.h>

#include "esp_log.h"
#include "esp_system.h"

#include "mqtt.h"
#include "mqtt_debug.h"

static const char *TAG = "MqttDebug";


/**
 * Print out a string
 */
void print_string(char *p_string) {
	ESP_LOGD(TAG, "String:%s", p_string);
}

char *dump_packet(uint8_t *p_buff, int p_len) {
	int l_ix, l_outix = 0;
	char *l_str = malloc(512);
	char *l_chr = malloc(6);
	char l_byte;
	for (l_ix = 0; l_ix < p_len; l_ix++) {
		l_byte = p_buff[l_ix];
		if (l_byte < 32 || l_byte > 127) {
			sprintf(l_chr, "\\%02X ", p_buff[l_ix]);
			memcpy(&l_str[l_outix], l_chr, 3);
			l_str[l_outix+2] = ' ';
			l_outix += 3;
		} else {
			l_str[l_outix++] = l_byte;
		}
	}
	l_str[++l_ix] = '<';
	l_str[++l_ix] = 0;
	return l_str;
}

/**
 * Print put the current heap sizes
 */
void print_heap(void) {
	int l_heap = esp_get_free_heap_size();
	ESP_LOGI(TAG, " 42 Start - Heap: (0x%X) %d", l_heap, l_heap);
}

void print_client(Client_t *p_client) {
	ESP_LOGD(TAG, "ClientDebug - Client:%p", p_client);
	ESP_LOGD(TAG, "ClientDebug - State:%p; Buffers:%p", p_client->State, p_client->Buffers);
	ESP_LOGD(TAG, "ClientDebug - Broker:%p; Will:%p", p_client->Broker, p_client->Will);
	ESP_LOGD(TAG, "ClientDebug - Cb:%p; Packet:%p", p_client->Cb, p_client->Packet);
	ESP_LOGD(TAG, "ClientDebug - SendingQueue:%p; RingBuff:%p", p_client->xSendingQueue, p_client->send_rb);
	ESP_LOGD(TAG, " ");
}

void print_packet(PacketInfo_t *p_packet) {
	ESP_LOGD(TAG, "PacketDebug - Packet:%p;  Type:%d;  Id:%d", p_packet, p_packet->PacketType, p_packet->PacketId);
	ESP_LOGD(TAG, "PacketDebug - BuffersLen:%d;  Buffer:%p", p_packet->PacketBuffer_length, p_packet->PacketBuffer);
	ESP_LOGD(TAG, "PacketDebug - TopicLen:%d;  Topic:%p", p_packet->PacketTopic_length, p_packet->PacketTopic);
	ESP_LOGD(TAG, "PacketDebug - PayloadLen:%d;  PayloadOffset:%d;  Payload:%p", p_packet->PacketPayload_length, p_packet->PacketPayload_offset, p_packet->PacketPayload);
//	ESP_LOGD(TAG, "PacketDebug - SendingQueue:%p;  RingBuff:%p", p_packet->xSendingQueue, p_packet->send_rb);
	ESP_LOGD(TAG, "Packet: %s ", dump_packet(p_packet->PacketBuffer, p_packet->PacketPayload_length))
	ESP_LOGD(TAG, " ");
}

// ### END DBK
