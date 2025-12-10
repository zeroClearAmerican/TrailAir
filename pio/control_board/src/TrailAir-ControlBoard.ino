#include <stdint.h>
#include <Arduino.h>
#include <WiFi.h>
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
  // CRITICAL: Set actuator pins to OUTPUT/LOW immediately to prevent spurious activation
  pinMode(9, OUTPUT);
  pinMode(10, OUTPUT);
  digitalWrite(9, LOW);
  digitalWrite(10, LOW);
  
  Serial.begin(115200);
  delay(100);
  
  Serial.println("\n\n=== TrailAir Control Board Starting ===");
  
  app.begin();
  
  // Print MAC address AFTER WiFi initialization to avoid double-init
  uint8_t mac[6];
  WiFi.macAddress(mac);
  Serial.printf("Board MAC: %02X:%02X:%02X:%02X:%02X:%02X\n",
                mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
  
  Serial.println("=== Control Board Ready ===\n");
}

void loop() {
  app.loop();
}