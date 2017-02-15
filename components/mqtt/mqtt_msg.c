/*
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
 *
 */
#include <stdint.h>
#include <string.h>

#include "esp_log.h"

#include "mqtt_msg.h"
#include "mqtt_config.h"
#define MQTT_MAX_FIXED_HEADER_SIZE 3

static const char *TAG = "MqttMsg";

enum mqtt_connect_flag {
	MQTT_CONNECT_FLAG_USERNAME = 1 << 7,
	MQTT_CONNECT_FLAG_PASSWORD = 1 << 6,
	MQTT_CONNECT_FLAG_WILL_RETAIN = 1 << 5,
	MQTT_CONNECT_FLAG_WILL = 1 << 2,
	MQTT_CONNECT_FLAG_CLEAN_SESSION = 1 << 1
};

struct __attribute((__packed__)) mqtt_connect_variable_header {
	uint8_t lengthMsb;
	uint8_t lengthLsb;
#if defined(CONFIG_MQTT_PROTOCOL_311)
	uint8_t magic[4];
#else
	uint8_t magic[6];
#endif
	uint8_t version;
	uint8_t flags;
	uint8_t keepaliveMsb;
	uint8_t keepaliveLsb;
};

static int append_string(mqtt_connection_t* connection, const char* string, int len) {
	ESP_LOGI(TAG, "Append_String - All");
	if (connection->message.PayloadLength + len + 2 > connection->buffer_length) {
		return -1;
	}
	connection->buffer[connection->message.PayloadLength++] = len >> 8;
	connection->buffer[connection->message.PayloadLength++] = len & 0xff;
	memcpy(connection->buffer + connection->message.PayloadLength, string, len);
	connection->message.PayloadLength += len;
	return len + 2;
}

static uint16_t append_message_id(mqtt_connection_t* connection,
		uint16_t message_id) {
	ESP_LOGI(TAG, "Append_Message_Id - All");
	// If message_id is zero then we should assign one, otherwise we'll use the one supplied by the caller
	while (message_id == 0) {
		message_id = ++connection->message_id;
	}
	if (connection->message.PayloadLength + 2 > connection->buffer_length) {
		return 0;
	}
	connection->buffer[connection->message.PayloadLength++] = message_id >> 8;
	connection->buffer[connection->message.PayloadLength++] = message_id & 0xff;
	return message_id;
}

static int init_message(mqtt_connection_t* connection) {
	ESP_LOGI(TAG, "Init_Message - All");
	connection->message.PayloadLength = MQTT_MAX_FIXED_HEADER_SIZE;
	return MQTT_MAX_FIXED_HEADER_SIZE;
}

static mqtt_message_t* fail_message(mqtt_connection_t* connection) {
	ESP_LOGW(TAG, "Fail_Message - All");
	// Reset the buffer
	connection->message.PayloadData = connection->buffer;
	connection->message.PayloadLength = 0;
	return &connection->message;
}

static mqtt_message_t* fini_message(mqtt_connection_t* connection, int type, int dup, int qos, int retain) {
	ESP_LOGI(TAG, "Fini_Message - All");
	int remaining_length = connection->message.PayloadLength - MQTT_MAX_FIXED_HEADER_SIZE;
	if (remaining_length > 127) {
		connection->buffer[0] = ((type & 0x0f) << 4) | ((dup & 1) << 3) | ((qos & 3) << 1) | (retain & 1);
		connection->buffer[1] = 0x80 | (remaining_length % 128);
		connection->buffer[2] = remaining_length / 128;
		connection->message.PayloadLength = remaining_length + 3;
		connection->message.PayloadData = connection->buffer;
	} else {
		connection->buffer[1] = ((type & 0x0f) << 4) | ((dup & 1) << 3) | ((qos & 3) << 1) | (retain & 1);
		connection->buffer[2] = remaining_length;
		connection->message.PayloadLength = remaining_length + 2;
		connection->message.PayloadData = connection->buffer + 1;
	}
	return &connection->message;
}

/*
 * Setup the connection information (p_connection).
 */
