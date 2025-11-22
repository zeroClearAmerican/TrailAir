/**
 * Unit tests for TA_Display Animation State Machine
 * Tests non-blocking logoWipe animation logic without hardware rendering
 */

#define TA_TIME_TEST_MODE
#include <gtest/gtest.h>
#include <TA_Display.h>
#include <TA_Time_test.h>

using namespace ta::display;

// ============================================================================
// Mock Display Hardware - Tracks calls instead of rendering
// ============================================================================
class MockSSD1306 {
public:
    int clearDisplayCalls = 0;
    int displayCalls = 0;
    int drawBitmapCalls = 0;
    int fillRectCalls = 0;
    int beginCalls = 0;
    bool beginResult = true;
    
    // Mock dimensions
    int width_ = 128;
    int height_ = 64;
    
    // Track last operations
    struct LastBitmap {
        int x = 0, y = 0;
        const uint8_t* bitmap = nullptr;
        uint8_t w = 0, h = 0;
        uint16_t color = 0;
    } lastBitmap;
    
    struct LastRect {
        int x = 0, y = 0, w = 0, h = 0;
        uint16_t color = 0;
    } lastRect;
    
    // Mock Adafruit API
    bool begin(uint8_t switchvcc, uint8_t i2caddr) {
        beginCalls++;
        return beginResult;
    }
    
    void clearDisplay() {
        clearDisplayCalls++;
    }
    
    void display() {
        displayCalls++;
    }
    
    void drawBitmap(int16_t x, int16_t y, const uint8_t* bitmap, 
                    int16_t w, int16_t h, uint16_t color) {
        drawBitmapCalls++;
        lastBitmap = {x, y, bitmap, (uint8_t)w, (uint8_t)h, color};
    }
    
    void fillRect(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t color) {
        fillRectCalls++;
        lastRect = {x, y, w, h, color};
    }
    
    int width() const { return width_; }
    int height() const { return height_; }
    
    // Test helpers
    void reset() {
        clearDisplayCalls = displayCalls = drawBitmapCalls = fillRectCalls = 0;
        beginCalls = 0;
    }
    
    void setTextSize(uint8_t) {}
    void setTextColor(uint16_t) {}
    void setCursor(int16_t, int16_t) {}
    void print(const char*) {}
    void print(const String&) {}
    void getTextBounds(const char*, int16_t, int16_t, 
                      int16_t*, int16_t*, uint16_t*, uint16_t*) {}
    void drawRect(int16_t, int16_t, int16_t, int16_t, uint16_t) {}
};

// ============================================================================
// Test Fixture
// ============================================================================
class DisplayTest : public ::testing::Test {
protected:
    MockSSD1306 mockDisplay;
    TA_Display* display = nullptr;
    ta::time::test::MockTime mockTime;
    
    // Test logo data (8x8 pixels)
    static const uint8_t testLogo[];
    static constexpr uint8_t logoW = 8;
    static constexpr uint8_t logoH = 8;
    
    void SetUp() override {
        // Create display with mock hardware
        display = new TA_Display(*reinterpret_cast<Adafruit_SSD1306*>(&mockDisplay));
        mockDisplay.reset();
        mockTime.set(0);
    }
    
    void TearDown() override {
        delete display;
    }
    
    // Helper to advance time and update animation
    void advanceAndUpdate(uint32_t ms) {
        mockTime.advance(ms);
        display->updateLogoWipe();
    }
};

// Simple test logo (8x8, all pixels set)
const uint8_t DisplayTest::testLogo[] = {
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF
};

// ============================================================================
// Initialization Tests
// ============================================================================
TEST_F(DisplayTest, InitialState_NotActive) {
    EXPECT_FALSE(display->isLogoWipeActive());
}

TEST_F(DisplayTest, Begin_WithoutBootLogo_DoesNotStartAnimation) {
    display->begin(0x3C, false);
    EXPECT_FALSE(display->isLogoWipeActive());
}

