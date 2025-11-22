#pragma once
#include <stdint.h>

// Conditional include: Arduino.h for production, test override for mocking
#ifndef TA_TIME_TEST_MODE
  #if defined(ARDUINO)
    #include <Arduino.h>
  #endif
#endif

namespace ta {
namespace time {

// ============================================================================
// Time Source Abstraction (mockable in tests)
// ============================================================================

#ifdef TA_TIME_TEST_MODE
  // Test mode: use mockable function pointer
  extern uint32_t (*_testMillis)();
  
  inline uint32_t getMillis() {
    return _testMillis ? _testMillis() : 0;
  }
#else
  // Production mode: call Arduino millis() directly
  inline uint32_t getMillis() {
    return millis();
  }
#endif

// ============================================================================
// Overflow-Safe Time Operations
// ============================================================================

/**
 * @brief Check if a timeout has elapsed using overflow-safe arithmetic
 * 
 * This function correctly handles millis() overflow which occurs every ~49.7 days.
 * It uses unsigned arithmetic wraparound which is well-defined in C/C++.
 * 
 * @param now Current time in milliseconds (from millis())
 * @param start Start time in milliseconds
 * @param timeoutMs Timeout duration in milliseconds
 * @return true if timeout has elapsed, false otherwise
 * 
 * @example
 *   uint32_t startTime = millis();
 *   // ... later ...
 *   if (hasElapsed(millis(), startTime, 5000)) {
 *     // 5 seconds have passed
 *   }
 */
inline bool hasElapsed(uint32_t now, uint32_t start, uint32_t timeoutMs) {
    return (now - start) >= timeoutMs;
}

/**
 * @brief Check if current time is at or past a target time (overflow-safe)
 * 
 * Use this for absolute time comparisons like "is it time yet?"
 * 
 * @param now Current time in milliseconds
 * @param target Target time in milliseconds  
 * @return true if now >= target (accounting for overflow)
 */
inline bool isTimeFor(uint32_t now, uint32_t target) {
    // Equivalent to: (now - target) < 0x80000000
    // This works because of unsigned wraparound
    return (int32_t)(now - target) >= 0;
}

/**
 * @brief Calculate a future time safely (overflow-safe)
 * 
 * @param now Current time in milliseconds
 * @param delayMs Delay in milliseconds
 * @return Future time (may be wrapped around)
 */
inline uint32_t futureTime(uint32_t now, uint32_t delayMs) {
    return now + delayMs; // Wraparound is intentional and safe
}

} // namespace time
} // namespace ta
