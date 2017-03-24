/*
 * mqtt_packet.c
 *
 *  Created on: Mar 16, 2017
 *      Author: briank
 *
 * This module builds MQTT Control Packets.
 * See:  http://docs.oasis-open.org/mqtt/mqtt/v3.1.1/mqtt-v3.1.1.pdf
 *
 * MQTT cONTROL Packets have 3 parts:
 * |==================================================================|
 * | Fixed length header     |  Present in all MQTT Control Packets   |
 * |-------------------------|----------------------------------------|
 * | Variable length header  |  Present in some MQTT Control Packets  |
 * |-------------------------|----------------------------------------|
 * | Payload                 |  Present in some MQTT Control Packets  |
 * |==================================================================|
 */



#include <stdint.h>
#include <string.h>

#include "esp_log.h"

#include "mqtt.h"
#include "mqtt_packet.h"
#include "mqtt_debug.h"


static const char *TAG = "MqttPacket    ";

enum mqtt_connect_flag {
	MQTT_CONNECT_FLAG_USERNAME = 1 << 7,
	MQTT_CONNECT_FLAG_PASSWORD = 1 << 6,
	MQTT_CONNECT_FLAG_WILL_RETAIN = 1 << 5,
	MQTT_CONNECT_FLAG_WILL = 1 << 2,
	MQTT_CONNECT_FLAG_CLEAN_SESSION = 1 << 1
};

/**
 * Encode a utf-8 string and append it to the Payload.
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
static esp_err_t  append_string(PacketInfo_t* p_packet, const char* p_string, int p_len) {
	ESP_LOGI(TAG, "Append_String - Buffer:%p;  Offset:%d;  String:%s;  Len:%d",
			p_packet->PacketPayload, p_packet->PacketPayload_length, p_string, p_len);
//	if (p_len + 2 > p_packet->PacketPayload_length) {
//		ESP_LOGE(TAG, " 82 Append_String - Not enough room.")
//		return ESP_ERR_NO_MEM;
//	}
	p_packet->PacketPayload[p_packet->PacketPayload_length++] = p_len >> 8;
	p_packet->PacketPayload[p_packet->PacketPayload_length++] = p_len & 0xff;
	memcpy(p_packet->PacketPayload + p_packet->PacketPayload_length, p_string, p_len);
	p_packet->PacketPayload_length += p_len;
	return ESP_OK;
}

void packet_failure(Client_t *p_client) {
	p_client->Packet->Packet_length = 0;
}

/**
 * Put the Packet ID into the variable header and return the value put.
 */
uint16_t put_packet_id(Client_t *p_client) {
	p_client->Packet->PacketId++;
	p_client->Packet->PacketVariableHeader_length = 2;
	p_client->Packet->PacketVariableHeader[0] = p_client->Packet->PacketId >> 8;
	p_client->Packet->PacketVariableHeader[1] = p_client->Packet->PacketId & 0xff;
	return p_client->Packet->PacketId;
}

void get_packet_id() {

}

/*
 * Since we are building packets to write,
 */
void packet_finish(Client_t *p_client) {
	int l_remaining_length = p_client->Packet->PacketVariableHeader_length + p_client->Packet->PacketPayload_length;
	p_client->Buffers->out_buffer_length = p_client->Packet->Packet_length;
	if (l_remaining_length > 127) {
		p_client->Packet->PacketFixedHeader[1] = 0x80 | (l_remaining_length % 128);
		p_client->Packet->PacketFixedHeader[2] = l_remaining_length / 128;
		p_client->Packet->PacketFixedHeader_length = 3;
	} else {
		p_client->Packet->PacketFixedHeader[1] = l_remaining_length;
		p_client->Packet->PacketFixedHeader_length = 2;
	}
}

// ====== The 14 control packets follow ========

