#pragma once
#include <Arduino.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <TA_Errors.h>

/**
 * @file TA_Display.h
 * @brief Display controller for TrailAir OLED screen
 * 
 * Provides high-level rendering API for the SSD1306 OLED display, including:
 * - Multiple view screens (Disconnected, Idle, Manual, Seeking, Error, Pairing)
 * - Status icons (battery, connection)
 * - Button hints
 * - Logo animations (blocking and non-blocking)
 * 
 * The display operates on a simple model-view pattern where the application
 * fills a DisplayModel struct each frame and calls render().
 */

namespace trailair {
    namespace display {

        /**
         * @brief High-level view/screen selection
         * 
         * Defines which screen layout to render. Maps 1:1 to application states.
         */
        enum class ViewType {
            Disconnected,  ///< Waiting for connection to control board
            Idle,          ///< Connected, showing current/target pressure
            Manual,        ///< Manual control mode (user-controlled inflate/deflate)
            Seeking,       ///< Automatic seeking to target pressure
            Error,         ///< Error condition display
            Pairing        ///< Device pairing mode
        };

        /**
         * @brief Connection status for display purposes
         * 
         * Local to display layer to avoid coupling with communications layer.
         */
        enum class ConnectionStatus { 
            Disconnected,  ///< No wireless connection
            Connected      ///< Wireless connection established
        };
        
        /**
         * @brief Controller activity state for display purposes
         * 
         * Indicates what the pressure controller is currently doing.
         */
        enum class ControllerActivity  { 
            Idle,      ///< Controller inactive
            AirUp,     ///< Actively inflating (compressor running)
            Venting,   ///< Actively deflating (vent open)
            Checking,  ///< Settling/checking pressure
            Error      ///< Controller in error state
        };

        /**
         * @brief Display data model filled by application each frame
         * 
         * Single struct containing all data needed to render the current display state.
         * The application/state layer populates this and passes to render().
         */
        struct DisplayModel {
            // Status indicators
            int batteryPercentage = 0;                             ///< Battery level 0-100
            bool showBatteryIcon = true;                           ///< Whether to show battery icon (false for externally powered boards)
            ConnectionStatus connectionStatus = ConnectionStatus::Disconnected;  ///< Connection icon state
            ControllerActivity controllerActivity = ControllerActivity::Idle;    ///< Controller activity for status text
            ViewType viewType = ViewType::Disconnected;            ///< Which screen to render

            // Pressure data
            float currentPressurePSI = 0.0f;                       ///< Current measured pressure
            float targetPressurePSI = 0.0f;                        ///< Desired target pressure

            // View-specific flags
            bool seekingShowDoneHold = false;                      ///< Show "Done!" message during Seeking
            uint8_t lastErrorCode = 0;                             ///< Error code for Error screen
            bool showReconnectHint = false;                        ///< Show reconnect button hint on Disconnected

            // Pairing-specific flags (remote only)
            bool pairingActive = false;                            ///< Pairing in progress
            bool pairingFailed = false;                            ///< Pairing timeout/canceled/busy
            bool pairingBusy = false;                              ///< Target board reported Busy
        };

        /**
         * @brief Display controller for SSD1306 OLED screen
         * 
         * Manages all rendering operations for the TrailAir remote display.
         * Provides both blocking and non-blocking animation APIs.
         * 
         * @example
         * ```cpp
         * Adafruit_SSD1306 oled(128, 64, &Wire, -1);
         * DisplayController display(oled);
         * 
         * display.begin(0x3C, true);  // Initialize with boot logo
         * 
         * // In main loop:
         * DisplayModel model;
         * model.batteryPercentage = 85;
         * model.connectionStatus = ConnectionStatus::Connected;
         * model.viewType = ViewType::Idle;
         * display.render(model);
         * ```
         */
        class DisplayController {
            public:
                /**
                 * @brief Style configuration for display layout
                 * 
                 * Defines standard spacing, sizes, and layout constants.
                 */
                struct StyleConfiguration {
                    uint8_t statusRowHeight = 8;     ///< Height reserved for top status row
                    uint8_t buttonIconSize = 6;      ///< Size of button hint icons
                    uint8_t columnGap = 16;          ///< Gap between dual-column values
                    uint8_t valueTextSize = 2;       ///< Font size for pressure values
                };

                /**
                 * @brief Constructs display controller
                 * @param display Reference to initialized Adafruit_SSD1306 instance
                 */
                explicit DisplayController(Adafruit_SSD1306& display) : _display(display) {}