// ============================================================================
// Animation Start Tests
// ============================================================================
TEST_F(DisplayTest, StartLogoWipe_ActivatesAnimation) {
    display->startLogoWipe(testLogo, logoW, logoH, true, 5);
    EXPECT_TRUE(display->isLogoWipeActive());
}

TEST_F(DisplayTest, StartLogoWipe_DrawsInitialFrame) {
    mockDisplay.reset();
    display->startLogoWipe(testLogo, logoW, logoH, true, 5);
    
    // Should draw initial frame immediately
    EXPECT_GT(mockDisplay.clearDisplayCalls, 0);
    EXPECT_GT(mockDisplay.drawBitmapCalls, 0);
    EXPECT_GT(mockDisplay.displayCalls, 0);
}

TEST_F(DisplayTest, StartLogoWipe_WipeIn_InitiallyMasked) {
    mockDisplay.reset();
    display->startLogoWipe(testLogo, logoW, logoH, true, 5);
    
    // Wipe-in starts with logo mostly masked (fillRect should be called)
    EXPECT_GT(mockDisplay.fillRectCalls, 0);
}

TEST_F(DisplayTest, StartLogoWipe_WipeOut_InitiallyUnmasked) {
    mockDisplay.reset();
    display->startLogoWipe(testLogo, logoW, logoH, false, 5);
    
    // Wipe-out starts unmasked, will progressively mask
    EXPECT_GT(mockDisplay.fillRectCalls, 0);
}

// ============================================================================
// Animation Progression Tests
// ============================================================================
TEST_F(DisplayTest, UpdateLogoWipe_BeforeStepDelay_DoesNotAdvance) {
    display->startLogoWipe(testLogo, logoW, logoH, true, 100);
    mockDisplay.reset();
    
    // Advance time but not enough for next step
    advanceAndUpdate(50);
    
    // Should not draw new frame
    EXPECT_EQ(mockDisplay.clearDisplayCalls, 0);
    EXPECT_TRUE(display->isLogoWipeActive());
}

TEST_F(DisplayTest, UpdateLogoWipe_AfterStepDelay_AdvancesFrame) {
    display->startLogoWipe(testLogo, logoW, logoH, true, 100);
    mockDisplay.reset();
    
    // Advance time past step delay
    advanceAndUpdate(100);
    
    // Should draw next frame
    EXPECT_GT(mockDisplay.clearDisplayCalls, 0);
    EXPECT_GT(mockDisplay.drawBitmapCalls, 0);
    EXPECT_GT(mockDisplay.displayCalls, 0);
}

TEST_F(DisplayTest, UpdateLogoWipe_MultipleSteps_ProgressesThroughAnimation) {
    display->startLogoWipe(testLogo, logoW, logoH, true, 10);
    mockDisplay.reset();
    
    // Advance through several frames
    for (int i = 0; i < 5; i++) {
        advanceAndUpdate(10);
    }
    
    // Should have drawn 5 additional frames
    EXPECT_GE(mockDisplay.clearDisplayCalls, 5);
    EXPECT_TRUE(display->isLogoWipeActive()); // Not complete yet (8 cols total)
}

TEST_F(DisplayTest, UpdateLogoWipe_CompletesAfterAllColumns) {
    display->startLogoWipe(testLogo, logoW, logoH, true, 10);
    
    // Advance through all columns (width + 1 for final frame)
    for (int i = 0; i <= logoW; i++) {
        advanceAndUpdate(10);
    }
    
    // Animation should be complete
    EXPECT_FALSE(display->isLogoWipeActive());
}

// ============================================================================
// Animation Completion Tests
// ============================================================================
TEST_F(DisplayTest, IsLogoWipeActive_ReturnsFalse_AfterCompletion) {
    display->startLogoWipe(testLogo, logoW, logoH, true, 5);
    
    // Complete the animation
    for (int i = 0; i <= logoW; i++) {
        advanceAndUpdate(5);
    }
    
    EXPECT_FALSE(display->isLogoWipeActive());
}

