#include "TA_CommsBoard.h"
#include <Arduino.h>

using namespace trailair::comms;

// Static instance used by C callbacks to reach the current object
BoardLink* BoardLink::inst_ = nullptr;

bool BoardLink::begin() {
  Serial.println("    [BoardLink] Starting ESP-NOW initialization...");
  inst_ = this;
  
  Serial.println("    [BoardLink] Setting WiFi mode to STA...");
  WiFi.mode(WIFI_STA);
  delay(100); // Give WiFi mode time to stabilize
  WiFi.disconnect();
  
  Serial.println("    [BoardLink] Initializing ESP-NOW...");
  esp_err_t init_result = esp_now_init();
  if (init_result != ESP_OK) {
    Serial.printf("    [BoardLink] ESP-NOW init failed with error: 0x%X\n", init_result);
    return false;
  }
  Serial.println("    [BoardLink] ESP-NOW initialized successfully");
  
  // Bind instance methods via lambdas capturing no state (function pointer compatible)
  esp_now_register_recv_cb([](const uint8_t* mac, const uint8_t* data, int len){ if (inst_) inst_->onRecv(mac, data, len); });
  esp_now_register_send_cb([](const uint8_t* /*mac*/, esp_now_send_status_t /*status*/){ /* no-op */ });

  Serial.println("    [BoardLink] Loading paired peer from NVS...");
  loadPeer_();
  if (paired_) {
    ensurePeer_(peer_);
    Serial.printf("    [BoardLink] Paired remote loaded: %02X:%02X:%02X:%02X:%02X:%02X\n",
      peer_[0],peer_[1],peer_[2],peer_[3],peer_[4],peer_[5]);
  } else {
    Serial.println("    [BoardLink] Unpaired. Waiting for PairReq...");
  }
  Serial.println("    [BoardLink] Ready");
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
         : trailair::protocol::convertPSIToByte(psi);
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
  Serial.printf("    [BoardLink] Received PairReq from %02X:%02X:%02X:%02X:%02X:%02X, group=0x%02X\n",
                mac[0], mac[1], mac[2], mac[3], mac[4], mac[5], group);
  
  if (group != groupId_) {
    Serial.printf("    [BoardLink] PairReq wrong group (expected 0x%02X)\n", groupId_);
    return;
  }
  if (!paired_) {
    Serial.println("    [BoardLink] Not paired - accepting pairing request");
    savePeer_(mac);
    ensurePeer_(mac);
    uint8_t ack[2]; trailair::protocol::packPairingAcknowledge(ack, groupId_);
    esp_now_send(peer_, ack, 2);
    Serial.println("    [BoardLink] Paired (saved); Ack sent.");
  } else {
    if (memcmp(mac, peer_, 6) == 0) {
      uint8_t ack[2]; trailair::protocol::packPairingAcknowledge(ack, groupId_);
      esp_now_send(peer_, ack, 2);
      Serial.println("    [BoardLink] Re-Ack existing peer");
    } else {
      Serial.printf("    [BoardLink] Busy: already paired to %02X:%02X:%02X:%02X:%02X:%02X\n",
                    peer_[0], peer_[1], peer_[2], peer_[3], peer_[4], peer_[5]);
      uint8_t busy[2]; trailair::protocol::packPairingBusy(busy, 1);
      esp_now_send(mac, busy, 2);
      Serial.println("    [BoardLink] Busy response sent.");
    }
  }
}

void BoardLink::onRecv(const uint8_t* mac, const uint8_t* data, int len) {
  using namespace trailair::protocol;
  
  // Check for pairing frames
  if (len == 2 && isPairingFrame(data, len)) {
    PairingMessage pm;
    if (parsePairingMessage(data, len, pm) && pm.operation == PairingOperation::Request) {
      handlePairReq_(mac, pm.value);
    }
    return;
  }
  
  // Only accept data from paired peer
  if (!paired_ || memcmp(mac, peer_, 6) != 0) {
    if (!paired_) {
      Serial.println("    [BoardLink] Ignoring data: not paired");
    } else {
      Serial.printf("    [BoardLink] Ignoring data from non-paired MAC: %02X:%02X:%02X:%02X:%02X:%02X\n",
                    mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
    }
    return;
  }
  
  if (len != 2) return;

  Request req;
  if (!parseRequest(data, len, req)) return;

  // Write lastRxMs_ atomically
  portENTER_CRITICAL(&isrMux_);
  lastRxMs_ = millis();
  portEXIT_CRITICAL(&isrMux_);

  if (reqCb_) reqCb_(reqCtx_, req);
}