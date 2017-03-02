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

#include "lwip/sockets.h"
#include "lwip/dns.h"
#include "lwip/netdb.h"

#include "sdkconfig.h"

#include "mqtt_structs.h"
#include "ringbuf.h"
#include "mqtt_message.h"
#include "mqtt_debug.h"
#include "mqtt_transport.h"
#include "mqtt.h"

static TaskHandle_t xMqttTask = NULL;
static TaskHandle_t xMqttSendingTask = NULL;

static const char *TAG = "Mqtt          ";

/*
 *
 */
static void mqtt_queue(Client_t *p_client) {
// TOD: detect buffer full (ringbuf and queue)
	ESP_LOGI(TAG, " 38 Mqtt_Queue - All");
	rb_write(p_client->send_rb, p_client->State->outbound_message->PayloadData, p_client->State->outbound_message->PayloadLength);
	ESP_LOGI(TAG, " 40 Mqtt_Queue - All");
	xQueueSend(p_client->xSendingQueue, &p_client->State->outbound_message->PayloadLength, 0);
	ESP_LOGI(TAG, " 42 Mqtt_Queue - All");
}

/*
 * A FreeRtos TASK
 */
void mqtt_sending_task(void *pvParameters) {
	Client_t *l_client = (Client_t *) pvParameters;
	uint32_t msg_len, send_len;

	ESP_LOGI(TAG, " 47 Sending_Task - Begin");
	while (1) {
		if (xQueueReceive(l_client->xSendingQueue, &msg_len, 1000 / portTICK_RATE_MS)) {
			//queue available
			while (msg_len > 0) {
				send_len = msg_len;
				if (send_len > CONFIG_MQTT_BUFFER_SIZE_BYTE)
					send_len = CONFIG_MQTT_BUFFER_SIZE_BYTE;
				ESP_LOGE(TAG, " 55 Sending_Task - Sending...%d bytes", send_len);

				rb_read(l_client->send_rb, l_client->Buffers->out_buffer, send_len);
				l_client->State->pending_msg_type = mqtt_get_type(l_client->Buffers->out_buffer);
				l_client->State->pending_msg_id = mqtt_get_id(l_client->Buffers->out_buffer, send_len);
				write(l_client->Broker->Socket, l_client->Buffers->out_buffer, send_len);
				//TOD: Check sending type, to callback publish message
				msg_len -= send_len;
			}
			//invalidate keepalive timer
			l_client->Will->Keepalive = l_client->Will->Keepalive / 2;
		} else {
//			if (l_client->keepalive_tick > 0)
//				l_client->keepalive_tick--;
//			else {
//				l_client->keepalive_tick = l_client->settings->keepalive / 2;
//				l_client->State->outbound_message = mqtt_msg_pingreq(l_client->State->mqtt_connection);
				l_client->State->pending_msg_type = mqtt_get_type(l_client->State->outbound_message->PayloadData);
				l_client->State->pending_msg_id = mqtt_get_id(l_client->State->outbound_message->PayloadData, l_client->State->outbound_message->PayloadLength);
				ESP_LOGE(TAG, " 77 Sending_Task - Sending pingreq");
				write(l_client->Broker->Socket, l_client->State->outbound_message->PayloadData, l_client->State->outbound_message->PayloadLength);
//			}
		}
	}
	ESP_LOGI(TAG, " 82 Sending_Task - Exiting");
	vTaskDelete(NULL);
}

/*
 *
 */
void deliver_publish(Client_t *p_client, uint8_t *p_message, int p_length) {
	PacketInfo_t  event_data;
	int len_read, total_mqtt_len = 0, mqtt_len = 0, mqtt_offset = 0;
	ESP_LOGI(TAG, " 92 Deliver Publish");
	do {
		event_data.PacketTopic_length = p_length;
		event_data.PacketTopic = mqtt_get_publish_topic(p_message, &event_data.PacketTopic_length);
		event_data.PacketPayload_length = p_length;
		event_data.PacketPayload = mqtt_get_publish_data(p_message, &event_data.PacketPayload_length);
		if (total_mqtt_len == 0) {
//			total_mqtt_len = p_client->State->message_length - p_client->State->message_length_read + event_data.PacketPayload_length;
			mqtt_len = event_data.PacketPayload_length;
		} else {
			mqtt_len = len_read;
		}
		event_data.Packet_length = total_mqtt_len;
		event_data.PacketPayload_offset = mqtt_offset;
		event_data.PacketPayload_length = mqtt_len;
		ESP_LOGI(TAG, "107 Data received: %d/%d bytes ", mqtt_len, total_mqtt_len);
		if (p_client->Cb->data_cb) {
			p_client->Cb->data_cb(p_client, &event_data);
		}
		mqtt_offset += mqtt_len;
//		if (p_client->State->message_length_read >= p_client->State->message_length)
//			break;
		len_read = read(p_client->Broker->Socket, p_client->Buffers->in_buffer, CONFIG_MQTT_BUFFER_SIZE_BYTE);
		p_client->State->message_length_read += len_read;
	} while (1);
}

