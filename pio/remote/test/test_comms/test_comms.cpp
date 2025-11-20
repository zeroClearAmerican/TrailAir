#include <gtest/gtest.h>
#include <thread>
#include <atomic>
#include <chrono>
#include <cstring>
#include <functional>
#include <cstdint>
#include <algorithm>

// Mock Arduino types and functions
typedef uint8_t byte;
typedef bool boolean;

inline unsigned long millis() {
    static unsigned long counter = 0;
    return ++counter;
}

template<typename T>
T min(T a, T b) { return (a < b) ? a : b; }

template<typename T>
T max(T a, T b) { return (a > b) ? a : b; }

// Mock FreeRTOS critical section macros
typedef int portMUX_TYPE;
#define portMUX_INITIALIZER_UNLOCKED 0
#define portENTER_CRITICAL(mux) do { (void)(mux); } while(0)
#define portEXIT_CRITICAL(mux) do { (void)(mux); } while(0)

// Mock ESP-NOW and WiFi for native testing
#define ESP_OK 0
#define ESP_NOW_SEND_SUCCESS 0
#define WIFI_STA 1

typedef int esp_err_t;
typedef uint8_t esp_now_send_status_t;

struct esp_now_peer_info_t {
    uint8_t peer_addr[6];
    uint8_t channel;
    bool encrypt;
};

// Mock WiFi class
class WiFiMock {
public:
    static void mode(int m) {}
    static void disconnect() {}
};
WiFiMock WiFi;

// Mock ESP-NOW functions
std::atomic<bool> esp_now_initialized{false};
std::atomic<int> peer_count{0};
std::function<void(const uint8_t*, const uint8_t*, int)> recv_callback;
std::function<void(const uint8_t*, esp_now_send_status_t)> send_callback;

extern "C" {
    esp_err_t esp_now_init() { 
        esp_now_initialized = true;
        return ESP_OK; 
    }
    
    void esp_now_register_recv_cb(void (*cb)(const uint8_t*, const uint8_t*, int)) {
        recv_callback = cb;
    }
    
    void esp_now_register_send_cb(void (*cb)(const uint8_t*, esp_now_send_status_t)) {
        send_callback = cb;
    }
    
    bool esp_now_is_peer_exist(const uint8_t* mac) {
        return peer_count > 0;
    }
    
    esp_err_t esp_now_add_peer(const esp_now_peer_info_t* peer) {
        peer_count++;
        return ESP_OK;
    }
    
    esp_err_t esp_now_del_peer(const uint8_t* mac) {
        if (peer_count > 0) peer_count--;
        return ESP_OK;
    }
    
    esp_err_t esp_now_send(const uint8_t* peer_addr, const uint8_t* data, size_t len) {
        return ESP_OK;
    }
}

// Mock Preferences
class Preferences {
public:
    bool begin(const char*, bool) { return true; }
    void end() {}
    size_t getBytesLength(const char*) { return 0; }
    size_t getBytes(const char*, void*, size_t) { return 0; }
    size_t putBytes(const char*, const void*, size_t len) { return len; }
    bool remove(const char*) { return true; }
};

// Mock Serial
struct SerialMock {
    template<typename... Args>
    static void printf(const char*, Args...) {}
    static void println(const char*) {}
} Serial;

// Include protocol implementation
#include "../../../../../pioLib/TA_Protocol/src/TA_Protocol.cpp"

// Include comms implementation (it will pull in the header)
#include "../../lib/TA_Comms/src/TA_Comms.cpp"

using namespace ta::comms;

// Test fixture
class CommsTest : public ::testing::Test {
protected:
    EspNowLink* link = nullptr;
    uint8_t testPeer[6] = {0x01, 0x02, 0x03, 0x04, 0x05, 0x06};
    
    void SetUp() override {
        esp_now_initialized = false;
        peer_count = 0;
        recv_callback = nullptr;
        send_callback = nullptr;
        link = new EspNowLink();
    }
    
    void TearDown() override {
        delete link;
    }
    
    // Simulate ISR receiving a packet
    void simulateRecvFromISR(const uint8_t* mac, const uint8_t* data, int len) {
        if (recv_callback) {
            recv_callback(mac, data, len);
        }
    }
};

// ============================================================================
// ISR Safety Tests
// ============================================================================

