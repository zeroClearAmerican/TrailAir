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
            if (showBootLogo && Icons::LogoBMP && Icons::LogoW && Icons::LogoH) {
                logoWipe(Icons::LogoBMP, Icons::LogoW, Icons::LogoH, true, 10);
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
            }
            d_.display();
        }

        void TA_Display::drawBatteryIcon_(int percent) {
            int batteryX = 0;
            int batteryY = 0;
            int batteryW = 12;
            int batteryH = 6;
            int fillW = (int)((constrain(percent, 0, 100) / 100.0f) * (batteryW - 2));

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
                d_.drawBitmap(connX, connY, Icons::Conn8x6, 8, 6, SSD1306_WHITE);
            } else {
                d_.drawBitmap(connX, connY, Icons::Disconn8x6, 8, 6, SSD1306_WHITE);
            }
        }

        void TA_Display::drawButtonHints_(const uint8_t* left, const uint8_t* down, const uint8_t* up, const uint8_t* right) {
            const int iconSize = 6;
            const int cellW = 32;
            const int y = d_.height() - iconSize;
            const int offset = (cellW - iconSize) / 2;

            if (left)  d_.drawBitmap(0   + offset, y, left,  iconSize, iconSize, SSD1306_WHITE);
            if (down)  d_.drawBitmap(32  + offset, y, down,  iconSize, iconSize, SSD1306_WHITE);
            if (up)    d_.drawBitmap(64  + offset, y, up,    iconSize, iconSize, SSD1306_WHITE);
            if (right) d_.drawBitmap(96  + offset, y, right, iconSize, iconSize, SSD1306_WHITE);
        }

        void TA_Display::drawDisconnected(const DisplayModel& m) {
            drawBatteryIcon_(m.batteryPercent);

            // Centered 20x20 icon
            const uint8_t* bmp = (m.link == Link::Connected) ? Icons::Conn20x20 : Icons::Disconn20x20;
            const uint8_t w = 20, h = 20;
            const int16_t x = (d_.width()  - w) / 2;
            const int16_t y = (d_.height() - h) / 2;
            d_.drawBitmap(x, y, bmp, w, h, SSD1306_WHITE);

            // Right button hint (retry) when disconnected
            if (m.link == Link::Disconnected && m.showReconnectHint) {
                drawButtonHints_(nullptr, nullptr, nullptr, Icons::ArrowRight6);
            }
        }

        void TA_Display::drawIdle(const DisplayModel& m) {
            drawBatteryIcon_(m.batteryPercent);
            drawConnectionIcon_(m.link);

            // Hints: Left=manual, Down=decrease, Up=increase, Right=start
            drawButtonHints_(Icons::Manual6, Icons::Dash6, Icons::Plus6, Icons::ArrowRight6);

            String currentStr = String((int)m.currentPSI);
            String targetStr  = String((int)m.targetPSI);

            d_.setTextColor(SSD1306_WHITE);
            d_.setTextSize(2);

            int16_t bx, by; uint16_t bw, bh;
            d_.getTextBounds(currentStr, 0, 0, &bx, &by, &bw, &bh);
            uint16_t curW = bw, curH = bh;
            d_.getTextBounds(targetStr, 0, 0, &bx, &by, &bw, &bh);
            uint16_t tgtW = bw, tgtH = bh;

            int leftX0 = 0, leftX1 = (d_.width() / 2) - 8;
            int rightX0 = (d_.width() / 2) + 8, rightX1 = d_.width();
            int centerY = (d_.height() - 16) / 2;
            if (centerY < 0) centerY = 0;

            int curX = leftX0 + (leftX1 - leftX0 - (int)curW) / 2;
            if (curX < 0) curX = 0;
            d_.setCursor(curX, centerY);
            d_.print(currentStr);

            int tgtX = rightX0 + (rightX1 - rightX0 - (int)tgtW) / 2;
            if (tgtX < rightX0) tgtX = rightX0;
            d_.setCursor(tgtX, centerY);
            d_.print(targetStr);

            int underlineY = centerY + (int)tgtH;
            if (underlineY < d_.height()) {
                d_.drawLine(tgtX, underlineY, tgtX + (int)tgtW, underlineY, SSD1306_WHITE);
            }

            int ax = (d_.width() / 2) - 5;
            int ay = d_.height() / 2;
            d_.fillTriangle(ax, ay - 5, ax, ay + 5, ax + 9, ay, SSD1306_WHITE);
        }

        void TA_Display::drawSeeking(const DisplayModel& m) {
            drawBatteryIcon_(m.batteryPercent);
            drawConnectionIcon_(m.link);

            // Right=Cancel
            drawButtonHints_(nullptr, nullptr, nullptr, Icons::Cancel6);

            if (m.seekingShowDoneHold) {
                const char* doneTxt = "Done!";
                d_.setTextColor(SSD1306_WHITE);
                d_.setTextSize(2);
                int16_t bx, by; uint16_t bw, bh;
                d_.getTextBounds(doneTxt, 0, 0, &bx, &by, &bw, &bh);
                int x = (d_.width() - (int)bw) / 2;
                int y = (d_.height() - (int)bh) / 2;
                if (y < 8) y = 8;
                d_.setCursor(x, y);
                d_.print(doneTxt);
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

            int16_t bx, by; uint16_t bw1, bh1, bw2, bh2;
            d_.setTextColor(SSD1306_WHITE);
            d_.setTextSize(1);
            d_.getTextBounds(verb, 0, 0, &bx, &by, &bw1, &bh1);
            d_.setTextSize(2);
            d_.getTextBounds(psiStr, 0, 0, &bx, &by, &bw2, &bh2);

            int totalH = (int)bh1 + 2 + (int)bh2;
            int yStart = (d_.height() - totalH) / 2;
            if (yStart < 8) yStart = 8;

            int xVerb = (d_.width() - (int)bw1) / 2;
            d_.setTextSize(1);
            d_.setCursor(xVerb, yStart);
            d_.print(verb);

            int xPSI = (d_.width() - (int)bw2) / 2;
            int yPSI = yStart + (int)bh1 + 2;
            d_.setTextSize(2);
            d_.setCursor(xPSI, yPSI);
            d_.print(psiStr);
        }

        void TA_Display::drawManual(const DisplayModel& m) {
            drawBatteryIcon_(m.batteryPercent);
            drawConnectionIcon_(m.link);

            // Left=cancel, Down=vent, Up=airup
            drawButtonHints_(Icons::Cancel6, Icons::ArrowDown6, Icons::ArrowUp6, nullptr);

            const char* txt = "Manual";
            if (m.ctrl == Ctrl::AirUp) txt = "Inflating...";
            else if (m.ctrl == Ctrl::Venting) txt = "Deflating...";

            d_.setTextColor(SSD1306_WHITE);
            d_.setTextSize(1);
            int16_t bx, by; uint16_t bw, bh;
            d_.getTextBounds(txt, 0, 0, &bx, &by, &bw, &bh);
            int x = (d_.width() - (int)bw) / 2;
            int y = (d_.height() - (int)bh) / 2;
            if (y < 8) y = 8;
            d_.setCursor(x, y);
            d_.print(txt);
        }

        const char* TA_Display::shortError_(uint8_t code) const {
            switch (code) {
                case 0:   return "None";
                case 1:   return "No change";
                case 2:   return "Too slow";
                case 3:   return "Sensor";
                case 4:   return "Over PSI";
                case 5:   return "Under PSI";
                case 6:   return "Conflict";
                case 255: return "Unknown";
                default:  return "Error";
            }
        }

        void TA_Display::drawError(const DisplayModel& m) {
            drawBatteryIcon_(m.batteryPercent);
            drawConnectionIcon_(m.link);

            // Right = acknowledge
            drawButtonHints_(nullptr, nullptr, nullptr, Icons::ArrowRight6);

            const char* desc = shortError_(m.lastErrorCode);
            String msg = desc;
            if (strcmp(desc, "Error") == 0) {
                msg = "E:";
                msg += String((int)m.lastErrorCode);
            }

            d_.setTextColor(SSD1306_WHITE);
            d_.setTextSize(2);
            int16_t bx, by; uint16_t bw, bh;
            d_.getTextBounds(msg, 0, 0, &bx, &by, &bw, &bh);
            if (bw > d_.width()) {
                d_.setTextSize(1);
                d_.getTextBounds(msg, 0, 0, &bx, &by, &bw, &bh);
            }
            int x = (d_.width() - (int)bw) / 2;
            int y = (d_.height() - (int)bh) / 2;
            if (y < 8) y = 8;
            d_.setCursor(x, y);
            d_.print(msg);
        }

    } // namespace display
} // namespace ta