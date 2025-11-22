/**
 * Test-only implementation of TA_Display animation functions
 * Extracts just the non-blocking animation state machine logic for testing
 * This avoids Arduino.h and Adafruit dependencies
 */

#include <cstdint>
#include <TA_Time.h>

namespace ta {
namespace display {

// Minimal display interface needed for animation testing
class IDisplay {
public:
    virtual ~IDisplay() = default;
    virtual int width() const = 0;
    virtual int height() const = 0;
    virtual void clearDisplay() = 0;
    virtual void display() = 0;
    virtual void drawBitmap(int x, int y, const uint8_t* bitmap, int w, int h, uint16_t color) = 0;
    virtual void fillRect(int x, int y, int w, int h, uint16_t color) = 0;
};

// Test-only TA_Display that uses the interface
class TA_DisplayAnim {
public:
    explicit TA_DisplayAnim(IDisplay& d) : d_(d) {}
    
    // Non-blocking animation API
    void startLogoWipe(const uint8_t* logo, uint8_t w, uint8_t h, bool wipeIn, uint16_t stepDelayMs) {
        wipeState_.active = true;
        wipeState_.logo = logo;
        wipeState_.w = w;
        wipeState_.h = h;
        wipeState_.wipeIn = wipeIn;
        wipeState_.stepDelayMs = stepDelayMs;
        wipeState_.currentCol = 0;
        wipeState_.lastStepMs = ta::time::getMillis();
        
        // Draw initial frame immediately
        updateLogoWipe();
    }

    void updateLogoWipe() {
        if (!wipeState_.active) return;
        
        uint32_t now = ta::time::getMillis();
        
        // Check if it's time for the next step
        if (!ta::time::hasElapsed(now, wipeState_.lastStepMs, wipeState_.stepDelayMs)) {
            return;  // Not time yet
        }
        
        // Calculate logo position
        int x = (d_.width() - wipeState_.w) / 2;
        int y = (d_.height() - wipeState_.h) / 2;
        
        // Draw current frame
        d_.clearDisplay();
        d_.drawBitmap(x, y, wipeState_.logo, wipeState_.w, wipeState_.h, 1);  // 1 = white
        
        if (wipeState_.wipeIn) {
            // Mask the right side, revealing only the left pixels
            d_.fillRect(x + wipeState_.currentCol, y, wipeState_.w - wipeState_.currentCol, wipeState_.h, 0);  // 0 = black
        } else {
            // Mask the left side, hiding the left pixels
            d_.fillRect(x, y, wipeState_.currentCol, wipeState_.h, 0);  // 0 = black
        }
        d_.display();
        
        // Advance to next step
        wipeState_.currentCol++;
        wipeState_.lastStepMs = now;
        
        // Check if animation is complete
        if (wipeState_.currentCol > wipeState_.w) {
            wipeState_.active = false;
        }
    }

    bool isLogoWipeActive() const {
        return wipeState_.active;
    }

private:
    IDisplay& d_;
    
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
