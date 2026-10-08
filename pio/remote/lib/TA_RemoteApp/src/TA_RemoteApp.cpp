#include "TA_RemoteApp.h"
#include <Arduino.h>
#include <esp_sleep.h>
#include <driver/gpio.h>
#include <TA_Time.h>  // Overflow-safe time utilities

namespace trailair { namespace app {

void RemoteApp::begin() {
  batteryMon_.begin(pins_.batteryPin, ADC_11db);

  // Display (blocking boot logo wipe-in)
  if (ui_) ui_->begin(0x3C, true);

  // Buttons -> state
  buttons_.begin();
  buttons_.subscribe([](void* ctx, const trailair::ButtonEvent& e){
    auto* self = static_cast<RemoteApp*>(ctx);
    self->lastButtonPressedMs_ = trailair::time::getMilliseconds();
    self->state_.onButton(e);
  }, this);

  // Left button wakes from light sleep
  gpio_wakeup_enable(static_cast<gpio_num_t>(pins_.btnLeft), GPIO_INTR_LOW_LEVEL);
  if (esp_sleep_enable_gpio_wakeup() != ESP_OK) Serial.println("Failed to set GPIO Wake-Up as wake-up source.");

  // Link -> state (callbacks survive link end()/begin())
  link_.setStatusCallback([](void* ctx, const trailair::protocol::Status& status){
    static_cast<trailair::state::StateController*>(ctx)->onStatus(status);
  }, &state_);
  link_.setPairCallback([](void* ctx, trailair::comms::PairEvent ev){
    static_cast<trailair::state::StateController*>(ctx)->onPairEvent(ev);
  }, &state_);
  startLink_();

  lastButtonPressedMs_ = trailair::time::getMilliseconds();
}

void RemoteApp::startLink_() {
  if (!link_.begin()) Serial.println("ESP-NOW init failed");
}

void RemoteApp::waitForLeftRelease_() {
  while (digitalRead(pins_.btnLeft) == LOW) delay(10);
}

void RemoteApp::goToSleep_(bool criticalBattery) {
  Serial.println(criticalBattery ? "CRITICAL BATTERY - forcing sleep" : "Entering light sleep...");
  // Stop any manual hold now rather than waiting for the board's lease to lapse
  link_.sendButton(trailair::ButtonAction::Released, trailair::ButtonId::Down);
  link_.sendButton(trailair::ButtonAction::Released, trailair::ButtonId::Up);
  if (ui_) {
    if (criticalBattery) {
      ui_->drawCriticalBattery();
      delay(1000);
    } else {
      ui_->drawLogo();
      delay(1000);
      ui_->wipeLogo(false);
    }
  }
  link_.end();

  for (;;) {
    waitForLeftRelease_();   // LOW-level wake: a still-held button would wake us instantly
    esp_light_sleep_start();
    waitForLeftRelease_();   // swallow the wake press so it never reaches the board as a click
    Serial.println("Woke up from sleep.");

    // Fresh battery reading: the rolling average still holds pre-sleep samples
    batteryMon_.reset();
    for (int i = 0; i < batteryMon_.config().sampleCount; ++i) batteryMon_.update();
    if (!batteryMon_.isCritical()) break;

    Serial.println("Battery still critical, back to sleep.");
    if (ui_) {
      ui_->drawCriticalBattery();
      delay(1000);
    }
  }

  startLink_();
  state_.resetAfterWake();
  lastButtonPressedMs_ = trailair::time::getMilliseconds();  // or the inactivity timer fires immediately
}

void RemoteApp::loop() {
  buttons_.service();
  // Holding a button (e.g. a long manual inflate) counts as activity
  for (uint8_t i = 0; i < trailair::BUTTON_COUNT; ++i) {
    if (buttons_.isHeld(static_cast<trailair::ButtonId>(i))) lastButtonPressedMs_ = trailair::time::getMilliseconds();
  }
  if (trailair::time::hasElapsed(trailair::time::getMilliseconds(), lastButtonPressedMs_, SLEEP_TIMEOUT_MS_)) {
    Serial.println("Sleep timeout exceeded.");
    goToSleep_(false);
  }

  batteryMon_.update();
  state_.onBatteryPercent(batteryMon_.percent());
  if (batteryMon_.isCritical()) {
    goToSleep_(true);
    return;
  }

  link_.service();
  state_.update(trailair::time::getMilliseconds());
  if (state_.takeSleepRequest()) {
    goToSleep_(false);
  }

  if (ui_) {
    trailair::display::DisplayModel dm;
    state_.buildDisplayModel(dm);
    ui_->render(dm);
  }

  delay(10);
}

}} // namespace trailair::app
