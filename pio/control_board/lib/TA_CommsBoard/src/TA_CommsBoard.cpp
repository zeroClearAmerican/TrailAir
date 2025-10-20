#include "TA_CommsBoard.h"
#include <Arduino.h>

using namespace ta::comms;

// Static instance used by C callbacks to reach the current object
BoardLink* BoardLink::inst_ = nullptr;

bool BoardLink::begin() {
  inst_ = this;
  WiFi.mode(WIFI_STA);
  WiFi.disconnect();
  if (esp_now_init() != ESP_OK) {
    Serial.println("ESP-NOW init failed");
    return false;
  }
  // Bind instance methods via lambdas capturing no state (function pointer compatible)
  esp_now_register_recv_cb([](const uint8_t* mac, const uint8_t* data, int len){ if (inst_) inst_->onRecv(mac, data, len); });
  esp_now_register_send_cb([](const uint8_t* /*mac*/, esp_now_send_status_t /*status*/){ /* no-op */ });

  loadPeer_();
  if (paired_) {
    ensurePeer_(peer_);
    Serial.printf("Paired remote loaded: %02X:%02X:%02X:%02X:%02X:%02X\n",
      peer_[0],peer_[1],peer_[2],peer_[3],peer_[4],peer_[5]);
  } else {
    Serial.println("Unpaired. Waiting for PairReq...");
  }
  return true;
}

void BoardLink::service() {
  // (Reserved for future timers)
}

bool BoardLink::loadPeer_() {
  if (!prefs_.begin("trailair", true)) return false;
  size_t len = prefs_.getBytesLength("peer");
  if (len == 6) {
    prefs_.getBytes("peer", peer_, 6);
    paired_ = true;
  }
  prefs_.end();
  return paired_;
}

bool BoardLink::savePeer_(const uint8_t mac[6]) {
  if (!prefs_.begin("trailair", false)) return false;
  bool ok = prefs_.putBytes("peer", mac, 6) == 6;
  prefs_.end();
  if (ok) {
    memcpy(peer_, mac, 6);
    paired_ = true;
  }
  return ok;
}

bool BoardLink::clearPeer_() {
  if (!prefs_.begin("trailair", false)) return false;
  bool ok = prefs_.remove("peer");
  prefs_.end();
  if (ok) {
    paired_ = false;
    memset(peer_, 0, 6);
  }
  return ok;
}

void BoardLink::forget() {
  clearPeer_();
  Serial.println("Peer cleared. Awaiting PairReq.");
}

void BoardLink::ensurePeer_(const uint8_t mac[6]) {
  if (esp_now_is_peer_exist(mac)) return;
  esp_now_peer_info_t pi{};
  memcpy(pi.peer_addr, mac, 6);
  pi.channel = 0;
  pi.encrypt = false;
  esp_now_add_peer(&pi);
}

bool BoardLink::sendStatus(char statusChar, float psi) {
  if (!paired_) return false;
  uint8_t p[2];
  p[0] = (uint8_t)statusChar;
  p[1] = (statusChar == 'E')
         ? psi  // psi holds error code when E
         : ta::protocol::psiToByte05(psi);
  return esp_now_send(peer_, p, 2) == ESP_OK;
}

bool BoardLink::sendError(uint8_t errorCode) {
  if (!paired_) return false;
  uint8_t p[2];
  p[0] = 'E';
  p[1] = errorCode;
  return esp_now_send(peer_, p, 2) == ESP_OK;
}

void BoardLink::handlePairReq_(const uint8_t* mac, uint8_t group) {
  if (group != groupId_) {
    Serial.println("PairReq wrong group");
    return;
  }
  if (!paired_) {
    savePeer_(mac);
    ensurePeer_(mac);
    uint8_t ack[2]; ta::protocol::packPairAck(ack, groupId_);
    esp_now_send(peer_, ack, 2);
    Serial.println("Paired (saved); Ack sent.");
  } else {
    if (memcmp(mac, peer_, 6) == 0) {
      uint8_t ack[2]; ta::protocol::packPairAck(ack, groupId_);
      esp_now_send(peer_, ack, 2);
      Serial.println("Re-Ack existing peer");
    } else {
      uint8_t busy[2]; ta::protocol::packPairBusy(busy, 1);
      esp_now_send(mac, busy, 2);
      Serial.println("Busy: already paired.");
    }
  }
}

void BoardLink::onRecv(const uint8_t* mac, const uint8_t* data, int len) {
  using namespace ta::protocol;
  if (len == 2 && isPairingFrame(data, len)) {
    PairMsg pm;
    if (parsePair(data, len, pm) && pm.op == PairOp::Req) {
      handlePairReq_(mac, pm.value);
    }
    return;
  }
  if (!paired_ || memcmp(mac, peer_, 6) != 0) return;
  if (len != 2) return;

  Request req;
  if (!parseRequest(data, len, req)) return;

  lastRxMs_ = millis(); // mark remote activity

  if (reqCb_) reqCb_(reqCtx_, req);
}