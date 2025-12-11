/**
 * @file TA_Display.cpp
 * @brief Implementation of DisplayController for TrailAir OLED screen
 */
#include "TA_Display.h"
#include "TA_DisplayIcons.h"
#include <TA_Time.h>

namespace trailair {
    namespace display {

        bool DisplayController::begin(uint8_t i2cAddress, bool showBootLogo) {
            // Note: caller should have constructed Adafruit_SSD1306 with width/height/Wire/reset already
            if (!_display.begin(SSD1306_SWITCHCAPVCC, i2cAddress)) {
                return false;
            }
            _display.clearDisplay();
            if (showBootLogo && Icons::logo_bmp && Icons::LogoW && Icons::LogoH) {
                // Use blocking version during begin() - happens once at startup
                logoWipe(Icons::logo_bmp, Icons::LogoW, Icons::LogoH, true, 5);
                // Clear display after logo animation completes
                _display.clearDisplay();
                _display.display();
            } else {
                _display.display();
            }
            return true;
        }

        void DisplayController::drawLogo(const uint8_t* logoBitmap, uint8_t width, uint8_t height) {
            _display.clearDisplay();
            int x = (_display.width()  - width) / 2;
            int y = (_display.height() - height) / 2;
            _display.drawBitmap(x, y, logoBitmap, width, height, SSD1306_WHITE);
            _display.display();
        }

        void DisplayController::logoWipe(const uint8_t* logoBitmap, uint8_t width, uint8_t height, bool wipeIn, uint16_t stepDelayMilliseconds) {
            int x = (_display.width()  - width) / 2;
            int y = (_display.height() - height) / 2;

            _display.clearDisplay();
            for (int col = 0; col <= width; ++col) {
                _display.drawBitmap(x, y, logoBitmap, width, height, SSD1306_WHITE);
                if (wipeIn) {
                    // Mask the right side, revealing only the left pixels
                    _display.fillRect(x + col, y, width - col, height, SSD1306_BLACK);
                } else {
                    // Mask the left side, hiding the left pixels
                    _display.fillRect(x, y, col, height, SSD1306_BLACK);
                }
                _display.display();
                delay(stepDelayMilliseconds);
            }
        }

        // Non-blocking animation API
        void DisplayController::startLogoWipe(const uint8_t* logoBitmap, uint8_t width, uint8_t height, bool wipeIn, uint16_t stepDelayMilliseconds) {
            _wipeAnimationState.active = true;
            _wipeAnimationState.logoBitmap = logoBitmap;
            _wipeAnimationState.width = width;
            _wipeAnimationState.height = height;
            _wipeAnimationState.wipeIn = wipeIn;
            _wipeAnimationState.stepDelayMilliseconds = stepDelayMilliseconds;
            _wipeAnimationState.currentColumn = 0;
            // Set to past time to ensure first frame draws immediately
            _wipeAnimationState.lastStepMilliseconds = trailair::time::getMilliseconds() - stepDelayMilliseconds;
            
            // Draw initial frame immediately
            updateLogoWipe();
        }

