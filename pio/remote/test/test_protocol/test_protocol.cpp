/**
 * Unit tests for TA_Protocol
 * Tests pack/unpack of all message types, edge cases, and data integrity
 */

#include <gtest/gtest.h>
#include <TA_Protocol.h>

using namespace ta::protocol;

// ============================================================================
// PSI Conversion Tests
// ============================================================================

TEST(Protocol, PsiToByte_NormalRange) {
    EXPECT_EQ(psiToByte05(0.0f), 0);
    EXPECT_EQ(psiToByte05(10.0f), 20);
    EXPECT_EQ(psiToByte05(30.0f), 60);
    EXPECT_EQ(psiToByte05(63.5f), 127);
}

TEST(Protocol, PsiToByte_HalfSteps) {
    EXPECT_EQ(psiToByte05(0.5f), 1);
    EXPECT_EQ(psiToByte05(15.5f), 31);
    EXPECT_EQ(psiToByte05(30.5f), 61);
}

TEST(Protocol, PsiToByte_Clamping) {
    EXPECT_EQ(psiToByte05(-10.0f), 0);    // negative clamped to 0
    EXPECT_EQ(psiToByte05(200.0f), 255);  // over max clamped to 127.5 PSI = 255
}

TEST(Protocol, ByteToPsi_NormalRange) {
    EXPECT_FLOAT_EQ(byteToPsi05(0), 0.0f);
    EXPECT_FLOAT_EQ(byteToPsi05(20), 10.0f);
    EXPECT_FLOAT_EQ(byteToPsi05(60), 30.0f);
    EXPECT_FLOAT_EQ(byteToPsi05(127), 63.5f);
}

TEST(Protocol, ByteToPsi_HalfSteps) {
    EXPECT_FLOAT_EQ(byteToPsi05(1), 0.5f);
    EXPECT_FLOAT_EQ(byteToPsi05(31), 15.5f);
}

TEST(Protocol, PsiRoundTrip) {
    float original = 25.5f;
    uint8_t packed = psiToByte05(original);
    float unpacked = byteToPsi05(packed);
    EXPECT_FLOAT_EQ(unpacked, original);
}

// ============================================================================
// Request Packing/Parsing Tests
// ============================================================================

TEST(Protocol, PackRequest_Idle) {
    Request req;
    req.kind = Request::Kind::Idle;
    
    uint8_t buf[kPayloadLen];
    packRequest(buf, req);
    
    EXPECT_EQ(buf[0], static_cast<uint8_t>(Cmd::Idle));
    EXPECT_EQ(buf[1], 0);
}

TEST(Protocol, PackRequest_Start) {
    Request req;
    req.kind = Request::Kind::Start;
    req.targetPsi = 30.0f;
    
    uint8_t buf[kPayloadLen];
    packRequest(buf, req);
    
    EXPECT_EQ(buf[0], static_cast<uint8_t>(Cmd::Start));
    EXPECT_EQ(buf[1], 60); // 30 PSI * 2
}

TEST(Protocol, PackRequest_ManualVent) {
    Request req;
    req.kind = Request::Kind::Manual;
    req.manual = ManualCode::Vent;
    
    uint8_t buf[kPayloadLen];
    packRequest(buf, req);
    
    EXPECT_EQ(buf[0], static_cast<uint8_t>(Cmd::Manual));
    EXPECT_EQ(buf[1], 0x00);
}

TEST(Protocol, PackRequest_ManualAir) {
    Request req;
    req.kind = Request::Kind::Manual;
    req.manual = ManualCode::Air;
    
    uint8_t buf[kPayloadLen];
    packRequest(buf, req);
    
    EXPECT_EQ(buf[0], static_cast<uint8_t>(Cmd::Manual));
    EXPECT_EQ(buf[1], 0xFF);
}

TEST(Protocol, PackRequest_Ping) {
    Request req;
    req.kind = Request::Kind::Ping;
    
    uint8_t buf[kPayloadLen];
    packRequest(buf, req);
    
    EXPECT_EQ(buf[0], static_cast<uint8_t>(Cmd::Ping));
    EXPECT_EQ(buf[1], 0);
}

TEST(Protocol, ParseRequest_Idle) {
    uint8_t data[] = {'I', 0x00};
    Request req;
    
    ASSERT_TRUE(parseRequest(data, kPayloadLen, req));
    EXPECT_EQ(req.kind, Request::Kind::Idle);
}

