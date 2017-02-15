/*
 * @file mqtt.c
 */

#include <stdio.h>
#include <string.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "freertos/queue.h"

#include "esp_log.h"
#include "esp_system.h"

#include "lwip/sockets.h"
#include "lwip/dns.h"
#include "lwip/netdb.h"

#include "sdkconfig.h"

#include "ringbuf.h"
#include "mqtt.h"

static TaskHandle_t xMqttTask = NULL;
static TaskHandle_t xMqttSendingTask = NULL;

static const char *TAG = "Proj_Mqtt";

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

/*
 *
 */
static void mqtt_queue(Client *p_client) {
// TODO: detect buffer full (ringbuf and queue)
	ESP_LOGI(TAG, "Mqtt_Queue - All");
	rb_write(&p_client->send_rb, p_client->State->outbound_message->PayloadData, p_client->State->outbound_message->PayloadLength);
	xQueueSend(p_client->xSendingQueue, &p_client->State->outbound_message->PayloadLength, 0);
}

/**
 * Establish a network connection to the broker.
 */
static int client_connect(const char *stream_host, int stream_port) {
	int l_sock;
	struct sockaddr_in l_remote_ip;
	ESP_LOGI(TAG, "Client_Connect - Begin     ");
	while (1) {
		bzero(&l_remote_ip, sizeof(struct sockaddr_in));
		l_remote_ip.sin_family = AF_INET;
		//if stream_host is not ip address, resolve it
		if (inet_aton(stream_host, &(l_remote_ip.sin_addr)) == 0) {
			ESP_LOGI(TAG, "Client_Connect - Resolve dns for domain: %s", stream_host);
			if (!resolve_dns(stream_host, &l_remote_ip)) {
				vTaskDelay(1000 / portTICK_RATE_MS);
				continue;
			}
		}
		l_sock = socket(PF_INET, SOCK_STREAM, 0);
		if (l_sock == -1) {
			continue;
		}
		l_remote_ip.sin_port = htons(stream_port);
		ESP_LOGI(TAG, "Client_Connect - Connecting to server %s: port:%d, From Local port:%d", inet_ntoa((l_remote_ip.sin_addr)), stream_port, l_remote_ip.sin_port);
		if (connect(l_sock, (struct sockaddr * )(&l_remote_ip), sizeof(struct sockaddr)) != 00) {
			close(l_sock);
			ESP_LOGE(TAG, "Client_Connect - Network Connection error.");
			vTaskDelay(1000 / portTICK_RATE_MS);
			continue;
		}
		return l_sock;
	}
}

/*
 *
 */
void mqtt_sending_task(void *pvParameters) {
	Client *client = (Client *) pvParameters;
	uint32_t msg_len, send_len;
	ESP_LOGI(TAG, "Sending_Task - Begin");
	while (1) {
		if (xQueueReceive(client->xSendingQueue, &msg_len, 1000 / portTICK_RATE_MS)) {
			//queue available
			while (msg_len > 0) {
				send_len = msg_len;
				if (send_len > CONFIG_MQTT_BUFFER_SIZE_BYTE)
					send_len = CONFIG_MQTT_BUFFER_SIZE_BYTE;
				ESP_LOGE(TAG, "Sending_Task - Sending...%d bytes", send_len);

				rb_read(&client->send_rb, client->State->out_buffer, send_len);
				client->State->pending_msg_type = mqtt_get_type(client->State->out_buffer);
				client->State->pending_msg_id = mqtt_get_id(client->State->out_buffer, send_len);
				write(client->Socket, client->State->out_buffer, send_len);
				//TODO: Check sending type, to callback publish message
				msg_len -= send_len;
			}
			//invalidate keepalive timer
			//client->Keep_alive = client->Will->Keep_alive / 2;
		} else {
//			if (client->keepalive_tick > 0)
//				client->keepalive_tick--;
//			else {
//				client->keepalive_tick = client->settings->keepalive / 2;
				client->State->outbound_message = mqtt_msg_pingreq(client->State->mqtt_connection);
				client->State->pending_msg_type = mqtt_get_type(client->State->outbound_message->PayloadData);
				client->State->pending_msg_id = mqtt_get_id(client->State->outbound_message->PayloadData, client->State->outbound_message->PayloadLength);
				ESP_LOGE(TAG, "Sending_Task - Sending pingreq");
				write(client->Socket, client->State->outbound_message->PayloadData, client->State->outbound_message->PayloadLength);
//			}
		}
	}
//	vTaskDelete(NULL);
}

