#pragma once
#include <Arduino.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

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
                explicit TA_Display(Adafruit_SSD1306& d) : d_(d) {}

                // Initialize the display (call once from setup)
                // Returns true on success
                bool begin(uint8_t i2cAddr = 0x3C, bool showBootLogo = true);

                // Optional: draw centered logo and a left-to-right wipe animation
                void drawLogo(const uint8_t* logo, uint8_t w, uint8_t h);
                void logoWipe(const uint8_t* logo, uint8_t w, uint8_t h, bool wipeIn, uint16_t stepDelayMs);

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

            private:
                Adafruit_SSD1306& d_;
        };

    } // namespace display
} // namespace ta