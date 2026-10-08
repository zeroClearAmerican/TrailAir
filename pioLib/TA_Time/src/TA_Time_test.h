/**
 * @file TA_Time_test.h
 * @brief Test utilities for mocking time in TA_Time tests
 */
#pragma once
#include <stdint.h>

#define TA_TIME_TEST_MODE
#include "TA_Time.h"

namespace trailair {
namespace time {
namespace test {

/**
 * @brief Mock time source for deterministic testing
 * 
 * Provides a controllable time source for unit testing time-dependent code.
 * Automatically hooks into TA_Time's test mode when instantiated.
 * 
 * @example
 * ```cpp
 * MockTime mockTime;
 * mockTime.set(1000);      // Set current time to 1000ms
 * mockTime.advance(500);   // Advance by 500ms
 * // Now getMilliseconds() returns 1500
 * ```
 */
class MockTime {
public:
    /**
     * @brief Constructs and activates the mock time source
     */
    MockTime() {
        _testMillisFunction = &MockTime::getCurrentTime;
        _currentTime = 0;
        _instance = this;
    }
    
    /**
     * @brief Destructs and deactivates the mock time source
     */
    ~MockTime() {
        _testMillisFunction = nullptr;
        _instance = nullptr;
    }
    
    /**
     * @brief Set the current time to a specific value
     * @param timeMilliseconds Time value in milliseconds
     */
    void set(uint32_t timeMilliseconds) {
        _currentTime = timeMilliseconds;
    }
    
    /**
     * @brief Advance the current time by a delta
     * @param deltaMilliseconds Amount to advance in milliseconds
     */
    void advance(uint32_t deltaMilliseconds) {
        _currentTime += deltaMilliseconds;
    }
    
    /**
     * @brief Get the current mock time value
     * @return Current time in milliseconds
     */
    uint32_t get() const {
        return _currentTime;
    }
    
private:
    /**
     * @brief Static getter for the current time (used by TA_Time.h)
     * @return Current time from active MockTime instance
     */
    static uint32_t getCurrentTime() {
        return _instance ? _instance->_currentTime : 0;
    }
    
    uint32_t _currentTime;
    static MockTime* _instance;
};

} // namespace test
} // namespace time
} // namespace trailair