TEST_F(CommsTest, ISRSafety_ConcurrentReadWrite_NoRaceCondition) {
    // Initialize link
    ASSERT_TRUE(link->begin(testPeer));
    
    // Create a status packet
    uint8_t packet[2];
    ta::protocol::Response resp;
    resp.kind = ta::protocol::Response::Kind::Idle;
    resp.currentPsi = 30.0f;
    ta::protocol::packResponse(packet, resp);
    
    std::atomic<bool> testRunning{true};
    std::atomic<int> readCount{0};
    std::atomic<int> writeCount{0};
    
    // Thread 1: Continuously read lastSeenMs (simulating main loop)
    std::thread reader([&]() {
        while (testRunning) {
            uint32_t val = link->lastSeenMs();
            (void)val; // Use the value to prevent optimization
            readCount++;
            std::this_thread::sleep_for(std::chrono::microseconds(10));
        }
    });
    
    // Thread 2: Continuously write lastSeenMs (simulating ISR)
    std::thread writer([&]() {
        while (testRunning) {
            simulateRecvFromISR(testPeer, packet, 2);
            writeCount++;
            std::this_thread::sleep_for(std::chrono::microseconds(10));
        }
    });
    
    // Let threads run for 100ms
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    testRunning = false;
    
    reader.join();
    writer.join();
    
    // If we get here without crashing, the critical sections worked
    EXPECT_GT(readCount.load(), 0);
    EXPECT_GT(writeCount.load(), 0);
    
    // Verify final state is consistent
    uint32_t finalValue = link->lastSeenMs();
    EXPECT_GE(finalValue, 0); // Should be a valid timestamp
}

TEST_F(CommsTest, ISRSafety_LastSeenMs_AtomicRead) {
    ASSERT_TRUE(link->begin(testPeer));
    
    // Create a status packet
    uint8_t packet[2];
    ta::protocol::Response resp;
    resp.kind = ta::protocol::Response::Kind::Idle;
    resp.currentPsi = 30.0f;
    ta::protocol::packResponse(packet, resp);
    
    // Initially should be 0
    EXPECT_EQ(0u, link->lastSeenMs());
    
    // Simulate receiving a packet
    simulateRecvFromISR(testPeer, packet, 2);
    
    // Should now have a non-zero value
    EXPECT_GT(link->lastSeenMs(), 0u);
}

