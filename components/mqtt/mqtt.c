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
#include "mqtt_packet.h"
#include "mqtt_debug.h"
#include "mqtt_transport.h"
#include "mqtt.h"

static TaskHandle_t xMqttTask = NULL;
static TaskHandle_t xMqttSendingTask = NULL;

static const char *TAG = "Mqtt          ";

uint8_t		g_BufferIn;
uint8_t 	g_BufferOut;
Client_t   	g_ClientPtr;

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

	ESP_LOGI(TAG, " 57 Sending_Task - Begin");
	while (1) {
		// this loop checks for some packet to be sent
		if (xQueueReceive(l_client->SendingQueue, &msg_len, 1000 / portTICK_RATE_MS)) {
			//queue available
			ESP_LOGI(TAG, " 62 Sending_Task - Something to send");
			while (msg_len > 0) {
				send_len = msg_len;
				if (send_len > CONFIG_MQTT_BUFFER_SIZE_BYTE) {
					send_len = CONFIG_MQTT_BUFFER_SIZE_BYTE;
				}
				ESP_LOGE(TAG, " 68 Sending_Task - Sending...%d bytes", send_len);

				rb_read(l_client->Send_rb, l_client->Buffers->out_buffer, send_len);
//				l_client->State->pending_msg_type = mqtt_get_type(l_client->Buffers->out_buffer);
//				l_client->State->pending_msg_id = mqtt_get_id(l_client->Buffers->out_buffer, send_len);
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
				ESP_LOGI(TAG, " 83 Sending_Task - Timed out ");
				l_client->Will->Keepalive_tick = l_client->Will->Keepalive / 2;
				mqtt_build_pingreq_packet(l_client);
				//print_packet(l_client->Packet);
				ESP_LOGI(TAG, " 86 Sending_Task ");
//				l_client->Buffers->out_buffer = l_client->Packet;
//				l_client->State->pending_msg_type = mqtt_get_type(l_client->State->outbound_message->PayloadData);
//				l_client->State->pending_msg_id = mqtt_get_id(l_client->State->outbound_message->PayloadData, l_client->State->outbound_message->PayloadLength);
				ESP_LOGI(TAG, " 89 Sending_Task - Sending pingreq");
				mqtt_transport_write(l_client->Broker->Socket, l_client->Packet);
			}
		}
	}
	vQueueDelete(l_client->SendingQueue);
	ESP_LOGI(TAG, " 95 Sending_Task - Exiting");
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
//		event_data.PacketTopic = mqtt_get_publish_topic(p_message, &event_data.PacketTopic_length);
		event_data.PacketPayload_length = p_length;
//		event_data.PacketPayload = mqtt_get_publish_data(p_message, &event_data.PacketPayload_length);
		if (total_mqtt_len == 0) {
			total_mqtt_len = p_client->State->message_length - p_client->State->message_length_read + event_data.PacketPayload_length;
			mqtt_len = event_data.PacketPayload_length;
		} else {
			mqtt_len = len_read;
		}
		event_data.Packet_length = total_mqtt_len;
//		event_data.PacketPayload_offset = mqtt_offset;
		event_data.PacketPayload_length = mqtt_len;
		ESP_LOGI(TAG, "107 Data received: %d/%d bytes ", mqtt_len, total_mqtt_len);
		if (p_client->Cb->data_cb) {
			p_client->Cb->data_cb(p_client, &event_data);
		}
		mqtt_offset += mqtt_len;
		if (p_client->State->message_length_read >= p_client->State->message_length)
			break;
		len_read = mqtt_transport_read(p_client->Broker->Socket, p_client->Buffers->in_buffer);
		p_client->State->message_length_read += len_read;
	} while (1);
}

/*
 * This is a high level routine called from                                                                                                                                          the users application.
 * It will read MQTT packets from the transport socket and dispatch/act on the packets.
 */