        void DisplayController::updateLogoWipe() {
            if (!_wipeAnimationState.active) return;
            
            uint32_t now = trailair::time::getMilliseconds();
            
            // Check if it's time for next frame
            if (!trailair::time::hasElapsed(now, _wipeAnimationState.lastStepMilliseconds, _wipeAnimationState.stepDelayMilliseconds)) {
                return;  // Not time yet
            }
            
            // For zero delay, draw one frame per call
            // For non-zero delay, draw all ready frames (catch up if behind)
            bool continueDrawing = true;
            while (_wipeAnimationState.active && continueDrawing) {
                // Calculate logo position
                int x = (_display.width() - _wipeAnimationState.width) / 2;
                int y = (_display.height() - _wipeAnimationState.height) / 2;
                
                // Draw current frame
                _display.clearDisplay();
                _display.drawBitmap(x, y, _wipeAnimationState.logoBitmap, _wipeAnimationState.width, _wipeAnimationState.height, SSD1306_WHITE);
                
                if (_wipeAnimationState.wipeIn) {
                    // Mask the right side, revealing only the left pixels
                    _display.fillRect(x + _wipeAnimationState.currentColumn, y, _wipeAnimationState.width - _wipeAnimationState.currentColumn, _wipeAnimationState.height, SSD1306_BLACK);
                } else {
                    // Mask the left side, hiding the left pixels
                    _display.fillRect(x, y, _wipeAnimationState.currentColumn, _wipeAnimationState.height, SSD1306_BLACK);
                }
                _display.display();
                
                // Advance to next step
                _wipeAnimationState.currentColumn++;
                
                // Update timing - different strategies for zero vs non-zero delay
                if (_wipeAnimationState.stepDelayMilliseconds == 0) {
                    // Zero delay: one frame per call, advance time to prevent immediate re-trigger
                    _wipeAnimationState.lastStepMilliseconds = now;
                    continueDrawing = false;  // Draw only one frame
                } else {
                    // Non-zero delay: advance by step amount for precise timing
                    _wipeAnimationState.lastStepMilliseconds += _wipeAnimationState.stepDelayMilliseconds;
                    // Check if more frames are ready
                    continueDrawing = trailair::time::hasElapsed(now, _wipeAnimationState.lastStepMilliseconds, _wipeAnimationState.stepDelayMilliseconds);
                }
                
                // Check if animation is complete
                if (_wipeAnimationState.currentColumn > _wipeAnimationState.width) {
                    _wipeAnimationState.active = false;
                }
            }
        }

        bool DisplayController::isLogoWipeActive() const {
            return _wipeAnimationState.active;
        }

        void DisplayController::drawCriticalBattery() {
            _display.clearDisplay();
            _display.setTextSize(1);
            _display.setTextColor(SSD1306_WHITE);
            
            String message = "Charge Battery";
            int16_t width, height;
            measureTextDimensions(message, 1, width, height);
            int x = calculateCenterX(width);
            int y = calculateCenterYBetween(height, 0, _display.height());
            
            _display.setCursor(x, y);
            _display.print(message);
            _display.display();
        }

        void DisplayController::render(const DisplayModel& model) {
            _display.clearDisplay();
            switch (model.viewType) {
                case ViewType::Disconnected: renderDisconnectedView(model); break;
                case ViewType::Idle:         renderIdleView(model);         break;
                case ViewType::Manual:       renderManualView(model);       break;
                case ViewType::Seeking:      renderSeekingView(model);      break;
                case ViewType::Error:        renderErrorView(model);        break;
                case ViewType::Pairing:      renderPairingView(model);      break;
            }
            _display.display();
        }

        void DisplayController::drawBatteryIcon(int percentage) {
            const int BATTERY_X = 0;
            const int BATTERY_Y = 0;
            const int BATTERY_WIDTH = 12;
            const int BATTERY_HEIGHT = 6;
            const int BATTERY_TIP_OFFSET_Y = 2;
            const int BATTERY_TIP_WIDTH = 1;
            const int BATTERY_TIP_HEIGHT = 2;
            const int LOW_BATTERY_THRESHOLD = 15;
            
            int fillWidth = (int)((constrain(percentage, 0, 98) / 100.0f) * (BATTERY_WIDTH - 2));

            _display.drawRect(BATTERY_X, BATTERY_Y, BATTERY_WIDTH, BATTERY_HEIGHT, SSD1306_WHITE);
            _display.drawRect(BATTERY_X + BATTERY_WIDTH, BATTERY_Y + BATTERY_TIP_OFFSET_Y, 
                            BATTERY_TIP_WIDTH, BATTERY_TIP_HEIGHT, SSD1306_WHITE);
            _display.fillRect(BATTERY_X + 1, BATTERY_Y + 1, fillWidth, BATTERY_HEIGHT - 2, SSD1306_WHITE);

            if (percentage < LOW_BATTERY_THRESHOLD) {
                _display.setTextSize(1);
                _display.setTextColor(SSD1306_WHITE);
                _display.setCursor(BATTERY_X + BATTERY_WIDTH + 2, BATTERY_Y);
                _display.print("!");
            }
        }

