#include "TA_App.h"
#include <Arduino.h>

namespace ta { namespace app {

void App::begin() {
  Serial.println("  [App] Initializing actuators...");
  // Actuators
  actuators_.begin({9, 10});
  
  Serial.println("  [App] Initializing pressure sensor...");
  // Sensors
  pressure_.begin(3, 10, 0.5f);
  
  Serial.println("  [App] Initializing controller...");
  // Controller
  ta::ctl::Config cfg; // defaults for now
  controller_.begin(&actuators_, cfg);
  
  Serial.println("  [App] Initializing comms (ESP-NOW)...");
  // Comms
  if (!comms_.begin()) {
    Serial.println("  [App] ERROR: Comms init failed!");
  }
  comms_.setRequestCallback(&App::onRequestStatic_, this);
  
  Serial.println("  [App] Initializing state...");
  // State
  state_.begin();
  
  // Display (optional)
  if (ui_ && disp_) {
    Serial.println("  [App] Initializing display...");
    const uint8_t SCREEN_ADDRESS = 0x3C;
    if (!ui_->begin(SCREEN_ADDRESS, true)) {
      Serial.println("  [App] WARNING: Display init failed!");
    }
  } else {
    Serial.println("  [App] No display configured");
  }
  
  Serial.println("  [App] Initialization complete");
}

void App::onRequestStatic_(void* ctx, const ta::protocol::Request& req) {
  static_cast<App*>(ctx)->onRequest_(req);
}

void App::onRequest_(const ta::protocol::Request& req) {
  using RK = ta::protocol::Request::Kind;
  switch (req.kind) {
    case RK::Idle:
      controller_.cancel();
      controller_.clearError();
      break;
    case RK::Start:
      controller_.startSeek(req.targetPsi);
      break;
    case RK::Manual:
      if (req.manual == ta::protocol::ManualCode::Vent) controller_.manualVent(true);
      else if (req.manual == ta::protocol::ManualCode::Air) controller_.manualAirUp(true);
      break;
    case RK::Ping:
      // no-op
      break;
  }
}

void App::loop() {
  uint32_t now = millis();
  // Service comms
  comms_.service();
  // Sensor + controller
  float psi = pressure_.readPsi();
  controller_.update(now, psi);
  // Periodic status to remote (only if paired)
  if (comms_.isPaired() && (now - lastStatusMs_ >= STATUS_INTERVAL_MS_)) {
    if (controller_.state() == ta::ctl::State::ERROR) {
      comms_.sendError(controller_.errorByte());
    } else {
      comms_.sendStatus(controller_.statusChar(), controller_.currentPsi());
    }
    lastStatusMs_ = now;
  }
  // Board UI state and render if display present
  state_.update(now, controller_, comms_);
  if (ui_) {
    ta::display::DisplayModel dm;
    state_.buildDisplayModel(dm, controller_, comms_, now);
    ui_->render(dm);
  }
  // Small delay
  delay(10);
}

}} // namespace ta::app
