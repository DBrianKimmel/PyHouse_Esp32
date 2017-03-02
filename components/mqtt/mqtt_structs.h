#ifndef COMPONENTS_MQTT_MQTT_STRUCTS_H_
#define COMPONENTS_MQTT_MQTT_STRUCTS_H_

/*
 * mqtt_structs.h
 *
 *  Created on: Feb 25, 2017
 *      Author: briank
 */


#include <stdint.h>

#include "ringbuf.h"

/*
 *
 */
typedef void (*mqtt_callback)(void *, void *);

/**
 * All Callbacks for the client
 */
typedef struct Callback {
	mqtt_callback   	connected_cb;
	mqtt_callback   	disconnected_cb;
	mqtt_callback   	reconnect_cb;
	mqtt_callback   	subscribe_cb;
	mqtt_callback   	publish_cb;
	mqtt_callback   	data_cb;
} Callback_t;

/**
 *
 */
typedef struct mqtt_message {
	uint8_t 			*PayloadData;
	uint16_t 			PayloadLength;
} mqtt_message_t;

/*
 * 10 bytes
 */
struct __attribute((__packed__)) mqtt_connect_variable_header {
	uint8_t				lengthMsb;
	uint8_t				lengthLsb;
//#if defined(CONFIG_MQTT_PROTOCOL_311)
	uint8_t 			magic[4];
//#else
//	uint8_t magic[6];
//#endif
	uint8_t 			version;
	uint8_t 			flags;
	uint8_t 			keepaliveMsb;
	uint8_t 			keepaliveLsb;
};

/**
 * MQTT Packets.
 * See:  http://docs.oasis-open.org/mqtt/mqtt/v3.1.1/os/mqtt-v3.1.1-os.pdf
 */
typedef struct PacketInfo {
	uint8_t				PacketType;
	uint16_t			PacketId;
	uint16_t			PacketStart;
	const uint8_t		*PacketTopic;
	uint16_t			PacketTopic_length;
	const uint8_t		*PacketPayload;
	uint16_t			PacketPayload_offset;
	uint16_t			PacketPayload_length;
	uint16_t			Packet_length;
	uint8_t				*PacketBuffer;
	uint16_t			PacketBuffer_length;
} PacketInfo_t;

/**
 * LastWillAndTestament for the client
 * If we miss a timeout from the broker, this is what the broker will act on.
 * The broker will send out this Message telling other clients it has lost contact with us.
 */
typedef struct LastWill {
	char				WillTopic[32];
	char				WillMessage[32];
	uint32_t			WillQos;
	uint32_t			WillRetain;
	uint32_t			CleanSession;
	uint32_t			Keepalive;
} LastWill_t;

/**
 * Buffer runtime stuff
 */
typedef struct Buffers {
	uint8_t				*in_buffer;
	uint8_t				*out_buffer;
	int					in_buffer_length;
	int					out_buffer_length;
} Buffers_t;

/*
 *
 */
typedef struct mqtt_connection {
	mqtt_message_t 		*message;
	uint16_t 			message_id;
	uint8_t				*buffer;
	uint16_t 			buffer_length;
} mqtt_connection_t;

/**
 * Broker Information
 * This is the broker (only one) that this client will use.
 */
typedef struct BrokerConfig {
	char				Host[64];
	uint32_t			Port;
	char				Client_id[64];
	char				Username[32];
	char				Password[32];
	uint32_t			Socket;
	char* 				ClientId;
} BrokerConfig_t;

/*
 *
 */
typedef struct State {
	uint16_t			message_length;
	uint16_t			message_length_read;
	mqtt_message_t		*outbound_message;
	mqtt_connection_t	*Connection;
	uint16_t			pending_msg_id;
	int					pending_msg_type;
	int					pending_publish_qos;
} State_t;

/*
 * Client = Top level MQTT information.
 */
typedef struct Client {
	State_t				*State;
	Buffers_t			*Buffers;
	BrokerConfig_t		*Broker;
	LastWill_t			*Will;
	Callback_t			*Cb;
	PacketInfo_t		*Packet;  // mqtt_msg.h
	QueueHandle_t		*xSendingQueue;
	RINGBUF				*send_rb;
} Client_t;

#endif /* COMPONENTS_MQTT_MQTT_STRUCTS_H_ */
// ### END DBK
