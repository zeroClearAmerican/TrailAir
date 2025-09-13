#pragma once
#include <Arduino.h>
#include <vector>

namespace ta {
namespace sensors {

class PressureFilter {
public:
  void begin(int analogPin, int samples, float noiseThreshPsi) {
    pin_ = analogPin;
    capacity_ = samples;
    noiseThresh_ = noiseThreshPsi;
    count_ = idx_ = 0;
  }

  float readPsi() {
    int mV = analogReadMilliVolts(pin_);
    float volts = mV / 1000.0f;
    float psi = (volts - 0.5f) * (150.0f / 4.0f);
    if (psi < 0) psi = 0;
    if (psi > 150) psi = 150;
    if (capacity_ == 0) return psi;
    if (buffer_.size() != (size_t)capacity_) buffer_.resize(capacity_, psi);
    buffer_[idx_] = psi;
    idx_ = (idx_ + 1) % capacity_;
    if (count_ < capacity_) count_++;
    float sum = 0;
    for (int i = 0; i < count_; ++i) sum += buffer_[i];
    float avg = sum / count_;
    if (avg < noiseThresh_) return 0.0f;
    return avg;
  }

private:
  int pin_ = -1;
  int capacity_ = 0;
  int count_ = 0;
  int idx_ = 0;
  float noiseThresh_ = 0.5f;
  std::vector<float> buffer_;
};

} // namespace sensors
} // namespace ta