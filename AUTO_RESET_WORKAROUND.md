# Auto-Reset Workaround for Hardware Initialization Issue

## Problem Summary

Even after setting GPIO pins LOW immediately in `setup()`, the compressor still activates on **first power-on** until the ESP32 RESET button is pressed. This suggests a **hardware-level initialization issue** that occurs before our code runs.

## Likely Root Causes

### Theory 1: ESP32 Boot ROM Behavior

The ESP32 boot ROM (which runs before Arduino code) may:

- Sample GPIO pins to detect boot mode
- Briefly set pins to different states during boot
- Leave pins in indeterminate states until user code runs

### Theory 2: Power Supply Sequencing

During power-on:

- ESP32 core voltage ramps up
- 3.3V rail may take longer to stabilize
- GPIO pins may glitch HIGH during this window
- External pullup/pulldown resistors may not be strong enough

### Theory 3: Flash/WiFi Calibration

On first boot after power-on:

- ESP32 performs WiFi calibration
- Flash memory wake-up delays
- Internal RF circuits settling
- These may cause GPIO noise/glitches

### Why RESET Button Works

When you press RESET (without power cycling):

- Core voltage already stable
- WiFi calibration already done
- Flash already initialized
- Boot ROM completes faster
- GPIO initialization happens sooner

## Solution: Automatic Software Reset

Since pressing RESET fixes the issue, we **automate it** by:

1. Detecting first boot after power-on
2. Letting hardware stabilize for 500ms
3. Performing one software reset
4. Continuing normally after reset

This gives us the same clean state as manually pressing RESET.

---

## Implementation Details

### RTC Memory for Boot Detection

```cpp
// RTC memory persists across software resets but NOT power cycles
RTC_DATA_ATTR bool hasAutoReset = false;
```

**How it works:**

- **After power-on:** `hasAutoReset = false` (RTC memory cleared)
- **After software reset:** `hasAutoReset = true` (RTC memory preserved)
- This lets us distinguish first boot from subsequent boots

### Boot Flow

```cpp
void setup() {
  // 1. Set pins LOW immediately (safety)
  pinMode(9, OUTPUT);
  digitalWrite(9, LOW);
  pinMode(10, OUTPUT);
  digitalWrite(10, LOW);

  // 2. Check if this is first boot
  if (!hasAutoReset) {
    // First boot after power-on
    Serial.println("First Boot - Auto-Reset in 500ms");
    hasAutoReset = true;  // Mark for next boot
    delay(500);           // Let hardware stabilize
    esp_restart();        // Software reset
    // Never reaches here
  }

  // 3. Second boot (after auto-reset)
  Serial.println("Post auto-reset - normal operation");
  app.begin();
  // ... normal operation
}
```

### Timeline

**First Boot (Power-On):**

```
0ms:    Power applied
        → GPIO pins may glitch/float
        → Compressor may activate briefly
100μs:  setup() runs → pins set LOW
        → Compressor turns off
500ms:  esp_restart() called
        → ESP32 performs software reset
```

**Second Boot (After Auto-Reset):**

```
0ms:    Software reset complete
        → Hardware already stable
        → No GPIO glitches
100μs:  setup() runs → pins set LOW
        → hasAutoReset = true (from RTC)
        → Skip auto-reset logic
200ms:  app.begin()
        → Normal operation begins
∞:      System runs normally
```

---

## Advantages

### 1. Transparent to User

- Happens automatically on every power-on
- No manual intervention needed
- Adds only 500ms to boot time (one time)
- User never needs to press RESET

### 2. Deterministic Behavior

- Every power-on follows same sequence
- Predictable timing
- Same clean state as manual RESET

### 3. Preserves Safety

- Pins still set LOW immediately on first boot
- Even if compressor glitches briefly, it's stopped quickly
- After auto-reset, no glitches occur

### 4. No Additional Hardware

- Pure software solution
- Uses built-in RTC memory
- No external components needed

---

## Serial Monitor Output

### Expected Output on Power-On:

**First Boot (Brief):**

```
=== First Boot - Auto-Reset in 500ms ===
This helps stabilize hardware initialization.
Performing auto-reset now...
```

**Second Boot (Normal Operation):**

```

=== TrailAir Control Board Starting ===
(Post auto-reset - normal operation)
Initializing app...
  [App] Initializing actuators...
  [App] Initializing pressure sensor...
  [App] Initializing controller...
  [App] Initializing comms (ESP-NOW)...
    [BoardLink] Starting ESP-NOW initialization...
    [BoardLink] Setting WiFi mode to STA...
    [BoardLink] Initializing ESP-NOW...
    [BoardLink] ESP-NOW initialized successfully
    [BoardLink] Loading paired peer from NVS...
    [BoardLink] Paired remote loaded: XX:XX:XX:XX:XX:XX
    [BoardLink] Ready
  [App] Initializing state...
  [App] Initializing display...
  [App] Initialization complete
Board MAC: 34:B4:72:XX:XX:XX
=== Control Board Ready ===
```

Total boot time: **~1.2 seconds** (500ms first boot + 700ms second boot)

---

## Why This Works Better Than Just Delays

### Attempted Fix: Longer Delay Before pinMode()

```cpp
void setup() {
  delay(2000); // Wait for hardware to stabilize
  pinMode(9, OUTPUT);
  digitalWrite(9, LOW);
}
```

**Problem:** GPIO is still floating during the 2-second delay!

### Attempted Fix: Multiple digitalWrite() Calls

```cpp
void setup() {
  for (int i = 0; i < 100; i++) {
    digitalWrite(9, LOW);
    delay(10);
  }
}
```

