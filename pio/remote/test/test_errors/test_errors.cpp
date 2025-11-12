/**
 * Unit tests for TA_Errors
 * Tests error code catalog and text mapping
 */

#include <gtest/gtest.h>
#include <TA_Errors.h>
#include <cstring>

using namespace ta::errors;

// ============================================================================
// Error Code Constants
// ============================================================================

TEST(Errors, ErrorCodes_ValidValues) {
    EXPECT_EQ(NONE, 0);
    EXPECT_EQ(NO_CHANGE, 1);
    EXPECT_EQ(EXCESSIVE_TIME, 2);
    EXPECT_EQ(SENSOR, 3);
    EXPECT_EQ(OVER_PSI, 4);
    EXPECT_EQ(UNDER_PSI, 5);
    EXPECT_EQ(CONFLICT, 6);
    EXPECT_EQ(UNKNOWN, 255);
}

// ============================================================================
// Error Text Mapping
// ============================================================================

TEST(Errors, ShortText_None) {
    EXPECT_STREQ(shortText(NONE), "None");
}

TEST(Errors, ShortText_NoChange) {
    EXPECT_STREQ(shortText(NO_CHANGE), "No change");
}

TEST(Errors, ShortText_ExcessiveTime) {
    EXPECT_STREQ(shortText(EXCESSIVE_TIME), "Too slow");
}

TEST(Errors, ShortText_Sensor) {
    EXPECT_STREQ(shortText(SENSOR), "Sensor");
}

TEST(Errors, ShortText_OverPsi) {
    EXPECT_STREQ(shortText(OVER_PSI), "Over PSI");
}

TEST(Errors, ShortText_UnderPsi) {
    EXPECT_STREQ(shortText(UNDER_PSI), "Under PSI");
}

TEST(Errors, ShortText_Conflict) {
    EXPECT_STREQ(shortText(CONFLICT), "Conflict");
}

TEST(Errors, ShortText_Unknown) {
    EXPECT_STREQ(shortText(UNKNOWN), "Unknown");
}

TEST(Errors, ShortText_InvalidCode) {
    // Unmapped error codes should return "Error"
    EXPECT_STREQ(shortText(99), "Error");
    EXPECT_STREQ(shortText(200), "Error");
}

// ============================================================================
// Text Length Validation (for display constraints)
// ============================================================================

TEST(Errors, ShortText_ReasonableLength) {
    // All error texts should fit on small OLED displays
    // Verify none exceed 12 characters
    EXPECT_LE(strlen(shortText(NONE)), 12u);
    EXPECT_LE(strlen(shortText(NO_CHANGE)), 12u);
    EXPECT_LE(strlen(shortText(EXCESSIVE_TIME)), 12u);
    EXPECT_LE(strlen(shortText(SENSOR)), 12u);
    EXPECT_LE(strlen(shortText(OVER_PSI)), 12u);
    EXPECT_LE(strlen(shortText(UNDER_PSI)), 12u);
    EXPECT_LE(strlen(shortText(CONFLICT)), 12u);
    EXPECT_LE(strlen(shortText(UNKNOWN)), 12u);
}

// ============================================================================
// Protocol Integration
// ============================================================================

TEST(Errors, ErrorCode_FitsInProtocolByte) {
    // All error codes must fit in protocol's uint8_t value field
    EXPECT_LE(NONE, 255);
    EXPECT_LE(NO_CHANGE, 255);
    EXPECT_LE(EXCESSIVE_TIME, 255);
    EXPECT_LE(SENSOR, 255);
    EXPECT_LE(OVER_PSI, 255);
    EXPECT_LE(UNDER_PSI, 255);
    EXPECT_LE(CONFLICT, 255);
    EXPECT_EQ(UNKNOWN, 255); // Max value
}

// ============================================================================
// Realistic Scenario
// ============================================================================

TEST(Errors, DisplayErrorScenario) {
    // Simulate controller detecting an error and remote displaying it
    uint8_t errorCode = NO_CHANGE;
    
    // Remote receives error code in protocol
    const char* displayText = shortText(errorCode);
    
    EXPECT_STREQ(displayText, "No change");
    EXPECT_TRUE(strlen(displayText) > 0);
    EXPECT_TRUE(strlen(displayText) < 20); // Reasonable for display
}
