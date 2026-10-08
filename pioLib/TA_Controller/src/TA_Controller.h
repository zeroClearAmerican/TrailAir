#pragma once
#include <stdint.h>
#include <TA_Types.h>
#include <TA_Errors.h>

namespace trailair {
namespace controller {

/**
 * @brief Actuator outputs the controller drives (hardware, or a fake in tests)
 */
struct IActuatorOutputs {
  virtual ~IActuatorOutputs() = default;
  virtual void setCompressor(bool enable) = 0;
  virtual void setVent(bool open) = 0;
  virtual void stopAll() = 0;
};

/**
 * @brief Controller tuning. Pressure limits here are the single source of truth; the board
 *        UI reads them via PressureController::getConfig().
 */
struct ControllerConfig {
  // Pressure limits
  float minimumPSI = 5.0f;                                ///< Lowest seek target
  float maximumPSI = 50.0f;                               ///< Highest seek target; also the manual air cutoff
  // Done when settled within +/- this. Below 0.5 so the rounded display reads exactly the target,
  // above the sensor's averaged noise (~0.1 PSI) so seeks don't hunt.
  float pressureTolerancePSI = 0.3f;

  // Timing parameters
  uint32_t settleDurationMilliseconds = 1500;             ///< Hose/tire equalize + sensor average window refill
  uint32_t initialBurstDurationMilliseconds = 5000;       ///< Longest probe burst (used to learn a direction's rate)
  uint32_t minimumRunDurationMilliseconds = 500;          ///< Shortest burst
  uint32_t maximumRunDurationMilliseconds = 8000;         ///< Longest predicted burst before re-checking
  uint32_t maximumContinuousDurationMilliseconds = 30UL * 60UL * 1000UL; ///< 30 minute safety limit
  uint32_t manualLeaseMilliseconds = 1000;                ///< Manual output stops unless renewed this often

  // Learning and adaptation parameters (PSI thresholds sit above the sensor noise floor)
  float noChangeThresholdPSI = 0.2f;                      ///< Probe burst moving less than this = no progress
  int maximumNoChangeBursts = 3;                          ///< Consecutive no-progress probes before error
  float approachMarginPSI = 0.2f;                         ///< Aim this far short of target (< tolerance, so we land inside)
  /// Probes in an unlearned direction are sized as if the rate were this fast, so they can't
  /// overshoot unless the real rate is faster. Raise for small tires on a big compressor.
  float maximumExpectedRatePSIPerSecond = 1.0f;
  float pressureNoiseThresholdPSI = 0.2f;                 ///< Smaller changes don't count toward rate learning
  float minimumRateThreshold = 0.001f;                    ///< Minimum rate to consider valid (PSI/sec)
  float minimumCheckIntervalSeconds = 0.02f;              ///< Minimum time window for rate calc
};

/**
 * @brief Pressure seek and manual control with adaptive burst timing.
 *
 * Seek: probe bursts learn the inflate/vent rates, then predicted runs aim just short of
 * target, with settle/check pauses between them. Stalls and impossible seeks raise errors.
 * Decisions use settled readings only: while air flows the sensor reads the hose, not the tire
 * (high when inflating, low when venting), so live readings only enforce the maximumPSI cap.
 *
 * Manual: manualAirUp/manualVent(true) start or renew a lease of manualLeaseMilliseconds.
 * The caller must keep renewing while the button is held; if renewals stop (lost radio
 * release, dead remote) the controller stops on its own. Manual air also stops with an
 * OverPressure error at maximumPSI.
 *
 * All commands take effect synchronously: after startSeek() the state is already AirUp,
 * Venting or (already at target) Idle. Time is always passed in, never read.
 */
class PressureController {
public:
  void begin(IActuatorOutputs* outputs, const ControllerConfig& config);

  /// Call every loop with the latest filtered pressure
  void update(uint32_t now, float currentPSI);

  /// Seek to targetPSI (clamped to the configured limits)
  void startSeek(float targetPSI, uint32_t now);
  /// Start/renew (true) or stop (false) manual air. A stop only affects manual air.
  void manualAirUp(bool active, uint32_t now);
  /// Start/renew (true) or stop (false) manual vent. A stop only affects manual vent.
  void manualVent(bool active, uint32_t now);
  /// Stop any seek or manual action (does not clear an error)
  void cancel();
  void clearError();

  ControllerState getState() const { return _state; }
  trailair::errors::ErrorCode getError() const { return _errorCode; }
  float getTargetPSI() const { return _targetPSI; }
  float getCurrentPSI() const { return _currentPSI; }
  bool isManualActive() const { return _isManualActive; }
  const ControllerConfig& getConfig() const { return _config; }

private:
  void manual_(ControllerState direction, bool active, uint32_t now);
  void stopManual_();
  void runOutputs_(ControllerState direction);
  void handleRunPhase_(uint32_t now);
  void handleCheckingPhase_(uint32_t now);
  void endBurst_(uint32_t now);
  void scheduleBurst_(ControllerState direction, uint32_t durationMilliseconds, uint32_t now);
  void scheduleProbe_(float remaining, uint32_t now);
  void enterError_(trailair::errors::ErrorCode code);
  void stopAllOutputs_();

  IActuatorOutputs* _outputs = nullptr;
  ControllerConfig _config{};

  ControllerState _state = ControllerState::Idle;
  trailair::errors::ErrorCode _errorCode = trailair::errors::ErrorCode::None;
  float _targetPSI = 0.0f;
  float _currentPSI = 0.0f;

  // Manual
  bool _isManualActive = false;
  uint32_t _manualLeaseEnd = 0;

  // Burst/check phase
  bool _skipStallCheck = false;
  uint32_t _phaseStartTime = 0;
  uint32_t _phaseEndTime = 0;
  uint32_t _lastBurstEndTime = 0;
  float _phaseStartPressure = 0.0f;

  // Learned rates
  float _inflationRatePSIPerSecond = 0.0f;
  float _deflationRatePSIPerSecond = 0.0f;
  int _inflationSampleCount = 0;
  int _deflationSampleCount = 0;
  int _noChangeDetectionCount = 0;
};

} // namespace controller
} // namespace trailair
