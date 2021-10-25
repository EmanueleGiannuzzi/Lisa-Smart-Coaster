#include <Arduino.h>

#include <Ticker.h>
#include <RGBLed.h>
#include <avdweb_Switch.h>

#define RED_PIN 11
#define GREEN_PIN 10
#define BLUE_PIN 9

#define BUTTON_PIN 6

#define FLASH_RATE 500 //on/off each FLASH_RATE millisecond

#define TIME_DIVIDER 60 //Debug purpose
#define STEP_PER_SEC 1

#define COLOR_UPDATE_FREQ 500 //millis

#define DEFAULT_TIMER_SETTING 0

#define CUP_UP_DELAY 1000

#define TURN_OFF_DELAY 15 // seconds

const int DEFAULT_OFF_COLOR[3] = {0, 0, 0};

const int timerSettingsSize = 6;
unsigned int currentTimerSetting = DEFAULT_TIMER_SETTING;
unsigned int timerSettings[] = {15, 30, 45, 60, 90, 120};

RGBLed led(RED_PIN, GREEN_PIN, BLUE_PIN, RGBLed::COMMON_CATHODE);
Switch button = Switch(BUTTON_PIN);

#pragma region FlashTicker

int flashOnRed = 0;
int flashOnGreen = 0;
int flashOnBlue = 0;

int flashOffRed = 0;
int flashOffGreen = 0;
int flashOffBlue = 0;

bool* onOffPattern;
unsigned int patternSize = 0;
unsigned int currentOnOff = 0;

void flashTick(){
  
  // for(int i = 0; i < patternSize; ++i) {
  //   Serial.print(onOffPattern[i]);
  //   Serial.print(" ");
  // }
  // Serial.println();

  bool flashOn = onOffPattern[currentOnOff];
  if(flashOn) {
    Serial.println("ON");
    led.setColor(flashOnRed, flashOnGreen, flashOnBlue);
  }
  else {
    Serial.println("OFF");
    led.setColor(flashOffRed, flashOffGreen, flashOffBlue);
  }
  currentOnOff++;
  if(currentOnOff >= patternSize) {
    currentOnOff = 0;
    
    Serial.println("NEXT");
  }
}

Ticker flashTicker(flashTick, FLASH_RATE, 0, MILLIS);
int defafultOff[3] = {0,0,0};

void stopFlash() {
  flashTicker.stop();
}

void startFlash(bool* pattern, int _patternSize, const int onColor[3], const int offColor[3] = defafultOff) {
  stopFlash();
  flashOnRed = onColor[0];
  flashOnGreen = onColor[1];
  flashOnBlue = onColor[2];

  flashOffRed = offColor[0];
  flashOffGreen = offColor[1];
  flashOffBlue = offColor[2];

  memcpy(onOffPattern, pattern, patternSize);
  
  patternSize = _patternSize;

  for(int i = 0; i < patternSize; ++i) {
    Serial.print(onOffPattern[i]);
    Serial.print(" ");
  }
  Serial.println();

  flashTicker.start();
}

#pragma endregion


bool isCupDown = false;
bool isInConfigMode = false;

int colorStage = 0;//0 = blue; 1 = blue->purple; 2 = purple->red; 3 = red; 4 = red flashing
float currentStageProgress = 0;
int steps = 0;

void ledLoop() {
  static int red = 0;
  static int green = 0;
  static int blue = 0;

  static unsigned long lastMillis = millis();
  static bool flashOn = false;

  switch (colorStage) {
    case 0://blue
      blue = 255;
      green = 0;
      red = 0;
      break;
    case 1://blue->purple
      blue = 255;
      green = 0;
      red = currentStageProgress * 255;
      break;
    case 2://purple->red
      blue =  255 - (currentStageProgress * 255);
      green = 0;
      red = 255;
      break;
    case 3://red
      blue = 0;
      green = 0;
      red = 255;
      break;
    case 4://red flashing
      blue = 0;
      green = 0;
      if(millis() - lastMillis > FLASH_RATE) {
        lastMillis = millis();
        flashOn = !flashOn;
      }
      red = flashOn ? 255 : 0;
      break;
    default:
      break;
  }

  currentStageProgress += 1.0f / steps;
  
  if(currentStageProgress >= 1.0f) {
    if(colorStage < 4) {
      colorStage++;
    }
    currentStageProgress = 0.0f;
  }

  led.setColor(red, green, blue);
}

Ticker ledTicker(ledLoop, COLOR_UPDATE_FREQ, 0, MILLIS);

void turnOff() {
  Serial.println("OFF");
  ledTicker.stop();
  led.off();
}
Ticker* turnOffTicker;

void startLedTimer() {
  currentStageProgress = 0;
  colorStage = 0;

  ledTicker.stop();
  steps = ((1000 / COLOR_UPDATE_FREQ) * timerSettings[currentTimerSetting] * 60) / 4 / TIME_DIVIDER;
  ledTicker.start();

  delete turnOffTicker;
  uint32_t offTime = (((timerSettings[currentTimerSetting] * 60) / TIME_DIVIDER) + TURN_OFF_DELAY) * 1000L;
  Serial.println(offTime);
  turnOffTicker = new Ticker(turnOff, offTime, 1, MILLIS);
  turnOffTicker->start();
}

void onCupUp() {
  turnOffTicker->stop();
  Serial.println("Cup up");
  isCupDown = false;

  ledTicker.stop();
  led.setColor(DEFAULT_OFF_COLOR[0], DEFAULT_OFF_COLOR[1], DEFAULT_OFF_COLOR[2]);
}

Ticker cupUpTicker(onCupUp, CUP_UP_DELAY, 1, MILLIS);

void onCupDown() {
  led.setColor(RGBLed::BLUE);
  Serial.println("Cup down");
  isCupDown = true;
  cupUpTicker.stop();
  startLedTimer();
}

void flashSetting() {
  unsigned int currentSetting = currentTimerSetting + 1;
  static const int pauseSize = 6; 
  static const int onColor[3] = {0, 255, 0};

  unsigned int size = (currentSetting * 2) + pauseSize;
  bool pattern[size];
  unsigned int i;
  for(i = 0; i < (currentSetting * 2); i+=2) {
    pattern[i] = false;
    pattern[i+1] = true;
  }
  for( ; i < size; ++i) {
    pattern[i] = false;
  }

  startFlash(pattern, size, onColor);
}

void onEnterConfigMode() {
  Serial.println("Config mode");
  isInConfigMode = true;

  isCupDown = false;
  ledTicker.stop();
  cupUpTicker.stop();
  if(turnOffTicker != nullptr) {
    turnOffTicker->stop();
  }
  led.off();

  flashSetting();
}

void onExitConfigMode() {
  //TODO: save currentTimerSetting to eeprom
  isInConfigMode = false;
  stopFlash();

  onCupUp();
}

void onNextTimerSelection() {
  currentTimerSetting++;
  if(currentTimerSetting >= timerSettingsSize) {
    currentTimerSetting = 0;
  }

  flashSetting();

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
  if(!isInConfigMode && isCupDown) {
    cupUpTicker.start();
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

  led.setColor(DEFAULT_OFF_COLOR[0], DEFAULT_OFF_COLOR[1], DEFAULT_OFF_COLOR[2]);
}


void loop() {
  button.poll();

  ledTicker.update();
  cupUpTicker.update();
  flashTicker.update();
  if(turnOffTicker != nullptr) {
    turnOffTicker->update();
  }
}