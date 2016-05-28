#include <Time.h>
#include <TimeLib.h>
#include <EEPROM.h>

#include <Wire.h>
#include <ArduinoNunchuk.h>

#include "LPD8806.h"
#include "SPI.h"

#include <Ticker.h>

#ifdef __AVR__
  #include <avr/power.h>
#endif

// Watchdog / EEPROM

int eeStateAddr = 0;
int eeNextStateAddr = 1;
int eeCurrBrightnessAddr = 2;

static unsigned long last_loop;

Ticker tickerOSWatch;

#define ONE_MINUTE  60
#define OSWATCH_RESET_TIME_SECONDS      5 * ONE_MINUTE        // in seconds

// callback to be called when Ticker period trigger
void ICACHE_RAM_ATTR osWatchCallback(void) {
  unsigned long t = millis();
  unsigned long last_run = abs(t - last_loop);
  //if (last_run >= ( OSWATCH_RESET_TIME_SECONDS * 1000)) {
    // save the hit here to eeprom or to rtc memory if needed
    ESP.restart();  // normal reboot 
    //ESP.reset();  // hard reset
  //}
}


// ----- strip -----

#define   clkPin    13
#define   dataPin   12
#define   nLEDs     62

// red: 5v, clk, data, gnd

LPD8806 strip = LPD8806(nLEDs, dataPin, clkPin);

#define   BRIGHTNESS_1    2   // 1 == 2mA/8 (16mA for 62)
#define   BRIGHTNESS_12   12  // 12 == 12mA/8 (93 mA for 62)
#define   BRIGHTNESS_24   24  // 24 == 23mA/8 (178mA for 62)

#define BRIGHTNESS_OFF    0
#define BRIGHTNESS_IDLE   BRIGHTNESS_1
#define BRIGHTNESS_ON     BRIGHTNESS_24

#define BRIGHTNESS_DEFAULT  BRIGHTNESS_IDLE

int currentBrightness = 0;

//int BRIGHTNESS_ON = 3;  // up to 12

// ---- WiiChuck -----
ArduinoNunchuk nunchuk = ArduinoNunchuk();

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

#define   STATE_MAX_STATE  4

#define   STATE_DEFAULT     STATE_OFF

#define   LOOP_DELAYms      100
#define   LIGHTS_ON_PERIOD   60 * 10

int last;
int state;
int next;
int targetBrightness;
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

  EEPROM.begin(4);

  RestoreFromEEPROM();

  changeStateToThen(state, next);

  last_loop = millis();
  tickerOSWatch.attach_ms(OSWATCH_RESET_TIME_SECONDS * 1000, osWatchCallback);
}

void loop() {

  zButton_old = nunchuk.zButton;
  nunchuk.update();

  if (state == STATE_OFF) {
    if (zButtonPressDownEvent()) {
      changeStateToThen(STATE_FADING_UP, STATE_IDLE);
    }
  }

  if (state == STATE_IDLE) {
    currentBrightness = BRIGHTNESS_IDLE;

    if (zButtonPressDownEvent()) {
      if (last == STATE_FADING_DOWN) {
        changeStateToThen(STATE_FADING_DOWN, STATE_OFF);
      } else if (last == STATE_FADING_UP) {
        changeStateToThen(STATE_FADING_UP, STATE_ON);
      } else {
        changeStateToThen(STATE_FADING_UP, STATE_ON);
      }
    }
  }

  if (state == STATE_FADING_UP) {
    currentBrightness = stripFadeFromTo(currentBrightness, stateBrightness(next));
    if (currentBrightness == stateBrightness(next)) {
      changeStateToThen(next, next);
      onCounter = 0;
    }
  }

  if (state == STATE_FADING_DOWN) {
    currentBrightness = stripFadeFromTo(currentBrightness, stateBrightness(next));
    if (currentBrightness == stateBrightness(next)) {
      changeStateToThen(next, next);
    }
  }  
  
  if (state == STATE_ON) {
    if (zButtonPressDownEvent()) {
      changeStateToThen(STATE_FADING_DOWN, STATE_IDLE);
    } else {
      onCounter++;
      if (onCounter >= LIGHTS_ON_PERIOD)
        changeStateToThen(STATE_FADING_DOWN, STATE_DEFAULT);
    }
  }

  //chuckDebug();

  delay(LOOP_DELAYms);
  loopCounter++;
  updateLEDs();
}

/* ----------------------------------------- */

void RestoreFromEEPROM() {

  state = EEPROM.read(eeStateAddr);
  next = EEPROM.read(eeNextStateAddr);
  last = state;
  currentBrightness = EEPROM.read(eeCurrBrightnessAddr);
  
  if (state < 0 || state > STATE_MAX_STATE) {
    state = STATE_DEFAULT;
    next = STATE_DEFAULT;
    currentBrightness = BRIGHTNESS_DEFAULT;
  }
}

void changeStateToThen(int newState, int nextState) {

  Serial.print(stateName(state));
  Serial.print(" >> ");
  Serial.print(stateName(newState));
  Serial.print(" >> ");
  if (newState != nextState) {
    Serial.println(stateName(nextState));
  } else {
    Serial.println("... waiting");
  }
  
  last = state;
  state = newState;
  next = nextState;
  targetBrightness = stateBrightness(nextState);
  EEPROM.write(eeStateAddr, state);
  EEPROM.write(eeNextStateAddr, next);
  EEPROM.commit();
}

String stateName(int state) {
  switch (state) {
    case STATE_OFF:         return "OFF";         break;
    case STATE_ON:          return "ON";          break;
    case STATE_IDLE:        return "IDLE";        break;
    case STATE_FADING_UP:   return "FADING_UP";   break;
    case STATE_FADING_DOWN: return "FADING_DOWN"; break;
    default:      
      return "stateName:  ERROR NOT FOUND (" + String(state) + ")";  
      break;
  }
}

int stateBrightness(int state) {
  switch (state) {
    case STATE_OFF:         return BRIGHTNESS_OFF;    break;
    case STATE_ON:          return BRIGHTNESS_ON;     break;
    case STATE_IDLE:        return BRIGHTNESS_IDLE;   break;
    case STATE_FADING_UP:   return BRIGHTNESS_OFF;    break;
    case STATE_FADING_DOWN: return BRIGHTNESS_IDLE;   break;
    default:      
      return -1;  
      break;
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

int stripFadeFromTo(int from, int to) {
  if (from < to) {
    redStrip(from++);
  } else {
    redStrip(from--);
  }
  EEPROM.write(eeCurrBrightnessAddr, from);
  EEPROM.commit();
  return from;
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

