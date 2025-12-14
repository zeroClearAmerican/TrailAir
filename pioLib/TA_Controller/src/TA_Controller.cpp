#ifndef UNIT_TEST
#include <Arduino.h>
#else
// Stub for unit tests
#include <cstdint>
inline uint32_t millis() { return 0; }
#endif

#include "TA_Controller.h"
#ifndef UNIT_TEST
#include "TA_Actuators.h"
#endif
#include <math.h>

using namespace trailair::controller;

// Adapter implementations
#ifndef UNIT_TEST
void PressureController::ActuatorAdapter::setCompressor(bool enable) {
  if (hardware) hardware->setCompressor(enable);
}
void PressureController::ActuatorAdapter::setVent(bool open) {
  if (hardware) hardware->setVent(open);
}
void PressureController::ActuatorAdapter::stopAll() {
  if (hardware) hardware->stopAll();
}
#else
// Stub implementations for unit tests (vtable needs these)
void PressureController::ActuatorAdapter::setCompressor(bool) {}
void PressureController::ActuatorAdapter::setVent(bool) {}
void PressureController::ActuatorAdapter::stopAll() {}
#endif

#ifndef UNIT_TEST
void PressureController::begin(ta::act::Actuators* actuators, const ControllerConfig& config) {
  _config = config;
  _actuatorAdapter.hardware = actuators;
  _outputs = actuators ? static_cast<IActuatorOutputs*>(&_actuatorAdapter) : nullptr;
  reset();
}
#endif

void PressureController::begin(IActuatorOutputs* outputs, const ControllerConfig& config) {
  _config = config;
  _outputs = outputs;
  _actuatorAdapter.hardware = nullptr;
  reset();
}

void PressureController::reset() {
  _state = ControllerState::Idle;
  _previousState = ControllerState::Idle;
  _targetPSI = 0.0f;
  _isManualActive = false;
  _isContinuousPhase = false;
  _inflationRatePSIPerSecond = 0.0f;
  _deflationRatePSIPerSecond = 0.0f;
  _inflationSampleCount = 0;
  _deflationSampleCount = 0;
  _noChangeDetectionCount = 0;
  _errorCode = ErrorCode::None;
}

void PressureController::stopAllOutputs() {
  if (_outputs) _outputs->stopAll();
}

char PressureController::getStatusCharacter() const {
  switch (_state) {
    case ControllerState::Idle:     return 'I';
    case ControllerState::AirUp:    return 'U';
    case ControllerState::Venting:  return 'V';
    case ControllerState::Checking: return 'C';
    case ControllerState::Error:    return 'E';
  }
  return 'I';
}

void PressureController::enterState(ControllerState state, uint32_t now) {
  _previousState = _state;
  _state = state;
  if (state == ControllerState::Checking) {
    _phaseEndTime = now + _config.settleDurationMilliseconds;
  }
}

void PressureController::manualAirUp(bool active) {
  _isManualActive = active;
  if (!_outputs) return;
  if (active) {
    _outputs->setCompressor(true);
    _state = ControllerState::AirUp;
  } else {
    stopAllOutputs();
    _state = ControllerState::Idle;
  }
}

void PressureController::manualVent(bool active) {
  _isManualActive = active;
  if (!_outputs) return;
  if (active) {
    _outputs->setVent(true);
    _state = ControllerState::Venting;
  } else {
    stopAllOutputs();
    _state = ControllerState::Idle;
  }
}

void PressureController::cancel() {
  _isManualActive = false;
  _isContinuousPhase = false;
  stopAllOutputs();
  _targetPSI = 0.0f;
  if (_state != ControllerState::Error) {
    _state = ControllerState::Idle;
  }
}

void PressureController::clearError() {
  if (_state == ControllerState::Error) {
    _errorCode = ErrorCode::None;
    _state = ControllerState::Idle;
  }
}

void PressureController::startSeek(float targetPSI) {
  if (targetPSI < _config.minimumPSI) targetPSI = _config.minimumPSI;
  if (targetPSI > _config.maximumPSI) targetPSI = _config.maximumPSI;
  _targetPSI = targetPSI;
  _isManualActive = false;
  _isContinuousPhase = false;
  _inflationRatePSIPerSecond = 0.0f;
  _deflationRatePSIPerSecond = 0.0f;
  _inflationSampleCount = 0;
  _deflationSampleCount = 0;
  _noChangeDetectionCount = 0;

  stopAllOutputs();
  float difference = _targetPSI - _currentPSI;
  if (fabsf(difference) <= _config.pressureTolerancePSI) {
    _state = ControllerState::Idle;
    return;
  }
  scheduleBurst(
    difference > 0 ? ControllerState::AirUp : ControllerState::Venting,
    _config.initialBurstDurationMilliseconds,
    millis()
  );
}

void PressureController::scheduleBurst(ControllerState direction, uint32_t durationMilliseconds, uint32_t now) {
  _phaseStartPressure = _currentPSI;
  _phaseStartTime = now;
  _phaseEndTime = now + durationMilliseconds;
  _isContinuousPhase = false;
  if (!_outputs) return;
  if (direction == ControllerState::AirUp) {
    _outputs->setCompressor(true);
    _state = ControllerState::AirUp;
  } else {
    _outputs->setVent(true);
    _state = ControllerState::Venting;
  }
}

