#include "TA_CommsBoard.h"

namespace trailair {
namespace comms {

using trailair::protocol::Frame;
using trailair::protocol::FrameType;

void BoardLink::forget() {
  clearPeer();
  Serial.println("[BoardLink] Remote forgotten; accepting pair requests");
}

bool BoardLink::sendStatus(const trailair::protocol::Status& status) {
  Frame f;
  f.type = FrameType::Status;
  f.status = status;
  return sendToPeer(f);
}

void BoardLink::onFrame_(const uint8_t mac[6], const Frame& f) {
  if (f.type == FrameType::PairRequest) {
    Frame reply;
    if (!isPaired() || isPeer(mac)) {
      if (!isPaired()) {
        setPeer(mac);
        Serial.printf("[BoardLink] Paired %02X:%02X:%02X:%02X:%02X:%02X\n",
                      mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
      }
      reply.type = FrameType::PairAck;
      sendToPeer(reply);
    } else {
      reply.type = FrameType::PairBusy;
      broadcast(reply);
    }
    return;
  }

  // Everything else only counts from our remote
  if (isPeer(mac) && cb_) cb_(ctx_, f);
}

} // namespace comms
} // namespace trailair
