#include "TA_App.h"
#include <Arduino.h>

namespace trailair { namespace app {

void App::begin() {
  Serial.println("  [App] Initializing actuators...");
  // Actuators
  actuators_.begin({9, 10});
  
  Serial.println("  [App] Initializing pressure sensor...");
  // Sensors
  pressure_.begin(3, 10, 0.5f);
  
  Serial.println("  [App] Initializing controller...");
  // Controller
  trailair::controller::ControllerConfig cfg; // defaults for now
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
  
  // Buttons (optional)
  if (buttons_ && hasButtons_) {
    Serial.println("  [App] Initializing buttons...");
    buttons_->begin();
    buttons_->subscribe(&App::onButtonStatic_, this);
  } else {
    Serial.println("  [App] No buttons configured");
  }
  
  Serial.println("  [App] Initialization complete");
}

void App::onRequestStatic_(void* ctx, const trailair::protocol::Request& req) {
  static_cast<App*>(ctx)->onRequest_(req);
}

void App::onButtonStatic_(void* ctx, const trailair::input::ButtonEvent& ev) {
  static_cast<App*>(ctx)->onButton_(ev);
}

void App::onRequest_(const trailair::protocol::Request& req) {
  using RK = trailair::protocol::Request::Kind;
  switch (req.kind) {
    case RK::Idle:
      controller_.cancel();
      controller_.clearError();
      break;
    case RK::Start:
      controller_.startSeek(req.targetPSI);
      break;
    case RK::Manual:
      if (req.manualMode == trailair::protocol::ManualMode::Vent) controller_.manualVent(true);
      else if (req.manualMode == trailair::protocol::ManualMode::Air) controller_.manualAirUp(true);
      break;
    case RK::Ping:
      // no-op
      break;
  }
}

void App::onButton_(const trailair::input::ButtonEvent& ev) {
  state_.onButton(ev, controller_);
}

void App::loop() {
  uint32_t now = millis();
  
  // Service buttons
  if (buttons_ && hasButtons_) {
    buttons_->service();
  }
  
  // Service comms
  comms_.service();
  // Sensor + controller
  float psi = pressure_.readPsi();
  controller_.update(now, psi);
  // Periodic status to remote (only if paired)
  if (comms_.isPaired() && (now - lastStatusMs_ >= STATUS_INTERVAL_MS_)) {
    if (controller_.getState() == trailair::controller::ControllerState::Error) {
      comms_.sendError(controller_.getErrorByte());
    } else {
      comms_.sendStatus(controller_.getStatusCharacter(), controller_.getCurrentPSI());
    }
    lastStatusMs_ = now;
  }
  // Board UI state and render if display present
  state_.update(now, controller_, comms_);
  if (ui_) {
    trailair::display::DisplayModel dm;
    state_.buildDisplayModel(dm, controller_, comms_, now);
    ui_->render(dm);
  }
  // Small delay
  delay(10);
}

}} // namespace trailair::app
