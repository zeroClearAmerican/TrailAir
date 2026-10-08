#pragma once
#include <TA_Link.h>

namespace trailair {
namespace comms {

/// A frame from the paired remote (button events and pings)
typedef void (*RemoteFrameCallback)(void* ctx, const trailair::protocol::Frame& frame);

/**
 * @brief Board side of the link: accepts pairing, answers Busy, sends status.
 *
 * Pairing: an unpaired board accepts the first PairRequest; a paired board re-acks its own
 * remote and answers anyone else with a broadcast Busy (the requester isn't a registered
 * ESP-NOW peer, so a unicast reply would be rejected). forget() frees it for a new remote.
 */
class BoardLink : public trailair::link::Link {
public:
  /// Dispatch received frames. Call every loop.
  void service() { poll(); }

  bool isPaired() const { return hasPeer(); }
  void forget();

  /// The paired remote sent something within timeoutMs
  bool isRemoteActive(uint32_t timeoutMs) const { return heardFromPeerWithin(timeoutMs); }

  bool sendStatus(const trailair::protocol::Status& status);

  void setRemoteFrameCallback(RemoteFrameCallback cb, void* ctx) { cb_ = cb; ctx_ = ctx; }

protected:
  void onFrame_(const uint8_t mac[6], const trailair::protocol::Frame& f) override;

private:
  RemoteFrameCallback cb_ = nullptr;
  void* ctx_ = nullptr;
};

} // namespace comms
} // namespace trailair
