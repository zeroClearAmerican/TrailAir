#include <stdint.h>
#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <TA_App.h>

// Display
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 32
#define OLED_RESET     -1
#define SCREEN_ADDRESS 0x3C
Adafruit_SSD1306 d_(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

ta::app::App app(&d_);

void setup() {
  Serial.begin(115200);
  app.begin();
}

void loop() {
  app.loop();
}