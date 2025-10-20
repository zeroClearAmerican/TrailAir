#include "TA_RemoteApp.h"
#include <Arduino.h>
#include <WiFi.h>
#include <esp_wifi.h>
#include <esp_sleep.h>
#include <TA_DisplayIcons.h>

namespace ta { namespace app {

void RemoteApp::begin() {
  // Battery monitor
  batteryMon_.begin(pins_.batteryPin, ADC_11db);

  // Display
  if (ui_ && disp_) {
    const uint8_t SCREEN_ADDRESS = 0x3C;
    ui_->begin(SCREEN_ADDRESS, true);
  }

  // Buttons -> state
  buttons_.begin();
  buttons_.subscribe([](void* ctx, const ta::input::Event& e){
    auto* self = static_cast<RemoteApp*>(ctx);
    self->lastButtonPressedMs_ = millis();
    self->state_.onButton(e);
  }, this);

  // Wakeup setup
  setupWakeup_();

  // Link
  if (!link_.begin(nullptr)) {
    Serial.println("ESP-NOW init failed");
  }
  // Configure link from shared config defaults
  ta::state::Config scfg; // default values from ta::cfg
  link_.setConnectionTimeoutMs(scfg.link.connectionTimeoutMs);
  link_.setPingBackoffStartMs(scfg.link.pingBackoffStartMs);
  link_.setPairReqIntervalMs(scfg.link.pairReqIntervalMs);
  link_.setStatusCallback(&RemoteApp::onStatusStatic_, this);
  link_.setPairCallback(&RemoteApp::onPairEventStatic_, this);
  Serial.println("ESP-NOW initialized");

  state_.begin();

  delay(500);
  if (ui_) {
    ui_->logoWipe(ta::display::Icons::logo_bmp, ta::display::Icons::LogoW, ta::display::Icons::LogoH, false, 5);
  }
}

void RemoteApp::onStatusStatic_(void* ctx, const ta::protocol::Response& msg) {
  static_cast<RemoteApp*>(ctx)->onStatus_(msg);
}
void RemoteApp::onPairEventStatic_(void* ctx, ta::comms::PairEvent ev, const uint8_t mac[6]) {
  static_cast<RemoteApp*>(ctx)->onPairEvent_(ev, mac);
}

void RemoteApp::onStatus_(const ta::protocol::Response& msg) {
  state_.onStatus(msg);
}
void RemoteApp::onPairEvent_(ta::comms::PairEvent ev, const uint8_t mac[6]) {
  state_.onPairEvent(ev, mac);
}

void RemoteApp::setupWakeup_() {
  gpio_wakeup_enable(GPIO_NUM_10, GPIO_INTR_LOW_LEVEL);
  esp_err_t result = esp_sleep_enable_gpio_wakeup();
  if (result == ESP_OK) Serial.println("GPIO Wake-Up set successfully.");
  else Serial.println("Failed to set GPIO Wake-Up as wake-up source.");
}

void RemoteApp::goToSleep_() {
  Serial.println("Entering light sleep...");
  link_.sendCancel();
  if (ui_) {
    ui_->drawLogo(ta::display::Icons::logo_bmp, ta::display::Icons::LogoW, ta::display::Icons::LogoH);
    delay(1000);
    ui_->logoWipe(ta::display::Icons::logo_bmp, ta::display::Icons::LogoW, ta::display::Icons::LogoH, false, 5);
  }
  WiFi.disconnect();
  WiFi.mode(WIFI_OFF);
  esp_wifi_stop();
  esp_light_sleep_start();
  Serial.println("Woke up from sleep.");
  // Re-init after wake
  if (link_.hasPeer()) link_.requestReconnect();
  state_.resetAfterWake();
}

void RemoteApp::loop() {
  // Read buttons
  buttons_.service();
  if (millis() - lastButtonPressedMs_ > SLEEP_TIMEOUT_MS_) {
    Serial.println("Sleep timeout exceeded.");
    goToSleep_();
  }

  // Battery
  batteryMon_.update();
  state_.onBatteryPercent(batteryMon_.percent());

  // Link service
  link_.service();
  bool isConn = link_.isConnected();
  bool isConnIng = link_.isConnecting();

  // State update
  state_.update(millis(), isConn, isConnIng);
  if (state_.takeSleepRequest()) {
    goToSleep_();
  }

  // Render
  if (ui_) {
    ta::display::DisplayModel dm;
    state_.buildDisplayModel(dm);
    ui_->render(dm);
  }

  delay(10);
}

}} // namespace ta::app
