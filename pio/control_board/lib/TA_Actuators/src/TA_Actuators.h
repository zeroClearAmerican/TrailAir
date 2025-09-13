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
    pinMode(pins_.compressorPin, OUTPUT);
    pinMode(pins_.ventPin, OUTPUT);
    stopAll();
  }

  void setCompressor(bool on) {
    if (on) digitalWrite(pins_.ventPin, LOW);
    digitalWrite(pins_.compressorPin, on ? HIGH : LOW);
  }

  void setVent(bool open) {
    if (open) digitalWrite(pins_.compressorPin, LOW);
    digitalWrite(pins_.ventPin, open ? HIGH : LOW);
  }

  void stopAll() {
    digitalWrite(pins_.compressorPin, LOW);
    digitalWrite(pins_.ventPin, LOW);
  }

private:
  Pins pins_{};
};

} // namespace act
} // namespace ta