/*
 * Copyright (c) 2023 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#include <zephyr/kernel.h>
#include <stddef.h>
#include <string.h>
#include <errno.h>
#include <zephyr/sys/printk.h>

#include <zephyr/net/wifi.h>
#include <zephyr/net/wifi_mgmt.h>

#include <zephyr/net/socket.h>
#include <zephyr/net/mqtt.h>
#include <dk_buttons_and_leds.h>

#include "app_mqtt.h"

LOG_MODULE_REGISTER(MQTT_OVER_WIFI, LOG_LEVEL_INF);

K_SEM_DEFINE(wifi_connected_sem, 0, 1);

#define NETWORK_SSID "EmeaWorkshop"
#define NETWORK_PWD  "BillionBluetooth"

static struct net_mgmt_event_callback wifi_prov_cb;

static void button_handler(uint32_t button_state, uint32_t has_changed)
{
	switch (has_changed) {
	case DK_BTN1_MSK:
		if (button_state & DK_BTN1_MSK){	
			int err = app_mqtt_publish(CONFIG_BUTTON1_EVENT_PUBLISH_MSG, 
									sizeof(CONFIG_BUTTON1_EVENT_PUBLISH_MSG)-1);
			if (err) {
				LOG_ERR("Failed to send message, %d", err);
				return;	
			}
		}
		break;
	case DK_BTN2_MSK:
		if (button_state & DK_BTN2_MSK){	
			int err = app_mqtt_publish(CONFIG_BUTTON2_EVENT_PUBLISH_MSG, 
									sizeof(CONFIG_BUTTON2_EVENT_PUBLISH_MSG)-1);
			if (err) {
				LOG_ERR("Failed to send message, %d", err);
				return;	
			}
		}
		break;
	}
}

static void mqtt_connected_handler(void)
{
	dk_set_led_on(DK_LED2);
}

static void mqtt_disconnected_handler(int result)
{
	dk_set_led_off(DK_LED2);
}

static void mqtt_data_rx_handler(const uint8_t *data, uint32_t len, const uint8_t *topic_string)
{
	// Verify the topic of the incoming message
	if (strcmp(topic_string, CONFIG_MQTT_SUB_TOPIC) == 0) {
		// Control LED1 and LED2 
		if (strncmp(data, CONFIG_TURN_LED1_ON_CMD, sizeof(CONFIG_TURN_LED1_ON_CMD) - 1) == 0) {
			dk_set_led_on(DK_LED1);
		}
		else if (strncmp(data, CONFIG_TURN_LED1_OFF_CMD, sizeof(CONFIG_TURN_LED1_OFF_CMD) - 1) == 0) {
			dk_set_led_off(DK_LED1);
		}
		else if (strncmp(data, CONFIG_TURN_LED2_ON_CMD, sizeof(CONFIG_TURN_LED2_ON_CMD) - 1) == 0) {
			//dk_set_led_on(DK_LED2);
		}
		else if (strncmp(data, CONFIG_TURN_LED2_OFF_CMD, sizeof(CONFIG_TURN_LED2_OFF_CMD) - 1) == 0) {
			//dk_set_led_off(DK_LED2);
		}
	}
}

const struct app_mqtt_callbacks_t mqtt_callbacks = {
	.connected = mqtt_connected_handler,
	.disconnected = mqtt_disconnected_handler,
	.data_rx = mqtt_data_rx_handler
};

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

	// Sleep 1 seconds to allow initialization of wifi driver.
	k_sleep(K_SECONDS(1));

	if (dk_leds_init() != 0) {
		LOG_ERR("Failed to initialize the LED library");
	}

	if (dk_buttons_init(button_handler) != 0) {
		LOG_ERR("Failed to initialize the buttons library");
	}

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

	// Wait for the interface to be up
	k_sleep(K_SECONDS(6));

	LOG_INF("Connecting to MQTT Broker...");

	// Sett the callbacks for the app_mqtt module
	app_mqtt_set_callbacks(&mqtt_callbacks);

	// Run the MQTT connect loop (NOTE: this function will never exit)
	app_mqtt_run();
}