/*
 *
 */
void deliver_publish(Client *p_client, uint8_t *message, int length) {
	MessageInfo event_data;
	int len_read, total_mqtt_len = 0, mqtt_len = 0, mqtt_offset = 0;
	ESP_LOGI(TAG, "Deliver Publish");
	do {
		event_data.MessageTopic_length = length;
		event_data.MessageTopic = mqtt_get_publish_topic(message, &event_data.MessageTopic_length);
		event_data.MessagePayload_length = length;
		event_data.MessagePayload = mqtt_get_publish_data(message, &event_data.MessagePayload_length);
		if (total_mqtt_len == 0) {
			total_mqtt_len = p_client->State->message_length - p_client->State->message_length_read + event_data.MessagePayload_length;
			mqtt_len = event_data.MessagePayload_length;
		} else {
			mqtt_len = len_read;
		}
		event_data.Message_total_length = total_mqtt_len;
		event_data.MessagePayload_offset = mqtt_offset;
		event_data.MessagePayload_length = mqtt_len;
		ESP_LOGI(TAG, "Data received: %d/%d bytes ", mqtt_len, total_mqtt_len);
		if (p_client->Cb->data_cb) {
			p_client->Cb->data_cb(p_client, &event_data);
		}
		mqtt_offset += mqtt_len;
		if (p_client->State->message_length_read >= p_client->State->message_length)
			break;
		len_read = read(p_client->Socket, p_client->State->in_buffer, CONFIG_MQTT_BUFFER_SIZE_BYTE);
		p_client->State->message_length_read += len_read;
	} while (1);
}

/*
 *
 */
