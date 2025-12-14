#include "TA_Battery.h"
#include <math.h>

// Pull in adc_attenuation_t for analogSetPinAttenuation cast
#include "driver/adc.h"

namespace trailair {
    namespace battery {

        BatteryMonitor::BatteryMonitor(const Config& cfg) : _batteryConfig(cfg) {
            clampConfig_();
        }

        bool BatteryMonitor::begin(uint8_t pin, int attenEnum) {
            _analogReadPin = pin;
            attenEnum_ = attenEnum;
            reset();

            // Configure ADC attenuation (Arduino-ESP32 core)
            analogSetPinAttenuation(_analogReadPin, (adc_attenuation_t)attenEnum_);
            return true;
        }

        void BatteryMonitor::reset() {
            memset(buf_, 0, sizeof(buf_));
            idx_ = 0;
            count_ = 0;
            sum_ = 0;
            _filteredMillivolts = 0;
            _estimatedPercentage = 0;
            _hasFixOnRead = false;
        }

        bool BatteryMonitor::update() {
            // Read mV at pin (ADC), then convert to battery-side mV using divider ratio
            uint32_t mvPin = analogReadMilliVolts(_analogReadPin);
            int mvBatt = (int)lroundf((float)mvPin * _batteryConfig.dividerRatio);

            // Update rolling average
            pushSample_(mvBatt);
            int avg = avgMv_();

            bool changed = false;
            if (!_hasFixOnRead) {
                _filteredMillivolts = avg;
                _hasFixOnRead = true;
                changed = true;
            } else if (abs(avg - _filteredMillivolts) >= _batteryConfig.deadbandMv) {
                _filteredMillivolts = avg;
                changed = true;
            }

            // Always recompute percent from filtered value (clamped inside)
            recomputePercent_();
            return changed;
        }

        void BatteryMonitor::pushSample_(int mvBatt) {
            if (count_ < _batteryConfig.sampleCount) {
                buf_[idx_] = mvBatt;
                sum_ += mvBatt;
                idx_ = (idx_ + 1) % _batteryConfig.sampleCount;
                count_++;
            } else {
                sum_ -= buf_[idx_];
                buf_[idx_] = mvBatt;
                sum_ += mvBatt;
                idx_ = (idx_ + 1) % _batteryConfig.sampleCount;
            }
        }

        int BatteryMonitor::avgMv_() const {
            if (count_ == 0) return 0;
            return (int)(sum_ / (long)count_);
        }

        void BatteryMonitor::recomputePercent_() {
            float v = voltage();
            float denom = (_batteryConfig.vFull - _batteryConfig.vEmpty);
            if (denom <= 0.01f) denom = 0.01f;

            float pct = ((v - _batteryConfig.vEmpty) / denom) * 100.0f;
            if (pct < 0.0f) pct = 0.0f;
            if (pct > 100.0f) pct = 100.0f;

            _estimatedPercentage = (int)lroundf(pct);
        }

        void BatteryMonitor::clampConfig_() {
            if (_batteryConfig.sampleCount == 0) _batteryConfig.sampleCount = 1;
            if (_batteryConfig.sampleCount > MAX_SAMPLES) _batteryConfig.sampleCount = MAX_SAMPLES;
            if (_batteryConfig.dividerRatio < 1.0f) _batteryConfig.dividerRatio = 1.0f;
            if (_batteryConfig.deadbandMv < 0) _batteryConfig.deadbandMv = 0;
            if (_batteryConfig.vFull <= _batteryConfig.vEmpty) _batteryConfig.vFull = _batteryConfig.vEmpty + 0.1f;
            if (_batteryConfig.lowPercent < 0) _batteryConfig.lowPercent = 0;
            if (_batteryConfig.lowPercent > 100) _batteryConfig.lowPercent = 100;
        }

    } // namespace battery
} // namespace ta