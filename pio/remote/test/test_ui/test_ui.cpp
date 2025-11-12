/**
 * Unit tests for TA_UI
 * Tests UI state machine, button handling, and view transitions
 */

#include <gtest/gtest.h>
#include <TA_UI.h>

using namespace ta::ui;

// ============================================================================
// Mock Device Actions - Tracks what the UI requests
// ============================================================================
class MockDeviceActions : public DeviceActions {
public:
    int cancelCalls = 0;
    int clearErrorCalls = 0;
    int startSeekCalls = 0;
    float lastSeekTarget = 0.0f;
    int manualVentCalls = 0;
    bool lastVentState = false;
    int manualAirCalls = 0;
    bool lastAirState = false;
    bool connected = true;

    void cancel() override {
        cancelCalls++;
    }

    void clearError() override {
        clearErrorCalls++;
    }

    void startSeek(float targetPsi) override {
        startSeekCalls++;
        lastSeekTarget = targetPsi;
    }

    void manualVent(bool on) override {
        manualVentCalls++;
        lastVentState = on;
    }

    void manualAirUp(bool on) override {
        manualAirCalls++;
        lastAirState = on;
    }

    bool isConnected() const override {
        return connected;
    }

    void reset() {
        cancelCalls = clearErrorCalls = startSeekCalls = 0;
        manualVentCalls = manualAirCalls = 0;
        lastSeekTarget = 0.0f;
        lastVentState = lastAirState = false;
        connected = true;
    }
};

// ============================================================================
// Test Fixture
// ============================================================================
class UiTest : public ::testing::Test {
protected:
    UiStateMachine ui;
    MockDeviceActions device;
    UiConfig cfg;

    void SetUp() override {
        cfg.minPsi = 5.0f;
        cfg.maxPsi = 50.0f;
        cfg.defaultTargetPsi = 32.0f;
        cfg.stepSmall = 1.0f;
        cfg.doneHoldMs = 1000;
        cfg.errorAutoClearMs = 3000;

        ui.begin(cfg);
        device.reset();
    }

    // Helper to create button events
    ButtonEvent makeEvent(Button btn, Action act) {
        return ButtonEvent{btn, act};
    }
};

// ============================================================================
// Initialization Tests
// ============================================================================
TEST_F(UiTest, InitialState) {
    EXPECT_EQ(ui.view(), View::Idle);
    EXPECT_FLOAT_EQ(ui.targetPsi(), cfg.defaultTargetPsi);
}

TEST_F(UiTest, InitialConfig_ClampsTarget) {
    UiConfig customCfg = cfg;
    customCfg.defaultTargetPsi = 100.0f; // Above max
    ui.begin(customCfg);
    EXPECT_FLOAT_EQ(ui.targetPsi(), customCfg.maxPsi);
}

// ============================================================================
// Idle View - Target PSI Adjustment
// ============================================================================
TEST_F(UiTest, Idle_UpButton_IncreasesTarget) {
    float initial = ui.targetPsi();
    ui.onButton(makeEvent(Button::Up, Action::Click), device);
    EXPECT_FLOAT_EQ(ui.targetPsi(), initial + cfg.stepSmall);
}

TEST_F(UiTest, Idle_DownButton_DecreasesTarget) {
    float initial = ui.targetPsi();
    ui.onButton(makeEvent(Button::Down, Action::Click), device);
    EXPECT_FLOAT_EQ(ui.targetPsi(), initial - cfg.stepSmall);
}

TEST_F(UiTest, Idle_UpButton_ClampsAtMax) {
    ui.setTargetPsi(cfg.maxPsi - 0.5f);
    ui.onButton(makeEvent(Button::Up, Action::Click), device);
    EXPECT_FLOAT_EQ(ui.targetPsi(), cfg.maxPsi);
}

TEST_F(UiTest, Idle_DownButton_ClampsAtMin) {
    ui.setTargetPsi(cfg.minPsi + 0.5f);
    ui.onButton(makeEvent(Button::Down, Action::Click), device);
    EXPECT_FLOAT_EQ(ui.targetPsi(), cfg.minPsi);
}

TEST_F(UiTest, Idle_MultipleClicks_AccumulateSteps) {
    float initial = ui.targetPsi();
    ui.onButton(makeEvent(Button::Up, Action::Click), device);
    ui.onButton(makeEvent(Button::Up, Action::Click), device);
    ui.onButton(makeEvent(Button::Up, Action::Click), device);
    EXPECT_FLOAT_EQ(ui.targetPsi(), initial + 3.0f * cfg.stepSmall);
}

