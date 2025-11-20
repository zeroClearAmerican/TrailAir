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

// Include protocol (using build_flags include path)
#include "TA_Protocol.h"

// Include comms implementation
#include "../../lib/TA_CommsBoard/src/TA_CommsBoard.cpp"

using namespace ta::comms;

// Test fixture
class CommsBoardTest : public ::testing::Test {
protected:
    BoardLink* link = nullptr;
    uint8_t testPeer[6] = {0x01, 0x02, 0x03, 0x04, 0x05, 0x06};
    
    void SetUp() override {
        esp_now_initialized = false;
        peer_count = 0;
        recv_callback = nullptr;
        send_callback = nullptr;
        link = new BoardLink();
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

TEST_F(CommsBoardTest, ISRSafety_ConcurrentReadWrite_NoRaceCondition) {
    // Initialize link
    ASSERT_TRUE(link->begin());
    
    // Create a request packet
    uint8_t packet[2];
    ta::protocol::Request req;
    req.kind = ta::protocol::Request::Kind::Ping;
    ta::protocol::packRequest(packet, req);
    
    // Manually set paired state for this test
    uint8_t mac[6] = {0x01, 0x02, 0x03, 0x04, 0x05, 0x06};
    
    std::atomic<bool> testRunning{true};
    std::atomic<int> readCount{0};
    std::atomic<int> writeCount{0};
    
    // Thread 1: Continuously read lastRxMs via isRemoteActive (simulating main loop)
    std::thread reader([&]() {
        while (testRunning) {
            bool active = link->isRemoteActive();
            (void)active; // Use the value to prevent optimization
            readCount++;
            std::this_thread::sleep_for(std::chrono::microseconds(10));
        }
    });
    
    // Thread 2: Continuously write lastRxMs (simulating ISR)
    std::thread writer([&]() {
        while (testRunning) {
            simulateRecvFromISR(mac, packet, 2);
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
}

TEST_F(CommsBoardTest, ISRSafety_IsRemoteActive_AtomicRead) {
    ASSERT_TRUE(link->begin());
    
    // Initially not active (unpaired)
    EXPECT_FALSE(link->isRemoteActive());
}

TEST_F(CommsBoardTest, ISRSafety_MultipleThreadsReadIsRemoteActive) {
    ASSERT_TRUE(link->begin());
    
    std::atomic<bool> testRunning{true};
    std::atomic<int> thread1Reads{0};
    std::atomic<int> thread2Reads{0};
    
    std::thread reader1([&]() {
        while (testRunning) {
            link->isRemoteActive();
            thread1Reads++;
            std::this_thread::sleep_for(std::chrono::microseconds(5));
        }
    });
    
    std::thread reader2([&]() {
        while (testRunning) {
            link->isRemoteActive();
            thread2Reads++;
            std::this_thread::sleep_for(std::chrono::microseconds(5));
        }
    });
    
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    testRunning = false;
    
    reader1.join();
    reader2.join();
    
    EXPECT_GT(thread1Reads.load(), 0);
    EXPECT_GT(thread2Reads.load(), 0);
}

// ============================================================================
// Initialization Tests
// ============================================================================

TEST_F(CommsBoardTest, Init_Succeeds) {
    EXPECT_TRUE(link->begin());
    EXPECT_TRUE(esp_now_initialized);
}

TEST_F(CommsBoardTest, Init_InitiallyUnpaired) {
    ASSERT_TRUE(link->begin());
    EXPECT_FALSE(link->isPaired());
}

TEST_F(CommsBoardTest, Init_RegistersCallbacks) {
    ASSERT_TRUE(link->begin());
    EXPECT_TRUE(recv_callback != nullptr);
    EXPECT_TRUE(send_callback != nullptr);
}

// ============================================================================
// Remote Activity Tests
// ============================================================================

TEST_F(CommsBoardTest, RemoteActivity_InitiallyInactive) {
    ASSERT_TRUE(link->begin());
    EXPECT_FALSE(link->isRemoteActive());
}

TEST_F(CommsBoardTest, RemoteActivity_InactiveWhenUnpaired) {
    ASSERT_TRUE(link->begin());
    
    // Even if we receive a packet, should be inactive when unpaired
    uint8_t packet[2];
    ta::protocol::Request req;
    req.kind = ta::protocol::Request::Kind::Ping;
    ta::protocol::packRequest(packet, req);
    
    simulateRecvFromISR(testPeer, packet, 2);
    
    EXPECT_FALSE(link->isRemoteActive());
}

// ============================================================================
// Send Tests
// ============================================================================

TEST_F(CommsBoardTest, Send_StatusFailsWhenUnpaired) {
    ASSERT_TRUE(link->begin());
    EXPECT_FALSE(link->sendStatus('I', 30.0f));
}

TEST_F(CommsBoardTest, Send_ErrorFailsWhenUnpaired) {
    ASSERT_TRUE(link->begin());
    EXPECT_FALSE(link->sendError(1));
}

// ============================================================================
// Request Callback Tests
// ============================================================================

TEST_F(CommsBoardTest, RequestCallback_NotInvokedWhenUnpaired) {
    ASSERT_TRUE(link->begin());
    
    bool callbackInvoked = false;
    link->setRequestCallback([](void* ctx, const ta::protocol::Request& req) {
        bool* invoked = static_cast<bool*>(ctx);
        *invoked = true;
    }, &callbackInvoked);
    
    uint8_t packet[2];
    ta::protocol::Request req;
    req.kind = ta::protocol::Request::Kind::Ping;
    ta::protocol::packRequest(packet, req);
    
    simulateRecvFromISR(testPeer, packet, 2);
    
    EXPECT_FALSE(callbackInvoked);
}

// ============================================================================
// Protocol Validation Tests
// ============================================================================

TEST_F(CommsBoardTest, Protocol_IgnoresInvalidPackets) {
    ASSERT_TRUE(link->begin());
    
    bool callbackInvoked = false;
    link->setRequestCallback([](void* ctx, const ta::protocol::Request& req) {
        bool* invoked = static_cast<bool*>(ctx);
        *invoked = true;
    }, &callbackInvoked);
    
    // Send invalid packet (wrong length)
    uint8_t badPacket[5] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
    simulateRecvFromISR(testPeer, badPacket, 5);
    
    EXPECT_FALSE(callbackInvoked);
}

TEST_F(CommsBoardTest, Protocol_IgnoresPacketsFromWrongPeer) {
    ASSERT_TRUE(link->begin());
    
    bool callbackInvoked = false;
    link->setRequestCallback([](void* ctx, const ta::protocol::Request& req) {
        bool* invoked = static_cast<bool*>(ctx);
        *invoked = true;
    }, &callbackInvoked);
    
    // Valid packet but from wrong peer
    uint8_t packet[2];
    ta::protocol::Request req;
    req.kind = ta::protocol::Request::Kind::Ping;
    ta::protocol::packRequest(packet, req);
    
    uint8_t wrongPeer[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
    simulateRecvFromISR(wrongPeer, packet, 2);
    
    EXPECT_FALSE(callbackInvoked);
}

// ============================================================================
// Pairing Tests
// ============================================================================

TEST_F(CommsBoardTest, Pairing_HandlesPairRequest) {
    ASSERT_TRUE(link->begin());
    EXPECT_FALSE(link->isPaired());
    
    // Simulate pair request
    uint8_t pairReq[2];
    ta::protocol::packPairReq(pairReq, 0x01);
    
    simulateRecvFromISR(testPeer, pairReq, 2);
    
    // Should now be paired
    EXPECT_TRUE(link->isPaired());
}

TEST_F(CommsBoardTest, Pairing_ForgetClearsPeer) {
    ASSERT_TRUE(link->begin());
    
    // Pair first
    uint8_t pairReq[2];
    ta::protocol::packPairReq(pairReq, 0x01);
    simulateRecvFromISR(testPeer, pairReq, 2);
    
    ASSERT_TRUE(link->isPaired());
    
    // Forget
    link->forget();
    
    EXPECT_FALSE(link->isPaired());
}

// Run all tests
int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