TEST(Protocol, ParseRequest_Start) {
    uint8_t data[] = {'S', 40}; // 20 PSI
    Request req;
    
    ASSERT_TRUE(parseRequest(data, kPayloadLen, req));
    EXPECT_EQ(req.kind, Request::Kind::Start);
    EXPECT_FLOAT_EQ(req.targetPsi, 20.0f);
}

TEST(Protocol, ParseRequest_ManualVent) {
    uint8_t data[] = {'M', 0x00};
    Request req;
    
    ASSERT_TRUE(parseRequest(data, kPayloadLen, req));
    EXPECT_EQ(req.kind, Request::Kind::Manual);
    EXPECT_EQ(req.manual, ManualCode::Vent);
}

TEST(Protocol, ParseRequest_ManualAir) {
    uint8_t data[] = {'M', 0xFF};
    Request req;
    
    ASSERT_TRUE(parseRequest(data, kPayloadLen, req));
    EXPECT_EQ(req.kind, Request::Kind::Manual);
    EXPECT_EQ(req.manual, ManualCode::Air);
}

TEST(Protocol, ParseRequest_InvalidLength) {
    uint8_t data[] = {'S', 40, 99}; // 3 bytes
    Request req;
    
    EXPECT_FALSE(parseRequest(data, 3, req));
}

TEST(Protocol, ParseRequest_InvalidCommand) {
    uint8_t data[] = {'X', 0x00}; // Unknown command
    Request req;
    
    EXPECT_FALSE(parseRequest(data, kPayloadLen, req));
}

TEST(Protocol, RequestRoundTrip_Start) {
    Request original;
    original.kind = Request::Kind::Start;
    original.targetPsi = 35.5f;
    
    uint8_t buf[kPayloadLen];
    packRequest(buf, original);
    
    Request parsed;
    ASSERT_TRUE(parseRequest(buf, kPayloadLen, parsed));
    
    EXPECT_EQ(parsed.kind, original.kind);
    EXPECT_FLOAT_EQ(parsed.targetPsi, original.targetPsi);
}

// ============================================================================
// Response Parsing Tests
// ============================================================================

TEST(Protocol, ParseResponse_Idle) {
    uint8_t data[] = {'I', 50}; // Idle at 25 PSI
    Response resp;
    
    ASSERT_TRUE(parseResponse(data, kPayloadLen, resp));
    EXPECT_EQ(resp.status, Status::Idle);
    EXPECT_EQ(resp.value, 50);
    EXPECT_FLOAT_EQ(byteToPsi05(resp.value), 25.0f);
}

TEST(Protocol, ParseResponse_AirUp) {
    uint8_t data[] = {'U', 30};
    Response resp;
    
    ASSERT_TRUE(parseResponse(data, kPayloadLen, resp));
    EXPECT_EQ(resp.status, Status::AirUp);
    EXPECT_EQ(resp.value, 30);
}

TEST(Protocol, ParseResponse_Venting) {
    uint8_t data[] = {'V', 60};
    Response resp;
    
    ASSERT_TRUE(parseResponse(data, kPayloadLen, resp));
    EXPECT_EQ(resp.status, Status::Venting);
    EXPECT_EQ(resp.value, 60);
}

TEST(Protocol, ParseResponse_Checking) {
    uint8_t data[] = {'C', 58};
    Response resp;
    
    ASSERT_TRUE(parseResponse(data, kPayloadLen, resp));
    EXPECT_EQ(resp.status, Status::Checking);
    EXPECT_EQ(resp.value, 58);
}

TEST(Protocol, ParseResponse_Error) {
    uint8_t data[] = {'E', 42}; // Error code 42
    Response resp;
    
    ASSERT_TRUE(parseResponse(data, kPayloadLen, resp));
    EXPECT_EQ(resp.status, Status::Error);
    EXPECT_EQ(resp.value, 42); // Error code, not PSI
}

TEST(Protocol, ParseResponse_InvalidLength) {
    uint8_t data[] = {'I'}; // 1 byte
    Response resp;
    
    EXPECT_FALSE(parseResponse(data, 1, resp));
}