void mqtt_msg_init(mqtt_connection_t* p_connection, uint8_t* p_buffer, uint16_t p_buffer_length) {
	ESP_LOGI(TAG, "Msg_init - Create empty Message");
	memset(p_connection, 0, sizeof(mqtt_connection_t));
	p_connection->buffer = p_buffer;
	p_connection->buffer_length = p_buffer_length;
}

/*
 * This only works for up to 16,383 bytes
 */
int mqtt_get_total_length(uint8_t* buffer, uint16_t length) {
	int i;
	int totlen = 0;
	ESP_LOGI(TAG, "Get_Total_Length - All");
	for (i = 1; i < length; ++i) {
		totlen += (buffer[i] & 0x7f) << (7 * (i - 1));
		if ((buffer[i] & 0x80) == 0) {
			++i;
			break;
		}
	}
	totlen += i;
	return totlen;
}

const char* mqtt_get_publish_topic(uint8_t* buffer, uint16_t* length) {
	int i;
	int totlen = 0;
	int topiclen;
	ESP_LOGI(TAG, "Get_Publish_Topic - All");
	for (i = 1; i < *length; ++i) {
		totlen += (buffer[i] & 0x7f) << (7 * (i - 1));
		if ((buffer[i] & 0x80) == 0) {
			++i;
			break;
		}
	}
	totlen += i;
	if (i + 2 >= *length) {
		return 0;
	}
	topiclen = buffer[i++] << 8;
	topiclen |= buffer[i++];
	if (i + topiclen > *length) {
		return 0;
	}
	*length = topiclen;
	return (const char*) (buffer + i);
}

const char* mqtt_get_publish_data(uint8_t* buffer, uint16_t* length) {
	int i;
	int totlen = 0;
	int topiclen;
	int blength = *length;
	*length = 0;
	ESP_LOGI(TAG, "Get_Publish_Data");
	for (i = 1; i < blength; ++i) {
		totlen += (buffer[i] & 0x7f) << (7 * (i - 1));
		if ((buffer[i] & 0x80) == 0) {
			++i;
			break;
		}
	}
	totlen += i;
	if (i + 2 >= blength) {
		return 0;
	}
	topiclen = buffer[i++] << 8;
	topiclen |= buffer[i++];
	if (i + topiclen >= blength) {
		return 0;
	}
	i += topiclen;
	if (mqtt_get_qos(buffer) > 0) {
		if (i + 2 >= blength) {
			return 0;
		}
		i += 2;
	}
	if (totlen < i) {
		return 0;
	}
	if (totlen <= blength) {
		*length = totlen - i;
	} else {
		*length = blength - i;
	}
	return (const char*) (buffer + i);
}

uint16_t mqtt_get_id(uint8_t* buffer, uint16_t length) {
	ESP_LOGI(TAG, "Get_Id - All");
	if (length < 1) {
		return 0;
	}
	switch (mqtt_get_type(buffer)) {
		case MQTT_MSG_TYPE_PUBLISH: {
			int i;
			int topiclen;
			for (i = 1; i < length; ++i) {
				if ((buffer[i] & 0x80) == 0) {
					++i;
					break;
				}
			}
			if (i + 2 >= length) {
				return 0;
			}
			topiclen = buffer[i++] << 8;
			topiclen |= buffer[i++];
			if (i + topiclen >= length) {
				return 0;
			}
			i += topiclen;
			if (mqtt_get_qos(buffer) > 0) {
				if (i + 2 >= length)
					return 0;
				//i += 2;
			} else {
				return 0;
			}
			return (buffer[i] << 8) | buffer[i + 1];
		}
		case MQTT_MSG_TYPE_PUBACK:
		case MQTT_MSG_TYPE_PUBREC:
		case MQTT_MSG_TYPE_PUBREL:
		case MQTT_MSG_TYPE_PUBCOMP:
		case MQTT_MSG_TYPE_SUBACK:
		case MQTT_MSG_TYPE_UNSUBACK:
		case MQTT_MSG_TYPE_SUBSCRIBE: {
			// This requires the remaining length to be encoded in 1 byte,
			// which it should be.
			if (length >= 4 && (buffer[1] & 0x80) == 0) {
				return (buffer[2] << 8) | buffer[3];
			} else {
				return 0;
			}
		}
		default:
			return 0;
	}
}

