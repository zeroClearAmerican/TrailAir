/**
 * Unit tests for TA_Protocol (wire format v2)
 */

#include <gtest/gtest.h>
#include <TA_Protocol.h>

using namespace trailair;
using namespace trailair::protocol;

// ============================================================================
// PSI conversion
// ============================================================================

TEST(Protocol, PsiToByte) {
    EXPECT_EQ(psiToByte(0.0f), 0);
    EXPECT_EQ(psiToByte(30.0f), 60);
    EXPECT_EQ(psiToByte(15.5f), 31);
    EXPECT_EQ(psiToByte(-10.0f), 0);    // clamped
    EXPECT_EQ(psiToByte(200.0f), 255);  // clamped to 127.5
}

TEST(Protocol, PsiRoundTrip) {
    EXPECT_FLOAT_EQ(byteToPsi(psiToByte(25.5f)), 25.5f);
    EXPECT_FLOAT_EQ(byteToPsi(psiToByte(127.5f)), 127.5f);
}

// ============================================================================
// Round trips
// ============================================================================

static Frame roundTrip(const Frame& in, int expectedLength) {
    uint8_t buf[MAX_FRAME_LENGTH];
    int len = pack(buf, in);
    EXPECT_EQ(len, expectedLength);
    EXPECT_EQ(buf[0], MAGIC);
    Frame out;
    EXPECT_TRUE(parse(buf, len, out));
    EXPECT_EQ(out.type, in.type);
    return out;
}

TEST(Protocol, ButtonFramesRoundTrip) {
    const ButtonAction actions[] = { ButtonAction::Pressed, ButtonAction::Released,
                                     ButtonAction::Click, ButtonAction::LongHold };
    for (ButtonAction a : actions) {
        for (uint8_t b = 0; b < BUTTON_COUNT; ++b) {
            Frame f;
            f.type = buttonFrameType(a);
            f.button = static_cast<ButtonId>(b);
            Frame out = roundTrip(f, 3);
            EXPECT_EQ(out.button, f.button);
            ButtonAction back;
            ASSERT_TRUE(buttonAction(out.type, back));
            EXPECT_EQ(back, a);
        }
    }
}

TEST(Protocol, StatusRoundTrip) {
    Frame f;
    f.type = FrameType::Status;
    f.status.state = ControllerState::AirUp;
    f.status.view = View::Seeking;
    f.status.currentPSI = 28.5f;
    f.status.targetPSI = 32.0f;
    f.status.errorCode = 0;
    Frame out = roundTrip(f, 7);
    EXPECT_EQ(out.status.state, ControllerState::AirUp);
    EXPECT_EQ(out.status.view, View::Seeking);
    EXPECT_FLOAT_EQ(out.status.currentPSI, 28.5f);
    EXPECT_FLOAT_EQ(out.status.targetPSI, 32.0f);
}

TEST(Protocol, StatusCarriesErrorAndPressureTogether) {
    Frame f;
    f.type = FrameType::Status;
    f.status.state = ControllerState::Error;
    f.status.view = View::Error;
    f.status.currentPSI = 50.0f;
    f.status.errorCode = 4;  // OverPressure
    Frame out = roundTrip(f, 7);
    EXPECT_EQ(out.status.errorCode, 4);
    EXPECT_FLOAT_EQ(out.status.currentPSI, 50.0f);
}

TEST(Protocol, SimpleFramesRoundTrip) {
    const FrameType types[] = { FrameType::Ping, FrameType::PairRequest, FrameType::PairAck, FrameType::PairBusy };
    for (FrameType t : types) {
        Frame f;
        f.type = t;
        roundTrip(f, 2);
    }
}

// ============================================================================
// Rejection
// ============================================================================

TEST(Protocol, RejectsBadMagic) {
    uint8_t data[] = { 0x00, 'P' };
    Frame f;
    EXPECT_FALSE(parse(data, 2, f));
}

TEST(Protocol, RejectsUnknownType) {
    uint8_t data[] = { MAGIC, 'Z' };
    Frame f;
    EXPECT_FALSE(parse(data, 2, f));
}

TEST(Protocol, RejectsWrongLength) {
    uint8_t data[] = { MAGIC, 'D', 0, 0 };
    Frame f;
    EXPECT_FALSE(parse(data, 4, f));  // button frame is 3 bytes
    EXPECT_FALSE(parse(data, 2, f));
    EXPECT_FALSE(parse(data, 1, f));
}

TEST(Protocol, RejectsOutOfRangeButton) {
    uint8_t data[] = { MAGIC, 'C', BUTTON_COUNT };
    Frame f;
    EXPECT_FALSE(parse(data, 3, f));
}

TEST(Protocol, RejectsInvalidStatusFields) {
    uint8_t badState[] = { MAGIC, 'S', 'Q', 'I', 0, 0, 0 };
    uint8_t remoteOnlyView[] = { MAGIC, 'S', 'I', static_cast<uint8_t>(View::Pairing), 0, 0, 0 };
    Frame f;
    EXPECT_FALSE(parse(badState, 7, f));
    EXPECT_FALSE(parse(remoteOnlyView, 7, f));  // board never sends remote-local screens
}

TEST(Protocol, RejectsLegacyV1Frames) {
    uint8_t v1Status[] = { 'I', 'I', 60, 64 };  // old 4-byte status
    uint8_t v1Pair[] = { 'R', 0x01 };
    Frame f;
    EXPECT_FALSE(parse(v1Status, 4, f));
    EXPECT_FALSE(parse(v1Pair, 2, f));
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
