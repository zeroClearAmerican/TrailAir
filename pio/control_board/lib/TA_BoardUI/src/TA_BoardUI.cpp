#include "TA_BoardUI.h"
#include <math.h>

namespace trailair {
namespace boardui {

namespace {
  inline bool reached(uint32_t now, uint32_t t) { return static_cast<int32_t>(now - t) >= 0; }
}

void BoardUI::begin(const trailair::config::UserInterfaceConfiguration& cfg) {
  cfg_ = cfg;
  view_ = View::Idle;
  target_ = cfg_.defaultTargetPressurePSI;
  stepTarget_(0.0f);  // clamp to the controller's limits
}

void BoardUI::stepTarget_(float delta) {
  const auto& limits = ctl_.getConfig();
  target_ += delta;
  if (target_ < limits.minimumPSI) target_ = limits.minimumPSI;
  if (target_ > limits.maximumPSI) target_ = limits.maximumPSI;
}

void BoardUI::onButton(const ButtonEvent& e, uint32_t now) {
  bool click = e.action == ButtonAction::Click;

  switch (view_) {
    case View::Idle:
    case View::Done:
      if (!click) break;
      if (e.id == ButtonId::Left) {
        ctl_.cancel();
        view_ = View::Manual;
      } else if (e.id == ButtonId::Right) {
        ctl_.startSeek(target_, now);
        view_ = View::Seeking;  // update() moves on to Done once the controller is Idle
      } else {
        // SmartButton merges rapid taps into one Click with clickCount=N
        int taps = e.clickCount > 1 ? e.clickCount : 1;
        stepTarget_((e.id == ButtonId::Up ? 1.0f : -1.0f) * cfg_.pressureStepPSI * taps);
        view_ = View::Idle;
      }
      break;

    case View::Manual:
      if (click && e.id == ButtonId::Left) {
        ctl_.cancel();
        view_ = View::Idle;
      } else if (e.id == ButtonId::Up || e.id == ButtonId::Down) {
        bool up = e.id == ButtonId::Up;
        if (e.action == ButtonAction::Pressed) {
          // Also renews the lease when the remote repeats Pressed while held
          if (up) ctl_.manualAirUp(true, now); else ctl_.manualVent(true, now);
        } else if (e.action == ButtonAction::Released) {
          if (up) ctl_.manualAirUp(false, now); else ctl_.manualVent(false, now);
        }
      }
      break;

    case View::Seeking:
      if (click && e.id == ButtonId::Right) {
        ctl_.cancel();
        view_ = View::Idle;
      }
      break;

    case View::Error:
      if (click && e.id == ButtonId::Right) ctl_.clearError();  // update() returns to Idle
      break;

    case View::Disconnected:
    case View::Pairing:
      break;  // remote-only screens
  }
}

void BoardUI::update(uint32_t now, bool upHeld, bool downHeld) {
  ControllerState state = ctl_.getState();

  if (state == ControllerState::Error) {
    if (view_ != View::Error) {
      view_ = View::Error;
      errorSince_ = now;
    } else if (cfg_.errorAutoClearDurationMilliseconds > 0 &&
               now - errorSince_ >= cfg_.errorAutoClearDurationMilliseconds) {
      ctl_.clearError();
    }
    return;
  }
  if (view_ == View::Error) view_ = View::Idle;

  // startSeek() sets the controller state synchronously, so Idle while Seeking means finished
  // (or already at target), never "not started yet"
  if (view_ == View::Seeking && state == ControllerState::Idle) {
    view_ = View::Done;
    doneUntil_ = now + cfg_.doneHoldDurationMilliseconds;
  }
  if (view_ == View::Done && reached(now, doneUntil_)) view_ = View::Idle;

  // Board buttons are wired, so their hold is real until they're released: keep the lease alive.
  // Only renew the direction that's running, so a remote press of the other one still wins.
  if (view_ == View::Manual && ctl_.isManualActive()) {
    if (state == ControllerState::AirUp && upHeld) ctl_.manualAirUp(true, now);
    else if (state == ControllerState::Venting && downHeld) ctl_.manualVent(true, now);
  }
}

trailair::protocol::Status BoardUI::status() const {
  trailair::protocol::Status s;
  s.state = ctl_.getState();
  s.view = view_;
  s.currentPSI = roundf(ctl_.getCurrentPSI());
  s.targetPSI = roundf(target_);
  s.errorCode = static_cast<uint8_t>(ctl_.getError());
  return s;
}

void BoardUI::fillDisplay(trailair::display::DisplayModel& m) const {
  m.view = view_;
  m.controllerState = ctl_.getState();
  m.currentPressurePSI = ctl_.getCurrentPSI();
  m.targetPressurePSI = target_;
  m.errorCode = static_cast<uint8_t>(ctl_.getError());
  m.showLinkIcon = false;
  m.showBatteryIcon = false;
}

} // namespace boardui
} // namespace trailair
