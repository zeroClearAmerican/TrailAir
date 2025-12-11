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
    UserInterfaceStateMachine ui;
    MockDeviceActions device;
    UserInterfaceConfig cfg;

    void SetUp() override {
        cfg.minimumPSI = 5.0f;
        cfg.maximumPSI = 50.0f;
        cfg.defaultTargetPSI = 32.0f;
        cfg.stepSize = 1.0f;
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
    EXPECT_EQ(ui.getViewState(), ViewState::Idle);
    EXPECT_FLOAT_EQ(ui.getTargetPSI(), cfg.defaultTargetPSI);
}

TEST_F(UiTest, InitialConfig_ClampsTarget) {
    UserInterfaceConfig customCfg = cfg;
    customCfg.defaultTargetPSI = 100.0f; // Above max
    ui.begin(customCfg);
    EXPECT_FLOAT_EQ(ui.getTargetPSI(), customCfg.maximumPSI);
}

// ============================================================================
// Idle View - Target PSI Adjustment
// ============================================================================
TEST_F(UiTest, Idle_UpButton_IncreasesTarget) {
    float initial = ui.getTargetPSI();
    ui.onButton(makeEvent(ButtonId::Up, ButtonAction::Click), device);
    EXPECT_FLOAT_EQ(ui.getTargetPSI(), initial + cfg.stepSize);
}

TEST_F(UiTest, Idle_DownButton_DecreasesTarget) {
    float initial = ui.getTargetPSI();
    ui.onButton(makeEvent(ButtonId::Down, ButtonAction::Click), device);
    EXPECT_FLOAT_EQ(ui.getTargetPSI(), initial - cfg.stepSize);
}

TEST_F(UiTest, Idle_UpButton_ClampsAtMax) {
    ui.setTargetPSI(cfg.maximumPSI - 0.5f);
    ui.onButton(makeEvent(ButtonId::Up, ButtonAction::Click), device);
    EXPECT_FLOAT_EQ(ui.getTargetPSI(), cfg.maximumPSI);
}

TEST_F(UiTest, Idle_DownButton_ClampsAtMin) {
    ui.setTargetPSI(cfg.minimumPSI + 0.5f);
    ui.onButton(makeEvent(ButtonId::Down, ButtonAction::Click), device);
    EXPECT_FLOAT_EQ(ui.getTargetPSI(), cfg.minimumPSI);
}

TEST_F(UiTest, Idle_MultipleClicks_AccumulateSteps) {
    float initial = ui.getTargetPSI();
    ui.onButton(makeEvent(ButtonId::Up, ButtonAction::Click), device);
    ui.onButton(makeEvent(ButtonId::Up, ButtonAction::Click), device);
    ui.onButton(makeEvent(ButtonId::Up, ButtonAction::Click), device);
    EXPECT_FLOAT_EQ(ui.getTargetPSI(), initial + 3.0f * cfg.stepSize);
}

// ============================================================================
// Idle View - View Transitions
// ============================================================================
TEST_F(UiTest, Idle_LeftClick_EntersManual) {
    ui.onButton(makeEvent(ButtonId::Left, ButtonAction::Click), device);
    EXPECT_EQ(ui.getViewState(), ViewState::Manual);
    EXPECT_EQ(device.cancelCalls, 1);
}

TEST_F(UiTest, Idle_RightClick_StartsSeeking) {
    ui.onButton(makeEvent(ButtonId::Right, ButtonAction::Click), device);
    EXPECT_EQ(ui.getViewState(), ViewState::Seeking);
    EXPECT_EQ(device.startSeekCalls, 1);
    EXPECT_FLOAT_EQ(device.lastSeekTarget, ui.getTargetPSI());
}

// ============================================================================
// Manual View Tests
// ============================================================================
TEST_F(UiTest, Manual_DownPress_ActivatesVent) {
    ui.onButton(makeEvent(ButtonId::Left, ButtonAction::Click), device); // Enter manual
    device.reset();

    ui.onButton(makeEvent(ButtonId::Down, ButtonAction::Pressed), device);
    EXPECT_EQ(device.manualVentCalls, 1);
    EXPECT_TRUE(device.lastVentState);
}

