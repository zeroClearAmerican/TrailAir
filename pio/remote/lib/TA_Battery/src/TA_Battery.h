#pragma once
#include <Arduino.h>

namespace ta {
    namespace battery {

        struct Config {
            // Hardware config
            float dividerRatio = 2.0f;   // e.g., 2:1 divider => 2.0
            uint8_t sampleCount = 10;    // rolling average N
            int deadbandMv = 50;         // change threshold in mV (battery side)

            // Percent mapping (LiPo 1S typical)
            float vEmpty = 3.30f;        // volts at 0% (battery protection threshold)
            float vFull  = 4.14f;        // volts at 100%
            int lowPercent = 15;         // low-batt threshold
        };

        class TA_BatteryMonitor {
            public:
                explicit TA_BatteryMonitor(const Config& cfg = Config{});

                // attenEnum: pass ADC_11db, ADC_6db, etc. (from core). Returns true on init.
                bool begin(uint8_t pin, int attenEnum);

                // Take one sample, update filters. Returns true if filtered mV changed (deadband passed) or first fix.
                bool update();

                // Accessors
                int   millivolts()   const { return filteredMv_; } // battery-side mV (after divider correction)
                float voltage()      const { return filteredMv_ / 1000.0f; }
                int   percent()      const { return percent_; }
                bool  isLow()        const { return percent_ <= cfg_.lowPercent; }
                bool  isCritical()   const { return voltage() <= cfg_.vEmpty; } // at or below protection threshold
                bool  hasFix()       const { return hasFix_; }

                // Maintenance
                void  reset(); // clears buffers and state

                // Optional: tweak at runtime (careful while running)
                void  setConfig(const Config& cfg) { cfg_ = cfg; clampConfig_(); reset(); }
                const Config& config() const { return cfg_; }

            private:
                void clampConfig_();
                void pushSample_(int mvBatt);
                int  avgMv_() const;
                void recomputePercent_();

            private:
                static constexpr uint8_t MAX_SAMPLES = 32;

                Config cfg_;
                uint8_t pin_ = 0;
                int attenEnum_ = 0;

                int   buf_[MAX_SAMPLES] = {0};
                uint8_t idx_ = 0;
                uint8_t count_ = 0;
                long  sum_ = 0;

                int filteredMv_ = 0;   // battery-side mV after deadbanded average
                int percent_    = 0;
                bool hasFix_    = false;
        };
        
    } // namespace battery
} // namespace ta