void mqtt_start_receive_schedule(Client_t *p_client) {
	int l_read_len;
	uint8_t l_msg_type;
	uint8_t l_msg_qos;
//	uint16_t msg_id;

	ESP_LOGI(TAG, "128 Receive_Schedule");
	while (1) {
		l_read_len = mqtt_transport_read(p_client->Broker->Socket, p_client->Buffers->in_buffer);
		ESP_LOGI(TAG, "131 Receive_Schedule - Read length %d\n", l_read_len);
		if (l_read_len == 0) {
			break;
		}
		l_msg_type = mqtt_get_packet_type(p_client->Buffers->in_buffer);
		l_msg_qos = mqtt_get_packet_qos(p_client->Buffers->in_buffer);
//		msg_id = mqtt_get_packet_id(p_client->Buffers->in_buffer, p_client->Buffers->in_buffer_length);
//		ESP_LOGE(TAG, "137 Receive_Schedule - msg_type:%d;  msg_id:%d;  pending_id:%d", msg_type, msg_id, p_client->State->pending_msg_type);
		switch (l_msg_type) {
		case MQTT_CONTROL_PACKET_TYPE_SUBACK:
//			if (p_client->State->pending_msg_type == MQTT_MSG_TYPE_SUBSCRIBE && p_client->State->pending_msg_id == msg_id) {
//				ESP_LOGE(TAG, "Receive_Schedule - Subscribe successful");
//				if (p_client->Cb->subscribe_cb) {
//					p_client->Cb->subscribe_cb(p_client, NULL);
//				}
//			}
			break;
		case MQTT_CONTROL_PACKET_TYPE_UNSUBACK:
			ESP_LOGI(TAG, "Receive_Schedule - UnSubAck");
//			if (p_client->State->pending_msg_type == MQTT_MSG_TYPE_UNSUBSCRIBE && p_client->State->pending_msg_id == msg_id) {
//				ESP_LOGI(TAG, "Receive_Schedule - UnSubscribe successful");
//		}
			break;
		case MQTT_CONTROL_PACKET_TYPE_PUBLISH:
			ESP_LOGI(TAG, "Receive_Schedule - Publish");
//			if (msg_qos == 1) {
//				// mqtt_msg_puback(p_client->State->Connection, msg_id);
//				mqtt_msg_puback(p_client->Packet, msg_id);
//				p_client->Buffers->out_buffer = p_client->State->Connection;
//			} else if (msg_qos == 2) {
//				mqtt_msg_pubrec(p_client->State->Connection, msg_id);
//				p_client->State->outbound_message = p_client->State->Connection;
//				;
//			}
//			if (msg_qos == 1 || msg_qos == 2) {
//				ESP_LOGI(TAG,"Receive_Schedule - Queue response QoS: %d", msg_qos);
//				mqtt_queue(p_client);
//			}
//			p_client->State->message_length_read = read_len;
//			p_client->State->message_length = mqtt_get_total_length(p_client->Buffers->in_buffer, p_client->State->message_length_read);
//			ESP_LOGI(TAG, "Receive_Schedule - deliver_publish");

//			deliver_publish(p_client, p_client->Buffers->in_buffer, p_client->State->message_length_read);
//			deliver_publish(p_client, p_client->Buffers->in_buffer, p_client->State->message_length_read);
 			break;
		case MQTT_CONTROL_PACKET_TYPE_PUBACK:
			ESP_LOGI(TAG, "Receive_Schedule - PubAck");
//			if (p_client->State->pending_msg_type == MQTT_MSG_TYPE_PUBLISH && p_client->State->pending_msg_id == msg_id) {
//				ESP_LOGI(TAG, "Receive_Schedule - received MQTT_MSG_TYPE_PUBACK, finish QoS1 publish");
//			}
			break;
		case MQTT_CONTROL_PACKET_TYPE_PUBREC:
			ESP_LOGI(TAG, "Receive_Schedule - PubRec");
//			mqtt_msg_pubrel(p_client->State->Connection, msg_id);
//			p_client->Buffers->out_buffer = p_client->State->Connection;
//			mqtt_queue(p_client);
			break;
		case MQTT_CONTROL_PACKET_TYPE_PUBREL:
			ESP_LOGI(TAG, "Receive_Schedule - PubRel");
//			mqtt_msg_pubcomp(p_client->State->Connection, msg_id);
//			p_client->Buffers->out_buffer = p_client->State->Connection;
//			mqtt_queue(p_client);
			break;
		case MQTT_CONTROL_PACKET_TYPE_PUBCOMP:
			ESP_LOGI(TAG, "Receive_Schedule - PubComp");
//			if (p_client->State->pending_msg_type == MQTT_MSG_TYPE_PUBLISH && p_client->State->pending_msg_id == msg_id) {
//				ESP_LOGI(TAG, "Receive_Schedule - Receive MQTT_MSG_TYPE_PUBCOMP, finish QoS2 publish");
//			}
			break;
		case MQTT_CONTROL_PACKET_TYPE_PINGREQ:
			ESP_LOGI(TAG, "Receive_Schedule - PingReq");
//			mqtt_msg_pingresp(p_client->State->Connection);
//			p_client->Buffers->out_buffer = p_client->State->Connection;
//			mqtt_queue(p_client);
			break;
		case MQTT_CONTROL_PACKET_TYPE_PINGRESP:
			ESP_LOGI(TAG, "MQTT_MSG_TYPE_PINGRESP");
			break;
		}
	}
	ESP_LOGI(TAG, "Receive_Schedule - network disconnected");
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
	return ESP_OK;
}

