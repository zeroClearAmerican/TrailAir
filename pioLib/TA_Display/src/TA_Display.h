#pragma once
#include <Arduino.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <TA_Errors.h>

namespace ta {
    namespace display {

        // High-level view selection (maps 1:1 to your current screens)
        enum class View {
            Disconnected,
            Idle,
            Manual,
            Seeking,
            Error,
            Pairing
        };

        // Link status and controller activity (kept display-local to avoid coupling)
        enum class Link { Disconnected, Connected };
        enum class Ctrl  { Idle, AirUp, Venting, Checking, Error };

        // Single struct the app/state layer fills each frame
        struct DisplayModel {
            // Status
            int batteryPercent = 0;         // 0..100
            Link link = Link::Disconnected; // connection icon
            Ctrl ctrl = Ctrl::Idle;         // controller activity for verb text
            View view = View::Disconnected; // which screen to render

            // Data
            float currentPSI = 0.0f;
            float targetPSI = 0.0f;

            // Flags
            bool seekingShowDoneHold = false; // “Done!” hold during Seeking
            uint8_t lastErrorCode = 0;        // for Error screen
            bool showReconnectHint = false;   // show right-arrow on Disconnected

            // Pairing flags (remote only)
            bool pairingActive = false;
            bool pairingFailed = false;   // timeout / canceled / busy
            bool pairingBusy = false;     // board reported Busy
        };

        class TA_Display {
            public:
                // Style tokens to standardize spacing/sizes
                struct Style {
                    uint8_t statusRowH = 8;
                    uint8_t btnIcon = 6;
                    uint8_t colGap = 16;
                    uint8_t valueTextSize = 2;
                };

                explicit TA_Display(Adafruit_SSD1306& d) : d_(d) {}

                // Initialize the display (call once from setup)
                // Returns true on success
                bool begin(uint8_t i2cAddr = 0x3C, bool showBootLogo = true);

                // Optional: draw centered logo and a left-to-right wipe animation
                void drawLogo(const uint8_t* logo, uint8_t w, uint8_t h);
                
                // Blocking version (for backward compatibility)
                void logoWipe(const uint8_t* logo, uint8_t w, uint8_t h, bool wipeIn, uint16_t stepDelayMs);
                
                // Non-blocking animation API
                void startLogoWipe(const uint8_t* logo, uint8_t w, uint8_t h, bool wipeIn, uint16_t stepDelayMs);
                void updateLogoWipe();  // Call from loop to advance animation
                bool isLogoWipeActive() const;

                // Critical battery warning (called before forced sleep)
                void drawCriticalBattery();

                // Main render entrypoint; call every loop with current model
                void render(const DisplayModel& m);

                private:
                // Screen painters
                void drawDisconnected(const DisplayModel& m);
                void drawIdle(const DisplayModel& m);
                void drawManual(const DisplayModel& m);
                void drawSeeking(const DisplayModel& m);
                void drawError(const DisplayModel& m);
                void drawPairing(const DisplayModel& m);

                // Widgets
                void drawBatteryIcon_(int percent);
                void drawConnectionIcon_(Link link);
                void drawButtonHints_(const uint8_t* left, const uint8_t* down, const uint8_t* up, const uint8_t* right);

                // Helpers
                const char* shortError_(uint8_t code) const;
                // Layout helpers (to reduce repeated getTextBounds/centering math)
                int topSafe_() const; // space for status row
                void measure_(const String& s, uint8_t size, int16_t& w, int16_t& h);
                int centerX_(int w) const;
                int centerYBetween_(int h, int top, int bottom) const;
                void drawCenteredText_(const String& s, uint8_t size, int y);
                void drawTwoLineCentered_(const String& top, uint8_t topSize,
                                          const String& bottom, uint8_t bottomSize,
                                          int spacing, int topClamp);
                void drawTwoColumnValues_(const String& left, const String& right, uint8_t textSize, uint8_t gap);

            private:
                Adafruit_SSD1306& d_;
                Style style_{};
                
                // Non-blocking animation state
                struct {
                    bool active = false;
                    const uint8_t* logo = nullptr;
                    uint8_t w = 0;
                    uint8_t h = 0;
                    bool wipeIn = true;
                    uint16_t stepDelayMs = 0;
                    int currentCol = 0;
                    uint32_t lastStepMs = 0;
                } wipeState_;
        };

    } // namespace display
} // namespace ta