/**
 * CONNECT (1) – Client requests a connection to a Server
 *
 * After a Network Connection is established by a Client to a Server, the first Packet sent from the Client to the Server MUST be a CONNECT Packet [MQTT-3.1.0-1].
 * A Client can only send the CONNECT Packet once over a Network Connection.
 * The Server MUST process a second CONNECT Packet sent from a Client as a protocol violation and disconnect the Client [MQTT-3.1.0-2].
 * See section 4.8 for information about handling errors.
 * The payload contains one or more encoded fields.
 * They specify a unique Client identifier for the Client, a Will topic, Will Message, User Name and Password.
 * All but the Client identifier are optional and their presence is determined based on flags in the variable header.
 */
esp_err_t mqtt_build_connect_packet(Client_t* p_client) {
	ESP_LOGI(TAG, "BuildConnectPacket - Begin.");

	// Fixed Header
	ESP_LOGI(TAG, "BuildConnectPacket - Fixed Header.");
	p_client->Packet->PacketFixedHeader[0] = MQTT_CONTROL_PACKET_TYPE_CONNECT << 4 | 0;
	p_client->Packet->PacketFixedHeader[1] = 0;
	p_client->Packet->PacketFixedHeader[2] = 0;
	p_client->Packet->PacketFixedHeader[3] = 0;

	// Build the variable header (10 bytes)
	ESP_LOGI(TAG, "BuildConnectPacket - Variable.");
	p_client->Packet->PacketVariableHeader = calloc(10, sizeof(uint8_t));
	p_client->Packet->PacketVariableHeader_length = 10;
	p_client->Packet->PacketVariableHeader[0] = 0;
	p_client->Packet->PacketVariableHeader[1] = 4;
	memcpy(&p_client->Packet->PacketVariableHeader[2], "MQTT", 4);
	p_client->Packet->PacketVariableHeader[6] = 4;
	p_client->Packet->PacketVariableHeader[7] = 0;
	if (p_client->Will->CleanSession) {
		p_client->Packet->PacketVariableHeader[7] |= 1 << 1; // Clean session flag
	}
	p_client->Packet->PacketVariableHeader[8] = p_client->Will->Keepalive >> 8;
	p_client->Packet->PacketVariableHeader[9] = p_client->Will->Keepalive & 0xff;

	// Build the Payload
	ESP_LOGI(TAG, "BuildConnectPacket - Payload.");
	p_client->Packet->PacketPayload_length = 0;
	p_client->Packet->PacketPayload = calloc(1024, sizeof(uint8_t));

	if (p_client->Broker->ClientId != 0 && p_client->Broker->ClientId[0] != '\0') {
		if (append_string(p_client->Packet, p_client->Broker->ClientId, strlen(p_client->Broker->ClientId)) != ESP_OK) {
			ESP_LOGE(TAG, "BuildConnectPacket - Failed - Wrong ID");
			packet_failure(p_client);
			return ESP_ERR_INVALID_ARG;
		}
	} else {
		ESP_LOGE(TAG, "BuildConnectPacket - Bad ID");
		print_packet(p_client->Packet);
		packet_failure(p_client);
		return ESP_ERR_INVALID_ARG;
	}

	ESP_LOGI(TAG, "BuildConnectPacket - Will Topic.");
	if (p_client->Will->WillTopic != 0 && p_client->Will->WillTopic[0] != '\0') {
		if (append_string(p_client->Packet, p_client->Will->WillTopic, strlen(p_client->Will->WillTopic)) != ESP_OK) {
			ESP_LOGE(TAG, "BuildConnectPacket - Failed - Will topic bad")
			packet_failure(p_client);
			return ESP_ERR_INVALID_ARG;
		}
		if (append_string(p_client->Packet, p_client->Will->WillMessage, strlen(p_client->Will->WillMessage)) != ESP_OK) {
			ESP_LOGE(TAG, "BuildConnectPacket - Failed - Will message bad");
			packet_failure(p_client);
			return ESP_ERR_INVALID_ARG;
		}
		p_client->Packet->PacketVariableHeader[7] |= MQTT_CONNECT_FLAG_WILL;
		if (p_client->Will->WillRetain) {
			p_client->Packet->PacketVariableHeader[7] |= MQTT_CONNECT_FLAG_WILL_RETAIN;
		}
		p_client->Packet->PacketVariableHeader[7] |= (p_client->Will->WillQos & 3) << 3;
	}

	ESP_LOGI(TAG, "BuildConnectPacket - Username.");
	if (p_client->Broker->Username != 0 && p_client->Broker->Username[0] != '\0') {
		if (append_string(p_client->Packet, p_client->Broker->Username, strlen(p_client->Broker->Username)) < 0) {
			ESP_LOGE(TAG, "BuildConnectPacket - Failed - Username");
			packet_failure(p_client);
			return ESP_ERR_INVALID_ARG;
		}
		p_client->Packet->PacketVariableHeader[7] |= MQTT_CONNECT_FLAG_USERNAME;
	}

	ESP_LOGI(TAG, "BuildConnectPacket - Password.");
	if (p_client->Broker->Password != 0 && p_client->Broker->Password[0] != '\0') {
		if (append_string(p_client->Packet, p_client->Broker->Password, strlen(p_client->Broker->Password)) < 0) {
			ESP_LOGE(TAG, "BuildConnectPacket - Failed - Password");
			packet_failure(p_client);
			return ESP_ERR_INVALID_ARG;
		}
		p_client->Packet->PacketVariableHeader[7] |= MQTT_CONNECT_FLAG_PASSWORD;
	}

	ESP_LOGI(TAG, "BuildConnectPacket - Finish.");
	packet_finish(p_client);
	print_packet(p_client->Packet);
	ESP_LOGI(TAG, "BuildConnectPacket - Succeeded")
	return ESP_OK;
}