/*
 * CONNECT – Client requests a connection to a Server
 * After a Network Connection is established by a Client to a Server, the first Packet sent from the Client to the Server MUST be a CONNECT Packet [MQTT-3.1.0-1].
 * A Client can only send the CONNECT Packet once over a Network Connection.
 * The Server MUST process a second CONNECT Packet sent from a Client as a protocol violation and disconnect the Client [MQTT-3.1.0-2].
 * See section 4.8 for information about handling errors.
 * The payload contains one or more encoded fields.
 * They specify a unique Client identifier for the Client, a Will topic, Will Message, User Name and Password.
 * All but the Client identifier are optional and their presence is determined based on flags in the variable header.
 */
mqtt_message_t* mqtt_msg_connect(mqtt_connection_t* p_connection, mqtt_connect_info_t* p_info) {
	struct mqtt_connect_variable_header* variable_header;
	ESP_LOGI(TAG, "Msg_Connect - Start");
	init_message(p_connection);
	if (p_connection->message.PayloadLength + sizeof(*variable_header) > p_connection->buffer_length) {
		ESP_LOGE(TAG, "Msg_Connect - Failed - Too big for buffer")
		return fail_message(p_connection);
	}
	variable_header = (void*) (p_connection->buffer + p_connection->message.PayloadLength);
	p_connection->message.PayloadLength += sizeof(*variable_header);
	variable_header->lengthMsb = 0;
//#if defined(CONFIG_MQTT_PROTOCOL_311)
	variable_header->lengthLsb = 4;
	memcpy(variable_header->magic, "MQTT", 4);
	variable_header->version = 4;
//#else
//	variable_header->lengthLsb = 6;
//	memcpy(variable_header->magic, "MQIsdp", 6);
//	variable_header->version = 3;
//#endif
	variable_header->flags = 0;
	variable_header->keepaliveMsb = p_info->keepalive >> 8;
	variable_header->keepaliveLsb = p_info->keepalive & 0xff;
	if (p_info->Clean_session) {
		variable_header->flags |= MQTT_CONNECT_FLAG_CLEAN_SESSION;
	}
	if (p_info->client_id != 0 && p_info->client_id[0] != '\0') {
		if (append_string(p_connection, p_info->client_id, strlen(p_info->client_id)) < 0) {
			ESP_LOGE(TAG, "Msg_Connect - Failed - Wrong ID")
			return fail_message(p_connection);
		}
	} else {
		return fail_message(p_connection);
	}
	if (p_info->will_topic != 0 && p_info->will_topic[0] != '\0') {
		if (append_string(p_connection, p_info->will_topic, strlen(p_info->will_topic)) < 0) {
			ESP_LOGE(TAG, "Msg_Connect - Failed - Will 1")
			return fail_message(p_connection);
		}
		if (append_string(p_connection, p_info->will_message, strlen(p_info->will_message)) < 0) {
			ESP_LOGE(TAG, "Msg_Connect - Failed - Will 2")
			return fail_message(p_connection);
		}
		variable_header->flags |= MQTT_CONNECT_FLAG_WILL;
		if (p_info->will_retain) {
			variable_header->flags |= MQTT_CONNECT_FLAG_WILL_RETAIN;
		}
		variable_header->flags |= (p_info->will_qos & 3) << 3;
	}
	if (p_info->username != 0 && p_info->username[0] != '\0') {
		if (append_string(p_connection, p_info->username, strlen(p_info->username)) < 0) {
			ESP_LOGE(TAG, "Msg_Connect - Failed - Username")
			return fail_message(p_connection);
		}
		variable_header->flags |= MQTT_CONNECT_FLAG_USERNAME;
	}
	if (p_info->password != 0 && p_info->password[0] != '\0') {
		if (append_string(p_connection, p_info->password, strlen(p_info->password)) < 0) {
			ESP_LOGE(TAG, "Msg_Connect - Failed - Password")
			return fail_message(p_connection);
		}
		variable_header->flags |= MQTT_CONNECT_FLAG_PASSWORD;
	}
	ESP_LOGI(TAG, "Msg_Connect - Suceeded - Connect Message")
	return fini_message(p_connection, MQTT_MSG_TYPE_CONNECT, 0, 0, 0);
}

