#pragma once
#include <stdint.h>

namespace trailair {
namespace errors {

/**
 * @brief Error codes for the TrailAir system
 * 
 * Shared error catalog used by both control board and remote.
 * Maps error conditions to numeric codes for transmission and display.
 * 
 * Code 0 is reserved for no error, code 255 for unknown errors.
 */
enum class ErrorCode : uint8_t {
  /// No error - normal operation
  None = 0,
  
  /// Pressure did not change during control operation
  NoChange = 1,
  
  /// Control operation took too long to reach target
  ExcessiveTime = 2,
  
  /// Pressure sensor malfunction or reading error
  Sensor = 3,
  
  /// Pressure exceeded maximum safe threshold
  OverPressure = 4,
  
  /// Pressure fell below minimum threshold
  UnderPressure = 5,
  
  /// Controller received conflicting commands
  Conflict = 6,
  
  /// Unknown or unspecified error
  Unknown = 255
};

/**
 * @brief Get a short human-readable description of an error code
 * 
 * @param code The error code to describe
 * @return Short text description suitable for display (e.g., "Sensor", "Over PSI")
 */
inline const char* getShortDescription(ErrorCode code) {
  switch (code) {
    case ErrorCode::None:          return "None";
    case ErrorCode::NoChange:      return "No change";
    case ErrorCode::ExcessiveTime: return "Too slow";
    case ErrorCode::Sensor:        return "Sensor";
    case ErrorCode::OverPressure:  return "Over PSI";
    case ErrorCode::UnderPressure: return "Under PSI";
    case ErrorCode::Conflict:      return "Conflict";
    case ErrorCode::Unknown:       return "Unknown";
    default:                       return "Error";
  }
}

/**
 * @brief Get a short human-readable description of an error code (uint8_t overload)
 * 
 * @param code The error code as uint8_t
 * @return Short text description suitable for display
 */
inline const char* getShortDescription(uint8_t code) {
  return getShortDescription(static_cast<ErrorCode>(code));
}

}  // namespace errors
}  // namespace trailair
