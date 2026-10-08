#pragma once
#include <Arduino.h>
#include <TA_Controller.h>

namespace trailair {
namespace actuators {

struct Pins {
  int compressorPin;
  int ventPin;
};

// Compressor and vent are mutually exclusive, and each stays off at least MIN_OFF_MS
// before turning back on (protects relay/compressor from rapid toggling). A turn-on that
// comes too soon is deferred, not dropped: service() applies it once allowed.
class Actuators : public trailair::controller::IActuatorOutputs {
public:
  static constexpr uint32_t MIN_OFF_MS = 500;

  void begin(const Pins& p) {
    pins_ = p;
    // Ensure pins are LOW before setting to OUTPUT to prevent glitches
    digitalWrite(pins_.compressorPin, LOW);
    digitalWrite(pins_.ventPin, LOW);
    pinMode(pins_.compressorPin, OUTPUT);
    pinMode(pins_.ventPin, OUTPUT);
  }

  void setCompressor(bool on) override {
    wantCompressor_ = on;
    if (on) wantVent_ = false;
    service();
  }

  void setVent(bool open) override {
    wantVent_ = open;
    if (open) wantCompressor_ = false;
    service();
  }

  void stopAll() override {
    wantCompressor_ = wantVent_ = false;
    service();
  }

  // Call every loop to complete deferred turn-ons
  void service() {
    uint32_t now = millis();
    // Offs first, so both outputs are never on together
    if (!wantCompressor_) drive_(pins_.compressorPin, compressorOn_, compressorOffAt_, false, now);
    if (!wantVent_)       drive_(pins_.ventPin,       ventOn_,       ventOffAt_,       false, now);
    if (wantCompressor_)  drive_(pins_.compressorPin, compressorOn_, compressorOffAt_, true,  now);
    if (wantVent_)        drive_(pins_.ventPin,       ventOn_,       ventOffAt_,       true,  now);
  }

private:
  static void drive_(int pin, bool& isOn, uint32_t& offAt, bool want, uint32_t now) {
    if (!want && isOn) {
      digitalWrite(pin, LOW);
      isOn = false;
      offAt = now;
    } else if (want && !isOn && now - offAt >= MIN_OFF_MS) {
      digitalWrite(pin, HIGH);
      isOn = true;
    }
  }

  Pins pins_{};
  bool wantCompressor_ = false;
  bool wantVent_ = false;
  bool compressorOn_ = false;
  bool ventOn_ = false;
  uint32_t compressorOffAt_ = 0;
  uint32_t ventOffAt_ = 0;
};

} // namespace actuators
} // namespace trailair
