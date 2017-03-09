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

uint8_t		g_BufferIn;
uint8_t 	g_BufferOut;

/*
 * Write the payload
 */
static void mqtt_queue(Client_t *p_client) {
// TOD: detect buffer full (ringbuf and queue)
	ESP_LOGI(TAG, " 38 Mqtt_Queue - All");
	print_buffer(p_client->State->outbound_message->PayloadData, p_client->State->outbound_message->PayloadLength);
	rb_write(p_client->Send_rb, p_client->State->outbound_message->PayloadData, p_client->State->outbound_message->PayloadLength);
	xQueueSend(p_client->SendingQueue, &p_client->State->outbound_message->PayloadLength, 0);
	ESP_LOGI(TAG, " 42 Mqtt_Queue - All");
}



/*
 * A FreeRtos TASK for sending packets.
 */
void mqtt_sending_task(void *pvParameters) {
	Client_t *l_client = (Client_t *) pvParameters;
	uint32_t msg_len, send_len;

	ESP_LOGI(TAG, " 47 Sending_Task - Begin");
	while (1) {
		// this loop checks for some packet to be sent
		if (xQueueReceive(l_client->SendingQueue, &msg_len, 1000 / portTICK_RATE_MS)) {
			//queue available
			while (msg_len > 0) {
				send_len = msg_len;
				if (send_len > CONFIG_MQTT_BUFFER_SIZE_BYTE) {
					send_len = CONFIG_MQTT_BUFFER_SIZE_BYTE;
				}
				ESP_LOGE(TAG, " 55 Sending_Task - Sending...%d bytes", send_len);

				rb_read(l_client->Send_rb, l_client->Buffers->out_buffer, send_len);
				l_client->State->pending_msg_type = mqtt_get_type(l_client->Buffers->out_buffer);
				l_client->State->pending_msg_id = mqtt_get_id(l_client->Buffers->out_buffer, send_len);
				write(l_client->Broker->Socket, l_client->Buffers->out_buffer, send_len);
				//TOD: Check sending type, to callback publish message
				msg_len -= send_len;
			}
			//invalidate keep alive timer
			l_client->Will->Keepalive_tick = l_client->Will->Keepalive / 2;
		} else {
			if (l_client->Will->Keepalive_tick > 0)
				l_client->Will->Keepalive_tick--;
			else {
				l_client->Will->Keepalive_tick = l_client->Will->Keepalive / 2;
				mqtt_msg_pingreq(l_client->Packet);
// ??				l_client->Buffers->out_buffer = l_client->Packet;
				l_client->State->pending_msg_type = mqtt_get_type(l_client->State->outbound_message->PayloadData);
				l_client->State->pending_msg_id = mqtt_get_id(l_client->State->outbound_message->PayloadData, l_client->State->outbound_message->PayloadLength);
				ESP_LOGE(TAG, " 77 Sending_Task - Sending pingreq");
				write(l_client->Broker->Socket, l_client->State->outbound_message->PayloadData, l_client->State->outbound_message->PayloadLength);
			}
		}
	}
	vQueueDelete(l_client->SendingQueue);
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
			total_mqtt_len = p_client->State->message_length - p_client->State->message_length_read + event_data.PacketPayload_length;
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
		if (p_client->State->message_length_read >= p_client->State->message_length)
			break;
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
			if (msg_qos == 1) {
				mqtt_msg_puback(p_client->State->Connection, msg_id);
				p_client->Buffers->out_buffer = p_client->State->Connection;
			} else if (msg_qos == 2) {
				mqtt_msg_pubrec(p_client->State->Connection, msg_id);
				p_client->State->outbound_message = p_client->State->Connection;
			}
			if (msg_qos == 1 || msg_qos == 2) {
				ESP_LOGI(TAG,"Receive_Schedule - Queue response QoS: %d", msg_qos);
				mqtt_queue(p_client);
			}
			p_client->State->message_length_read = read_len;
			p_client->State->message_length = mqtt_get_total_length(p_client->Buffers->in_buffer, p_client->State->message_length_read);
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
			mqtt_msg_pubrel(p_client->State->Connection, msg_id);
			p_client->Buffers->out_buffer = p_client->State->Connection;
			mqtt_queue(p_client);
			break;
		case MQTT_MSG_TYPE_PUBREL:
			mqtt_msg_pubcomp(p_client->State->Connection, msg_id);
			p_client->Buffers->out_buffer = p_client->State->Connection;
			mqtt_queue(p_client);
			break;
		case MQTT_MSG_TYPE_PUBCOMP:
			if (p_client->State->pending_msg_type == MQTT_MSG_TYPE_PUBLISH && p_client->State->pending_msg_id == msg_id) {
				ESP_LOGI(TAG, "Receive_Schedule - Receive MQTT_MSG_TYPE_PUBCOMP, finish QoS2 publish");
			}
			break;
		case MQTT_MSG_TYPE_PINGREQ:
			mqtt_msg_pingresp(p_client->State->Connection);
			p_client->Buffers->out_buffer = p_client->State->Connection;
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
	ESP_LOGI(TAG, "240 Publish - Begin");
	p_client->Buffers->out_buffer = mqtt_msg_publish(p_client->State->Connection, p_topic, p_data, p_len, p_qos, p_retain, &p_client->State->pending_msg_id);
	mqtt_queue(p_client);
	ESP_LOGI(TAG, "Queuing publish, length: %d, queue size(%d/%d)\r\n",
			p_client->State->outbound_message->PayloadLength, p_client->Send_rb->fill_cnt, p_client->Send_rb->size);
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
	print_buffer(p_client->Buffers->in_buffer, read_len);

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
void Mqtt_transport_task(void *pvParameters) {
	Client_t *l_client = (Client_t *) pvParameters;
	ESP_LOGI(TAG, "346 Task - Begin.  l_client:%p", l_client);
	while (1) {
		// Establish a transport connection
		l_client->Broker->Socket = mqtt_transport_connect(l_client->Broker->Host, l_client->Broker->Port);
		if (mqtt_connect(l_client) != ESP_OK) {
			ESP_LOGE(TAG, "335 Task - Connect Failed");
			continue;
		}
		ESP_LOGI(TAG, "338 Task - Connected to MQTT broker, create sending thread before call connected callback");
		xTaskCreate(&mqtt_sending_task, "mqtt_sending_task", 2048, l_client, 6, &xMqttSendingTask);
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
esp_err_t Mqtt_start(Client_t *p_client, char* p_will_topic, char* p_will_message) {
	ESP_LOGI(TAG, "380 Start - Begin.");

	ESP_LOGI(TAG, "382 Start - Creating transport task");
	xTaskCreate(&Mqtt_transport_task, "Mqtt_transport_task", 2048, p_client, 5, &xMqttTask);

	ESP_LOGI(TAG, "385 Start - Done.");
	return ESP_ERR_MQTT_OK;
}









/*
 * Set up the Last Will and Testament structure.
 */
esp_err_t Mqtt_init_will(Will_t *p_will) {
	int l_len = 0;
	p_will = calloc(1, sizeof(Will_t));
	ESP_LOGI(TAG, "415 Setup Will - Begin  Will:%p", p_will);
	p_will->WillTopic = calloc(32, sizeof(uint8_t));
	l_len = snprintf(p_will->WillTopic, 32, "pyhouse/%s/lwt", CONFIG_PYHOUSE_HOUSE_NAME);
	if (l_len < 0) {
		ESP_LOGE(TAG, "419 Will topic too long");
		return ESP_ERR_INVALID_SIZE;
	}
	p_will->WillMessage = calloc(32, sizeof(uint8_t));
	l_len = snprintf(p_will->WillMessage, 32, "%s Offline", CONFIG_MQTT_CLIENT_ID);
	if (l_len < 0) {
		ESP_LOGE(TAG, "425 Will message too long");
		return ESP_ERR_INVALID_SIZE;
	}
	p_will->WillQos = 0;
	p_will->WillRetain = 0;
	p_will->CleanSession = 0;
	p_will->Keepalive = 60;
	p_will->Keepalive_tick = 30;
	ESP_LOGI(TAG, "433 Will has been set up. Will:%p", p_will);
	return ESP_OK;
}


esp_err_t Mqtt_init_state(Client_t *p_client) {
	ESP_LOGI(TAG, "439 InitState - All");
	return ESP_OK;
}

esp_err_t Mqtt_init_ring_buffer(Client_t *p_client) {
	uint8_t *l_rb_buf;
	ESP_LOGI(TAG, "445 InitRingBuff - All");
	l_rb_buf = (uint8_t*) malloc(CONFIG_MQTT_QUEUE_BUFFER_SIZE_WORD * 4);
	if (l_rb_buf == 0) {
		ESP_LOGE(TAG, "442 Start - Not Enough Memory");
		return ESP_ERR_NO_MEM;
	}
	p_client->Send_rb = l_rb_buf;
	rb_init(p_client->Send_rb, l_rb_buf, CONFIG_MQTT_QUEUE_BUFFER_SIZE_WORD * 4, 1);
	ESP_LOGI(TAG, "467 InitRingBuffer - Created RingBuffer:%p", p_client->Send_rb);
	return ESP_OK;
}

esp_err_t Mqtt_init_sending_queue(Client_t *p_client) {
	p_client->SendingQueue = xQueueCreate(64, sizeof(uint32_t));
	ESP_LOGI(TAG, "459 InitSendingQueue - All");
	if (p_client->SendingQueue == NULL){
		ESP_LOGE(TAG, "459 Start Failed to create a Packet Sending Queue");
		return ESP_ERR_NO_MEM;
	}
	ESP_LOGI(TAG, "462 InitSendingQueue - Created SendingQueue:%p", p_client->SendingQueue);
	return ESP_OK;
}


esp_err_t Mqtt_init_packet(Client_t *p_client) {
	ESP_LOGI(TAG, "470 InitPacket - All");
	mqtt_msg_init(p_client->Packet, p_client->Buffers->out_buffer, p_client->Buffers->out_buffer_length);
	return ESP_OK;
}





esp_err_t Mqtt_init_callback(Client_t *p_client) {
	ESP_LOGI(TAG, "480 InitCallback - All");
	return ESP_OK;
}

esp_err_t Mqtt_init_buffers(Buffers_t *p_buffers) {
	ESP_LOGI(TAG, "485 InitBuffers - All");
	p_buffers->in_buffer_length = 1024;
	p_buffers->in_buffer = calloc(1024, sizeof(uint8_t));
	p_buffers->out_buffer_length = 1024;
	p_buffers->out_buffer = calloc(1024, sizeof(uint8_t));
	return ESP_OK;
}


esp_err_t Mqtt_init_broker(BrokerConfig_t *p_broker) {
	ESP_LOGI(TAG, "495 InitBroker - All");
	snprintf(p_broker->Host, 64, "%s", CONFIG_MQTT_HOST_NAME);
	p_broker->Port = CONFIG_MQTT_HOST_PORT;
	snprintf(p_broker->Username, 64, "%s", CONFIG_MQTT_HOST_USERNAME);
	snprintf(p_broker->Password, 64, "%s", CONFIG_MQTT_HOST_PASSWORD);
	snprintf(p_broker->ClientId, 64, "%s", CONFIG_MQTT_CLIENT_ID);
	return ESP_OK;
}





/*
 * Set up the data structures, Allocate memory.
 */
esp_err_t Mqtt_init(Client_t *p_client) {
	uint8_t *rb_buf;
	int l_len;

	ESP_LOGI(TAG, "515 Init - All");
	p_client           = calloc(1, sizeof(Client_t));
	p_client->Broker   = calloc(1, sizeof(BrokerConfig_t));
	p_client->Buffers  = calloc(1, sizeof(Buffers_t));
	p_client->Cb       = calloc(1, sizeof(Callback_t));
	p_client->Packet   = calloc(1, sizeof(PacketInfo_t));
	p_client->State    = calloc(1, sizeof(State_t));
	ESP_LOGI(TAG, "522 Init - Main allocated");

	Mqtt_init_broker(p_client->Broker);
	Mqtt_init_buffers(p_client->Buffers);
	Mqtt_init_callback(p_client);
	Mqtt_init_packet(p_client);
	Mqtt_init_sending_queue(p_client);
	Mqtt_init_ring_buffer(p_client);
	Mqtt_init_state(p_client);
	Mqtt_init_will(p_client->Will);
	ESP_LOGI(TAG, "532 Init - Sub allocs done.");

	print_client(p_client);
	return ESP_ERR_MQTT_OK;
}

// ### END DBK
