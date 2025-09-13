#include "TA_Battery.h"
#include <math.h>

// Pull in adc_attenuation_t for analogSetPinAttenuation cast
#include "driver/adc.h"

namespace ta {
    namespace battery {

        TA_BatteryMonitor::TA_BatteryMonitor(const Config& cfg) : cfg_(cfg) {
            clampConfig_();
        }

        bool TA_BatteryMonitor::begin(uint8_t pin, int attenEnum) {
            pin_ = pin;
            attenEnum_ = attenEnum;
            reset();

            // Configure ADC attenuation (Arduino-ESP32 core)
            analogSetPinAttenuation(pin_, (adc_attenuation_t)attenEnum_);
            return true;
        }

        void TA_BatteryMonitor::reset() {
            memset(buf_, 0, sizeof(buf_));
            idx_ = 0;
            count_ = 0;
            sum_ = 0;
            filteredMv_ = 0;
            percent_ = 0;
            hasFix_ = false;
        }

        bool TA_BatteryMonitor::update() {
            // Read mV at pin (ADC), then convert to battery-side mV using divider ratio
            uint32_t mvPin = analogReadMilliVolts(pin_);
            int mvBatt = (int)lroundf((float)mvPin * cfg_.dividerRatio);

            // Update rolling average
            pushSample_(mvBatt);
            int avg = avgMv_();

            bool changed = false;
            if (!hasFix_) {
                filteredMv_ = avg;
                hasFix_ = true;
                changed = true;
            } else if (abs(avg - filteredMv_) >= cfg_.deadbandMv) {
                filteredMv_ = avg;
                changed = true;
            }

            // Always recompute percent from filtered value (clamped inside)
            recomputePercent_();
            return changed;
        }

        void TA_BatteryMonitor::pushSample_(int mvBatt) {
            if (count_ < cfg_.sampleCount) {
                buf_[idx_] = mvBatt;
                sum_ += mvBatt;
                idx_ = (idx_ + 1) % cfg_.sampleCount;
                count_++;
            } else {
                sum_ -= buf_[idx_];
                buf_[idx_] = mvBatt;
                sum_ += mvBatt;
                idx_ = (idx_ + 1) % cfg_.sampleCount;
            }
        }

        int TA_BatteryMonitor::avgMv_() const {
            if (count_ == 0) return 0;
            return (int)(sum_ / (long)count_);
        }

        void TA_BatteryMonitor::recomputePercent_() {
            float v = voltage();
            float denom = (cfg_.vFull - cfg_.vEmpty);
            if (denom <= 0.01f) denom = 0.01f;

            float pct = ((v - cfg_.vEmpty) / denom) * 100.0f;
            if (pct < 0.0f) pct = 0.0f;
            if (pct > 100.0f) pct = 100.0f;

            percent_ = (int)lroundf(pct);
        }

        void TA_BatteryMonitor::clampConfig_() {
            if (cfg_.sampleCount == 0) cfg_.sampleCount = 1;
            if (cfg_.sampleCount > MAX_SAMPLES) cfg_.sampleCount = MAX_SAMPLES;
            if (cfg_.dividerRatio < 1.0f) cfg_.dividerRatio = 1.0f;
            if (cfg_.deadbandMv < 0) cfg_.deadbandMv = 0;
            if (cfg_.vFull <= cfg_.vEmpty) cfg_.vFull = cfg_.vEmpty + 0.1f;
            if (cfg_.lowPercent < 0) cfg_.lowPercent = 0;
            if (cfg_.lowPercent > 100) cfg_.lowPercent = 100;
        }

    } // namespace battery
} // namespace ta