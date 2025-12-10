# Control Board Silent Boot Fix

## Problem Summary

After replacing the control board with a new, identical ESP32-C3:

- Board is completely silent on boot (no Serial output)
- Remote cannot pair to the board
- Display shows no boot logo

## Root Cause Analysis

### Primary Issue: Changed MAC Address

When you replaced the ESP32-C3 board, the **MAC address changed**. This causes:

1. Remote has old board's MAC stored in NVS (pairing memory)
2. Remote sends pairing requests to wrong MAC address
3. New board never receives pairing requests

### Secondary Issue: Possible Initialization Hang

On some ESP32-C3 boards, WiFi/ESP-NOW initialization can be timing-sensitive:

- `WiFi.mode(WIFI_STA)` called immediately after `Serial.begin()` can fail silently
- New board may have slightly different timing characteristics
- No error checking meant failures went undetected

## Changes Made

### 1. Added Diagnostic Output (TrailAir-ControlBoard.ino)

```cpp
void setup() {
  Serial.begin(115200);
  delay(500); // Give Serial time to initialize

  Serial.println("\n\n=== TrailAir Control Board Starting ===");

  // Print MAC address for pairing reference
  WiFi.mode(WIFI_STA);
  uint8_t mac[6];
  WiFi.macAddress(mac);
  Serial.printf("Board MAC: %02X:%02X:%02X:%02X:%02X:%02X\n", ...);

  app.begin();
  Serial.println("=== Control Board Ready ===\n");
}
```

**Benefits:**

- Shows board MAC address on every boot
- Confirms Serial is working
- Helps diagnose where initialization fails

### 2. Enhanced App Initialization (TA_App.cpp)

Added detailed logging for each subsystem initialization:

- Actuators
- Pressure sensor
- Controller
- **Comms (ESP-NOW)** - now checks return value!
- State
- Display

**Benefits:**

- See exactly where initialization fails
- Error checking on critical `comms_.begin()` call
- Display warnings for non-critical failures

### 3. Improved ESP-NOW Initialization (TA_CommsBoard.cpp)

```cpp
bool BoardLink::begin() {
  // Added: Detailed logging at each step
  // Added: 100ms delay after WiFi.mode() for stabilization
  // Added: Error code reporting for esp_now_init()
  // Added: Pairing request diagnostics
}
```

**Benefits:**

- 100ms stabilization delay prevents timing issues
- Error codes help diagnose ESP-NOW failures
- Verbose pairing diagnostics show exactly what's happening

### 4. Enhanced Pairing Diagnostics

Now logs:

- Every pairing request received with source MAC
- Group ID mismatches
- Already-paired conflicts with MAC addresses
- Successful pairing confirmations

## How to Fix Your System

### Step 1: Upload New Firmware

1. Build and upload the updated control board firmware
2. Open Serial Monitor (115200 baud)
3. Press RESET button on control board

### Step 2: Check Serial Output

You should see:

```
=== TrailAir Control Board Starting ===
Board MAC: XX:XX:XX:XX:XX:XX
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
    [BoardLink] Paired remote loaded: YY:YY:YY:YY:YY:YY
    [BoardLink] Ready
  [App] Initializing state...
  [App] Initializing display...
  [App] Initialization complete
=== Control Board Ready ===
```

**IMPORTANT:** Copy the "Board MAC: XX:XX:XX:XX:XX:XX" address - you'll need it!

### Step 3A: If Board Shows Old Paired Remote

The board may show "Paired remote loaded: YY:YY:YY:YY:YY:YY" (old remote's MAC).

**Solution:** Clear the pairing on the control board:

1. You need to add a way to call `comms_.forget()` or
2. Erase the NVS partition:
   - In PlatformIO: `pio run -t erase` (from control_board directory)
   - Or add a "forget" button feature

### Step 3B: Clear Remote's Pairing

The remote also has the OLD board's MAC stored. You need to:

1. On the remote, trigger the "forget pairing" function
2. Or erase the remote's NVS partition
3. Or re-upload the remote firmware (this clears NVS)

### Step 4: Re-Pair

1. Board should show: `[BoardLink] Unpaired. Waiting for PairReq...`
2. On remote, enter pairing mode (Right button click while disconnected)
3. Watch Serial Monitor - you should see:
   ```
   [BoardLink] Received PairReq from XX:XX:XX:XX:XX:XX, group=0x01
   [BoardLink] Not paired - accepting pairing request
   [BoardLink] Paired (saved); Ack sent.
   ```
4. Remote should connect successfully!

## Troubleshooting

### If Serial is Still Silent

- ESP32-C3 may be bricked or in wrong boot mode
- Try holding BOOT button while pressing RESET
- Check USB cable and connection
- Verify correct board selected in platformio.ini

### If ESP-NOW Init Fails

Serial will show: `[BoardLink] ESP-NOW init failed with error: 0xXXXX`

Common error codes:

- `0x101` (ESP_ERR_NO_MEM) - Insufficient memory
- `0x103` (ESP_ERR_INVALID_STATE) - WiFi not initialized properly

**Solutions:**

- Increase delay after Serial.begin to 1000ms
- Try adding `WiFi.mode(WIFI_OFF)` then `WiFi.mode(WIFI_STA)` with delays
- Check for other WiFi code conflicts

### If Board Doesn't Receive Pairing Requests

1. Verify remote is actually sending (check remote's Serial output)
2. Verify both devices use same channel (0 = auto)
3. Check group ID matches (default 0x01)
4. Ensure boards are within ~10m with clear line of sight

### If "Busy: already paired" Message

The board thinks it's paired to a different remote. Clear NVS and retry.

## Quick Reference: MAC Addresses

When pairing, keep track of:

- **Control Board MAC:** (from Serial at boot)
- **Remote MAC:** (from remote's Serial at boot)

These must match what's stored in each device's NVS "trailair" namespace, "peer" key.

## Prevention for Future Board Swaps

Consider adding to control board setup():

1. Print MAC on every boot (✅ done!)
2. Add button combo to clear pairing (e.g., hold button during boot)
3. Store board ID in NVS to detect board changes
4. Add "factory reset" command via Serial

## Next Steps

1. **Upload the updated firmware** to control board
2. **Check Serial output** to see initialization progress
3. **Note the new MAC address**
4. **Clear pairing on both devices**
5. **Re-pair using remote's pairing mode**

The enhanced diagnostics will show you exactly where things are working or failing!
