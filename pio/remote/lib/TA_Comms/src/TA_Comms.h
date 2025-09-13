#pragma once
#include <Arduino.h>
#include <WiFi.h>
#include <esp_wifi.h>
#include <esp_now.h>
#include <Preferences.h>
#include "TA_Protocol.h"

#ifndef TA_COMMS_DEBUG
#define TA_COMMS_DEBUG 1
#endif

namespace ta {
    namespace comms {

        enum class PairEvent {
            Started,
            Acked,
            Timeout,
            Canceled,
            Busy,
            Saved,
            Cleared
        };

        using ta::protocol::StatusMsg;

        typedef void (*StatusCallback)(void* ctx, const StatusMsg& msg);
        typedef void (*PairCallback)(void* ctx, PairEvent ev, const uint8_t mac[6]);

        class EspNowLink {
            public:
                EspNowLink();

                // Setup WIFI STA, init ESP-NOW, register peer and callbacks
                bool begin(const uint8_t peerMac[6]);

                // Send commands
                bool sendStart(float targetPsi);
                bool sendCancel();
                bool sendManual(uint8_t code);
                bool sendPing();

                // Reconnect ping logic with backoff (call service() in loop)
                void requestReconnect();
                void service();

                // Connection state (derived from lastSeen + timeout)
                void setConnectionTimeoutMs(uint32_t ms) { connectionTimeoutMs_ = ms; }
                bool isConnected() const { return isConnected_; }
                bool isConnecting() const { return isConnecting_; }
                uint32_t lastSeenMs() const { return lastSeenMs_; }

                // App callback when a valid status packet arrives
                void setStatusCallback(StatusCallback cb, void* ctx) {
                    cb_ = cb; cbCtx_ = ctx;
                }

                // Persistence
                bool loadPeerFromNVS();
                bool savePeerToNVS(const uint8_t mac[6]);
                bool clearPeerFromNVS();
                bool hasPeer() const { return hasPeer_; }

                // Pairing
                bool startPairing(uint8_t groupId = ta::protocol::kPairGroupId, uint32_t timeoutMs = 30000);
                void cancelPairing();
                bool isPairing() const { return pairing_; }
                void setPairCallback(PairCallback cb, void* ctx) { pairCb_ = cb; pairCtx_ = ctx; }

            private:
                // esp-now callbacks (static trampolines)
                static void onRecvStatic(const uint8_t* mac, const uint8_t* data, int len);
                static void onSentStatic(const uint8_t* mac, esp_now_send_status_t status);
                void onRecv(const uint8_t* mac, const uint8_t* data, int len);
                void onSent(const uint8_t* mac, esp_now_send_status_t status);

                bool ensurePeer_();
                bool sendRaw_(const uint8_t payload[ta::protocol::kPayloadLen]);

                void emitPairEvent_(PairEvent ev, const uint8_t mac[6]);

                void handlePairFrame_(const uint8_t* mac, const ta::protocol::PairMsg& pm);
                void stopPairing_(PairEvent finalEv, const uint8_t* mac);

                bool sendPairReq_();
                void ensureBroadcastPeer_();

            private:
                static EspNowLink* s_instance_;

                uint8_t peer_[6] = {0};
                bool inited_ = false;

                // Connection tracking
                volatile uint32_t lastSeenMs_ = 0;
                uint32_t connectionTimeoutMs_ = 5000; // ms
                bool isConnected_ = false;
                bool isConnecting_ = false;

                // Reconnect backoff
                uint32_t nextPingAtMs_ = 0;
                uint32_t pingBackoffMs_ = 200;
                const uint32_t pingBackoffMaxMs_ = 2000;

                // Persistence
                Preferences prefs_;
                bool hasPeer_ = false;

                // Pairing
                bool pairing_ = false;
                uint32_t pairingTimeoutAt_ = 0;
                uint32_t nextPairReqAt_ = 0;
                uint32_t pairReqIntervalMs_ = 500;
                uint8_t pairingGroupId_ = ta::protocol::kPairGroupId;

                // Callback
                StatusCallback cb_ = nullptr;
                void* cbCtx_ = nullptr;

                PairCallback pairCb_ = nullptr;
                void* pairCtx_ = nullptr;
        };

    } // namespace comms
} // namespace ta