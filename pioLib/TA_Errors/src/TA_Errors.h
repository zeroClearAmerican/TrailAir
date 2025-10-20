#pragma once
#include <stdint.h>

namespace ta { namespace errors {

// Shared error catalog (codes -> short human-readable text)
// Keep aligned with controller and any firmware error emitters.
// 0 reserved for NONE; 255 for UNKNOWN.

enum : uint8_t {
  NONE = 0,
  NO_CHANGE = 1,
  EXCESSIVE_TIME = 2,
  SENSOR = 3,
  OVER_PSI = 4,
  UNDER_PSI = 5,
  CONFLICT = 6,
  UNKNOWN = 255
};

inline const char* shortText(uint8_t code) {
  switch (code) {
    case NONE:           return "None";
    case NO_CHANGE:      return "No change";
    case EXCESSIVE_TIME: return "Too slow";
    case SENSOR:         return "Sensor";
    case OVER_PSI:       return "Over PSI";
    case UNDER_PSI:      return "Under PSI";
    case CONFLICT:       return "Conflict";
    case UNKNOWN:        return "Unknown";
    default:             return "Error";
  }
}

}} // namespace ta::errors
