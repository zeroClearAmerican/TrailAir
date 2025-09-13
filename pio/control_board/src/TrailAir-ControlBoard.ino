#include <stdint.h> // for uint8_t type
#include <Arduino.h>
#include <WiFi.h>
#include <esp_wifi.h>
#include <esp_now.h>
#include <deque>
#include <math.h>
#include <string.h>
#include <Preferences.h>
#include <TA_Protocol.h>
#include "TA_Actuators.h"
#include "TA_Sensors.h"
#include "TA_Controller.h"
#include "TA_CommsBoard.h"
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include "TA_Display.h"
#include "TA_DisplayIcons.h"
#include "TA_Input.h"
#include "TA_StateBoard.h"

// Remove old peer/address arrays & large logic sections after migration.

// Globals (new)
ta::act::Actuators actuators;
ta::sensors::PressureFilter pressure;
ta::ctl::Controller controller;
ta::comms::BoardLink boardLink;
// Board UI state
static ta::stateboard::StateBoard boardState;

// Timing
static uint32_t lastStatusMs = 0;
static const uint32_t STATUS_INTERVAL_MS = 1000;

void onCommand(void* /*ctx*/, const ta::comms::Command& cmd) {
  using namespace ta::ctl;
  switch (cmd.type) {
    case ta::comms::CmdType::Idle:
      controller.cancel();
      controller.clearError();
      Serial.println("CMD Idle");
      break;
    case ta::comms::CmdType::Seek:
      controller.startSeek(cmd.targetPsi);
      Serial.printf("CMD Seek %.1f\n", cmd.targetPsi);
      break;
    case ta::comms::CmdType::Manual:
      if (cmd.raw == 0) {
        controller.manualVent(true);
      } else if (cmd.raw == 255) {
        controller.manualAirUp(true);
      } else {
        // Unknown manual code; ignore
      }
      // Manual stop handled by an Idle command from remote
      break;
    case ta::comms::CmdType::Ping:
      // No action; next status covers it
      break;
    default:
      break;
  }
}

#pragma region Screen Variables
// #define SCREEN_WIDTH 128 // OLED display width, in pixels
// #define SCREEN_HEIGHT 32 // OLED display height, in pixels

// // Declaration for an SSD1306 display connected to I2C (SDA, SCL pins)
// #define OLED_RESET     -1 // Reset pin # (or -1 if sharing Arduino reset pin)
// #define SCREEN_ADDRESS 0x3C ///< See datasheet for Address; 0x3D for 128x64, 0x3C for 128x32
// Adafruit_SSD1306 d_(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);
// ta::display::TA_Display ui(d_);
#pragma endregion

// #pragma region Button Variables
// // GPIO pins for each button
// #define BTN_LEFT_PIN   20  // retry / wake
// #define BTN_DOWN_PIN   21
// #define BTN_UP_PIN     5
// #define BTN_RIGHT_PIN  4

// // Buttons (order: Left, Down, Up, Right to match StateBoard expectation)
// static ta::input::Buttons buttons({ BTN_LEFT_PIN, BTN_DOWN_PIN, BTN_UP_PIN, BTN_RIGHT_PIN });
// // Forward button callback
// static void onButtonEvent(void* /*ctx*/, const ta::input::Event& ev) {
//   boardState.onButton(ev, controller);
// }
// #pragma endregion 

void setup() {
  Serial.begin(115200);
  Serial.println("TrailAir Control Board Starting...");

  // Actuators
  actuators.begin({9, 10});

  // Sensors
  pressure.begin(3, 10, 0.5f);

  // Controller
  ta::ctl::Config cfg;
  controller.begin(&actuators, cfg);

  // Comms
  boardLink.begin();
  boardLink.setCommandCallback(&onCommand, nullptr);

  // // Display init
  // if(!ui.begin(SCREEN_ADDRESS, true)) {
  //   Serial.println(F("SSD1306 OLED screen allocation failed"));
  //   for(;;); // Don't proceed, loop forever
  // }

  // UI state
  boardState.begin();

  // Buttons
  // buttons.begin(&onButtonEvent, nullptr); // adjust if signature differs
  Serial.println("Setup complete");

  // delay(500);
  // // Wipe out logo before beginning loop
  // ui.logoWipe(ta::display::Icons::logo_bmp, 
  //   ta::display::Icons::LogoW, 
  //   ta::display::Icons::LogoH, 
  //   false, 
  //   5);
}

void loop() {
  uint32_t now = millis();

  // Service comms (pairing automatic; we no longer early-return when unpaired)
  boardLink.service();

  // Sensor + controller
  float psi = pressure.readPsi();
  controller.update(now, psi);

  // Periodic status to remote (only if paired)
  if (boardLink.isPaired() && (now - lastStatusMs >= STATUS_INTERVAL_MS)) {
    if (controller.state() == ta::ctl::State::ERROR) {
      boardLink.sendError(controller.errorByte());
    } else {
      boardLink.sendStatus(controller.statusChar(), controller.currentPsi());
    }
    lastStatusMs = now;
  }

  // Serial forget (optional)
  if (Serial.available()) {
    char c = Serial.read();
    if (c == 'f') {
      boardLink.forget();
      controller.cancel();
    }
  }

  // Buttons
  // buttons.service();

  // Board UI state update
  boardState.update(now, controller, boardLink);

  // Build & render display model
  // ta::display::DisplayModel dm;
  // boardState.buildDisplayModel(dm, controller, boardLink, now);
  // ui.render(dm);

  // Small delay to ease CPU (optional)
  delay(10);
}