TEST_F(UiTest, Manual_DownRelease_DeactivatesVent) {
    ui.onButton(makeEvent(ButtonId::Left, ButtonAction::Click), device); // Enter manual
    ui.onButton(makeEvent(ButtonId::Down, ButtonAction::Pressed), device);
    device.reset();

    ui.onButton(makeEvent(ButtonId::Down, ButtonAction::Released), device);
    EXPECT_EQ(device.manualVentCalls, 1);
    EXPECT_FALSE(device.lastVentState);
}

TEST_F(UiTest, Manual_UpPress_ActivatesAir) {
    ui.onButton(makeEvent(ButtonId::Left, ButtonAction::Click), device); // Enter manual
    device.reset();

    ui.onButton(makeEvent(ButtonId::Up, ButtonAction::Pressed), device);
    EXPECT_EQ(device.manualAirCalls, 1);
    EXPECT_TRUE(device.lastAirState);
}

TEST_F(UiTest, Manual_UpRelease_DeactivatesAir) {
    ui.onButton(makeEvent(ButtonId::Left, ButtonAction::Click), device); // Enter manual
    ui.onButton(makeEvent(ButtonId::Up, ButtonAction::Pressed), device);
    device.reset();

    ui.onButton(makeEvent(ButtonId::Up, ButtonAction::Released), device);
    EXPECT_EQ(device.manualAirCalls, 1);
    EXPECT_FALSE(device.lastAirState);
}

TEST_F(UiTest, Manual_LeftClick_ExitsToIdle) {
    ui.onButton(makeEvent(ButtonId::Left, ButtonAction::Click), device); // Enter manual
    EXPECT_EQ(ui.getViewState(), ViewState::Manual);

    ui.onButton(makeEvent(ButtonId::Left, ButtonAction::Click), device); // Exit manual
    EXPECT_EQ(ui.getViewState(), ViewState::Idle);
}

TEST_F(UiTest, Manual_ExitWhileVenting_StopsVent) {
    ui.onButton(makeEvent(ButtonId::Left, ButtonAction::Click), device); // Enter manual
    ui.onButton(makeEvent(ButtonId::Down, ButtonAction::Pressed), device); // Start venting
    device.reset();

    ui.onButton(makeEvent(ButtonId::Left, ButtonAction::Click), device); // Exit
    EXPECT_EQ(device.manualVentCalls, 1);
    EXPECT_FALSE(device.lastVentState);
}

TEST_F(UiTest, Manual_ExitWhileAiring_StopsAir) {
    ui.onButton(makeEvent(ButtonId::Left, ButtonAction::Click), device); // Enter manual
    ui.onButton(makeEvent(ButtonId::Up, ButtonAction::Pressed), device); // Start air
    device.reset();

    ui.onButton(makeEvent(ButtonId::Left, ButtonAction::Click), device); // Exit
    EXPECT_EQ(device.manualAirCalls, 1);
    EXPECT_FALSE(device.lastAirState);
}

TEST_F(UiTest, Manual_BothButtonsPressed_BothActive) {
    ui.onButton(makeEvent(ButtonId::Left, ButtonAction::Click), device); // Enter manual
    device.reset();

    ui.onButton(makeEvent(ButtonId::Down, ButtonAction::Pressed), device);
    ui.onButton(makeEvent(ButtonId::Up, ButtonAction::Pressed), device);
    
    EXPECT_EQ(device.manualVentCalls, 1);
    EXPECT_EQ(device.manualAirCalls, 1);
}

// ============================================================================
// Seeking View Tests
// ============================================================================
TEST_F(UiTest, Seeking_RightClick_Cancels) {
    ui.onButton(makeEvent(ButtonId::Right, ButtonAction::Click), device); // Start seek
    EXPECT_EQ(ui.getViewState(), ViewState::Seeking);
    device.reset();

    ui.onButton(makeEvent(ButtonId::Right, ButtonAction::Click), device); // Cancel
    EXPECT_EQ(ui.getViewState(), ViewState::Idle);
    EXPECT_EQ(device.cancelCalls, 1);
}

TEST_F(UiTest, Seeking_IgnoresOtherButtons) {
    ui.onButton(makeEvent(ButtonId::Right, ButtonAction::Click), device); // Start seek
    float target = ui.getTargetPSI();
    device.reset();

    ui.onButton(makeEvent(ButtonId::Up, ButtonAction::Click), device);
    ui.onButton(makeEvent(ButtonId::Down, ButtonAction::Click), device);
    ui.onButton(makeEvent(ButtonId::Left, ButtonAction::Click), device);
    
    EXPECT_FLOAT_EQ(ui.getTargetPSI(), target); // Unchanged
    EXPECT_EQ(device.startSeekCalls, 0); // No new seeks
}

