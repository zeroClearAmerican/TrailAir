#include "TA_Controller.h"
#include <math.h>

namespace trailair {
namespace controller {

using trailair::errors::ErrorCode;

namespace {
  /// Wrap-safe "now >= t" for millisecond timestamps
  inline bool reached(uint32_t now, uint32_t t) { return static_cast<int32_t>(now - t) >= 0; }
}

void PressureController::begin(IActuatorOutputs* outputs, const ControllerConfig& config) {
  _outputs = outputs;
  _config = config;
  _state = ControllerState::Idle;
  _errorCode = ErrorCode::None;
  _isManualActive = false;
  _targetPSI = 0.0f;
  stopAllOutputs_();
}

void PressureController::stopAllOutputs_() {
  if (_outputs) _outputs->stopAll();
}

void PressureController::runOutputs_(ControllerState direction) {
  if (!_outputs) return;
  if (direction == ControllerState::AirUp) _outputs->setCompressor(true);
  else _outputs->setVent(true);
}

// ---------------------------------------------------------------------------------------------
// Manual
// ---------------------------------------------------------------------------------------------

void PressureController::manualAirUp(bool active, uint32_t now) { manual_(ControllerState::AirUp, active, now); }
void PressureController::manualVent(bool active, uint32_t now)  { manual_(ControllerState::Venting, active, now); }

// One direction at a time: the latest press wins, and a release only stops its own direction
// (releasing Down after switching to Up must not kill Up). Repeated presses just renew the lease.
void PressureController::manual_(ControllerState direction, bool active, uint32_t now) {
  bool running = _isManualActive && _state == direction;
  if (active) {
    if (_state == ControllerState::Error) return;  // error must be cleared first
    if (!running) {
      stopAllOutputs_();  // ends a seek or the other manual direction
      runOutputs_(direction);
      _state = direction;
      _isManualActive = true;
    }
    _manualLeaseEnd = now + _config.manualLeaseMilliseconds;
  } else if (running) {
    stopManual_();
  }
}

void PressureController::stopManual_() {
  stopAllOutputs_();
  _isManualActive = false;
  _state = ControllerState::Idle;
}

// ---------------------------------------------------------------------------------------------
// Commands
// ---------------------------------------------------------------------------------------------

void PressureController::cancel() {
  stopAllOutputs_();
  _isManualActive = false;
  _targetPSI = 0.0f;
  if (_state != ControllerState::Error) _state = ControllerState::Idle;
}

void PressureController::clearError() {
  if (_state != ControllerState::Error) return;
  _errorCode = ErrorCode::None;
  _state = ControllerState::Idle;
}

void PressureController::startSeek(float targetPSI, uint32_t now) {
  if (_state == ControllerState::Error) return;
  if (targetPSI < _config.minimumPSI) targetPSI = _config.minimumPSI;
  if (targetPSI > _config.maximumPSI) targetPSI = _config.maximumPSI;
  _targetPSI = targetPSI;
  _isManualActive = false;
  _inflationRatePSIPerSecond = 0.0f;
  _deflationRatePSIPerSecond = 0.0f;
  _inflationSampleCount = 0;
  _deflationSampleCount = 0;
  _noChangeDetectionCount = 0;

  stopAllOutputs_();
  float difference = _targetPSI - _currentPSI;
  if (fabsf(difference) <= _config.pressureTolerancePSI) {
    _state = ControllerState::Idle;
    return;
  }
  scheduleProbe_(difference, now);
}

void PressureController::enterError_(ErrorCode code) {
  stopAllOutputs_();
  _errorCode = code;
  _isManualActive = false;
  _state = ControllerState::Error;
}

// ---------------------------------------------------------------------------------------------
// Seek phases
// ---------------------------------------------------------------------------------------------

void PressureController::scheduleBurst_(ControllerState direction, uint32_t durationMilliseconds, uint32_t now) {
  _phaseStartPressure = _currentPSI;
  _phaseStartTime = now;
  _phaseEndTime = now + durationMilliseconds;
  _skipStallCheck = false;
  _state = direction;
  runOutputs_(direction);
}

// Burst in an unlearned direction. Sized so that even at maximumExpectedRate it stops short of
// target: full length far from target (learns the rate, detects stalls), short when close
// (e.g. correcting an overshoot). A shortened probe may legitimately move less than the noise
// floor, so only full-length probes count toward stall detection.
void PressureController::scheduleProbe_(float remaining, uint32_t now) {
  float safeMs = 1000.0f * fabsf(remaining) / _config.maximumExpectedRatePSIPerSecond;
  uint32_t duration = _config.initialBurstDurationMilliseconds;
  if (safeMs < duration) duration = static_cast<uint32_t>(safeMs);
  if (duration < _config.minimumRunDurationMilliseconds) duration = _config.minimumRunDurationMilliseconds;
  scheduleBurst_(remaining > 0 ? ControllerState::AirUp : ControllerState::Venting, duration, now);
  _skipStallCheck = duration < _config.initialBurstDurationMilliseconds;
}

void PressureController::endBurst_(uint32_t now) {
  stopAllOutputs_();
  _state = ControllerState::Checking;
  _lastBurstEndTime = now;
  _phaseEndTime = now + _config.settleDurationMilliseconds;
}

void PressureController::handleRunPhase_(uint32_t now) {
  // Timed bursts; Checking measures the settled result. The live reading is hose pressure (reads
  // high while inflating), so it's only used as a conservative hard cap.
  bool overCap = _state == ControllerState::AirUp && _currentPSI >= _config.maximumPSI;
  if (overCap) _skipStallCheck = true;  // cut short on purpose, not a stall
  if (overCap || reached(now, _phaseEndTime)) endBurst_(now);
}

void PressureController::handleCheckingPhase_(uint32_t now) {
  if (!reached(now, _phaseEndTime)) return;

  float deltaTimeSeconds = (_lastBurstEndTime - _phaseStartTime) / 1000.0f;
  float deltaPressure = _currentPSI - _phaseStartPressure;

  if (deltaTimeSeconds > _config.minimumCheckIntervalSeconds) {
    // Rates drift as pressure changes (compressor slows as it climbs, vent slows as it falls),
    // so blend 50/50 with the latest burst rather than averaging the whole seek.
    float rate = fabsf(deltaPressure) / deltaTimeSeconds;
    if (deltaPressure > _config.pressureNoiseThresholdPSI) {
      _inflationRatePSIPerSecond = _inflationSampleCount ? 0.5f * (_inflationRatePSIPerSecond + rate) : rate;
      _inflationSampleCount++;
    } else if (deltaPressure < -_config.pressureNoiseThresholdPSI) {
      _deflationRatePSIPerSecond = _deflationSampleCount ? 0.5f * (_deflationRatePSIPerSecond + rate) : rate;
      _deflationSampleCount++;
    }
    if (_skipStallCheck || fabsf(deltaPressure) >= _config.noChangeThresholdPSI) {
      _noChangeDetectionCount = 0;
    } else if (++_noChangeDetectionCount >= _config.maximumNoChangeBursts) {
      enterError_(ErrorCode::NoChange);
      return;
    }
  }

  float remaining = _targetPSI - _currentPSI;
  if (fabsf(remaining) <= _config.pressureTolerancePSI) {
    stopAllOutputs_();
    _state = ControllerState::Idle;
    return;
  }

  bool needInflation = remaining > 0;
  ControllerState direction = needInflation ? ControllerState::AirUp : ControllerState::Venting;
  float rate = needInflation ? _inflationRatePSIPerSecond : _deflationRatePSIPerSecond;
  int samples = needInflation ? _inflationSampleCount : _deflationSampleCount;

  if (samples > 0 && rate > _config.minimumRateThreshold) {
    // Predicted run toward target, aiming just short of it
    uint32_t predictedFullDuration = static_cast<uint32_t>(1000.0f * (fabsf(remaining) / rate));
    if (predictedFullDuration > _config.maximumContinuousDurationMilliseconds) {
      enterError_(ErrorCode::ExcessiveTime);
      return;
    }
    float aimDistance = fmaxf(0.0f, fabsf(remaining) - _config.approachMarginPSI);
    uint32_t runDuration = static_cast<uint32_t>(1000.0f * (aimDistance / rate));
    if (runDuration < _config.minimumRunDurationMilliseconds) runDuration = _config.minimumRunDurationMilliseconds;
    if (runDuration > _config.maximumRunDurationMilliseconds) runDuration = _config.maximumRunDurationMilliseconds;
    scheduleBurst_(direction, runDuration, now);
    // A run expected to move well past the noise floor must actually move (catches a compressor
    // or valve dying mid-seek); a short trim run legitimately might not.
    _skipStallCheck = rate * (runDuration / 1000.0f) < 2.0f * _config.noChangeThresholdPSI;
    return;
  }

  scheduleProbe_(remaining, now);
}

// ---------------------------------------------------------------------------------------------

void PressureController::update(uint32_t now, float currentPSI) {
  _currentPSI = currentPSI;

  if (_isManualActive) {
    // Seeks are clamped to maximumPSI, but manual air isn't, so cap it here
    if (_state == ControllerState::AirUp && currentPSI >= _config.maximumPSI) {
      enterError_(ErrorCode::OverPressure);
    } else if (reached(now, _manualLeaseEnd)) {
      stopManual_();  // nobody renewed: lost release, silent remote
    }
    return;
  }

  switch (_state) {
    case ControllerState::AirUp:
    case ControllerState::Venting:
      handleRunPhase_(now);
      break;
    case ControllerState::Checking:
      handleCheckingPhase_(now);
      break;
    case ControllerState::Idle:
      stopAllOutputs_();  // belt and braces: idle means everything off
      break;
    case ControllerState::Error:
      break;
  }
}

} // namespace controller
} // namespace trailair
