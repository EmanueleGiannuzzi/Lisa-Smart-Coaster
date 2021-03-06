#include <Arduino.h>

#include <EEPROM.h>
#include <Ticker.h>
#include <avdweb_Switch.h>
#include <Adafruit_NeoPixel.h>

#define TIME_DIVIDER 1 //Debug purpose

#define BUTTON_PIN 6
#define PIXELS_PIN 3
#define NUMPIXELS 8

#define BRIGHTNESS 60 //0-255
#define TURN_OFF_DELAY 120 // seconds

#define DEFAULT_TIMER_SETTING 0
#define CUP_UP_DELAY 1000
const int DEFAULT_OFF_COLOR[3] = {0, 0, 0};

#define EEPROM_ADDRESS 0
#define FLASH_RATE 500 //on/off each FLASH_RATE millisecond
#define STEP_PER_SEC 1
#define COLOR_UPDATE_FREQ 50 //millis


const int timerSettingsSize = 6;
int currentTimerSetting = DEFAULT_TIMER_SETTING;
int timerSettings[] = {15, 30, 45, 60, 90, 120};

Switch button = Switch(BUTTON_PIN);
Adafruit_NeoPixel pixels(NUMPIXELS, PIXELS_PIN, NEO_GRB + NEO_KHZ800);

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



void setAllColor(uint8_t r, uint8_t g, uint8_t b) {
  for(int i=0; i<NUMPIXELS; i++) {
    pixels.setPixelColor(i, pixels.Color(r, g, b));
  }
  pixels.show();
}

void startAnimation() {

  for(int i=0; i<NUMPIXELS; i++) {
    pixels.setPixelColor(i, pixels.Color(0, 255, 0));
    pixels.show(); 
    delay(500);
  }

  setAllColor(DEFAULT_OFF_COLOR[0], DEFAULT_OFF_COLOR[1], DEFAULT_OFF_COLOR[2]);
}

void flashTick(){
  bool flashOn = onOffPattern[currentOnOff];
  if(flashOn) {
    setAllColor(flashOnRed, flashOnGreen, flashOnBlue);
  }
  else {
    setAllColor(flashOffRed, flashOffGreen, flashOffBlue);
  }
  currentOnOff++;
  if(currentOnOff >= patternSize) {
    currentOnOff = 0;
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

  delete onOffPattern;
  onOffPattern = new boolean[_patternSize];
  memcpy(onOffPattern, pattern, _patternSize);
  
  patternSize = _patternSize;

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

  float weight = 1.0f;

  switch (colorStage) {
    case 0://blue
      blue = 255;
      green = 0;
      red = 0;

      // weight = 0.5;
      break;
    case 1://blue->purple
      blue = 255;
      green = 0;
      red = currentStageProgress * 255;

      // weight = 2.0f;
      break;
    case 2://purple->red
      blue =  255 - (currentStageProgress * 255);
      green = 0;
      red = 255;

      // weight = 2.0f;
      break;
    case 3://red
      blue = 0;
      green = 0;
      red = 255;
      
      // weight = 0.5f;
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

  currentStageProgress += (1.0f / steps) / weight;
  
  if(currentStageProgress >= 1.0f) {
    if(colorStage < 4) {
      colorStage++;
    }
    currentStageProgress = 0.0f;
  }

  setAllColor(red, green, blue);
}

Ticker ledTicker(ledLoop, COLOR_UPDATE_FREQ, 0, MILLIS);

void turnOff() {
  Serial.println("OFF");
  ledTicker.stop();
  setAllColor(0, 0, 0);
  pixels.clear();
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
  turnOffTicker = new Ticker(turnOff, offTime, 1, MILLIS);
  turnOffTicker->start();
}

void onCupUp() {
  turnOffTicker->stop();
  Serial.println("Cup up");
  isCupDown = false;

  ledTicker.stop();
  setAllColor(DEFAULT_OFF_COLOR[0], DEFAULT_OFF_COLOR[1], DEFAULT_OFF_COLOR[2]);
}

Ticker cupUpTicker(onCupUp, CUP_UP_DELAY, 1, MILLIS);

void onCupDown() {
  setAllColor(0, 0, 255);
  Serial.println("Cup down");
  isCupDown = true;
  cupUpTicker.stop();
  startLedTimer();
}

void flashSetting() {
  int currentSetting = currentTimerSetting + 1;
  static const int pauseSize = 4; 
  static const int onColor[3] = {0, 255, 0};

  int size = (currentSetting * 2) + pauseSize;
  bool* pattern = new bool[size];
  for(int i = 0; i < (currentSetting * 2); i+=2) {
    pattern[i] = false;
    pattern[i+1] = true;
  }

  for(int i = (currentSetting * 2); i < size; ++i) {
    pattern[i] = false;
  }

  startFlash(pattern, size, onColor);
  delete pattern;
}

void onEnterConfigMode() {
  Serial.print("Config mode ");
  Serial.println(currentTimerSetting + 1);
  isInConfigMode = true;

  isCupDown = false;
  ledTicker.stop();
  cupUpTicker.stop();
  if(turnOffTicker != nullptr) {
    turnOffTicker->stop();
  }
  setAllColor(0, 0, 0);
  pixels.clear();

  flashSetting();
}

void onExitConfigMode() {
  EEPROM.write(EEPROM_ADDRESS, currentTimerSetting);
  isInConfigMode = false;
  stopFlash();

  setAllColor(255, 0, 0);
  delay(400);
  setAllColor(0, 255, 0);
  delay(400);

  onCupUp();
}

void onNextTimerSelection() {
  currentTimerSetting++;
  if(currentTimerSetting >= timerSettingsSize) {
    currentTimerSetting = 0;
  }

  flashSetting();

  Serial.print("Current timer selection ");
  Serial.println(currentTimerSetting + 1);
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
  if(!isCupDown && !isInConfigMode) {
    onEnterConfigMode();
  }
}

void onSingleClick(void* param) {
  if(isInConfigMode) {
    onNextTimerSelection();
  }
}


void setup() {
  Serial.begin(9600);

  currentTimerSetting = EEPROM.read(EEPROM_ADDRESS);
  
  button.setLongPressCallback(onLongPress);
  button.setReleasedCallback(onReleased);
  button.setDoubleClickCallback(onDoubleClick);
  button.setSingleClickCallback(onSingleClick);

  pixels.begin(); 
  pixels.setBrightness(BRIGHTNESS);
  pixels.clear();
  startAnimation();
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