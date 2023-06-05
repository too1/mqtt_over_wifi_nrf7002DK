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

#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/bluetooth/hci.h>
#include <bluetooth/scan.h>

#include "app_mqtt.h"

LOG_MODULE_REGISTER(MQTT_OVER_WIFI, LOG_LEVEL_INF);

K_SEM_DEFINE(wifi_connected_sem, 0, 1);

#define NETWORK_SSID "EmeaWorkshop"
#define NETWORK_PWD  "BillionBluetooth"

static struct net_mgmt_event_callback wifi_prov_cb;

static bool mqtt_connected = false;

// Function for enabling or disabling TWT mode
static void wifi_set_twt(bool enable)
{
	struct net_if *iface = net_if_get_default();
	struct wifi_twt_params params = { 0 };
	uint8_t flow_id = 1;

	params.negotiation_type = WIFI_TWT_INDIVIDUAL;
	params.setup_cmd = WIFI_TWT_SETUP_CMD_REQUEST;
	params.flow_id = flow_id;

	if (enable){
		params.operation = WIFI_TWT_SETUP;
		params.setup.twt_interval_ms = 15000;
		params.setup.responder = 0;
		params.setup.trigger = 1;
		params.setup.implicit = 1;
		params.setup.announce = 1;
		params.setup.twt_wake_interval_ms = 65;
	} else {
		params.operation = WIFI_TWT_TEARDOWN;
		params.teardown.teardown_all = 1;
		flow_id = (flow_id < WIFI_MAX_TWT_FLOWS) ? flow_id + 1 : 1;
	}
	
	if (net_mgmt(NET_REQUEST_WIFI_TWT, iface, &params, sizeof(params))) {
		LOG_ERR("Operation %s with negotiation type %s failed", wifi_twt_operation2str[params.operation], wifi_twt_negotiation_type2str[params.negotiation_type]);
	}
	LOG_INF("TWT operation %s with flow_id: %d requested", wifi_twt_operation2str[params.operation], params.flow_id);
}

static void handle_wifi_twt_event(struct net_mgmt_event_callback *cb)
{
	const struct wifi_twt_params *resp = (const struct wifi_twt_params *)cb->info;

	LOG_INF("TWT response: CMD %s for dialog: %d and flow: %d\n",
	      wifi_twt_setup_cmd2str[resp->setup_cmd], resp->dialog_token, resp->flow_id);

	// If accepted, then no need to print TWT params
	if (resp->setup_cmd != WIFI_TWT_SETUP_CMD_ACCEPT) {
		LOG_INF("TWT parameters: trigger: %s wake_interval_ms: %d, interval_ms: %d\n",
		      resp->setup.trigger ? "trigger" : "no_trigger",
		      resp->setup.twt_wake_interval_ms,
		      resp->setup.twt_interval_ms);
	}
}

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
			wifi_set_twt(true);
		}
		break;
	}
}

static float read_temperature(void)
{
	// Keep track of the previously returned temperature
	static float previous_temp = 20.0f;

	// Generate a random temperature in the range 0-40 C
	float random_temp = (float)(rand() % 40001) * 0.001f;

	// Set the temperature to a mix of the old and the new, in order to simulate a slowly changing temperature
	previous_temp = previous_temp * 0.9f + random_temp * 0.1f;
	
	return previous_temp;
}

static void mqtt_connected_handler(void)
{
	dk_set_led_on(DK_LED2);
	mqtt_connected = true;
}

static void mqtt_disconnected_handler(int result)
{
	dk_set_led_off(DK_LED2);
	mqtt_connected = false;
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
	// Check if something is received on the temperature request topic
	else if (strcmp(topic_string, CONFIG_MQTT_SUB_TEMP_REQUEST_TOPIC) == 0) {
		// If anything is received on the temp request topic, send a temperature reading in return
		app_mqtt_publish_temp(read_temperature());
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
		case NET_EVENT_WIFI_TWT:
			handle_wifi_twt_event(cb);
			break;
		default:
			break;
	}
}

K_SEM_DEFINE(sem_temp_from_beacon, 0, 1);
static float temp_from_beacon;