void mqtt_start_receive_schedule(Client *p_client) {
	int read_len;
	uint8_t msg_type;
	uint8_t msg_qos;
	uint16_t msg_id;
	ESP_LOGI(TAG, "Receive_Schedule");
	while (1) {
		read_len = read(p_client->Socket, p_client->State->in_buffer, CONFIG_MQTT_BUFFER_SIZE_BYTE);
		ESP_LOGI(TAG, "Receive_Schedule - Read len %d", read_len);
		if (read_len == 0)
			break;
		msg_type = mqtt_get_type(p_client->State->in_buffer);
		msg_qos = mqtt_get_qos(p_client->State->in_buffer);
		msg_id = mqtt_get_id(p_client->State->in_buffer, p_client->State->in_buffer_length);
		ESP_LOGE(TAG, "Receive_Schedule - msg_type %d, msg_id: %d, pending_id: %d", msg_type, msg_id, p_client->State->pending_msg_type);
		switch (msg_type) {
		case MQTT_MSG_TYPE_SUBACK:
			if (p_client->State->pending_msg_type == MQTT_MSG_TYPE_SUBSCRIBE && p_client->State->pending_msg_id == msg_id) {
				ESP_LOGE(TAG, "Receive_Schedule - Subscribe successful");
				if (p_client->Cb->subscribe_cb) {
					p_client->Cb->subscribe_cb(p_client, NULL);
				}
			}
			break;
		case MQTT_MSG_TYPE_UNSUBACK:
			if (p_client->State->pending_msg_type == MQTT_MSG_TYPE_UNSUBSCRIBE && p_client->State->pending_msg_id == msg_id)
				ESP_LOGI(TAG, "Receive_Schedule - UnSubscribe successful");
			break;
		case MQTT_MSG_TYPE_PUBLISH:
			if (msg_qos == 1)
				p_client->State->outbound_message = mqtt_msg_puback(p_client->State->mqtt_connection, msg_id);
			else if (msg_qos == 2)
				p_client->State->outbound_message = mqtt_msg_pubrec(p_client->State->mqtt_connection, msg_id);
			if (msg_qos == 1 || msg_qos == 2) {
				ESP_LOGI(TAG,"Receive_Schedule - Queue response QoS: %d", msg_qos);
				mqtt_queue(p_client);
				// if (QUEUE_Puts(&client->msgQueue, client->mqtt_state.outbound_message->data, client->mqtt_state.outbound_message->length) == -1) {
				//     mqtt_info("MQTT: Queue full");
				// }
			}
			p_client->State->message_length_read = read_len;
			p_client->State->message_length = mqtt_get_total_length(p_client->State->in_buffer, p_client->State->message_length_read);
			ESP_LOGI(TAG, "Receive_Schedule - deliver_publish");

			deliver_publish(p_client, p_client->State->in_buffer, p_client->State->message_length_read);
			deliver_publish(p_client, p_client->State->in_buffer, p_client->State->message_length_read);
			break;
		case MQTT_MSG_TYPE_PUBACK:
			if (p_client->State->pending_msg_type == MQTT_MSG_TYPE_PUBLISH && p_client->State->pending_msg_id == msg_id) {
				ESP_LOGI(TAG, "Receive_Schedule - received MQTT_MSG_TYPE_PUBACK, finish QoS1 publish");
			}

			break;
		case MQTT_MSG_TYPE_PUBREC:
			p_client->State->outbound_message = mqtt_msg_pubrel(p_client->State->mqtt_connection, msg_id);
			mqtt_queue(p_client);
			break;
		case MQTT_MSG_TYPE_PUBREL:
			p_client->State->outbound_message = mqtt_msg_pubcomp(p_client->State->mqtt_connection, msg_id);
			mqtt_queue(p_client);
			break;
		case MQTT_MSG_TYPE_PUBCOMP:
			if (p_client->State->pending_msg_type == MQTT_MSG_TYPE_PUBLISH && p_client->State->pending_msg_id == msg_id) {
				ESP_LOGI(TAG, "Receive_Schedule - Receive MQTT_MSG_TYPE_PUBCOMP, finish QoS2 publish");
			}
			break;
		case MQTT_MSG_TYPE_PINGREQ:
			p_client->State->outbound_message = mqtt_msg_pingresp(p_client->State->mqtt_connection);
			mqtt_queue(p_client);
			break;
		case MQTT_MSG_TYPE_PINGRESP:
			ESP_LOGI(TAG, "MQTT_MSG_TYPE_PINGRESP");
			// Ignore
			break;
		}
	} ESP_LOGI(TAG, "Receive_Schedule - network disconnected");
}

/*
 *
 */
esp_err_t mqtt_destroy(Client *p_client) {
	ESP_LOGI(TAG, "Destroy");
	free(p_client->State->in_buffer);
	free(p_client->State->out_buffer);
	free(p_client);
	vTaskDelete(xMqttTask);
	return ESP_ERR_MQTT_OK;
}

/*
 *
 */
void mqtt_task(void *pvParameters) {
	Client *l_client = (Client *) pvParameters;
	ESP_LOGI(TAG, "Task - Begin.");
	while (1) {
		l_client->Socket = client_connect(l_client->Broker->Host, l_client->Broker->Port);  // Establish a network connection
		ESP_LOGI(TAG, "Task - Connected to server %s:%d", l_client->Broker->Host, l_client->Broker->Port);
		if (!mqtt_connect(l_client)) {
			ESP_LOGE(TAG, "Task - Connect Failed");
			continue;
		}
		ESP_LOGI(TAG, "Task - Connected to MQTT broker, create sending thread before call connected callback");
		xTaskCreate(&mqtt_sending_task, "mqtt_sending_task", 2048, l_client, CONFIG_MQTT_PRIORITY + 1, &xMqttSendingTask);
		if (l_client->Cb->connected_cb) {
			l_client->Cb->connected_cb(l_client, NULL);
		}
		ESP_LOGI(TAG, "Task - mqtt_start_receive_schedule");
		mqtt_start_receive_schedule(l_client);
		close(l_client->Socket);
		vTaskDelete(xMqttSendingTask);
		vTaskDelay(1000 / portTICK_RATE_MS);
	}
	mqtt_destroy(l_client);
}

