#include "TA_Display.h"
#include "TA_DisplayIcons.h"

namespace ta {
    namespace display {

        bool TA_Display::begin(uint8_t i2cAddr, bool showBootLogo) {
            // Note: caller should have constructed Adafruit_SSD1306 with width/height/Wire/reset already
            if (!d_.begin(SSD1306_SWITCHCAPVCC, i2cAddr)) {
                return false;
            }
            d_.clearDisplay();
            if (showBootLogo && Icons::logo_bmp && Icons::LogoW && Icons::LogoH) {
                logoWipe(Icons::logo_bmp, Icons::LogoW, Icons::LogoH, true, 5);
            } else {
                d_.display();
            }
            return true;
        }

        void TA_Display::drawLogo(const uint8_t* logo, uint8_t w, uint8_t h) {
            d_.clearDisplay();
            int x = (d_.width()  - w) / 2;
            int y = (d_.height() - h) / 2;
            d_.drawBitmap(x, y, logo, w, h, SSD1306_WHITE);
            d_.display();
        }

        void TA_Display::logoWipe(const uint8_t* logo, uint8_t w, uint8_t h, bool wipeIn, uint16_t stepDelayMs) {
            int x = (d_.width()  - w) / 2;
            int y = (d_.height() - h) / 2;

            d_.clearDisplay();
            for (int col = 0; col <= w; ++col) {
                d_.drawBitmap(x, y, logo, w, h, SSD1306_WHITE);
                if (wipeIn) {
                    // Mask the right side, revealing only the left w pixels
                    d_.fillRect(x + col, y, w - col, h, SSD1306_BLACK);
                } else {
                    // Mask the left side, hiding the left w pixels
                    d_.fillRect(x, y, col, h, SSD1306_BLACK);
                }
                d_.display();
                delay(stepDelayMs);
            }
        }

        void TA_Display::render(const DisplayModel& m) {
            d_.clearDisplay();
            switch (m.view) {
                case View::Disconnected: drawDisconnected(m); break;
                case View::Idle:         drawIdle(m);         break;
                case View::Manual:       drawManual(m);       break;
                case View::Seeking:      drawSeeking(m);      break;
                case View::Error:        drawError(m);        break;
                case View::Pairing:      drawPairing(m);      break; // NEW
            }
            d_.display();
        }

        void TA_Display::drawBatteryIcon_(int percent) {
            int batteryX = 0;
            int batteryY = 0;
            int batteryW = 12;
            int batteryH = 6;
            int fillW = (int)((constrain(percent, 0, 98) / 100.0f) * (batteryW - 2));

            d_.drawRect(batteryX, batteryY, batteryW, batteryH, SSD1306_WHITE);
            d_.drawRect(batteryX + batteryW, batteryY + 2, 1, 2, SSD1306_WHITE);
            d_.fillRect(batteryX + 1, batteryY + 1, fillW, batteryH - 2, SSD1306_WHITE);

            if (percent < 15) {
                d_.setTextSize(1);
                d_.setTextColor(SSD1306_WHITE);
                d_.setCursor(batteryX + batteryW + 2, batteryY);
                d_.print("!");
            }
        }

        void TA_Display::drawConnectionIcon_(Link link) {
            int connX = d_.width() - 8;
            int connY = 1;
            if (link == Link::Connected) {
                d_.drawBitmap(connX, connY, Icons::icon_connected_8x6, 8, 6, SSD1306_WHITE);
            } else {
                d_.drawBitmap(connX, connY, Icons::icon_disconnected_8x6, 8, 6, SSD1306_WHITE);
            }
        }

        void TA_Display::drawButtonHints_(const uint8_t* left, const uint8_t* down, const uint8_t* up, const uint8_t* right) {
            const int iconSize = style_.btnIcon;
            const int cellW = 32;
            const int y = d_.height() - iconSize;
            const int offset = (cellW - iconSize) / 2;
            if (left)  d_.drawBitmap(0   + offset, y, left,  iconSize, iconSize, SSD1306_WHITE);
            if (down)  d_.drawBitmap(32  + offset, y, down,  iconSize, iconSize, SSD1306_WHITE);
            if (up)    d_.drawBitmap(64  + offset, y, up,    iconSize, iconSize, SSD1306_WHITE);
            if (right) d_.drawBitmap(96  + offset, y, right, iconSize, iconSize, SSD1306_WHITE);
        }

