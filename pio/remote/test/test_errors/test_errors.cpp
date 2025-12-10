/**
 * Unit tests for TA_Errors
 * Tests error code catalog and text mapping
 */

#include <gtest/gtest.h>
#include <TA_Errors.h>
#include <cstring>

using namespace trailair::errors;

// ============================================================================
// Error Code Constants
// ============================================================================

TEST(Errors, ErrorCodes_ValidValues) {
    EXPECT_EQ(static_cast<uint8_t>(ErrorCode::None), 0);
    EXPECT_EQ(static_cast<uint8_t>(ErrorCode::NoChange), 1);
    EXPECT_EQ(static_cast<uint8_t>(ErrorCode::ExcessiveTime), 2);
    EXPECT_EQ(static_cast<uint8_t>(ErrorCode::Sensor), 3);
    EXPECT_EQ(static_cast<uint8_t>(ErrorCode::OverPressure), 4);
    EXPECT_EQ(static_cast<uint8_t>(ErrorCode::UnderPressure), 5);
    EXPECT_EQ(static_cast<uint8_t>(ErrorCode::Conflict), 6);
    EXPECT_EQ(static_cast<uint8_t>(ErrorCode::Unknown), 255);
}

// ============================================================================
// Error Text Mapping
// ============================================================================

TEST(Errors, ShortText_None) {
    EXPECT_STREQ(getShortDescription(ErrorCode::None), "None");
}

TEST(Errors, ShortText_NoChange) {
    EXPECT_STREQ(getShortDescription(ErrorCode::NoChange), "No change");
}

TEST(Errors, ShortText_ExcessiveTime) {
    EXPECT_STREQ(getShortDescription(ErrorCode::ExcessiveTime), "Too slow");
}

TEST(Errors, ShortText_Sensor) {
    EXPECT_STREQ(getShortDescription(ErrorCode::Sensor), "Sensor");
}

TEST(Errors, ShortText_OverPsi) {
    EXPECT_STREQ(getShortDescription(ErrorCode::OverPressure), "Over PSI");
}

TEST(Errors, ShortText_UnderPsi) {
    EXPECT_STREQ(getShortDescription(ErrorCode::UnderPressure), "Under PSI");
}

TEST(Errors, ShortText_Conflict) {
    EXPECT_STREQ(getShortDescription(ErrorCode::Conflict), "Conflict");
}

TEST(Errors, ShortText_Unknown) {
    EXPECT_STREQ(getShortDescription(ErrorCode::Unknown), "Unknown");
}

TEST(Errors, ShortText_InvalidCode) {
    // Unmapped error codes should return "Error"
    EXPECT_STREQ(getShortDescription(99), "Error");
    EXPECT_STREQ(getShortDescription(200), "Error");
}

// ============================================================================
// Text Length Validation (for display constraints)
// ============================================================================

TEST(Errors, ShortText_ReasonableLength) {
    // All error texts should fit on small OLED displays
    // Verify none exceed 12 characters
    EXPECT_LE(strlen(getShortDescription(ErrorCode::None)), 12u);
    EXPECT_LE(strlen(getShortDescription(ErrorCode::NoChange)), 12u);
    EXPECT_LE(strlen(getShortDescription(ErrorCode::ExcessiveTime)), 12u);
    EXPECT_LE(strlen(getShortDescription(ErrorCode::Sensor)), 12u);
    EXPECT_LE(strlen(getShortDescription(ErrorCode::OverPressure)), 12u);
    EXPECT_LE(strlen(getShortDescription(ErrorCode::UnderPressure)), 12u);
    EXPECT_LE(strlen(getShortDescription(ErrorCode::Conflict)), 12u);
    EXPECT_LE(strlen(getShortDescription(ErrorCode::Unknown)), 12u);
}

// ============================================================================
// Protocol Integration
// ============================================================================

TEST(Errors, ErrorCode_FitsInProtocolByte) {
    // All error codes must fit in protocol's uint8_t value field
    EXPECT_LE(static_cast<uint8_t>(ErrorCode::None), 255);
    EXPECT_LE(static_cast<uint8_t>(ErrorCode::NoChange), 255);
    EXPECT_LE(static_cast<uint8_t>(ErrorCode::ExcessiveTime), 255);
    EXPECT_LE(static_cast<uint8_t>(ErrorCode::Sensor), 255);
    EXPECT_LE(static_cast<uint8_t>(ErrorCode::OverPressure), 255);
    EXPECT_LE(static_cast<uint8_t>(ErrorCode::UnderPressure), 255);
    EXPECT_LE(static_cast<uint8_t>(ErrorCode::Conflict), 255);
    EXPECT_EQ(static_cast<uint8_t>(ErrorCode::Unknown), 255); // Max value
}

// ============================================================================
// Realistic Scenario
// ============================================================================

TEST(Errors, DisplayErrorScenario) {
    // Simulate controller detecting an error and remote displaying it
    uint8_t errorCode = static_cast<uint8_t>(ErrorCode::NoChange);
    
    // Remote receives error code in protocol
    const char* displayText = getShortDescription(errorCode);
    
    EXPECT_STREQ(displayText, "No change");
    EXPECT_TRUE(strlen(displayText) > 0);
    EXPECT_TRUE(strlen(displayText) < 20); // Reasonable for display
}

// ============================================================================
// Main function
// ============================================================================
int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
