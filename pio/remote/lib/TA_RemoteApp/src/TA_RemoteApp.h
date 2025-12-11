#pragma once
#include <stdint.h>
#include <TA_Protocol.h>
#include <TA_Comms.h>
#include <TA_State.h>
#include <TA_Input.h>
#include <TA_Display.h>
#include <TA_Battery.h>
#include <Adafruit_SSD1306.h>

namespace trailair { namespace app {

class RemoteApp {
public:
  struct Pins { uint8_t btnLeft, btnDown, btnUp, btnRight; int batteryPin; };

  explicit RemoteApp(const Pins& pins, Adafruit_SSD1306* disp = nullptr)
    : pins_(pins), buttons_({ pins.btnLeft, pins.btnDown, pins.btnUp, pins.btnRight }),
      disp_(disp), ui_(disp ? new trailair::display::DisplayController(*disp) : nullptr), state_(link_) {}
  ~RemoteApp() { delete ui_; }

  void begin();
  void loop();

  // Accessors
  trailair::comms::EspNowLink& link() { return link_; }
  trailair::state::StateController& state() { return state_; }

private:
  // Callbacks
  static void onStatusStatic_(void* ctx, const trailair::protocol::Response& msg);
  static void onPairEventStatic_(void* ctx, trailair::comms::PairEvent ev, const uint8_t mac[6]);
  void onStatus_(const trailair::protocol::Response& msg);
  void onPairEvent_(trailair::comms::PairEvent ev, const uint8_t mac[6]);

  void setupWakeup_();
  void goToSleep_();
  void criticalBatteryShutdown_(); // Force sleep due to low battery

private:
  Pins pins_{};

  // Subsystems
  trailair::comms::EspNowLink link_{};
  trailair::state::StateController state_;
  trailair::input::ButtonManager buttons_;
  trailair::battery::BatteryMonitor batteryMon_{};

  // Display (optional)
  Adafruit_SSD1306* disp_ = nullptr;
  trailair::display::DisplayController* ui_ = nullptr;

  // Sleep/inactivity
  static constexpr unsigned long SLEEP_TIMEOUT_MS_ = 300000; // 5 minutes
  unsigned long lastButtonPressedMs_ = 0;
};

}} // namespace ta::app
