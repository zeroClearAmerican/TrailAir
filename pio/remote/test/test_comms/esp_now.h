#pragma once
// Mock esp_now.h for native testing
#include <cstdint>
#include <cstddef>

#define ESP_OK 0
#define ESP_NOW_SEND_SUCCESS 0

typedef int esp_err_t;
typedef uint8_t esp_now_send_status_t;

struct esp_now_peer_info_t {
    uint8_t peer_addr[6];
    uint8_t channel;
    bool encrypt;
};

// Declare functions (definitions in test file)
extern "C" {
    esp_err_t esp_now_init();
    void esp_now_register_recv_cb(void (*cb)(const uint8_t*, const uint8_t*, int));
    void esp_now_register_send_cb(void (*cb)(const uint8_t*, esp_now_send_status_t));
    bool esp_now_is_peer_exist(const uint8_t* mac);
    esp_err_t esp_now_add_peer(const esp_now_peer_info_t* peer);
    esp_err_t esp_now_del_peer(const uint8_t* mac);
    esp_err_t esp_now_send(const uint8_t* peer_addr, const uint8_t* data, size_t len);
}