/**
 * CONNACK (2) – Acknowledge connection request
 * The CONNACK Packet is the packet sent by the Server in response to a CONNECT Packet received from a Client.
 * The first packet sent from the Server to the Client MUST be a CONNACK Packet [MQTT- 3.2.0-1].
 * If the Client does not receive a CONNACK Packet from the Server within a reasonable amount of time, the Client SHOULD close the Network Connection.
 * A "reasonable" amount of time depends on the type of application and the communications infrastructure.
 *
 * We Never need to do this!
 */
esp_err_t mqtt_build_connack_packet(Client_t* p_client) {
	ESP_LOGE(TAG, "BuildConnackPacket - Not Allowed.");
	return ESP_FAIL;
}

/**
 * PUBLISH (3) – Publish message
 * A PUBLISH Control Packet is sent from a Client to a Server or from Server to a Client to transport an Application Message.
 */
esp_err_t mqtt_build_publish_packet(Client_t* p_client) {
	ESP_LOGI(TAG, "3 BuildPublishPacket - Begin.");
	print_packet(p_client->Packet);
	ESP_LOGI(TAG, "3-Z BuildPublishPacket - Succeeded")
	return ESP_OK;
}

/**
 * PUBACK (4) – Publish acknowledgement
 * A PUBACK Packet is the response to a PUBLISH Packet with QoS level 1.
 */
esp_err_t mqtt_build_puback_packet(Client_t* p_client) {
	ESP_LOGI(TAG, "4 BuildPubackPacket - Begin.");
	print_packet(p_client->Packet);
	ESP_LOGI(TAG, "4-Z BuildAckackPacket - Succeeded")
	return ESP_OK;
}

/**
 * PUBREC (5) – Publish received (QoS 2 publish received, part 1)
 * A PUBREC Packet is the response to a PUBLISH Packet with QoS 2.
 * It is the second packet of the QoS 2 protocol exchange.
 */
esp_err_t mqtt_build_pubrec_packet(Client_t* p_client) {
	ESP_LOGI(TAG, "5 BuildPubrecPacket - Begin.");
	print_packet(p_client->Packet);
	ESP_LOGI(TAG, "15-Z BuildPubrecPacket - Succeeded")
	return ESP_OK;
}

/**
 * PUBREL (6) – Publish release (QoS 2 publish received, part 2)
 * A PUBREL Packet is the response to a PUBREC Packet.
 * It is the third packet of the QoS 2 protocol exchange.
 */