TEST_F(DisplayTest, UpdateLogoWipe_AfterCompletion_DoesNothing) {
    display->startLogoWipe(testLogo, logoW, logoH, true, 5);
    
    // Complete the animation
    for (int i = 0; i <= logoW; i++) {
        advanceAndUpdate(5);
    }
    
    mockDisplay.reset();
    advanceAndUpdate(100);
    
    // Should not draw anything
    EXPECT_EQ(mockDisplay.clearDisplayCalls, 0);
    EXPECT_EQ(mockDisplay.drawBitmapCalls, 0);
}

// ============================================================================
// Wipe Direction Tests
// ============================================================================
TEST_F(DisplayTest, WipeIn_ProgressivelyRevealsLogo) {
    display->startLogoWipe(testLogo, logoW, logoH, true, 10);
    
    // First frame should mask most of the logo
    // As animation progresses, mask width decreases
    int initialMaskWidth = mockDisplay.lastRect.w;
    
    advanceAndUpdate(10);
    advanceAndUpdate(10);
    
    // Mask should be getting smaller (revealing more logo)
    EXPECT_LT(mockDisplay.lastRect.w, initialMaskWidth);
}

TEST_F(DisplayTest, WipeOut_ProgressivelyHidesLogo) {
    display->startLogoWipe(testLogo, logoW, logoH, false, 10);
    
    // First frame should have minimal mask
    int initialMaskWidth = mockDisplay.lastRect.w;
    
    advanceAndUpdate(10);
    advanceAndUpdate(10);
    
    // Mask should be getting bigger (hiding more logo)
    EXPECT_GT(mockDisplay.lastRect.w, initialMaskWidth);
}

// ============================================================================
// Timing Edge Cases
// ============================================================================
TEST_F(DisplayTest, UpdateLogoWipe_ZeroStepDelay_ProgressesEveryCall) {
    display->startLogoWipe(testLogo, logoW, logoH, true, 0);
    mockDisplay.reset();
    
    // Should progress even with zero delay
    advanceAndUpdate(0);
    EXPECT_GT(mockDisplay.clearDisplayCalls, 0);
}

TEST_F(DisplayTest, UpdateLogoWipe_LargeStepDelay_WaitsAppropriately) {
    display->startLogoWipe(testLogo, logoW, logoH, true, 1000);
    mockDisplay.reset();
    
    // 999ms should not trigger next frame
    advanceAndUpdate(999);
    EXPECT_EQ(mockDisplay.clearDisplayCalls, 0);
    
    // 1ms more should trigger it
    advanceAndUpdate(1);
    EXPECT_GT(mockDisplay.clearDisplayCalls, 0);
}

TEST_F(DisplayTest, UpdateLogoWipe_TimeOverflow_HandlesCorrectly) {
    // Start animation near overflow
    mockTime.set(0xFFFFFFF0);
    display->startLogoWipe(testLogo, logoW, logoH, true, 100);
    mockDisplay.reset();
    
    // Advance past overflow boundary
    mockTime.set(0x00000050); // Overflowed, elapsed = 0x60 = 96ms
    display->updateLogoWipe();
    EXPECT_EQ(mockDisplay.clearDisplayCalls, 0); // Not enough time yet
    
    mockTime.set(0x00000090); // Total elapsed = 0xA0 = 160ms
    display->updateLogoWipe();
    EXPECT_GT(mockDisplay.clearDisplayCalls, 0); // Should advance now
}

