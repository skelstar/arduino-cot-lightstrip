
#include <Wire.h>
#include <ArduinoNunchuk.h>

#include <ESP8266WiFi.h>
#include <WiFiUdp.h>

#include <Time.h>
#include <TimeLib.h>

#include <LPD8806.h>
#include "SPI.h"


#include <Adafruit_NeoPixel.h>
#ifdef __AVR__
  #include <avr/power.h>
#endif

// --- esp 8266 ---

#define SSID "Leila2Net"       
#define PASS "ec1122%f*&"   
#define DST_IP "192.168.0.1" 

// ----- strip -----

#define   clkPin    13
#define   dataPin   12
#define   nLEDs     62

// red: 5v, clk, data, gnd

LPD8806 strip = LPD8806(nLEDs, dataPin, clkPin);

#define   BRIGHTNESS_1    1   // 1 == 2mA/8 (16mA for 62)
#define   BRIGHTNESS_12   12  // 12 == 12mA/8 (93 mA for 62)
#define   BRIGHTNESS_24   24  // 24 == 23mA/8 (178mA for 62)

#define BRIGHTNESS_IDLE   BRIGHTNESS_1
#define BRIGHTNESS_ON     BRIGHTNESS_24

int currentBrightness = 0;

//int BRIGHTNESS_ON = 3;  // up to 12

// ---- WiiChuck -----
ArduinoNunchuk nunchuk = ArduinoNunchuk();
#define pwrpin  
int zButton_old;
bool analogOffAxis_old = false;

#define   ANALOG_UP     1
#define   ANALOG_RIGHT  2
#define   ANALOG_DOWN   3
#define   ANALOG_LEFT   4


// - + SDA SCL
// red: -
// green: +
// yellow: data
// white: clk

// state machine

#define   STATE_OFF         0
#define   STATE_IDLE        1
#define   STATE_ON          2
#define   STATE_FADING_UP   3
#define   STATE_FADING_DOWN 4

#define   STATE_DEFAULT     STATE_IDLE

#define   LOOP_DELAYms      100
#define   LIGHTS_ON_SECONDS   5

int state;
int next;
int loopCounter;
int onCounter = 0;

void setup() {

  delay(1000);
  
  loopCounter = 0;
  
  Serial.begin(115200);
  Serial.println("\n\nReady..");

  strip.begin();
  strip.show();

  nunchuk.init();

  changeStateToThen(STATE_DEFAULT, STATE_DEFAULT);
}

void loop() {

  zButton_old = nunchuk.zButton;
  nunchuk.update();

  if (state == STATE_OFF) {
    if (zButtonPressDownEvent()) {
      changeStateToThen(STATE_FADING_UP, STATE_ON);
    }
  }

  if (state == STATE_IDLE) {
    currentBrightness = BRIGHTNESS_IDLE;

    if (zButtonPressDownEvent()) {
      changeStateToThen(STATE_FADING_UP, STATE_ON);
    }
  }

  if (state == STATE_FADING_UP) {
    currentBrightness = stripFadeUp(currentBrightness, BRIGHTNESS_ON);
    if (currentBrightness == BRIGHTNESS_ON) {
      changeStateToThen(STATE_ON, STATE_ON);
      setTime(hour(), minute(), 0, day(), month(), year());
    }

    if (zButtonPressDownEvent()) {
      changeStateToThen(STATE_FADING_DOWN, STATE_DEFAULT);
    }
  }

  if (state == STATE_FADING_DOWN) {
    currentBrightness = stripFadeDown(currentBrightness);
    if (currentBrightness == BRIGHTNESS_IDLE) {
      changeStateToThen(STATE_DEFAULT, STATE_DEFAULT);
    }
  }  
  
  if (state == STATE_ON) {
    if (zButtonPressDownEvent()) {
      changeStateToThen(STATE_FADING_DOWN, STATE_DEFAULT);
    } else {
      if (second() > LIGHTS_ON_SECONDS)
        changeStateToThen(STATE_FADING_DOWN, STATE_DEFAULT);
    }
  }

  //chuckDebug();
  

  if (hour() >= 12)
    changeStateToThen(STATE_OFF, STATE_OFF);

  delay(LOOP_DELAYms);
  loopCounter++;
  updateLEDs();
}

void changeStateToThen(int newState, int nextState) {

  Serial.print("State: "); 
  Serial.print(stateName(newState));
  Serial.print(" >> ");
  if (newState != nextState) {
    Serial.println(stateName(nextState));
  } else {
    Serial.println("... waiting");
  }
  state = newState;
  next = nextState;
}

String stateName(int state) {
  switch (state) {
    case STATE_OFF:         return "OFF";         break;
    case STATE_ON:          return "ON";          break;
    case STATE_IDLE:        return "IDLE";        break;
    case STATE_FADING_UP:   return "FADING_UP";   break;
    case STATE_FADING_DOWN: return "FADING_DOWN"; break;
    default:      return "stateName:  ERROR NOT FOUND (" + String(state) + ")";  break;
  }
}

void stripOff() {
  redStrip(0);
}

bool zButtonPressDownEvent() {
  bool buttonDown = nunchuk.zButton == 1 && zButton_old == 0;
  zButton_old = nunchuk.zButton;
  return buttonDown;
}

bool analogStickMovedOffAxisEvent() {
  bool offAxis = nunchuk.analogX < 5 ||
                 nunchuk.analogX > 250 ||
                 nunchuk.analogY < 5 ||
                 nunchuk.analogY > 250;
  bool event = offAxis && !analogOffAxis_old;
  if (event)
    Serial.println("Gone OffAxis!");
  analogOffAxis_old = offAxis;  
  return event;
}

void redStrip(int brightness) {
  int i;

  for (i=0; i < strip.numPixels(); i++) {
      strip.setPixelColor(i, strip.Color(brightness, 0, 0));
      strip.show();
  }  
}

void updateLEDs() {

  redStrip(currentBrightness);
}

int stripFadeUp(int brightness, int targetBrightness) {

  if (brightness < targetBrightness) {
    redStrip(brightness++);
    //delay(FADE_UP_DELAYms);
    return brightness;
  }
  return brightness;
}

int stripFadeDown(int brightness) {

  if (brightness > 0) {
    redStrip(brightness--);
    //delay(FADE_DOWN_DELAYms);
    return brightness;
  }
  return brightness;
}

void chuckDebug() {

  Serial.print(nunchuk.analogX, DEC);
  Serial.print(' ');
  Serial.print(nunchuk.analogY, DEC);
  Serial.print(' ');
  Serial.print(nunchuk.accelX, DEC);
  Serial.print(' ');
  Serial.print(nunchuk.accelY, DEC);
  Serial.print(' ');
  Serial.print(nunchuk.accelZ, DEC);
  Serial.print(' ');
  Serial.print(nunchuk.zButton, DEC);
  Serial.print(' ');
  Serial.println(nunchuk.cButton, DEC);
}