// ============================================================================
// Idle View - View Transitions
// ============================================================================
TEST_F(UiTest, Idle_LeftClick_EntersManual) {
    ui.onButton(makeEvent(Button::Left, Action::Click), device);
    EXPECT_EQ(ui.view(), View::Manual);
    EXPECT_EQ(device.cancelCalls, 1);
}

TEST_F(UiTest, Idle_RightClick_StartsSeeking) {
    ui.onButton(makeEvent(Button::Right, Action::Click), device);
    EXPECT_EQ(ui.view(), View::Seeking);
    EXPECT_EQ(device.startSeekCalls, 1);
    EXPECT_FLOAT_EQ(device.lastSeekTarget, ui.targetPsi());
}

// ============================================================================
// Manual View Tests
// ============================================================================
TEST_F(UiTest, Manual_DownPress_ActivatesVent) {
    ui.onButton(makeEvent(Button::Left, Action::Click), device); // Enter manual
    device.reset();

    ui.onButton(makeEvent(Button::Down, Action::Pressed), device);
    EXPECT_EQ(device.manualVentCalls, 1);
    EXPECT_TRUE(device.lastVentState);
}

TEST_F(UiTest, Manual_DownRelease_DeactivatesVent) {
    ui.onButton(makeEvent(Button::Left, Action::Click), device); // Enter manual
    ui.onButton(makeEvent(Button::Down, Action::Pressed), device);
    device.reset();

    ui.onButton(makeEvent(Button::Down, Action::Released), device);
    EXPECT_EQ(device.manualVentCalls, 1);
    EXPECT_FALSE(device.lastVentState);
}

TEST_F(UiTest, Manual_UpPress_ActivatesAir) {
    ui.onButton(makeEvent(Button::Left, Action::Click), device); // Enter manual
    device.reset();

    ui.onButton(makeEvent(Button::Up, Action::Pressed), device);
    EXPECT_EQ(device.manualAirCalls, 1);
    EXPECT_TRUE(device.lastAirState);
}

TEST_F(UiTest, Manual_UpRelease_DeactivatesAir) {
    ui.onButton(makeEvent(Button::Left, Action::Click), device); // Enter manual
    ui.onButton(makeEvent(Button::Up, Action::Pressed), device);
    device.reset();

    ui.onButton(makeEvent(Button::Up, Action::Released), device);
    EXPECT_EQ(device.manualAirCalls, 1);
    EXPECT_FALSE(device.lastAirState);
}

TEST_F(UiTest, Manual_LeftClick_ExitsToIdle) {
    ui.onButton(makeEvent(Button::Left, Action::Click), device); // Enter manual
    EXPECT_EQ(ui.view(), View::Manual);

    ui.onButton(makeEvent(Button::Left, Action::Click), device); // Exit manual
    EXPECT_EQ(ui.view(), View::Idle);
}

TEST_F(UiTest, Manual_ExitWhileVenting_StopsVent) {
    ui.onButton(makeEvent(Button::Left, Action::Click), device); // Enter manual
    ui.onButton(makeEvent(Button::Down, Action::Pressed), device); // Start venting
    device.reset();

    ui.onButton(makeEvent(Button::Left, Action::Click), device); // Exit
    EXPECT_EQ(device.manualVentCalls, 1);
    EXPECT_FALSE(device.lastVentState);
}

TEST_F(UiTest, Manual_ExitWhileAiring_StopsAir) {
    ui.onButton(makeEvent(Button::Left, Action::Click), device); // Enter manual
    ui.onButton(makeEvent(Button::Up, Action::Pressed), device); // Start air
    device.reset();

    ui.onButton(makeEvent(Button::Left, Action::Click), device); // Exit
    EXPECT_EQ(device.manualAirCalls, 1);
    EXPECT_FALSE(device.lastAirState);
}

TEST_F(UiTest, Manual_BothButtonsPressed_BothActive) {
    ui.onButton(makeEvent(Button::Left, Action::Click), device); // Enter manual
    device.reset();

    ui.onButton(makeEvent(Button::Down, Action::Pressed), device);
    ui.onButton(makeEvent(Button::Up, Action::Pressed), device);
    
    EXPECT_EQ(device.manualVentCalls, 1);
    EXPECT_EQ(device.manualAirCalls, 1);
}