static void temp_update_thread_func(void *p)
{
	float current_temperature;
	static float temperature_list[CONFIG_TEMP_ARRAY_MAX_LENGTH];
	int temperature_list_count = 0;

	while (1) {
		if(mqtt_connected) {
			if (k_sem_take(&sem_temp_from_beacon, K_NO_WAIT) == 0)  {
				LOG_INF("Reading temperature from beacon data");
				current_temperature = temp_from_beacon;
			} else {
				LOG_INF("No beacon found. Using simulated temperature data");
				current_temperature = read_temperature();
			}

			// Send the current temperature over MQTT
			app_mqtt_publish_temp(current_temperature);
			
			// Update and send a list of the current temperature, as well as previous readings
			if(temperature_list_count < CONFIG_TEMP_ARRAY_MAX_LENGTH) {
				temperature_list[temperature_list_count++] = current_temperature;
			} else {
				// In case the list is full, move all existing values one step up to make room for another value
				for(int i = 0; i < (CONFIG_TEMP_ARRAY_MAX_LENGTH - 1); i++) {
					temperature_list[i] = temperature_list[i + 1];
				}
				temperature_list[CONFIG_TEMP_ARRAY_MAX_LENGTH - 1] = current_temperature;
			}
			app_mqtt_publish_temp_array(temperature_list, temperature_list_count);
		}
		k_msleep(10000);
	}
}

uint8_t *beacon_short_name = "TempBeacon";
struct adv_mfg_data {
	uint16_t company_code;
	uint16_t temperature; 
	uint32_t rnd_number;  
} __packed;

static struct adv_mfg_data beacon_data;
static bool valid_name_found, valid_manuf_data_found; 

static bool data_cb(struct bt_data *data, void *user_data)
{
	switch (data->type) {
		case BT_DATA_NAME_COMPLETE:
			if(memcmp(data->data, beacon_short_name, strlen(beacon_short_name)) == 0) {
				valid_name_found = true;
			}
			return true;
		case BT_DATA_MANUFACTURER_DATA:
			if(data->data_len == sizeof(struct adv_mfg_data)) {
				memcpy(&beacon_data, data->data, data->data_len);
				if(beacon_data.company_code == 0x0059) {
					LOG_DBG("NORDIC beacon found! Temp %x, Rnd %x", 
							beacon_data.temperature, beacon_data.rnd_number);
					valid_manuf_data_found = true;
					return false;
				}
			}
			return true;
		default:
			return true;
	}
}

void on_scan_no_match(struct bt_scan_device_info *device_info, bool connectable)
{
	valid_name_found = valid_manuf_data_found = false;
	bt_data_parse(device_info->adv_data, data_cb, (void *)&device_info->recv_info->addr);
	if(valid_name_found && valid_manuf_data_found) {
		temp_from_beacon = (float)beacon_data.temperature * 0.25f;
		k_sem_give(&sem_temp_from_beacon);
	}
}

BT_SCAN_CB_INIT(scan_cb, NULL, on_scan_no_match, NULL, NULL);

static int bt_init(void)
{
	int err;

	struct bt_le_scan_param my_scan_params = {
		.type       = BT_LE_SCAN_TYPE_ACTIVE,
		.options    = BT_LE_SCAN_OPT_NONE,
		.interval   = (BT_GAP_SCAN_FAST_INTERVAL * 4),
		.window     = BT_GAP_SCAN_FAST_WINDOW,
	};

	struct bt_scan_init_param scan_init = {
		.scan_param = &my_scan_params,
	};

	err = bt_enable(0);
	if (err) {
		LOG_ERR("BT enable failed (err %i)", err);
		return err;
	}

	bt_scan_init(&scan_init);
	bt_scan_cb_register(&scan_cb);

	err = bt_scan_start(BT_SCAN_TYPE_SCAN_PASSIVE);
	if (err) {
		LOG_ERR("Scanning failed to start (err %d)", err);
		return err;
	}

	LOG_INF("Bluetooth initialized");
	return 0;
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

	bt_init();

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

	// Wait for the interface to be up
	k_sleep(K_SECONDS(6));

	LOG_INF("Connecting to MQTT Broker...");

	// Sett the callbacks for the app_mqtt module
	app_mqtt_set_callbacks(&mqtt_callbacks);

	// Run the MQTT connect loop (NOTE: this function will never exit)
	app_mqtt_run();
}

// Define an additional thread that will be used to send regular temperature updates
K_THREAD_DEFINE(temp_update_thread, 2048, temp_update_thread_func, 
				0, 0, 0, // P1, P2, P3
				K_LOWEST_APPLICATION_THREAD_PRIO, 0, 0); // Priority, Options, Delay
