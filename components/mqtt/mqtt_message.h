#ifndef MQTT_MESSAGE_H
#define MQTT_MESSAGE_H

/**
 * @file mqtt_message.h
 * @brief Handle the message internal composition and decomposition.
 *
 */

#include <stdint.h>

#include "mqtt_config.h"

#ifdef  __cplusplus
extern "C" {
#endif

/**
 * Copyright (c) 2014, Stephen Robinson
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 * notice, this list of conditions and the following disclaimer in the
 * documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the copyright holder nor the names of its
 * contributors may be used to endorse or promote products derived
 * from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

/*    7      6        5        4        3         2         1         0     */
/*|      --- Message Type----     |  DUP Flag |    QoS Level    | Retain  | */
/*                    Remaining Length                 */

enum mqtt_message_type {
	MQTT_MSG_TYPE_CONNECT = 1,
	MQTT_MSG_TYPE_CONNACK = 2,
	MQTT_MSG_TYPE_PUBLISH = 3,
	MQTT_MSG_TYPE_PUBACK = 4,
	MQTT_MSG_TYPE_PUBREC = 5,
	MQTT_MSG_TYPE_PUBREL = 6,
	MQTT_MSG_TYPE_PUBCOMP = 7,
	MQTT_MSG_TYPE_SUBSCRIBE = 8,
	MQTT_MSG_TYPE_SUBACK = 9,
	MQTT_MSG_TYPE_UNSUBSCRIBE = 10,
	MQTT_MSG_TYPE_UNSUBACK = 11,
	MQTT_MSG_TYPE_PINGREQ = 12,
	MQTT_MSG_TYPE_PINGRESP = 13,
	MQTT_MSG_TYPE_DISCONNECT = 14
};

enum mqtt_connect_return_code {
	CONNECTION_ACCEPTED = 0,
	CONNECTION_REFUSE_PROTOCOL,
	CONNECTION_REFUSE_ID_REJECTED,
	CONNECTION_REFUSE_SERVER_UNAVAILABLE,
	CONNECTION_REFUSE_BAD_USERNAME,
	CONNECTION_REFUSE_NOT_AUTHORIZED
};

typedef struct mqtt_message {
	uint8_t 		*PayloadData;
	uint16_t 		PayloadLength;
} mqtt_message_t;

typedef struct mqtt_connection {
	mqtt_message_t 	*message;
	uint16_t 		message_id;
	uint8_t			*buffer;
	uint16_t 		buffer_length;
} mqtt_connection_t;

/**
 * MQTT Packets.
 * See:  http://docs.oasis-open.org/mqtt/mqtt/v3.1.1/os/mqtt-v3.1.1-os.pdf
 */
typedef struct PacketInfo {
	uint8_t				PacketType;
	uint16_t			PacketId;
	const uint8_t		*PacketTopic;
	uint16_t			PacketTopic_length;
	const uint8_t		*PacketPayload;
	uint16_t			PacketPayload_offset;
	uint16_t			PacketPayload_length;
	uint16_t			Packet_length;
	uint8_t				*PacketBuffer;
	uint16_t			PacketBuffer_length;
} PacketInfo_t;

/*
 * This details the connection information to a given broker
 */
typedef struct ConnectInfo {
	char* 			ClientId;
	char* 			WillTopic;
	char* 			WillMessage;
	int 			Keepalive;
	int 			WillQos;
	int 			WillRetain;
	int 			CleanSession;
} ConnectInfo_t;


static inline int mqtt_get_type(uint8_t* p_buffer) {
	return (p_buffer[0] & 0xf0) >> 4;
}
static inline int mqtt_get_dup(uint8_t* p_buffer) {
	return (p_buffer[0] & 0x08) >> 3;
}
static inline int mqtt_get_qos(uint8_t* p_buffer) {
	return (p_buffer[0] & 0x06) >> 1;
}
static inline int mqtt_get_retain(uint8_t* p_buffer) {
	return (p_buffer[0] & 0x01);
}
static inline int mqtt_get_connect_return_code(uint8_t* p_buffer) {
	return p_buffer[3];
}

void mqtt_msg_init(PacketInfo_t* connection, uint8_t* buffer, uint16_t buffer_length);

int mqtt_get_total_length(uint8_t* buffer, uint16_t length);
const uint8_t* mqtt_get_publish_topic(uint8_t* buffer, uint16_t* length);
const uint8_t* mqtt_get_publish_data(uint8_t* buffer, uint16_t* length);
uint16_t mqtt_get_id(uint8_t* buffer, uint16_t length);

mqtt_message_t* mqtt_msg_connect(PacketInfo_t* connection, ConnectInfo_t* info);
mqtt_message_t* mqtt_msg_publish(PacketInfo_t* connection, const char* topic, const char* data, int data_length, int qos, int retain, uint16_t* message_id);
mqtt_message_t* mqtt_msg_puback(PacketInfo_t* connection, uint16_t message_id);
mqtt_message_t* mqtt_msg_pubrec(PacketInfo_t* connection, uint16_t message_id);
mqtt_message_t* mqtt_msg_pubrel(PacketInfo_t* connection, uint16_t message_id);
mqtt_message_t* mqtt_msg_pubcomp(PacketInfo_t* connection, uint16_t message_id);
mqtt_message_t* mqtt_msg_subscribe(PacketInfo_t* connection, const char* topic, int qos, uint16_t* message_id);
mqtt_message_t* mqtt_msg_unsubscribe(PacketInfo_t* connection, const char* topic, uint16_t* message_id);
mqtt_message_t* mqtt_msg_pingreq(PacketInfo_t* connection);
mqtt_message_t* mqtt_msg_pingresp(PacketInfo_t* connection);
mqtt_message_t* mqtt_msg_disconnect(PacketInfo_t* connection);

#ifdef  __cplusplus
}
#endif

#endif  /* MQTT_MESSAGE_H */
