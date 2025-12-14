# Remote Command Refactoring - Control Board

## Summary

Successfully refactored the control board's remote command handling to use the shared ButtonEvent translation layer, eliminating custom state tracking and duplicated logic.

## Problem Statement

The control board was handling remote commands (Start, Idle, Manual, Ping) with custom logic in `TA_App.cpp::onRequest_()` that:

- Duplicated state machine logic already present in `TA_UI`
- Directly manipulated the controller AND sent events to the state machine
- Tracked manual button state with custom booleans (`remoteManualVentActive_`, `remoteManualAirActive_`)
- Was error-prone and complex due to streaming manual commands (sent every 100ms while held)

## Solution

Refactored remote commands to be translated into standard `ButtonEvent` objects that feed into the same `StateBoard::onButton()` → `TA_UI` state machine that physical buttons use.

### Key Changes

#### 1. TA_App.h - Removed Custom State Tracking

**Before:**

```cpp
bool remoteManualVentActive_ = false;
bool remoteManualAirActive_ = false;
```

**After:**

```cpp
// Remote command translation: track last command to detect transitions
trailair::protocol::Request::Kind lastRemoteCommand_ = trailair::protocol::Request::Kind::Idle;
trailair::protocol::ManualMode lastRemoteManualMode_ = trailair::protocol::ManualMode::Vent;
```

#### 2. TA_App.cpp - Clean Translation Layer

The `onRequest_()` method now:

- Translates protocol commands into ButtonEvent objects
- Tracks the last command to detect transitions (avoiding duplicate Press events)
- Feeds events to `state_.onButton()` just like physical buttons
- Lets the UI state machine handle ALL logic

### Protocol → ButtonEvent Mapping

| Remote Command                  | Translation                                                | Notes                                       |
| ------------------------------- | ---------------------------------------------------------- | ------------------------------------------- |
| `Request::Idle` (after Manual)  | `ButtonEvent{Down/Up, Released}`                           | Releases whichever manual button was active |
| `Request::Idle` (normal)        | `ButtonEvent{Left, Click}`                                 | Cancel/exit                                 |
| `Request::Start`                | Sync target PSI + `ButtonEvent{Right, Click}`              | Start seek operation                        |
| `Request::Manual` (first)       | `ButtonEvent{Left, Click}` to enter Manual view            | Only on transition                          |
| `Request::Manual` (Vent, first) | `ButtonEvent{Down, Pressed}`                               | Only on transition                          |
| `Request::Manual` (Vent, hold)  | No event                                                   | Continuing to hold                          |
| `Request::Manual` (Air, first)  | `ButtonEvent{Up, Pressed}`                                 | Only on transition                          |
| `Request::Manual` (Air, hold)   | No event                                                   | Continuing to hold                          |
| `Request::Manual` (mode switch) | `ButtonEvent{Old, Released}` + `ButtonEvent{New, Pressed}` | Switching between Air/Vent                  |
| `Request::Ping`                 | Sync target PSI only                                       | No button event                             |

### Transition Detection Logic

The refactoring solves the streaming manual command problem by tracking:

1. **Last command kind**: Detects transitions like `Idle → Manual` or `Manual → Idle`
2. **Last manual mode**: Detects switches between Vent and Air while in Manual

This allows the code to:

- Send `Pressed` only on the first Manual command
- Send `Released` when returning to Idle from Manual
- Handle mode switches (Vent ↔ Air) gracefully
- Ignore redundant streaming commands (no duplicate Press events)

## Benefits

### 1. **Unified Code Path**

Remote and physical buttons now flow through identical logic:

```
Remote Command → ButtonEvent → StateBoard → TA_UI State Machine
Physical Button → ButtonEvent → StateBoard → TA_UI State Machine
```

### 2. **Eliminated Duplicate Logic**

- Removed custom state tracking (`remoteManualVentActive_`, etc.)
- Removed direct controller calls from `onRequest_()`
- Removed UI state checks (`if (state_.uiState() != UiState::Manual)`)
- All logic now lives in one place: `TA_UI.cpp`