        void DisplayController::drawConnectionIcon(ConnectionStatus status) {
            const int ICON_X = _display.width() - 8;
            const int ICON_Y = 1;
            const int ICON_WIDTH = 8;
            const int ICON_HEIGHT = 6;
            
            if (status == ConnectionStatus::Connected) {
                _display.drawBitmap(ICON_X, ICON_Y, Icons::icon_connected_8x6, ICON_WIDTH, ICON_HEIGHT, SSD1306_WHITE);
            } else {
                _display.drawBitmap(ICON_X, ICON_Y, Icons::icon_disconnected_8x6, ICON_WIDTH, ICON_HEIGHT, SSD1306_WHITE);
            }
        }

        void DisplayController::drawButtonHints(const uint8_t* leftIcon, const uint8_t* downIcon, const uint8_t* upIcon, const uint8_t* rightIcon) {
            const int iconSize = _styleConfig.buttonIconSize;
            const int cellWidth = _display.width() / 4;
            const int yPosition = _display.height() - iconSize;
            const int iconOffset = (cellWidth - iconSize) / 2;
            
            if (leftIcon)  _display.drawBitmap(0   + iconOffset, yPosition, leftIcon,  iconSize, iconSize, SSD1306_WHITE);
            if (downIcon)  _display.drawBitmap(32  + iconOffset, yPosition, downIcon,  iconSize, iconSize, SSD1306_WHITE);
            if (upIcon)    _display.drawBitmap(64  + iconOffset, yPosition, upIcon,    iconSize, iconSize, SSD1306_WHITE);
            if (rightIcon) _display.drawBitmap(96  + iconOffset, yPosition, rightIcon, iconSize, iconSize, SSD1306_WHITE);
        }

        // Layout helpers
        int DisplayController::getTopSafeArea() const { 
            return _styleConfig.statusRowHeight; 
        }
        
        int DisplayController::getBottomSafeArea() const { 
            return _display.height() - _styleConfig.buttonIconSize - 2;  // Reserve space for button hints + 2px margin
        }

        void DisplayController::measureTextDimensions(const String& text, uint8_t fontSize, int16_t& width, int16_t& height) {
            int16_t boundsX, boundsY; 
            uint16_t boundsWidth, boundsHeight;
            _display.setTextSize(fontSize);
            _display.getTextBounds(text, 0, 0, &boundsX, &boundsY, &boundsWidth, &boundsHeight);
            width = (int16_t)boundsWidth; 
            height = (int16_t)boundsHeight;
        }
        
        int DisplayController::calculateCenterX(int width) const { 
            return (_display.width() - width) / 2; 
        }
        
        int DisplayController::calculateCenterYBetween(int height, int topBound, int bottomBound) const {
            int availableSpace = bottomBound - topBound; 
            return topBound + (availableSpace - height) / 2;
        }
        
        void DisplayController::drawCenteredText(const String& text, uint8_t fontSize, int yPosition) {
            int16_t width, height; 
            measureTextDimensions(text, fontSize, width, height);
            int xPosition = calculateCenterX(width);
            _display.setTextSize(fontSize);
            _display.setTextColor(SSD1306_WHITE);
            _display.setCursor(xPosition, yPosition);
            _display.print(text);
        }
        
        void DisplayController::drawTwoLinesCentered(const String& topLine, uint8_t topFontSize,
                                                     const String& bottomLine, uint8_t bottomFontSize,
                                                     int lineSpacing, int topClamp) {
            int16_t topWidth, topHeight, bottomWidth, bottomHeight;
            measureTextDimensions(topLine, topFontSize, topWidth, topHeight);
            measureTextDimensions(bottomLine, bottomFontSize, bottomWidth, bottomHeight);
            
            int totalHeight = topHeight + lineSpacing + bottomHeight;
            int yStart = calculateCenterYBetween(totalHeight, topClamp, _display.height());
            if (yStart < topClamp) yStart = topClamp;
            
            _display.setTextColor(SSD1306_WHITE);
            _display.setTextSize(topFontSize);
            _display.setCursor(calculateCenterX(topWidth), yStart);
            _display.print(topLine);
            
            _display.setTextSize(bottomFontSize);
            _display.setCursor(calculateCenterX(bottomWidth), yStart + topHeight + lineSpacing);
            _display.print(bottomLine);
        }