/*
 * This is a high level routine called from the users application.
 * It will read MQTT packets from the transport socket and dispatch/act on the packets.
 */
void mqtt_start_receive_schedule(Client_t *p_client) {
	int read_len;
	uint8_t msg_type;
	uint8_t msg_qos;
	uint16_t msg_id;

	ESP_LOGI(TAG, "128 Receive_Schedule");
	while (1) {
		read_len = read(p_client->Broker->Socket, p_client->Buffers->in_buffer, CONFIG_MQTT_BUFFER_SIZE_BYTE);
		ESP_LOGI(TAG, "131 Receive_Schedule - Read length %d", read_len);
		if (read_len == 0)
			break;
		msg_type = mqtt_get_type(p_client->Buffers->in_buffer);
		msg_qos = mqtt_get_qos(p_client->Buffers->in_buffer);
		msg_id = mqtt_get_id(p_client->Buffers->in_buffer, p_client->Buffers->in_buffer_length);
		ESP_LOGE(TAG, "137 Receive_Schedule - msg_type %d, msg_id: %d, pending_id: %d", msg_type, msg_id, p_client->State->pending_msg_type);
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
//			if (msg_qos == 1)
//				p_client->State->outbound_message = mqtt_msg_puback(p_client->State->mqtt_connection, msg_id);
//			else if (msg_qos == 2)
//				p_client->State->outbound_message = mqtt_msg_pubrec(p_client->State->mqtt_connection, msg_id);
			if (msg_qos == 1 || msg_qos == 2) {
				ESP_LOGI(TAG,"Receive_Schedule - Queue response QoS: %d", msg_qos);
				mqtt_queue(p_client);
				// if (QUEUE_Puts(&client->msgQueue, client->mqtt_state.outbound_message->data, client->mqtt_state.outbound_message->length) == -1) {
				//     mqtt_info("MQTT: Queue full");
				// }
			}
			p_client->State->message_length_read = read_len;
//			p_client->State->message_length = mqtt_get_total_length(p_client->Buffers->in_buffer, p_client->State->message_length_read);
			ESP_LOGI(TAG, "Receive_Schedule - deliver_publish");

			deliver_publish(p_client, p_client->Buffers->in_buffer, p_client->State->message_length_read);
			deliver_publish(p_client, p_client->Buffers->in_buffer, p_client->State->message_length_read);
			break;
		case MQTT_MSG_TYPE_PUBACK:
			if (p_client->State->pending_msg_type == MQTT_MSG_TYPE_PUBLISH && p_client->State->pending_msg_id == msg_id) {
				ESP_LOGI(TAG, "Receive_Schedule - received MQTT_MSG_TYPE_PUBACK, finish QoS1 publish");
			}

			break;
		case MQTT_MSG_TYPE_PUBREC:
//			p_client->State->outbound_message = mqtt_msg_pubrel(p_client->State->mqtt_connection, msg_id);
			mqtt_queue(p_client);
			break;
		case MQTT_MSG_TYPE_PUBREL:
//			p_client->Buffers->outbound_message = mqtt_msg_pubcomp(p_client->State->mqtt_connection, msg_id);
			mqtt_queue(p_client);
			break;
		case MQTT_MSG_TYPE_PUBCOMP:
			if (p_client->State->pending_msg_type == MQTT_MSG_TYPE_PUBLISH && p_client->State->pending_msg_id == msg_id) {
				ESP_LOGI(TAG, "Receive_Schedule - Receive MQTT_MSG_TYPE_PUBCOMP, finish QoS2 publish");
			}
			break;
		case MQTT_MSG_TYPE_PINGREQ:
//			p_client->Buffers->outbound_message = mqtt_msg_pingresp(p_client->State->mqtt_connection);
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
esp_err_t mqtt_destroy(Client_t *p_client) {
	ESP_LOGI(TAG, "Destroy");
	free(p_client->Buffers->in_buffer);
	free(p_client->Buffers->out_buffer);
	free(p_client);
	vTaskDelete(xMqttTask);
	return ESP_ERR_MQTT_OK;
}

/*
 *
 */
esp_err_t mqtt_subscribe(Client_t *p_client, char *topic, uint8_t qos) {
	ESP_LOGI(TAG, "218 Subscribe - Begin");
	mqtt_msg_subscribe(p_client, topic, qos, &p_client->State->pending_msg_id);
	ESP_LOGI(TAG, "220 Subscribe - Queue subscribe, topic\"%s\", id: %d", topic, p_client->State->pending_msg_id);
	mqtt_queue(p_client);
	return ESP_ERR_MQTT_OK;
}

/*
 *
 */
esp_err_t mqtt_publish(Client_t *p_client, char *p_topic, char *p_data, int p_len, int p_qos, int p_retain) {
	ESP_LOGI(TAG, "250 Publish - Begin");
//	p_client->Buffers->outbound_message = mqtt_msg_publish(p_client->State->mqtt_connection, p_topic, p_data, p_len, p_qos, p_retain, &p_client->State->pending_msg_id);
	mqtt_queue(p_client);
//	ESP_LOGI(TAG, "Queuing publish, length: %d, queue size(%d/%d)\r\n", p_client->State->outbound_message->PayloadLength, p_client->send_rb.fill_cnt, p_client->send_rb.size);
	return ESP_ERR_MQTT_OK;
}

/*
 *
 */
void mqtt_stop() {
	ESP_LOGI(TAG, "265 Stop");
}







/*
 * mqtt_connect
 * input - client
 * return 1: success, 0: fail
 */
esp_err_t mqtt_connect(Client_t *p_client) {
	struct timeval tv;
	tv.tv_sec = 10; /* 30 Secs Timeout */
	tv.tv_usec = 0;  // Not init'ing this can cause strange error                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                 s
	ESP_LOGI(TAG, "260 Connect - Begin.");
	print_client(p_client);

	uint32_t  l_socket = p_client->Broker->Socket;
	setsockopt(l_socket, SOL_SOCKET, SO_RCVTIMEO, (char * )&tv, sizeof(struct timeval));
	ESP_LOGI(TAG, "265 Connect - Socket options set");

//	vTaskDelay(10000);
	mqtt_msg_init(p_client->Packet, p_client->Buffers->out_buffer, p_client->Buffers->out_buffer_length);
	mqtt_msg_connect(p_client);
	p_client->Buffers->out_buffer = (uint8_t *)p_client->Packet;


	p_client->State->pending_msg_type = mqtt_get_type(p_client->State->outbound_message->PayloadData);
	p_client->State->pending_msg_id   = mqtt_get_id(p_client->State->outbound_message->PayloadData, p_client->State->outbound_message->PayloadLength);
	ESP_LOGI(TAG, "275 Connect - Sending MQTT CONNECT message, MsgType: %d, MessageId: %04X", p_client->State->pending_msg_type, p_client->State->pending_msg_id);


	print_packet(p_client->Packet);
	int write_len = write(l_socket, &p_client->Packet->PacketBuffer[p_client->Packet->PacketStart], p_client->Packet->PacketPayload_length);
	ESP_LOGI(TAG, "280 Connect - Write Len: %d;  %d ", write_len, p_client->Packet->PacketPayload_length)
	vTaskDelay(100);



	ESP_LOGI(TAG, "285 Connect - Reading MQTT CONNECT response message  Socket: %d;  Len:%d", l_socket, CONFIG_MQTT_BUFFER_SIZE_BYTE);
	int read_len = read(l_socket, p_client->Buffers->in_buffer, CONFIG_MQTT_BUFFER_SIZE_BYTE);
	ESP_LOGI(TAG, "287 Connect - ReadLen: %d;  Buffer:%p", read_len, p_client->Buffers->in_buffer);

	tv.tv_sec = 0; /* No timeout */
	setsockopt(l_socket, SOL_SOCKET, SO_RCVTIMEO, (char *)&tv, sizeof(struct timeval));



	if (read_len < 0) {
		ESP_LOGE(TAG, "295 Connect - Error network response");
		return ESP_ERR_MQTT_FAIL;
	}

	if (mqtt_get_type(p_client->Buffers->in_buffer) != MQTT_MSG_TYPE_CONNACK) {
		ESP_LOGE(TAG, "300 Connect - Invalid MSG_TYPE response: %d, read_len: %d", mqtt_get_type(p_client->Buffers->in_buffer), read_len);
		return ESP_ERR_MQTT_FAIL;
	}
	int connect_rsp_code = mqtt_get_connect_return_code(p_client->Buffers->in_buffer);
	switch (connect_rsp_code) {
		case CONNECTION_ACCEPTED:
			ESP_LOGI(TAG, "306 Connect - Connected");
			return ESP_OK;

		case CONNECTION_REFUSE_PROTOCOL:
		case CONNECTION_REFUSE_SERVER_UNAVAILABLE:
		case CONNECTION_REFUSE_BAD_USERNAME:
		case CONNECTION_REFUSE_NOT_AUTHORIZED:
			ESP_LOGW(TAG, "313 Connect - Connection refused, reason code: %d", connect_rsp_code);
			return ESP_ERR_MQTT_FAIL;
		default:
			ESP_LOGW(TAG, "316 Connect - Connection refused, Unknown reason");
			return ESP_ERR_MQTT_FAIL;
	}
	return ESP_OK;
}


/*
 * A FreeRtos TASK.
 * Network connect to the broker.
 * Create a sending task.
 */
void mqtt_task(void *pvParameters) {
	Client_t *l_client = (Client_t *) pvParameters;
	ESP_LOGI(TAG, "330 Task - Begin.  l_client:%p", l_client);
	while (1) {
		// Establish a network connection
		l_client->Broker->Socket = mqtt_transport_connect(l_client->Broker->Host, l_client->Broker->Port);
		if (mqtt_connect(l_client) != ESP_OK) {
			ESP_LOGE(TAG, "335 Task - Connect Failed");
			continue;
		}
		ESP_LOGI(TAG, "338 Task - Connected to MQTT broker, create sending thread before call connected callback");
		xTaskCreate(&mqtt_sending_task, "mqtt_sending_task", 2048, l_client, CONFIG_MQTT_PRIORITY + 1, &xMqttSendingTask);
		if (l_client->Cb->connected_cb) {
			l_client->Cb->connected_cb(l_client, NULL);
		}
		ESP_LOGI(TAG, "343 Task - mqtt_start_receive_schedule");
		mqtt_start_receive_schedule(l_client);
		close(l_client->Broker->Socket);
		vTaskDelete(xMqttSendingTask);
		vTaskDelay(1000 / portTICK_RATE_MS);
	}
	ESP_LOGW(TAG, "349 Task Exiting")
	mqtt_destroy(NULL);
}




/*
 *
 */
esp_err_t mqtt_start(Client_t *p_client) {
	uint8_t *rb_buf;
	ESP_LOGI(TAG, "340 Start - Begin.");

//	p_client->connect_info.client_id = p_settings->client_id;
//	p_client->connect_info.will_topic = p_settings->lwt_topic;
//	p_client->connect_info.will_message = p_settings->lwt_msg;
//	p_client->connect_info.will_qos = p_settings->lwt_qos;
//	p_client->connect_info.will_retain = p_settings->lwt_retain;
//	p_client->keepalive_tick = p_settings->keepalive / 2;
	p_client->Will->Keepalive = 60;
	p_client->Will->CleanSession = 1;
//	p_client->State.connect_info = &p_client->connect_info;

	/* Create a queue capable of containing 64 unsigned long values. */
	p_client->xSendingQueue = xQueueCreate(64, sizeof(uint32_t));
	ESP_LOGI(TAG, "354 Start - Created SendingQueue:%p", p_client->xSendingQueue);

	rb_buf = (uint8_t*) malloc(CONFIG_MQTT_QUEUE_BUFFER_SIZE_WORD * 4);
	if (rb_buf == 0) {
		ESP_LOGE(TAG, "358 Start - Not Enough Memory");
		return ESP_ERR_MQTT_NO_MEM;
	}
	rb_init(p_client->send_rb, rb_buf, CONFIG_MQTT_QUEUE_BUFFER_SIZE_WORD * 4, 1);
	mqtt_msg_init(p_client->Packet, p_client->Buffers->out_buffer, p_client->Buffers->out_buffer_length);

	ESP_LOGI(TAG, "364 Start - Creating another task - send_rb:%p; Packet:%p", p_client->send_rb, p_client->Packet);
	xTaskCreate(&mqtt_task, "mqtt_task", 2048, p_client, CONFIG_MQTT_PRIORITY, &xMqttTask);

	ESP_LOGI(TAG, "367 Start - Done.");
	return ESP_ERR_MQTT_OK;
}






/*
 * Set up the data structures
 */
esp_err_t mqtt_init(Client_t *p_client) {
	ESP_LOGI(TAG, "380 Init - All");
	print_client(p_client);
	memset(p_client->Buffers->in_buffer, 0, 1024);
	return ESP_ERR_MQTT_OK;
}

// ### END DBK
