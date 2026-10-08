#pragma once
#include <Arduino.h>

namespace trailair {
namespace sensors {

// Rolling-average pressure reading.
//
// Calibration (tune against a known-good gauge):
//   Sensor: 0.5-4.5 V spans 0-150 PSI.
//   Divider: sensor -> 3.3k -> pin -> 4.7k -> GND, so the pin sees 4.7/8.0 of the sensor voltage
//   (4.5 V -> 2.64 V; 50 PSI -> ~1.08 V, well inside the ESP32-C3's linear ADC range).
class PressureFilter {
public:
  static constexpr int MAX_SAMPLES = 32;
  static constexpr float DIVIDER_GAIN = (3.3f + 4.7f) / 4.7f;  // pin volts -> sensor volts
  static constexpr float SENSOR_ZERO_V = 0.5f;                 // sensor output at 0 PSI
  static constexpr float PSI_PER_SENSOR_V = 150.0f / 4.0f;
  static constexpr int OVERSAMPLE = 4;                         // ADC reads averaged per call (cheap noise cut)

  void begin(int analogPin, int samples, float noiseThreshPsi) {
    pin_ = analogPin;
    capacity_ = constrain(samples, 1, MAX_SAMPLES);
    noiseThresh_ = noiseThreshPsi;
    count_ = idx_ = 0;
  }

  float readPsi() {
    uint32_t mv = 0;
    for (int i = 0; i < OVERSAMPLE; ++i) mv += analogReadMilliVolts(pin_);
    float sensorVolts = (mv / (float)OVERSAMPLE) / 1000.0f * DIVIDER_GAIN;
    float psi = constrain((sensorVolts - SENSOR_ZERO_V) * PSI_PER_SENSOR_V, 0.0f, 150.0f);

    buffer_[idx_] = psi;
    idx_ = (idx_ + 1) % capacity_;
    if (count_ < capacity_) count_++;

    // ponytail: re-sum each read (<=32 floats); a running float sum would drift over days of uptime
    float sum = 0.0f;
    for (int i = 0; i < count_; ++i) sum += buffer_[i];
    float avg = sum / count_;
    return avg < noiseThresh_ ? 0.0f : avg;
  }

private:
  int pin_ = -1;
  int capacity_ = 1;
  int count_ = 0;
  int idx_ = 0;
  float noiseThresh_ = 0.5f;
  float buffer_[MAX_SAMPLES] = {};
};

} // namespace sensors
} // namespace trailair
