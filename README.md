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

### Step 3 - Add a thread to send temperature updates periodically

### Step 4 - Enable the Wi-Fi 6 Target Wake Time (TWT) feature in the code to evaulate power consumption

### Step 5 - Add Bluetooth functionality to the application, and enable the Nordic UART Service (NUS) in the peripheral role