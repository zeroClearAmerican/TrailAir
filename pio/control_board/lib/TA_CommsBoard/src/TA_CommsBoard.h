#pragma once
#include <Arduino.h>
#include <WiFi.h>
#include <esp_wifi.h>
#include <esp_now.h>
#include <Preferences.h>
#include "TA_Protocol.h"

namespace ta {
namespace comms {

enum class CmdType { Idle, Seek, Manual, Ping, Unknown };

struct Command {
  CmdType type = CmdType::Unknown;
  float targetPsi = 0;
  uint8_t raw = 0;
};

typedef void (*CommandCallback)(void* ctx, const Command& cmd);

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
  void setCommandCallback(CommandCallback cb, void* ctx) { cmdCb_ = cb; cmdCtx_ = ctx; }

  // Returns true if a remote is paired AND has sent something recently.
  bool isRemoteActive(uint32_t timeoutMs = 3000) const {
    if (!paired_ || lastRxMs_ == 0) return false;
    return (millis() - lastRxMs_) < timeoutMs;
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
  uint8_t groupId_ = ta::protocol::kPairGroupId;

  CommandCallback cmdCb_ = nullptr;
  void* cmdCtx_ = nullptr;

  uint32_t lastRxMs_ = 0; // millis() of last valid packet from remote

  static BoardLink* inst_;
};

} // namespace comms
} // namespace ta