/*
 *
 */
esp_err_t mqtt_subscribe(Client *p_client, char *topic, uint8_t qos) {
	ESP_LOGI(TAG, "Subscribe - Begin");
	p_client->State->outbound_message = mqtt_msg_subscribe(p_client->State->mqtt_connection, topic, qos, &p_client->State->pending_msg_id);
	ESP_LOGI(TAG, "Subscribe - Queue subscribe, topic\"%s\", id: %d", topic, p_client->State->pending_msg_id);
	mqtt_queue(p_client);
	return ESP_ERR_MQTT_OK;
}

/*
 *
 */
esp_err_t mqtt_publish(Client *p_client, char *p_topic, char *p_data, int p_len, int p_qos, int p_retain) {
	ESP_LOGI(TAG, "Publish - Begin");
	p_client->State->outbound_message = mqtt_msg_publish(p_client->State->mqtt_connection, p_topic, p_data, p_len, p_qos, p_retain, &p_client->State->pending_msg_id);
	mqtt_queue(p_client);
	ESP_LOGI(TAG, "Queuing publish, length: %d, queue size(%d/%d)\r\n", p_client->State->outbound_message->PayloadLength, p_client->send_rb.fill_cnt, p_client->send_rb.size);
	return ESP_ERR_MQTT_OK;
}

/*
 *
 */
void mqtt_stop() {
	ESP_LOGI(TAG, "Stop");
}




/*
 * Set up the data structures
 */
esp_err_t mqtt_init(Client *p_client) {
	ESP_LOGI(TAG, "Init");
	ESP_LOGI(TAG, "Init p_client %d", sizeof(p_client));
	ESP_LOGI(TAG, "Init Client: %d", sizeof(Client));
	ESP_LOGI(TAG, "Init State: %d", sizeof(State));
	ESP_LOGI(TAG, "Init MessageInfo: %d", sizeof(MessageInfo));
	ESP_LOGI(TAG, "Init LastWill: %d", sizeof(LastWill));
	ESP_LOGI(TAG, "Init Callback: %d", sizeof(Callback));
	return ESP_ERR_MQTT_OK;
}

/*
 *
 */
esp_err_t mqtt_start(Client *p_client) {
	// uint8_t *rb_buf;
	ESP_LOGI(TAG, "Start - Client: %d", sizeof(Client));
	int l_heap = esp_get_free_heap_size();
	ESP_LOGI(TAG, "Start - Heap: (0x%X) %d", l_heap, l_heap);

//	p_client->connect_info.client_id = p_settings->client_id;
//	p_client->connect_info.will_topic = p_settings->lwt_topic;
//	p_client->connect_info.will_message = p_settings->lwt_msg;
//	p_client->connect_info.will_qos = p_settings->lwt_qos;
//	p_client->connect_info.will_retain = p_settings->lwt_retain;
//	p_client->keepalive_tick = p_settings->keepalive / 2;
//	p_client->connect_info.keepalive = p_settings->keepalive;
//	p_client->Connection_info.clean_session = p_settings.clean_session;
//	p_client->State.connect_info = &p_client->connect_info;

	/* Create a queue capable of containing 64 unsigned long values. */
	p_client->xSendingQueue = xQueueCreate(64, sizeof(uint32_t));
	ESP_LOGI(TAG, "Start - Created sending queue %p", p_client->xSendingQueue);

	uint8_t *rb_buf = (uint8_t*) malloc(CONFIG_MQTT_QUEUE_BUFFER_SIZE_WORD * 4);
	if (rb_buf == 0) {
		ESP_LOGE(TAG, "Start - Not Enough Memory");
		return ESP_ERR_MQTT_NO_MEM;
	}
	rb_init(&p_client->send_rb, rb_buf, CONFIG_MQTT_QUEUE_BUFFER_SIZE_WORD * 4, 1);
	ESP_LOGI(TAG, "Start - Creating another task");
	mqtt_msg_init(p_client->State->mqtt_connection, p_client->State->out_buffer, p_client->State->out_buffer_length);
	xTaskCreate(&mqtt_task, "mqtt_task", 2048, p_client, CONFIG_MQTT_PRIORITY, &xMqttTask);
	ESP_LOGI(TAG, "Start - Done.");
	return ESP_ERR_MQTT_OK;
}

