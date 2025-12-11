#pragma once
#include <stdint.h>
#include "TA_Protocol.h"
#include <TA_Errors.h>

// Forward declaration for board-specific actuators
namespace ta { namespace act { class Actuators; } }

namespace trailair {
namespace controller {

/**
 * @brief Controller states for pressure management
 */
enum class ControllerState {
  Idle,      ///< No active pressure adjustment
  AirUp,     ///< Actively inflating (compressor on)
  Venting,   ///< Actively deflating (vent open)
  Checking,  ///< Settling/stabilizing period between bursts
  Error      ///< Error state requiring user intervention
};

/**
 * @brief Error codes specific to controller operation
 * 
 * Maps to shared error catalog for wire/display compatibility.
 */
enum class ErrorCode : uint8_t {
  None = static_cast<uint8_t>(trailair::errors::ErrorCode::None),
  NoChange = static_cast<uint8_t>(trailair::errors::ErrorCode::NoChange),
  ExcessiveTime = static_cast<uint8_t>(trailair::errors::ErrorCode::ExcessiveTime),
  Unknown = static_cast<uint8_t>(trailair::errors::ErrorCode::Unknown)
};

/**
 * @brief Abstract interface for actuator control
 * 
 * Injectable outputs for unit testing or alternative hardware drivers.
 * Allows controller logic to be tested independently of hardware.
 */
struct IActuatorOutputs {
  virtual ~IActuatorOutputs() = default;
  
  /// @brief Control compressor state
  /// @param enable true to turn on, false to turn off
  virtual void setCompressor(bool enable) = 0;
  
  /// @brief Control vent valve state
  /// @param open true to open valve, false to close
  virtual void setVent(bool open) = 0;
  
  /// @brief Emergency stop - turn off all actuators
  virtual void stopAll() = 0;
};

/**
 * @brief Configuration parameters for pressure controller
 */
struct ControllerConfig {
  // Pressure limits
  float minimumPSI = 5.0f;                                ///< Minimum allowed pressure
  float maximumPSI = 50.0f;                               ///< Maximum allowed pressure
  float pressureTolerancePSI = 0.1f;                      ///< Acceptable error around target
  
  // Timing parameters
  uint32_t settleDurationMilliseconds = 1000;             ///< Settling time between bursts
  uint32_t initialBurstDurationMilliseconds = 5000;       ///< First burst duration
  uint32_t minimumRunDurationMilliseconds = 1000;         ///< Minimum continuous run time
  uint32_t maximumRunDurationMilliseconds = 4000;         ///< Maximum continuous run time
  uint32_t manualRefreshTimeoutMilliseconds = 1000;       ///< Manual mode watchdog timeout
  uint32_t maximumContinuousDurationMilliseconds = 30UL * 60UL * 1000UL; ///< 30 minute safety limit
  
  // Learning and adaptation parameters
  float noChangeThresholdPSI = 0.02f;                     ///< Threshold to detect stalled progress
  int maximumNoChangeBursts = 3;                          ///< Max bursts without progress before error
  float approachMarginPSI = 0.2f;                         ///< Safety margin before final approach
  float pressureNoiseThresholdPSI = 0.01f;                ///< Noise threshold for rate calculation
  float minimumRateThreshold = 0.001f;                    ///< Minimum rate to consider valid (PSI/sec)
  float minimumCheckIntervalSeconds = 0.02f;              ///< Minimum time window for rate calc
};

/**
 * @brief Intelligent pressure controller with adaptive learning
 * 
 * Manages automatic pressure seeking with:
 * - Adaptive burst duration based on learned inflation/deflation rates
 * - Safety timeouts and error detection
 * - Manual override capability
 * - Hardware abstraction for testability
 */
class PressureController {
public:
  PressureController() = default;
  
  /// @brief Initialize with hardware actuators (backward compatible)
  /// @param actuators Pointer to hardware actuator interface
  /// @param config Controller configuration parameters
  void begin(ta::act::Actuators* actuators, const ControllerConfig& config);

  /// @brief Initialize with abstract outputs (for testing)
  /// @param outputs Abstract actuator interface
  /// @param config Controller configuration parameters
  void begin(IActuatorOutputs* outputs, const ControllerConfig& config);

  /// @brief Update controller state (call every loop)
  /// @param currentTimeMilliseconds Current time in milliseconds
  /// @param currentPressurePSI Current measured pressure
  void update(uint32_t currentTimeMilliseconds, float currentPressurePSI);

  // Commands
  
  /// @brief Start automatic seeking to target pressure
  /// @param targetPSI Desired pressure (will be clamped to min/max)
  void startSeek(float targetPSI);
  
  /// @brief Manual air up control (direct compressor activation)
  /// @param active true to activate, false to deactivate
  void manualAirUp(bool active);
  
  /// @brief Manual vent control (direct valve activation)
  /// @param active true to activate, false to deactivate
  void manualVent(bool active);
  
  /// @brief Cancel current operation and return to idle
  void cancel();
  
  /// @brief Clear error state and return to idle
  void clearError();

  // Accessors
  
  /// @brief Get current controller state
  ControllerState getState() const { return _state; }
  
  /// @brief Get current error code
  ErrorCode getError() const { return _errorCode; }
  
  /// @brief Get current target pressure
  float getTargetPSI() const { return _targetPSI; }
  
  /// @brief Get last measured pressure
  float getCurrentPSI() const { return _currentPSI; }

  /// @brief Get protocol status character
  /// @return Single character representing state ('I', 'U', 'V', 'C', 'E')
  char getStatusCharacter() const;
  
  /// @brief Get error code as byte (for protocol transmission)
  uint8_t getErrorByte() const { return static_cast<uint8_t>(_errorCode); }

private:
  // State handlers
  void handleRunPhase(ControllerState runState, uint32_t now);
  void handleCheckingPhase(uint32_t now);
  void handleIdleState(uint32_t now);
  
  void enterState(ControllerState state, uint32_t now);
  void stopAllOutputs();
  void scheduleBurst(ControllerState direction, uint32_t durationMilliseconds, uint32_t now);
  void enterErrorState(ErrorCode errorCode, const char* reason);
  void reset();

  // Hardware abstraction
  
  /// @brief Adapter to wrap board Actuators as IActuatorOutputs
  struct ActuatorAdapter : IActuatorOutputs {
    ta::act::Actuators* hardware = nullptr;
    void setCompressor(bool enable) override;
    void setVent(bool open) override;
    void stopAll() override;
  } _actuatorAdapter;

  IActuatorOutputs* _outputs = nullptr;
  ControllerConfig _config{};

  // State tracking
  ControllerState _state = ControllerState::Idle;
  ControllerState _previousState = ControllerState::Idle;

  // Runtime state
  float _targetPSI = 0.0f;
  float _currentPSI = 0.0f;
  bool _isManualActive = false;
  uint32_t _lastManualRefreshTime = 0;

  // Phase tracking
  bool _isContinuousPhase = false;
  uint32_t _phaseStartTime = 0;
  uint32_t _phaseEndTime = 0;
  uint32_t _lastBurstEndTime = 0;
  float _phaseStartPressure = 0.0f;

  // Learned rates (adaptive control)
  float _inflationRatePSIPerSecond = 0.0f;
  float _deflationRatePSIPerSecond = 0.0f;
  int _inflationSampleCount = 0;
  int _deflationSampleCount = 0;

  // Error detection
  ErrorCode _errorCode = ErrorCode::None;
  int _noChangeDetectionCount = 0;
};

} // namespace controller
} // namespace trailair