        void DisplayController::drawTwoColumnValues(const String& leftValue, const String& rightValue, uint8_t textSize, uint8_t gapWidth) {
            _display.setTextColor(SSD1306_WHITE);
            
            int16_t leftWidth, leftHeight, rightWidth, rightHeight;
            measureTextDimensions(leftValue, textSize, leftWidth, leftHeight);
            measureTextDimensions(rightValue, textSize, rightWidth, rightHeight);
            
            int centerY = calculateCenterYBetween(leftHeight, getTopSafeArea(), getBottomSafeArea());
            int midpoint = _display.width() / 2;
            
            // Calculate column boundaries
            int leftCellStart = 0;
            int leftCellEnd = midpoint - gapWidth/2;
            int rightCellStart = midpoint + gapWidth/2;
            int rightCellEnd = _display.width();
            
            // Center text within each column
            int leftX = leftCellStart + (leftCellEnd - leftCellStart - leftWidth) / 2;
            if (leftX < leftCellStart) leftX = leftCellStart;
            
            int rightX = rightCellStart + (rightCellEnd - rightCellStart - rightWidth) / 2;
            if (rightX < rightCellStart) rightX = rightCellStart;
            
            _display.setTextSize(textSize);
            _display.setCursor(leftX, centerY); 
            _display.print(leftValue);
            _display.setCursor(rightX, centerY); 
            _display.print(rightValue);
            
            // Underline right value
            int underlineY = centerY + rightHeight;
            if (underlineY < _display.height()) {
                _display.drawLine(rightX, underlineY, rightX + rightWidth, underlineY, SSD1306_WHITE);
            }
            
            // Direction arrow between columns
            const int ARROW_WIDTH = 9;
            const int ARROW_HEIGHT = 10;  // 5 pixels above and below center
            int arrowX = midpoint - 5;
            int arrowY = _display.height() / 2;
            _display.fillTriangle(arrowX, arrowY - 5, arrowX, arrowY + 5, arrowX + ARROW_WIDTH, arrowY, SSD1306_WHITE);
        }

        void DisplayController::renderDisconnectedView(const DisplayModel& model) {
            if (model.showBatteryIcon) {
                drawBatteryIcon(model.batteryPercentage);
            }

            // Center 20x20 icon, keeping top status row clear
            const uint8_t* iconBitmap = (model.connectionStatus == ConnectionStatus::Connected) 
                ? Icons::icon_connected_20x20 
                : Icons::icon_disconnected_20x20;
            const int ICON_WIDTH = 20;
            const int ICON_HEIGHT = 20;
            const int xPosition = calculateCenterX(ICON_WIDTH);
            const int yPosition = calculateCenterYBetween(ICON_HEIGHT, getTopSafeArea(), getBottomSafeArea());
            _display.drawBitmap(xPosition, yPosition, iconBitmap, ICON_WIDTH, ICON_HEIGHT, SSD1306_WHITE);

            // Right button hint (retry) when disconnected
            if (model.connectionStatus == ConnectionStatus::Disconnected && model.showReconnectHint) {
                drawButtonHints(nullptr, nullptr, nullptr, Icons::icon_arrow_right_6x6);
            }
        }

        void DisplayController::renderIdleView(const DisplayModel& model) {
            if (model.showBatteryIcon) {
                drawBatteryIcon(model.batteryPercentage);
            }
            drawConnectionIcon(model.connectionStatus);
            drawButtonHints(Icons::icon_manual_control_6x6, Icons::icon_dash_6x6, 
                          Icons::icon_plus_6x6, Icons::icon_arrow_right_6x6);
            
            String currentPressure = String((int)model.currentPressurePSI);
            String targetPressure  = String((int)model.targetPressurePSI);
            drawTwoColumnValues(currentPressure, targetPressure, _styleConfig.valueTextSize, _styleConfig.columnGap);
        }

