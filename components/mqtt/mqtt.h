#ifndef _MQTT_H_
#define _MQTT_H_

#include <stdint.h>
#include <string.h>

#include "esp_err.h"

#include "mqtt_config.h"
#include "mqtt_msg.h"
#include "ringbuf.h"

#include "sdkconfig.h"


#define ESP_ERR_MQTT_OK          	ESP_OK                    /*!< No error */
#define ESP_ERR_MQTT_FAIL        	ESP_FAIL                  /*!< General fail code */
#define ESP_ERR_MQTT_NO_MEM      	ESP_ERR_NO_MEM            /*!< Out of memory */
#define ESP_ERR_MQTT_ARG         	ESP_ERR_INVALID_ARG       /*!< Invalid argument */
#define ESP_ERR_MQTT_NOT_SUPPORTED 	ESP_ERR_NOT_SUPPORTED     /*!< Indicates that API is not supported yet */


typedef void (*mqtt_callback)(void *, void *);


/**
 * All Callbacks for the client
 */
typedef struct Callback {
	mqtt_callback   connected_cb;
	mqtt_callback   disconnected_cb;
	mqtt_callback   reconnect_cb;
	mqtt_callback   subscribe_cb;
	mqtt_callback   publish_cb;
	mqtt_callback   data_cb;
} Callback;

/**
 * LastWillAndTestament for the client
 */
typedef struct LastWill {
	char				Will_topic[32];
	char				Will_msg[32];
	uint32_t			Will_qos;
	uint32_t			Will_retain;
	uint32_t			Clean_session;
	uint32_t			Keep_alive;
} LastWill;

/**
 * Broker Information
 */
typedef struct BrokerConfig {
	char				Host[64];
	uint32_t			Port;
	char				Client_id[64];
	char				Username[32];
	char				Password[32];
} BrokerConfig;

/**
 * Message Event Data
 */
typedef struct MessageInfo {
	uint8_t				MessageType;
	const char			*MessageTopic;
	uint16_t			MessageTopic_length;
	const char			*MessagePayload;
	uint16_t			MessagePayload_offset;
	uint16_t			MessagePayload_length;
	uint16_t			Message_total_length;
} MessageInfo;

/**
 * Buffers runtime stuff
 */
typedef struct Buffer
	uint8_t				*in_buffer;
	uint8_t				*out_buffer;
	int					in_buffer_length;
	int					out_buffer_length;
} Buffer;

/**
 * State runtime stuff
 */
typedef struct State {
//	uint16_t			message_length;
	uint16_t			message_length_read;
	mqtt_message_t		*outbound_message;
//	mqtt_connection_t	*mqtt_connection;
	uint16_t			pending_msg_id;
	int					pending_msg_type;
	int					pending_publish_qos;
} State;

/*
 * Client
 */
typedef struct Client {
	State				*State;
	BrokerConfig		*Broker;
	LastWill			*Will;
	Callback			*Cb;
//	mqtt_connect_info_t	Connection_info;
	QueueHandle_t		xSendingQueue;
	RINGBUF				send_rb;
	uint32_t			Socket;
} Client;


// New Signatures

/**
 * @brief Initialize the esp Mqtt subsystem.
 */
esp_err_t mqtt_init(Client*);
void mqtt_task(void *);
esp_err_t mqtt_start(Client*);
esp_err_t mqtt_connect(Client*);
esp_err_t mqtt_detroy(Client*);
esp_err_t mqtt_subscribe(Client*, char*, uint8_t);
esp_err_t mqtt_publish(Client *, char *, char *, int, int, int);

#endif  /* __MQTT_H__ */

// ### END DBK