        // Layout helpers
        int TA_Display::topSafe_() const { return style_.statusRowH; }

        void TA_Display::measure_(const String& s, uint8_t size, int16_t& w, int16_t& h) {
            int16_t bx, by; uint16_t bw, bh;
            d_.setTextSize(size);
            d_.getTextBounds(s, 0, 0, &bx, &by, &bw, &bh);
            w = (int16_t)bw; h = (int16_t)bh;
        }
        int TA_Display::centerX_(int w) const { return (d_.width() - w) / 2; }
        int TA_Display::centerYBetween_(int h, int top, int bottom) const {
            int avail = bottom - top; return top + (avail - h) / 2;
        }
        void TA_Display::drawCenteredText_(const String& s, uint8_t size, int y) {
            int16_t w, h; measure_(s, size, w, h);
            int x = centerX_(w);
            d_.setTextSize(size);
            d_.setTextColor(SSD1306_WHITE);
            d_.setCursor(x, y);
            d_.print(s);
        }
        void TA_Display::drawTwoLineCentered_(const String& top, uint8_t topSize,
                                              const String& bottom, uint8_t bottomSize,
                                              int spacing, int topClamp) {
            int16_t w1, h1, w2, h2;
            measure_(top, topSize, w1, h1);
            measure_(bottom, bottomSize, w2, h2);
            int totalH = h1 + spacing + h2;
            int yStart = centerYBetween_(totalH, topClamp, d_.height());
            if (yStart < topClamp) yStart = topClamp;
            d_.setTextColor(SSD1306_WHITE);
            d_.setTextSize(topSize);
            d_.setCursor(centerX_(w1), yStart);
            d_.print(top);
            d_.setTextSize(bottomSize);
            d_.setCursor(centerX_(w2), yStart + h1 + spacing);
            d_.print(bottom);
        }

        void TA_Display::drawTwoColumnValues_(const String& left, const String& right, uint8_t textSize, uint8_t gap) {
            d_.setTextColor(SSD1306_WHITE);
            int16_t lw, lh, rw, rh;
            measure_(left, textSize, lw, lh);
            measure_(right, textSize, rw, rh);
            int centerY = centerYBetween_(lh, topSafe_(), d_.height());
            int mid = d_.width() / 2;
            // Left cell
            int l0 = 0, l1 = mid - gap/2;
            int r0 = mid + gap/2, r1 = d_.width();
            int lx = l0 + (l1 - l0 - lw) / 2; if (lx < l0) lx = l0;
            int rx = r0 + (r1 - r0 - rw) / 2; if (rx < r0) rx = r0;
            d_.setTextSize(textSize);
            d_.setCursor(lx, centerY); d_.print(left);
            d_.setCursor(rx, centerY); d_.print(right);
            // Underline right
            int underlineY = centerY + rh;
            if (underlineY < d_.height()) d_.drawLine(rx, underlineY, rx + rw, underlineY, SSD1306_WHITE);
            // Direction arrow between columns
            int ax = mid - 5;
            int ay = d_.height() / 2;
            d_.fillTriangle(ax, ay - 5, ax, ay + 5, ax + 9, ay, SSD1306_WHITE);
        }

        void TA_Display::drawDisconnected(const DisplayModel& m) {
            drawBatteryIcon_(m.batteryPercent);

            // Center 20x20 icon using helpers, keeping top status row clear
            const uint8_t* bmp = (m.link == Link::Connected) ? Icons::icon_connected_20x20 : Icons::icon_disconnected_20x20;
            const int w = 20, h = 20;
            const int x = centerX_(w);
            const int y = centerYBetween_(h, topSafe_(), d_.height());
            d_.drawBitmap(x, y, bmp, w, h, SSD1306_WHITE);

            // Right button hint (retry) when disconnected
            if (m.link == Link::Disconnected && m.showReconnectHint) {
                drawButtonHints_(nullptr, nullptr, nullptr, Icons::icon_arrow_right_6x6);
            }
        }

