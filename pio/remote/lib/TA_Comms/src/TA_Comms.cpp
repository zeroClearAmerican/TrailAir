#include "TA_Comms.h"
#include <TA_Time.h>

namespace trailair {
namespace comms {

using trailair::protocol::Frame;
using trailair::protocol::FrameType;
using trailair::time::getMilliseconds;
using trailair::time::isTimeFor;
using trailair::time::calculateFutureTime;

namespace {
  Frame simpleFrame(FrameType type) {
    Frame f;
    f.type = type;
    return f;
  }
}

bool RemoteLink::begin() {
  pairing_ = false;
  wasConnected_ = false;
  if (!Link::begin()) return false;
  restartPings_(getMilliseconds());
  return true;
}

void RemoteLink::end() {
  pairing_ = false;
  Link::end();
}

void RemoteLink::restartPings_(uint32_t now) {
  pingBackoff_ = cfg_.pingBackoffStartMilliseconds;
  nextPingAt_ = now;  // ping immediately
}

bool RemoteLink::sendButton(ButtonAction action, ButtonId button) {
  Frame f;
  f.type = trailair::protocol::buttonFrameType(action);
  f.button = button;
  return sendToPeer(f);
}

void RemoteLink::service() {
  poll();
  uint32_t now = getMilliseconds();

  if (pairing_) {
    if (busySeen_ && isTimeFor(now, busyGiveUpAt_)) {
      stopPairing_(PairEvent::Busy);
    } else if (isTimeFor(now, pairingTimeoutAt_)) {
      stopPairing_(PairEvent::Timeout);
    } else if (isTimeFor(now, nextPairRequestAt_)) {
      broadcast(simpleFrame(FrameType::PairRequest));
      nextPairRequestAt_ = calculateFutureTime(now, cfg_.pairingRequestIntervalMilliseconds);
    }
    return;  // no pings while pairing
  }

  if (!hasPeer() || !isReady()) return;

  bool connected = isConnected();
  if (connected != wasConnected_) {
    wasConnected_ = connected;
    if (connected) nextPingAt_ = calculateFutureTime(now, cfg_.keepAliveIntervalMilliseconds);
    else restartPings_(now);  // lost link: fall back to fast reconnect pings
  }

  if (isTimeFor(now, nextPingAt_)) {
    sendToPeer(simpleFrame(FrameType::Ping));
    if (connected) {
      nextPingAt_ = calculateFutureTime(now, cfg_.keepAliveIntervalMilliseconds);
    } else {
      nextPingAt_ = calculateFutureTime(now, pingBackoff_);
      pingBackoff_ = min(pingBackoff_ * 2, cfg_.pingBackoffMaximumMilliseconds);
    }
  }
}

void RemoteLink::onFrame_(const uint8_t mac[6], const Frame& f) {
  switch (f.type) {
    case FrameType::Status:
      if (isPeer(mac) && statusCb_) statusCb_(statusCtx_, f.status);  // only our board's status counts
      break;

    case FrameType::PairAck:
      if (!pairing_) break;
      setPeer(mac);  // replaces any previous board, so it can't keep feeding us status
      wasConnected_ = false;
      restartPings_(getMilliseconds());
      stopPairing_(PairEvent::Acked);
      break;

    case FrameType::PairBusy:
      // A nearby board paired to someone else. Keep asking briefly in case a free board answers.
      if (pairing_ && !busySeen_) {
        busySeen_ = true;
        busyGiveUpAt_ = calculateFutureTime(getMilliseconds(), cfg_.pairingBusyGraceMilliseconds);
      }
      break;

    default:
      break;  // board-bound frames
  }
}

bool RemoteLink::startPairing() {
  if (pairing_ || !isReady()) return false;
  uint32_t now = getMilliseconds();
  pairing_ = true;
  busySeen_ = false;
  pairingTimeoutAt_ = calculateFutureTime(now, cfg_.pairingTimeoutMilliseconds);
  nextPairRequestAt_ = now;
  if (pairCb_) pairCb_(pairCtx_, PairEvent::Started);
  return true;
}

void RemoteLink::cancelPairing() {
  if (pairing_) stopPairing_(PairEvent::Canceled);
}

void RemoteLink::stopPairing_(PairEvent ev) {
  pairing_ = false;
  if (pairCb_) pairCb_(pairCtx_, ev);
}

} // namespace comms
} // namespace trailair