        void DisplayController::renderSeekingView(const DisplayModel& model) {
            if (model.showBatteryIcon) {
                drawBatteryIcon(model.batteryPercentage);
            }
            drawConnectionIcon(model.connectionStatus);

            // Right=Cancel
            drawButtonHints(nullptr, nullptr, nullptr, Icons::icon_cancel_6x6);

            if (model.seekingShowDoneHold) {
                drawCenteredText("Done!", 2, calculateCenterYBetween(0, getTopSafeArea(), getBottomSafeArea()));
                return;
            }

            const char* statusVerb = "Ready";
            switch (model.controllerActivity) {
                case ControllerActivity::Idle:     statusVerb = "Ready";        break;
                case ControllerActivity::AirUp:    statusVerb = "Inflating..."; break;
                case ControllerActivity::Venting:  statusVerb = "Deflating..."; break;
                case ControllerActivity::Checking: statusVerb = "Checking...";  break;
                case ControllerActivity::Error:    statusVerb = "Error";        break;
            }

            String pressureText = String((int)model.currentPressurePSI) + " PSI";
            drawTwoLinesCentered(statusVerb, 1, pressureText, 2, 2, getTopSafeArea());
        }

        void DisplayController::renderManualView(const DisplayModel& model) {
            if (model.showBatteryIcon) {
                drawBatteryIcon(model.batteryPercentage);
            }
            drawConnectionIcon(model.connectionStatus);

            // Left=cancel, Down=vent, Up=airup
            drawButtonHints(Icons::icon_cancel_6x6, Icons::icon_arrow_down_6x6, 
                          Icons::icon_arrow_up_6x6, nullptr);

            const char* statusText = "Manual";
            if (model.controllerActivity == ControllerActivity::AirUp) {
                statusText = "Inflating...";
            } else if (model.controllerActivity == ControllerActivity::Venting) {
                statusText = "Deflating...";
            }

            // Show status text and current PSI
            String pressureText = String((int)model.currentPressurePSI) + " PSI";
            drawTwoLinesCentered(statusText, 1, pressureText, 2, 2, getTopSafeArea());
        }

        const char* DisplayController::getShortErrorDescription(uint8_t errorCode) const {
            return trailair::errors::getShortDescription(errorCode);
        }

        void DisplayController::renderErrorView(const DisplayModel& model) {
            if (model.showBatteryIcon) {
                drawBatteryIcon(model.batteryPercentage);
            }
            drawConnectionIcon(model.connectionStatus);

            // Right = acknowledge
            drawButtonHints(nullptr, nullptr, nullptr, Icons::icon_arrow_right_6x6);

            const char* description = getShortErrorDescription(model.lastErrorCode);
            String errorMessage = description;
            if (strcmp(description, "Error") == 0) {
                errorMessage = "E:";
                errorMessage += String((int)model.lastErrorCode);
            }

            // Auto-size: use large font, fallback to small if text too wide
            int16_t textWidth, textHeight; 
            measureTextDimensions(errorMessage, 2, textWidth, textHeight);
            uint8_t fontSize = (textWidth > _display.width()) ? 1 : 2;
            drawCenteredText(errorMessage, fontSize, calculateCenterYBetween(0, getTopSafeArea(), getBottomSafeArea()));
        }

        void DisplayController::renderPairingView(const DisplayModel& model) {
            if (model.showBatteryIcon) {
                drawBatteryIcon(model.batteryPercentage);
            }
            // No connection icon: pairing precedes connection
            
            // Right button = cancel
            drawButtonHints(nullptr, nullptr, nullptr, Icons::icon_cancel_6x6);

            const char* statusLine = "Pairing";
            if (model.pairingFailed) {
                statusLine = model.pairingBusy ? "Device Busy" : "No Device";
            }

            // Simple dot animation while active
            char animationBuffer[16];
            if (model.pairingActive && !model.pairingFailed) {
                uint8_t dotCount = (millis() / 500) % 4;
                snprintf(animationBuffer, sizeof(animationBuffer), "Pairing%.*s", dotCount, "...");
                statusLine = animationBuffer;
            }

            drawCenteredText(statusLine, 1, calculateCenterYBetween(0, getTopSafeArea(), getBottomSafeArea()));
        }
        
    } // namespace display
} // namespace trailair