// ============================================================================
// Multiple Animations Tests
// ============================================================================
TEST_F(DisplayTest, StartLogoWipe_WhileActive_RestartsAnimation) {
    display->startLogoWipe(testLogo, logoW, logoH, true, 10);
    advanceAndUpdate(10);
    advanceAndUpdate(10);
    
    // Start new animation
    display->startLogoWipe(testLogo, logoW, logoH, false, 5);
    
    // Should be active and reset
    EXPECT_TRUE(display->isLogoWipeActive());
    
    // Complete with new timing
    for (int i = 0; i <= logoW; i++) {
        advanceAndUpdate(5);
    }
    EXPECT_FALSE(display->isLogoWipeActive());
}

// ============================================================================
// Integration Scenarios
// ============================================================================
TEST_F(DisplayTest, RealisticBootSequence_320msAnimation) {
    // Simulate 64-pixel logo with 5ms steps = 320ms total
    const uint8_t wideLogo[64] = {0xFF}; // Simplified
    
    mockTime.set(0);
    display->startLogoWipe(wideLogo, 64, 8, true, 5);
    
    // Verify it's active
    EXPECT_TRUE(display->isLogoWipeActive());
    
    // Simulate loop calling updateLogoWipe every 10ms
    uint32_t elapsed = 0;
    while (display->isLogoWipeActive() && elapsed < 400) {
        advanceAndUpdate(10);
        elapsed += 10;
    }
    
    // Should complete around 320ms (64 cols * 5ms)
    EXPECT_FALSE(display->isLogoWipeActive());
    EXPECT_LE(elapsed, 340); // Allow some tolerance
}

TEST_F(DisplayTest, SleepSequence_WipeOutThenBlank) {
    // Simulate sleep: wipe out logo then screen stays blank
    display->startLogoWipe(testLogo, logoW, logoH, false, 5);
    
    // Complete animation
    for (int i = 0; i <= logoW; i++) {
        advanceAndUpdate(5);
    }
    
    EXPECT_FALSE(display->isLogoWipeActive());
    
    // Further updates should do nothing
    mockDisplay.reset();
    advanceAndUpdate(1000);
    EXPECT_EQ(mockDisplay.clearDisplayCalls, 0);
}

TEST_F(DisplayTest, FastLoopCalls_DoesNotAdvancePrematurel) {
    display->startLogoWipe(testLogo, logoW, logoH, true, 50);
    mockDisplay.reset();
    
    // Rapid calls within one step delay
    for (int i = 0; i < 10; i++) {
        advanceAndUpdate(1);
    }
    
    // Should still be on first frame (only 10ms elapsed, need 50ms)
    EXPECT_LE(mockDisplay.clearDisplayCalls, 1); // At most 1 frame advance
}

// ============================================================================
// Different Logo Sizes
// ============================================================================
TEST_F(DisplayTest, SmallLogo_CompletesQuickly) {
    const uint8_t tinyLogo[1] = {0xFF};
    display->startLogoWipe(tinyLogo, 1, 1, true, 10);
    
    // Should complete after width + 1 = 2 steps
    advanceAndUpdate(10);
    advanceAndUpdate(10);
    
    EXPECT_FALSE(display->isLogoWipeActive());
}

TEST_F(DisplayTest, WideLogo_TakesLonger) {
    const uint8_t wideLogo[128] = {0xFF};
    display->startLogoWipe(wideLogo, 128, 8, true, 1);
    
    int steps = 0;
    while (display->isLogoWipeActive() && steps < 200) {
        advanceAndUpdate(1);
        steps++;
    }
    
    // Should take ~129 steps (width + 1)
    EXPECT_GE(steps, 128);
    EXPECT_LE(steps, 130);
}

// ============================================================================
// Blocking API Compatibility
// ============================================================================
TEST_F(DisplayTest, BlockingLogoWipe_StillWorks) {
    // The old blocking API should still be available for backward compatibility
    // It won't be tested for animation state since it's synchronous
    // Just verify it exists and can be called
    display->logoWipe(testLogo, logoW, logoH, true, 5);
    
    // Should have rendered (called display multiple times)
    EXPECT_GT(mockDisplay.displayCalls, 0);
}

// ============================================================================
// Main
// ============================================================================
int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
