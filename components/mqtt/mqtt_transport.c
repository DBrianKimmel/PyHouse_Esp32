/*
 * mqtt_transport.c
 *
 *  Created on: Feb 18, 2017
 *      Author: briank
 */

#include <stdio.h>
#include <string.h>

#include "freertos/FreeRTOS.h"
#include "esp_log.h"
#include "esp_system.h"
#include "lwip/sockets.h"
#include "lwip/dns.h"
#include "lwip/netdb.h"


static const char *TAG = "Mqtt_Transport";

/**
 *
 */
static int resolve_dns(const char *host, struct sockaddr_in *ip) {
	struct hostent *he;
	struct in_addr **addr_list;
	ESP_LOGI(TAG, "Resolve_Dns - All");
	he = gethostbyname(host);
	if (he == NULL)
		return 0;
	addr_list = (struct in_addr **) he->h_addr_list;
	if (addr_list[0] == NULL)
		return 0;
	ip->sin_family = AF_INET;
	memcpy(&ip->sin_addr, addr_list[0], sizeof(ip->sin_addr));
	return 1;
}

/**
 * Establish a network connection to the broker.
 */
int mqtt_transport_connect(const char *p_host, int p_port) {
	int l_sock;
	struct sockaddr_in l_remote_ip;
	ESP_LOGI(TAG, "Client_Connect - Begin     ");
	while (1) {
		bzero(&l_remote_ip, sizeof(struct sockaddr_in));
		l_remote_ip.sin_family = AF_INET;
		//if stream_host is not ip address, resolve it
		if (inet_aton(p_host, &(l_remote_ip.sin_addr)) == 0) {
			ESP_LOGI(TAG, "Client_Connect - Resolve dns for domain: %s", p_host);
			if (!resolve_dns(p_host, &l_remote_ip)) {
				vTaskDelay(1000 / portTICK_RATE_MS);
				continue;
			}
		}
		l_sock = socket(PF_INET, SOCK_STREAM, 0);
		if (l_sock == -1) {
			continue;
		}
		l_remote_ip.sin_port = htons(p_port);
		ESP_LOGI(TAG, "Client_Connect - Connecting to server %s: port:%d, From Local port:%d", inet_ntoa((l_remote_ip.sin_addr)), p_port, l_remote_ip.sin_port);
		if (connect(l_sock, (struct sockaddr * )(&l_remote_ip), sizeof(struct sockaddr)) != 00) {
			close(l_sock);
			ESP_LOGE(TAG, "Client_Connect - Network Connection error.");
			vTaskDelay(1000 / portTICK_RATE_MS);
			continue;
		}
		return l_sock;
	}
}
// ### END DBK
