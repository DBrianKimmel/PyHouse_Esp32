#ifndef _MQTT_H_
#define _MQTT_H_

/*
 * @file mqtt.h
 * @brief Handle the mqtt protocol; the various message types and their sequencing and interactions.
 */

#include <string.h>

#include "mqtt_structs.h"

// New Signatures

static inline int mqtt_get_packet_type(uint8_t* p_buffer) {
	return (p_buffer[0] & 0xf0) >> 4;
}
static inline int mqtt_get_packet_dup_flag(uint8_t* p_buffer) {
	return (p_buffer[0] & 0x08) >> 3;
}
static inline int mqtt_get_packet_qos(uint8_t* p_buffer) {
	return (p_buffer[0] & 0x06) >> 1;
}
static inline int mqtt_get_packet_retain(uint8_t* p_buffer) {
	return (p_buffer[0] & 0x01);
}
static inline int mqtt_get_packet_connect_return_code(uint8_t* p_buffer) {
	return p_buffer[3];
}


/**
 * @brief Initialize the esp Mqtt subsystem.
 */
esp_err_t Mqtt_init(Client_t*);
esp_err_t Mqtt_start(Client_t*, char* will_topic, char* will_message);

void mqtt_task(void *);
esp_err_t mqtt_connect(Client_t*);
esp_err_t mqtt_detroy(Client_t*);
esp_err_t mqtt_subscribe(Client_t*, char*, uint8_t);
esp_err_t mqtt_publish(Client_t*, char *, char *, int, int, int);

#endif  /* __MQTT_H__ */

// ### END DBK
