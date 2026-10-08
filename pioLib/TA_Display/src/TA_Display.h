#pragma once
#include <Arduino.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <TA_Types.h>

/**
 * @file TA_Display.h
 * @brief SSD1306 rendering for both devices.
 *
 * The app fills a DisplayModel (plain data) every loop and calls render(). The screen is only
 * redrawn when something visible changed: a full redraw is ~13 ms of blocking I2C, which would
 * otherwise slow button and sensor sampling on every loop.
 */

namespace trailair {
namespace display {

struct DisplayModel {
  View view = View::Disconnected;
  ControllerState controllerState = ControllerState::Idle;

  float currentPressurePSI = 0.0f;
  float targetPressurePSI = 0.0f;
  uint8_t errorCode = 0;                 ///< trailair::errors::ErrorCode, for the Error view

  // Remote-only status widgets; everything else renders identically on both devices
  bool showLinkIcon = true;              ///< false on the board
  bool connected = false;                ///< Link icon state
  bool showBatteryIcon = true;           ///< false on the board (externally powered)
  int batteryPercentage = 0;             ///< 0-100

  bool showReconnectHint = false;        ///< Disconnected view: show the Right (pair) hint
  bool pairingFailed = false;            ///< Pairing view: failed (timeout or busy)
  bool pairingBusy = false;              ///< Pairing view: failed because the board is taken
};

class DisplayController {
public:
  explicit DisplayController(Adafruit_SSD1306& display) : _display(display) {}

  /// Initialize the panel; optionally play the boot logo wipe-in (blocking)
  bool begin(uint8_t i2cAddress = 0x3C, bool showBootLogo = true);

  /// Draw the logo centered (static)
  void drawLogo();
  /// Wipe the logo in (reveal) or out (hide), blocking, stepDelayMs per column
  void wipeLogo(bool wipeIn, uint16_t stepDelayMs = 5);
  /// "Charge Battery" screen shown before a forced low-battery sleep
  void drawCriticalBattery();

  /// Draw the model if anything visible changed since the last render
  void render(const DisplayModel& model);

private:
  // Screens
  void renderDisconnected_(const DisplayModel& m);
  void renderIdle_(const DisplayModel& m);
  void renderManual_(const DisplayModel& m);
  void renderSeeking_(const DisplayModel& m);
  void renderDone_(const DisplayModel& m);
  void renderError_(const DisplayModel& m);
  void renderPairing_(const DisplayModel& m);

  // Widgets
  void drawStatusRow_(const DisplayModel& m, bool withLinkIcon);
  void drawBatteryIcon_(int percentage);
  void drawConnectionIcon_(bool connected);
  void drawButtonHints_(const uint8_t* left, const uint8_t* down, const uint8_t* up, const uint8_t* right);

  // Layout
  int contentTop_() const;
  int contentBottom_() const;
  void textSize_(const char* text, uint8_t fontSize, int16_t& w, int16_t& h);
  int centeredY_(int height, int top, int bottom) const;
  void drawCenteredText_(const char* text, uint8_t fontSize, int y);
  void drawTwoLinesCentered_(const char* top, uint8_t topSize, const char* bottom, uint8_t bottomSize);
  void drawTwoColumnValues_(const char* left, const char* right);

  /// What render() compares to decide whether to redraw
  struct Snapshot {
    View view; ControllerState state;
    long current; long target; uint8_t error;
    bool link; bool connected; bool battery; int batteryPct;
    bool reconnectHint; bool pairFailed; bool pairBusy;
    uint8_t animFrame;
    bool operator==(const Snapshot& o) const;
  };
  static Snapshot snapshot_(const DisplayModel& m);

  Adafruit_SSD1306& _display;
  Snapshot _last{};
  bool _hasLast = false;  ///< false forces the next render (after logo/critical screens)
};

} // namespace display
} // namespace trailair
