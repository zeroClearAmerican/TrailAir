# TrailAir Thin Client Architecture Refactoring

## Vision: Remote as a Wireless Input/Display Device

Transform the remote from a duplicate state machine into a "thin client" that simply:

1. **Sends button presses** to the control board
2. **Displays what the board tells it to display**
3. **Manages remote-only concerns**: battery, sleep, wireless connection

The control board becomes the **single source of truth** for all UI logic, state management, and pressure control.

## Current Architecture (Problematic)

### Duplication Everywhere

```
┌─────────────────────────────────────────────────────────────┐
│                    CONTROL BOARD                             │
├─────────────────────────────────────────────────────────────┤
│ • Physical buttons → ButtonManager → TA_UI state machine   │
│ • Protocol commands → onRequest_() → Custom translation     │
│   - Duplicates state logic                                   │
│   - Inconsistent with button handling                        │
│ • Has display, shows UI state                                │
│ • Manages PressureController                                 │
│ • Runs TA_UI::UserInterfaceStateMachine                      │
└─────────────────────────────────────────────────────────────┘

┌─────────────────────────────────────────────────────────────┐
│                       REMOTE                                 │
├─────────────────────────────────────────────────────────────┤
│ • Physical buttons → TA_State → TA_UI state machine         │
│   - DUPLICATE state machine instance                         │
│   - Different from board's state machine                     │
│ • Status from board → onStatus() → Sync state                │
│   - Board's UI state overrides local in buildDisplayModel()  │
│   - Confusing "who is master?" logic                         │
│ • Has display, shows what board reports (mostly)             │
│ • Manages battery (remote-specific) ✓                        │
│ • Manages sleep (remote-specific) ✓                          │
│ • Runs TA_UI::UserInterfaceStateMachine (DUPLICATE!)         │
└─────────────────────────────────────────────────────────────┘
```

### Problems

1. **Two state machines** - Remote runs its own `TA_UI::UserInterfaceStateMachine`, then overrides it with board's state
2. **Confusing ownership** - Which state is "real"? Board's or remote's?
3. **Sync complexity** - Remote's local state vs. `boardUIState_` override in `buildDisplayModel()`
4. **Duplicate logic** - Same UI transitions coded twice
5. **Testing nightmare** - Must test board and remote separately, ensure they stay in sync
6. **Bugs** - Remote and board can get out of sync, show different things

## New Architecture (Thin Client)

### Control Board: The Master

```
┌─────────────────────────────────────────────────────────────┐
│                    CONTROL BOARD (Master)                    │
├─────────────────────────────────────────────────────────────┤
│ INPUTS:                                                      │
│   • Physical buttons → ButtonEvent → StateBoard → TA_UI     │
│   • Remote buttons → Protocol → ButtonEvent → TA_UI         │
│     (Already refactored in REMOTE_COMMAND_REFACTORING.md)   │
│                                                               │
│ STATE MACHINE:                                                │
│   • TA_UI::UserInterfaceStateMachine (SINGLE SOURCE)        │
│   • Manages all transitions: Idle ↔ Manual ↔ Seeking ↔ Error│
│   • Controls PressureController                              │
│                                                               │
│ OUTPUTS:                                                      │
│   • Local display (optional)                                 │
│   • Protocol Response to remote:                             │
│     - Current PSI                                             │
│     - Target PSI                                              │
│     - Controller status (Idle/AirUp/Venting/Checking/Error) │
│     - UI state (Idle/Manual/Seeking/Error)                   │
└─────────────────────────────────────────────────────────────┘
```

### Remote: The Thin Client

```
┌─────────────────────────────────────────────────────────────┐
│                  REMOTE (Thin Client)                        │
├─────────────────────────────────────────────────────────────┤
│ INPUTS:                                                      │
│   • Physical buttons → Send to board as protocol commands   │
│     - Left press → Send ButtonPress(Left)                   │
│     - Left release → Send ButtonRelease(Left)                │
│     - Up click → Send ButtonClick(Up)                        │
│     - etc.                                                    │
│                                                               │
│ REMOTE-SPECIFIC LOGIC:                                       │
│   • Battery monitoring ✓                                     │
│   • Sleep/wake on inactivity ✓                              │
│   • Connection management (pairing, reconnect) ✓             │
│   • Left long-hold → Sleep (remote only) ✓                  │
│   • Right click when disconnected → Pairing ✓                │
│                                                               │
│ DISPLAY:                                                      │
│   • Simply render what board says:                           │
│     - dm.currentPressurePSI = board's current PSI            │
│     - dm.targetPressurePSI = board's target PSI              │
│     - dm.viewType = board's UI state                         │
│     - dm.controllerActivity = board's controller status      │
│   • Plus remote-specific overlays:                           │
│     - dm.batteryPercentage                                   │
│     - dm.connectionStatus                                    │
│     - dm.pairingActive/Failed/Busy                           │
└─────────────────────────────────────────────────────────────┘
```

