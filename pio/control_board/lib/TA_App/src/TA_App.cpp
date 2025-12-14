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
  using trailair::input::ButtonEvent;
  using trailair::input::ButtonId;
  using trailair::input::ButtonAction;
  
  // Helper to convert protocol ButtonId to input ButtonId
  auto toInputButtonId = [](trailair::protocol::ButtonId pb) -> ButtonId {
    switch (pb) {
      case trailair::protocol::ButtonId::Left:  return ButtonId::Left;
      case trailair::protocol::ButtonId::Down:  return ButtonId::Down;
      case trailair::protocol::ButtonId::Up:    return ButtonId::Up;
      case trailair::protocol::ButtonId::Right: return ButtonId::Right;
    }
    return ButtonId::Left;
  };
  
  switch (req.kind) {
    // ========== NEW BUTTON-BASED PROTOCOL (Thin Client) ==========
    // Direct 1:1 mapping - no translation needed!
    case RK::ButtonPress: {
      ButtonId id = toInputButtonId(req.button);
      state_.onButton(ButtonEvent{id, ButtonAction::Pressed, 0}, controller_);
      break;
    }
    
    case RK::ButtonRelease: {
      ButtonId id = toInputButtonId(req.button);
      state_.onButton(ButtonEvent{id, ButtonAction::Released, 0}, controller_);
      break;
    }
    
    case RK::ButtonClick: {
      ButtonId id = toInputButtonId(req.button);
      state_.onButton(ButtonEvent{id, ButtonAction::Click, 1}, controller_);
      break;
    }
    
    case RK::ButtonLongHold: {
      ButtonId id = toInputButtonId(req.button);
      state_.onButton(ButtonEvent{id, ButtonAction::LongHold, 1}, controller_);
      break;
    }
    
    // ========== LEGACY PROTOCOL (Backward Compatibility) ==========
    // Complex translation layer - will be removed once remote is updated
    case RK::Idle: {
      // Idle can mean two things:
      // 1. User released a manual button (Manual → Idle transition)
      // 2. User pressed cancel/exit
      
      // Detect manual button release: if we were in Manual mode, send Released event
      if (lastRemoteCommand_ == RK::Manual) {
        // Release whichever manual button was active
        ButtonId btnToRelease = (lastRemoteManualMode_ == trailair::protocol::ManualMode::Vent) 
                                 ? ButtonId::Down : ButtonId::Up;
        state_.onButton(ButtonEvent{btnToRelease, ButtonAction::Released, 0}, controller_);
      } else {
        // True cancel/exit - send Left click
        state_.onButton(ButtonEvent{ButtonId::Left, ButtonAction::Click, 1}, controller_);
      }
      
      // Update tracking
      lastRemoteCommand_ = RK::Idle;
      break;
    }
      
    case RK::Start: {
      // Remote wants to start seeking to target pressure
      // First sync the target PSI from the remote
      state_.setTargetPsi(req.targetPSI);
      
      // Then send Right click to trigger seek
      state_.onButton(ButtonEvent{ButtonId::Right, ButtonAction::Click, 1}, controller_);
      
      // Update tracking
      lastRemoteCommand_ = RK::Start;
      break;
    }
      
    case RK::Manual: {
      // Remote is in manual mode
      // This command streams continuously while button is held
      // We need to detect transitions to avoid duplicate Press events
      
      bool isTransitionToManual = (lastRemoteCommand_ != RK::Manual);
      bool modeChanged = (lastRemoteManualMode_ != req.manualMode);
      
      if (isTransitionToManual) {
        // First Manual command: send Left click to enter Manual view (if not already there)
        // The UI state machine will handle this gracefully
        state_.onButton(ButtonEvent{ButtonId::Left, ButtonAction::Click, 1}, controller_);
      }
      
      // Handle button transitions
      if (req.manualMode == trailair::protocol::ManualMode::Vent) {
        // User wants to vent
        if (modeChanged && lastRemoteCommand_ == RK::Manual) {
          // Was holding Air, now holding Vent - release Up, press Down
          state_.onButton(ButtonEvent{ButtonId::Up, ButtonAction::Released, 0}, controller_);
          state_.onButton(ButtonEvent{ButtonId::Down, ButtonAction::Pressed, 0}, controller_);
        } else if (isTransitionToManual) {
          // First manual command with Vent - just press Down
          state_.onButton(ButtonEvent{ButtonId::Down, ButtonAction::Pressed, 0}, controller_);
        }
        // Else: continuing to hold Down - no event needed
      } else {
        // User wants to air up
        if (modeChanged && lastRemoteCommand_ == RK::Manual) {
          // Was holding Vent, now holding Air - release Down, press Up
          state_.onButton(ButtonEvent{ButtonId::Down, ButtonAction::Released, 0}, controller_);
          state_.onButton(ButtonEvent{ButtonId::Up, ButtonAction::Pressed, 0}, controller_);
        } else if (isTransitionToManual) {
          // First manual command with Air - just press Up
          state_.onButton(ButtonEvent{ButtonId::Up, ButtonAction::Pressed, 0}, controller_);
        }
        // Else: continuing to hold Up - no event needed
      }
      
      // Update tracking
      lastRemoteCommand_ = RK::Manual;
      lastRemoteManualMode_ = req.manualMode;
      break;
    }
      
    case RK::Ping: {
      // Ping is just a keep-alive / sync message
      // Sync target PSI if provided, but don't send any button events
      if (req.targetPSI > 0.0f) {
        state_.setTargetPsi(req.targetPSI);
      }
      
      // Ping doesn't change lastRemoteCommand_ - it's not a user action
      break;
    }
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
      comms_.sendStatus(controller_.getStatusCharacter(), state_.getUIStateChar(), 
                       controller_.getCurrentPSI(), state_.targetPsi());
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
