#ifndef COMPONENTS_MQTT_MQTT_STRUCTS_H_
#define COMPONENTS_MQTT_MQTT_STRUCTS_H_

/*
 * mqtt_structs.h
 *
 *  Created on: Feb 25, 2017
 *      Author: briank
 */


#include <stdint.h>
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
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
 * Fixed header
 * We only have enough memory to support 1 or 2 bytes of remaining length.
 * 1 byte  =   0 - 127
 * 2 bytes = 128 - 16383
 *
 * |===============================================================|
 * | Bit         |    7     6     5     4     3     2     1     0  |
 * |-------------|-------------------------------------------------|
 * | byte 1      | Packet type             |  F  |  L  |  A  |  G  |
 * | byte 2...   | Remaining Length...(1 to 4 Bytes)               |
 * |===============================================================|
 *
 */
struct mqtt_fixed_header {
	uint8_t				packet_type_and_flags;
	uint8_t				remaining_length[4];
};
/*
 * 10 bytes
 */
struct __attribute((__packed__)) mqtt_connect_variable_header {
	uint8_t				lengthMsb;
	uint8_t				lengthLsb;
	uint8_t 			magic[4];
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
//	uint16_t			PacketStart;
	const uint8_t		*PacketTopic;
	uint16_t			PacketTopic_length;
	uint16_t			Packet_length;
	uint8_t				*PacketBuffer;
	uint16_t			PacketBuffer_length;
//	uint16_t			PacketPayload_offset;

	uint8_t				*PacketFixedHeader;
	uint8_t				PacketFixedHeader_length;
	uint8_t				*PacketVariableHeader;
	uint8_t				PacketVariableHeader_length;
	uint8_t				*PacketPayload;
	uint16_t			PacketPayload_length;

} PacketInfo_t;

/**
 * Last Will And Testament for the client
 * If we miss a timeout from the broker, this is what the broker will act on.
 * The broker will send out this Message telling other clients it has lost contact with us.
 */
typedef struct Will {
	char				*WillTopic;
	char				*WillMessage;
	uint32_t			WillQos;
	uint32_t			WillRetain;
	uint32_t			CleanSession;
	uint32_t			Keepalive;
	uint32_t			Keepalive_tick;
} Will_t;

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
	char				Username[32];
	char				Password[32];
	uint32_t			Socket;
	char 				ClientId[64];
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
	BrokerConfig_t		*Broker;
	Buffers_t			*Buffers;
	Callback_t			*Cb;
	PacketInfo_t		*Packet;  // mqtt_msg.h
	QueueHandle_t		*SendingQueue;
	Ringbuff_t			*Send_rb;
	State_t				*State;
	Will_t				*Will;
} Client_t;

#endif /* COMPONENTS_MQTT_MQTT_STRUCTS_H_ */
// ### END DBK
