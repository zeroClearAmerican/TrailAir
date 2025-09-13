#include "TA_Comms.h"

namespace ta {
    namespace comms {

        EspNowLink* EspNowLink::s_instance_ = nullptr;

        static const char* kPrefsNs  = "trailair";
        static const char* kPrefsKey = "peer";

        EspNowLink::EspNowLink() {
            s_instance_ = this;
        }

        bool EspNowLink::begin(const uint8_t peerMac[6]) {
            WiFi.mode(WIFI_STA);
            WiFi.disconnect();

            if (esp_now_init() != ESP_OK) {
        #if TA_COMMS_DEBUG
                Serial.println("ESP-NOW init failed");
        #endif
                return false;
            }

            esp_now_register_recv_cb(&EspNowLink::onRecvStatic);
            esp_now_register_send_cb(&EspNowLink::onSentStatic);

            inited_ = true;
            isConnected_ = false;
            isConnecting_ = false;
            lastSeenMs_ = 0;
            pingBackoffMs_ = 200;
            nextPingAtMs_ = 0;

            // Try persisted peer first
            loadPeerFromNVS();

            if (!hasPeer_ && peerMac) {
                memcpy(peer_, peerMac, 6);
                hasPeer_ = true;
            }

            if (hasPeer_) {
                if (!ensurePeer_()) {
        #if TA_COMMS_DEBUG
                    Serial.println("Failed to add peer");
        #endif
                    return false;
                }
        #if TA_COMMS_DEBUG
                Serial.printf("ESP-NOW peer ready %02X:%02X:%02X:%02X:%02X:%02X\n",
                    peer_[0],peer_[1],peer_[2],peer_[3],peer_[4],peer_[5]);
        #endif
            }

        #if TA_COMMS_DEBUG
            Serial.println("ESP-NOW initialized");
        #endif
            return true;
        }

        bool EspNowLink::ensurePeer_() {
            if (!hasPeer_) return false;
            if (esp_now_is_peer_exist(peer_)) return true;
            esp_now_peer_info_t peerInfo = {};
            memcpy(peerInfo.peer_addr, peer_, 6);
            peerInfo.channel = 0;
            peerInfo.encrypt = false;
            return esp_now_add_peer(&peerInfo) == ESP_OK;
        }

        bool EspNowLink::sendRaw_(const uint8_t payload[ta::protocol::kPayloadLen]) {
            if (!inited_) return false;
            if (!ensurePeer_()) return false;
            return esp_now_send(peer_, payload, ta::protocol::kPayloadLen) == ESP_OK;
        }

        bool EspNowLink::sendStart(float targetPsi) {
            uint8_t p[ta::protocol::kPayloadLen];
            ta::protocol::packStart(p, targetPsi);
            return sendRaw_(p);
        }
        bool EspNowLink::sendCancel() {
            uint8_t p[ta::protocol::kPayloadLen];
            ta::protocol::packCancel(p);
            return sendRaw_(p);
        }
        bool EspNowLink::sendManual(uint8_t code) {
            uint8_t p[ta::protocol::kPayloadLen];
            ta::protocol::packManual(p, code);
            return sendRaw_(p);
        }
        bool EspNowLink::sendPing() {
            uint8_t p[ta::protocol::kPayloadLen];
            ta::protocol::packPing(p);
            return sendRaw_(p);
        }

        void EspNowLink::requestReconnect() {
            if (isConnected_) return;
            isConnecting_ = true;
            pingBackoffMs_ = 200;
            nextPingAtMs_ = 0; // send immediately
        }

        // Emit helper
        void EspNowLink::emitPairEvent_(PairEvent ev, const uint8_t mac[6]) {
            #if TA_COMMS_DEBUG
            const char* n = "";
            switch (ev) {
                case PairEvent::Started: n="Started"; break;
                case PairEvent::Acked: n="Acked"; break;
                case PairEvent::Timeout: n="Timeout"; break;
                case PairEvent::Canceled: n="Canceled"; break;
                case PairEvent::Busy: n="Busy"; break;
                case PairEvent::Saved: n="Saved"; break;
                case PairEvent::Cleared: n="Cleared"; break;
            }
            Serial.printf("[PAIR] %s\n", n);
            #endif
            if (pairCb_) pairCb_(pairCtx_, ev, mac);
        }

        bool EspNowLink::loadPeerFromNVS() {
            if (!prefs_.begin(kPrefsNs, true)) return false;
            size_t len = prefs_.getBytesLength(kPrefsKey);
            if (len == 6) {
                uint8_t mac[6];
                prefs_.getBytes(kPrefsKey, mac, 6);
                prefs_.end();
                if (esp_now_is_peer_exist(mac)) {
                memcpy(peer_, mac, 6);
                hasPeer_ = true;
                return true;
                }
                // (re)add peer
                esp_now_peer_info_t pi = {};
                memcpy(pi.peer_addr, mac, 6);
                pi.channel = 0;
                pi.encrypt = false;
                if (esp_now_add_peer(&pi) == ESP_OK) {
                memcpy(peer_, mac, 6);
                hasPeer_ = true;
                return true;
                }
            } else {
                prefs_.end();
            }
            return false;
        }

        bool EspNowLink::savePeerToNVS(const uint8_t mac[6]) {
            if (!mac) return false;
            if (!prefs_.begin(kPrefsNs, false)) return false;
            bool ok = prefs_.putBytes(kPrefsKey, mac, 6) == 6;
            prefs_.end();
            if (ok) {
                hasPeer_ = true;
                memcpy(peer_, mac, 6);
                emitPairEvent_(PairEvent::Saved, mac);
            }
            return ok;
        }

