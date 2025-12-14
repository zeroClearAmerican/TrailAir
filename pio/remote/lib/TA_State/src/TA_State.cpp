#include "TA_State.h"
#include <Arduino.h>
#include <TA_Protocol.h>
#include <TA_Comms.h>
#include <TA_Display.h>
#include <TA_Input.h>

namespace trailair { namespace state {

StateController::StateController(trailair::comms::EspNowLink& link)
  : link_(link) {
}

void StateController::begin() {
  leftLongHoldSent_ = false;
  pairingActive_ = false;
  pairingFailed_ = false;
  pairingBusy_ = false;
  sleepRequested_ = false;
}

void StateController::resetAfterWake() {
  leftLongHoldSent_ = false;
  sleepRequested_ = false;
  pairingActive_ = false;
  pairingFailed_ = false;
  pairingBusy_ = false;
  pairingFailHoldUntil_ = 0;
}

bool StateController::takeSleepRequest() {
  bool r = sleepRequested_;
  sleepRequested_ = false;
  return r;
}

void StateController::onBatteryPercent(int percent) {
  batteryPercent_ = constrain(percent, 0, 100);
}

void StateController::onStatus(const trailair::protocol::Response& msg) {
  using trailair::protocol::StatusCode;
  
  // Store board's state - board is the master, we just display what it says
  boardControllerStatus_ = msg.status;
  boardUIState_ = msg.uiState;
  
  // Update pressure values
  if (msg.status != StatusCode::Error) {
    currentPsi_ = trailair::protocol::convertByteToPSI(msg.value);
  } else {
    lastErrorCode_ = msg.value;
  }
  
  // Sync target PSI from board (board is master for target too)
  if (msg.targetPSI > 0) {
    targetPsi_ = trailair::protocol::convertByteToPSI(msg.targetPSI);
  }
}

void StateController::update(uint32_t now, bool isConnected, bool isConnecting) {
  isConnected_ = isConnected;
  isConnecting_ = isConnecting;
  
  // Pairing failure hold auto-exit
  if (pairingFailed_ && pairingFailHoldUntil_ != 0) {
    if (now >= pairingFailHoldUntil_) {
      pairingFailHoldUntil_ = 0;
      pairingFailed_ = false;
      pairingBusy_ = false;
    }
  }
}

void StateController::onButton(const trailair::input::ButtonEvent& e) {
  using trailair::input::ButtonId;
  using trailair::input::ButtonAction;
  
  uint32_t now = millis();
  lastButtonTime_ = now;
  
  // Helper to convert input ButtonId to protocol ButtonId
  auto toProtoButtonId = [](ButtonId b) -> trailair::protocol::ButtonId {
    switch (b) {
      case ButtonId::Left:  return trailair::protocol::ButtonId::Left;
      case ButtonId::Down:  return trailair::protocol::ButtonId::Down;
      case ButtonId::Up:    return trailair::protocol::ButtonId::Up;
      case ButtonId::Right: return trailair::protocol::ButtonId::Right;
    }
    return trailair::protocol::ButtonId::Left;
  };
  
  // ========== REMOTE-SPECIFIC BUTTON HANDLING ==========
  
  // Special: Left long-hold = sleep (remote only, don't send to board)
  if (e.id == ButtonId::Left && e.action == ButtonAction::LongHold) {
    if (!leftLongHoldSent_) {
      sleepRequested_ = true;
      leftLongHoldSent_ = true;
    }
    return;  // Don't send to board
  }
  
  // Reset long-hold flag when Left is released
  if (e.action == ButtonAction::Released && e.id == ButtonId::Left) {
    leftLongHoldSent_ = false;
  }
  
  // Special: Right click when disconnected = start pairing (remote only)
  if (!isConnected_ && e.id == ButtonId::Right && e.action == ButtonAction::Click) {
    link_.startPairing(0x01, 10000);  // groupId=1, timeout=10s
    return;  // Don't send to board
  }
  
  // Special: Right click during pairing = retry or cancel
  if (pairingActive_) {
    if (e.id == ButtonId::Right && e.action == ButtonAction::Click) {
      if (link_.isPairing()) {
        link_.cancelPairing();
      } else if (pairingFailed_) {
        link_.startPairing(0x01, 10000);
      }
    }
    return;  // Don't send to board while pairing
  }
  
  // ========== FORWARD ALL OTHER BUTTONS TO BOARD ==========
  // This is the thin client magic - just forward button events!
  
  trailair::protocol::ButtonId protoBtn = toProtoButtonId(e.id);
  
  switch (e.action) {
    case ButtonAction::Pressed:
      link_.sendButtonPress(protoBtn);
      break;
      
    case ButtonAction::Released:
      link_.sendButtonRelease(protoBtn);
      break;
      
    case ButtonAction::Click:
      link_.sendButtonClick(protoBtn);
      break;
      
    case ButtonAction::LongHold:
      // Only send long-hold for non-Left buttons (Left is sleep, handled above)
      if (e.id != ButtonId::Left) {
        link_.sendButtonLongHold(protoBtn);
      }
      break;
  }
}

void StateController::buildDisplayModel(trailair::display::DisplayModel& dm) const {
  using trailair::display::ViewType;
  using trailair::display::ControllerActivity;
  using trailair::display::ConnectionStatus;
  
  // ========== BOARD'S STATE (MASTER) ==========
  // Remote simply displays what the board tells it to display
  
  dm.currentPressurePSI = currentPsi_;
  dm.targetPressurePSI = targetPsi_;
  dm.lastErrorCode = lastErrorCode_;
  
  // Map board's UI state to view type
  switch (boardUIState_) {
    case trailair::protocol::UIState::Idle:    dm.viewType = ViewType::Idle;    break;
    case trailair::protocol::UIState::Manual:  dm.viewType = ViewType::Manual;  break;
    case trailair::protocol::UIState::Seeking: dm.viewType = ViewType::Seeking; break;
    case trailair::protocol::UIState::Error:   dm.viewType = ViewType::Error;   break;
  }
  
  // Map board's controller status to activity
  switch (boardControllerStatus_) {
    case trailair::protocol::StatusCode::Idle:     dm.controllerActivity = ControllerActivity::Idle;     break;
    case trailair::protocol::StatusCode::AirUp:    dm.controllerActivity = ControllerActivity::AirUp;    break;
    case trailair::protocol::StatusCode::Venting:  dm.controllerActivity = ControllerActivity::Venting;  break;
    case trailair::protocol::StatusCode::Checking: dm.controllerActivity = ControllerActivity::Checking; break;
    case trailair::protocol::StatusCode::Error:    dm.controllerActivity = ControllerActivity::Error;    break;
  }
  
  // ========== REMOTE-SPECIFIC OVERLAYS ==========
  // These are things the board doesn't know about
  
  dm.batteryPercentage = batteryPercent_;
  dm.showBatteryIcon = true;  // Remote always shows battery
  
  dm.connectionStatus = isConnected_ ? ConnectionStatus::Connected : ConnectionStatus::Disconnected;
  dm.showReconnectHint = !isConnecting_;
  
  // Pairing UI (remote-only state)
  dm.pairingActive = pairingActive_;
  dm.pairingFailed = pairingFailed_;
  dm.pairingBusy = pairingBusy_;
  
  // Override view to show pairing screen when pairing
  if (pairingActive_ || pairingFailed_ || pairingBusy_) {
    dm.viewType = ViewType::Pairing;
  }
  
  // Override view to show disconnected screen when not connected (unless pairing)
  if (!isConnected_ && !pairingActive_ && !pairingFailed_ && !pairingBusy_) {
    dm.viewType = ViewType::Disconnected;
  }
  
  // Board-only fields (set to sensible defaults for remote)
  dm.seekingShowDoneHold = false;  // Board controls this, we just display
}

void StateController::onPairEvent(trailair::comms::PairEvent ev, const uint8_t* /*mac*/) {
  switch (ev) {
    case trailair::comms::PairEvent::Started:
      pairingActive_ = true;
      pairingFailed_ = false;
      pairingBusy_ = false;
      pairingFailHoldUntil_ = 0;
      break;
      
    case trailair::comms::PairEvent::Acked:
      pairingActive_ = false;
      pairingFailed_ = false;
      pairingBusy_ = false;
      break;
      
    case trailair::comms::PairEvent::Busy:
      pairingActive_ = false;
      pairingBusy_ = true;
      pairingFailed_ = true;
      pairingFailHoldUntil_ = millis() + 2000;
      break;
      
    case trailair::comms::PairEvent::Timeout:
    case trailair::comms::PairEvent::Canceled:
      pairingActive_ = false;
      pairingBusy_ = false;
      pairingFailed_ = true;
      pairingFailHoldUntil_ = millis() + 2000;
      break;
      
    case trailair::comms::PairEvent::Saved:
    case trailair::comms::PairEvent::Cleared:
    default:
      break;
  }
}

} // namespace state
} // namespace trailair
