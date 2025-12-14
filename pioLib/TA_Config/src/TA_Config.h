#pragma once
#include <stdint.h>

namespace trailair {
namespace config {

/**
 * @brief User interface configuration shared by both control board and remote
 * 
 * Defines pressure limits, step sizes, and timing constants for the UI layer.
 */
struct UserInterfaceConfiguration {
  /// Minimum allowable pressure in PSI
  float minimumPressurePSI = 0.0f;
  
  /// Maximum allowable pressure in PSI
  float maximumPressurePSI = 50.0f;
  
  /// Default target pressure in PSI when system starts
  float defaultTargetPressurePSI = 32.0f;
  
  /// Small pressure adjustment step in PSI (per button click)
  float pressureStepSmallPSI = 1.0f;
  
  /// Duration to display "Done" message after seeking completes
  uint32_t doneHoldDurationMilliseconds = 1500;
  
  /// Auto-clear error screen after this duration (0 = disabled)
  uint32_t errorAutoClearDurationMilliseconds = 4000;
};

/**
 * @brief Communication link configuration shared by both devices
 * 
 * Defines timeouts, retry intervals, and pairing parameters for the 
 * wireless communication link between remote and control board.
 */
struct CommunicationConfiguration {
  /// Control board: timeout to consider remote as inactive (should be > 2x ping interval)
  uint32_t remoteActiveTimeoutMilliseconds = 5000;
  
  /// Remote: timeout to consider connection lost
  uint32_t connectionTimeoutMilliseconds = 5000;
  
  /// Interval for resending manual control commands (remote manual streaming)
  uint32_t manualRepeatIntervalMilliseconds = 300;
  
  /// Initial backoff delay for ping/reconnect attempts
  uint32_t pingBackoffStartMilliseconds = 200;
  
  /// Maximum backoff delay for ping/reconnect attempts
  uint32_t pingBackoffMaximumMilliseconds = 2000;
  
  /// Default pairing group identifier (allows multiple systems in same area)
  uint8_t pairingGroupIdentifier = 0x01;
  
  /// Interval between pairing request broadcasts
  uint32_t pairingRequestIntervalMilliseconds = 500;
  
  /// Total timeout for pairing process before giving up
  uint32_t pairingTimeoutMilliseconds = 30000;
};

}  // namespace config
}  // namespace trailair
