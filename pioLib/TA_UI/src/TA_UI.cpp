#include "TA_UI.h"

namespace trailair {
namespace ui {

void UserInterfaceStateMachine::update(uint32_t now, DeviceActions& deviceActions, ControllerState controllerState) {
  // Controller error gates Error view
  if (controllerState == ControllerState::Error) {
    if (_viewState != ViewState::Error) {
      _viewState = ViewState::Error;
      _errorEntryTime = now;
    } else {
      // Optional auto-clear trigger via strategy if desired by device
      if (_config.errorAutoClearDurationMilliseconds > 0 &&
          (now - _errorEntryTime) >= _config.errorAutoClearDurationMilliseconds) {
        deviceActions.clearError();
        // Wait for controllerState to change before leaving Error view
      }
    }
    return;
  }

  // If connected dimension matters (remote), exit to Disconnected
  if (!deviceActions.isConnected()) {
    // do not force Disconnected for board (isConnected true by default)
    _viewState = ViewState::Disconnected;
    return;
  }

  // Reconnected - restore from Disconnected to Idle
  if (_viewState == ViewState::Disconnected) {
    _viewState = ViewState::Idle;
  }

  // Seeking completion -> Done hold then Idle
  if (_viewState == ViewState::Seeking) {
    if (controllerState == ControllerState::AirUp ||
        controllerState == ControllerState::Venting ||
        controllerState == ControllerState::Checking) {
      _hasSeenSeekingActivity = true;
    }
    if (controllerState == ControllerState::Idle && _hasSeenSeekingActivity) {
      _showDoneHold = true;
      _doneHoldEndTime = now + _config.doneHoldDurationMilliseconds;
      _viewState = ViewState::Idle;
    }
  }

  if (_viewState == ViewState::Error && controllerState != ControllerState::Error) {
    _viewState = ViewState::Idle;
  }

  if (_showDoneHold && now >= _doneHoldEndTime) {
    _showDoneHold = false;
  }
}

void UserInterfaceStateMachine::onButton(const ButtonEvent& event, DeviceActions& deviceActions) {
  switch (_viewState) {
    case ViewState::Idle: {
      if (event.action == ButtonAction::Click) {
        if (event.id == ButtonId::Left) {
          _viewState = ViewState::Manual;
          _isManualVentActive = false;
          _isManualAirActive = false;
          deviceActions.cancel();
        } else if (event.id == ButtonId::Right) {
          deviceActions.startSeek(_targetPSI);
          _viewState = ViewState::Seeking;
          _hasSeenSeekingActivity = false;
          _showDoneHold = false;
        } else if (event.id == ButtonId::Up) {
          _targetPSI += _config.stepSize;
          clampTargetPressure();
        } else if (event.id == ButtonId::Down) {
          _targetPSI -= _config.stepSize;
          clampTargetPressure();
        }
      }
      break;
    }

    case ViewState::Manual: {
      if (event.action == ButtonAction::Click && event.id == ButtonId::Left) {
        // Exit manual mode - stop everything
        deviceActions.manualVent(false);
        deviceActions.manualAirUp(false);
        _isManualVentActive = _isManualAirActive = false;
        _viewState = ViewState::Idle;
        break;
      }
      if (event.action == ButtonAction::Pressed) {
        if (event.id == ButtonId::Down) {
          deviceActions.manualVent(true);
          _isManualVentActive = true;
        }
        if (event.id == ButtonId::Up) {
          deviceActions.manualAirUp(true);
          _isManualAirActive = true;
        }
      } else if (event.action == ButtonAction::Released) {
        if (event.id == ButtonId::Down) {
          deviceActions.manualVent(false);
          _isManualVentActive = false;
        }
        if (event.id == ButtonId::Up) {
          deviceActions.manualAirUp(false);
          _isManualAirActive = false;
        }
      }
      break;
    }

    case ViewState::Seeking: {
      if (event.action == ButtonAction::Click && event.id == ButtonId::Right) {
        deviceActions.cancel();
        _viewState = ViewState::Idle;
        _showDoneHold = false;
      }
      break;
    }

    case ViewState::Error: {
      if (event.action == ButtonAction::Click && event.id == ButtonId::Right) {
        deviceActions.clearError();
      }
      break;
    }

    case ViewState::Disconnected: {
      // shared layer doesn't do pairing; device may map buttons separately
      break;
    }

    case ViewState::Pairing: {
      break;
    }
  }
}

} // namespace ui
} // namespace trailair