## Protocol Changes Required

### Current Protocol (Command-Based)

```cpp
enum class CommandCode {
  Start,   // Start seeking to target
  Idle,    // Cancel / idle
  Manual,  // Manual mode (streaming)
  Ping     // Keep-alive
};

struct Request {
  Kind kind;           // Start/Idle/Manual/Ping
  float targetPSI;     // For Start
  ManualMode mode;     // For Manual (Vent/Air)
};
```

**Problem**: High-level commands that require translation. Remote needs to "know" what Start/Manual/Idle mean.

### New Protocol (Button-Based)

```cpp
enum class ButtonCommand {
  Press,    // Button pressed down
  Release,  // Button released
  Click,    // Quick tap
  LongHold, // Held for duration
  Ping      // Keep-alive (no button)
};

enum class ButtonId {
  Left,
  Down,
  Up,
  Right
};

struct Request {
  ButtonCommand command;
  ButtonId button;
  float targetPSI;  // For Ping sync only
};
```

**Examples**:

- Remote user presses Down → Send `{Press, Down, 0}`
- Remote user releases Down → Send `{Release, Down, 0}`
- Remote user clicks Right → Send `{Click, Right, 0}`
- Remote keep-alive → Send `{Ping, Left, currentTargetPSI}`

The control board receives these and creates `ButtonEvent` objects **identical to physical buttons**.

## Implementation Plan

### Phase 1: Protocol Refactor ✅ (Already Done!)

The control board side is **already done** per `REMOTE_COMMAND_REFACTORING.md`:

- `TA_App::onRequest_()` translates protocol → ButtonEvent
- All events flow through `StateBoard::onButton()` → `TA_UI`
- Single state machine on the board

### Phase 2: Extend Protocol for Button Events

**File**: `pioLib/TA_Protocol/src/TA_Protocol.h`

Add new command codes:

```cpp
enum class CommandCode : uint8_t {
  // New button-based commands
  ButtonPress   = 'D',  // Button pressed (Down)
  ButtonRelease = 'U',  // Button released (Up)
  ButtonClick   = 'C',  // Button clicked
  ButtonLongHold = 'L', // Button long-held

  // Legacy (can be deprecated)
  Start  = 'S',
  Idle   = 'I',
  Manual = 'M',
  Ping   = 'P'
};

enum class ButtonId : uint8_t {
  Left  = 0,
  Down  = 1,
  Up    = 2,
  Right = 3
};

struct Request {
  enum class Kind {
    ButtonPress,
    ButtonRelease,
    ButtonClick,
    ButtonLongHold,
    Ping
  } kind = Kind::Ping;

  ButtonId button = ButtonId::Left;
  float targetPSI = 0.0f;  // For Ping sync only
};
```

**Serialization** (2 bytes):

```cpp
inline void packRequest(uint8_t output[2], const Request& req) {
  switch (req.kind) {
    case Request::Kind::ButtonPress:
      output[0] = static_cast<uint8_t>(CommandCode::ButtonPress);
      output[1] = static_cast<uint8_t>(req.button);
      break;
    case Request::Kind::ButtonRelease:
      output[0] = static_cast<uint8_t>(CommandCode::ButtonRelease);
      output[1] = static_cast<uint8_t>(req.button);
      break;
    case Request::Kind::ButtonClick:
      output[0] = static_cast<uint8_t>(CommandCode::ButtonClick);
      output[1] = static_cast<uint8_t>(req.button);
      break;
    case Request::Kind::ButtonLongHold:
      output[0] = static_cast<uint8_t>(CommandCode::ButtonLongHold);
      output[1] = static_cast<uint8_t>(req.button);
      break;
    case Request::Kind::Ping:
      output[0] = static_cast<uint8_t>(CommandCode::Ping);
      output[1] = convertPSIToByte(req.targetPSI);
      break;
  }
}
```