### 3. **Cleaner Separation of Concerns**

- `TA_App`: Protocol translation only
- `StateBoard`: Event forwarding and model building
- `TA_UI`: All state machine logic
- `Controller`: Actuator control only

### 4. **Easier to Test and Debug**

- Single source of truth for UI behavior
- Remote and local buttons guaranteed to behave identically
- Transition logic is explicit and traceable

### 5. **More Maintainable**

- Adding new commands only requires updating translation layer
- State machine changes automatically apply to both remote and local
- No risk of remote and local behavior diverging

## Implementation Details

### Command Transition Detection

```cpp
bool isTransitionToManual = (lastRemoteCommand_ != RK::Manual);
bool modeChanged = (lastRemoteManualMode_ != req.manualMode);
```

This simple state tracking enables:

- **First Manual**: `isTransitionToManual == true` → send Press event
- **Continuing Manual**: `isTransitionToManual == false` → no event
- **Mode Switch**: `modeChanged == true` → send Release + Press

### Idle Command Disambiguation

```cpp
if (lastRemoteCommand_ == RK::Manual) {
  // Release manual button
  ButtonId btnToRelease = (lastRemoteManualMode_ == ManualMode::Vent)
                           ? ButtonId::Down : ButtonId::Up;
  state_.onButton(ButtonEvent{btnToRelease, ButtonAction::Released}, controller_);
} else {
  // True cancel/exit
  state_.onButton(ButtonEvent{ButtonId::Left, ButtonAction::Click}, controller_);
}
```

The `Idle` command serves dual purpose:

1. Manual button release (when coming from Manual)
2. Cancel/exit button (when coming from other states)

Tracking `lastRemoteCommand_` lets us determine which interpretation is correct.

## Testing Recommendations

### Unit Tests

1. **Transition Detection**

   - Idle → Start → Idle sequence
   - Idle → Manual(Vent) → Idle sequence
   - Manual(Vent) → Manual(Air) → Idle sequence
   - Verify correct Press/Release events generated

2. **Streaming Commands**

   - Send 10x Manual(Vent) in a row
   - Verify only 1 Press event generated
   - Verify no duplicate events

3. **Edge Cases**
   - Start → Manual transition
   - Ping commands don't affect transitions
   - Multiple mode switches in quick succession

### Integration Tests

1. **Remote vs Local Equivalence**

   - Same sequence via remote and physical buttons should produce identical controller states
   - Example: Left click (enter Manual) → Down press (start vent) → Down release → Left click (exit)

2. **State Sync**
   - Target PSI sync via Ping and Start commands
   - UI state reporting back to remote

## Future Enhancements

### Potential Improvements

1. **Click Count Support**: Currently hardcoded to 0/1, could support double-click if protocol adds it
2. **Long Hold**: Could add long hold detection for remote commands if desired
3. **Debouncing**: Could add hysteresis if remote experiences spurious Idle commands

### Protocol Extensions

If the protocol is extended to support more commands:

1. Add new case to `onRequest_()` switch
2. Map to appropriate ButtonEvent
3. Update `lastRemoteCommand_` tracking if needed

No changes to `TA_UI` or `StateBoard` required - they just handle button events.

## Validation

### Code Metrics

- **Lines removed**: ~30 lines of custom logic
- **Lines added**: ~20 lines of translation logic
- **Net reduction**: ~10 lines
- **Complexity reduction**: Significant (eliminated dual code paths)

### Architecture Compliance

✅ Single Responsibility Principle - each component has one job
✅ DRY - no duplicate state machine logic
✅ Open/Closed - can extend with new commands without modifying state machine
✅ Dependency Inversion - TA_App depends on ButtonEvent abstraction

## Files Modified

1. **TA_App.h** - Replaced state booleans with transition tracking
2. **TA_App.cpp** - Complete rewrite of `onRequest_()` method

## Conclusion

This refactoring successfully unified the remote and local button handling through a clean translation layer. Remote commands now behave identically to physical button presses, with all state management handled by the existing, proven UI state machine. The code is simpler, more maintainable, and less error-prone.
