# 1 "C:\\Users\\Mason\\AppData\\Local\\Temp\\tmpk9zycx1i"
#include <Arduino.h>
# 1 "C:/Users/Mason/3D Objects/Code/TrailAir/pio/remote/src/TrailAir-Remote.ino"
#include <SPI.h>
#include <Wire.h>
#include <TA_RemoteApp.h>
#include <TA_Display.h>
#include <Adafruit_SSD1306.h>

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 32
#define OLED_RESET -1
#define SCREEN_ADDRESS 0x3C
Adafruit_SSD1306 d_(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);


#define BTN_LEFT_PIN 10
#define BTN_DOWN_PIN 9
#define BTN_UP_PIN 8
#define BTN_RIGHT_PIN 20

static ta::app::RemoteApp app({ BTN_LEFT_PIN, BTN_DOWN_PIN, BTN_UP_PIN, BTN_RIGHT_PIN, 2 }, &d_);
void setup();
void loop();
#line 21 "C:/Users/Mason/3D Objects/Code/TrailAir/pio/remote/src/TrailAir-Remote.ino"
void setup() {
  Serial.begin(115200);
  app.begin();
}

void loop() {
  app.loop();
}