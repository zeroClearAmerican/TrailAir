/**
 * @file TA_Display.cpp
 * @brief DisplayController implementation
 */
#include "TA_Display.h"
#include "TA_DisplayIcons.h"
#include <TA_Errors.h>
#include <math.h>
#include <string.h>

namespace trailair {
namespace display {

namespace {
  constexpr int STATUS_ROW_HEIGHT = 8;   // battery + link icons
  constexpr int BUTTON_ICON_SIZE = 6;    // hint row at the bottom
  constexpr int COLUMN_GAP = 16;         // between current and target values
  constexpr uint8_t VALUE_TEXT_SIZE = 2;
  constexpr int LOW_BATTERY_PERCENT = 15;
  constexpr uint32_t PAIRING_DOT_MS = 500;
}

// ---------------------------------------------------------------------------------------------
// Logo / special screens (blocking, drawn directly)
// ---------------------------------------------------------------------------------------------

bool DisplayController::begin(uint8_t i2cAddress, bool showBootLogo) {
  if (!_display.begin(SSD1306_SWITCHCAPVCC, i2cAddress)) return false;
  _display.clearDisplay();
  if (showBootLogo) {
    wipeLogo(true);
    _display.clearDisplay();
  }
  _display.display();
  _hasLast = false;
  return true;
}

void DisplayController::drawLogo() {
  _display.clearDisplay();
  int x = (_display.width()  - Icons::LogoW) / 2;
  int y = (_display.height() - Icons::LogoH) / 2;
  _display.drawBitmap(x, y, Icons::logo_bmp, Icons::LogoW, Icons::LogoH, SSD1306_WHITE);
  _display.display();
  _hasLast = false;
}

void DisplayController::wipeLogo(bool wipeIn, uint16_t stepDelayMs) {
  int x = (_display.width()  - Icons::LogoW) / 2;
  int y = (_display.height() - Icons::LogoH) / 2;
  for (int col = 0; col <= Icons::LogoW; ++col) {
    _display.clearDisplay();
    _display.drawBitmap(x, y, Icons::logo_bmp, Icons::LogoW, Icons::LogoH, SSD1306_WHITE);
    if (wipeIn) _display.fillRect(x + col, y, Icons::LogoW - col, Icons::LogoH, SSD1306_BLACK);  // reveal left→right
    else        _display.fillRect(x, y, col, Icons::LogoH, SSD1306_BLACK);                     // hide left→right
    _display.display();
    delay(stepDelayMs);
  }
  _hasLast = false;
}

void DisplayController::drawCriticalBattery() {
  _display.clearDisplay();
  drawCenteredText_("Charge Battery", 1, centeredY_(8, 0, _display.height()));
  _display.display();
  _hasLast = false;
}

// ---------------------------------------------------------------------------------------------
// Model rendering
// ---------------------------------------------------------------------------------------------

bool DisplayController::Snapshot::operator==(const Snapshot& o) const {
  return view == o.view && state == o.state && current == o.current && target == o.target &&
         error == o.error && link == o.link && connected == o.connected && battery == o.battery &&
         batteryPct == o.batteryPct && reconnectHint == o.reconnectHint &&
         pairFailed == o.pairFailed && pairBusy == o.pairBusy && animFrame == o.animFrame;
}

DisplayController::Snapshot DisplayController::snapshot_(const DisplayModel& m) {
  Snapshot s;
  s.view = m.view;
  s.state = m.controllerState;
  s.current = lroundf(m.currentPressurePSI);  // compare what's drawn, not sensor jitter
  s.target = lroundf(m.targetPressurePSI);
  s.error = m.errorCode;
  s.link = m.showLinkIcon;
  s.connected = m.connected;
  s.battery = m.showBatteryIcon;
  s.batteryPct = m.batteryPercentage;
  s.reconnectHint = m.showReconnectHint;
  s.pairFailed = m.pairingFailed;
  s.pairBusy = m.pairingBusy;
  s.animFrame = (m.view == View::Pairing && !m.pairingFailed) ? (millis() / PAIRING_DOT_MS) % 4 : 0;
  return s;
}

void DisplayController::render(const DisplayModel& m) {
  Snapshot s = snapshot_(m);
  if (_hasLast && s == _last) return;
  _last = s;
  _hasLast = true;

  _display.clearDisplay();
  switch (m.view) {
    case View::Disconnected: renderDisconnected_(m); break;
    case View::Idle:         renderIdle_(m);         break;
    case View::Manual:       renderManual_(m);       break;
    case View::Seeking:      renderSeeking_(m);      break;
    case View::Done:         renderDone_(m);         break;
    case View::Error:        renderError_(m);        break;
    case View::Pairing:      renderPairing_(m);      break;
  }
  _display.display();
}

void DisplayController::renderDisconnected_(const DisplayModel& m) {
  if (m.showBatteryIcon) drawBatteryIcon_(m.batteryPercentage);
  const int ICON = 20;
  _display.drawBitmap((_display.width() - ICON) / 2, centeredY_(ICON, contentTop_(), contentBottom_()),
                      m.connected ? Icons::icon_connected_20x20 : Icons::icon_disconnected_20x20,
                      ICON, ICON, SSD1306_WHITE);
  if (m.showReconnectHint) drawButtonHints_(nullptr, nullptr, nullptr, Icons::icon_arrow_right_6x6);
}

void DisplayController::renderIdle_(const DisplayModel& m) {
  drawStatusRow_(m, true);
  drawButtonHints_(Icons::icon_manual_control_6x6, Icons::icon_dash_6x6, Icons::icon_plus_6x6, Icons::icon_arrow_right_6x6);
  char current[8], target[8];
  snprintf(current, sizeof(current), "%ld", lroundf(m.currentPressurePSI));
  snprintf(target, sizeof(target), "%ld", lroundf(m.targetPressurePSI));
  drawTwoColumnValues_(current, target);
}

void DisplayController::renderSeeking_(const DisplayModel& m) {
  drawStatusRow_(m, true);
  drawButtonHints_(nullptr, nullptr, nullptr, Icons::icon_cancel_6x6);  // Right = cancel

  const char* verb = "Ready";
  switch (m.controllerState) {
    case ControllerState::AirUp:    verb = "Inflating..."; break;
    case ControllerState::Venting:  verb = "Deflating..."; break;
    case ControllerState::Checking: verb = "Checking...";  break;
    case ControllerState::Error:    verb = "Error";        break;
    case ControllerState::Idle:     break;
  }
  char pressure[12];
  snprintf(pressure, sizeof(pressure), "%ld PSI", lroundf(m.currentPressurePSI));
  drawTwoLinesCentered_(verb, 1, pressure, 2);
}

void DisplayController::renderDone_(const DisplayModel& m) {
  drawStatusRow_(m, true);
  drawCenteredText_("Done!", 2, centeredY_(16, contentTop_(), contentBottom_()));
}

void DisplayController::renderManual_(const DisplayModel& m) {
  drawStatusRow_(m, true);
  drawButtonHints_(Icons::icon_cancel_6x6, Icons::icon_arrow_down_6x6, Icons::icon_arrow_up_6x6, nullptr);

  const char* status = "Manual";
  if (m.controllerState == ControllerState::AirUp) status = "Inflating...";
  else if (m.controllerState == ControllerState::Venting) status = "Deflating...";
  char pressure[12];
  snprintf(pressure, sizeof(pressure), "%ld PSI", lroundf(m.currentPressurePSI));
  drawTwoLinesCentered_(status, 1, pressure, 2);
}

void DisplayController::renderError_(const DisplayModel& m) {
  drawStatusRow_(m, true);
  drawButtonHints_(nullptr, nullptr, nullptr, Icons::icon_arrow_right_6x6);  // Right = acknowledge

  char message[16];
  const char* description = trailair::errors::getShortDescription(m.errorCode);
  if (strcmp(description, "Error") == 0) snprintf(message, sizeof(message), "E:%u", static_cast<unsigned>(m.errorCode));  // uncatalogued code
  else snprintf(message, sizeof(message), "%s", description);

  int16_t w, h;
  textSize_(message, 2, w, h);
  drawCenteredText_(message, w > _display.width() ? 1 : 2, centeredY_(16, contentTop_(), contentBottom_()));
}

void DisplayController::renderPairing_(const DisplayModel& m) {
  drawStatusRow_(m, false);  // no link icon: pairing precedes a connection
  drawButtonHints_(nullptr, nullptr, nullptr, Icons::icon_cancel_6x6);  // Right = cancel

  char line[16];
  if (m.pairingFailed) {
    snprintf(line, sizeof(line), "%s", m.pairingBusy ? "Device Busy" : "No Device");
  } else {
    int dots = (millis() / PAIRING_DOT_MS) % 4;
    snprintf(line, sizeof(line), "Pairing%.*s", dots, "...");
  }
  drawCenteredText_(line, 1, centeredY_(8, contentTop_(), contentBottom_()));
}

// ---------------------------------------------------------------------------------------------
// Widgets
// ---------------------------------------------------------------------------------------------

void DisplayController::drawStatusRow_(const DisplayModel& m, bool withLinkIcon) {
  if (m.showBatteryIcon) drawBatteryIcon_(m.batteryPercentage);
  if (withLinkIcon && m.showLinkIcon) drawConnectionIcon_(m.connected);
}

void DisplayController::drawBatteryIcon_(int percentage) {
  const int W = 12, H = 6;
  int fill = (constrain(percentage, 0, 100) * (W - 2)) / 100;
  _display.drawRect(0, 0, W, H, SSD1306_WHITE);
  _display.drawRect(W, 2, 1, 2, SSD1306_WHITE);  // tip
  _display.fillRect(1, 1, fill, H - 2, SSD1306_WHITE);
  if (percentage < LOW_BATTERY_PERCENT) {
    _display.setTextSize(1);
    _display.setTextColor(SSD1306_WHITE);
    _display.setCursor(W + 2, 0);
    _display.print("!");
  }
}

void DisplayController::drawConnectionIcon_(bool connected) {
  _display.drawBitmap(_display.width() - 8, 1,
                      connected ? Icons::icon_connected_8x6 : Icons::icon_disconnected_8x6,
                      8, 6, SSD1306_WHITE);
}

void DisplayController::drawButtonHints_(const uint8_t* left, const uint8_t* down, const uint8_t* up, const uint8_t* right) {
  const uint8_t* icons[4] = { left, down, up, right };
  const int cell = _display.width() / 4;
  const int y = _display.height() - BUTTON_ICON_SIZE;
  const int offset = (cell - BUTTON_ICON_SIZE) / 2;
  for (int i = 0; i < 4; ++i) {
    if (icons[i]) _display.drawBitmap(i * cell + offset, y, icons[i], BUTTON_ICON_SIZE, BUTTON_ICON_SIZE, SSD1306_WHITE);
  }
}

// ---------------------------------------------------------------------------------------------
// Layout helpers
// ---------------------------------------------------------------------------------------------

int DisplayController::contentTop_() const { return STATUS_ROW_HEIGHT; }
int DisplayController::contentBottom_() const { return _display.height() - BUTTON_ICON_SIZE - 2; }  // above hints + margin

void DisplayController::textSize_(const char* text, uint8_t fontSize, int16_t& w, int16_t& h) {
  int16_t x1, y1;
  uint16_t bw, bh;
  _display.setTextSize(fontSize);
  _display.getTextBounds(text, 0, 0, &x1, &y1, &bw, &bh);
  w = static_cast<int16_t>(bw);
  h = static_cast<int16_t>(bh);
}

int DisplayController::centeredY_(int height, int top, int bottom) const {
  return top + (bottom - top - height) / 2;
}

void DisplayController::drawCenteredText_(const char* text, uint8_t fontSize, int y) {
  int16_t w, h;
  textSize_(text, fontSize, w, h);
  _display.setTextSize(fontSize);
  _display.setTextColor(SSD1306_WHITE);
  _display.setCursor((_display.width() - w) / 2, y);
  _display.print(text);
}

void DisplayController::drawTwoLinesCentered_(const char* top, uint8_t topSize, const char* bottom, uint8_t bottomSize) {
  const int LINE_SPACING = 2;
  int16_t tw, th, bw, bh;
  textSize_(top, topSize, tw, th);
  textSize_(bottom, bottomSize, bw, bh);
  int y = centeredY_(th + LINE_SPACING + bh, contentTop_(), contentBottom_());
  if (y < contentTop_()) y = contentTop_();

  _display.setTextColor(SSD1306_WHITE);
  _display.setTextSize(topSize);
  _display.setCursor((_display.width() - tw) / 2, y);
  _display.print(top);
  _display.setTextSize(bottomSize);
  _display.setCursor((_display.width() - bw) / 2, y + th + LINE_SPACING);
  _display.print(bottom);
}

void DisplayController::drawTwoColumnValues_(const char* left, const char* right) {
  int16_t lw, lh, rw, rh;
  textSize_(left, VALUE_TEXT_SIZE, lw, lh);
  textSize_(right, VALUE_TEXT_SIZE, rw, rh);

  const int mid = _display.width() / 2;
  const int y = centeredY_(lh, contentTop_(), contentBottom_());
  // Each value centered in its half, either side of the gap
  int leftEnd = mid - COLUMN_GAP / 2;
  int rightStart = mid + COLUMN_GAP / 2;
  int lx = max(0, (leftEnd - lw) / 2);
  int rx = max(rightStart, rightStart + (_display.width() - rightStart - rw) / 2);

  _display.setTextColor(SSD1306_WHITE);
  _display.setTextSize(VALUE_TEXT_SIZE);
  _display.setCursor(lx, y);
  _display.print(left);
  _display.setCursor(rx, y);
  _display.print(right);

  // Underline the target, arrow from current to target
  if (y + rh < _display.height()) _display.drawLine(rx, y + rh, rx + rw, y + rh, SSD1306_WHITE);
  const int ax = mid - 5, ay = _display.height() / 2;
  _display.fillTriangle(ax, ay - 5, ax, ay + 5, ax + 9, ay, SSD1306_WHITE);
}

} // namespace display
} // namespace trailair
