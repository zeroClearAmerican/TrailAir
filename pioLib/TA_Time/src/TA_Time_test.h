/**
 * Test utilities for mocking time in TA_Time tests
 */
#pragma once
#include <stdint.h>

#define TA_TIME_TEST_MODE
#include "TA_Time.h"

namespace ta {
namespace time {
namespace test {

/**
 * @brief Mock time source for deterministic testing
 * 
 * Usage:
 *   MockTime mockTime;
 *   mockTime.set(1000);  // Set current time to 1000ms
 *   mockTime.advance(500); // Advance by 500ms
 *   // Now getMillis() returns 1500
 */
class MockTime {
public:
    MockTime() {
        _testMillis = &MockTime::getCurrentTime;
        currentTime_ = 0;
        instance_ = this;
    }
    
    ~MockTime() {
        _testMillis = nullptr;
        instance_ = nullptr;
    }
    
    void set(uint32_t time) {
        currentTime_ = time;
    }
    
    void advance(uint32_t deltaMs) {
        currentTime_ += deltaMs;
    }
    
    uint32_t get() const {
        return currentTime_;
    }
    
private:
    static uint32_t getCurrentTime() {
        return instance_ ? instance_->currentTime_ : 0;
    }
    
    uint32_t currentTime_;
    static MockTime* instance_;
};

} // namespace test
} // namespace time
} // namespace ta
