/*
 * Copyright (c) 2023 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */
#include <zephyr/types.h>
#include <stddef.h>
#include <string.h>
#include <errno.h>
#include <zephyr/sys/printk.h>
#include <zephyr/kernel.h>

#include <zephyr/net/wifi.h>
#include <zephyr/net/wifi_mgmt.h>

#include <zephyr/net/socket.h>
#include <zephyr/net/mqtt.h>
#include <dk_buttons_and_leds.h>

#include "mqtt_connection.h"

LOG_MODULE_REGISTER(MQTT_OVER_WIFI, LOG_LEVEL_INF);
K_SEM_DEFINE(wifi_connected_sem, 0, 1);

#define ADV_DAEMON_STACK_SIZE 4096
#define ADV_DAEMON_PRIORITY 5

#define NETWORK_SSID "EmeaWorkshop"
#define NETWORK_PWD  "BillionBluetooth"

/* The mqtt client struct */
static struct mqtt_client client;

/* File descriptor */
static struct pollfd fds;

static struct net_mgmt_event_callback wifi_prov_cb;

static void button_handler(uint32_t button_state, uint32_t has_changed)
{
	switch (has_changed) {
	case DK_BTN1_MSK:
		if (button_state & DK_BTN1_MSK){	
			int err = data_publish(&client, MQTT_QOS_1_AT_LEAST_ONCE,
				   CONFIG_BUTTON1_EVENT_PUBLISH_MSG, sizeof(CONFIG_BUTTON1_EVENT_PUBLISH_MSG)-1);
			if (err) {
				LOG_ERR("Failed to send message, %d", err);
				return;	
			}
		}
		break;
	case DK_BTN2_MSK:
		if (button_state & DK_BTN2_MSK){	
			int err = data_publish(&client, MQTT_QOS_1_AT_LEAST_ONCE,
				   CONFIG_BUTTON2_EVENT_PUBLISH_MSG, sizeof(CONFIG_BUTTON2_EVENT_PUBLISH_MSG)-1);
			if (err) {
				LOG_ERR("Failed to send message, %d", err);
				return;	
			}
		}
		break;
	}
}

static void connect_mqtt(void)
{
	int err;
	uint32_t connect_attempt = 0;

	if (dk_leds_init() != 0) {
		LOG_ERR("Failed to initialize the LED library");
	}

	if (dk_buttons_init(button_handler) != 0) {
		LOG_ERR("Failed to initialize the buttons library");
	}

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

static void wifi_connect_handler(struct net_mgmt_event_callback *cb,
				    uint32_t mgmt_event, struct net_if *iface)
{
	switch (mgmt_event) {
	case NET_EVENT_WIFI_CONNECT_RESULT:
		LOG_INF("Connected to a Wi-Fi Network");
		k_sem_give(&wifi_connected_sem);
		break;
	default:
		break;
	}
}

void main(void)
{
	int rc;
	struct net_if *iface = net_if_get_default();
	struct wifi_connect_req_params cnx_params = { 0 };

	/* Sleep 1 seconds to allow initialization of wifi driver. */
	k_sleep(K_SECONDS(1));

	LOG_INF("Using static Wi-Fi configuration\n");
	char *wifi_static_ssid = NETWORK_SSID;
	char *wifi_static_pwd = NETWORK_PWD;

	cnx_params.ssid = wifi_static_ssid;
	cnx_params.ssid_length = strlen(wifi_static_ssid);
	cnx_params.security = WIFI_SECURITY_TYPE_PSK;

	cnx_params.psk = NULL;
	cnx_params.psk_length = 0;
	cnx_params.sae_password = NULL;
	cnx_params.sae_password_length = 0;

	cnx_params.psk = wifi_static_pwd;
	cnx_params.psk_length = strlen(wifi_static_pwd);

	cnx_params.channel = WIFI_CHANNEL_ANY;
	cnx_params.band = WIFI_FREQ_BAND_2_4_GHZ;
	cnx_params.mfp = WIFI_MFP_OPTIONAL;
	rc = net_mgmt(NET_REQUEST_WIFI_CONNECT, iface,
		&cnx_params, sizeof(struct wifi_connect_req_params));
	if (rc < 0) {
		LOG_ERR("Cannot apply saved Wi-Fi configuration, err = %d.\n", rc);
	} else {
		LOG_INF("Configuration applied.\n");
	}

	net_mgmt_init_event_callback(&wifi_prov_cb, wifi_connect_handler, NET_EVENT_WIFI_CONNECT_RESULT);

	net_mgmt_add_event_callback(&wifi_prov_cb);

	k_sem_take(&wifi_connected_sem, K_FOREVER);

	/* Wait for the interface to be up */
	k_sleep(K_SECONDS(6));

	LOG_INF("Connecting to MQTT Broker...");

	/* Connect to MQTT Broker */
	connect_mqtt();
}
