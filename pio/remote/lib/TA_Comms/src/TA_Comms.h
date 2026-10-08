#pragma once
#include <TA_Link.h>
#include <TA_Config.h>

namespace trailair {
namespace comms {

enum class PairEvent { Started, Acked, Timeout, Canceled, Busy };

typedef void (*StatusCallback)(void* ctx, const trailair::protocol::Status& status);
typedef void (*PairCallback)(void* ctx, PairEvent ev);

/**
 * @brief Remote side of the link: connection tracking, reconnect pings, pairing.
 *
 * Connected = a frame from the paired board within connectionTimeout. While paired but not
 * connected it pings with backoff (forever: the remote sleeps after inactivity anyway); while
 * connected it sends a keep-alive. Status frames from the paired board go to the status callback.
 */
class RemoteLink : public trailair::link::Link {
public:
  /// Radio up (also after sleep); starts reconnecting if paired
  bool begin();
  /// Radio down before sleep (cancels pairing)
  void end();
  /// Call every loop
  void service();

  bool sendButton(ButtonAction action, ButtonId button);

  bool isConnected() const { return heardFromPeerWithin(cfg_.connectionTimeoutMilliseconds); }

  bool startPairing();
  void cancelPairing();
  bool isPairing() const { return pairing_; }

  void setStatusCallback(StatusCallback cb, void* ctx) { statusCb_ = cb; statusCtx_ = ctx; }
  void setPairCallback(PairCallback cb, void* ctx) { pairCb_ = cb; pairCtx_ = ctx; }

protected:
  void onFrame_(const uint8_t mac[6], const trailair::protocol::Frame& f) override;

private:
  void stopPairing_(PairEvent ev);
  void restartPings_(uint32_t now);

  const trailair::config::CommunicationConfiguration cfg_{};

  // Pings
  bool wasConnected_ = false;
  uint32_t nextPingAt_ = 0;
  uint32_t pingBackoff_ = 0;

  // Pairing
  bool pairing_ = false;
  uint32_t pairingTimeoutAt_ = 0;
  uint32_t nextPairRequestAt_ = 0;
  bool busySeen_ = false;
  uint32_t busyGiveUpAt_ = 0;

  StatusCallback statusCb_ = nullptr;
  void* statusCtx_ = nullptr;
  PairCallback pairCb_ = nullptr;
  void* pairCtx_ = nullptr;
};

} // namespace comms
} // namespace trailair