/*
 * CONNACK – Acknowledge connection request
 * The CONNACK Packet is the packet sent by the Server in response to a CONNECT Packet received from a Client.
 * The first packet sent from the Server to the Client MUST be a CONNACK Packet [MQTT- 3.2.0-1].
 * If the Client does not receive a CONNACK Packet from the Server within a reasonable amount of time, the Client SHOULD close the Network Connection.
 * A "reasonable" amount of time depends on the type of application and the communications infrastructure.
 */


/*
 * PUBLISH – Publish message
 * A PUBLISH Control Packet is sent from a Client to a Server or from Server to a Client to transport an Application Message.
 */
mqtt_message_t* mqtt_msg_publish(mqtt_connection_t* connection, const char* topic, const char* data, int data_length, int qos, int retain, uint16_t* message_id) {
	init_message(connection);
	ESP_LOGI(TAG, "Msg_Publish - Start");
	if (topic == 0 || topic[0] == '\0') {
		return fail_message(connection);
	}
	if (append_string(connection, topic, strlen(topic)) < 0) {
		return fail_message(connection);
	}
	if (qos > 0) {
		if ((*message_id = append_message_id(connection, 0)) == 0)
			return fail_message(connection);
	} else {
		*message_id = 0;
	}
	if (connection->message.PayloadLength + data_length > connection->buffer_length) {
		return fail_message(connection);
	}
	memcpy(connection->buffer + connection->message.PayloadLength, data, data_length);
	connection->message.PayloadLength += data_length;
	return fini_message(connection, MQTT_MSG_TYPE_PUBLISH, 0, qos, retain);
}

/*
 * PUBACK – Publish acknowledgement
 * A PUBACK Packet is the response to a PUBLISH Packet with QoS level 1.
 */
mqtt_message_t* mqtt_msg_puback(mqtt_connection_t* connection, uint16_t message_id) {
	ESP_LOGI(TAG, "Msg_PubAck");
	init_message(connection);
	if (append_message_id(connection, message_id) == 0) {
		return fail_message(connection);
	}
	return fini_message(connection, MQTT_MSG_TYPE_PUBACK, 0, 0, 0);
}

/*
 * PUBREC – Publish received (QoS 2 publish received, part 1)
 * A PUBREC Packet is the response to a PUBLISH Packet with QoS 2.
 * It is the second packet of the QoS 2 protocol exchange.
 */
mqtt_message_t* mqtt_msg_pubrec(mqtt_connection_t* connection, uint16_t message_id) {
	ESP_LOGI(TAG, "Msg_PubRec");
	init_message(connection);
	if (append_message_id(connection, message_id) == 0) {
		return fail_message(connection);
	}
	return fini_message(connection, MQTT_MSG_TYPE_PUBREC, 0, 0, 0);
}

/*
 * PUBREL – Publish release (QoS 2 publish received, part 2)
 * A PUBREL Packet is the response to a PUBREC Packet.
 * It is the third packet of the QoS 2 protocol exchange.
 */
mqtt_message_t* mqtt_msg_pubrel(mqtt_connection_t* connection, 	uint16_t message_id) {
	ESP_LOGI(TAG, "Msg_PuBRel");
	init_message(connection);
	if (append_message_id(connection, message_id) == 0) {
		return fail_message(connection);
	}
	return fini_message(connection, MQTT_MSG_TYPE_PUBREL, 0, 1, 0);
}

/*
 * PUBCOMP – Publish complete (QoS 2 publish received, part 3)
 * The PUBCOMP Packet is the response to a PUBREL Packet.
 * It is the fourth and final packet of the QoS 2 protocol exchange.
 */
mqtt_message_t* mqtt_msg_pubcomp(mqtt_connection_t* connection,
		uint16_t message_id) {
	init_message(connection);
	if (append_message_id(connection, message_id) == 0) {
		return fail_message(connection);
	}
	return fini_message(connection, MQTT_MSG_TYPE_PUBCOMP, 0, 0, 0);
}