### Phase 3: Simplify Control Board's onRequest\_()

**File**: `pio/control_board/lib/TA_App/src/TA_App.cpp`

Replace the entire translation logic with direct mapping:

```cpp
void App::onRequest_(const trailair::protocol::Request& req) {
  using RK = trailair::protocol::Request::Kind;
  using trailair::input::ButtonEvent;
  using trailair::input::ButtonId;
  using trailair::input::ButtonAction;

  // Direct 1:1 mapping - no translation needed!
  switch (req.kind) {
    case RK::ButtonPress: {
      ButtonId id = static_cast<ButtonId>(req.button);
      state_.onButton(ButtonEvent{id, ButtonAction::Pressed, 0}, controller_);
      break;
    }

    case RK::ButtonRelease: {
      ButtonId id = static_cast<ButtonId>(req.button);
      state_.onButton(ButtonEvent{id, ButtonAction::Released, 0}, controller_);
      break;
    }

    case RK::ButtonClick: {
      ButtonId id = static_cast<ButtonId>(req.button);
      state_.onButton(ButtonEvent{id, ButtonAction::Click, 1}, controller_);
      break;
    }

    case RK::ButtonLongHold: {
      ButtonId id = static_cast<ButtonId>(req.button);
      state_.onButton(ButtonEvent{id, ButtonAction::LongHold, 1}, controller_);
      break;
    }

    case RK::Ping: {
      // Just sync target PSI, no button event
      if (req.targetPSI > 0.0f) {
        state_.setTargetPsi(req.targetPSI);
      }
      break;
    }
  }

  // That's it! No state tracking, no complex transitions, no duplicate logic!
}
```

### Phase 4: Gut TA_State (Remote Side)

**File**: `pio/remote/lib/TA_State/src/TA_State.h` and `.cpp`

**REMOVE**:

