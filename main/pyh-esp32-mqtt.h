/*
 * pyh-esp32-mqtt.h
 *
 *  Created on: Jan 27, 2017
 *      Author: briank
 */

#ifndef ESP32_BASE_PROJECT_MAIN_PYH_ESP32_MQTT_H_
#define ESP32_BASE_PROJECT_MAIN_PYH_ESP32_MQTT_H_

#include "esp_err.h"

#define ESP_ERR_WIFI_OK          ESP_OK                    /*!< No error */
#define ESP_ERR_WIFI_FAIL        ESP_FAIL                  /*!< General fail code */
#define ESP_ERR_WIFI_NO_MEM      ESP_ERR_NO_MEM            /*!< Out of memory */
#define ESP_ERR_WIFI_ARG         ESP_ERR_INVALID_ARG       /*!< Invalid argument */
#define ESP_ERR_WIFI_NOT_SUPPORT ESP_ERR_NOT_SUPPORTED     /*!< Indicates that API is not supported yet */


void pyh_mqtt_init(void);
void pyh_mqtt_start(void);

void cb_connected(void *, void *);
void cb_disconnected(void *, void *);
void cb_reconnected(void *, void *);
void cb_subscribe(void *, void *);
void cb_publish(void *, void *);
void cb_data(void *, void *);


#endif /* ESP32_BASE_PROJECT_MAIN_PYH_ESP32_MQTT_H_ */
// ### END DBK
