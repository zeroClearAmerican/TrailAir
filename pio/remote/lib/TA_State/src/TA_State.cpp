#include "TA_State.h"
#include <Arduino.h>
#include <TA_Protocol.h>
#include <TA_Comms.h>
#include <TA_Display.h>
#include <TA_Input.h>
#include <TA_UI.h>
#include <TA_Config.h>

namespace trailair { namespace state {

StateController::StateController(trailair::comms::EspNowLink& link, const Config& cfg)
  : link_(link) {
  // Ensure config pointers are valid by falling back to defaults
  static const trailair::config::UserInterfaceConfiguration kUiDefaults{};
  static const trailair::config::CommunicationConfiguration kLinkDefaults{};
  cfg_.ui = cfg.ui ? cfg.ui : &kUiDefaults;
  cfg_.link = cfg.link ? cfg.link : &kLinkDefaults;

  trailair::ui::UserInterfaceConfig uic;
  uic.minimumPSI = cfg_.ui->minimumPressurePSI;
  uic.maximumPSI = cfg_.ui->maximumPressurePSI;
  uic.defaultTargetPSI = cfg_.ui->defaultTargetPressurePSI;
  uic.stepSize = cfg_.ui->pressureStepSmallPSI;
  uic.doneHoldDurationMilliseconds = cfg_.ui->doneHoldDurationMilliseconds;
  uic.errorAutoClearDurationMilliseconds = cfg_.ui->errorAutoClearDurationMilliseconds;
  ui_.begin(uic);
}

void StateController::begin() {
  leftLongHoldActive_ = false;
  enter_(RemoteState::DISCONNECTED, millis());
}

void StateController::resetAfterWake() {
  suppressLeftClicksUntil_ = 0;
  leftSleepHold_ = false;
  leftLongHoldActive_ = false;   // clear latch after wake
  errorClearRequested_ = false;
  leftPressed_ = false;
  enter_(RemoteState::DISCONNECTED, millis());
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
  if (msg.status != StatusCode::Error) {
    currentPsi_ = trailair::protocol::convertByteToPSI(msg.value);
  } else {
    lastErrorCode_ = msg.value;
  }

  switch (msg.status) {
    case StatusCode::Idle:     cState_ = ControlState::IDLE;     break;
    case StatusCode::AirUp:    cState_ = ControlState::AIRUP;    break;
    case StatusCode::Venting:  cState_ = ControlState::VENTING;  break;
    case StatusCode::Checking: cState_ = ControlState::CHECKING; break;
    case StatusCode::Error:    cState_ = ControlState::ERROR;    break;
  }

  // Leave ERROR view when board recovers
  if (rState_ == RemoteState::ERROR && msg.status != StatusCode::Error) {
    errorClearRequested_ = false; // allow future auto-clears
    enter_(RemoteState::IDLE, millis());
  }
}

void StateController::update(uint32_t now, bool isConnected, bool isConnecting) {
  isConnected_ = isConnected;
  isConnecting_ = isConnecting;

  // Pairing failure hold auto-exit
  if (rState_ == RemoteState::PAIRING && pairingFailed_) {
    if (now >= pairingFailHoldUntil_ && pairingFailHoldUntil_ != 0) {
      pairingFailHoldUntil_ = 0;
      enter_(RemoteState::DISCONNECTED, now);
    }
  }

  // Shared UI update (delegates error autoclear Cancel)
  RemoteActions ra; ra.self = this;
  ui_.update(now, ra, toUiCtrl(cState_));

  // Keep rState_ aligned with shared view
  switch (ui_.getViewState()) {
    case trailair::ui::ViewState::Idle:         rState_ = RemoteState::IDLE; break;
    case trailair::ui::ViewState::Manual:       rState_ = RemoteState::MANUAL; break;
    case trailair::ui::ViewState::Seeking:      rState_ = RemoteState::SEEKING; break;
    case trailair::ui::ViewState::Error:        rState_ = RemoteState::ERROR; break;
    case trailair::ui::ViewState::Disconnected: rState_ = RemoteState::DISCONNECTED; break;
    case trailair::ui::ViewState::Pairing:      rState_ = RemoteState::PAIRING; break;
  }

  // Manual resend while truly in Manual
  if (ui_.getViewState() == trailair::ui::ViewState::Manual && manualSending_) {
    if (now - lastManualSentMs_ >= cfg_.link->manualRepeatIntervalMilliseconds) {
      link_.sendManual(manualCode_);
      lastManualSentMs_ = now;
    }
  }

  // If we left Manual due to error or other transitions, stop manual stream
  if (ui_.getViewState() != trailair::ui::ViewState::Manual && manualSending_) {
    link_.sendCancel();
    manualSending_ = false;
    lastManualSentMs_ = 0;
    manualCode_ = 0x00;
  }
}

void StateController::enter_(RemoteState s, uint32_t now) {
  rPrev_ = rState_;
  rState_ = s;
  stateEntryMs_ = now;

  if (rPrev_ == RemoteState::MANUAL && rState_ != RemoteState::MANUAL) {
    if (manualSending_) {
      link_.sendCancel();
      manualSending_ = false;
    }
    lastManualSentMs_ = 0;
    manualCode_ = 0x00;
  }

  switch (rState_) {
    case RemoteState::DISCONNECTED:
      pairingFailed_ = false;
      pairingBusy_ = false;
      break;
    case RemoteState::IDLE:
      break;
    case RemoteState::MANUAL:
      break;
    case RemoteState::SEEKING:
      break;
    case RemoteState::ERROR:
      if (!errorClearRequested_) {
        // Will be auto-cleared by UserInterfaceStateMachine via DeviceActions::clearError()
        errorClearRequested_ = true;
      }
      break;
    case RemoteState::PAIRING:
      pairingFailed_ = false;
      pairingBusy_ = false;
      break;
  }
}

void StateController::onButton(const trailair::input::ButtonEvent& e) {
  uint32_t now = millis();

  // Left sleep long-hold handling remains remote-specific
  if (e.id == trailair::input::ButtonId::Left) {
    if (e.action == trailair::input::ButtonAction::Pressed) leftPressed_ = true;
    else if (e.action == trailair::input::ButtonAction::Released) leftPressed_ = false;

    if (e.action == trailair::input::ButtonAction::LongHold) {
      sleepRequested_ = true;
      leftLongHoldActive_ = true;
      leftPressed_ = false;
      suppressLeftClicksUntil_ = now + 1500;
      return;
    }
    if (leftLongHoldActive_) return; // swallow all following left events until wake
    if (e.action == trailair::input::ButtonAction::Click && leftPressed_) return; // ignore repeat clicks while held
    if ((e.action == trailair::input::ButtonAction::Click || e.action == trailair::input::ButtonAction::Released) && now < suppressLeftClicksUntil_) return;
  }

  // Disconnected & Pairing shortcuts remain remote-specific
  if (rState_ == RemoteState::DISCONNECTED && e.id == trailair::input::ButtonId::Right && e.action == trailair::input::ButtonAction::Click) {
    // Always try pairing - if board is already paired to us, it will just re-ack
    // If board changed (new MAC), this allows automatic re-pairing
    // If board is paired to different remote, it will send Busy
    link_.startPairing(cfg_.link->pairingGroupIdentifier, cfg_.link->pairingTimeoutMilliseconds);
    return;
  }
  if (rState_ == RemoteState::PAIRING) {
    if (e.id == trailair::input::ButtonId::Right && e.action == trailair::input::ButtonAction::Click) {
      if (link_.isPairing()) link_.cancelPairing();
      else if (pairingFailed_) link_.startPairing(cfg_.link->pairingGroupIdentifier, cfg_.link->pairingTimeoutMilliseconds);
    }
    return;
  }

  // Delegate to shared UI machine
  RemoteActions ra; ra.self = this;
  trailair::ui::ButtonEvent be{
    e.id == trailair::input::ButtonId::Left ? trailair::ui::ButtonId::Left :
    e.id == trailair::input::ButtonId::Down ? trailair::ui::ButtonId::Down :
    e.id == trailair::input::ButtonId::Up   ? trailair::ui::ButtonId::Up   : trailair::ui::ButtonId::Right,
    e.action == trailair::input::ButtonAction::Pressed ? trailair::ui::ButtonAction::Pressed :
    e.action == trailair::input::ButtonAction::Released? trailair::ui::ButtonAction::Released:
    e.action == trailair::input::ButtonAction::Click   ? trailair::ui::ButtonAction::Click   : trailair::ui::ButtonAction::LongHold
  };
  ui_.onButton(be, ra);

  // rState_ will be synced in update(); no need to mutate here.
}

void StateController::handleButtonsDisconnected_(const trailair::input::ButtonEvent& e, uint32_t /*now*/) {
  (void)e;
}

void StateController::buildDisplayModel(trailair::display::DisplayModel& dm) const {
  dm.batteryPercentage = batteryPercent_;
  dm.connectionStatus = isConnected_ ? trailair::display::ConnectionStatus::Connected : trailair::display::ConnectionStatus::Disconnected;

  switch (cState_) {
    case ControlState::IDLE:     dm.controllerActivity = trailair::display::ControllerActivity::Idle;     break;
    case ControlState::AIRUP:    dm.controllerActivity = trailair::display::ControllerActivity::AirUp;    break;
    case ControlState::VENTING:  dm.controllerActivity = trailair::display::ControllerActivity::Venting;  break;
    case ControlState::CHECKING: dm.controllerActivity = trailair::display::ControllerActivity::Checking; break;
    case ControlState::ERROR:    dm.controllerActivity = trailair::display::ControllerActivity::Error;    break;
  }

  dm.currentPressurePSI = currentPsi_;
  dm.targetPressurePSI  = ui_.getTargetPSI();
  dm.lastErrorCode = lastErrorCode_;
  dm.seekingShowDoneHold = ui_.isDoneHoldActive(millis());
  dm.showReconnectHint = (!isConnecting_);

  // Pairing flags
  dm.pairingActive = (rState_ == RemoteState::PAIRING) && link_.isPairing() && !pairingFailed_;
  dm.pairingFailed = (rState_ == RemoteState::PAIRING) && pairingFailed_;
  dm.pairingBusy   = (rState_ == RemoteState::PAIRING) && pairingBusy_;

  switch (ui_.getViewState()) {
    case trailair::ui::ViewState::Disconnected: dm.viewType = trailair::display::ViewType::Disconnected; break;
    case trailair::ui::ViewState::Idle:         dm.viewType = trailair::display::ViewType::Idle;         break;
    case trailair::ui::ViewState::Manual:       dm.viewType = trailair::display::ViewType::Manual;       break;
    case trailair::ui::ViewState::Seeking:      dm.viewType = trailair::display::ViewType::Seeking;      break;
    case trailair::ui::ViewState::Error:        dm.viewType = trailair::display::ViewType::Error;        break;
    case trailair::ui::ViewState::Pairing:      dm.viewType = trailair::display::ViewType::Pairing;      break;
  }
}

void StateController::onPairEvent(trailair::comms::PairEvent ev, const uint8_t* /*mac*/) {
  switch (ev) {
    case trailair::comms::PairEvent::Started:
      pairingFailed_ = false;
      pairingBusy_ = false;
      pairingFailHoldUntil_ = 0;
      rState_ = RemoteState::PAIRING;
      break;
    case trailair::comms::PairEvent::Acked:
      rState_ = RemoteState::DISCONNECTED;
      break;
    case trailair::comms::PairEvent::Busy:
      pairingBusy_ = true;
      pairingFailed_ = true;
      pairingFailHoldUntil_ = millis() + 2000;
      break;
    case trailair::comms::PairEvent::Timeout:
    case trailair::comms::PairEvent::Canceled:
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

// RemoteActions implementations
void StateController::RemoteActions::cancel() {
  if (!self) return;
  // Avoid spamming cancel while in ERROR; send once per error occurrence
  if (self->cState_ == ControlState::ERROR) {
    if (!self->errorClearRequested_) {
      self->link_.sendCancel();
      self->errorClearRequested_ = true;
    }
  } else {
    self->link_.sendCancel();
  }
}
void StateController::RemoteActions::startSeek(float targetPsi) { if (self) self->link_.sendStart(targetPsi); }
void StateController::RemoteActions::manualVent(bool on) {
  if (!self) return;
  if (on && !self->manualSending_) {
    self->manualCode_ = 0x00;
    self->manualSending_ = true;
    self->lastManualSentMs_ = millis();
    self->link_.sendManual(self->manualCode_);
  } else if (!on && self->manualSending_ && self->manualCode_ == 0x00) {
    self->manualSending_ = false;
    self->link_.sendCancel();
  }
}
void StateController::RemoteActions::manualAirUp(bool on) {
  if (!self) return;
  if (on && !self->manualSending_) {
    self->manualCode_ = 0xFF;
    self->manualSending_ = true;
    self->lastManualSentMs_ = millis();
    self->link_.sendManual(self->manualCode_);
  } else if (!on && self->manualSending_ && self->manualCode_ == 0xFF) {
    self->manualSending_ = false;
    self->link_.sendCancel();
  }
}

} // namespace state
} // namespace ta
