#ifndef _MQTT_H_
#define _MQTT_H_

/*
 * @file mqtt.h
 * @brief Handle the mqtt protocol; the various message types and their sequencing and interactions.
 */

#include <stdint.h>
#include <string.h>

#include "esp_err.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"

#include "mqtt_structs.h"
#include "mqtt_message.h"
#include "mqtt_transport.h"
#include "ringbuf.h"

#include "sdkconfig.h"


// Specific error codes for the Mqtt component
#define ESP_ERR_MQTT_OK          	ESP_OK                    /*!< No error */
#define ESP_ERR_MQTT_FAIL        	ESP_FAIL                  /*!< General fail code */
#define ESP_ERR_MQTT_NO_MEM      	ESP_ERR_NO_MEM            /*!< Out of memory */
#define ESP_ERR_MQTT_ARG         	ESP_ERR_INVALID_ARG       /*!< Invalid argument */
#define ESP_ERR_MQTT_NOT_SUPPORTED 	ESP_ERR_NOT_SUPPORTED     /*!< Indicates that API is not supported yet */




// New Signatures

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