                /**
                 * @brief Initialize the display hardware
                 * @param i2cAddress I2C address of the display (typically 0x3C)
                 * @param showBootLogo Whether to show animated boot logo on startup
                 * @return true if initialization successful, false otherwise
                 */
                bool begin(uint8_t i2cAddress = 0x3C, bool showBootLogo = true);

                /**
                 * @brief Draw centered logo (static, no animation)
                 * @param logoBitmap Pointer to bitmap data
                 * @param width Logo width in pixels
                 * @param height Logo height in pixels
                 */
                void drawLogo(const uint8_t* logoBitmap, uint8_t width, uint8_t height);
                
                /**
                 * @brief Draw logo with wipe animation (blocking)
                 * @param logoBitmap Pointer to bitmap data
                 * @param width Logo width in pixels
                 * @param height Logo height in pixels
                 * @param wipeIn true to reveal (wipe in), false to hide (wipe out)
                 * @param stepDelayMilliseconds Delay between animation frames in milliseconds
                 */
                void logoWipe(const uint8_t* logoBitmap, uint8_t width, uint8_t height, 
                             bool wipeIn, uint16_t stepDelayMilliseconds);
                
                /**
                 * @brief Start non-blocking logo wipe animation
                 * @param logoBitmap Pointer to bitmap data
                 * @param width Logo width in pixels
                 * @param height Logo height in pixels
                 * @param wipeIn true to reveal (wipe in), false to hide (wipe out)
                 * @param stepDelayMilliseconds Delay between animation frames in milliseconds
                 */
                void startLogoWipe(const uint8_t* logoBitmap, uint8_t width, uint8_t height, 
                                  bool wipeIn, uint16_t stepDelayMilliseconds);
                
                /**
                 * @brief Update non-blocking logo wipe animation
                 * 
                 * Call from main loop to advance the animation. Does nothing if no animation active.
                 */
                void updateLogoWipe();
                
                /**
                 * @brief Check if logo wipe animation is currently active
                 * @return true if animation running, false otherwise
                 */
                bool isLogoWipeActive() const;

                /**
                 * @brief Display critical battery warning before forced sleep
                 * 
                 * Shows "Charge Battery" message. Used when battery critically low.
                 */
                void drawCriticalBattery();

                /**
                 * @brief Main render entry point - call every loop with current model
                 * @param model Display data model containing all view state
                 */
                void render(const DisplayModel& model);

            private:
                // Screen rendering methods
                void renderDisconnectedView(const DisplayModel& model);
                void renderIdleView(const DisplayModel& model);
                void renderManualView(const DisplayModel& model);
                void renderSeekingView(const DisplayModel& model);
                void renderErrorView(const DisplayModel& model);
                void renderPairingView(const DisplayModel& model);

                // Widget rendering
                void drawBatteryIcon(int percentage);
                void drawConnectionIcon(ConnectionStatus status);
                void drawButtonHints(const uint8_t* leftIcon, const uint8_t* downIcon, 
                                    const uint8_t* upIcon, const uint8_t* rightIcon);

                // Helper methods
                const char* getShortErrorDescription(uint8_t errorCode) const;
                
                // Layout helpers
                int getTopSafeArea() const;
                int getBottomSafeArea() const;
                void measureTextDimensions(const String& text, uint8_t fontSize, 
                                          int16_t& width, int16_t& height);
                int calculateCenterX(int width) const;
                int calculateCenterYBetween(int height, int topBound, int bottomBound) const;
                void drawCenteredText(const String& text, uint8_t fontSize, int yPosition);
                void drawTwoLinesCentered(const String& topLine, uint8_t topFontSize,
                                         const String& bottomLine, uint8_t bottomFontSize,
                                         int lineSpacing, int topClamp);
                void drawTwoColumnValues(const String& leftValue, const String& rightValue, 
                                        uint8_t textSize, uint8_t gapWidth);

            private:
                Adafruit_SSD1306& _display;
                StyleConfiguration _styleConfig{};
                
                /**
                 * @brief State for non-blocking logo wipe animation
                 */
                struct {
                    bool active = false;
                    const uint8_t* logoBitmap = nullptr;
                    uint8_t width = 0;
                    uint8_t height = 0;
                    bool wipeIn = true;
                    uint16_t stepDelayMilliseconds = 0;
                    int currentColumn = 0;
                    uint32_t lastStepMilliseconds = 0;
                } _wipeAnimationState;
        };

    } // namespace display
} // namespace trailair
