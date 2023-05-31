/*
 * Copyright (c) 2023 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */
#include "app_mqtt.h"
#include <stdio.h>
#include <string.h>
#include <zephyr/random/rand32.h>
#include <zephyr/net/socket.h>
#include <zephyr/net/mqtt.h>
#include <zephyr/sys/util.h>

#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(APP_MQTT, LOG_LEVEL_DBG);

/* Buffers for MQTT client. */
static uint8_t rx_buffer[CONFIG_MQTT_MESSAGE_BUFFER_SIZE];
static uint8_t tx_buffer[CONFIG_MQTT_MESSAGE_BUFFER_SIZE];
static uint8_t payload_buf[CONFIG_MQTT_PAYLOAD_BUFFER_SIZE];
static uint8_t topic_buf[CONFIG_MQTT_PAYLOAD_BUFFER_SIZE];

/* MQTT Broker details. */
static struct sockaddr_storage broker;

/* The mqtt client struct */
static struct mqtt_client client;

/* File descriptor */
static struct pollfd fds;

static struct app_mqtt_callbacks_t registered_callbacks = {0};

/**@brief Function to get the payload of recived data.
 */
static int get_received_payload(struct mqtt_client *c, size_t length)
{
	int ret;
	int err = 0;

	/* Return an error if the payload is larger than the payload buffer.
	 * Note: To allow new messages, we have to read the payload before returning.
	 */
	if (length > sizeof(payload_buf)) {
		err = -EMSGSIZE;
	}

	/* Truncate payload until it fits in the payload buffer. */
	while (length > sizeof(payload_buf)) {
		ret = mqtt_read_publish_payload_blocking(
				c, payload_buf, (length - sizeof(payload_buf)));
		if (ret == 0) {
			return -EIO;
		} else if (ret < 0) {
			return ret;
		}

		length -= ret;
	}

	ret = mqtt_readall_publish_payload(c, payload_buf, length);
	if (ret) {
		return ret;
	}

	return err;
}

/**@brief Function to subscribe to the configured topic
 */
static int subscribe(struct mqtt_client *const c)
{
	struct mqtt_topic subscribe_topics[] = {
		{
			.topic = {
				.utf8 = CONFIG_MQTT_SUB_TOPIC,
				.size = strlen(CONFIG_MQTT_SUB_TOPIC)
			},
			.qos = MQTT_QOS_1_AT_LEAST_ONCE
		},
	};

	const struct mqtt_subscription_list subscription_list = {
		.list = subscribe_topics,
		.list_count = ARRAY_SIZE(subscribe_topics),
		.message_id = 1234
	};

	for(int i = 0; i < ARRAY_SIZE(subscribe_topics); i++) {
		LOG_DBG("Subscribing to: %s len %u", subscribe_topics[i].topic.utf8, subscribe_topics[i].topic.size);
	}

	return mqtt_subscribe(c, &subscription_list);
}

/**@brief Function to print strings without null-termination
 */
static void data_print(uint8_t *prefix, uint8_t *data, size_t len)
{
	char buf[len + 1];

	memcpy(buf, data, len);
	buf[len] = 0;
	LOG_DBG("%s%s", (char *)prefix, (char *)buf);
}

/**@brief Generic function to publish data on any topic
 */
int data_publish_generic(struct mqtt_client *c, char *topic, uint8_t *data, size_t len)
{
	struct mqtt_publish_param param;

	param.message.topic.qos = MQTT_QOS_1_AT_LEAST_ONCE;
	param.message.topic.topic.utf8 = topic;
	param.message.topic.topic.size = strlen(topic);
	param.message.payload.data = data;
	param.message.payload.len = len;
	param.message_id = sys_rand32_get();
	param.dup_flag = 0;
	param.retain_flag = 0;

	data_print("Publishing: ", data, len);
	LOG_DBG("to topic: %s len: %u", topic, (unsigned int)strlen(topic));

	return mqtt_publish(c, &param);
}

/**@brief Function to publish data on the default topic
 */
int app_mqtt_publish(uint8_t *data, size_t len)
{
	return data_publish_generic(&client, CONFIG_MQTT_PUB_TOPIC, data, len);
}

/**@brief MQTT client event handler
 */
