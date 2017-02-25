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


#endif /* COMPONENTS_MQTT_MQTT_TRANSPORT_H_ */

// ### END DBK