        bool EspNowLink::clearPeerFromNVS() {
            if (!prefs_.begin(kPrefsNs, false)) return false;
            bool ok = prefs_.remove(kPrefsKey);
            prefs_.end();
            if (ok) {
                if (hasPeer_) {
                    esp_now_del_peer(peer_);
                }
                hasPeer_ = false;
                uint8_t zero[6] = {0};
                emitPairEvent_(PairEvent::Cleared, zero);
            }
            return ok;
        }

        void EspNowLink::ensureBroadcastPeer_() {
            uint8_t bcast[6] = {0xFF,0xFF,0xFF,0xFF,0xFF,0xFF};
            if (!esp_now_is_peer_exist(bcast)) {
                esp_now_peer_info_t pi = {};
                memcpy(pi.peer_addr, bcast, 6);
                pi.channel = 0;
                pi.encrypt = false;
                esp_now_add_peer(&pi);
            }
        }

        bool EspNowLink::startPairing(uint8_t groupId, uint32_t timeoutMs) {
            if (pairing_) return false;
            pairing_ = true;
            pairingGroupId_ = groupId;
            pairingTimeoutAt_ = millis() + timeoutMs;
            nextPairReqAt_ = 0;
            pairReqIntervalMs_ = 500;
            ensureBroadcastPeer_();
            emitPairEvent_(PairEvent::Started, peer_);
            return true;
        }

        void EspNowLink::cancelPairing() {
            if (!pairing_) return;
            pairing_ = false;
            emitPairEvent_(PairEvent::Canceled, peer_);
        }

        void EspNowLink::stopPairing_(PairEvent finalEv, const uint8_t* mac) {
            pairing_ = false;
            emitPairEvent_(finalEv, mac ? mac : peer_);
        }

        bool EspNowLink::sendPairReq_() {
            uint8_t p[ta::protocol::kPayloadLen];
            ta::protocol::packPairReq(p, pairingGroupId_);
            uint8_t bcast[6] = {0xFF,0xFF,0xFF,0xFF,0xFF,0xFF};
            return esp_now_send(bcast, p, ta::protocol::kPayloadLen) == ESP_OK;
        }

        void EspNowLink::handlePairFrame_(const uint8_t* mac, const ta::protocol::PairMsg& pm) {
            using namespace ta::protocol;
            if (!pairing_) return;

            switch (pm.op) {
                case PairOp::Ack:
                    if (pm.value == pairingGroupId_) {
                        stopPairing_(PairEvent::Acked, mac);   // Acked first
                        savePeerToNVS(mac);                    // then Saved event
                        // add peer if needed
                        if (!esp_now_is_peer_exist(mac)) {
                            esp_now_peer_info_t pi = {};
                            memcpy(pi.peer_addr, mac, 6);
                            pi.channel = 0;
                            pi.encrypt = false;
                            esp_now_add_peer(&pi);
                        }
                        memcpy(peer_, mac, 6);
                        hasPeer_ = true;
                        requestReconnect(); // start normal connection attempts
                    }
                    break;
                case PairOp::Busy:
                    stopPairing_(PairEvent::Busy, mac);
                    break;
                default:
                    break;
            }
        }

        void EspNowLink::service() {
            uint32_t now = millis();

            // Skip ping logic while pairing (optional)
            if (!pairing_) {
                if (isConnected_ && (now - lastSeenMs_) > connectionTimeoutMs_) {
                    isConnected_ = false;
        #if TA_COMMS_DEBUG
                    Serial.println("Connection lost.");
        #endif
                }
                if (isConnecting_ && !isConnected_) {
                    if (now >= nextPingAtMs_) {
                        sendPing();
                        nextPingAtMs_ = now + pingBackoffMs_;
                        pingBackoffMs_ = min(pingBackoffMs_ * 2, pingBackoffMaxMs_);
                    }
                }
            }

            if (pairing_) {
                if (now >= pairingTimeoutAt_) {
                    stopPairing_(PairEvent::Timeout, peer_);
                } else if (now >= nextPairReqAt_) {
                    sendPairReq_();
                    nextPairReqAt_ = now + pairReqIntervalMs_;
                }
            }
        }

        void EspNowLink::onRecvStatic(const uint8_t* mac, const uint8_t* data, int len) {
            if (s_instance_) s_instance_->onRecv(mac, data, len);
        }
        void EspNowLink::onSentStatic(const uint8_t* mac, esp_now_send_status_t status) {
            if (s_instance_) s_instance_->onSent(mac, status);
        }

        void EspNowLink::onRecv(const uint8_t* mac, const uint8_t* data, int len) {
          using namespace ta::protocol;

          // Pairing frames
          if (isPairingFrame(data, len)) {
            PairMsg pm;
            if (parsePair(data, len, pm)) {
              handlePairFrame_(mac, pm);
              return;
            }
          }

          // Normal status
          StatusMsg sm;
          if (!parseStatus(data, len, sm)) return;

          lastSeenMs_ = millis();
          isConnected_ = true;
          isConnecting_ = false;

          if (cb_) cb_(cbCtx_, sm);
        }

        void EspNowLink::onSent(const uint8_t* /*mac*/, esp_now_send_status_t status) {
            #if TA_COMMS_DEBUG
            Serial.printf("Last Packet Send Status: %s\n", status == ESP_NOW_SEND_SUCCESS ? "Success" : "Fail");
            #endif
        }

    } // namespace comms
} // namespace ta