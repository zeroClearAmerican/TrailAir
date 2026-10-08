#pragma once
#include <Arduino.h>

namespace trailair {
namespace battery {

/**
 * @brief Battery monitor configuration
 * 
 * Hardware and calibration settings for voltage divider-based
 * battery monitoring with rolling average filtering.
 */
struct Config {
  // Hardware config
  float dividerRatio = 2.0f;   ///< Voltage divider ratio (e.g., 2:1 => 2.0)
  uint8_t sampleCount = 10;    ///< Rolling average sample count
  int deadbandMv = 50;         ///< Change threshold in mV (battery side)

  // Percent mapping (LiPo 1S typical)
  float vEmpty = 3.30f;        ///< Voltage at 0% (battery protection threshold)
  float vFull  = 4.00f;        ///< Voltage at 100%
  int lowPercent = 15;         ///< Low battery threshold percentage
};

/**
 * @brief Battery voltage and charge monitoring
 * 
 * Monitors battery voltage through ADC with configurable voltage divider,
 * applies rolling average filtering, and estimates charge percentage.
 */
class BatteryMonitor {
public:
  explicit BatteryMonitor(const Config& cfg = Config{});

  /// @brief Initialize ADC pin and attenuation
  /// @param pin ADC pin number
  /// @param attenEnum ADC attenuation (ADC_11db, ADC_6db, etc.)
  /// @return true if initialization succeeded
  bool begin(uint8_t pin, int attenEnum);

  /// @brief Take one sample and update filters
  /// @return true if filtered value changed beyond deadband or first reading
  bool update();

  // Accessors
  
  /// @brief Get filtered battery voltage in millivolts
  int   millivolts()   const { return _filteredMillivolts; }
  
  /// @brief Get filtered battery voltage in volts
  float voltage()      const { return _filteredMillivolts / 1000.0f; }
  
  /// @brief Get estimated battery charge percentage (0-100)
  int   percent()      const { return _estimatedPercentage; }
  
  /// @brief Check if battery is below low threshold
  bool  isLow()        const { return _estimatedPercentage <= _batteryConfig.lowPercent; }
  
  /// @brief Check if battery is at or below empty threshold
  bool  isCritical()   const { return voltage() <= _batteryConfig.vEmpty; }
  
  /// @brief Check if first valid reading has been obtained
  bool  hasFix()       const { return _hasFixOnRead; }

  // Maintenance
  
  /// @brief Clear buffers and reset state
  void  reset();

  /// @brief Update configuration at runtime (clears buffers)
  void  setConfig(const Config& cfg) { _batteryConfig = cfg; clampConfig_(); reset(); }
  
  /// @brief Get current configuration
  const Config& config() const { return _batteryConfig; }

private:
  void clampConfig_();
  void pushSample_(int mvBatt);
  int  avgMv_() const;
  void recomputePercent_();

private:
  static constexpr uint8_t MAX_SAMPLES = 32;

  Config _batteryConfig;
  uint8_t _analogReadPin = 0;
  int attenEnum_ = 0;

  int   buf_[MAX_SAMPLES] = {0};
  uint8_t idx_ = 0;
  uint8_t count_ = 0;
  long  sum_ = 0;

  int _filteredMillivolts = 0;   ///< Battery-side mV after deadbanded average
  int _estimatedPercentage    = 0;   ///< Estimated charge percentage
  bool _hasFixOnRead    = false; ///< First valid reading obtained
};

} // namespace battery
} // namespace trailair