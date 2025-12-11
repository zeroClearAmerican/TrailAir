#pragma once
#include <stdint.h>

// Conditional include: Arduino.h for production, test override for mocking
#ifndef TA_TIME_TEST_MODE
  #if defined(ARDUINO)
    #include <Arduino.h>
  #endif
#endif

/**
 * @file TA_Time.h
 * @brief Overflow-safe time utilities for TrailAir embedded systems
 * 
 * Provides overflow-safe time comparison and calculation functions that correctly
 * handle the 32-bit millis() counter overflow (~49.7 days). All functions use
 * unsigned arithmetic wraparound which is well-defined in C/C++.
 * 
 * Features:
 * - Overflow-safe elapsed time checks
 * - Safe future time calculations  
 * - Mockable time source for unit testing
 */

namespace trailair {
namespace time {

// ============================================================================
// Time Source Abstraction (mockable in tests)
// ============================================================================

#ifdef TA_TIME_TEST_MODE
  /// @brief Test mode function pointer for mocking millis()
  extern uint32_t (*_testMillisFunction)();
  
  /**
   * @brief Get current time in milliseconds (test mode - mockable)
   * @return Current time from mock function, or 0 if not set
   */
  inline uint32_t getMilliseconds() {
    return _testMillisFunction ? _testMillisFunction() : 0;
  }
#else
  /**
   * @brief Get current time in milliseconds (production mode)
   * @return Current time from Arduino millis()
   */
  inline uint32_t getMilliseconds() {
    return millis();
  }
#endif

// ============================================================================
// Overflow-Safe Time Operations
// ============================================================================

/**
 * @brief Check if a timeout duration has elapsed (overflow-safe)
 * 
 * This function correctly handles millis() overflow which occurs every ~49.7 days.
 * Uses unsigned arithmetic wraparound which is well-defined in C/C++.
 * 
 * @param currentTime Current time in milliseconds (typically from millis())
 * @param startTime Start time in milliseconds
 * @param timeoutMilliseconds Timeout duration in milliseconds
 * @return true if the timeout duration has elapsed, false otherwise
 * 
 * @example
 * ```cpp
 * uint32_t startTime = millis();
 * // ... later ...
 * if (hasElapsed(millis(), startTime, 5000)) {
 *   // 5 seconds have passed
 * }
 * ```
 */
inline bool hasElapsed(uint32_t currentTime, uint32_t startTime, uint32_t timeoutMilliseconds) {
    return (currentTime - startTime) >= timeoutMilliseconds;
}

/**
 * @brief Check if current time has reached or passed a target time (overflow-safe)
 * 
 * Use this for absolute time comparisons when checking scheduled events.
 * Works correctly even when times wrap around the 32-bit boundary.
 * 
 * @param currentTime Current time in milliseconds
 * @param targetTime Target time in milliseconds  
 * @return true if currentTime >= targetTime (accounting for overflow)
 * 
 * @example
 * ```cpp
 * uint32_t nextEventTime = millis() + 10000; // 10 seconds from now
 * // ... later ...
 * if (isTimeFor(millis(), nextEventTime)) {
 *   // Time for the event
 * }
 * ```
 */
inline bool isTimeFor(uint32_t currentTime, uint32_t targetTime) {
    // This comparison works because of modular arithmetic:
    // (currentTime - targetTime) is treated as a signed difference
    // If result is >= 0, we've reached or passed the target
    return (int32_t)(currentTime - targetTime) >= 0;
}

/**
 * @brief Calculate a future time by adding a delay to current time (overflow-safe)
 * 
 * Safely adds a delay to the current time. The result may wrap around the
 * 32-bit boundary, which is intentional and handled correctly by hasElapsed()
 * and isTimeFor().
 * 
 * @param currentTime Current time in milliseconds
 * @param delayMilliseconds Delay to add in milliseconds
 * @return Future time (may be numerically less than currentTime due to wraparound)
 * 
 * @example
 * ```cpp
 * uint32_t eventTime = calculateFutureTime(millis(), 30000); // 30 seconds from now
 * // ... later ...
 * if (isTimeFor(millis(), eventTime)) {
 *   // Event time reached
 * }
 * ```
 */
inline uint32_t calculateFutureTime(uint32_t currentTime, uint32_t delayMilliseconds) {
    return currentTime + delayMilliseconds; // Wraparound is intentional and safe
}

} // namespace time
} // namespace trailair
