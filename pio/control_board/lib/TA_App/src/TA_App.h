#pragma once
#include <stdint.h>
#include <TA_Protocol.h>
#include "TA_Actuators.h"
#include "TA_Sensors.h"
#include "TA_Controller.h"
#include "TA_CommsBoard.h"
#include "TA_StateBoard.h"
#include "TA_Input.h"
// Display optional
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <TA_Display.h>

namespace trailair { namespace app {

// Minimal orchestrator: owns subsystems and wires them together.
class App {
public:
  struct Pins {
    uint8_t btnLeft;
    uint8_t btnDown; 
    uint8_t btnUp;
    uint8_t btnRight;
    
    Pins() : btnLeft(0), btnDown(0), btnUp(0), btnRight(0) {}
    Pins(uint8_t l, uint8_t d, uint8_t u, uint8_t r) : btnLeft(l), btnDown(d), btnUp(u), btnRight(r) {}
  };

  // Optionally pass a display to enable on-board UI rendering, and button pins
  explicit App(Adafruit_SSD1306* disp = nullptr, const Pins& buttonPins = Pins())
    : disp_(disp),
      ui_(disp ? new trailair::display::DisplayController(*disp) : nullptr),
      hasButtons_(buttonPins.btnLeft != 0),
      buttons_(hasButtons_ ? new trailair::input::ButtonManager({buttonPins.btnLeft, buttonPins.btnDown, buttonPins.btnUp, buttonPins.btnRight}) : nullptr) {}
  ~App() { delete ui_; delete buttons_; }

  void begin();
  void loop();

  // Expose some state (optional)
  trailair::controller::PressureController& controller() { return controller_; }
  trailair::comms::BoardLink& comms() { return comms_; }
  trailair::stateboard::StateBoard& state() { return state_; }

private:
  static void onRequestStatic_(void* ctx, const trailair::protocol::Request& req);
  void onRequest_(const trailair::protocol::Request& req);
  
  static void onButtonStatic_(void* ctx, const trailair::input::ButtonEvent& ev);
  void onButton_(const trailair::input::ButtonEvent& ev);

  // Subsystems
  ta::act::Actuators actuators_{};
  trailair::sensors::PressureFilter pressure_{};
  trailair::controller::PressureController controller_{};
  trailair::comms::BoardLink comms_{};
  trailair::stateboard::StateBoard state_{};

  // Display (optional)
  Adafruit_SSD1306* disp_ = nullptr;
  trailair::display::DisplayController* ui_ = nullptr;

  // Buttons (optional)
  bool hasButtons_ = false;
  trailair::input::ButtonManager* buttons_ = nullptr;

  // Timing
  uint32_t lastStatusMs_ = 0;
  static constexpr uint32_t STATUS_INTERVAL_MS_ = 1000;
};

}} // namespace trailair::app