esp_err_t mqtt_build_pubrel_packet(Client_t* p_client) {
	ESP_LOGI(TAG, "6 BuildPubrelPacket - Begin.");
	print_packet(p_client->Packet);
	ESP_LOGI(TAG, "6-Z BuildPubrelPacket - Succeeded")
	return ESP_OK;
}

/**
 * PUBCOMP (7) – Publish complete (QoS 2 publish received, part 3)
 * The PUBCOMP Packet is the response to a PUBREL Packet.
 * It is the fourth and final packet of the QoS 2 protocol exchange.
 */
esp_err_t mqtt_build_pubcomp_packet(Client_t* p_client) {
	ESP_LOGI(TAG, "7 BuildPubcompPacket - Begin.");
	print_packet(p_client->Packet);
	ESP_LOGI(TAG, "7-Z BuildPubcompPacket - Succeeded")
	return ESP_OK;
}

/**
 * SUBSCRIBE (8) - Subscribe to topics
 * The SUBSCRIBE Packet is sent from the Client to the Server to create one or more Subscriptions.
 * Each Subscription registers a Client’s interest in one or more Topics.
 * The Server sends PUBLISH Packets to the Client in order to forward Application Messages that were published
 *  to Topics that match these Subscriptions.
 * The SUBSCRIBE Packet also specifies (for each Subscription) the maximum QoS with which the Server can send
 *  Application Messages to the Client.
 *
 * @param p_client
 */
esp_err_t mqtt_build_subscribe_packet(Client_t* p_client, char *p_topic, uint8_t p_qos, uint16_t *r_id) {
	ESP_LOGI(TAG, "8 BuildSubscribePacket - Begin.");
	// Fixed Header
	ESP_LOGI(TAG, "BuildSubscribePacket - Fixed Header.");
	p_client->Packet->PacketFixedHeader[0] = MQTT_CONTROL_PACKET_TYPE_SUBSCRIBE << 4 | 2;
	p_client->Packet->PacketFixedHeader[1] = 0;
	p_client->Packet->PacketFixedHeader[2] = 0;
	p_client->Packet->PacketFixedHeader[3] = 0;
	// Build the variable header (2 bytes)
	*r_id = put_packet_id(p_client);
	// Build the Payload
	ESP_LOGI(TAG, "BuildSubscribePacket - Payload.");
	append_string(p_client->Packet, p_topic, strlen(p_topic));
	p_client->Packet->PacketPayload[p_client->Packet->PacketPayload_length] = p_qos;
	p_client->Packet->PacketPayload_length++;
	print_packet(p_client->Packet);
	ESP_LOGI(TAG, "8-Z BuildSubscribePacket - Succeeded")
	return ESP_OK;
}

/**
 * SUBACK (9) – Subscribe acknowledgement
 * A SUBACK Packet is sent by the Server to the Client to confirm receipt and processing of a SUBSCRIBE Packet.
 * A SUBACK Packet contains a list of return codes, that specify the maximum QoS level that was granted in each Subscription
 *  that was requested by the SUBSCRIBE.
 *
 *  We never need to do this!
 */
esp_err_t mqtt_build_suback_packet(Client_t* p_client) {
	ESP_LOGE(TAG, "BuildSubackPacket - Not Allowed.");
	return ESP_FAIL;
}

/**
 * UNSUBSCRIBE (10) – Unsubscribe from topics
 * An UNSUBSCRIBE Packet is sent by the Client to the Server, to unsubscribe from topics.
 */