void mqtt_evt_handler(struct mqtt_client *const c,
		      const struct mqtt_evt *evt)
{
	int err;

	switch (evt->type) {
	case MQTT_EVT_CONNACK:
		if (evt->result != 0) {
			LOG_ERR("MQTT connect failed: %d", evt->result);
			break;
		}

		// Call the connected callback, unless it is not set (NULL)
		if(registered_callbacks.connected) {
			registered_callbacks.connected();
		}
		
		subscribe(c);
		break;

	case MQTT_EVT_DISCONNECT:
		LOG_DBG("MQTT client disconnected: %d", evt->result);

		// Call the disconnected callback, unless it is not set (NULL) 
		if(registered_callbacks.disconnected) {
			registered_callbacks.disconnected(evt->result);
		}
		break;

	case MQTT_EVT_PUBLISH:
	{
		const struct mqtt_publish_param *p = &evt->param.publish;
		// Print the length of the recived message 
		LOG_DBG("MQTT PUBLISH result=%d len=%d",
			evt->result, p->message.payload.len);

		// Extract the data of the recived message 
		err = get_received_payload(c, p->message.payload.len);
		
		// Send acknowledgment to the broker on receiving QoS1 publish message 
		if (p->message.topic.qos == MQTT_QOS_1_AT_LEAST_ONCE) {
			const struct mqtt_puback_param ack = {
				.message_id = p->message_id
			};

			// Send acknowledgment
			mqtt_publish_qos1_ack(c, &ack);
		}

		if (err >= 0) {
			data_print("Received: ", payload_buf, p->message.payload.len);

			// Store the topic name in a separate null terminated string
			int topic_len = MIN(CONFIG_MQTT_PAYLOAD_BUFFER_SIZE - 1, p->message.topic.topic.size);
			memcpy(topic_buf, p->message.topic.topic.utf8, topic_len);
			topic_buf[topic_len] = 0;

			// Call the data_rx callback as long as it is set, and provide the payload data
			if(registered_callbacks.data_rx) {

				registered_callbacks.data_rx(payload_buf, p->message.payload.len, topic_buf);
			}
		} 
		// Payload buffer is smaller than the received data 
		else if (err == -EMSGSIZE) {
			LOG_ERR("Received payload (%d bytes) is larger than the payload buffer size (%d bytes).",
				p->message.payload.len, sizeof(payload_buf));
		} 
		// Failed to extract data, disconnect 
		else {
			LOG_ERR("get_received_payload failed: %d", err);
			LOG_INF("Disconnecting MQTT client...");

			err = mqtt_disconnect(c);
			if (err) {
				LOG_ERR("Could not disconnect: %d", err);
			}
		}
	} break;

	case MQTT_EVT_PUBACK:
		if (evt->result != 0) {
			LOG_ERR("MQTT PUBACK error: %d", evt->result);
			break;
		}

		LOG_DBG("PUBACK packet id: %u", evt->param.puback.message_id);
		break;

	case MQTT_EVT_SUBACK:
		if (evt->result != 0) {
			LOG_ERR("MQTT SUBACK error: %d", evt->result);
			break;
		}

		LOG_DBG("SUBACK packet id: %u", evt->param.suback.message_id);
		break;

	case MQTT_EVT_PINGRESP:
		if (evt->result != 0) {
			LOG_ERR("MQTT PINGRESP error: %d", evt->result);
		}
		break;

	default:
		LOG_WRN("Unhandled MQTT event type: %d", evt->type);
		break;
	}
}

/**@brief Resolves the configured hostname and
 * initializes the MQTT broker structure
 */
static int broker_init(void)
{
	int err;
	struct addrinfo *result;
	struct addrinfo *addr;
	struct addrinfo hints = {
		.ai_family = AF_INET,
		.ai_socktype = SOCK_STREAM
	};

	err = getaddrinfo(CONFIG_MQTT_BROKER_HOSTNAME, NULL, &hints, &result);
	if (err) {
		LOG_ERR("getaddrinfo failed: %d", err);
		return -ECHILD;
	}

	addr = result;

	/* Look for address of the broker. */
	while (addr != NULL) {
		/* IPv4 Address. */
		if (addr->ai_addrlen == sizeof(struct sockaddr_in)) {
			struct sockaddr_in *broker4 =
				((struct sockaddr_in *)&broker);
			char ipv4_addr[NET_IPV4_ADDR_LEN];

			broker4->sin_addr.s_addr =
				((struct sockaddr_in *)addr->ai_addr)
				->sin_addr.s_addr;
			broker4->sin_family = AF_INET;
			broker4->sin_port = htons(CONFIG_MQTT_BROKER_PORT);

			inet_ntop(AF_INET, &broker4->sin_addr.s_addr,
				  ipv4_addr, sizeof(ipv4_addr));
			LOG_INF("IPv4 Address found %s", (char *)(ipv4_addr));

			break;
		} else {
			LOG_ERR("ai_addrlen = %u should be %u or %u",
				(unsigned int)addr->ai_addrlen,
				(unsigned int)sizeof(struct sockaddr_in),
				(unsigned int)sizeof(struct sockaddr_in6));
		}

		addr = addr->ai_next;
	}

	/* Free the address. */
	freeaddrinfo(result);

	return err;
}