/*
 * mqtt_connect
 * input - client
 * return 1: success, 0: fail
 */
esp_err_t mqtt_connect(Client *p_client) {
	struct timeval tv;
	tv.tv_sec = 10; /* 30 Secs Timeout */
	tv.tv_usec = 0;  // Not init'ing this can cause strange error                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                 s
	ESP_LOGI(TAG, "Connect - Begin");

	setsockopt(p_client->Socket, SOL_SOCKET, SO_RCVTIMEO, (char * )&tv, sizeof(struct timeval));
	ESP_LOGI(TAG, "Connect - Socket options set");

	mqtt_msg_init(p_client->State->mqtt_connection, p_client->State->out_buffer, p_client->State->out_buffer_length);
	ESP_LOGI(TAG, "Connect - Message initialized");

	p_client->State->outbound_message = mqtt_msg_connect(p_client->State->mqtt_connection, &p_client->Connection_info);
	p_client->State->pending_msg_type = mqtt_get_type(p_client->State->outbound_message->PayloadData);
	p_client->State->pending_msg_id   = mqtt_get_id(p_client->State->outbound_message->PayloadData, p_client->State->outbound_message->PayloadLength);
	ESP_LOGI(TAG, "Connect - Sending MQTT CONNECT message, MsgType: %d, ClientId: %04X", p_client->State->pending_msg_type, p_client->State->pending_msg_id);

	ESP_LOGI(TAG, "Connect - Write Socket");
	int write_len = write(p_client->Socket, p_client->State->outbound_message->PayloadData, p_client->State->outbound_message->PayloadLength);
	ESP_LOGI(TAG, "Connect - Write Len: %d - %d ", write_len, p_client->State->outbound_message->PayloadLength)

	ESP_LOGI(TAG, "Connect - Reading MQTT CONNECT response message");
	int read_len = read(p_client->Socket, p_client->State->in_buffer, CONFIG_MQTT_BUFFER_SIZE_BYTE);
	ESP_LOGI(TAG, "Connect - ReadLen: %d - %d", read_len, sizeof(p_client->State->in_buffer));

	tv.tv_sec = 0; /* No timeout */
	setsockopt(p_client->Socket, SOL_SOCKET, SO_RCVTIMEO, (char * )&tv, sizeof(struct timeval));

	if (read_len < 0) {
		ESP_LOGE(TAG, "Connect - Error network response");
		return ESP_ERR_MQTT_FAIL;
	}
	if (mqtt_get_type(p_client->State->in_buffer) != MQTT_MSG_TYPE_CONNACK) {
		ESP_LOGE(TAG, "Connect - Invalid MSG_TYPE response: %d, read_len: %d", mqtt_get_type(p_client->State->in_buffer), read_len);
		return ESP_ERR_MQTT_FAIL;
	}
	int connect_rsp_code = mqtt_get_connect_return_code(p_client->State->in_buffer);
	switch (connect_rsp_code) {
		case CONNECTION_ACCEPTED:
			ESP_LOGI(TAG, "Connect - Connected");
			return ESP_ERR_MQTT_OK;
		case CONNECTION_REFUSE_PROTOCOL:
		case CONNECTION_REFUSE_SERVER_UNAVAILABLE:
		case CONNECTION_REFUSE_BAD_USERNAME:
		case CONNECTION_REFUSE_NOT_AUTHORIZED:
			ESP_LOGW(TAG, "Connect - Connection refused, reason code: %d", connect_rsp_code);
			return ESP_ERR_MQTT_FAIL;
		default:
			ESP_LOGW(TAG, "Connect - Connection refused, Unknown reason");
			return ESP_ERR_MQTT_FAIL;
	}
	return ESP_ERR_MQTT_OK;
}

// ### END DBK