/*
 * SUBSCRIBE - Subscribe to topics
 * The SUBSCRIBE Packet is sent from the Client to the Server to create one or more Subscriptions.
 * Each Subscription registers a Client’s interest in one or more Topics.
 * The Server sends PUBLISH Packets to the Client in order to forward Application Messages that were published to Topics that match these Subscriptions.
 * The SUBSCRIBE Packet also specifies (for each Subscription) the maximum QoS with which the Server can send Application Messages to the Client.
 */
mqtt_message_t* mqtt_msg_subscribe(mqtt_connection_t* connection, const char* topic, int qos, uint16_t* message_id) {
	init_message(connection);
	ESP_LOGI(TAG, "Msg_Subscribe");
	if (topic == 0 || topic[0] == '\0') {
		return fail_message(connection);
	}
	if ((*message_id = append_message_id(connection, 0)) == 0) {
		return fail_message(connection);
	}
	if (append_string(connection, topic, strlen(topic)) < 0) {
		return fail_message(connection);
	}
	if (connection->message.PayloadLength + 1 > connection->buffer_length) {
		return fail_message(connection);
	}
	connection->buffer[connection->message.PayloadLength++] = qos;
	return fini_message(connection, MQTT_MSG_TYPE_SUBSCRIBE, 0, 1, 0);
}

/*
 * SUBACK – Subscribe acknowledgement
 * A SUBACK Packet is sent by the Server to the Client to confirm receipt and processing of a SUBSCRIBE Packet.
 * A SUBACK Packet contains a list of return codes, that specify the maximum QoS level that was granted in each Subscription that was requested by the SUBSCRIBE.
 */

/*
 * UNSUBSCRIBE – Unsubscribe from topics
 * An UNSUBSCRIBE Packet is sent by the Client to the Server, to unsubscribe from topics.
 */
mqtt_message_t* mqtt_msg_unsubscribe(mqtt_connection_t* connection, const char* topic, uint16_t* message_id) {
	init_message(connection);
	ESP_LOGI(TAG, "Msg_UnSubscribe");
	if (topic == 0 || topic[0] == '\0')
		return fail_message(connection);
	if ((*message_id = append_message_id(connection, 0)) == 0)
		return fail_message(connection);
	if (append_string(connection, topic, strlen(topic)) < 0)
		return fail_message(connection);
	return fini_message(connection, MQTT_MSG_TYPE_UNSUBSCRIBE, 0, 1, 0);
}

/*
 * UNSUBACK – Unsubscribe acknowledgement
 * The UNSUBACK Packet is sent by the Server to the Client to confirm receipt of an UNSUBSCRIBE Packet.
 */

/*
 * PINGREQ – PING request
 * The PINGREQ Packet is sent from a Client to the Server.
 *  It can be used to:
 *   1. Indicate to the Server that the Client is alive in the absence of any other Control Packets being sent from the Client to the Server.
 *   2. Request that the Server responds to confirm that it is alive.
 *   3. Exercise the network to indicate that the Network Connection is active.
 * This Packet is used in Keep Alive processing.
 */
mqtt_message_t* mqtt_msg_pingreq(mqtt_connection_t* connection) {
	ESP_LOGI(TAG, "Msg_PingReq");
	init_message(connection);
	return fini_message(connection, MQTT_MSG_TYPE_PINGREQ, 0, 0, 0);
}

/*
 * PINGRESP – PING response
 * A PINGRESP Packet is sent by the Server to the Client in response to a PINGREQ Packet.
 *  It indicates that the Server is alive.
 *  This Packet is used in Keep Alive processing.
 */
mqtt_message_t* mqtt_msg_pingresp(mqtt_connection_t* connection) {
	ESP_LOGI(TAG, "Msg_PingResp");
	init_message(connection);
	return fini_message(connection, MQTT_MSG_TYPE_PINGRESP, 0, 0, 0);
}

/*
 * DISCONNECT – Disconnect notification
 * The DISCONNECT Packet is the final Control Packet sent from the Client to the Server.
 * It indicates that the Client is disconnecting cleanly.
 */
mqtt_message_t* mqtt_msg_disconnect(mqtt_connection_t* connection) {
	ESP_LOGI(TAG, "Msg_Disconnect");
	init_message(connection);
	return fini_message(connection, MQTT_MSG_TYPE_DISCONNECT, 0, 0, 0);
}

// ### END DBK