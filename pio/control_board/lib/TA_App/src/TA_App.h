#pragma once
#include <stdint.h>
#include <Adafruit_SSD1306.h>
#include <TA_Types.h>
#include <TA_Config.h>
#include <TA_Controller.h>
#include <TA_Display.h>
#include <TA_Input.h>
#include "TA_Actuators.h"
#include "TA_Sensors.h"
#include "TA_CommsBoard.h"
#include "TA_BoardUI.h"

namespace trailair { namespace app {

/// Board orchestrator: owns the subsystems and runs the main loop.
class App {
public:
  struct Pins {
    uint8_t btnLeft;
    uint8_t btnDown;
    uint8_t btnUp;
    uint8_t btnRight;

    Pins() : btnLeft(0), btnDown(0), btnUp(0), btnRight(0) {}
    Pins(uint8_t l, uint8_t d, uint8_t u, uint8_t r) : btnLeft(l), btnDown(d), btnUp(u), btnRight(r) {}
  };

  /// Optional display and on-board buttons (btnLeft == 0 means no buttons)
  explicit App(Adafruit_SSD1306* disp = nullptr, const Pins& buttonPins = Pins())
    : ui_(controller_),
      display_(disp ? new trailair::display::DisplayController(*disp) : nullptr),
      buttons_(buttonPins.btnLeft != 0
               ? new trailair::input::ButtonManager({buttonPins.btnLeft, buttonPins.btnDown, buttonPins.btnUp, buttonPins.btnRight})
               : nullptr) {}
  ~App() { delete display_; delete buttons_; }

  void begin();
  void loop();

private:
  void onRemoteFrame_(const trailair::protocol::Frame& f);
  void onLocalButton_(const ButtonEvent& e);
  bool localHeld_(ButtonId id) const { return buttons_ && buttons_->isHeld(id); }

  const trailair::config::CommunicationConfiguration linkCfg_{};

  trailair::actuators::Actuators actuators_{};
  trailair::sensors::PressureFilter pressure_{};
  trailair::controller::PressureController controller_{};
  trailair::boardui::BoardUI ui_;  // after controller_: holds a reference to it
  trailair::comms::BoardLink comms_{};
  trailair::display::DisplayController* display_ = nullptr;
  trailair::input::ButtonManager* buttons_ = nullptr;

  // Hold Right on the board to forget the paired remote; "Pairing" shows until one pairs
  bool pairingWindow_ = false;
  uint32_t pairingUntilMs_ = 0;

  // Status to the remote: periodic while it listens, plus right after each of its frames
  bool statusDue_ = false;
  uint32_t lastStatusMs_ = 0;
  static constexpr uint32_t STATUS_INTERVAL_MS_ = 200;
};

}} // namespace trailair::app