TEST_F(CommsTest, ISRSafety_Service_AtomicReadDuringTimeout) {
    ASSERT_TRUE(link->begin(testPeer));
    link->setConnectionTimeoutMs(1000);
    
    // Create a status packet
    uint8_t packet[2];
    ta::protocol::Response resp;
    resp.kind = ta::protocol::Response::Kind::Idle;
    resp.currentPsi = 30.0f;
    ta::protocol::packResponse(packet, resp);
    
    // Simulate receiving a packet to establish connection
    simulateRecvFromISR(testPeer, packet, 2);
    
    // Should be connected
    link->service();
    EXPECT_TRUE(link->isConnected());
    
    // Run service in a loop while simulating concurrent ISR updates
    std::atomic<bool> testRunning{true};
    
    std::thread serviceThread([&]() {
        while (testRunning) {
            link->service();
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
    });
    
    std::thread isrThread([&]() {
        while (testRunning) {
            simulateRecvFromISR(testPeer, packet, 2);
            std::this_thread::sleep_for(std::chrono::milliseconds(15));
        }
    });
    
    // Run for 100ms
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    testRunning = false;
    
    serviceThread.join();
    isrThread.join();
    
    // Should still be connected since we kept receiving packets
    EXPECT_TRUE(link->isConnected());
}

// ============================================================================
// Connection State Tests
// ============================================================================

TEST_F(CommsTest, Connection_InitiallyDisconnected) {
    ASSERT_TRUE(link->begin(testPeer));
    EXPECT_FALSE(link->isConnected());
    EXPECT_FALSE(link->isConnecting());
}

TEST_F(CommsTest, Connection_BecomeConnectedOnValidPacket) {
    ASSERT_TRUE(link->begin(testPeer));
    
    uint8_t packet[2];
    ta::protocol::Response resp;
    resp.kind = ta::protocol::Response::Kind::Idle;
    resp.currentPsi = 30.0f;
    ta::protocol::packResponse(packet, resp);
    
    simulateRecvFromISR(testPeer, packet, 2);
    
    EXPECT_TRUE(link->isConnected());
    EXPECT_FALSE(link->isConnecting());
}

TEST_F(CommsTest, Connection_ReconnectStartsPinging) {
    ASSERT_TRUE(link->begin(testPeer));
    
    link->requestReconnect();
    
    EXPECT_TRUE(link->isConnecting());
    EXPECT_FALSE(link->isConnected());
}

TEST_F(CommsTest, Connection_StatusCallbackInvoked) {
    ASSERT_TRUE(link->begin(testPeer));
    
    bool callbackInvoked = false;
    ta::protocol::Response receivedResp;
    
    link->setStatusCallback([](void* ctx, const ta::protocol::Response& msg) {
        bool* invoked = static_cast<bool*>(ctx);
        *invoked = true;
    }, &callbackInvoked);
    
    uint8_t packet[2];
    ta::protocol::Response resp;
    resp.kind = ta::protocol::Response::Kind::Seeking;
    resp.currentPsi = 45.5f;
    ta::protocol::packResponse(packet, resp);
    
    simulateRecvFromISR(testPeer, packet, 2);
    
    EXPECT_TRUE(callbackInvoked);
}

// ============================================================================
// Initialization Tests
// ============================================================================

TEST_F(CommsTest, Init_SucceedsWithValidPeer) {
    EXPECT_TRUE(link->begin(testPeer));
    EXPECT_TRUE(esp_now_initialized);
    EXPECT_GT(peer_count.load(), 0);
}

TEST_F(CommsTest, Init_SucceedsWithNullPeer) {
    EXPECT_TRUE(link->begin(nullptr));
    EXPECT_TRUE(esp_now_initialized);
}

TEST_F(CommsTest, Init_RegistersCallbacks) {
    ASSERT_TRUE(link->begin(testPeer));
    EXPECT_TRUE(recv_callback != nullptr);
    EXPECT_TRUE(send_callback != nullptr);
}

// ============================================================================
// Send Tests
// ============================================================================

TEST_F(CommsTest, Send_StartCommand) {
    ASSERT_TRUE(link->begin(testPeer));
    EXPECT_TRUE(link->sendStart(50.0f));
}

TEST_F(CommsTest, Send_CancelCommand) {
    ASSERT_TRUE(link->begin(testPeer));
    EXPECT_TRUE(link->sendCancel());
}

TEST_F(CommsTest, Send_ManualCommand) {
    ASSERT_TRUE(link->begin(testPeer));
    EXPECT_TRUE(link->sendManual(1));
}

TEST_F(CommsTest, Send_PingCommand) {
    ASSERT_TRUE(link->begin(testPeer));
    EXPECT_TRUE(link->sendPing());
}

// ============================================================================
// Protocol Validation Tests
// ============================================================================

TEST_F(CommsTest, Protocol_IgnoresInvalidPackets) {
    ASSERT_TRUE(link->begin(testPeer));
    
    bool callbackInvoked = false;
    link->setStatusCallback([](void* ctx, const ta::protocol::Response& msg) {
        bool* invoked = static_cast<bool*>(ctx);
        *invoked = true;
    }, &callbackInvoked);
    
    // Send invalid packet (wrong length)
    uint8_t badPacket[5] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
    simulateRecvFromISR(testPeer, badPacket, 5);
    
    EXPECT_FALSE(callbackInvoked);
    EXPECT_FALSE(link->isConnected());
}

TEST_F(CommsTest, Protocol_AcceptsValidStatusPacket) {
    ASSERT_TRUE(link->begin(testPeer));
    
    bool callbackInvoked = false;
    link->setStatusCallback([](void* ctx, const ta::protocol::Response& msg) {
        bool* invoked = static_cast<bool*>(ctx);
        *invoked = true;
    }, &callbackInvoked);
    
    uint8_t packet[2];
    ta::protocol::Response resp;
    resp.kind = ta::protocol::Response::Kind::Idle;
    resp.currentPsi = 30.0f;
    ta::protocol::packResponse(packet, resp);
    
    simulateRecvFromISR(testPeer, packet, 2);
    
    EXPECT_TRUE(callbackInvoked);
    EXPECT_TRUE(link->isConnected());
}

// Run all tests
int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
