#include "TA_StateBoard.h"

using namespace ta::stateboard;

void StateBoard::begin() {
  Config def;
  begin(def);
}

void StateBoard::begin(const Config& cfg) {
  cfg_ = cfg;
  targetPsi_ = cfg_.defaultTargetPsi;
  clampTarget();
}

void StateBoard::clampTarget() {
  if (targetPsi_ < cfg_.minPsi) targetPsi_ = cfg_.minPsi;
  if (targetPsi_ > cfg_.maxPsi) targetPsi_ = cfg_.maxPsi;
}

void StateBoard::enterManual(ta::ctl::Controller& controller) {
  uiState_ = UiState::Manual;
  manualVentActive_ = false;
  manualAirActive_ = false;
  controller.cancel(); // ensure no seek continues
}

void StateBoard::exitManual(ta::ctl::Controller& controller) {
  if (manualVentActive_) controller.manualVent(false);
  if (manualAirActive_) controller.manualAirUp(false);
  manualVentActive_ = manualAirActive_ = false;
  uiState_ = UiState::Idle;
}

void StateBoard::startSeek(ta::ctl::Controller& controller) {
  controller.startSeek(targetPsi_);
  uiState_ = UiState::Seeking;
  seenSeekingActivity_ = false;
  showDoneHold_ = false;
}

void StateBoard::cancelSeek(ta::ctl::Controller& controller) {
  controller.cancel();
  uiState_ = UiState::Idle;
  showDoneHold_ = false;
}

void StateBoard::onButton(const ta::input::Event& ev, ta::ctl::Controller& controller) {
  using ta::input::ButtonId;
  using ta::input::Action;

  ButtonId btn = ev.id;
  Action act = ev.action;

  if (act == Action::Click) {
    if (uiState_ == UiState::Idle) {
      if (btn == ButtonId::Left)       { enterManual(controller); }
      else if (btn == ButtonId::Right) { startSeek(controller); }
      else if (btn == ButtonId::Up)    { targetPsi_ += cfg_.stepPsiSmall; clampTarget(); }
      else if (btn == ButtonId::Down)  { targetPsi_ -= cfg_.stepPsiSmall; clampTarget(); }
    } else if (uiState_ == UiState::Manual) {
      if (btn == ButtonId::Left) { exitManual(controller); }
    } else if (uiState_ == UiState::Seeking) {
      if (btn == ButtonId::Right) { cancelSeek(controller); }
    } else if (uiState_ == UiState::Error) {
      if (btn == ButtonId::Right) { controller.clearError(); uiState_ = UiState::Idle; }
    }
  } else if (act == Action::LongHold) {
    // (No long-hold actions on control board)
  } else if (act == Action::Pressed) {
    if (uiState_ == UiState::Manual) {
      if (btn == ButtonId::Down && !manualVentActive_) {
        controller.manualVent(true); manualVentActive_ = true;
      } else if (btn == ButtonId::Up && !manualAirActive_) {
        controller.manualAirUp(true); manualAirActive_ = true;
      }
    }
  } else if (act == Action::Released) {
    if (uiState_ == UiState::Manual) {
      if (btn == ButtonId::Down && manualVentActive_) {
        controller.manualVent(false); manualVentActive_ = false;
      } else if (btn == ButtonId::Up && manualAirActive_) {
        controller.manualAirUp(false); manualAirActive_ = false;
      }
    }
  }
}

void StateBoard::handleControllerState(uint32_t now, ta::ctl::Controller& controller) {
  using CS = ta::ctl::State;
  CS cs = controller.state();
  if (cs == CS::ERROR) {
    if (uiState_ != UiState::Error) {
      uiState_ = UiState::Error;
      errorEntryMs_ = now;
    }
    return;
  }

  if (uiState_ == UiState::Seeking) {
    if (cs == CS::AIRUP || cs == CS::VENTING || cs == CS::CHECKING) {
      seenSeekingActivity_ = true;
    }
    if (cs == CS::IDLE && seenSeekingActivity_) {
      // Completed
      showDoneHold_ = true;
      doneHoldUntil_ = now + cfg_.doneHoldMs;
      uiState_ = UiState::Idle; // base state now idle; done hold flag drives display
    }
  }

  if (uiState_ == UiState::Error && cs != CS::ERROR) {
    uiState_ = UiState::Idle;
  }

  if (showDoneHold_ && now >= doneHoldUntil_) {
    showDoneHold_ = false;
  }
}

void StateBoard::handleErrorAutoClear(uint32_t now, ta::ctl::Controller& controller) {
  if (uiState_ == UiState::Error) {
    if (now - errorEntryMs_ >= cfg_.errorAutoClearMs) {
      controller.clearError();
      uiState_ = UiState::Idle;
    }
  }
}

void StateBoard::update(uint32_t now,
                        ta::ctl::Controller& controller,
                        const ta::comms::BoardLink& link) {
  (void)link; // currently only used for icon; kept for future logic
  handleControllerState(now, controller);
  handleErrorAutoClear(now, controller);
}

void StateBoard::buildDisplayModel(ta::display::DisplayModel& m,
                                   const ta::ctl::Controller& controller,
                                   const ta::comms::BoardLink& link,
                                   uint32_t now) const {

  // PSI values
  m.currentPSI = controller.currentPsi();
  m.targetPSI  = targetPsi_;

  // Connection (icon only)
  m.link = (link.isPaired() && link.isRemoteActive())
          ? ta::display::Link::Connected
          : ta::display::Link::Disconnected;

  // Controller activity -> Ctrl
  switch (controller.state()) {
    case ta::ctl::State::IDLE:     m.ctrl = ta::display::Ctrl::Idle; break;
    case ta::ctl::State::AIRUP:    m.ctrl = ta::display::Ctrl::AirUp; break;
    case ta::ctl::State::VENTING:  m.ctrl = ta::display::Ctrl::Venting; break;
    case ta::ctl::State::CHECKING: m.ctrl = ta::display::Ctrl::Checking; break;
    case ta::ctl::State::ERROR:    m.ctrl = ta::display::Ctrl::Error; break;
  }

  // View
  switch (uiState_) {
    case UiState::Idle:    m.view = ta::display::View::Idle; break;
    case UiState::Manual:  m.view = ta::display::View::Manual; break;
    case UiState::Seeking: m.view = ta::display::View::Seeking; break;
    case UiState::Error:   m.view = ta::display::View::Error; break;
  }

  // Done-hold (Seeking “Done!” flash)
  m.seekingShowDoneHold = showDoneHold_ && now < doneHoldUntil_;

  // Error code
  if (controller.state() == ta::ctl::State::ERROR) {
    m.lastErrorCode = controller.errorByte();
  } else {
    m.lastErrorCode = 0;
  }

  // Fields unused on board
  m.batteryPercent = 0;
  m.showReconnectHint = false;
  m.pairingActive = false;
  m.pairingFailed = false;
  m.pairingBusy = false;
}