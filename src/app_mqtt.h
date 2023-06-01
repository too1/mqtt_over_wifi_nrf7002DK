/*
 * Copyright (c) 2023 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */
#ifndef __APP_MQTT_H
#define __APP_MQTT_H

#include <zephyr/kernel.h>

#define RANDOM_LEN 10
#define CLIENT_ID_LEN sizeof(CONFIG_BOARD) + 1 + RANDOM_LEN

typedef void (*app_mqtt_connected_cb)(void);
typedef void (*app_mqtt_disconnected_cb)(int result);
typedef void (*app_mqtt_data_rx_cb)(const uint8_t *data, uint32_t len, const uint8_t *topic_string);

struct app_mqtt_callbacks_t {
	app_mqtt_connected_cb connected;
	app_mqtt_disconnected_cb disconnected;
	app_mqtt_data_rx_cb data_rx;
};

void app_mqtt_set_callbacks(const struct app_mqtt_callbacks_t *callbacks);

int app_mqtt_publish(uint8_t *data, size_t len);

int app_mqtt_publish_temp(float temp);

int app_mqtt_publish_temp_array(float *temp_list, int len);

void app_mqtt_run(void);

#endif 
