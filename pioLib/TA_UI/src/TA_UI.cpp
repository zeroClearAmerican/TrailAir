#include "TA_UI.h"

namespace ta { namespace ui {

void UiStateMachine::update(uint32_t now, DeviceActions& dev, Ctrl ctrlState) {
  // Controller error gates Error view
  if (ctrlState == Ctrl::Error) {
    if (view_ != View::Error) {
      view_ = View::Error;
      errorEntryMs_ = now;
    } else {
      // Optional auto-clear trigger via strategy if desired by device
      if (cfg_.errorAutoClearMs > 0 && (now - errorEntryMs_) >= cfg_.errorAutoClearMs) {
        dev.clearError();
        // Wait for ctrlState to change before leaving Error view
      }
    }
    return;
  }

  // If connected dimension matters (remote), exit to Disconnected
  if (!dev.isConnected()) {
    // do not force Disconnected for board (isConnected true by default)
    view_ = View::Disconnected;
    return;
  }

  // Reconnected - restore from Disconnected to Idle
  if (view_ == View::Disconnected) {
    view_ = View::Idle;
  }

  // Seeking completion -> Done hold then Idle
  if (view_ == View::Seeking) {
    if (ctrlState == Ctrl::AirUp || ctrlState == Ctrl::Venting || ctrlState == Ctrl::Checking) {
      seenSeekingActivity_ = true;
    }
    if (ctrlState == Ctrl::Idle && seenSeekingActivity_) {
      showDoneHold_ = true;
      doneHoldUntil_ = now + cfg_.doneHoldMs;
      view_ = View::Idle;
    }
  }

  if (view_ == View::Error && ctrlState != Ctrl::Error) {
    view_ = View::Idle;
  }

  if (showDoneHold_ && now >= doneHoldUntil_) {
    showDoneHold_ = false;
  }
}

void UiStateMachine::onButton(const ButtonEvent& e, DeviceActions& dev) {
  switch (view_) {
    case View::Idle: {
      if (e.action == Action::Click) {
        if (e.id == Button::Left) {
          view_ = View::Manual;
          manualVentActive_ = false;
          manualAirActive_ = false;
          dev.cancel();
        } else if (e.id == Button::Right) {
          dev.startSeek(targetPsi_);
          view_ = View::Seeking;
          seenSeekingActivity_ = false;
          showDoneHold_ = false;
        } else if (e.id == Button::Up) {
          targetPsi_ += cfg_.stepSmall; clampTarget_();
        } else if (e.id == Button::Down) {
          targetPsi_ -= cfg_.stepSmall; clampTarget_();
        }
      }
      break;
    }

    case View::Manual: {
      if (e.action == Action::Click && e.id == Button::Left) {
        if (manualVentActive_) dev.manualVent(false);
        if (manualAirActive_) dev.manualAirUp(false);
        manualVentActive_ = manualAirActive_ = false;
        view_ = View::Idle;
        break;
      }
      if (e.action == Action::Pressed) {
        if (e.id == Button::Down && !manualVentActive_) { dev.manualVent(true); manualVentActive_ = true; }
        if (e.id == Button::Up   && !manualAirActive_)  { dev.manualAirUp(true); manualAirActive_ = true; }
      } else if (e.action == Action::Released) {
        if (e.id == Button::Down && manualVentActive_) { dev.manualVent(false); manualVentActive_ = false; }
        if (e.id == Button::Up   && manualAirActive_)  { dev.manualAirUp(false); manualAirActive_ = false; }
      }
      break;
    }

    case View::Seeking: {
      if (e.action == Action::Click && e.id == Button::Right) {
        dev.cancel();
        view_ = View::Idle; // base state; done-hold is independent flag
        showDoneHold_ = false;
      }
      break;
    }

    case View::Error: {
      if (e.action == Action::Click && e.id == Button::Right) {
        dev.clearError();
      }
      break;
    }

    case View::Disconnected: {
      // shared layer doesn't do pairing; device may map buttons separately
      break;
    }

    case View::Pairing: {
      break;
    }
  }
}

}} // namespace ta::ui
