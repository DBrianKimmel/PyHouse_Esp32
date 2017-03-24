/*
 * mqtt_transport.h
 *
 *  Created on: Feb 18, 2017
 *      Author: briank
 */

#ifndef COMPONENTS_MQTT_MQTT_TRANSPORT_H_
#define COMPONENTS_MQTT_MQTT_TRANSPORT_H_

#include <stdint.h>
#include <string.h>


// Public interfaces

/*
 * @param host is the hostname
 * @param port is the port number
 * @return a socket handle.
 */
int mqtt_transport_connect(const char *host, int port);
void mqtt_transport_set_timeout(uint32_t p_socket, int p_timeout);
int mqtt_transport_write(uint32_t p_socket, PacketInfo_t *p_packet);
int mqtt_transport_read(uint32_t p_socket, uint8_t *p_buffers);



#endif /* COMPONENTS_MQTT_MQTT_TRANSPORT_H_ */

// ### END DBK
