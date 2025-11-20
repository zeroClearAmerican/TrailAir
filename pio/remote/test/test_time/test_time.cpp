#include <gtest/gtest.h>
#include "TA_Time.h"

using namespace ta::time;

// ============================================================================
// Overflow Safety Tests
// ============================================================================

TEST(TimeUtils, HasElapsed_NormalCase) {
    uint32_t start = 1000;
    uint32_t now = 6000;
    
    EXPECT_TRUE(hasElapsed(now, start, 5000));   // Exactly 5000ms elapsed
    EXPECT_TRUE(hasElapsed(now, start, 4999));   // More than 4999ms elapsed
    EXPECT_FALSE(hasElapsed(now, start, 5001));  // Less than 5001ms elapsed
}

TEST(TimeUtils, HasElapsed_ZeroTimeout) {
    uint32_t start = 1000;
    uint32_t now = 1000;
    
    EXPECT_TRUE(hasElapsed(now, start, 0));  // 0ms timeout always elapsed
}

TEST(TimeUtils, HasElapsed_ImmediateCheck) {
    uint32_t start = 1000;
    uint32_t now = 1000;
    
    EXPECT_FALSE(hasElapsed(now, start, 1));  // 0ms elapsed, need 1ms
    EXPECT_TRUE(hasElapsed(now, start, 0));   // 0ms elapsed, need 0ms
}

TEST(TimeUtils, HasElapsed_OverflowCase_JustBefore) {
    // millis() is about to overflow
    uint32_t start = 0xFFFFFFFE;  // 4,294,967,294 (2 before overflow)
    uint32_t now   = 0xFFFFFFFF;  // 4,294,967,295 (1 before overflow)
    
    EXPECT_TRUE(hasElapsed(now, start, 1));    // 1ms elapsed
    EXPECT_FALSE(hasElapsed(now, start, 2));   // Only 1ms elapsed, need 2
}

TEST(TimeUtils, HasElapsed_OverflowCase_Wrapped) {
    // millis() has wrapped around from max to 0
    uint32_t start = 0xFFFFFFFE;  // 4,294,967,294
    uint32_t now   = 0x00000002;  // 2 (wrapped around)
    
    // Elapsed time: 0xFFFFFFFF - 0xFFFFFFFE + 0x00000002 + 1 = 5ms
    EXPECT_TRUE(hasElapsed(now, start, 4));    // 4ms elapsed
    EXPECT_TRUE(hasElapsed(now, start, 5));    // Exactly 5ms elapsed  
    EXPECT_FALSE(hasElapsed(now, start, 6));   // Only 5ms elapsed, need 6
}

TEST(TimeUtils, HasElapsed_OverflowCase_LongWrap) {
    // Long time after overflow
    uint32_t start = 0xFFFF0000;  // Near max
    uint32_t now   = 0x00010000;  // After overflow
    
    // Elapsed: 0x00020000 = 131,072ms
    EXPECT_TRUE(hasElapsed(now, start, 131072));
    EXPECT_FALSE(hasElapsed(now, start, 131073));
}

TEST(TimeUtils, HasElapsed_MaxTimeout) {
    uint32_t start = 100;
    uint32_t now = 100 + 0x7FFFFFFF;  // Max safe timeout (~24.8 days)
    
    EXPECT_TRUE(hasElapsed(now, start, 0x7FFFFFFF));
}

TEST(TimeUtils, HasElapsed_HalfwayOverflow) {
    // Start near middle, wrap around
    uint32_t start = 0x80000000;
    uint32_t now   = 0x00000000;  // Wrapped
    
    // Elapsed: 0x80000000 = 2,147,483,648ms (~24.8 days)
    EXPECT_TRUE(hasElapsed(now, start, 0x80000000));
}

// ============================================================================
// IsTimeFor Tests
// ============================================================================

TEST(TimeUtils, IsTimeFor_NormalCase) {
    uint32_t now = 5000;
    uint32_t target = 3000;
    
    EXPECT_TRUE(isTimeFor(now, target));   // now > target
    EXPECT_TRUE(isTimeFor(now, now));       // now == target
    EXPECT_FALSE(isTimeFor(now, 6000));     // now < target
}

TEST(TimeUtils, IsTimeFor_OverflowCase) {
    // Target was before overflow, now is after
    uint32_t target = 0xFFFFFFF0;  // Close to max
    uint32_t now    = 0x00000010;  // After overflow
    
    // This should return true because we've passed the target time
    EXPECT_TRUE(isTimeFor(now, target));
}

TEST(TimeUtils, IsTimeFor_BothWrapped) {
    // Both times after overflow
    uint32_t target = 0x00001000;
    uint32_t now    = 0x00002000;
    
    EXPECT_TRUE(isTimeFor(now, target));
    EXPECT_FALSE(isTimeFor(target, now));
}

