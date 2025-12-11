#pragma once
#include <stdint.h>

namespace trailair {
namespace ui {

/**
 * @brief View states for the TrailAir user interface
 * 
 * Represents the current screen/mode being displayed to the user.
 */
enum class ViewState {
  Idle,          ///< Default view showing current and target pressure
  Manual,        ///< Manual control mode (direct air up/vent buttons)
  Seeking,       ///< Automatic seeking to target pressure
  Error,         ///< Error display mode
  Disconnected,  ///< Remote disconnected from control board
  Pairing        ///< Pairing mode for remote/board connection
};

/**
 * @brief Configuration for the user interface behavior
 */
struct UserInterfaceConfig {
  float minimumPSI = 0.0f;                           ///< Minimum allowed pressure in PSI
  float maximumPSI = 50.0f;                          ///< Maximum allowed pressure in PSI
  float defaultTargetPSI = 32.0f;                    ///< Default target pressure on startup
  float stepSize = 1.0f;                             ///< PSI increment/decrement per button click
  uint32_t doneHoldDurationMilliseconds = 1500;      ///< Duration to show "Done" after seeking completes
  uint32_t errorAutoClearDurationMilliseconds = 4000; ///< Optional auto-clear window for errors (0 = disabled)
};

/**
 * @brief Button identifiers for the TrailAir interface
 */
enum class ButtonId {
  Left,   ///< Left button (typically Cancel/Manual)
  Down,   ///< Down button (decrease target / manual vent)
  Up,     ///< Up button (increase target / manual air up)
  Right   ///< Right button (typically OK/Seek)
};

/**
 * @brief Button action types
 */
enum class ButtonAction {
  Pressed,   ///< Button physically pressed down
  Released,  ///< Button physically released
  Click,     ///< Complete click (press + release)
  LongHold   ///< Button held for extended duration
};

/**
 * @brief Button event containing button ID and action
 */
struct ButtonEvent {
  ButtonId id;
  ButtonAction action;
};

/**
 * @brief Abstract interface for device-specific actions
 * 
 * Implemented differently by control board and remote to perform
 * hardware-specific operations like starting compressor, opening valves, etc.
 */
struct DeviceActions {
  virtual ~DeviceActions() = default;
  
  /// @brief Cancel current manual or seek operation
  virtual void cancel() = 0;
  
  /// @brief Clear/acknowledge error state
  virtual void clearError() = 0;
  
  /// @brief Start automatic seeking to target pressure
  /// @param targetPSI Target pressure in PSI
  virtual void startSeek(float targetPSI) = 0;
  
  /// @brief Enable/disable manual venting
  /// @param enable true to start venting, false to stop
  virtual void manualVent(bool enable) = 0;
  
  /// @brief Enable/disable manual air up (inflation)
  /// @param enable true to start inflating, false to stop
  virtual void manualAirUp(bool enable) = 0;
  
  /// @brief Check if device is connected (remote-specific)
  /// @return true if connected (always true for control board)
  virtual bool isConnected() const { return true; }
};

/**
 * @brief Controller activity states
 * 
 * Represents what the pressure controller is currently doing.
 * Mapped from concrete controller implementations.
 */
enum class ControllerState {
  Idle,     ///< No active pressure adjustment
  AirUp,    ///< Actively inflating
  Venting,  ///< Actively deflating
  Checking, ///< Checking/stabilizing pressure
  Error     ///< Controller in error state
};

/**
 * @brief Complete UI state model for rendering
 * 
 * Contains all data needed to render the display.
 * Populated by application and consumed by display layer.
 */
struct UserInterfaceModel {
  // Core pressure data
  float currentPSI = 0.0f;                         ///< Current measured pressure
  float targetPSI = 0.0f;                          ///< Desired target pressure
  ControllerState controllerState = ControllerState::Idle; ///< Current controller activity
  ViewState viewState = ViewState::Idle;           ///< Current UI view/screen
  
  // Status flags
  bool showDoneHold = false;                       ///< True to show "Done" animation
  uint8_t lastErrorCode = 0;                       ///< Last error code (0 = no error)
  
  // Remote-specific fields (ignored by control board)
  bool isConnected = true;                         ///< Connection status (remote only)
  int batteryPercent = 0;                          ///< Battery percentage (remote only)
  bool showReconnectHint = false;                  ///< Show reconnection instructions
  bool pairingActive = false;                      ///< Pairing in progress
  bool pairingFailed = false;                      ///< Pairing attempt failed
  bool pairingBusy = false;                        ///< Pairing busy/processing
};

/**
 * @brief User interface state machine
 * 
 * Manages UI state transitions, button handling, and view logic.
 * Device-independent; uses DeviceActions strategy pattern for hardware operations.
 */
class UserInterfaceStateMachine {
public:
  UserInterfaceStateMachine() = default;
  explicit UserInterfaceStateMachine(const UserInterfaceConfig& config)
    : _config(config), _targetPSI(config.defaultTargetPSI) {}
  
  /// @brief Initialize with configuration
  void begin(const UserInterfaceConfig& config) {
    _config = config;
    _targetPSI = _config.defaultTargetPSI;
    clampTargetPressure();
  }

  /// @brief Update state machine (call each loop)
  /// @param now Current time in milliseconds
  /// @param deviceActions Device-specific action handler
  /// @param controllerState Current controller state
  void update(uint32_t now, DeviceActions& deviceActions, ControllerState controllerState);
  
  /// @brief Handle button events
  /// @param event Button event (ID + action)
  /// @param deviceActions Device-specific action handler
  void onButton(const ButtonEvent& event, DeviceActions& deviceActions);

  /// @brief Set target pressure
  void setTargetPSI(float psi) { _targetPSI = psi; clampTargetPressure(); }
  
  /// @brief Get current target pressure
  float getTargetPSI() const { return _targetPSI; }
  
  /// @brief Get current view state
  ViewState getViewState() const { return _viewState; }
  
  /// @brief Get minimum allowed pressure
  float getMinimumPSI() const { return _config.minimumPSI; }
  
  /// @brief Get maximum allowed pressure
  float getMaximumPSI() const { return _config.maximumPSI; }

  /// @brief Check if "Done" animation should be shown
  /// @param now Current time in milliseconds
  /// @return true if done animation is active
  bool isDoneHoldActive(uint32_t now) const {
    return _showDoneHold && now < _doneHoldEndTime;
  }

private:
  /// @brief Clamp target pressure to configured min/max range
  void clampTargetPressure() {
    if (_targetPSI < _config.minimumPSI) _targetPSI = _config.minimumPSI;
    if (_targetPSI > _config.maximumPSI) _targetPSI = _config.maximumPSI;
  }
  
  UserInterfaceConfig _config{};
  ViewState _viewState = ViewState::Idle;
  float _targetPSI = 32.0f;

  // Seeking/done tracking
  bool _hasSeenSeekingActivity = false;
  bool _showDoneHold = false;
  uint32_t _doneHoldEndTime = 0;

  // Error tracking
  uint32_t _errorEntryTime = 0;

  // Manual control flags
  bool _isManualVentActive = false;
  bool _isManualAirActive = false;
};

} // namespace ui
} // namespace trailair
