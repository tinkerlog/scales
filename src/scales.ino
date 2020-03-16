/*
 * scales v0.1
 *
 * Adafruit feather M0 Basic
 *          +------------+
 *  WHT ----o RST        |
 *      ----o 3V3        |
 *      ----o VREF       |
 *  BRN ----o GND        |
 *      ----o A0     BAT o----
 *      ----o A1      EN o----
 *      ----o A2    VBUS o----
 *      ----o A3      13 o---- LED
 *      ----o A4      12 o----
 *      ----o A5      11 o----
 *      ----o 24      10 o---- BTN_TARA, GRN
 *      ----o 23       9 o---- VBAT Check (A7)
 *      ----o 22       6 o---- LOADCELL DOUT
 *      ----o  0       5 o---- LOADCELL CLK
 *      ----o  1      21 o---- SCL to Adafruit OLED
 *      ----o GND     20 o---- SDA to Adafruit OLED
 *          +------------+
 *
 *
 */
#include <SPI.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <Adafruit_SleepyDog.h>
#include <HX711.h>
#include "nasa20pt.h"


#define VERSION "scales v0.1"

#define LED 13
#define BTN_TARA 10

#define LOADCELL_DOUT 6
#define LOADCELL_CLK 5
#define LOADCELL_CALIBRATION 3320

#define VBAT A7
#define VBAT_REPORT_DELAY 5000

#define WATCHDOG_SLEEP 2000
#define TIMEOUT 60000
#define WEIGHT_DELTA 1.0

#define VBAT_DISPLAY_X 2
#define VBAT_DISPLAY_Y 25

#define VBAT_LOW 3.0

#define DISPLAY

HX711 scale;
Adafruit_SSD1306 display = Adafruit_SSD1306(128, 32, &Wire);

//                  0   1  2  3  4  5  6  7  8  9
int deltaPos[] =  { 0, 16, 0, 2, 2, 1, 0, 2, 0, 0};
float currentVBat = 0.0;
float currentWeight = 0.0;
float oldWeight = 0.0;

unsigned long nextVBatReport = 0;
unsigned long timeOutAt = 0;

bool ledOn = false;

void setup() {

  // setup outputs
  pinMode(BTN_TARA, INPUT_PULLUP);
  pinMode(LED, OUTPUT);

  Serial.begin(9600);
  delay(500);

  Serial.println(VERSION);

  // setup scale (HX711)
  Serial.println("setting up HX711 ...");
  scale.begin(LOADCELL_DOUT, LOADCELL_CLK);
  if (!scale.wait_ready_timeout(1000)) {
    Serial.println("HX711 not found!");
    while (true) {}
  }
  scale.set_scale(LOADCELL_CALIBRATION); // set calibration factor
  scale.tare(); // reset the scale to 0
  Serial.println("HX711 ready.");

  // setup DISPLAYplay
  #ifdef DISPLAY
    Serial.println("settings up display ...");
    display.begin(SSD1306_SWITCHCAPVCC, 0x3C); // Address 0x3C for 128x32
    Serial.println("display ready.");
    display.display();
    delay(500);

    display.clearDisplay();
    display.display();
    display.setTextSize(1);
    display.setTextColor(SSD1306_WHITE);
    display.print(VERSION);
    display.display();
    delay(500);
  #endif

  timeOutAt = millis() + TIMEOUT;
}

void loop() {
  delay(200);
  currentVBat = checkVBat(currentVBat);
  currentWeight = checkScales();

  clearScreen();

  unsigned long now = millis();
  if (now > timeOutAt) {
    displayOff();
    shutDown();
  }

  if (abs(currentWeight - oldWeight) > WEIGHT_DELTA) {
    oldWeight = currentWeight;
    timeOutAt = now + TIMEOUT;
    Serial.println("trigger!");
  }

  if (currentVBat < VBAT_LOW) {
    displayLow();
    shutDown();
  }

  displayVBat(currentVBat);
  displayWeight(currentWeight);

  flushDisplay();

  if (!digitalRead(BTN_TARA)) {
    Serial.println("--- TARA");
    timeOutAt = now + TIMEOUT;
    scale.tare();
  }

  ledOn = !ledOn;
  digitalWrite(LED, ledOn ? HIGH : LOW);

}

void shutDown() {
  digitalWrite(LED, LOW);
  clearScreen();
  flushDisplay();
  switchOff();
}

void flushDisplay() {
  #ifdef DISPLAY
    display.display();
  #endif
}

void clearScreen() {
  #ifdef DISPLAY
    display.fillRect(0, 0, display.width(), display.height(), 0);
  #endif
}

int getCursorPos(String str) {
  int16_t  x1, y1;
  uint16_t w, h;
  int cursorX = display.width() - 5;
  display.getTextBounds(str, 0, 0, &x1, &y1, &w, &h);
  char lastChr = str.charAt(str.length()-1);
  int pos = deltaPos[lastChr-48];
  return cursorX - (w + pos);
}

float checkVBat(float vbat) {
  if (millis() > nextVBatReport) {
    nextVBatReport = millis() + VBAT_REPORT_DELAY;
    vbat = analogRead(VBAT);
    vbat *= 2;    // we divided by 2, so multiply back
    vbat *= 3.3;  // Multiply by 3.3V, our reference voltage
    vbat /= 1024; // convert to voltage
    Serial.print("vbat: ");
    Serial.println(vbat);
  }
  return vbat;
}

float checkScales() {
  float weight = scale.get_units();
  if (weight < 0) {
    weight = 0.0;
  }
  return weight;
}

void displayOff() {
  Serial.println("timeout!");
  #ifdef DISPLAY
    display.setFont(&nasalization_rg20pt7b);
    display.setCursor(2, 30);
    display.print("off!");
    display.display();
  #endif
  delay(2000);
}

void displayLow() {
  Serial.println("battery low!");
  #ifdef DISPLAY
    display.setFont(&nasalization_rg20pt7b);
    display.setCursor(2, 30);
    display.print("low!");
    display.display();
  #endif
  delay(2000);
}

void displayVBat(float vbat) {
  #ifdef DISPLAY
    String batStr = String(vbat, 2);
    display.setFont();
    display.setCursor(VBAT_DISPLAY_X, VBAT_DISPLAY_Y);
    display.print(batStr);
  #endif
}

void displayWeight(float weight) {
  String str = String(weight, 1);
  Serial.print("weight: ");
  Serial.println(weight);
  #ifdef DISPLAY
    display.setFont(&nasalization_rg20pt7b);
    int cursorX = getCursorPos(str);
    display.setCursor(cursorX, 30);
    display.print(str);
  #endif
}

void switchOff() {
  Serial.println("switching off!");
  while (true) {
    Watchdog.sleep(WATCHDOG_SLEEP);
    digitalWrite(LED, HIGH);
    delay(100);
    digitalWrite(LED, LOW);
  }
}