TEST(TimeUtils, IsTimeFor_ExactlyAtOverflow) {
    uint32_t target = 0xFFFFFFFF;
    uint32_t now    = 0xFFFFFFFF;
    
    EXPECT_TRUE(isTimeFor(now, target));
}

TEST(TimeUtils, IsTimeFor_OneMsAfterOverflow) {
    uint32_t target = 0xFFFFFFFF;
    uint32_t now    = 0x00000000;
    
    EXPECT_TRUE(isTimeFor(now, target));
}

// ============================================================================
// FutureTime Tests
// ============================================================================

TEST(TimeUtils, FutureTime_NormalCase) {
    uint32_t now = 1000;
    uint32_t future = futureTime(now, 500);
    
    EXPECT_EQ(1500u, future);
}

TEST(TimeUtils, FutureTime_ZeroDelay) {
    uint32_t now = 1000;
    uint32_t future = futureTime(now, 0);
    
    EXPECT_EQ(1000u, future);
}

TEST(TimeUtils, FutureTime_WillOverflow) {
    uint32_t now = 0xFFFFFFF0;
    uint32_t future = futureTime(now, 0x20);
    
    // Should wrap: 0xFFFFFFF0 + 0x20 = 0x00000010
    EXPECT_EQ(0x00000010u, future);
}

TEST(TimeUtils, FutureTime_LargeDelay) {
    uint32_t now = 1000;
    uint32_t future = futureTime(now, 0xFFFFFF00);
    
    // Should wrap
    EXPECT_EQ(0xFFFF0F00u, future);
}

// ============================================================================
// Integration Tests - Realistic Scenarios
// ============================================================================

TEST(TimeUtils, Integration_TimeoutCheckAcrossOverflow) {
    // Simulating a 10-second timeout that spans millis() overflow
    uint32_t startTime = 0xFFFFF000;  // Close to overflow
    uint32_t timeoutMs = 10000;
    
    // Check various points in time
    EXPECT_FALSE(hasElapsed(0xFFFFF000, startTime, timeoutMs));  // 0ms
    EXPECT_FALSE(hasElapsed(0xFFFFFFFF, startTime, timeoutMs));  // ~4s
    EXPECT_FALSE(hasElapsed(0x00000000, startTime, timeoutMs));  // ~4s
    EXPECT_FALSE(hasElapsed(0x00001000, startTime, timeoutMs));  // ~8s
    EXPECT_TRUE(hasElapsed(0x00002710, startTime, timeoutMs));   // Exactly 10s
    EXPECT_TRUE(hasElapsed(0x00003000, startTime, timeoutMs));   // >10s
}

TEST(TimeUtils, Integration_ScheduledEventAcrossOverflow) {
    // Schedule an event 5 seconds in the future, near overflow
    uint32_t now = 0xFFFFF000;
    uint32_t eventTime = futureTime(now, 5000);
    
    // Event time should have wrapped
    EXPECT_EQ(0x00000388u, eventTime);
    
    // Check if it's time for the event at various points
    EXPECT_FALSE(isTimeFor(0xFFFFF000, eventTime));  // Start
    EXPECT_FALSE(isTimeFor(0xFFFFFFFF, eventTime));  // Before overflow
    EXPECT_FALSE(isTimeFor(0x00000000, eventTime));  // Just after overflow
    EXPECT_FALSE(isTimeFor(0x00000387, eventTime));  // 1ms before event
    EXPECT_TRUE(isTimeFor(0x00000388, eventTime));   // Exactly event time
    EXPECT_TRUE(isTimeFor(0x00000400, eventTime));   // After event time
}

TEST(TimeUtils, Integration_PingBackoffPattern) {
    // Simulating the ping backoff pattern from TA_Comms
    uint32_t now = 0xFFFFFFF0;
    uint32_t nextPingAt = now;  // Send immediately
    uint32_t backoff = 200;
    
    // First ping
    EXPECT_TRUE(isTimeFor(now, nextPingAt));
    nextPingAt = futureTime(now, backoff);
    
    // Advance time across overflow
    now = 0x00000100;
    EXPECT_TRUE(isTimeFor(now, nextPingAt));  // Should be time for next ping
}

TEST(TimeUtils, Integration_ConnectionTimeout) {
    // Simulating connection timeout check from TA_Comms.service()
    uint32_t lastSeenMs = 0xFFFFFFF0;
    uint32_t timeoutMs = 5000;
    
    uint32_t now1 = 0xFFFFFFFF;  // 15ms later
    EXPECT_FALSE(hasElapsed(now1, lastSeenMs, timeoutMs));
    
    uint32_t now2 = 0x000013

88;  // 5000ms later (wrapped)
    EXPECT_TRUE(hasElapsed(now2, lastSeenMs, timeoutMs));
}

// Run all tests
int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
