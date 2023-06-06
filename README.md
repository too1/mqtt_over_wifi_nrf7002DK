nRF7002 MQTT Workshop
---------------------

This is a modified version of the MQTT over Wi-Fi example shared and described in [this devzone blog](https://devzone.nordicsemi.com/nordic/nordic-blog/b/blog/posts/implementing-mqtt-over-wi-fi-on-the-nrf7002-development-kit). 

The original sample shows how to run MQTT over Wi-Fi on the nRF7002DK and communicate with a mobile app running an MQTT Dashboard application, allowing data to be sent back and forth between the DK and the phone as long as both devices have an Internet connection. 

This workshop will expand on this example to add the following functionality:
- MQTT topics will be added to request and send temperature updates from the nRF7002DK to the app
- A thread will be added to send temperature updates continuously
- In order to evaluate the low power capabilities of the nRF7002 device and Wi-Fi 6 code will be added to enable the Target Wake Time feature of the device
- Bluetooth functionality will be added to the example, to show how the native Bluetooth capabilities of the nRF5340 host device on the nRF7002DK can be leveraged in parallel with a Wi-Fi application

## HW Requirements
- nRF7002-DK
- Power Profiler Kit II
- 2x USB Micro-B cables (one for the nRF7002DK and one for the PPK)

## SW Requirements
- nRF Connect for Desktop
   - Toolchain manager
   - Power Profiler
   - Bluetooth Low Energy
   - Serial Terminal 
- Visual Studio Code (VSCode)
   - nRF Connect for VSCode extension
- nRF Connect SDK v2.3.0 (installed through the Toolchain Manager)

For instructions on how to install these items, please follow the exercise [here](https://academy.nordicsemi.com/topic/exercise-1-1/)

## Workshop chapters

### Chapter 1 - Go through the chapters in the MQTT over Wi-Fi blog
-------------------------------------------------------------------

The first part of this workshop is to replicate the chapters detailed in the [MQTT over Wi-Fi blog](https://devzone.nordicsemi.com/nordic/nordic-blog/b/blog/posts/implementing-mqtt-over-wi-fi-on-the-nrf7002-development-kit). 
Hint: For Windows users the download link to the required Protocol Buffers service is a bit hard to find, direct link [here](https://github.com/protocolbuffers/protobuf/releases/download/v23.2/protoc-23.2-win64.zip). 

Go through all the stages of the blog, and make sure to select Option A in Stage 1 to ensure you can build the application successfully in VSCode. 
To save time the chapters needed to set up the *LED2 On* and *LED2 Off* buttons can be omitted, as these buttons will not be needed in later chapters. 

Make sure to use an open Wi-Fi network for the connected devices. For the workshop a Wi-Fi 6 router will be set up, and login credentials will be provided. Please don't connect to this network with PC's or phones, the Wi-Fi router should be used for the nRF7002-DK's only. 

***Don't forget to use unique MQTT publish/subscribe topics!!***
Find a unique identifier/name and use it to personalize the topics, to avoid interfering with others doing the same workshop. 

In the later chapters the default topics will be something like *devacademy/publish/emeaworkshop/temp*. Make sure to replace "emeaworkshop" with your own name or identifier, and use the same one when setting up the MQTT Dashboard app. 

### Chapter 2 - Add MQTT topics to request and send a temperature update
---------------------------------------------------------------------

While this and the following chapters will be based on the blog article and code used in chapter 1, a modified project will be used that is more streamlined for further development. Before proceeding with the steps below please check out or download the code provided below:

[Modified workshop codebase](https://github.com/too1/mqtt_over_wifi_nrf7002DK)

Add the project in VSCode using the Open an existing application action in the nRF Connect for VSCode interface, and ensure that you can build the sample for the nrf7002dk_nrf5340_cpuapp board. 

In this chapter the goal is to add another publish topic for sending temperature updates from the DK, and subscribe to a topic that allows the mobile app to request a temperature update from the DK. 

This example uses application level Kconfig parameters to configure the application, defined in the Kconfig file in the root folder of the example. The original publish and subscribe topics are defined here, and in order to extend the example with more topics they should be added to this file as well. 

The definition here allows you to set a default value, which will be used for any parameter that is not defined in prj.conf. 

To add a publish topic for temperature and a subscribe topic for temperature request, add the following to Kconfig, somewhere after the original MQTT_PUB_TOPIC and MQTT_SUB_TOPIC definitions:

```C
config MQTT_PUB_TEMP_TOPIC
	string "MQTT temperature publish topic"
	default "devacademy/publish/topic44temp"

config MQTT_SUB_TEMP_REQUEST_TOPIC
	string "MQTT temperature request subscribe topic"
	default "devacademy/subscribe/topic44tempreq"
```

Then you should configure the new parameters in prj.conf, giving the topic an appropriate value. Make sure to replace 'emeaworkshop' below with something unique, to avoid conflict with other DK's. 
Note: If you set a unique default value in the Kconfig file this chapter is not strictly necessary.

```C
# Temperature related topics
CONFIG_MQTT_PUB_TEMP_TOPIC="devacademy/publish/emeaworkshop/temp"
CONFIG_MQTT_SUB_TEMP_REQUEST_TOPIC="devacademy/subscribe/emeaworkshop/tempreq"
```

In order to subscribe to the new topic it is necessary to modify the static int subscribe(struct mqtt_client *const c) function in app_mqtt.c. 

The subscribe_topics[] array currently has a single item only, and a second item should be added by adding the following code snippet to the array definition (line 82):
```C
		{
			.topic = {
				.utf8 = CONFIG_MQTT_SUB_TEMP_REQUEST_TOPIC,
				.size = strlen(CONFIG_MQTT_SUB_TEMP_REQUEST_TOPIC)
			},
			.qos = MQTT_QOS_1_AT_LEAST_ONCE
		},
```

In order to publish a temperature value a new function will be added to app_mqtt.c/h. Start by adding the following function prototype to app_mqtt.h (line 28):
```C
int app_mqtt_publish_temp(float temp);
```

The function itself should be defined in app_mgtt.c, around line 143. 
The full code will not be provided, but a good tip is to look at the implementation of the *app_mqtt_publish(..)* function showing how you can use the *data_publish_generic(struct mqtt_client *c, char *topic, uint8_t *data, size_t len)* function to publish generic data to any topic. 
Add code to convert the float temperature value to a string for transmission over MQTT. 
Make sure to use the new publish topic defined earlier, and not the default topic.  
```C
/**@brief Function to publish data on the temperature topic
 */
int app_mqtt_publish_temp(float temp)
{
	// Add your code  here
}
```

In order to have any data to send a *float read_temperature()* function needs to be implemented in main.c (around line 57). For the time being this function will just generate a dummy temperature value, since there is no temperature sensor easily available on the nRF7002-DK. 

The implementation of this function is up to you, but for convenience an example is provided below:
```C
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
```

Finally the mqtt_data_rx_handler(const uint8_t *data, uint32_t len, const uint8_t *topic_string) function in main.c (around line 80) needs to be updated to respond to a temperature request, and send a temperature value in return. 

First check the topic_string argument to make sure the received message was sent on the CONFIG_MQTT_SUB_TEMP_REQUEST_TOPIC. If this is the case, use the recently defined read_temperature() and app_mqtt_publish_temp(..) functions to get a new temperature reading and publish it to the temperature topic. 
```C
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
	// ADD YOUR CODE HERE
}
```

In order to test the new functionality a button and a text field needs to be added to the MQTT Dashboard app. 
The button should be configured to use the same string as CONFIG_MQTT_SUB_TEMP_REQUEST_TOPIC, while the text field should use CONFIG_MQTT_PUB_TEMP_TOPIC. Again make sure to use the personalized topic strings rather than the default ones used in the repository.
| Temp request button tile | Temperature text tile |
|--------------------------|-----------------------|
|<img src="https://github.com/too1/mqtt_over_wifi_nrf7002DK/blob/workshop_strip_down_v2/pics/mqttdash_tile_temp.jpg" width="300">|<img src="https://github.com/too1/mqtt_over_wifi_nrf7002DK/blob/workshop_strip_down_v2/pics/mqttdash_tile_req_temp.jpg" width="300">|



### Chapter 3 - Add a thread to send temperature updates periodically
---------------------------------------------------------------------
In this chapter the goal is to add a thread that will send a temperature update at regular intervals. Additionally, a new publish topic will be added to send a series of temperature values that can be added to a graph in the MQTT Dashboard application. 

Start by adding two new configuration parameters to the Kconfig file (around line 35):
```C
config MQTT_PUB_TEMP_ARRAY_TOPIC
	string "MQTT temperature graph publish topic"
	default "devacademy/publish/topic44temparray"

config TEMP_ARRAY_MAX_LENGTH
	int "Max number of items in the temperature array"
	default 16
```

MQTT_PUB_TEMP_ARRAY_TOPIC will be the topic for sending a series of temperature values, and TEMP_ARRAY_MAX_LENGTH will set the maximum number of temperature values in a series. 
Make sure to change the topic value (either the default value in Kconfig, or in prj.conf) to use a unique value. 

Next we will create a variable to store the MQTT connection state, to avoid trying to send updates when the connection is down. 
Start by adding the following variable definition to main.c (around line 31):
```C
static bool mqtt_connected = false;
```
Use the *mqtt_connected_handler()* and *mqtt_disconnected_handler()* functions in main.c to update the *mqtt_connected* variable, setting it true when connected and false when disconnected. 

Next we will create a thread that can be used to send temperature updates at regular intervals, as long as the *mqtt_connected* variable is true.

Add the following code to the bottom of main.c:
```C
// Define an additional thread that will be used to send regular temperature updates
K_THREAD_DEFINE(temp_update_thread, 2048, temp_update_thread_func, 
				0, 0, 0, // P1, P2, P3
				K_LOWEST_APPLICATION_THREAD_PRIO, 0, 0); // Priority, Options, Delay
```

Next we need to create the thread function that will implement the thread logic. Above the main function in main.c add the following empty function: 
```C
static void temp_update_thread_func(void *p)
{

}
```
Inside temp_update_thread_func, add a *while(1)* loop that will run forever. 
At the end of the while loop add a sleep call to avoid the thread blocking other threads, and to control how often temp updates are sent over MQTT:
```C
k_msleep(10000);
```
Before or after the sleep call, add code to check the state of the mqtt_connected variable. In case it is true, send a temperature update using the *app_mqtt_publish_temp()* function. 

Build and flash the code, and ensure that the temperature is updated every 10 seconds in the MQTT Dashboard app. 

In the next steps a new function will be implemented in app_mqtt.c/h to send a list of temperature values that will be displayed as a graph in the MQTT dashboard app. 

Start by adding the function prototype to app_mqtt.h (around line 30):
```C
int app_mqtt_publish_temp_array(float *temp_list, int len);
```
Then add the function definition anywhere in app_mqtt.c. 
The string needs to follow the format described [here](https://vetru-apps.github.io/mqtt-dashboard-documentation/tiles/line-chart.html). 

Floating point values can be used, and as an example in order to send the values 19.0, 19.5 and 20.0 the string should look like this: *[19.0,19.5,20.0]* 

Essentially a temporary string needs to be declared, then the list of floats should be converted to the above format, before the string needs to be sent to *CONFIG_MQTT_PUB_TEMP_ARRAY_TOPIC* using the *data_publish_generic(..)* function.  
Hint: The [*sprintf()* function](https://www.tutorialspoint.com/c_standard_library/c_function_sprintf.htm) can be used to convert float values to string and store it in a local string variable. 
In order to get a float with one decimal place, use the *%.1f* format specifier. 

In the MQTT Dashboard app add a new Line chart tile, and configure the Subscribe topic to *CONFIG_MQTT_PUB_TEMP_ARRAY_TOPIC*:
| Temperature graph line chart tile |
|-----------------------------------|
|<img src="https://github.com/too1/mqtt_over_wifi_nrf7002DK/blob/workshop_strip_down_v2/pics/mqttdash_tile_temp_graph.jpg" width="300">|

Build and flash the code, and verify that the temperature graph updates are coming in as expected. Please note that a graph with only one item will not work, the graph will not show anything unless you have at least 2 elements in the list. 
| Final MQTT Dashboard screen |
|-----------------------------|
|<img src="https://github.com/too1/mqtt_over_wifi_nrf7002DK/blob/workshop_strip_down_v2/pics/mqttdash_final_screen.jpg" width="300">|

### Chapter 4 - Enable the Wi-Fi 6 Target Wake Time (TWT) feature in the code to evaulate power consumption
-----------------------------------------------------------------------------------------------------------
In this chapter the Target Wake Time feature introduced with Wi-Fi 6 will be evaluated to see what kind of impact this can have on the average current consumption. Code will be added to the example to enable TWT at the press of a button, and the Power Profiler Kit II will be used to measure the difference in average current. 

Start by adding the following code to main.c, above the *button_handler(uint32_t button_state, uint32_t has_changed)* definition:
```C
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
```
Inside the *button_handler(uint32_t button_state, uint32_t has_changed)* definition change the DK_BTN2_MSK case handler to the following:
```C
	case DK_BTN2_MSK:
		if (button_state & DK_BTN2_MSK){	
			wifi_set_twt(true);
		}
		break;
```
This will ensure that when Button 2 is pressed on the DK the TWT request will be sent. 

Inside the *wifi_connect_handler(struct net_mgmt_event_callback *cb, uint32_t mgmt_event, struct net_if *iface)* function in main.c add one more case to the switch case:
```C
	case NET_EVENT_WIFI_TWT:
		handle_wifi_twt_event(cb);
		break;
```
Inside the main function in main.c, search for the net_mgmt_init_event_callback function and update the line to the following:
```C
net_mgmt_init_event_callback(&wifi_prov_cb, wifi_connect_handler, NET_EVENT_WIFI_CONNECT_RESULT | NET_EVENT_WIFI_TWT);
```
This will ensure that the NET_EVENT_WIFI_TWT event gets called when the TWT request has been processed. 

Build and flash the sample, and connect the PPK2 to the nRF7002-DK as shown in the picture below:

<img src="https://github.com/too1/mqtt_over_wifi_nrf7002DK/blob/workshop_strip_down_v2/pics/ppk_dk_setup.jpg" width="800">

Open the Power Profiler app in nRF Connect for Desktop, and connect to your PPK2. 
In the Power Profiler make sure *Ampere meter* mode is selected, and click *Start*. 

Wait for the nRF7002-DK to connect to the wifi network and start sending data. The current profile should look something like this:

<img src="https://github.com/too1/mqtt_over_wifi_nrf7002DK/blob/workshop_strip_down_v2/pics/ppk2_normal.jpg" width="800">

Press Button 2 on the nRF7002-DK and verify that TWT gets enabled in the log:

> [00:06:47.854,644] <inf> MQTT_OVER_WIFI: TWT operation TWT setup with flow_id: 1 requested
> [00:06:47.915,985] <inf> MQTT_OVER_WIFI: TWT response: CMD TWT accept for dialog: 0 and flow: 1

Check how the current profile changes in the Power Profile. It should now look something like this:

<img src="https://github.com/too1/mqtt_over_wifi_nrf7002DK/blob/workshop_strip_down_v2/pics/ppk2_twt.jpg" width="800">

Note: At the time of writing there is an issue with TWT where data communication becomes unreliable when TWT is enabled, and you might see the MQTT connection break when this mode is enabled. For the remainder of this workshop it is recommended to keep TWT disabled, to ensure reliable MQTT communication. This issue should be patched by the end of 2023. 

### Chapter 5 - Add Bluetooth functionality to the application, and look for a beacon advertising temperature information
-------------------------------------------------------------------------------------------------------------------------
In this chapter Bluetooth functionality will be added to the example in order to scan for Bluetooth beacons in the area. An advertiser has been set up in advance which will advertise the name "TempBeacon", and include a manufacturer specific data field which contains a temperature sensor reading, using the Nordic company ID of 0x0059. This temperature value can be used to update the temperature over MQTT to use real data rather than simulated values. 

The temperature data is stored as a 16-bit integer value, with a unit of 0.25C. This value needs to be converted to float before being sent over MQTT. 

Start by adding the following lines to prj.conf:
```C
## Bluetooth configuration
CONFIG_BT=y
CONFIG_BT_OBSERVER=y
CONFIG_BT_DEBUG_LOG=y
CONFIG_BT_SCAN=y
```

Add the following includes to the top of main.c:
```C
#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/bluetooth/hci.h>
#include <bluetooth/scan.h>
```
A semaphore will be defined to signal when a valid beacon packet was received over Bluetooth, in addition to a variable used to store the temperature value received. Add the following lines of code to main.c, somewhere above the implementation of the *temp_update_thread_func()* function:
```C
K_SEM_DEFINE(sem_temp_from_beacon, 0, 1);
static float temp_from_beacon;
```
In order to initialize the Bluetooth stack, and process the incoming beacon packets, add the following code to main.c:
```C
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
```
Inside the main function in main.c, after the call to *dk_buttons_init(button_handler)*, call the *bt_init()* function to initialize the Bluetooth stack. 

Inside the while loop in the *temp_update_thread_func(..)* function, add a check to see if the *sem_temp_from_beacon* semaphore is available. If you are able to acquire the semaphore, use the temperature value stored in the *temp_from_beacon* variable rather than call the *read_temperature()* function, as done in earlier chapters. 

