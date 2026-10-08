#pragma once
#include <Arduino.h>
#include <WiFi.h>
#include <esp_now.h>
#include <Preferences.h>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include <TA_Protocol.h>

namespace trailair {
namespace link {

/**
 * @brief ESP-NOW plumbing shared by the remote and the board.
 *
 * Owns: radio up/down, the single paired peer (registered with ESP-NOW and persisted in NVS),
 * frame validation, and a receive queue. ESP-NOW's receive callback runs on the WiFi task, so
 * it only enqueues; poll() parses and dispatches frames to onFrame_() on the main loop, which
 * keeps every role-level handler single-threaded.
 *
 * Each device derives and implements its role in onFrame_() (remote: pings, pairing, status;
 * board: requests, pair accept/busy, status replies).
 */
class Link {
public:
  virtual ~Link() = default;

  /// Radio on, ESP-NOW up, persisted peer restored. Safe to call again after end().
  bool begin();
  /// ESP-NOW down and radio off (before sleep). The peer stays known; begin() re-registers it.
  void end();
  /// Dispatch queued frames to onFrame_(). Call every loop.
  void poll();

  bool hasPeer() const { return hasPeer_; }
  /// True if a valid frame from the peer arrived within the last timeoutMs
  bool heardFromPeerWithin(uint32_t timeoutMs) const;

protected:
  bool sendToPeer(const protocol::Frame& f);
  bool broadcast(const protocol::Frame& f);
  /// Pair with mac: register with ESP-NOW and persist. RAM state is set even if NVS fails
  /// (the pairing then just won't survive a reboot).
  void setPeer(const uint8_t mac[6]);
  void clearPeer();
  bool isPeer(const uint8_t mac[6]) const { return hasPeer_ && memcmp(mac, peer_, 6) == 0; }
  bool isReady() const { return ready_; }

  /// A validated frame arrived (main loop). heardFromPeerWithin() is already updated.
  virtual void onFrame_(const uint8_t mac[6], const protocol::Frame& f) = 0;

private:
  struct RxFrame { uint8_t mac[6]; uint8_t data[protocol::MAX_FRAME_LENGTH]; uint8_t len; };

  static void onRecvStatic_(const uint8_t* mac, const uint8_t* data, int len);
  bool addEspNowPeer_(const uint8_t mac[6]);
  bool send_(const uint8_t mac[6], const protocol::Frame& f);

  static Link* instance_;  // ponytail: one Link per device; ESP-NOW callbacks carry no context

  QueueHandle_t rxQueue_ = nullptr;
  Preferences prefs_;
  uint8_t peer_[6] = {0};
  bool hasPeer_ = false;
  bool ready_ = false;
  bool heardFromPeer_ = false;
  uint32_t lastPeerRxMs_ = 0;
};

} // namespace link
} // namespace trailair