// ============================================================================
// Seeking View Tests
// ============================================================================
TEST_F(UiTest, Seeking_RightClick_Cancels) {
    ui.onButton(makeEvent(Button::Right, Action::Click), device); // Start seek
    EXPECT_EQ(ui.view(), View::Seeking);
    device.reset();

    ui.onButton(makeEvent(Button::Right, Action::Click), device); // Cancel
    EXPECT_EQ(ui.view(), View::Idle);
    EXPECT_EQ(device.cancelCalls, 1);
}

TEST_F(UiTest, Seeking_IgnoresOtherButtons) {
    ui.onButton(makeEvent(Button::Right, Action::Click), device); // Start seek
    float target = ui.targetPsi();
    device.reset();

    ui.onButton(makeEvent(Button::Up, Action::Click), device);
    ui.onButton(makeEvent(Button::Down, Action::Click), device);
    ui.onButton(makeEvent(Button::Left, Action::Click), device);
    
    EXPECT_FLOAT_EQ(ui.targetPsi(), target); // Unchanged
    EXPECT_EQ(device.startSeekCalls, 0); // No new seeks
}

// ============================================================================
// Seeking Completion - Done Hold Tests
// ============================================================================
TEST_F(UiTest, SeekingComplete_ShowsDoneHold) {
    uint32_t time = 0;
    ui.onButton(makeEvent(Button::Right, Action::Click), device); // Start seek
    EXPECT_EQ(ui.view(), View::Seeking);

    // Simulate controller activity
    ui.update(time, device, Ctrl::AirUp);
    time += 100;
    ui.update(time, device, Ctrl::Checking);
    time += 100;
    
    // Controller reaches idle
    ui.update(time, device, Ctrl::Idle);
    EXPECT_EQ(ui.view(), View::Idle);
    EXPECT_TRUE(ui.isDoneHoldActive(time));
}

TEST_F(UiTest, DoneHold_ExpiresAfterTimeout) {
    uint32_t time = 0;
    ui.onButton(makeEvent(Button::Right, Action::Click), device);
    ui.update(time, device, Ctrl::AirUp);
    ui.update(time, device, Ctrl::Idle);
    EXPECT_TRUE(ui.isDoneHoldActive(time));

    time += cfg.doneHoldMs + 100;
    ui.update(time, device, Ctrl::Idle);
    EXPECT_FALSE(ui.isDoneHoldActive(time));
}

TEST_F(UiTest, SeekingWithoutActivity_NoDoneHold) {
    uint32_t time = 0;
    ui.onButton(makeEvent(Button::Right, Action::Click), device);
    
    // Immediately idle (already at target)
    ui.update(time, device, Ctrl::Idle);
    EXPECT_FALSE(ui.isDoneHoldActive(time));
}

TEST_F(UiTest, DoneHold_ClearedByCancelDuringSeeking) {
    uint32_t time = 0;
    ui.onButton(makeEvent(Button::Right, Action::Click), device);
    ui.update(time, device, Ctrl::AirUp);
    
    ui.onButton(makeEvent(Button::Right, Action::Click), device); // Cancel
    EXPECT_FALSE(ui.isDoneHoldActive(time));
}

// ============================================================================
// Error View Tests
// ============================================================================
TEST_F(UiTest, ControllerError_EntersErrorView) {
    uint32_t time = 0;
    ui.update(time, device, Ctrl::Error);
    EXPECT_EQ(ui.view(), View::Error);
}

TEST_F(UiTest, Error_RightClick_ClearsError) {
    uint32_t time = 0;
    ui.update(time, device, Ctrl::Error);
    EXPECT_EQ(ui.view(), View::Error);
    device.reset();

    ui.onButton(makeEvent(Button::Right, Action::Click), device);
    EXPECT_EQ(device.clearErrorCalls, 1);
}

TEST_F(UiTest, Error_AutoClear_AfterTimeout) {
    uint32_t time = 0;
    ui.update(time, device, Ctrl::Error);
    EXPECT_EQ(ui.view(), View::Error);
    device.reset();

    time += cfg.errorAutoClearMs + 100;
    ui.update(time, device, Ctrl::Error);
    EXPECT_EQ(device.clearErrorCalls, 1);
}

TEST_F(UiTest, Error_ExitsWhenControllerIdle) {
    uint32_t time = 0;
    ui.update(time, device, Ctrl::Error);
    EXPECT_EQ(ui.view(), View::Error);

    ui.update(time, device, Ctrl::Idle);
    EXPECT_EQ(ui.view(), View::Idle);
}