// ============================================================================
// Seeking Completion - Done Hold Tests
// ============================================================================
TEST_F(UiTest, SeekingComplete_ShowsDoneHold) {
    uint32_t time = 0;
    ui.onButton(makeEvent(ButtonId::Right, ButtonAction::Click), device); // Start seek
    EXPECT_EQ(ui.getViewState(), ViewState::Seeking);

    // Simulate controller activity
    ui.update(time, device, ControllerState::AirUp);
    time += 100;
    ui.update(time, device, ControllerState::Checking);
    time += 100;
    
    // Controller reaches idle
    ui.update(time, device, ControllerState::Idle);
    EXPECT_EQ(ui.getViewState(), ViewState::Idle);
    EXPECT_TRUE(ui.isDoneHoldActive(time));
}

TEST_F(UiTest, DoneHold_ExpiresAfterTimeout) {
    uint32_t time = 0;
    ui.onButton(makeEvent(ButtonId::Right, ButtonAction::Click), device);
    ui.update(time, device, ControllerState::AirUp);
    ui.update(time, device, ControllerState::Idle);
    EXPECT_TRUE(ui.isDoneHoldActive(time));

    time += cfg.doneHoldMs + 100;
    ui.update(time, device, ControllerState::Idle);
    EXPECT_FALSE(ui.isDoneHoldActive(time));
}

TEST_F(UiTest, SeekingWithoutActivity_NoDoneHold) {
    uint32_t time = 0;
    ui.onButton(makeEvent(ButtonId::Right, ButtonAction::Click), device);
    
    // Immediately idle (already at target)
    ui.update(time, device, ControllerState::Idle);
    EXPECT_FALSE(ui.isDoneHoldActive(time));
}

TEST_F(UiTest, DoneHold_ClearedByCancelDuringSeeking) {
    uint32_t time = 0;
    ui.onButton(makeEvent(ButtonId::Right, ButtonAction::Click), device);
    ui.update(time, device, ControllerState::AirUp);
    
    ui.onButton(makeEvent(ButtonId::Right, ButtonAction::Click), device); // Cancel
    EXPECT_FALSE(ui.isDoneHoldActive(time));
}

// ============================================================================
// Error View Tests
// ============================================================================
TEST_F(UiTest, ControllerError_EntersErrorView) {
    uint32_t time = 0;
    ui.update(time, device, ControllerState::Error);
    EXPECT_EQ(ui.getViewState(), ViewState::Error);
}

TEST_F(UiTest, Error_RightClick_ClearsError) {
    uint32_t time = 0;
    ui.update(time, device, ControllerState::Error);
    EXPECT_EQ(ui.getViewState(), ViewState::Error);
    device.reset();

    ui.onButton(makeEvent(ButtonId::Right, ButtonAction::Click), device);
    EXPECT_EQ(device.clearErrorCalls, 1);
}

TEST_F(UiTest, Error_AutoClear_AfterTimeout) {
    uint32_t time = 0;
    ui.update(time, device, ControllerState::Error);
    EXPECT_EQ(ui.getViewState(), ViewState::Error);
    device.reset();

    time += cfg.errorAutoClearMs + 100;
    ui.update(time, device, ControllerState::Error);
    EXPECT_EQ(device.clearErrorCalls, 1);
}

TEST_F(UiTest, Error_ExitsWhenControllerIdle) {
    uint32_t time = 0;
    ui.update(time, device, ControllerState::Error);
    EXPECT_EQ(ui.getViewState(), ViewState::Error);

    ui.update(time, device, ControllerState::Idle);
    EXPECT_EQ(ui.getViewState(), ViewState::Idle);
}

TEST_F(UiTest, Error_DisabledAutoClear_DoesNotClear) {
    cfg.errorAutoClearMs = 0; // Disable auto-clear
    ui.begin(cfg);

    uint32_t time = 0;
    ui.update(time, device, ControllerState::Error);
    device.reset();

    time += 10000; // Wait very long
    ui.update(time, device, ControllerState::Error);
    EXPECT_EQ(device.clearErrorCalls, 0); // No auto-clear
}

// ============================================================================
// Disconnected View Tests (Remote-specific)
// ============================================================================
TEST_F(UiTest, Disconnected_WhenDeviceNotConnected) {
    device.connected = false;
    uint32_t time = 0;
    ui.update(time, device, ControllerState::Idle);
    EXPECT_EQ(ui.getViewState(), ViewState::Disconnected);
}

