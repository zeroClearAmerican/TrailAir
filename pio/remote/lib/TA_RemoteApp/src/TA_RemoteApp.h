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
    : pins_(pins), state_(link_), buttons_({ pins.btnLeft, pins.btnDown, pins.btnUp, pins.btnRight }),
      ui_(disp ? new trailair::display::DisplayController(*disp) : nullptr) {}
  ~RemoteApp() { delete ui_; }

  void begin();
  void loop();

private:
  void startLink_();
  void waitForLeftRelease_();
  void goToSleep_(bool criticalBattery);

private:
  Pins pins_{};

  // Subsystems
  trailair::comms::RemoteLink link_{};
  trailair::state::StateController state_;
  trailair::input::ButtonManager buttons_;
  trailair::battery::BatteryMonitor batteryMon_{};

  // Display (optional)
  trailair::display::DisplayController* ui_ = nullptr;

  // Sleep/inactivity
  static constexpr unsigned long SLEEP_TIMEOUT_MS_ = 300000; // 5 minutes
  unsigned long lastButtonPressedMs_ = 0;
};

}} // namespace trailair::app