/**
 * We only support one topic/qos per subscription.
 *
 */
esp_err_t mqtt_subscribe(Client_t *p_client, char *p_topic, uint8_t p_qos) {
	ESP_LOGI(TAG, "240 Subscribe - Begin");
	mqtt_build_subscribe_packet(p_client, p_topic, p_qos, &p_client->State->pending_msg_id);
	ESP_LOGI(TAG, "220 Subscribe - Queue subscribe, topic\"%s\", id: %d", p_topic, p_client->State->pending_msg_id);
	mqtt_queue(p_client);
	return ESP_OK;
}




/*
 *
 */
esp_err_t mqtt_publish(Client_t *p_client, char *p_topic, char *p_data, int p_len, int p_qos, int p_retain) {
	ESP_LOGI(TAG, "240 Publish - Begin");
//	p_client->Buffers->out_buffer = mqtt_msg_publish(p_client->State->Connection, p_topic, p_data, p_len, p_qos, p_retain, &p_client->State->pending_msg_id);
	mqtt_queue(p_client);
	ESP_LOGI(TAG, "Queuing publish, length: %d, queue size(%d/%d)\r\n",
			p_client->State->outbound_message->PayloadLength, p_client->Send_rb->fill_cnt, p_client->Send_rb->size);
	return ESP_OK;
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
 * return -
 */
esp_err_t mqtt_connect(Client_t *p_client) {
	int l_write_length;
	int l_read_length;
	int l_connection_response_code;

	ESP_LOGI(TAG, "280 Connect - Begin.");
	mqtt_transport_set_timeout(p_client->Broker->Socket, 10);
	ESP_LOGI(TAG, "282 Connect - Socket options set");

	mqtt_build_connect_packet(p_client);
	ESP_LOGI(TAG, "288 Connect - Sending MQTT CONNECT message, MsgType: %d, MessageId: %04X", p_client->State->pending_msg_type, p_client->State->pending_msg_id);

//	print_packet(p_client->Packet);
	l_write_length = mqtt_transport_write(p_client->Broker->Socket, p_client->Packet);
	ESP_LOGI(TAG, "292 Connect - Write Len: %d;  %d ", l_write_length, p_client->Packet->PacketPayload_length)

	l_read_length = mqtt_transport_read(p_client->Broker->Socket, p_client->Buffers->in_buffer);
	ESP_LOGI(TAG, "289 Connect - ReadLen: %d;  Buffer:%p", l_read_length, p_client->Buffers->in_buffer);
	print_buffer(p_client->Buffers->in_buffer, l_read_length);

	mqtt_transport_set_timeout(p_client->Broker->Socket, 0);

	if (l_read_length < 0) {
		ESP_LOGE(TAG, "304 Connect - Error network response");
		return ESP_FAIL;
	}

	if (mqtt_get_packet_type(p_client->Buffers->in_buffer) != MQTT_CONTROL_PACKET_TYPE_CONNACK) {
		ESP_LOGE(TAG, "309 Connect - Invalid MSG_TYPE response: %d, read_len: %d", mqtt_get_packet_type(p_client->Buffers->in_buffer), l_read_length);
		return ESP_FAIL;
	}
	l_connection_response_code = mqtt_get_packet_connect_return_code(p_client->Buffers->in_buffer);
	switch (l_connection_response_code) {
		case CONNECTION_ACCEPTED:
			ESP_LOGI(TAG, "315 Connect - Connected");
			// Subscribe
			return ESP_OK;

		case CONNECTION_REFUSE_PROTOCOL:
		case CONNECTION_REFUSE_SERVER_UNAVAILABLE:
		case CONNECTION_REFUSE_BAD_USERNAME:
		case CONNECTION_REFUSE_NOT_AUTHORIZED:
			ESP_LOGW(TAG, "313 Connect - Connection refused, reason code: %d", l_connection_response_code);
			return ESP_FAIL;
		default:
			ESP_LOGW(TAG, "316 Connect - Connection refused, Unknown reason");
			return ESP_FAIL;
	}
	return ESP_OK;
}


/**
 * A FreeRtos TASK.
 * Network connect to the broker.
 * Create a sending task.
 */
void Mqtt_transport_task(void *pvParameters) {
	Client_t *l_client = (Client_t *) pvParameters;

	ESP_LOGI(TAG, "340 TransportTask - Begin.  l_client:%p;  pvParams:%p", l_client, pvParameters);
	while (1) {
		// Establish a transport connection
		l_client->Broker->Socket = mqtt_transport_connect(l_client->Broker->Host, l_client->Broker->Port);
		if (mqtt_connect(l_client) != ESP_OK) {
			ESP_LOGE(TAG, "340 TransportTask - Connect Failed");
			continue;
		}
		ESP_LOGI(TAG, "340 TransportTask - Connected to MQTT broker, create sending thread before call connected callback");
		xTaskCreate(&mqtt_sending_task, "mqtt_sending_task", 2048, l_client, 6, &xMqttSendingTask);
		if (l_client->Cb->connected_cb) {
			l_client->Cb->connected_cb(l_client, NULL);
		}
		ESP_LOGI(TAG, "340 TransportTask - mqtt_start_receive_schedule");
		mqtt_start_receive_schedule(l_client);
		close(l_client->Broker->Socket);
		vTaskDelete(xMqttSendingTask);
		vTaskDelay(1000 / portTICK_RATE_MS);
	}
	ESP_LOGW(TAG, "340 TransportTask - Exiting")
	mqtt_destroy(NULL);
}

/*
 *
 */
esp_err_t Mqtt_start(Client_t *p_client, char* p_will_topic, char* p_will_message) {
//	ESP_LOGI(TAG, "381 Start - ClientPtr:%p", p_client);
	ESP_LOGI(TAG, "382 Start - Creating transport task");
	xTaskCreate(&Mqtt_transport_task, "Mqtt_transport_task", 2048, p_client, 5, &xMqttTask);
	ESP_LOGI(TAG, "385 Start - Done.\n");
	return ESP_OK;
}









/*
 * Set up the Last Will and Testament structure.
 */
esp_err_t Mqtt_init_will(Client_t *p_client) {
	int l_len = 0;
	p_client->Will = calloc(1, sizeof(Will_t));
	ESP_LOGI(TAG, "415 Setup Will - Begin  Will:%p", p_client->Will);
	p_client->Will->WillTopic = calloc(32, sizeof(uint8_t));
	l_len = snprintf(p_client->Will->WillTopic, 32, "pyhouse/%s/lwt", CONFIG_PYHOUSE_HOUSE_NAME);
	if (l_len < 0) {
		ESP_LOGE(TAG, "419 Will topic too long");
		return ESP_ERR_INVALID_SIZE;
	}
	p_client->Will->WillMessage = calloc(32, sizeof(uint8_t));
	l_len = snprintf(p_client->Will->WillMessage, 32, "%s Offline", CONFIG_MQTT_CLIENT_ID);
	if (l_len < 0) {
		ESP_LOGE(TAG, "425 Will message too long");
		return ESP_ERR_INVALID_SIZE;
	}
	p_client->Will->WillQos = 0;
	p_client->Will->WillRetain = 0;
	p_client->Will->CleanSession = 0;
	p_client->Will->Keepalive = 60;
	p_client->Will->Keepalive_tick = 30;
	ESP_LOGI(TAG, "433 Will has been set up. ClientPtr:%p;  Will:%p", p_client, p_client->Will);
	return ESP_OK;
}

esp_err_t Mqtt_init_state(Client_t *p_client) {
	ESP_LOGI(TAG, "439 InitState - ClientPtr:%p", p_client);
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
//	p_client->Send_rb = l_rb_buf;
	rb_init(p_client->Send_rb,  l_rb_buf, CONFIG_MQTT_QUEUE_BUFFER_SIZE_WORD * 4, 1);
	ESP_LOGI(TAG, "467 InitRingBuffer - clientPtr:%p;  Created RingBuffer:%p", p_client, p_client->Send_rb);
	return ESP_OK;
}

esp_err_t Mqtt_init_sending_queue(Client_t *p_client) {
	p_client->SendingQueue = xQueueCreate(64, sizeof(uint32_t));
	ESP_LOGI(TAG, "459 InitSendingQueue - All");
	if (p_client->SendingQueue == NULL){
		ESP_LOGE(TAG, "459 Start Failed to create a Packet Sending Queue");
		return ESP_ERR_NO_MEM;
	}
	ESP_LOGI(TAG, "462 InitSendingQueue - ClientPtr:%p;  Created SendingQueue:%p", p_client, p_client->SendingQueue);
	return ESP_OK;
}

esp_err_t Mqtt_init_packet(Client_t *p_client) {
	ESP_LOGI(TAG, "470 InitPacket - ClientPtr:%p", p_client);
	p_client->Packet->PacketFixedHeader = calloc(5, sizeof(uint8_t));
	return ESP_OK;
}

esp_err_t Mqtt_init_callback(Client_t *p_client) {
	ESP_LOGI(TAG, "480 InitCallback - All");
	return ESP_OK;
}

esp_err_t Mqtt_init_buffers(Client_t *p_client) {
	p_client->Buffers->in_buffer_length = 1024;
	p_client->Buffers->in_buffer = calloc(1024, sizeof(uint8_t));
	p_client->Buffers->out_buffer_length = 1024;
	p_client->Buffers->out_buffer = calloc(1024, sizeof(uint8_t));
//	ESP_LOGI(TAG, "485 InitBuffers - ClientPtr:%p;  BufferPtr:%p", p_client, p_client->Buffers);
	return ESP_OK;
}

esp_err_t Mqtt_init_broker(Client_t *p_client) {
	snprintf(p_client->Broker->Host, 64, "%s", CONFIG_MQTT_HOST_NAME);
	p_client->Broker->Port = CONFIG_MQTT_HOST_PORT;
	snprintf(p_client->Broker->Username, 64, "%s", CONFIG_MQTT_HOST_USERNAME);
	snprintf(p_client->Broker->Password, 64, "%s", CONFIG_MQTT_HOST_PASSWORD);
	snprintf(p_client->Broker->ClientId, 64, "%s", CONFIG_MQTT_CLIENT_ID);
//	ESP_LOGI(TAG, "495 InitBroker - ClientPtr:%p;  BrokerPtr:%p", p_client, p_client->Broker);
	return ESP_OK;
}

/*
 * Set up the data structures, Allocate memory.
 */
esp_err_t Mqtt_init(Client_t *p_client) {

	ESP_LOGI(TAG, "500 Init - Begin  %p", p_client);
	p_client->Broker	= calloc(1, sizeof(BrokerConfig_t));
	p_client->Buffers	= calloc(1, sizeof(Buffers_t));
	p_client->Cb		= calloc(1, sizeof(Callback_t));
	p_client->Packet	= calloc(1, sizeof(PacketInfo_t));
	p_client->State		= calloc(1, sizeof(State_t));
	p_client->Will 		= calloc(1, sizeof(Will_t));
	Mqtt_init_broker(p_client);
	Mqtt_init_buffers(p_client);
	Mqtt_init_callback(p_client);
	Mqtt_init_packet(p_client);
	Mqtt_init_sending_queue(p_client);
//	Mqtt_init_ring_buffer(p_client);
	Mqtt_init_state(p_client);
	Mqtt_init_will(p_client);
	print_client(p_client);
	ESP_LOGI(TAG, "532 Init - Sub allocs done.");
	return ESP_OK;
}

// ### END DBK
