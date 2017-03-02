/*
 * @file mqtt_msg.c
 * @brief
 */

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
#include <stdint.h>
#include <string.h>

#include "esp_log.h"

#include "mqtt.h"
#include "mqtt_message.h"
#include "mqtt_debug.h"

#define MQTT_MAX_FIXED_HEADER_SIZE 3

static const char *TAG = "MqttMsg       ";

enum mqtt_connect_flag {
	MQTT_CONNECT_FLAG_USERNAME = 1 << 7,
	MQTT_CONNECT_FLAG_PASSWORD = 1 << 6,
	MQTT_CONNECT_FLAG_WILL_RETAIN = 1 << 5,
	MQTT_CONNECT_FLAG_WILL = 1 << 2,
	MQTT_CONNECT_FLAG_CLEAN_SESSION = 1 << 1
};


/**
 * Encode a utf-8 string and append it to the Payload.
 *
 * Updates PacketBuffer and PacketPayload_length
 *
 * |===============================================================|
 * | Bit         |    7     6     5     4     3     2     1     0  |
 * |-------------|-------------------------------------------------|
 * | byte 1      | String length MSB                               |
 * | byte 2      | String length LSB                               |
 * | byte 3 ...  | UTF-8 Encoded Character Data, if length > 0.    |
 * |===============================================================|
 *
 * @param p_packet is the structure containing the various
 * @param p_string is the Utf-8 encoded string to be appended.
 * @param p_len is the length of the encoded string.
 * @returns
 */
static esp_err_t append_string(PacketInfo_t* p_packet, const char* p_string, int p_len) {
	ESP_LOGI(TAG, " 75 Append_String - Buffer:%p;  String:%s", p_packet->PacketBuffer, p_string);
	if (p_packet->PacketPayload_length + p_len + 2 > p_packet->PacketBuffer_length) {
		ESP_LOGE(TAG, " 82 Append_String - Not enough room.")
		return ESP_ERR_NO_MEM;
	}
	p_packet->PacketBuffer[p_packet->PacketPayload_length++] = p_len >> 8;
	p_packet->PacketBuffer[p_packet->PacketPayload_length++] = p_len & 0xff;
	memcpy(p_packet->PacketBuffer + p_packet->PacketPayload_length, p_string, p_len);
	p_packet->PacketPayload_length += p_len;
	return ESP_OK;
}




/*
 *
 */
static uint16_t append_message_id(PacketInfo_t* p_packet, uint16_t message_id) {
	ESP_LOGI(TAG, " 95 Append_Message_Id - All");
	// If message_id is zero then we should assign one, otherwise we'll use the one supplied by the caller
	while (message_id == 0) {
		message_id = ++p_packet->PacketId;
	}
	if (p_packet->PacketPayload_length + 2 > p_packet->PacketBuffer_length) {
		return 0;
	}
	p_packet->PacketBuffer[p_packet->PacketPayload_length++] = message_id >> 8;
	p_packet->PacketBuffer[p_packet->PacketPayload_length++] = message_id & 0xff;
	return message_id;
}




/*
 *
 */
static int init_message(PacketInfo_t* p_packet) {
	ESP_LOGI(TAG, "115 Init_Message - All");
	p_packet->PacketPayload = p_packet->PacketBuffer;
	p_packet->PacketPayload_length = MQTT_MAX_FIXED_HEADER_SIZE;
	return MQTT_MAX_FIXED_HEADER_SIZE;
}






/*
 *
 */
esp_err_t fail_message(PacketInfo_t *p_packet) {
	ESP_LOGW(TAG, "130 Fail_Message - All");
	// Reset the buffer
	p_packet->PacketPayload = p_packet->PacketBuffer;
	p_packet->PacketPayload_length = 0;
	return ESP_FAIL;
}






/*
 * Fill n the fixed header portion of the packet
 *
 * We only have enough memory to support 1 or 2 bytes of remaining length.
 *
 * |===============================================================|
 * | Bit         |    7     6     5     4     3     2     1     0  |
 * |-------------|-------------------------------------------------|
 * | byte 1      | Packet type             |  A  |  B  |  C  |  D  |
 * | byte 2      | Remaining Length...(1 to 4 Bytes)               |
 * |===============================================================|
 *
 */