- ❌ `trailair::ui::UserInterfaceStateMachine ui_` - Duplicate state machine
- ❌ `RemoteState rState_` - Local state tracking (use board's state)
- ❌ `enter_()` method - State transitions happen on board
- ❌ `RemoteActions` bridge - No longer calling DeviceActions
- ❌ `handleButtonsDisconnected_()` - Buttons just send to board
- ❌ All the complex button translation logic
- ❌ Manual streaming logic (now just press/release events)

**KEEP** (Remote-specific logic):

- ✅ `onBatteryPercent()` - Battery monitoring
- ✅ `takeSleepRequest()` - Sleep management
- ✅ `resetAfterWake()` - Wake management
- ✅ `onPairEvent()` - Pairing state
- ✅ Connection tracking (`isConnected_`, `isConnecting_`)
- ✅ Pairing UI (`pairingFailed_`, `pairingBusy_`)

**NEW Simple Logic**:

```cpp
class StateController {
public:
  StateController(trailair::comms::EspNowLink& link);

  void begin();
  void update(uint32_t now, bool isConnected, bool isConnecting);

  // Input from board (master state)
  void onStatus(const trailair::protocol::Response& msg);

  // Input from local buttons → just forward to board!
  void onButton(const trailair::input::ButtonEvent& e);

  // Remote-specific
  void onBatteryPercent(int percent);
  void onPairEvent(trailair::comms::PairEvent ev, const uint8_t mac[6]);
  bool takeSleepRequest();
  void resetAfterWake();

  // Display (board is master for everything except remote overlays)
  void buildDisplayModel(trailair::display::DisplayModel& dm) const;

private:
  trailair::comms::EspNowLink& link_;

  // Board's state (received via onStatus)
  float currentPsi_ = 0.0f;
  float targetPsi_ = 0.0f;
  trailair::protocol::UIState uiState_ = trailair::protocol::UIState::Idle;
  trailair::protocol::StatusCode controllerStatus_ = trailair::protocol::StatusCode::Idle;
  uint8_t lastErrorCode_ = 0;

  // Remote-only state
  int batteryPercent_ = 0;
  bool isConnected_ = false;
  bool isConnecting_ = false;
  bool sleepRequested_ = false;

  // Pairing
  bool pairingActive_ = false;
  bool pairingFailed_ = false;
  bool pairingBusy_ = false;

  // Sleep handling
  uint32_t lastButtonTime_ = 0;
  bool leftLongHoldSent_ = false;  // Prevent duplicate long-holds
};
```

**Button Handling - Ultra Simple**:

```cpp
void StateController::onButton(const trailair::input::ButtonEvent& e) {
  lastButtonTime_ = millis();

  // Special: Left long-hold = sleep (remote only)
  if (e.id == ButtonId::Left && e.action == ButtonAction::LongHold) {
    if (!leftLongHoldSent_) {
      sleepRequested_ = true;
      leftLongHoldSent_ = true;
    }
    return;  // Don't send to board
  }

  if (e.action == ButtonAction::Released && e.id == ButtonId::Left) {
    leftLongHoldSent_ = false;  // Reset for next press
  }

  // Special: Right click when disconnected = start pairing (remote only)
  if (!isConnected_ && e.id == ButtonId::Right && e.action == ButtonAction::Click) {
    link_.startPairing(/*group*/, /*timeout*/);
    return;  // Don't send to board
  }

  // All other buttons: just forward to board!
  // Convert ButtonEvent → Protocol Request
  trailair::protocol::Request req;
  req.button = static_cast<trailair::protocol::ButtonId>(e.id);
  req.targetPSI = targetPsi_;  // Include current target for sync

  switch (e.action) {
    case ButtonAction::Pressed:
      req.kind = Request::Kind::ButtonPress;
      link_.sendButtonPress(req.button);
      break;
    case ButtonAction::Released:
      req.kind = Request::Kind::ButtonRelease;
      link_.sendButtonRelease(req.button);
      break;
    case ButtonAction::Click:
      req.kind = Request::Kind::ButtonClick;
      link_.sendButtonClick(req.button);
      break;
    case ButtonAction::LongHold:
      // Only send long-hold for non-Left buttons (Left is sleep)
      if (e.id != ButtonId::Left) {
        req.kind = Request::Kind::ButtonLongHold;
        link_.sendButtonLongHold(req.button);
      }
      break;
  }
}
```

**Display - Just Echo Board State**:

```cpp
void StateController::buildDisplayModel(trailair::display::DisplayModel& dm) const {
  // Board's state (master)
  dm.currentPressurePSI = currentPsi_;
  dm.targetPressurePSI = targetPsi_;

  // Map board's UI state to view
  switch (uiState_) {
    case protocol::UIState::Idle:    dm.viewType = ViewType::Idle;    break;
    case protocol::UIState::Manual:  dm.viewType = ViewType::Manual;  break;
    case protocol::UIState::Seeking: dm.viewType = ViewType::Seeking; break;
    case protocol::UIState::Error:   dm.viewType = ViewType::Error;   break;
  }

  // Map controller status to activity
  switch (controllerStatus_) {
    case StatusCode::Idle:     dm.controllerActivity = ControllerActivity::Idle;     break;
    case StatusCode::AirUp:    dm.controllerActivity = ControllerActivity::AirUp;    break;
    case StatusCode::Venting:  dm.controllerActivity = ControllerActivity::Venting;  break;
    case StatusCode::Checking: dm.controllerActivity = ControllerActivity::Checking; break;
    case StatusCode::Error:    dm.controllerActivity = ControllerActivity::Error;    break;
  }

  dm.lastErrorCode = lastErrorCode_;

  // Remote-specific overlays
  dm.batteryPercentage = batteryPercent_;
  dm.connectionStatus = isConnected_ ? ConnectionStatus::Connected : ConnectionStatus::Disconnected;
  dm.showReconnectHint = !isConnecting_;
  dm.pairingActive = pairingActive_;
  dm.pairingFailed = pairingFailed_;
  dm.pairingBusy = pairingBusy_;

  // These are board-only, but include for compatibility
  dm.seekingShowDoneHold = false;  // Board handles this, remote just displays
  dm.showBatteryIcon = true;       // Remote always shows battery
}
```

### Phase 5: Update TA_Comms (Remote Link)

**File**: `pio/remote/lib/TA_Comms/src/TA_Comms.h` and `.cpp`

Add simple button-sending methods:

```cpp
class EspNowLink {
public:
  // ...existing methods...

  // New button-based API (replaces sendStart/sendCancel/sendManual)
  void sendButtonPress(protocol::ButtonId button);
  void sendButtonRelease(protocol::ButtonId button);
  void sendButtonClick(protocol::ButtonId button);
  void sendButtonLongHold(protocol::ButtonId button);

  // Ping with target sync (keep existing)
  void sendPing(float targetPSI);

  // Legacy (can deprecate)
  void sendStart(float targetPSI);
  void sendCancel();
  void sendManual(uint8_t mode);
};
```

Implementation is trivial - just pack and send:

```cpp
void EspNowLink::sendButtonClick(protocol::ButtonId button) {
  protocol::Request req;
  req.kind = protocol::Request::Kind::ButtonClick;
  req.button = button;
  req.targetPSI = 0.0f;

  uint8_t payload[2];
  protocol::packRequest(payload, req);
  send_(payload, 2);
}
```

## Benefits of Thin Client Architecture

### 1. **Single Source of Truth**

- All UI logic lives in ONE place: board's `TA_UI::UserInterfaceStateMachine`
- Remote literally cannot get out of sync - it displays what board says

### 2. **Massive Code Reduction**

Remote's `TA_State.cpp`:

- **Before**: ~350 lines of complex state machine logic
- **After**: ~100 lines of simple forwarding + remote-specific features

### 3. **Testing Simplification**

- Test board's state machine once
- Test remote's button forwarding (trivial)
- Test remote-specific features (battery, sleep) in isolation
- No need to verify "remote and board stay in sync" - they can't diverge!

### 4. **Easier Maintenance**

- Add new UI features? Change board's `TA_UI` only
- Remote automatically gains new features through protocol
- No risk of forgetting to update remote's duplicate logic

### 5. **Better UX**

- Remote shows EXACTLY what board is doing
- No confusion about "which view am I in?"
- Works even if remote's logic lags behind board's updates

### 6. **Cleaner Separation**

| Component    | Responsibility                                        |
| ------------ | ----------------------------------------------------- |
| **Board**    | All UI logic, all pressure control, master state      |
| **Remote**   | Button input, display output, battery, sleep, pairing |
| **Protocol** | Transparent button events + state updates             |

## Migration Strategy

### Step 1: Add Button Protocol (Non-Breaking)

- Extend `TA_Protocol.h` with new button commands
- Keep legacy commands for now
- Both can coexist during migration

### Step 2: Update Board to Handle Button Protocol

- Simplify `TA_App::onRequest_()` to handle both old and new
- Test with physical buttons on board (already works)

### Step 3: Gut Remote's TA_State

- Remove duplicate state machine
- Implement simple button forwarding
- Keep remote-specific features

### Step 4: Update Remote's TA_Comms

- Add button-sending methods
- Wire up to new protocol

### Step 5: Integration Test

- Verify remote buttons → board state changes → remote display updates
- Test all scenarios: Manual, Seeking, Error, Idle
- Test remote-specific: sleep, pairing, battery

### Step 6: Remove Legacy Protocol (Optional)

- Once confident, remove old Start/Idle/Manual commands
- Clean up any backward compatibility shims

## Success Criteria

✅ Remote has NO state machine (`TA_UI` removed)
✅ Remote buttons just send protocol commands (no local state changes)
✅ Remote display shows board's state verbatim (no override logic)
✅ Remote-specific features still work (battery, sleep, pairing)
✅ All existing functionality preserved
✅ Code is simpler and easier to understand
✅ Tests pass

## File Changes Summary

### New Files

- None (just modify existing)

### Modified Files

**Protocol** (shared):

- `pioLib/TA_Protocol/src/TA_Protocol.h` - Add button-based protocol

**Control Board**:

- `pio/control_board/lib/TA_App/src/TA_App.cpp` - Simplify `onRequest_()` (trivial 1:1 mapping)

**Remote**:

- `pio/remote/lib/TA_State/src/TA_State.h` - Remove state machine, simplify
- `pio/remote/lib/TA_State/src/TA_State.cpp` - Gut complex logic, keep remote features
- `pio/remote/lib/TA_Comms/src/TA_Comms.h` - Add button send methods
- `pio/remote/lib/TA_Comms/src/TA_Comms.cpp` - Implement button send methods

### Deleted Code

- ~250 lines of duplicate state machine logic from `TA_State.cpp`
- Complex transition tracking from `TA_App.cpp` (already done in previous refactor)

## Conclusion

This refactoring transforms TrailAir from a "dual state machine" architecture (fragile, complex, bug-prone) to a clean "client-server" architecture where:

- **Board = Server**: Owns all state, runs all logic
- **Remote = Client**: Sends input, displays output, manages local hardware

The result is simpler, more maintainable, more testable, and impossible to desync.
