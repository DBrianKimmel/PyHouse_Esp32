/*
 * esp32.c
 *
 *  Created on: Jan 25, 2017
 *      Author: briank
 *
 * This is a main app for the Esp32 device.
 * Major functions are in separate files (WiFi, OTA, Mqtt etc.).
 */

#include <string.h>
//#include "posix/sys/socket.h"
#include "posix/netdb.h"
#include <errno.h>

#include "freertos/FreeRTOS.h"
#include "esp_system.h"
#include "esp_event_loop.h"
#include "esp_log.h"
#include "esp_partition.h"
#include "nvs_flash.h"

#include "sdkconfig.h"
#include "pyh-esp32-mqtt.h"
#include "pyh-esp32-ota.h"
#include "pyh-esp32-wifi.h"


static const char *TAG = "PyHouse";
// static const char *VERSION = "00.00.01"

void __attribute__((noreturn)) task_fatal_error() {
	ESP_LOGE(TAG, "Exiting task due to fatal error...\n");
	// close(socket_id);
	(void) vTaskDelete(NULL);
	while (1) {
		;
	}
}

void main_task(void *pvParameter) {
	ESP_LOGI(TAG, "Starting PyHouse Main Task.");
	pyh_wifi_start();
	pyh_mqtt_start();
	// pyh_ota_start();
	ESP_LOGI(TAG, "Started.\n");
	while(1) {
		vTaskDelay(2000 / portTICK_PERIOD_MS);
	}
}

void app_main() {
	ESP_LOGI(TAG, "====================\n\n");
	ESP_LOGI(TAG, "Initializing.");
	nvs_flash_init();
	pyh_wifi_init();
	pyh_mqtt_init();
	// pyh_ota_init();
	ESP_LOGI(TAG, "Initialized.\n");
	xTaskCreate(&main_task, "main_task", 8192, NULL, 5, NULL);
}
// END DBK