esp_err_t fini_message(PacketInfo_t* p_packet, int p_type, int p_dup, int p_qos, int p_retain) {
	int l_remaining_length = p_packet->PacketPayload_length - MQTT_MAX_FIXED_HEADER_SIZE;
	ESP_LOGI(TAG, "155 Fini_Message - Remain:%d", l_remaining_length);
	if (l_remaining_length > 127) {
		p_packet->PacketStart = 0;
		p_packet->PacketBuffer[0] = ((p_type & 0x0f) << 4) | ((p_dup & 1) << 3) | ((p_qos & 3) << 1) | (p_retain & 1);
		p_packet->PacketBuffer[1] = 0x80 | (l_remaining_length % 128);
		p_packet->PacketBuffer[2] = l_remaining_length / 128;
		p_packet->PacketPayload_length = l_remaining_length + 3;
		p_packet->PacketPayload = p_packet->PacketBuffer;
	} else {
		p_packet->PacketStart = 1;
		p_packet->PacketBuffer[1] = ((p_type & 0x0f) << 4) | ((p_dup & 1) << 3) | ((p_qos & 3) << 1) | (p_retain & 1);
		p_packet->PacketBuffer[2] = l_remaining_length;
		p_packet->PacketPayload_length = l_remaining_length + 2;
		p_packet->PacketPayload = p_packet->PacketBuffer + 1;
	}
	return ESP_OK;
}




/*
 * This only works for up to 16,383 bytes
 */