TEST_F(UiTest, Disconnected_ReconnectRestoresIdle) {
    device.connected = false;
    uint32_t time = 0;
    ui.update(time, device, ControllerState::Idle);
    EXPECT_EQ(ui.getViewState(), ViewState::Disconnected);

    // Reconnect - should restore to Idle
    device.connected = true;
    ui.update(time, device, ControllerState::Idle);
    EXPECT_EQ(ui.getViewState(), ViewState::Idle);
}

// ============================================================================
// Controller State Tracking Tests
// ============================================================================
TEST_F(UiTest, Update_TracksControllerState) {
    uint32_t time = 0;
    EXPECT_EQ(ui.getViewState(), ViewState::Idle);

    ui.update(time, device, ControllerState::AirUp);
    // View doesn't change to follow controller unless seeking
    EXPECT_EQ(ui.getViewState(), ViewState::Idle);
}

TEST_F(UiTest, SeekingView_TracksControllerActivity) {
    uint32_t time = 0;
    ui.onButton(makeEvent(ButtonId::Right, ButtonAction::Click), device);
    EXPECT_EQ(ui.getViewState(), ViewState::Seeking);

    ui.update(time, device, ControllerState::AirUp);
    EXPECT_EQ(ui.getViewState(), ViewState::Seeking);
    
    ui.update(time, device, ControllerState::Checking);
    EXPECT_EQ(ui.getViewState(), ViewState::Seeking);
}

// ============================================================================
// Target PSI Management Tests
// ============================================================================
TEST_F(UiTest, SetTargetPsi_ClampsToMin) {
    ui.setTargetPSI(0.0f);
    EXPECT_FLOAT_EQ(ui.getTargetPSI(), cfg.minimumPSI);
}

TEST_F(UiTest, SetTargetPsi_ClampsToMax) {
    ui.setTargetPSI(100.0f);
    EXPECT_FLOAT_EQ(ui.getTargetPSI(), cfg.maximumPSI);
}

TEST_F(UiTest, SetTargetPsi_ValidRange) {
    ui.setTargetPSI(25.0f);
    EXPECT_FLOAT_EQ(ui.getTargetPSI(), 25.0f);
}

// ============================================================================
// Edge Cases
// ============================================================================
TEST_F(UiTest, RapidButtonPresses_HandleCorrectly) {
    ui.onButton(makeEvent(ButtonId::Up, ButtonAction::Click), device);
    ui.onButton(makeEvent(ButtonId::Down, ButtonAction::Click), device);
    ui.onButton(makeEvent(ButtonId::Left, ButtonAction::Click), device);
    ui.onButton(makeEvent(ButtonId::Right, ButtonAction::Click), device);
    
    // Should not crash, state should be valid
    EXPECT_TRUE(ui.getViewState() == ViewState::Idle || 
                ui.getViewState() == ViewState::Manual || 
                ui.getViewState() == ViewState::Seeking);
}

TEST_F(UiTest, PressedWithoutRelease_HandleGracefully) {
    ui.onButton(makeEvent(ButtonId::Left, ButtonAction::Click), device); // Manual
    ui.onButton(makeEvent(ButtonId::Up, ButtonAction::Pressed), device);
    
    // No release - exit manual
    ui.onButton(makeEvent(ButtonId::Left, ButtonAction::Click), device);
    EXPECT_EQ(ui.getViewState(), ViewState::Idle);
}

TEST_F(UiTest, Config_MinMaxEqual_DoesNotCrash) {
    UserInterfaceConfig badCfg = cfg;
    badCfg.minimumPSI = 20.0f;
    badCfg.maximumPSI = 20.0f;
    ui.begin(badCfg);
    
    ui.onButton(makeEvent(ButtonId::Up, ButtonAction::Click), device);
    ui.onButton(makeEvent(ButtonId::Down, ButtonAction::Click), device);
    
    EXPECT_FLOAT_EQ(ui.getTargetPSI(), 20.0f);
}

TEST_F(UiTest, AccessorsReturnCorrectValues) {
    EXPECT_FLOAT_EQ(ui.minimumPSI(), cfg.minimumPSI);
    EXPECT_FLOAT_EQ(ui.maximumPSI(), cfg.maximumPSI);
}

// ============================================================================
// Main function
// ============================================================================
int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
