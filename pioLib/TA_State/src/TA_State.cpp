#include "TA_State.h"

namespace ta {
namespace state {

StateController::StateController(ta::comms::EspNowLink& link, const Config& cfg)
  : link_(link), cfg_(cfg) {
  ta::ui::UiConfig uic;
  uic.minPsi = cfg_.ui.minPsi;
  uic.maxPsi = cfg_.ui.maxPsi;
  uic.defaultTargetPsi = cfg_.ui.defaultTargetPsi;
  uic.stepSmall = cfg_.ui.stepSmall;
  uic.doneHoldMs = cfg_.ui.doneHoldMs;
  uic.errorAutoClearMs = cfg_.ui.errorAutoClearMs;
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

void StateController::onStatus(const ta::protocol::StatusMsg& msg) {
  using ta::protocol::Status;
  if (msg.status != Status::Error) {
    currentPsi_ = ta::protocol::byteToPsi05(msg.value);
  } else {
    lastErrorCode_ = msg.value;
  }

  switch (msg.status) {
    case Status::Idle:     cState_ = ControlState::IDLE;     break;
    case Status::AirUp:    cState_ = ControlState::AIRUP;    break;
    case Status::Venting:  cState_ = ControlState::VENTING;  break;
    case Status::Checking: cState_ = ControlState::CHECKING; break;
    case Status::Error:    cState_ = ControlState::ERROR;    break;
  }

  // Leave ERROR view when board recovers
  if (rState_ == RemoteState::ERROR && msg.status != Status::Error) {
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
  switch (ui_.view()) {
    case ta::ui::View::Idle:         rState_ = RemoteState::IDLE; break;
    case ta::ui::View::Manual:       rState_ = RemoteState::MANUAL; break;
    case ta::ui::View::Seeking:      rState_ = RemoteState::SEEKING; break;
    case ta::ui::View::Error:        rState_ = RemoteState::ERROR; break;
    case ta::ui::View::Disconnected: rState_ = RemoteState::DISCONNECTED; break;
    case ta::ui::View::Pairing:      rState_ = RemoteState::PAIRING; break;
  }

  // Manual resend while truly in Manual
  if (ui_.view() == ta::ui::View::Manual && manualSending_) {
    if (now - lastManualSentMs_ >= cfg_.link.manualRepeatMs) {
      link_.sendManual(manualCode_);
      lastManualSentMs_ = now;
    }
  }

  // If we left Manual due to error or other transitions, stop manual stream
  if (ui_.view() != ta::ui::View::Manual && manualSending_) {
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
        // Will be auto-cleared by UiStateMachine via DeviceActions::clearError()
        errorClearRequested_ = true;
      }
      break;
    case RemoteState::PAIRING:
      pairingFailed_ = false;
      pairingBusy_ = false;
      break;
  }
}

void StateController::onButton(const ta::input::Event& e) {
  uint32_t now = millis();

  // Left sleep long-hold handling remains remote-specific
  if (e.id == ta::input::ButtonId::Left) {
    if (e.action == ta::input::Action::Pressed) leftPressed_ = true;
    else if (e.action == ta::input::Action::Released) leftPressed_ = false;

    if (e.action == ta::input::Action::LongHold) {
      sleepRequested_ = true;
      leftLongHoldActive_ = true;
      leftPressed_ = false;
      suppressLeftClicksUntil_ = now + 1500;
      return;
    }
    if (leftLongHoldActive_) return; // swallow all following left events until wake
    if (e.action == ta::input::Action::Click && leftPressed_) return; // ignore repeat clicks while held
    if ((e.action == ta::input::Action::Click || e.action == ta::input::Action::Released) && now < suppressLeftClicksUntil_) return;
  }

  // Disconnected & Pairing shortcuts remain remote-specific
  if (rState_ == RemoteState::DISCONNECTED && e.id == ta::input::ButtonId::Right && e.action == ta::input::Action::Click) {
    if (canStartPairing()) link_.startPairing(cfg_.link.pairGroupId, cfg_.link.pairTimeoutMs); else link_.requestReconnect();
    return;
  }
  if (rState_ == RemoteState::PAIRING) {
    if (e.id == ta::input::ButtonId::Right && e.action == ta::input::Action::Click) {
      if (link_.isPairing()) link_.cancelPairing();
      else if (pairingFailed_ && canStartPairing()) link_.startPairing(cfg_.link.pairGroupId, cfg_.link.pairTimeoutMs);
    }
    return;
  }

  // Delegate to shared UI machine
  RemoteActions ra; ra.self = this;
  ta::ui::ButtonEvent be{
    e.id == ta::input::ButtonId::Left ? ta::ui::Button::Left :
    e.id == ta::input::ButtonId::Down ? ta::ui::Button::Down :
    e.id == ta::input::ButtonId::Up   ? ta::ui::Button::Up   : ta::ui::Button::Right,
    e.action == ta::input::Action::Pressed ? ta::ui::Action::Pressed :
    e.action == ta::input::Action::Released? ta::ui::Action::Released:
    e.action == ta::input::Action::Click   ? ta::ui::Action::Click   : ta::ui::Action::LongHold
  };
  ui_.onButton(be, ra);

  // rState_ will be synced in update(); no need to mutate here.
}

void StateController::handleButtonsDisconnected_(const ta::input::Event& e, uint32_t /*now*/) {
  (void)e;
}

void StateController::buildDisplayModel(ta::display::DisplayModel& dm) const {
  dm.batteryPercent = batteryPercent_;
  dm.link = isConnected_ ? ta::display::Link::Connected : ta::display::Link::Disconnected;

  switch (cState_) {
    case ControlState::IDLE:     dm.ctrl = ta::display::Ctrl::Idle;     break;
    case ControlState::AIRUP:    dm.ctrl = ta::display::Ctrl::AirUp;    break;
    case ControlState::VENTING:  dm.ctrl = ta::display::Ctrl::Venting;  break;
    case ControlState::CHECKING: dm.ctrl = ta::display::Ctrl::Checking; break;
    case ControlState::ERROR:    dm.ctrl = ta::display::Ctrl::Error;    break;
  }

  dm.currentPSI = currentPsi_;
  dm.targetPSI  = ui_.targetPsi();
  dm.lastErrorCode = lastErrorCode_;
  dm.seekingShowDoneHold = ui_.isDoneHoldActive(millis());
  dm.showReconnectHint = (!isConnecting_);

  // Pairing flags
  dm.pairingActive = (rState_ == RemoteState::PAIRING) && link_.isPairing() && !pairingFailed_;
  dm.pairingFailed = (rState_ == RemoteState::PAIRING) && pairingFailed_;
  dm.pairingBusy   = (rState_ == RemoteState::PAIRING) && pairingBusy_;

  switch (ui_.view()) {
    case ta::ui::View::Disconnected: dm.view = ta::display::View::Disconnected; break;
    case ta::ui::View::Idle:         dm.view = ta::display::View::Idle;         break;
    case ta::ui::View::Manual:       dm.view = ta::display::View::Manual;       break;
    case ta::ui::View::Seeking:      dm.view = ta::display::View::Seeking;      break;
    case ta::ui::View::Error:        dm.view = ta::display::View::Error;        break;
    case ta::ui::View::Pairing:      dm.view = ta::display::View::Pairing;      break;
  }
}

bool StateController::canStartPairing() const {
  return !link_.hasPeer(); // only allow if no persisted peer yet
}

void StateController::onPairEvent(ta::comms::PairEvent ev, const uint8_t* /*mac*/) {
  switch (ev) {
    case ta::comms::PairEvent::Started:
      pairingFailed_ = false;
      pairingBusy_ = false;
      pairingFailHoldUntil_ = 0;
      rState_ = RemoteState::PAIRING;
      break;
    case ta::comms::PairEvent::Acked:
      rState_ = RemoteState::DISCONNECTED;
      break;
    case ta::comms::PairEvent::Busy:
      pairingBusy_ = true;
      pairingFailed_ = true;
      pairingFailHoldUntil_ = millis() + 2000;
      break;
    case ta::comms::PairEvent::Timeout:
    case ta::comms::PairEvent::Canceled:
      pairingBusy_ = false;
      pairingFailed_ = true;
      pairingFailHoldUntil_ = millis() + 2000;
      break;
    case ta::comms::PairEvent::Saved:
    case ta::comms::PairEvent::Cleared:
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