/* Function to get the client id */
static const uint8_t* client_id_get(void)
{
	static uint8_t client_id[MAX(sizeof(CONFIG_MQTT_CLIENT_ID),
				     CLIENT_ID_LEN)];

	if (strlen(CONFIG_MQTT_CLIENT_ID) > 0) {
		snprintf(client_id, sizeof(client_id), "%s",
			 CONFIG_MQTT_CLIENT_ID);
		goto exit;
	}

	uint32_t id = sys_rand32_get();
	snprintf(client_id, sizeof(client_id), "%s-%010u", CONFIG_BOARD, id);

exit:
	LOG_DBG("client_id = %s", (char *)client_id);

	return client_id;
}

/**@brief Initialize the MQTT client structure
 */
int client_init(struct mqtt_client *client)
{
	int err;
	/* Initializes the client instance. */
	mqtt_client_init(client);

	/* Resolves the configured hostname and initializes the MQTT broker structure */
	err = broker_init();
	if (err) {
		LOG_ERR("Failed to initialize broker connection");
		return err;
	}

	/* MQTT client configuration */
	client->broker = &broker;
	client->evt_cb = mqtt_evt_handler;
	client->client_id.utf8 = client_id_get();
	client->client_id.size = strlen(client->client_id.utf8);
	client->password = NULL;
	client->user_name = NULL;
	client->protocol_version = MQTT_VERSION_3_1_1;

	/* MQTT buffers configuration */
	client->rx_buf = rx_buffer;
	client->rx_buf_size = sizeof(rx_buffer);
	client->tx_buf = tx_buffer;
	client->tx_buf_size = sizeof(tx_buffer);

	/* We are not using TLS */
	client->transport.type = MQTT_TRANSPORT_NON_SECURE;

	return err;
}

/**@brief Initialize the file descriptor structure used by poll.
 */
int fds_init(struct mqtt_client *c, struct pollfd *fds)
{
	if (c->transport.type == MQTT_TRANSPORT_NON_SECURE) {
		fds->fd = c->transport.tcp.sock;
	} else {
		return -ENOTSUP;
	}

	fds->events = POLLIN;
	return 0;
}

void app_mqtt_set_callbacks(const struct app_mqtt_callbacks_t *callbacks)
{
	registered_callbacks = *callbacks;
}

void app_mqtt_run(void)
{
	int err;
	uint32_t connect_attempt = 0;

	err = client_init(&client);
	if (err) {
		LOG_ERR("Failed to initialize MQTT client: %d", err);
		return;
	}

	while (1) {
		do {
			if (connect_attempt++ > 0) {
				LOG_INF("Reconnecting in %d seconds...", CONFIG_MQTT_RECONNECT_DELAY_S);
				k_sleep(K_SECONDS(CONFIG_MQTT_RECONNECT_DELAY_S));
			}
			err = mqtt_connect(&client);
			if (err) {
				LOG_ERR("Error in mqtt_connect: %d", err);
			}
		} while (err != 0);

		err = fds_init(&client,&fds);
		if (err) {
			LOG_ERR("Error in fds_init: %d", err);
			return;
		}

		while (1) {
			err = poll(&fds, 1, mqtt_keepalive_time_left(&client));
			if (err < 0) {
				LOG_ERR("Error in poll(): %d", errno);
				break;
			}

			err = mqtt_live(&client);
			if ((err != 0) && (err != -EAGAIN)) {
				LOG_ERR("Error in mqtt_live: %d", err);
				break;
			}

			if ((fds.revents & POLLIN) == POLLIN) {
				err = mqtt_input(&client);
				if (err != 0) {
					LOG_ERR("Error in mqtt_input: %d", err);
					break;
				}
			}

			if ((fds.revents & POLLERR) == POLLERR) {
				LOG_ERR("POLLERR");
				break;
			}

			if ((fds.revents & POLLNVAL) == POLLNVAL) {
				LOG_ERR("POLLNVAL");
				break;
			}
		}

		LOG_INF("Disconnecting MQTT client");

		err = mqtt_disconnect(&client);
		if (err) {
			LOG_ERR("Could not disconnect MQTT client: %d", err);
		}
	}
}
