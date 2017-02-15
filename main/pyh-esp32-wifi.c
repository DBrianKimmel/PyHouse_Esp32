/*
 * esp32-wifi.c
 *
 *  Created on: Jan 26, 2017
 *      Author: briank
 *
 *  This is the WiFi functionality.
 */

#include "pyh-esp32-wifi.h"

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include "posix/sys/socket.h"
#include "posix/netdb.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"

#include "esp_wifi.h"
#include "esp_system.h"
#include "esp_event_loop.h"
#include "esp_log.h"
#include "esp_partition.h"

#include "lwip/inet.h"
#include "lwip/ip4_addr.h"
#include "lwip/ip_addr.h"
#include "lwip/dns.h"

#include "sdkconfig.h"


static const char *TAG = "PyH_Wifi";
// static const char *VERSION = "00.00.01"

wifi_ap_record_t l_connected_ap;

esp_err_t pyh_wifi_setup() {
	return ESP_ERR_WIFI_OK;
}

/* FreeRTOS event group to signal when we are connected & ready to make a request */
static EventGroupHandle_t l_wifi_event_group;

/* The event group allows multiple bits for each event, but we only care about one event
 *  - are we connected to the AP with an IP?
 */
const int CONNECTED_BIT = BIT0;

static esp_err_t event_handler(void *ctx, system_event_t *p_event) {
	switch (p_event->event_id) {
	case SYSTEM_EVENT_STA_START:
		ESP_LOGI(TAG, "Event Sta_Start");
		esp_wifi_connect();
		break;
	case SYSTEM_EVENT_STA_GOT_IP:
		ESP_LOGI(TAG, "Event Sta_Got_Ip");
		xEventGroupSetBits(l_wifi_event_group, CONNECTED_BIT);
		break;
	case SYSTEM_EVENT_STA_DISCONNECTED:
		ESP_LOGI(TAG, "Event Sta_Disconnected");
		/* This is a workaround as ESP32 WiFi libs don't currently auto-reassociate. */
		esp_wifi_connect();
		xEventGroupClearBits(l_wifi_event_group, CONNECTED_BIT);
		break;
	default:
		ESP_LOGI(TAG, "Event Default Id:%d", p_event->event_id);
		break;
	}
	return ESP_OK;
}

void pyh_wifi_start(void) {
	// esp_err_t err;
	ESP_LOGI(TAG, "Wifi Starting");
	// Wait for the callback to set the CONNECTED_BIT in the event group.
	xEventGroupWaitBits(l_wifi_event_group, CONNECTED_BIT, false, true, portMAX_DELAY);
	ESP_LOGI(TAG, "Wifi - Started and Connected.\n");
}

void pyh_wifi_init() {
	ESP_LOGI(TAG, "Wifi - Initializing");
	tcpip_adapter_init();
	l_wifi_event_group = xEventGroupCreate();
	ESP_ERROR_CHECK(esp_event_loop_init(event_handler, NULL));

	wifi_init_config_t l_cfg = WIFI_INIT_CONFIG_DEFAULT();
	ESP_ERROR_CHECK(esp_wifi_init(&l_cfg));

	ESP_ERROR_CHECK(esp_wifi_set_storage(WIFI_STORAGE_RAM));
	wifi_config_t l_wifi_config = { .sta = { .ssid = CONFIG_PYHOUSE_WIFI_SSID, .password = CONFIG_PYHOUSE_WIFI_PASSWORD, }, };
	ESP_LOGI(TAG, "Setting WiFi configuration SSID: %s", l_wifi_config.sta.ssid);
	ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
	ESP_ERROR_CHECK(esp_wifi_set_config(ESP_IF_WIFI_STA, &l_wifi_config));
	ESP_ERROR_CHECK(esp_wifi_start());
	ESP_LOGI(TAG, "Wifi - Initialized\n");
}
// ### END DBK
