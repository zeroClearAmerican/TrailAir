#pragma once
#include <Arduino.h>

namespace ta {
namespace act {

struct Pins {
  int compressorPin;
  int ventPin;
};

class Actuators {
public:
  void begin(const Pins& p) {
    pins_ = p;
    // Ensure pins are LOW before setting to OUTPUT to prevent glitches
    digitalWrite(pins_.compressorPin, LOW);
    digitalWrite(pins_.ventPin, LOW);
    pinMode(pins_.compressorPin, OUTPUT);
    pinMode(pins_.ventPin, OUTPUT);
    // Redundant stopAll() for safety
    stopAll();
    
    // Initialize debounce state
    compressorState_ = false;
    ventState_ = false;
    lastCompressorOffTime_ = 0;
    lastVentOffTime_ = 0;
  }

  void setCompressor(bool on) {
    // Turning OFF: immediate response
    if (!on) {
      if (compressorState_) {
        digitalWrite(pins_.compressorPin, LOW);
        compressorState_ = false;
        lastCompressorOffTime_ = millis();
      }
      return;
    }
    
    // Turning ON: apply debounce (500ms since last OFF)
    uint32_t now = millis();
    if (now - lastCompressorOffTime_ < 500) {
      return;  // Ignore, too soon after turning off
    }
    
    // Safe to turn on
    if (!compressorState_) {
      digitalWrite(pins_.ventPin, LOW);  // Ensure vent is off
      digitalWrite(pins_.compressorPin, HIGH);
      compressorState_ = true;
    }
  }

  void setVent(bool open) {
    // Turning OFF: immediate response
    if (!open) {
      if (ventState_) {
        digitalWrite(pins_.ventPin, LOW);
        ventState_ = false;
        lastVentOffTime_ = millis();
      }
      return;
    }
    
    // Turning ON: apply debounce (500ms since last OFF)
    uint32_t now = millis();
    if (now - lastVentOffTime_ < 500) {
      return;  // Ignore, too soon after turning off
    }
    
    // Safe to turn on
    if (!ventState_) {
      digitalWrite(pins_.compressorPin, LOW);  // Ensure compressor is off
      digitalWrite(pins_.ventPin, HIGH);
      ventState_ = true;
    }
  }

  void stopAll() {
    if (compressorState_) {
      digitalWrite(pins_.compressorPin, LOW);
      compressorState_ = false;
      lastCompressorOffTime_ = millis();
    }
    if (ventState_) {
      digitalWrite(pins_.ventPin, LOW);
      ventState_ = false;
      lastVentOffTime_ = millis();
    }
  }

private:
  Pins pins_{};
  bool compressorState_ = false;
  bool ventState_ = false;
  uint32_t lastCompressorOffTime_ = 0;
  uint32_t lastVentOffTime_ = 0;
};

} // namespace act
} // namespace ta