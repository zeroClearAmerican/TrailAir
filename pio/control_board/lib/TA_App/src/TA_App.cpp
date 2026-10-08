#include "TA_App.h"
#include <Arduino.h>

namespace trailair { namespace app {

namespace {
  constexpr int COMPRESSOR_PIN = 9;
  constexpr int VENT_PIN = 10;
  constexpr int PRESSURE_PIN = 3;
  constexpr int PRESSURE_SAMPLES = 10;
  constexpr float PRESSURE_NOISE_FLOOR_PSI = 0.5f;
  constexpr uint8_t SCREEN_ADDRESS = 0x3C;
}

void App::begin() {
  actuators_.begin({COMPRESSOR_PIN, VENT_PIN});
  pressure_.begin(PRESSURE_PIN, PRESSURE_SAMPLES, PRESSURE_NOISE_FLOOR_PSI);
  controller_.begin(&actuators_, trailair::controller::ControllerConfig{});
  ui_.begin();

  if (!comms_.begin()) Serial.println("[App] ERROR: comms init failed");
  comms_.setRemoteFrameCallback([](void* ctx, const trailair::protocol::Frame& f) {
    static_cast<App*>(ctx)->onRemoteFrame_(f);
  }, this);

  if (display_ && !display_->begin(SCREEN_ADDRESS, true)) Serial.println("[App] WARNING: display init failed");

  if (buttons_) {
    buttons_->begin();
    buttons_->subscribe([](void* ctx, const ButtonEvent& e) {
      static_cast<App*>(ctx)->onLocalButton_(e);
    }, this);
  }
  Serial.println("[App] Ready");
}

void App::onRemoteFrame_(const trailair::protocol::Frame& f) {
  statusDue_ = true;  // reply right away (button feedback, reconnects)
  ButtonAction action;
  if (trailair::protocol::buttonAction(f.type, action)) {
    // Remote taps arrive one frame each, so clickCount is always 1
    ui_.onButton(ButtonEvent{f.button, action, 1}, millis());
  }
}

void App::onLocalButton_(const ButtonEvent& e) {
  // Board-only: hold Right to forget the paired remote so a new one can pair (remote long-holds
  // arrive via onRemoteFrame_ and never reach here)
  if (e.id == ButtonId::Right && e.action == ButtonAction::LongHold) {
    comms_.forget();
    pairingWindow_ = true;
    pairingUntilMs_ = millis() + linkCfg_.boardPairingWindowMilliseconds;
    return;
  }
  // The pairing screen hides the normal UI: Right click closes it, other presses are ignored
  if (pairingWindow_) {
    if (e.id == ButtonId::Right && e.action == ButtonAction::Click) pairingWindow_ = false;
    return;
  }
  ui_.onButton(e, millis());
}

void App::loop() {
  if (buttons_) buttons_->service();
  comms_.service();

  uint32_t now = millis();
  float psi = pressure_.readPsi();

  // UI first: it renews the manual lease for held board buttons before the controller checks it
  ui_.update(now, localHeld_(ButtonId::Up), localHeld_(ButtonId::Down));
  controller_.update(now, psi);
  actuators_.service();  // completes a turn-on deferred by the actuator min-off time

  // Status to the remote. A sleeping remote pings on wake, which makes it active again.
  bool remoteActive = comms_.isRemoteActive(linkCfg_.remoteActiveTimeoutMilliseconds);
  if (remoteActive && (statusDue_ || now - lastStatusMs_ >= STATUS_INTERVAL_MS_)) {
    comms_.sendStatus(ui_.status());
    statusDue_ = false;
    lastStatusMs_ = now;
  }

  if (pairingWindow_ && (comms_.isPaired() || static_cast<int32_t>(now - pairingUntilMs_) >= 0)) pairingWindow_ = false;

  if (display_) {
    trailair::display::DisplayModel dm;
    ui_.fillDisplay(dm);
    if (pairingWindow_) dm.view = View::Pairing;
    display_->render(dm);
  }

  delay(10);
}

}} // namespace trailair::app