        void TA_Display::drawIdle(const DisplayModel& m) {
            drawBatteryIcon_(m.batteryPercent);
            drawConnectionIcon_(m.link);
            drawButtonHints_(Icons::icon_manual_control_6x6, Icons::icon_dash_6x6, Icons::icon_plus_6x6, Icons::icon_arrow_right_6x6);
            String currentStr = String((int)m.currentPSI);
            String targetStr  = String((int)m.targetPSI);
            drawTwoColumnValues_(currentStr, targetStr, style_.valueTextSize, style_.colGap);
        }

        void TA_Display::drawSeeking(const DisplayModel& m) {
            drawBatteryIcon_(m.batteryPercent);
            drawConnectionIcon_(m.link);

            // Right=Cancel
            drawButtonHints_(nullptr, nullptr, nullptr, Icons::icon_cancel_6x6);

            if (m.seekingShowDoneHold) {
                drawCenteredText_("Done!", 2, centerYBetween_(0, topSafe_(), d_.height()));
                return;
            }

            const char* verb = "Ready";
            switch (m.ctrl) {
                case Ctrl::Idle:     verb = "Ready";        break;
                case Ctrl::AirUp:    verb = "Inflating..."; break;
                case Ctrl::Venting:  verb = "Deflating..."; break;
                case Ctrl::Checking: verb = "Checking...";  break;
                case Ctrl::Error:    verb = "Error";        break;
            }

            String psiStr = String((int)m.currentPSI) + " PSI";
            drawTwoLineCentered_(verb, 1, psiStr, 2, 2, topSafe_());
        }

        void TA_Display::drawManual(const DisplayModel& m) {
            drawBatteryIcon_(m.batteryPercent);
            drawConnectionIcon_(m.link);

            // Left=cancel, Down=vent, Up=airup
            drawButtonHints_(Icons::icon_cancel_6x6, Icons::icon_arrow_down_6x6, Icons::icon_arrow_up_6x6, nullptr);

            const char* txt = "Manual";
            if (m.ctrl == Ctrl::AirUp) txt = "Inflating...";
            else if (m.ctrl == Ctrl::Venting) txt = "Deflating...";

            drawCenteredText_(txt, 1, centerYBetween_(0, topSafe_(), d_.height()));
        }

        const char* TA_Display::shortError_(uint8_t code) const {
            return ta::errors::shortText(code);
        }

        void TA_Display::drawError(const DisplayModel& m) {
            drawBatteryIcon_(m.batteryPercent);
            drawConnectionIcon_(m.link);

            // Right = acknowledge
            drawButtonHints_(nullptr, nullptr, nullptr, Icons::icon_arrow_right_6x6);

            const char* desc = shortError_(m.lastErrorCode);
            String msg = desc;
            if (strcmp(desc, "Error") == 0) {
                msg = "E:";
                msg += String((int)m.lastErrorCode);
            }

            // Auto-size large, fallback to small
            int16_t w, h; measure_(msg, 2, w, h);
            uint8_t size = (w > d_.width()) ? 1 : 2;
            drawCenteredText_(msg, size, centerYBetween_(0, topSafe_(), d_.height()));
        }

        void TA_Display::drawPairing(const DisplayModel& m) {
            drawBatteryIcon_(m.batteryPercent);
            // No connection icon: pairing precedes link
            // Right button = cancel
            drawButtonHints_(nullptr, nullptr, nullptr, Icons::icon_cancel_6x6);

            const char* line = "Pairing";
            if (m.pairingFailed) {
                line = m.pairingBusy ? "Device Busy" : "No Device";
            }

            // Simple dot animation while active
            char buf[16];
            if (m.pairingActive && !m.pairingFailed) {
                uint8_t dots = (millis() / 500) % 4;
                snprintf(buf, sizeof(buf), "Pairing%.*s", dots, "...");
                line = buf;
            }

            drawCenteredText_(line, 1, centerYBetween_(0, topSafe_(), d_.height()));
        }
    } // namespace display
} // namespace ta