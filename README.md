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
- nRF Connect SDK v2.3.0 (isntalled through the Toolchain Manager)

For instructions on how to install these items, please follow the exercise [here](https://academy.nordicsemi.com/topic/exercise-1-1/)

## Workshop steps

### Step 1 - Go through the steps in the MQTT over Wi-Fi blog
-------------------------------------------------------------

The first part of this workshop is to replicate the steps detailed in the MQTT over Wi-Fi blog. 
Hint: For Windows users the download link to the required Protocol Buffers service is a bit hard to find, direct link [here](https://github.com/protocolbuffers/protobuf/releases/download/v23.2/protoc-23.2-win64.zip). 

Go through all the stages of the blog, and make sure to select Option A in Stage 1 to ensure you can build the application successfully in VSCode. 
To save time the steps needed to set up the *LED2 On* and *LED2 Off* buttons can be omitted, as these buttons will not be needed in later steps. 

### Step 2 - Add MQTT topics to request and send a temperature update
---------------------------------------------------------------------
In this step the goal is to add another publish topic for sending temperature updates from the DK, and subscribe to a topic that allows the mobile app to request a temperature update from the DK. 

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
Note: If you set a unique default value in the Kconfig file this step is not strictly necessary.

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

In order to test the new functionality a button and a text field needs to be added to the MQTT Dashboard app. 
The button should be configured to use the same string as CONFIG_MQTT_SUB_TEMP_REQUEST_TOPIC, while the text field should use CONFIG_MQTT_PUB_TEMP_TOPIC. Again make sure to use the personalized topic strings rather than the default ones used in the repository. 

### Step 3 - Add a thread to send temperature updates periodically

### Step 4 - Enable the Wi-Fi 6 Target Wake Time (TWT) feature in the code to evaulate power consumption

### Step 5 - Add Bluetooth functionality to the application, and enable the Nordic UART Service (NUS) in the peripheral role