int mqtt_get_total_length(uint8_t* buffer, uint16_t length) {
	int i;
	int totlen = 0;
	ESP_LOGI(TAG, "175 Get_Total_Length - All");
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







/*
 * Extract the topic portion from the MQTT Packet
 */
const uint8_t* mqtt_get_publish_topic(uint8_t* p_packet, uint16_t* p_length) {
	int i;
	int totlen = 0;
	int topiclen;
	ESP_LOGI(TAG, "200 Get_Publish_Topic - All");
	for (i = 1; i < *p_length; ++i) {
		totlen += (p_packet[i] & 0x7f) << (7 * (i - 1));
		if ((p_packet[i] & 0x80) == 0) {
			++i;
			break;
		}
	}
	totlen += i;
	if (i + 2 >= *p_length) {
		return 0;
	}
	topiclen = p_packet[i++] << 8;
	topiclen |= p_packet[i++];
	if (i + topiclen > *p_length) {
		return 0;
	}
	*p_length = topiclen;
	return (const uint8_t*) (p_packet + i);
}






/*
 *
 */
const uint8_t* mqtt_get_publish_data(uint8_t* buffer, uint16_t* length) {
	int i;
	int totlen = 0;
	int topiclen;
	int blength = *length;
	*length = 0;
	ESP_LOGI(TAG, "235 Get_Publish_Data");
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
	return (const uint8_t*) (buffer + i);
}





uint16_t mqtt_get_id(uint8_t* buffer, uint16_t length) {
	ESP_LOGI(TAG, "275 Get_Id - All");
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
 *
 * After a Network Connection is established by a Client to a Server, the first Packet sent from the Client to the Server MUST be a CONNECT Packet [MQTT-3.1.0-1].
 * A Client can only send the CONNECT Packet once over a Network Connection.
 * The Server MUST process a second CONNECT Packet sent from a Client as a protocol violation and disconnect the Client [MQTT-3.1.0-2].
 * See section 4.8 for information about handling errors.
 * The payload contains one or more encoded fields.
 * They specify a unique Client identifier for the Client, a Will topic, Will Message, User Name and Password.
 * All but the Client identifier are optional and their presence is determined based on flags in the variable header.
 *
 * @param p_client is a pointer to the main, initialized data structure.
 * @param p_info is
 * @returns mqtt_message_t
 */
esp_err_t mqtt_msg_connect(Client_t* p_client) {
	struct mqtt_connect_variable_header* l_variable_header;

	ESP_LOGI(TAG, "350 Msg_Connect - Begin.");
	p_client->Packet->PacketTopic = (uint8_t*)p_client->Will->WillTopic;
	p_client->Packet->PacketTopic_length = strlen(p_client->Will->WillTopic);
	p_client->Packet->PacketPayload = (uint8_t*)p_client->Will->WillMessage;
	p_client->Packet->PacketPayload_length = strlen(p_client->Will->WillMessage);
	init_message(p_client->Packet);
	if (p_client->Packet->PacketPayload_length + sizeof(*l_variable_header) > p_client->Packet->PacketBuffer_length) {
		ESP_LOGE(TAG, "357 Msg_Connect - Failed - Too big for buffer")
		return fail_message(p_client->Packet);
	}
// Build the variable header (10 bytes)
	l_variable_header = (void*) (p_client->Packet->PacketBuffer + p_client->Packet->PacketPayload_length);
	p_client->Packet->PacketPayload_length += sizeof(*l_variable_header);
	l_variable_header->lengthMsb = 0;
	l_variable_header->lengthLsb = 4;
	memcpy(l_variable_header->magic, "MQTT", 4);
	l_variable_header->version = 4;
	l_variable_header->flags = 0;
	l_variable_header->keepaliveMsb = p_client->Will->Keepalive >> 8;
	l_variable_header->keepaliveLsb = p_client->Will->Keepalive & 0xff;
	ESP_LOGI(TAG, "370 Msg_Connect - Keepalive: %d", p_client->Will->Keepalive);

	if (p_client->Will->CleanSession) {
		l_variable_header->flags |= MQTT_CONNECT_FLAG_CLEAN_SESSION;
	}
	if (p_client->Broker->ClientId != 0 && p_client->Broker->ClientId[0] != '\0') {
		if (append_string(p_client->Packet, p_client->Broker->ClientId, strlen(p_client->Broker->ClientId)) != ESP_OK) {
			ESP_LOGE(TAG, "3577 Msg_Connect - Failed - Wrong ID")
			return fail_message(p_client->Packet);
		}
	} else {
		ESP_LOGE(TAG, "381 Msg_Connect - Bad ID");
		print_packet(p_client->Packet);
		return fail_message(p_client->Packet);
	}

	if (p_client->Will->WillTopic != 0 && p_client->Will->WillTopic[0] != '\0') {
		if (append_string(p_client->Packet, p_client->Will->WillTopic, strlen(p_client->Will->WillTopic)) != ESP_OK) {
			ESP_LOGE(TAG, "388 Msg_Connect - Failed - Will topic bad")
			return fail_message(p_client->Packet);
		}
		if (append_string(p_client->Packet, p_client->Will->WillMessage, strlen(p_client->Will->WillMessage)) != ESP_OK) {
			ESP_LOGE(TAG, "392 Msg_Connect - Failed - Will message bad")
			return fail_message(p_client->Packet);
		}
		l_variable_header->flags |= MQTT_CONNECT_FLAG_WILL;
		if (p_client->Will->WillRetain) {
			l_variable_header->flags |= MQTT_CONNECT_FLAG_WILL_RETAIN;
		}
		l_variable_header->flags |= (p_client->Will->WillQos & 3) << 3;
	}

//	if (p_info-> != 0 && p_info->username[0] != '\0') {
//		if (append_string(p_client->Packet, p_info->username, strlen(p_info->username)) < 0) {
//			ESP_LOGE(TAG, "379 Msg_Connect - Failed - Username")
//			return fail_message(p_client->Packet);
//		}
//		l_variable_header->flags |= MQTT_CONNECT_FLAG_USERNAME;
//	}

//	if (p_info->password != 0 && p_info->password[0] != '\0') {
//		if (append_string(p_client->Packet, p_info->password, strlen(p_info->password)) < 0) {
//			ESP_LOGE(TAG, "387 Msg_Connect - Failed - Password")
//			return fail_message(p_client->Packet);
//		}
//		l_variable_header->flags |= MQTT_CONNECT_FLAG_PASSWORD;
//	}

	fini_message(p_client->Packet, MQTT_MSG_TYPE_CONNECT, 0, 0, 0);
	print_packet(p_client->Packet);
	ESP_LOGI(TAG, "420 Msg_Connect - Suceeded - Connect Message")
	return ESP_OK;
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
esp_err_t mqtt_msg_publish(PacketInfo_t* p_packet, const char* topic, const char* data, int data_length, int qos, int retain, uint16_t* message_id) {
	init_message(p_packet);
	ESP_LOGI(TAG, "450 Msg_Publish - Start");
	if (topic == 0 || topic[0] == '\0') {
		return fail_message(p_packet);
	}
	if (append_string(p_packet, topic, strlen(topic)) != ESP_OK) {
		return fail_message(p_packet);
	}
	if (qos > 0) {
		if ((*message_id = append_message_id(p_packet, 0)) == 0)
			return fail_message(p_packet);
	} else {
		*message_id = 0;
	}
	if (p_packet->PacketPayload_length + data_length > p_packet->PacketBuffer_length) {
		return fail_message(p_packet);
	}
	memcpy(p_packet->PacketBuffer + p_packet->PacketPayload_length, data, data_length);
	p_packet->PacketPayload_length += data_length;
	fini_message(p_packet, MQTT_MSG_TYPE_PUBLISH, 0, qos, retain);
	print_packet(p_packet);
	return ESP_OK;
}








/*
 * PUBACK – Publish acknowledgement
 * A PUBACK Packet is the response to a PUBLISH Packet with QoS level 1.
 */
esp_err_t mqtt_msg_puback(PacketInfo_t* p_packet, uint16_t message_id) {
	ESP_LOGI(TAG, "485 Msg_PubAck");
	init_message(p_packet);
	if (append_message_id(p_packet, message_id) == 0) {
		return fail_message(p_packet);
	}
	fini_message(p_packet, MQTT_MSG_TYPE_PUBACK, 0, 0, 0);
	print_packet(p_packet);
	return ESP_OK;
}





/*
 * PUBREC – Publish received (QoS 2 publish received, part 1)
 * A PUBREC Packet is the response to a PUBLISH Packet with QoS 2.
 * It is the second packet of the QoS 2 protocol exchange.
 */
esp_err_t mqtt_msg_pubrec(PacketInfo_t* p_packet, uint16_t message_id) {
	ESP_LOGI(TAG, "505 Msg_PubRec");
	init_message(p_packet);
	if (append_message_id(p_packet, message_id) == 0) {
		return fail_message(p_packet);
	}
	fini_message(p_packet, MQTT_MSG_TYPE_PUBREC, 0, 0, 0);
	print_packet(p_packet);
	return ESP_OK;
}





/*
 * PUBREL – Publish release (QoS 2 publish received, part 2)
 * A PUBREL Packet is the response to a PUBREC Packet.
 * It is the third packet of the QoS 2 protocol exchange.
 */
esp_err_t mqtt_msg_pubrel(PacketInfo_t* p_packet, 	uint16_t message_id) {
	ESP_LOGI(TAG, "525 Msg_PuBRel");
	init_message(p_packet);
	if (append_message_id(p_packet, message_id) == 0) {
		return fail_message(p_packet);
	}
	fini_message(p_packet, MQTT_MSG_TYPE_PUBREL, 0, 1, 0);
	return ESP_OK;
}






/*
 * PUBCOMP – Publish complete (QoS 2 publish received, part 3)
 * The PUBCOMP Packet is the response to a PUBREL Packet.
 * It is the fourth and final packet of the QoS 2 protocol exchange.
 */
esp_err_t mqtt_msg_pubcomp(PacketInfo_t* p_packet, uint16_t message_id) {
	ESP_LOGI(TAG, "5245 Msg_PuBcOMP");
	init_message(p_packet);
	if (append_message_id(p_packet, message_id) == 0) {
		return fail_message(p_packet);
	}
	fini_message(p_packet, MQTT_MSG_TYPE_PUBCOMP, 0, 0, 0);
	print_packet(p_packet);
	return ESP_OK;
}






/*
 * SUBSCRIBE - Subscribe to topics
 * The SUBSCRIBE Packet is sent from the Client to the Server to create one or more Subscriptions.
 * Each Subscription registers a Client’s interest in one or more Topics.
 * The Server sends PUBLISH Packets to the Client in order to forward Application Messages that were published to Topics that match these Subscriptions.
 * The SUBSCRIBE Packet also specifies (for each Subscription) the maximum QoS with which the Server can send Application Messages to the Client.
 *
 * @param p_client
 */
esp_err_t mqtt_msg_subscribe(Client_t* p_client, const char* p_topic, int p_qos, uint16_t *p_message_id) {
	ESP_LOGI(TAG, "570 Msg_Subscribe");
	init_message(p_client->Packet);
	if (p_topic == 0 || p_topic[0] == '\0') {
		ESP_LOGE(TAG,"573 Subscribe - Topic Missing.");
		return fail_message(p_client->Packet);
	}
	if ((*p_message_id = append_message_id(p_client->Packet, 0)) == 0) {
		ESP_LOGE(TAG,"577 Subscribe - Message Missing");
		return fail_message(p_client->Packet);
	}
	if (append_string(p_client->Packet, p_topic, strlen(p_topic)) != ESP_OK) {
		ESP_LOGE(TAG,"581 Subscribe - ");
		return fail_message(p_client->Packet);
	}
	if (p_client->Packet->PacketPayload_length + 1 > p_client->Packet->PacketBuffer_length) {
		ESP_LOGE(TAG,"585 Subscribe - ");
		return fail_message(p_client->Packet);
	}
	p_client->Packet->PacketBuffer[p_client->Packet->PacketPayload_length++] = p_qos;
	fini_message(p_client->Packet, MQTT_MSG_TYPE_SUBSCRIBE, 0, 1, 0);
	print_packet(p_client->Packet);
	ESP_LOGI(TAG,"591 Subscribe - Message ok");
	return ESP_OK;
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
esp_err_t mqtt_msg_unsubscribe(PacketInfo_t* p_packet, const char* topic, uint16_t* message_id) {
	init_message(p_packet);
	ESP_LOGI(TAG, "620 Msg_UnSubscribe");
	if (topic == 0 || topic[0] == '\0')
		return fail_message(p_packet);
	if ((*message_id = append_message_id(p_packet, 0)) == 0)
		return fail_message(p_packet);
	if (append_string(p_packet, topic, strlen(topic)) != ESP_OK)
		return fail_message(p_packet);
	fini_message(p_packet, MQTT_MSG_TYPE_UNSUBSCRIBE, 0, 1, 0);
	return ESP_OK;
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
esp_err_t mqtt_msg_pingreq(PacketInfo_t* p_packet) {
	ESP_LOGI(TAG, "655 Msg_PingReq");
	init_message(p_packet);
	return fini_message(p_packet, MQTT_MSG_TYPE_PINGREQ, 0, 0, 0);
}




/*
 * PINGRESP – PING response
 * A PINGRESP Packet is sent by the Server to the Client in response to a PINGREQ Packet.
 *  It indicates that the Server is alive.
 *  This Packet is used in Keep Alive processing.
 */
esp_err_t mqtt_msg_pingresp(PacketInfo_t* p_packet) {
	ESP_LOGI(TAG, "670 Msg_PingResp");
	init_message(p_packet);
	return fini_message(p_packet, MQTT_MSG_TYPE_PINGRESP, 0, 0, 0);
}





/*
 * DISCONNECT – Disconnect notification
 * The DISCONNECT Packet is the final Control Packet sent from the Client to the Server.
 * It indicates that the Client is disconnecting cleanly.
 */
esp_err_t mqtt_msg_disconnect(PacketInfo_t* p_packet) {
	ESP_LOGI(TAG, "685 Msg_Disconnect");
	init_message(p_packet);
	return fini_message(p_packet, MQTT_MSG_TYPE_DISCONNECT, 0, 0, 0);
}






/**
 * Setup the connection information (p_packet).
 * Clear the buffer
 */
void mqtt_msg_init(PacketInfo_t* p_packet, uint8_t *p_buffer, uint16_t p_buffer_length) {
	ESP_LOGI(TAG, "700 Msg_init - Begin.");
	memset(p_packet, 0, sizeof(PacketInfo_t));
	p_packet->PacketBuffer = p_buffer;
	p_packet->PacketBuffer_length = p_buffer_length;
	ESP_LOGI(TAG, "704 Msg_init - Exit ")
}

// ### END DBK
