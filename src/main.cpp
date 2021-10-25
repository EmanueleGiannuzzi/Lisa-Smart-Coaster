#include <Arduino.h>

#include <Ticker.h>
#include <RGBLed.h>
#include <avdweb_Switch.h>

#define RED_PIN 11
#define GREEN_PIN 10
#define BLUE_PIN 9

#define BUTTON_PIN 6

#define FLASH_RATE 1000 //on/off each FLASH_RATE millisecond

RGBLed led(RED_PIN, GREEN_PIN, BLUE_PIN, RGBLed::COMMON_CATHODE);
Switch button = Switch(BUTTON_PIN);


Ticker* led_ticker;

class FlashTicker {
  int red = 0;
  int green = 0;
  int blue = 0;

  bool flashOn = false;

  Ticker* ticker;
  public:
  // FlashTicker() : ticker{[this](){this->tick();}, FLASH_RATE, 0, MILLIS} {
  //   // ticker.start();
  // }

  FlashTicker() {
    ticker = new Ticker([this](){this->tick();}, 1000, 0, MILLIS);
  }

  void start() {
    ticker->start();
  }

  void stop() {
    ticker->stop();
  }
  
  void tick() {
    redFlashOn = !redFlashOn;
    if(redFlashOn) {
      led.setColor(red ? 255 : 0, green ? 255 : 0, blue ? 255 : 0);
    }
    else {
    led.setColor(0, 0, 0);
    }
  }
};
FlashTicker flashTicker;

bool isCupDown = false;
bool isInConfigMode = false;

const int defaultTimerSetting = 1;
const int timerSettingsSize = 6;
int currentTimerSetting = defaultTimerSetting;
int timerSettings[] = {15, 30, 45, 60, 90, 120};

int color_stage = 0;//0 = blue; 1 = blue->purple; 2 = purple->red; 3 = red flashing

void ledLoop() {
  static const int steps = 100;//TODO

  static int red = 0;
  static int green = 0;
  static int blue = 0;
  static float currentStageProgress = 0;

  static unsigned long lastMillis = millis();

  static bool flashOn = false;

  switch (color_stage) {
    case 0:
      blue = 255;
      green = 0;
      red = 0;
      break;
    case 1:
      blue = 255;
      green = 0;
      red = currentStageProgress * 255;
      break;
    case 2://purple->red
      blue =  255 - (currentStageProgress * 255);
      green = 0;
      red = 255;
      break;
    case 3://red flashing
      blue = 0;
      green = 0;
      if(millis() - lastMillis > FLASH_RATE) {
        lastMillis = millis();
        flashOn = !flashOn;
      }
      red = flashOn ? 255 : 0;
      break;
    case 4://green flashing
      blue = 0;//TODO: Flash x times
      if(millis() - lastMillis > FLASH_RATE) {
        lastMillis = millis();
        flashOn = !flashOn;
      }
      green = flashOn ? 255 : 0;
      red = 0;
      break;
    default:
      break;
  }

  currentStageProgress += (1.0f / steps);//TODO
  
  if(currentStageProgress == 1.0f) {
    if(color_stage < 3) {
      color_stage++;
    }
    currentStageProgress = 0.0f;
  }

  led.setColor(red, green, blue);
}

void startTimer(uint32_t millis) {
  if(led_ticker != nullptr) {
    led_ticker->stop();
    delete led_ticker;
  }

  led_ticker = new Ticker(ledLoop, millis, 0, MILLIS);
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
  currentTimerSetting++;
  if(currentTimerSetting >= timerSettingsSize) {
    currentTimerSetting = 0;
  }

  //TODO: save to eeprom

  Serial.print("Current timer selection ");
  Serial.println(currentTimerSetting);
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
  if(led_ticker != nullptr) {
    led_ticker->update();
  }

}