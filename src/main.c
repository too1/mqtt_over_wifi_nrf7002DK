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

int wifi_set_twt();

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
			LOG_INF("Trying to enable TWT.......");
			wifi_set_twt();
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

bool nrf_wifi_ps_enabled = 0;
uint8_t flow_id = 1;
static void handle_wifi_twt_event(struct net_mgmt_event_callback *cb)
{
	const struct wifi_twt_params *resp = (const struct wifi_twt_params *)cb->info;

	LOG_INF("TWT response: CMD %s for dialog: %d and flow: %d\n",
	      wifi_twt_setup_cmd2str[resp->setup_cmd], resp->dialog_token, resp->flow_id);

	/* If accepted, then no need to print TWT params */
	if (resp->setup_cmd != WIFI_TWT_SETUP_CMD_ACCEPT) {
		LOG_INF("TWT parameters: trigger: %s wake_interval_ms: %d, interval_ms: %d\n",
		      resp->setup.trigger ? "trigger" : "no_trigger",
		      resp->setup.twt_wake_interval_ms,
		      resp->setup.twt_interval_ms);
	}
	nrf_wifi_ps_enabled = 1;
}

static void wifi_connect_handler(struct net_mgmt_event_callback *cb,
				    uint32_t mgmt_event, struct net_if *iface)
{
	switch (mgmt_event) {
	case NET_EVENT_WIFI_CONNECT_RESULT:
		LOG_INF("Connected to a Wi-Fi Network");
		k_sem_give(&wifi_connected_sem);
		break;
	case NET_EVENT_WIFI_TWT:
		handle_wifi_twt_event(cb);
		break;
	default:
		break;
	}
}

static float get_current_temperature()
{
	// Keep track of the previously returned temperature
	static float previous_temp = 20.0f;

	// Generate a random temperature in the range 16-24 C
	float random_temp = 16.0f + (float)(rand() % 8000) * 0.001f;

	// Set the temperature to a mix of the old and the new, in order to simulate a slowly changing temperature
	previous_temp = previous_temp * 0.8f + random_temp * 0.2f;
	
	return previous_temp;
}

void temp_update_thread_func(void *p)
{
	int ret;
	static float temperature = 16.0f;
	while (1) {
		LOG_INF("Trying to send a temperature update");
		ret = data_temp_publish(&client, MQTT_QOS_1_AT_LEAST_ONCE, get_current_temperature());
		if (ret < 0) {
			LOG_INF("MQTT publish failed (err %i)", ret);
		}
		temperature += 0.1f;
		k_msleep(10000);
	}
}

int wifi_set_twt()
{
	printk("TWT %s\n", nrf_wifi_ps_enabled ? "teardown" : "setup");
	struct net_if *iface = net_if_get_default();
	struct wifi_twt_params params = { 0 };

	params.negotiation_type = WIFI_TWT_INDIVIDUAL;
	params.setup_cmd = WIFI_TWT_SETUP_CMD_REQUEST;
	params.flow_id = flow_id;

	if (nrf_wifi_ps_enabled){
		params.operation = WIFI_TWT_TEARDOWN;
		params.teardown.teardown_all = 1;
		flow_id = flow_id<WIFI_MAX_TWT_FLOWS ? flow_id+1 : 1;
		nrf_wifi_ps_enabled = 0;
	}
	else {
		params.operation = WIFI_TWT_SETUP;
		params.setup.twt_interval_ms = 15000;
		params.setup.responder = 0;
		params.setup.trigger = 1;
		params.setup.implicit = 1;
		params.setup.announce = 1;
		params.setup.twt_wake_interval_ms = 65;
	}
	
	if (net_mgmt(NET_REQUEST_WIFI_TWT, iface, &params, sizeof(params))) {
		LOG_ERR("Operation %s with negotiation type %s failed", wifi_twt_operation2str[params.operation], wifi_twt_negotiation_type2str[params.negotiation_type]);
	}
	LOG_INF("TWT operation %s with flow_id: %d requested", wifi_twt_operation2str[params.operation], params.flow_id);
	return 0;
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

	net_mgmt_init_event_callback(&wifi_prov_cb, wifi_connect_handler, NET_EVENT_WIFI_CONNECT_RESULT | NET_EVENT_WIFI_TWT);

	net_mgmt_add_event_callback(&wifi_prov_cb);

	k_sem_take(&wifi_connected_sem, K_FOREVER);

	/* Wait for the interface to be up */
	k_sleep(K_SECONDS(6));

	LOG_INF("Connecting to MQTT Broker...");

	/* Connect to MQTT Broker */
	connect_mqtt();
}

K_THREAD_DEFINE(temp_update_thread, 2048, temp_update_thread_func, 0, 0, 0, 7, 0, 10000);
