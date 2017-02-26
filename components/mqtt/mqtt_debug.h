#ifndef COMPONENTS_MQTT_MQTT_DEBUG_H_
#define COMPONENTS_MQTT_MQTT_DEBUG_H_

/*
 * @file  mqtt_debug.h
 *
 *  Created on: Feb 24, 2017
 *      Author: briank
 */

#include "esp_err.h"

#include "mqtt.h"
//#include "mqtt_message.h"

void print_client(Client_t *p_client);
void print_packet(PacketInfo_t *p_packet);


#endif /* COMPONENTS_MQTT_MQTT_DEBUG_H_ */

// ### END DBK