TEST_F(UiTest, Error_DisabledAutoClear_DoesNotClear) {
    cfg.errorAutoClearMs = 0; // Disable auto-clear
    ui.begin(cfg);

    uint32_t time = 0;
    ui.update(time, device, Ctrl::Error);
    device.reset();

    time += 10000; // Wait very long
    ui.update(time, device, Ctrl::Error);
    EXPECT_EQ(device.clearErrorCalls, 0); // No auto-clear
}

// ============================================================================
// Disconnected View Tests (Remote-specific)
// ============================================================================
TEST_F(UiTest, Disconnected_WhenDeviceNotConnected) {
    device.connected = false;
    uint32_t time = 0;
    ui.update(time, device, Ctrl::Idle);
    EXPECT_EQ(ui.view(), View::Disconnected);
}

TEST_F(UiTest, Disconnected_ReconnectRestoresIdle) {
    device.connected = false;
    uint32_t time = 0;
    ui.update(time, device, Ctrl::Idle);
    EXPECT_EQ(ui.view(), View::Disconnected);

    device.connected = true;
    ui.update(time, device, Ctrl::Idle);
    EXPECT_EQ(ui.view(), View::Idle);
}

// ============================================================================
// Controller State Tracking Tests
// ============================================================================
TEST_F(UiTest, Update_TracksControllerState) {
    uint32_t time = 0;
    EXPECT_EQ(ui.view(), View::Idle);

    ui.update(time, device, Ctrl::AirUp);
    // View doesn't change to follow controller unless seeking
    EXPECT_EQ(ui.view(), View::Idle);
}

TEST_F(UiTest, SeekingView_TracksControllerActivity) {
    uint32_t time = 0;
    ui.onButton(makeEvent(Button::Right, Action::Click), device);
    EXPECT_EQ(ui.view(), View::Seeking);

    ui.update(time, device, Ctrl::AirUp);
    EXPECT_EQ(ui.view(), View::Seeking);
    
    ui.update(time, device, Ctrl::Checking);
    EXPECT_EQ(ui.view(), View::Seeking);
}

// ============================================================================
// Target PSI Management Tests
// ============================================================================
TEST_F(UiTest, SetTargetPsi_ClampsToMin) {
    ui.setTargetPsi(0.0f);
    EXPECT_FLOAT_EQ(ui.targetPsi(), cfg.minPsi);
}

TEST_F(UiTest, SetTargetPsi_ClampsToMax) {
    ui.setTargetPsi(100.0f);
    EXPECT_FLOAT_EQ(ui.targetPsi(), cfg.maxPsi);
}

TEST_F(UiTest, SetTargetPsi_ValidRange) {
    ui.setTargetPsi(25.0f);
    EXPECT_FLOAT_EQ(ui.targetPsi(), 25.0f);
}

// ============================================================================
// Edge Cases
// ============================================================================
TEST_F(UiTest, RapidButtonPresses_HandleCorrectly) {
    ui.onButton(makeEvent(Button::Up, Action::Click), device);
    ui.onButton(makeEvent(Button::Down, Action::Click), device);
    ui.onButton(makeEvent(Button::Left, Action::Click), device);
    ui.onButton(makeEvent(Button::Right, Action::Click), device);
    
    // Should not crash, state should be valid
    EXPECT_TRUE(ui.view() == View::Idle || 
                ui.view() == View::Manual || 
                ui.view() == View::Seeking);
}

TEST_F(UiTest, PressedWithoutRelease_HandleGracefully) {
    ui.onButton(makeEvent(Button::Left, Action::Click), device); // Manual
    ui.onButton(makeEvent(Button::Up, Action::Pressed), device);
    
    // No release - exit manual
    ui.onButton(makeEvent(Button::Left, Action::Click), device);
    EXPECT_EQ(ui.view(), View::Idle);
}

TEST_F(UiTest, Config_MinMaxEqual_DoesNotCrash) {
    UiConfig badCfg = cfg;
    badCfg.minPsi = 20.0f;
    badCfg.maxPsi = 20.0f;
    ui.begin(badCfg);
    
    ui.onButton(makeEvent(Button::Up, Action::Click), device);
    ui.onButton(makeEvent(Button::Down, Action::Click), device);
    
    EXPECT_FLOAT_EQ(ui.targetPsi(), 20.0f);
}

TEST_F(UiTest, AccessorsReturnCorrectValues) {
    EXPECT_FLOAT_EQ(ui.minPsi(), cfg.minPsi);
    EXPECT_FLOAT_EQ(ui.maxPsi(), cfg.maxPsi);
}

// ============================================================================
// Main function
// ============================================================================
int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
