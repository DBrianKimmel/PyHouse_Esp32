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

static const char *TAG = "MqttDebug     ";


/**
 * Print out a string
 */
void print_string(char *p_string) {
	ESP_LOGD(TAG, "String:%s", p_string);
}

char *dump_hex(uint8_t *p_array, int p_len) {
	int l_ix;
	int l_outix = 0;
	char *l_str = malloc(512);
	char *l_chr = malloc(6);
	char l_byte;
	if (p_len > 0) {
		for (l_ix = 0; l_ix <= p_len; l_ix++) {
			l_byte = p_array[l_ix];
			if (l_byte < 32 || l_byte > 127) {
				sprintf(l_chr, "\\%02X ", l_byte);
				memcpy(&l_str[l_outix], l_chr, 3);
				l_outix += 3;
			} else {
				l_str[l_outix++] = l_byte;
			}
		}
	}
	l_str[l_outix++] = '<';
	l_str[l_outix++] = '<';
	l_str[l_outix++] = 0;
	return l_str;
}

/**
 * Print put the current heap sizes
 */
void print_heap(void) {
	int l_heap = esp_get_free_heap_size();
	ESP_LOGI(TAG, " 42 Start - Heap: (0x%X) %d", l_heap, l_heap);
}

void print_will(Client_t *p_client) {
	ESP_LOGD(TAG, "WillDebug - ClientPtr:%p;  WillPtr:%p", p_client, p_client->Will);
	ESP_LOGD(TAG, "WillDebug - Topic:%s", dump_hex((uint8_t*)p_client->Will->WillTopic, strlen(p_client->Will->WillTopic)));
	ESP_LOGD(TAG, "WillDebug - Message:%s", dump_hex((uint8_t*)p_client->Will->WillMessage, strlen(p_client->Will->WillMessage)));
}

void print_client(Client_t *p_client) {
	ESP_LOGD(TAG, "ClientDebug - ClientPtr:%p", p_client);
	ESP_LOGD(TAG, "ClientDebug - StatePtr:%p; BuffersPtr:%p", p_client->State, p_client->Buffers);
	ESP_LOGD(TAG, "ClientDebug - BrokerPtr:%p; WillPtr:%p", p_client->Broker, p_client->Will);
	ESP_LOGD(TAG, "ClientDebug - CbPtr:%p; PacketPtr:%p", p_client->Cb, p_client->Packet);
	ESP_LOGD(TAG, "ClientDebug - SendingQueuePtr:%p; RingBuffPtr:%p", p_client->SendingQueue, p_client->Send_rb);
	ESP_LOGD(TAG, " ");
}

void print_packet(PacketInfo_t *p_packet) {
	ESP_LOGD(TAG, "PacketDebug - PacketPtr:%p;  Type:%d;  Id:%d", p_packet, p_packet->PacketType, p_packet->PacketId);
	ESP_LOGD(TAG, "PacketDebug -    Fixed:%s", dump_hex(p_packet->PacketFixedHeader, p_packet->PacketFixedHeader_length));
	ESP_LOGD(TAG, "PacketDebug - Variable:%s", dump_hex(p_packet->PacketVariableHeader, p_packet->PacketVariableHeader_length));
	ESP_LOGD(TAG, "PacketDebug -  Payload:%s", dump_hex(p_packet->PacketPayload, p_packet->PacketPayload_length));
	ESP_LOGD(TAG, " ");
}

void print_buffer(void *p_buffer, uint16_t p_len) {
	ESP_LOGD(TAG, "Buffer: >>%s", dump_hex(p_buffer, p_len));
	ESP_LOGD(TAG, " ");
}

// ### END DBK