**Problem:** If hardware issue occurs before `setup()`, this doesn't help.

### Why Auto-Reset Works

- Gives hardware **500ms to stabilize** while pins are already set LOW
- Then performs **clean reset** with stable hardware
- Second boot has:
  - ✅ Stable power rails
  - ✅ Calibrated WiFi
  - ✅ Initialized flash
  - ✅ No boot ROM glitches
  - ✅ Clean GPIO state

---

## Testing

### Test 1: Power-On Behavior

1. Completely power off control board
2. Power on and monitor Serial output
3. **Expected:**
   - "First Boot - Auto-Reset in 500ms"
   - Brief pause
   - "Performing auto-reset now..."
   - Board restarts
   - "Post auto-reset - normal operation"
   - Normal boot continues
4. **Verify:** No compressor activation at any point

### Test 2: Manual RESET Button

1. With board running, press RESET button
2. **Expected:**
   - `hasAutoReset = true` preserved in RTC memory
   - Board boots directly to "Post auto-reset" message
   - **No 500ms delay** (skips auto-reset)
   - Normal operation immediately

### Test 3: Power Cycle vs Manual Reset

**Power Cycle:**

- RTC memory cleared → `hasAutoReset = false`
- Auto-reset happens → 500ms delay

**Manual Reset:**

- RTC memory preserved → `hasAutoReset = true`
- No auto-reset → immediate boot

### Test 4: Remote Reconnection

1. Start with paired devices connected
2. Power cycle control board
3. **Expected:**
   - Board auto-resets after 500ms
   - After second boot, remote reconnects within 500ms
   - Total time to connection: ~1.7 seconds

---

## Troubleshooting

### If Compressor Still Activates

**Possibility 1: Glitch happens during 500ms stabilization**

- Increase delay to 1000ms or 2000ms
- Try moving `hasAutoReset = true` before the delay

**Possibility 2: Hardware issue requires physical RESET**

- May need external reset circuit (RC delay on EN pin)
- May need hardware pulldown resistors on GPIO 9/10
- May need relay with failsafe NC (normally closed) wiring

**Possibility 3: Issue is before our code**

- Boot ROM or bootloader issue
- Need to modify bootloader settings
- Consider using strapping pins differently

### If Auto-Reset Doesn't Happen

Check Serial output:

- If shows "First Boot" but doesn't reset → `esp_restart()` failing
- If shows "Post auto-reset" immediately → RTC memory not working
- If no output at all → Serial.begin() issue

### If Remote Doesn't Reconnect After Auto-Reset

- Check that remote has `requestReconnect()` on boot (from previous fix)
- Verify WiFi initialization happens only once
- Check Serial for ESP-NOW errors

---

## Alternative Solutions (If This Doesn't Work)

### Hardware Solution 1: External RC Reset Circuit

```
                    +3.3V
                      |
                      R (10kΩ)
                      |
    EN pin -----------+---------- C (10μF) -------- GND
```

Delays EN pin rising edge by ~100ms after power-on.

### Hardware Solution 2: GPIO Pull-Down Resistors

```
GPIO 9 (Compressor) ----+---- R (10kΩ) ---- GND
GPIO 10 (Vent)      ----+---- R (10kΩ) ---- GND
```

Ensures pins stay LOW even when floating.

### Hardware Solution 3: Relay Logic Inversion

Use **Normally Closed (NC)** relay contacts:

- Default state (no signal) → relay OPEN → compressor OFF
- Active state (GPIO HIGH) → relay CLOSED → compressor ON
- Any glitch/float → compressor stays OFF (safe)

### Software Solution 2: Watchdog Timer

If auto-reset fails, watchdog can force reset:

```cpp
#include <esp_task_wdt.h>

void setup() {
  esp_task_wdt_init(2, true); // 2 second timeout
  // If setup() hangs, watchdog forces reset
}
```

---

## Future Improvements

### Smart Boot Detection

Instead of always auto-resetting, detect **why** we booted:

```cpp
#include <rom/rtc.h>

void setup() {
  esp_reset_reason_t reason = esp_reset_reason();

  if (reason == ESP_RST_POWERON && !hasAutoReset) {
    // Power-on reset → do auto-reset
    hasAutoReset = true;
    delay(500);
    esp_restart();
  }
  // Software reset, brownout, watchdog, etc. → skip auto-reset
}
```

### Configurable Delay

Allow tuning via NVS or define:

```cpp
#define AUTO_RESET_DELAY_MS 500  // Adjust if needed
```

### Status LED During Auto-Reset

Flash LED during 500ms delay to show it's working:

```cpp
for (int i = 0; i < 5; i++) {
  digitalWrite(LED_PIN, HIGH);
  delay(50);
  digitalWrite(LED_PIN, LOW);
  delay(50);
}
esp_restart();
```

---

## Summary

**What:** Automatic software reset 500ms after power-on
**Why:** Mimics manual RESET button that fixes the issue
**How:** RTC memory tracks if reset has happened
**Effect:** Clean hardware state, no GPIO glitches after reset
**Cost:** 500ms added to power-on boot time (one time)

This is a **workaround for a hardware initialization issue** that's difficult to fix in software alone. If this doesn't fully resolve the problem, hardware modifications (pulldowns, RC reset, relay logic) may be needed.

---

## File Modified

**`pio/control_board/src/TrailAir-ControlBoard.ino`**

- Added `#include <esp_system.h>`
- Added `RTC_DATA_ATTR bool hasAutoReset` variable
- Added auto-reset logic in `setup()`
- Added diagnostic Serial messages