TEST(Protocol, ParseResponse_InvalidStatus) {
    uint8_t data[] = {'Z', 50}; // Unknown status
    Response resp;
    
    EXPECT_FALSE(parseResponse(data, kPayloadLen, resp));
}

// ============================================================================
// Pairing Frame Tests
// ============================================================================

TEST(Protocol, PackPairReq) {
    uint8_t buf[kPayloadLen];
    packPairReq(buf, 123);
    
    EXPECT_EQ(buf[0], 'R');
    EXPECT_EQ(buf[1], 123);
}

TEST(Protocol, PackPairAck) {
    uint8_t buf[kPayloadLen];
    packPairAck(buf, 123);
    
    EXPECT_EQ(buf[0], 'A');
    EXPECT_EQ(buf[1], 123);
}

TEST(Protocol, PackPairBusy) {
    uint8_t buf[kPayloadLen];
    packPairBusy(buf, 1);
    
    EXPECT_EQ(buf[0], 'B');
    EXPECT_EQ(buf[1], 1);
}

TEST(Protocol, IsPairingFrame_Req) {
    uint8_t data[] = {'R', 123};
    EXPECT_TRUE(isPairingFrame(data, kPayloadLen));
}

TEST(Protocol, IsPairingFrame_Ack) {
    uint8_t data[] = {'A', 123};
    EXPECT_TRUE(isPairingFrame(data, kPayloadLen));
}

TEST(Protocol, IsPairingFrame_Busy) {
    uint8_t data[] = {'B', 1};
    EXPECT_TRUE(isPairingFrame(data, kPayloadLen));
}

TEST(Protocol, IsPairingFrame_NotPairing) {
    uint8_t data[] = {'I', 50}; // Status frame
    EXPECT_FALSE(isPairingFrame(data, kPayloadLen));
}

TEST(Protocol, ParsePair_Req) {
    uint8_t data[] = {'R', 99};
    PairMsg msg;
    
    ASSERT_TRUE(parsePair(data, kPayloadLen, msg));
    EXPECT_EQ(msg.op, PairOp::Req);
    EXPECT_EQ(msg.value, 99);
}

TEST(Protocol, ParsePair_Ack) {
    uint8_t data[] = {'A', 99};
    PairMsg msg;
    
    ASSERT_TRUE(parsePair(data, kPayloadLen, msg));
    EXPECT_EQ(msg.op, PairOp::Ack);
    EXPECT_EQ(msg.value, 99);
}

TEST(Protocol, ParsePair_Busy) {
    uint8_t data[] = {'B', 2};
    PairMsg msg;
    
    ASSERT_TRUE(parsePair(data, kPayloadLen, msg));
    EXPECT_EQ(msg.op, PairOp::Busy);
    EXPECT_EQ(msg.value, 2);
}

TEST(Protocol, ParsePair_Invalid) {
    uint8_t data[] = {'I', 50}; // Not a pairing frame
    PairMsg msg;
    
    EXPECT_FALSE(parsePair(data, kPayloadLen, msg));
}

TEST(Protocol, PairRoundTrip) {
    uint8_t buf[kPayloadLen];
    packPairReq(buf, 42);
    
    PairMsg msg;
    ASSERT_TRUE(parsePair(buf, kPayloadLen, msg));
    
    EXPECT_EQ(msg.op, PairOp::Req);
    EXPECT_EQ(msg.value, 42);
}

// ============================================================================
// Edge Cases
// ============================================================================

TEST(Protocol, MaxPsiValue) {
    // Maximum representable PSI is 127.5 (255 / 2)
    float maxPsi = 127.5f;
    uint8_t packed = psiToByte05(maxPsi);
    EXPECT_EQ(packed, 255);
    
    float unpacked = byteToPsi05(packed);
    EXPECT_FLOAT_EQ(unpacked, maxPsi);
}

TEST(Protocol, ZeroPsi) {
    Request req;
    req.kind = Request::Kind::Start;
    req.targetPsi = 0.0f;
    
    uint8_t buf[kPayloadLen];
    packRequest(buf, req);
    
    EXPECT_EQ(buf[1], 0);
    
    Request parsed;
    parseRequest(buf, kPayloadLen, parsed);
    EXPECT_FLOAT_EQ(parsed.targetPsi, 0.0f);
}

// ============================================================================
// Main function
// ============================================================================
int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
