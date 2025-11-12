#pragma once
#include <stdint.h>
#include <TA_Protocol.h>
#include <TA_Comms.h>
#include <TA_State.h>
#include <TA_Input.h>
#include <TA_Display.h>
#include <TA_Battery.h>
#include <Adafruit_SSD1306.h>

namespace ta { namespace app {

class RemoteApp {
public:
  struct Pins { uint8_t btnLeft, btnDown, btnUp, btnRight; int batteryPin; };

  explicit RemoteApp(const Pins& pins, Adafruit_SSD1306* disp = nullptr)
    : pins_(pins), buttons_({ pins.btnLeft, pins.btnDown, pins.btnUp, pins.btnRight }),
      disp_(disp), ui_(disp ? new ta::display::TA_Display(*disp) : nullptr), state_(link_) {}
  ~RemoteApp() { delete ui_; }

  void begin();
  void loop();

  // Accessors
  ta::comms::EspNowLink& link() { return link_; }
  ta::state::StateController& state() { return state_; }

private:
  // Callbacks
  static void onStatusStatic_(void* ctx, const ta::protocol::Response& msg);
  static void onPairEventStatic_(void* ctx, ta::comms::PairEvent ev, const uint8_t mac[6]);
  void onStatus_(const ta::protocol::Response& msg);
  void onPairEvent_(ta::comms::PairEvent ev, const uint8_t mac[6]);

  void setupWakeup_();
  void goToSleep_();
  void criticalBatteryShutdown_(); // Force sleep due to low battery

private:
  Pins pins_{};

  // Subsystems
  ta::comms::EspNowLink link_{};
  ta::state::StateController state_;
  ta::input::Buttons buttons_;
  ta::battery::TA_BatteryMonitor batteryMon_{};

  // Display (optional)
  Adafruit_SSD1306* disp_ = nullptr;
  ta::display::TA_Display* ui_ = nullptr;

  // Sleep/inactivity
  static constexpr unsigned long SLEEP_TIMEOUT_MS_ = 300000; // 5 minutes
  unsigned long lastButtonPressedMs_ = 0;
};

}} // namespace ta::app
