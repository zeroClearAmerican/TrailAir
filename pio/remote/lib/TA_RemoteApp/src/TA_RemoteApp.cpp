#include "TA_RemoteApp.h"
#include <Arduino.h>
#include <WiFi.h>
#include <esp_wifi.h>
#include <esp_sleep.h>
#include <TA_DisplayIcons.h>
#include <TA_Config.h>
#include <TA_Time.h>  // Overflow-safe time utilities

namespace trailair { namespace app {

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
  buttons_.subscribe([](void* ctx, const trailair::input::ButtonEvent& e){
    auto* self = static_cast<RemoteApp*>(ctx);
    self->lastButtonPressedMs_ = trailair::time::getMilliseconds();
    self->state_.onButton(e);
  }, this);

  // Wakeup setup
  setupWakeup_();

  // Link
  if (!link_.begin(nullptr)) {
    Serial.println("ESP-NOW init failed");
  }
  // Configure link from shared config defaults
  const trailair::config::CommunicationConfiguration linkCfg{};
  link_.setConnectionTimeoutMs(linkCfg.connectionTimeoutMilliseconds);
  link_.setPingBackoffStartMs(linkCfg.pingBackoffStartMilliseconds);
  link_.setPairReqIntervalMs(linkCfg.pairingRequestIntervalMilliseconds);
  link_.setStatusCallback(&RemoteApp::onStatusStatic_, this);
  link_.setPairCallback(&RemoteApp::onPairEventStatic_, this);
  Serial.println("ESP-NOW initialized");
  
  // If we have a saved peer, start reconnection attempts immediately
  if (link_.hasPeer()) {
    Serial.println("Saved peer found - initiating reconnection");
    link_.requestReconnect();
  }

  state_.begin();

  delay(500);
  if (ui_) {
    ui_->startLogoWipe(trailair::display::Icons::logo_bmp, trailair::display::Icons::LogoW, trailair::display::Icons::LogoH, false, 5);
    // Wait for wipe to complete before continuing
    while (ui_->isLogoWipeActive()) {
      ui_->updateLogoWipe();
      delay(1);
    }
  }
}

void RemoteApp::onStatusStatic_(void* ctx, const trailair::protocol::Response& msg) {
  static_cast<RemoteApp*>(ctx)->onStatus_(msg);
}
void RemoteApp::onPairEventStatic_(void* ctx, trailair::comms::PairEvent ev, const uint8_t mac[6]) {
  static_cast<RemoteApp*>(ctx)->onPairEvent_(ev, mac);
}

void RemoteApp::onStatus_(const trailair::protocol::Response& msg) {
  state_.onStatus(msg);
}
void RemoteApp::onPairEvent_(trailair::comms::PairEvent ev, const uint8_t mac[6]) {
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
    ui_->drawLogo(trailair::display::Icons::logo_bmp, trailair::display::Icons::LogoW, trailair::display::Icons::LogoH);
    delay(1000);
    ui_->startLogoWipe(trailair::display::Icons::logo_bmp, trailair::display::Icons::LogoW, trailair::display::Icons::LogoH, false, 5);
    // Wait for wipe to complete before sleeping
    while (ui_->isLogoWipeActive()) {
      ui_->updateLogoWipe();
      delay(1);
    }
  }
  WiFi.disconnect();
  WiFi.mode(WIFI_OFF);
  esp_wifi_stop();
  esp_light_sleep_start();
  Serial.println("Woke up from sleep.");
  
  // Check battery FIRST before re-initializing anything
  batteryMon_.update();
  if (batteryMon_.isCritical()) {
    Serial.println("Critical battery detected on wake!");
    criticalBatteryShutdown_();
    return; // Will loop back into sleep without WiFi init
  }
  
  // Re-initialize WiFi and ESP-NOW after wake
  Serial.println("Reinitializing communication after wake...");
  if (link_.hasPeer()) {
    // Reinit ESP-NOW (WiFi was stopped during sleep)
    if (!link_.begin(nullptr)) {
      Serial.println("ESP-NOW reinit after wake failed!");
    }
    // Configure link again
    const trailair::config::CommunicationConfiguration linkCfg{};
    link_.setConnectionTimeoutMs(linkCfg.connectionTimeoutMilliseconds);
    link_.setPingBackoffStartMs(linkCfg.pingBackoffStartMilliseconds);
    link_.setPairReqIntervalMs(linkCfg.pairingRequestIntervalMilliseconds);
    link_.setStatusCallback(&RemoteApp::onStatusStatic_, this);
    link_.setPairCallback(&RemoteApp::onPairEventStatic_, this);
    // Now request reconnection
    link_.requestReconnect();
  }
  state_.resetAfterWake();
}

void RemoteApp::criticalBatteryShutdown_() {
  Serial.println("CRITICAL BATTERY - Forcing sleep for battery protection");
  
  // Show warning on screen
  if (ui_) {
    ui_->drawCriticalBattery();
    delay(1000);
  }
  
  // Go straight to sleep without WiFi/radio init
  esp_light_sleep_start();
  Serial.println("Woke from critical battery sleep.");
  
  // Re-check battery on wake
  batteryMon_.update();
  if (batteryMon_.isCritical()) {
    // Still critical, loop back
    criticalBatteryShutdown_();
  } else {
    // Battery recovered, do full wake
    Serial.println("Battery recovered, resuming normal operation.");
    if (link_.hasPeer()) link_.requestReconnect();
    state_.resetAfterWake();
  }
}

void RemoteApp::loop() {
  // Update any active animations first
  if (ui_) {
    ui_->updateLogoWipe();
  }

  // Read buttons
  buttons_.service();
  if (trailair::time::hasElapsed(trailair::time::getMilliseconds(), lastButtonPressedMs_, SLEEP_TIMEOUT_MS_)) {
    Serial.println("Sleep timeout exceeded.");
    goToSleep_();
  }

  // Battery
  batteryMon_.update();
  state_.onBatteryPercent(batteryMon_.percent());
  
  // Critical battery protection - force sleep immediately
  if (batteryMon_.isCritical()) {
    Serial.println("Critical battery detected during operation!");
    criticalBatteryShutdown_();
    return; // Exit loop to prevent further operation
  }

  // Link service
  link_.service();
  bool isConn = link_.isConnected();
  bool isConnIng = link_.isConnecting();

  // State update
  state_.update(trailair::time::getMilliseconds(), isConn, isConnIng);
  if (state_.takeSleepRequest()) {
    goToSleep_();
  }

  // Render
  if (ui_) {
    trailair::display::DisplayModel dm;
    state_.buildDisplayModel(dm);
    ui_->render(dm);
  }

  delay(10);
}

}} // namespace ta::app
