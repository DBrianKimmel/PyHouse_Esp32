/*
 * mqtt_packet.h
 *
 *  Created on: Mar 17, 2017
 *      Author: briank
 */

#ifndef COMPONENTS_MQTT_MQTT_PACKET_H_
#define COMPONENTS_MQTT_MQTT_PACKET_H_

#include "mqtt_structs.h"

enum mqtt_ctl_packet_type {
	MQTT_CONTROL_PACKET_TYPE_CONNECT = 1,
	MQTT_CONTROL_PACKET_TYPE_CONNACK = 2,
	MQTT_CONTROL_PACKET_TYPE_PUBLISH = 3,
	MQTT_CONTROL_PACKET_TYPE_PUBACK = 4,
	MQTT_CONTROL_PACKET_TYPE_PUBREC = 5,
	MQTT_CONTROL_PACKET_TYPE_PUBREL = 6,
	MQTT_CONTROL_PACKET_TYPE_PUBCOMP = 7,
	MQTT_CONTROL_PACKET_TYPE_SUBSCRIBE = 8,
	MQTT_CONTROL_PACKET_TYPE_SUBACK = 9,
	MQTT_CONTROL_PACKET_TYPE_UNSUBSCRIBE = 10,
	MQTT_CONTROL_PACKET_TYPE_UNSUBACK = 11,
	MQTT_CONTROL_PACKET_TYPE_PINGREQ = 12,
	MQTT_CONTROL_PACKET_TYPE_PINGRESP = 13,
	MQTT_CONTROL_PACKET_TYPE_DISCONNECT = 14
};

enum mqtt_connect_return_code {
	CONNECTION_ACCEPTED = 0,
	CONNECTION_REFUSE_PROTOCOL,
	CONNECTION_REFUSE_ID_REJECTED,
	CONNECTION_REFUSE_SERVER_UNAVAILABLE,
	CONNECTION_REFUSE_BAD_USERNAME,
	CONNECTION_REFUSE_NOT_AUTHORIZED
};

esp_err_t mqtt_build_connect_packet(Client_t* p_client);     // 1
esp_err_t mqtt_build_connack_packet(Client_t* p_client);     // 2
esp_err_t mqtt_build_publish_packet(Client_t* p_client);     // 3
esp_err_t mqtt_build_puback_packet(Client_t* p_client);      // 4
esp_err_t mqtt_build_pubrec_packet(Client_t* p_client);      // 5
esp_err_t mqtt_build_pubrel_packet(Client_t* p_client);      // 6
esp_err_t mqtt_build_pubcomp_packet(Client_t* p_client);     // 7
esp_err_t mqtt_build_subscribe_packet(Client_t* p_client, char *p_topic, uint8_t p_qos, uint16_t *r_id); // 8
esp_err_t mqtt_build_suback_packet(Client_t* p_client);      // 9
esp_err_t mqtt_build_unsubscribe_packet(Client_t* p_client); // 10
esp_err_t mqtt_build_unsuback_packet(Client_t* p_client);    // 11
esp_err_t mqtt_build_pingreq_packet(Client_t* p_client);     // 12
esp_err_t mqtt_build_pingresp_packet(Client_t* p_client);    // 13
esp_err_t mqtt_build_disconnect_packet(Client_t* p_client);  // 14

#endif /* COMPONENTS_MQTT_MQTT_PACKET_H_ */
