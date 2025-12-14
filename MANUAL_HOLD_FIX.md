# Manual Hold Button Fix

## Problem

When holding the inflate/deflate buttons in manual mode, the action would stop after ~1 second even though the button was still pressed. This happened on both the control board and remote.

## Root Cause

The issue was a combination of three factors:

### 1. Controller Watchdog (1000ms timeout)

`PressureController` has a manual mode watchdog that automatically deactivates manual control after 1000ms without a refresh:

```cpp
// In TA_Controller.h
uint32_t manualRefreshTimeoutMilliseconds = 1000;

// In TA_Controller.cpp update()
if (_isManualActive) {
  if (currentTimeMilliseconds - _lastManualRefreshTime > _config.manualRefreshTimeoutMilliseconds) {
    _isManualActive = false;
    stopAllOutputs();
    _state = ControllerState::Idle;
  }
}
```

The `_lastManualRefreshTime` is only updated when `manualAirUp(true)` or `manualVent(true)` is called.

### 2. SmartButton Event Sequence

SmartButton generates this event sequence when a button is held:

- `PRESSED` (immediate)
- `HOLD` (after 1000ms)
- `HOLD_REPEAT` (every 50ms while held)
- `LONG_HOLD` (after 2000ms)
- `LONG_HOLD_REPEAT` (every 50ms after that)

### 3. TA_Input Filtering

`TA_Input` was **filtering out** `HOLD` and `HOLD_REPEAT` events:

```cpp
// OLD CODE - caused the bug
static inline bool shouldForwardEvent(SmartButton::Event event) {
  // Skip HOLD and HOLD_REPEAT events - they interfere with manual mode press/release
  return event != SmartButton::Event::HOLD && event != SmartButton::Event::HOLD_REPEAT;
}
```

**Result**: User gets one `Pressed` event, then nothing for 1000ms, then controller watchdog times out and stops the action.

## Solution

### Part 1: Forward HOLD_REPEAT Events as Pressed

Modified `TA_Input.cpp` to:

1. Map `HOLD_REPEAT` and `LONG_HOLD_REPEAT` to `ButtonAction::Pressed`
2. Forward these events (only skip the initial `HOLD` event)

This ensures the UI layer receives repeated `Pressed` events every 50ms while the button is held.

### Part 2: Refresh Manual Mode on Every Pressed Event

Modified `TA_UI.cpp` Manual view handler to:

1. Remove the `!_isManualVentActive` check before calling `manualVent(true)`
2. Remove the `!_isManualAirActive` check before calling `manualAirUp(true)`

This ensures `manualAirUp(true)` or `manualVent(true)` is called on **every** `Pressed` event (including the repeats), which refreshes the watchdog timestamp.

## Files Changed

- `pioLib/TA_Input/src/TA_Input.cpp` - Event mapping and filtering
- `pioLib/TA_UI/src/TA_UI.cpp` - Manual view button handling

## Effect

- Button holds now work indefinitely (refreshed every 50ms via HOLD_REPEAT)
- Manual mode watchdog still provides safety (1000ms timeout if events stop)
- Works on both control board (local buttons) and remote (forwarded button events)
- No protocol changes needed - thin client architecture already forwards all button events

## Testing

Flash both devices and verify:

1. Hold Down button in manual mode - vent should stay open continuously
2. Hold Up button in manual mode - compressor should stay on continuously
3. Release button - action should stop immediately
4. Left long-hold still triggers sleep on remote (LONG_HOLD event at 2000ms)