esp_err_t mqtt_build_unsubscribe_packet(Client_t* p_client) {
	ESP_LOGI(TAG, "10 BuildUnsubscribePacket - Begin.");
	// Fixed Header
	p_client->Packet->PacketFixedHeader_length = 2;
	*p_client->Packet->PacketFixedHeader = MQTT_CONTROL_PACKET_TYPE_PINGREQ << 4;
	ESP_LOGI(TAG, "10-A BuildUnsubscribePacket - Fixed.");
	p_client->Packet->PacketFixedHeader[1] = 0;
	p_client->Packet->PacketFixedHeader[2] = 0;
	p_client->Packet->PacketFixedHeader[3] = 0;
	// Build the variable header (2 bytes)
	uint16_t l_packet_id = put_packet_id(p_client);
	// Build the Payload
	ESP_LOGI(TAG, "BuildUnspuubscribePacket - Payload.");

	print_packet(p_client->Packet);
	ESP_LOGI(TAG, "10-Z BuildUnsubscribePacket - Succeeded")
	return ESP_OK;
}

/** done
 * UNSUBACK (11) – Unsubscribe acknowledgement
 * The UNSUBACK Packet is sent by the Server to the Client to confirm receipt of an UNSUBSCRIBE Packet.
 *
 * We do not need to do this!
 */
esp_err_t mqtt_build_unsuback_packet(Client_t* p_client) {
	ESP_LOGI(TAG, "BuildUnsubackPacket - Not Allowed.");
	return ESP_FAIL;
}

/** done
 * PINGREQ (12) – PING request
 * The PINGREQ Packet is sent from a Client to the Server.
 *  It can be used to:
 *   1. Indicate to the Server that the Client is alive in the absence of any other Control Packets being sent from the Client to the Server.
 *   2. Request that the Server responds to confirm that it is alive.
 *   3. Exercise the network to indicate that the Network Connection is active.
 * This Packet is used in Keep Alive processing.
 */
esp_err_t mqtt_build_pingreq_packet(Client_t* p_client) {
	ESP_LOGI(TAG, "12 BuildPingreqPacket - Begin.");

	// Fixed Header
	p_client->Packet->PacketFixedHeader_length = 2;
	*p_client->Packet->PacketFixedHeader = MQTT_CONTROL_PACKET_TYPE_PINGREQ << 4;
	ESP_LOGI(TAG, "12-A BuildPingreqPacket - Fixed.");
	p_client->Packet->PacketFixedHeader[1] = 0;
	// Variable Header
	ESP_LOGI(TAG, "12-B BuildPingreqPacket - Variable.");
	p_client->Packet->PacketVariableHeader_length = 0;
	// Build the Payload
	ESP_LOGI(TAG, "12-C BuildPingreqPacket - Payload.");
	p_client->Packet->PacketPayload_length = 0;
//	print_packet(p_client->Packet);
	ESP_LOGI(TAG, "12-Z BuildPingreqPacket - Succeeded")
	return ESP_OK;
}

/** done
 * PINGRESP (13) – PING response
 * A PINGRESP Packet is sent by the Server to the Client in response to a PINGREQ Packet.
 *  It indicates that the Server is alive.
 *  This Packet is used in Keep Alive processing.
 *
 *  We do not need to do this!
 */
esp_err_t mqtt_build_pingresp_packet(Client_t* p_client) {
	ESP_LOGI(TAG, "BuildPingRespPacket - Not Allowed.");
	return ESP_FAIL;
}

/** done
 * DISCONNECT (14) – Disconnect notification
 * The DISCONNECT Packet is the final Control Packet sent from the Client to the Server.
 * It indicates that the Client is disconnecting cleanly.
 */
esp_err_t mqtt_build_disconnect_packet(Client_t* p_client) {
	ESP_LOGI(TAG, "14 BuildDisconnectPacket - Begin.");
	// Fixed Header
	p_client->Packet->PacketFixedHeader[0] = MQTT_CONTROL_PACKET_TYPE_DISCONNECT << 4 | 0;
	p_client->Packet->PacketFixedHeader[1] = 0;
	// Variable Header
	p_client->Packet->PacketVariableHeader_length = 0;
// Build the Payload
	p_client->Packet->PacketPayload_length = 0;
//	print_packet(p_client->Packet);
	ESP_LOGI(TAG, "14-Z BuildDisconnectPacket - Succeeded")
	return ESP_OK;
}




// ### END DBK
