#include <Arduino.h>

//#include <Ticker.h>
#include <RGBLed.h>
#include <avdweb_Switch.h>

#define RED_PIN 11
#define GREEN_PIN 10
#define BLUE_PIN 9

#define BUTTON_PIN 6

RGBLed led(RED_PIN, GREEN_PIN, BLUE_PIN, RGBLed::COMMON_CATHODE);

Switch button = Switch(BUTTON_PIN);

bool isCupDown = false;
bool isInConfigMode = false;

const int defaultTimer = 1;
const int timer_size = 6;
int currentTimer = defaultTimer;
int timers[] = {15, 30, 45, 60, 90, 120};

int color_stage = 0;//0 = blue; 1 = blue->purple; 2 = purple->red; 3 = red flashing

void ledLoop() {
  static int red = 0;
  static int blue = 0;
  static float count = 0;
    
  if(color_stage == 0) {//OFF
    blue = (count / 100) * 255;
    red = 0;
  }
  if(color_stage == 1) {//PURPLE
    blue = 255;
    red = (count / 100) * 255;
  }
  if(color_stage == 2) {//BLUE
    blue =  255 - ((count / 100) * 255);
    red = 255;
  }
  if(color_stage == 3) {
    blue = 0;
    red = 255 - ((count / 100) * 255);
  }

  count++;
  
  if(count == 100) {
    color_stage++;
    count = 0;
    if(color_stage > 3) {
      color_stage = 0;
    }
  }

  led.setColor(red, 0, blue);

  // Serial.print("RED ");
  // Serial.print(red);
  // Serial.print(" BLUE ");
  // Serial.println(blue);
}

void onCupDown() {
  Serial.println("Cup down");
  isCupDown = true;
}

void onCupUp() {
  Serial.println("Cup up");
  isCupDown = false;
}

void onEnterConfigMode() {
  Serial.println("Config mode");
  isInConfigMode = true;
}

void onExitConfigMode() {
  isInConfigMode = false;
}

void onNextTimerSelection() {
  currentTimer++;
  if(currentTimer >= timer_size) {
    currentTimer = 0;
  }

  //TODO: save to eeprom

  Serial.print("Current timer selection ");
  Serial.println(currentTimer);
}

void onLongPress(void* param) {
  if(!isInConfigMode) {
    onCupDown();
  }
  else {
    onExitConfigMode();
  }
}

void onReleased(void* param) {
  if(!isInConfigMode) {
    if(isCupDown) {
      onCupUp();
    }
  }
}

void onDoubleClick(void* param) {
    if(!isCupDown) {
      onEnterConfigMode();
    }
}

void onSingleClick(void* param) {
  if(isInConfigMode) {
    onNextTimerSelection();
  }
}


void setup() {
  //TODO: get current timer from eeprom
  
  Serial.begin(9600);
  button.setLongPressCallback(onLongPress);
  button.setReleasedCallback(onReleased);
  button.setDoubleClickCallback(onDoubleClick);
  button.setSingleClickCallback(onSingleClick);
}


void loop() {
  button.poll();
}