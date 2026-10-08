#include "TA_Link.h"

namespace trailair {
namespace link {

Link* Link::instance_ = nullptr;

static const char* kPrefsNs  = "trailair";
static const char* kPrefsKey = "peer";
static const uint8_t kBroadcast[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};

bool Link::begin() {
  instance_ = this;
  if (!rxQueue_) rxQueue_ = xQueueCreate(8, sizeof(RxFrame));

  WiFi.mode(WIFI_STA);
  WiFi.disconnect();
  if (esp_now_init() != ESP_OK) {
    Serial.println("[Link] ESP-NOW init failed");
    return false;
  }
  esp_now_register_recv_cb(&Link::onRecvStatic_);
  xQueueReset(rxQueue_);
  heardFromPeer_ = false;

  if (prefs_.begin(kPrefsNs, true)) {
    if (prefs_.getBytesLength(kPrefsKey) == 6) {
      prefs_.getBytes(kPrefsKey, peer_, 6);
      hasPeer_ = true;
    }
    prefs_.end();
  }
  if (hasPeer_) {
    addEspNowPeer_(peer_);
    Serial.printf("[Link] Peer %02X:%02X:%02X:%02X:%02X:%02X\n",
                  peer_[0], peer_[1], peer_[2], peer_[3], peer_[4], peer_[5]);
  }
  addEspNowPeer_(kBroadcast);

  ready_ = true;
  return true;
}

void Link::end() {
  if (ready_) esp_now_deinit();  // also drops the ESP-NOW peer list; begin() re-adds
  ready_ = false;
  heardFromPeer_ = false;
  WiFi.disconnect();
  WiFi.mode(WIFI_OFF);
}

void Link::poll() {
  RxFrame rx;
  while (rxQueue_ && xQueueReceive(rxQueue_, &rx, 0) == pdTRUE) {
    protocol::Frame f;
    if (!protocol::parse(rx.data, rx.len, f)) continue;
    if (isPeer(rx.mac)) {
      heardFromPeer_ = true;
      lastPeerRxMs_ = millis();
    }
    onFrame_(rx.mac, f);
  }
}

bool Link::heardFromPeerWithin(uint32_t timeoutMs) const {
  return hasPeer_ && heardFromPeer_ && (millis() - lastPeerRxMs_) < timeoutMs;
}

bool Link::sendToPeer(const protocol::Frame& f) {
  return hasPeer_ && send_(peer_, f);
}

bool Link::broadcast(const protocol::Frame& f) {
  return send_(kBroadcast, f);
}

void Link::setPeer(const uint8_t mac[6]) {
  if (hasPeer_ && memcmp(peer_, mac, 6) != 0 && ready_) esp_now_del_peer(peer_);
  memcpy(peer_, mac, 6);
  hasPeer_ = true;
  heardFromPeer_ = false;
  if (ready_) addEspNowPeer_(peer_);
  if (prefs_.begin(kPrefsNs, false)) {
    prefs_.putBytes(kPrefsKey, mac, 6);
    prefs_.end();
  }
}

void Link::clearPeer() {
  if (hasPeer_ && ready_) esp_now_del_peer(peer_);
  memset(peer_, 0, 6);
  hasPeer_ = false;
  heardFromPeer_ = false;
  if (prefs_.begin(kPrefsNs, false)) {
    prefs_.remove(kPrefsKey);
    prefs_.end();
  }
}

bool Link::addEspNowPeer_(const uint8_t mac[6]) {
  if (esp_now_is_peer_exist(mac)) return true;
  esp_now_peer_info_t info = {};
  memcpy(info.peer_addr, mac, 6);
  info.channel = 0;
  info.encrypt = false;
  return esp_now_add_peer(&info) == ESP_OK;
}

bool Link::send_(const uint8_t mac[6], const protocol::Frame& f) {
  if (!ready_) return false;
  uint8_t buf[protocol::MAX_FRAME_LENGTH];
  int len = protocol::pack(buf, f);
  return len > 0 && esp_now_send(mac, buf, len) == ESP_OK;
}

// WiFi task: copy and enqueue only. Dropped if the queue is full (status repeats; pairing retries).
void Link::onRecvStatic_(const uint8_t* mac, const uint8_t* data, int len) {
  Link* self = instance_;
  if (!self || !self->rxQueue_ || len < 2 || len > protocol::MAX_FRAME_LENGTH) return;
  RxFrame rx;
  memcpy(rx.mac, mac, 6);
  memcpy(rx.data, data, len);
  rx.len = static_cast<uint8_t>(len);
  xQueueSend(self->rxQueue_, &rx, 0);
}

} // namespace link
} // namespace trailair
