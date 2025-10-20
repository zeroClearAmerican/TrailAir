#pragma once
#include <stdint.h>
#include <TA_Protocol.h>
#include "TA_Actuators.h"
#include "TA_Sensors.h"
#include "TA_Controller.h"
#include "TA_CommsBoard.h"
#include "TA_StateBoard.h"
// Display optional
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include "TA_Display.h"

namespace ta { namespace app {

// Minimal orchestrator: owns subsystems and wires them together.
class App {
public:
  // Optionally pass a display to enable on-board UI rendering
  explicit App(Adafruit_SSD1306* disp = nullptr) : disp_(disp), ui_(disp ? new ta::display::TA_Display(*disp) : nullptr) {}
  ~App() { delete ui_; }

  void begin();
  void loop();

  // Expose some state (optional)
  ta::ctl::Controller& controller() { return controller_; }
  ta::comms::BoardLink& comms() { return comms_; }
  ta::stateboard::StateBoard& state() { return state_; }

private:
  static void onRequestStatic_(void* ctx, const ta::protocol::Request& req);
  void onRequest_(const ta::protocol::Request& req);

  // Subsystems
  ta::act::Actuators actuators_{};
  ta::sensors::PressureFilter pressure_{};
  ta::ctl::Controller controller_{};
  ta::comms::BoardLink comms_{};
  ta::stateboard::StateBoard state_{};

  // Display (optional)
  Adafruit_SSD1306* disp_ = nullptr;
  ta::display::TA_Display* ui_ = nullptr;

  // Timing
  uint32_t lastStatusMs_ = 0;
  static constexpr uint32_t STATUS_INTERVAL_MS_ = 1000;
};

}} // namespace ta::app
