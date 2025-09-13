#include "TA_State.h"

namespace ta {
namespace state {

StateController::StateController(ta::comms::EspNowLink& link, const Config& cfg)
  : link_(link), cfg_(cfg) {
  targetPsi_ = cfg_.defaultTargetPsi;
}

void StateController::begin() {
  leftLongHoldActive_ = false;
  enter_(RemoteState::DISCONNECTED, millis());
}

void StateController::resetAfterWake() {
  seekingActiveSeen_ = false;
  seekingDoneUntil_ = 0;
  disconnectedHoldUntil_ = 0;
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

  bool wasErrorView = (rState_ == RemoteState::ERROR);
  // (existing switch already set cState_)
  // If we were showing ERROR and board is now non-error, leave ERROR view
  if (wasErrorView && msg.status != Status::Error) {
    enter_(RemoteState::IDLE, millis());
  }

  // SEEKING done hold trigger
  if (rState_ == RemoteState::SEEKING) {
    if (msg.status == Status::AirUp || msg.status == Status::Venting || msg.status == Status::Checking) {
      seekingActiveSeen_ = true;
    } else if (msg.status == Status::Idle && seekingActiveSeen_ && seekingDoneUntil_ == 0) {
      seekingDoneUntil_ = millis() + cfg_.doneHoldMs;
    }
  }

  // Enter ERROR screen when error reported
  if (msg.status == Status::Error && rState_ != RemoteState::ERROR) {
    errorClearRequested_ = false;     // new error, allow future auto-clear
    enter_(RemoteState::ERROR, millis());
  }
}

void StateController::update(uint32_t now, bool isConnected, bool isConnecting) {
  isConnected_ = isConnected;
  isConnecting_ = isConnecting;

  // If in pairing failure hold, auto-exit after window
  if (rState_ == RemoteState::PAIRING && pairingFailed_) {
    if (now >= pairingFailHoldUntil_ && pairingFailHoldUntil_ != 0) {
      pairingFailHoldUntil_ = 0;
      enter_(RemoteState::DISCONNECTED, now);
    }
  }

  // Manual repeat refresh (keep board in manual mode)
  if (rState_ == RemoteState::MANUAL && manualSending_) {
    if (now - lastManualSentMs_ >= manualRepeatMs_) {
      link_.sendManual(manualCode_);
      lastManualSentMs_ = now;
    }
  }

  switch (rState_) {
    case RemoteState::DISCONNECTED: {
      if (isConnected_) {
        if (disconnectedHoldUntil_ == 0) {
          disconnectedHoldUntil_ = now + cfg_.connectionHoldMs;
        }
        if (now >= disconnectedHoldUntil_) {
          disconnectedHoldUntil_ = 0;
          enter_(RemoteState::IDLE, now);
        }
      } else {
        disconnectedHoldUntil_ = 0;
      }
      break;
    }

    case RemoteState::IDLE: {
      if (!isConnected_) {
        enter_(RemoteState::DISCONNECTED, now);
      }
      break;
    }

    case RemoteState::MANUAL: {
      if (!isConnected_) {
        enter_(RemoteState::DISCONNECTED, now);
        break;
      }
      if (manualSending_ && (now - lastManualSentMs_) >= cfg_.manualRepeatMs) {
        link_.sendManual(manualCode_);
        lastManualSentMs_ = now;
      }
      break;
    }

    case RemoteState::SEEKING: {
      if (!isConnected_) {
        enter_(RemoteState::DISCONNECTED, now);
      } else if (cState_ == ControlState::ERROR) {
        enter_(RemoteState::ERROR, now);
      } else if (seekingDoneUntil_ != 0 && now >= seekingDoneUntil_) {
        seekingDoneUntil_ = 0;
        enter_(RemoteState::IDLE, now);
      }
      break;
    }

    case RemoteState::ERROR: {
      // Automatically request board clear after timeout by sending Cancel once.
      if (!errorClearRequested_ &&
          (now - stateEntryMs_) >= cfg_.errorAutoClearMs) {
        link_.sendCancel();           // asks board to clear error
        errorClearRequested_ = true;
      }
      // (Remain in ERROR until board reports non-error status.)
      break;
    }

    case RemoteState::PAIRING: {
      // If user got paired (peer stored) but we are still in PAIRING (e.g. Saved event before Ack transition)
      if (link_.hasPeer() && !link_.isPairing() && !pairingFailed_) {
        enter_(RemoteState::DISCONNECTED, now);
      }
      break;
    }
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

  if (rState_ == RemoteState::SEEKING) {
    seekingActiveSeen_ = false;
    seekingDoneUntil_ = 0;
  }
  if (rState_ != RemoteState::ERROR) {
    // Leaving error or entering other state resets flag
    if (rPrev_ == RemoteState::ERROR)
      errorClearRequested_ = false;
  }
  if (rState_ == RemoteState::ERROR) {
    // ensure manual sending stops
    if (manualSending_) {
      link_.sendCancel();
      manualSending_ = false;
      lastManualSentMs_ = 0;
      manualCode_ = 0x00;
    }
  }
}

void StateController::onButton(const ta::input::Event& e) {
  uint32_t now = millis();

  if (e.id == ta::input::ButtonId::Left) {
    // Track physical press state
    if (e.action == ta::input::Action::Pressed) {
      leftPressed_ = true;
    } else if (e.action == ta::input::Action::Released) {
      leftPressed_ = false;
    }

    if (e.action == ta::input::Action::LongHold) {
      sleepRequested_ = true;
      leftLongHoldActive_ = true;
      leftPressed_ = false;                 // treat as consumed
      suppressLeftClicksUntil_ = now + 1500;
      return;
    }
    if (leftLongHoldActive_) {
      return; // swallow everything after long hold
    }
    // Suppress auto‑repeat clicks while still physically held
    if (e.action == ta::input::Action::Click && leftPressed_) {
      return;
    }
    // Existing suppression window
    if ((e.action == ta::input::Action::Click || e.action == ta::input::Action::Released) &&
        now < suppressLeftClicksUntil_) {
      return;
    }
  }

  // Disconnected pairing / reconnect
  if (rState_ == RemoteState::DISCONNECTED &&
      e.id == ta::input::ButtonId::Right &&
      e.action == ta::input::Action::Click) {
    if (canStartPairing()) link_.startPairing();
    else link_.requestReconnect();
    return;
  }

  if (rState_ == RemoteState::PAIRING) {
    if (e.id == ta::input::ButtonId::Right && e.action == ta::input::Action::Click) {
      if (link_.isPairing()) link_.cancelPairing();
      else if (pairingFailed_ && canStartPairing()) link_.startPairing();
    }
    return;
  }

  switch (rState_) {
    case RemoteState::DISCONNECTED: handleButtonsDisconnected_(e, now); break;
    case RemoteState::IDLE:         handleButtonsIdle_(e, now);         break;
    case RemoteState::MANUAL:       handleButtonsManual_(e, now);       break;
    case RemoteState::SEEKING:      handleButtonsSeeking_(e, now);      break;
    case RemoteState::ERROR:        handleButtonsError_(e, now);        break;
    case RemoteState::PAIRING:      break;
  }
}

void StateController::handleButtonsDisconnected_(const ta::input::Event& e, uint32_t /*now*/) {
  (void)e;
  // No direct actions here now; Right click logic moved to onButton()
}

void StateController::handleButtonsIdle_(const ta::input::Event& e, uint32_t /*now*/) {
  if (e.action == ta::input::Action::Click) {
    if (e.id == ta::input::ButtonId::Left) {
      // enter manual
      lastManualSentMs_ = 0;
      manualSending_ = false;
      manualCode_ = 0x00;
      enter_(RemoteState::MANUAL, millis());
    } else if (e.id == ta::input::ButtonId::Down) {
      targetPsi_ -= 1.0f;
      if (targetPsi_ < cfg_.minPsi) targetPsi_ = cfg_.minPsi;
    } else if (e.id == ta::input::ButtonId::Up) {
      targetPsi_ += 1.0f;
      if (targetPsi_ > cfg_.maxPsi) targetPsi_ = cfg_.maxPsi;
    } else if (e.id == ta::input::ButtonId::Right) {
      link_.sendStart(targetPsi_);
      enter_(RemoteState::SEEKING, millis());
    }
  }
}

void StateController::handleButtonsManual_(const ta::input::Event& e, uint32_t now) {
  // Exit manual
  if (e.id == ta::input::ButtonId::Left && e.action == ta::input::Action::Click) {
    if (manualSending_) {
      link_.sendCancel();
      manualSending_ = false;
    }
    enter_(RemoteState::IDLE, now);
    return;
  }

  // Vent (Down)
  if (e.id == ta::input::ButtonId::Down) {
    if (e.action == ta::input::Action::Pressed && !manualSending_) {
      manualCode_ = 0x00; // vent
      manualSending_ = true;
      lastManualSentMs_ = now;
      link_.sendManual(manualCode_);
    } else if (e.action == ta::input::Action::Released && manualSending_ && manualCode_ == 0x00) {
      manualSending_ = false;
      link_.sendCancel();
    }
    return;
  }

  // Air Up (Up)
  if (e.id == ta::input::ButtonId::Up) {
    if (e.action == ta::input::Action::Pressed && !manualSending_) {
      manualCode_ = 0xFF; // air
      manualSending_ = true;
      lastManualSentMs_ = now;
      link_.sendManual(manualCode_);
    } else if (e.action == ta::input::Action::Released && manualSending_ && manualCode_ == 0xFF) {
      manualSending_ = false;
      link_.sendCancel();
    }
    return;
  }
}

void StateController::handleButtonsSeeking_(const ta::input::Event& e, uint32_t /*now*/) {
  if (e.id == ta::input::ButtonId::Right && e.action == ta::input::Action::Click) {
    link_.sendCancel();
  }
}

void StateController::handleButtonsError_(const ta::input::Event& e, uint32_t now) {
  if (e.id == ta::input::ButtonId::Right && e.action == ta::input::Action::Click) {
    // Send Idle/Cancel command to ask board to clear error.
    link_.sendCancel();          // (Assumes sendCancel() transmits Cmd::Idle)
    // Do NOT leave ERROR yet; wait for non-error status frame.
    // Optionally give brief user feedback (e.g. flash icon) here.
    (void)now;
  }
  if (e.id == ta::input::ButtonId::Right && e.action == ta::input::Action::Click) {
    if (!errorClearRequested_) {
      link_.sendCancel();
      errorClearRequested_ = true;
    }
  }
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
  dm.targetPSI  = targetPsi_;
  dm.lastErrorCode = lastErrorCode_;
  dm.seekingShowDoneHold = (rState_ == RemoteState::SEEKING) && (seekingDoneUntil_ != 0);
  dm.showReconnectHint = (!isConnecting_);

  // Pairing flags
  dm.pairingActive = (rState_ == RemoteState::PAIRING) && link_.isPairing() && !pairingFailed_;
  dm.pairingFailed = (rState_ == RemoteState::PAIRING) && pairingFailed_;
  dm.pairingBusy   = (rState_ == RemoteState::PAIRING) && pairingBusy_;

  switch (rState_) {
    case RemoteState::DISCONNECTED: dm.view = ta::display::View::Disconnected; break;
    case RemoteState::IDLE:         dm.view = ta::display::View::Idle;         break;
    case RemoteState::MANUAL:       dm.view = ta::display::View::Manual;       break;
    case RemoteState::SEEKING:      dm.view = ta::display::View::Seeking;      break;
    case RemoteState::ERROR:        dm.view = ta::display::View::Error;        break;
    case RemoteState::PAIRING:      dm.view = ta::display::View::Pairing;      break;
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
      enter_(RemoteState::PAIRING, millis());
      break;
    case ta::comms::PairEvent::Acked:
      // Successfully paired -> transition to DISCONNECTED (will auto-connect)
      enter_(RemoteState::DISCONNECTED, millis());
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

} // namespace state
} // namespace ta