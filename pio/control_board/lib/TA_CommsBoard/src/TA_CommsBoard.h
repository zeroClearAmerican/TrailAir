#pragma once
#include <Arduino.h>
#include <WiFi.h>
#include <esp_wifi.h>
#include <esp_now.h>
#include <Preferences.h>
#include "TA_Protocol.h"
#include "TA_Time.h"  // Overflow-safe time utilities

namespace ta {
namespace comms {

using ta::protocol::Request;

typedef void (*RequestCallback)(void* ctx, const Request& req);

class BoardLink {
public:
  bool begin();
  void service(); // no-op now (placeholder)

  // Pairing / persistence
  bool isPaired() const { return paired_; }
  void forget();

  // Status
  bool sendStatus(char statusChar, float psi);
  bool sendError(uint8_t errorCode);

  // Registration
  void setRequestCallback(RequestCallback cb, void* ctx) { reqCb_ = cb; reqCtx_ = ctx; }

  // Returns true if a remote is paired AND has sent something recently.
  bool isRemoteActive(uint32_t timeoutMs = 3000) const {
    if (!paired_) return false;
    
    // Read lastRxMs_ atomically
    uint32_t lastRx;
    portENTER_CRITICAL(&isrMux_);
    lastRx = lastRxMs_;
    portEXIT_CRITICAL(&isrMux_);
    
    if (lastRx == 0) return false;
    return ta::time::hasElapsed(millis(), lastRx, 0) && !ta::time::hasElapsed(millis(), lastRx, timeoutMs);
  }

private:
  static void onRecvStatic(const uint8_t* mac, const uint8_t* data, int len);
  static void onSentStatic(const uint8_t* mac, esp_now_send_status_t status);
  void onRecv(const uint8_t* mac, const uint8_t* data, int len);

  bool loadPeer_();
  bool savePeer_(const uint8_t mac[6]);
  bool clearPeer_();
  void handlePairReq_(const uint8_t* mac, uint8_t group);

  void ensurePeer_(const uint8_t mac[6]);

  Preferences prefs_;
  bool paired_ = false;
  uint8_t peer_[6] = {0};
  uint8_t groupId_ = 0x01;

  RequestCallback reqCb_ = nullptr;
  void* reqCtx_ = nullptr;

  volatile uint32_t lastRxMs_ = 0; // millis() of last valid packet from remote
  portMUX_TYPE isrMux_ = portMUX_INITIALIZER_UNLOCKED; // Mutex for ISR safety

  static BoardLink* inst_;
};

} // namespace comms
} // namespace ta