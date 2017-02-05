#include <myWifiHelper.h>
#include "LPD8806.h"
#include "SPI.h"
#include <myPushButton.h>

/*------------------------------------------------------------------*/

#define   clkPin    0
#define   dataPin   2
#define   nLEDs     62

#define FEEDBOTTLE_FEED  "/dev/bottleFeed"
#define HHmm_FEED        "/dev/HHmm"
#define WIFI_OTA_NAME   "arduino-cot-lightstrip-dev"
#define WIFI_HOSTNAME   "arduino-cot-lightstrip-dev"

#define MQTTFEED_COT_BRIGHTNESS	"/dev/cot-brightness"
#define MQTTFEED_COT_BRIGHTNESS_REQUEST "/dev/cot-brightness-request"

char versionText[] = "arduino-cot-lightstrip v0.9.0";

/*------------------------------------------------------------------*/

// red: 5v, clk, data, gnd

LPD8806 strip = LPD8806(nLEDs, dataPin, clkPin);

#define   BRIGHTNESS_1    2   // 1 == 2mA/8 (16mA for 62)
#define   BRIGHTNESS_12   12  // 12 == 12mA/8 (93 mA for 62)
#define   BRIGHTNESS_24   24  // 24 == 23mA/8 (178mA for 62)

int currentBrightness = 0;

MyWifiHelper wifiHelper(WIFI_HOSTNAME);

bool updateLeds;

/*------------------------------------------------------------------*/

void brightness_callback(byte* payload, unsigned int length) {

	currentBrightness = payload[0] - '0';
	Serial.print("Brightness set to: "); Serial.println(currentBrightness);
	updateLeds = true;
}


#define BUTTON_PIN	0
#define BUTTON 		0
#define LEDS 		1

void listener_Button(int eventCode, int eventParams);
#define LONG_PRESS_TIME	2000

myPushButton button(BUTTON_PIN, true, LONG_PRESS_TIME, 1, listener_Button);

void listener_Button(int eventCode, int eventParams) {
	if (eventParams == button.EV_RELEASED) {
    	wifiHelper.mqttPublish(MQTTFEED_COT_BRIGHTNESS_REQUEST, "0");
	}
}


/*------------------------------------------------------------------*/

void setup() {

	delay(100);

	Serial.begin(9600);
	delay(50);
	Serial.println("\n\nReady..");

	strip.begin();
	strip.show();

	uint32_t red = strip.Color(BRIGHTNESS_1, 0, 0);
	for (int i=0; i<strip.numPixels(); i++) {
		strip.setPixelColor(i, red);
	}
	strip.show();

    wifiHelper.setupWifi();

    wifiHelper.setupOTA(WIFI_OTA_NAME);

    wifiHelper.setupMqtt();

    wifiHelper.mqttAddSubscription(MQTTFEED_COT_BRIGHTNESS, brightness_callback);

	wifiHelper.loopMqttNonBlocking();

	delay(1000);

    wifiHelper.mqttPublish(MQTTFEED_COT_BRIGHTNESS_REQUEST, "0");
}

void loop() {

    // need this for both subscribing and publishing
    if (!wifiHelper.loopMqttNonBlocking()) {
        Serial.print('.');
    }

    ArduinoOTA.handle();

    button.serviceEvents();

    if (updateLeds) {
    	changePinFunc(LEDS);
		uint32_t red = strip.Color(currentBrightness, 0, 0);
		for (int i=0; i<strip.numPixels(); i++) {
			strip.setPixelColor(i, red);
		}
		strip.show();
		updateLeds = false;
		delay(50);
		changePinFunc(BUTTON);
    }

    delay(50);
}

/* ----------------------------------------- */

void changePinFunc(int func) {
	if (func == BUTTON) {
		pinMode(BUTTON_PIN, INPUT_PULLUP);
	}
	else if (func == LEDS) {
		pinMode(BUTTON_PIN, OUTPUT);
	}
}