void PressureController::enterErrorState(ErrorCode errorCode, const char* /*reason*/) {
  _errorCode = errorCode;
  stopAllOutputs();
  _isManualActive = false;
  _isContinuousPhase = false;
  _state = ControllerState::Error;
}

void PressureController::handleRunPhase(ControllerState runState, uint32_t now) {
  float remaining = _targetPSI - _currentPSI;
  if (fabsf(remaining) <= _config.pressureTolerancePSI) {
    stopAllOutputs();
    enterState(ControllerState::Checking, now);
    _lastBurstEndTime = now;
    return;
  }
  // End burst / continuous phases
  if (now >= _phaseEndTime) {
    stopAllOutputs();
    enterState(ControllerState::Checking, now);
    _lastBurstEndTime = now;
    return;
  }
  (void)runState; // placeholder for per-mode nuances
}

void PressureController::handleCheckingPhase(uint32_t now) {
  if (now < _phaseEndTime) return;

  float deltaTimeSeconds = (_lastBurstEndTime > _phaseStartTime)
                         ? (_lastBurstEndTime - _phaseStartTime) / 1000.0f
                         : (now - _phaseStartTime) / 1000.0f;
  float deltaPressure = _currentPSI - _phaseStartPressure;

  if (deltaTimeSeconds > _config.minimumCheckIntervalSeconds) {
    if (deltaPressure > _config.pressureNoiseThresholdPSI) {
      _inflationRatePSIPerSecond = (_inflationRatePSIPerSecond * _inflationSampleCount + (fabsf(deltaPressure) / deltaTimeSeconds)) / (_inflationSampleCount + 1);
      _inflationSampleCount++;
    } else if (deltaPressure < -_config.pressureNoiseThresholdPSI) {
      _deflationRatePSIPerSecond = (_deflationRatePSIPerSecond * _deflationSampleCount + (fabsf(deltaPressure) / deltaTimeSeconds)) / (_deflationSampleCount + 1);
      _deflationSampleCount++;
    }
    if (!_isContinuousPhase) {
      if (fabsf(deltaPressure) < _config.noChangeThresholdPSI) {
        _noChangeDetectionCount++;
        if (_noChangeDetectionCount >= _config.maximumNoChangeBursts) {
          enterErrorState(ErrorCode::NoChange, "No change");
          return;
        }
      } else {
        _noChangeDetectionCount = 0;
      }
    } else {
      _noChangeDetectionCount = 0;
    }
  }

  float remaining = _targetPSI - _currentPSI;
  if (fabsf(remaining) <= _config.pressureTolerancePSI) {
    _state = ControllerState::Idle;
    stopAllOutputs();
    return;
  }

  bool needInflation = remaining > 0;
  bool haveLearnedRate = needInflation
    ? (_inflationSampleCount >= 2 && _inflationRatePSIPerSecond > _config.minimumRateThreshold)
    : (_deflationSampleCount >= 2 && _deflationRatePSIPerSecond > _config.minimumRateThreshold);
    
  if (haveLearnedRate) {
    float rate = needInflation ? _inflationRatePSIPerSecond : _deflationRatePSIPerSecond;
    uint32_t predictedFullDuration = (uint32_t)(1000.0f * (fabsf(remaining) / rate));
    if (predictedFullDuration > _config.maximumContinuousDurationMilliseconds) {
      enterErrorState(ErrorCode::ExcessiveTime, "Too long");
      return;
    }
    float aimDistance = fmaxf(0.0f, fabsf(remaining) - _config.approachMarginPSI);
    uint32_t runDuration = (uint32_t)(1000.0f * (aimDistance / rate));
    if (runDuration < _config.minimumRunDurationMilliseconds) {
      runDuration = _config.minimumRunDurationMilliseconds;
    }
    if (runDuration > _config.maximumRunDurationMilliseconds) {
      runDuration = _config.maximumRunDurationMilliseconds;
    }
    // schedule continuous
    _isContinuousPhase = true;
    _phaseStartPressure = _currentPSI;
    _phaseStartTime = now;
    _phaseEndTime = now + runDuration;
    if (needInflation) {
      _state = ControllerState::AirUp;
      if (_outputs) _outputs->setCompressor(true);
    } else {
      _state = ControllerState::Venting;
      if (_outputs) _outputs->setVent(true);
    }
  } else {
    scheduleBurst(
      needInflation ? ControllerState::AirUp : ControllerState::Venting,
      _config.initialBurstDurationMilliseconds,
      now
    );
  }
}

void PressureController::handleIdleState(uint32_t /*now*/) {
  stopAllOutputs();
}

void PressureController::update(uint32_t currentTimeMilliseconds, float currentPressurePSI) {
  _currentPSI = currentPressurePSI;

  // Manual mode is now controlled purely by manualAirUp()/manualVent() calls
  // No watchdog needed - Released events stop the action explicitly
  
  if (_state == ControllerState::Error || _isManualActive) return;

  switch (_state) {
    case ControllerState::AirUp:
    case ControllerState::Venting:
      handleRunPhase(_state, currentTimeMilliseconds);
      break;
    case ControllerState::Checking:
      handleCheckingPhase(currentTimeMilliseconds);
      break;
    case ControllerState::Idle:
    default:
      handleIdleState(currentTimeMilliseconds);
      break;
  }
}