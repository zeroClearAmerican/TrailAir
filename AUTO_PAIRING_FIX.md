# Automatic Re-Pairing Fix

## Problem Statement

When swapping out hardware (new control board or new remote), the devices couldn't pair automatically because:

1. Remote stored old control board's MAC address in NVS
2. Right-click in DISCONNECTED only attempted to **reconnect** to saved MAC, not re-pair
3. User had to manually clear NVS on both devices to pair again

**This was a poor UX** - swapping a board should "just work" with a simple button press.

---

## Solution: Always Allow Re-Pairing

### What Changed

#### 1. Remote: Always Initiate Pairing on Right-Click (DISCONNECTED)

**Before:**

```cpp
if (canStartPairing())
  link_.startPairing(...);  // Only if no peer saved
else
  link_.requestReconnect(); // Just ping old MAC forever
```

**After:**

```cpp
// Always try pairing - handles all scenarios gracefully
link_.startPairing(cfg_.link->pairGroupId, cfg_.link->pairTimeoutMs);
```

**File:** `pio/remote/lib/TA_State/src/TA_State.cpp`

**Why this works:**

- If control board is **unpaired** → accepts pairing, saves remote's MAC ✅
- If control board is **already paired to this remote** → sends Ack (re-confirms) ✅
- If control board is **paired to different remote** → sends Busy (user sees error) ✅
- If control board **changed (new MAC)** → accepts pairing, remote updates to new MAC ✅

#### 2. Remote: Switch Peer MAC When Re-Pairing

**Before:**

```cpp
// Just add new peer, didn't remove old one
if (!esp_now_is_peer_exist(mac)) {
  esp_now_add_peer(&pi);
}
```

**After:**

```cpp
// If switching to new peer MAC, remove old one first
if (hasPeer_ && memcmp(peer_, mac, 6) != 0) {
  Serial.printf("Switching from old peer to new peer\n");
  esp_now_del_peer(peer_);  // Remove old control board
}
// Then add and save new peer
savePeerToNVS(mac);
esp_now_add_peer(&pi);
```

**File:** `pio/remote/lib/TA_Comms/src/TA_Comms.cpp`

**Why this matters:**

- ESP-NOW has limited peer slots (20 total)
- Prevents accumulating dead peers
- Ensures communication goes to correct MAC

#### 3. Removed Unnecessary Gatekeeper

Removed `canStartPairing()` function and its check - no longer needed since we always allow pairing.

**Files Changed:**

- `pio/remote/lib/TA_State/src/TA_State.h` (removed declaration)
- `pio/remote/lib/TA_State/src/TA_State.cpp` (removed implementation)

---

## How It Works Now

### Scenario 1: New Remote + Existing Control Board

1. Remote has no saved peer (fresh or NVS cleared)
2. User presses **Right button** in DISCONNECTED
3. Remote broadcasts PairReq
4. Control board receives PairReq:
   - If **unpaired** → saves remote MAC, sends Ack
   - If **paired to different remote** → sends Busy
5. Remote receives Ack → saves control board MAC → connected! ✅

### Scenario 2: Existing Remote + New Control Board

1. Remote has **old control board MAC** saved
2. New control board has **different MAC**, unpaired
3. User presses **Right button** in DISCONNECTED
4. Remote broadcasts PairReq (ignores saved MAC)
5. New control board receives PairReq → saves remote MAC, sends Ack
6. Remote receives Ack with **new MAC**:
   - Removes old peer from ESP-NOW
   - Saves new MAC to NVS
   - Adds new peer to ESP-NOW
   - Connected to new board! ✅

### Scenario 3: Already Paired Devices (Re-confirmation)

1. Remote has control board MAC saved
2. Control board has remote MAC saved
3. User presses **Right button** (connection lost)
4. Remote broadcasts PairReq
5. Control board sees PairReq from **same MAC** it's paired to
6. Control board sends Ack (re-confirmation)
7. Remote receives Ack → already had this MAC → just reconnects ✅

### Scenario 4: Control Board Already Paired to Different Remote

1. Remote A tries to pair with control board
2. Control board already paired to Remote B
3. Remote A broadcasts PairReq
4. Control board sees different MAC → sends **Busy**
5. Remote A receives Busy → shows "Pairing Busy" on screen
6. User sees they need to unpair Remote B first ⚠️

---

## User Experience

### Before (Broken)

1. Swap control board
2. Remote shows "Disconnected"
3. Press Right button → "Reconnecting..." forever
4. **Doesn't work!** Must manually clear NVS on both devices

### After (Fixed)

1. Swap control board
2. Remote shows "Disconnected"
3. Press Right button → "Pairing..." → Success! ✅
4. **Just works!**

---

## Control Board Pairing Logic (Already Correct)

The control board's pairing logic in `TA_CommsBoard.cpp` already handled this correctly:

```cpp
void BoardLink::handlePairReq_(const uint8_t* mac, uint8_t group) {
  if (group != groupId_) return; // Wrong group

  if (!paired_) {
    // Unpaired: accept any pairing request
    savePeer_(mac);
    sendAck();
  } else {
    if (memcmp(mac, peer_, 6) == 0) {
      // Same remote: re-confirm
      sendAck();
    } else {
      // Different remote: reject
      sendBusy();
    }
  }
}
```

This means:

- ✅ Accepts new remotes when unpaired
- ✅ Re-confirms existing remote
- ✅ Rejects different remotes (prevents hijacking)

---

## Testing Scenarios

### Test 1: Fresh Pairing (Both Devices New)

1. Flash both devices (clears NVS)
2. Control board boots → "Unpaired. Waiting for PairReq..."
3. Remote boots → "Disconnected"
4. Press Right on remote
5. **Expected:** "Pairing..." → Success → Both show connected

### Test 2: Replace Control Board

1. Start with working system (both paired)
2. Flash new control board (different ESP32)
3. New board boots → "Unpaired. Waiting for PairReq..."
4. Remote boots → "Disconnected" (can't reach old MAC)
5. Press Right on remote
6. **Expected:** Remote auto-switches to new board MAC → Success

### Test 3: Replace Remote

1. Start with working system
2. Flash new remote (different ESP32)
3. Control board boots → "Paired remote loaded: XX:XX:XX:XX:XX:XX" (old remote)
4. New remote boots → "Disconnected"
5. Press Right on new remote
6. **Expected:** Control board sees different MAC → "Busy: already paired"
7. Must clear pairing on control board first (or use different group ID)

### Test 4: Re-Pairing Same Devices

1. Paired system, connection lost (out of range, etc.)
2. Remote shows "Disconnected"
3. Press Right on remote
4. **Expected:** Quick re-confirmation → Connected

---

## Advanced: Multiple Remotes

If you want to use **multiple remotes** with one control board, you have two options:

### Option A: Manual Switching (Current System)

1. Pair Remote A → control board accepts
2. Pair Remote B → control board sends **Busy** (already paired to A)
3. To switch: call `comms_.forget()` on control board, then pair Remote B

### Option B: Group IDs (Future Enhancement)

Each remote uses a different `pairGroupId`:

- Remote A: `groupId = 0x01`
- Remote B: `groupId = 0x02`

Control board maintains map of `{groupId → MAC}`.

**Not implemented yet** - current system uses single peer only.

---

## Diagnostics Added

With the enhanced logging from `CONTROL_BOARD_FIX.md`, you'll see:

**Control Board Serial:**

```
=== TrailAir Control Board Starting ===
Board MAC: 34:B4:72:XX:XX:XX
...
    [BoardLink] Unpaired. Waiting for PairReq...
    [BoardLink] Ready
=== Control Board Ready ===

[When pairing request arrives]
    [BoardLink] Received PairReq from A1:B2:C3:XX:XX:XX, group=0x01
    [BoardLink] Not paired - accepting pairing request
    [BoardLink] Paired (saved); Ack sent.
```

**Remote Serial (with TA_COMMS_DEBUG):**

```
ESP-NOW initialized
ESP-NOW peer ready XX:XX:XX:XX:XX:XX  (old MAC)

[User presses Right]
[PAIR] Started
[Pairing] Switching from old peer 34:B4:72:AA:BB:CC to new peer 34:B4:72:XX:XX:XX
[PAIR] Acked
[PAIR] Saved
```

---

## Summary

**The fix enables automatic re-pairing** by:

1. Removing the "only pair if no peer saved" restriction
2. Always broadcasting pairing requests when user presses Right in DISCONNECTED
3. Properly switching ESP-NOW peers when MAC changes
4. Relying on control board's existing Busy/Ack logic for conflict resolution

**Result:** Swapping hardware now requires a single button press, not manual NVS clearing.

---

## Files Modified

1. **`pio/remote/lib/TA_State/src/TA_State.cpp`**

   - Removed `canStartPairing()` check
   - Always call `startPairing()` on Right-click in DISCONNECTED
   - Removed `canStartPairing()` implementation

2. **`pio/remote/lib/TA_State/src/TA_State.h`**

   - Removed `canStartPairing()` declaration

3. **`pio/remote/lib/TA_Comms/src/TA_Comms.cpp`**

   - Enhanced `handlePairFrame_()` to remove old peer before adding new one
   - Added debug logging for peer switching

4. **`pio/control_board/lib/TA_CommsBoard/src/TA_CommsBoard.cpp`**
   - _(Already correct - no changes needed for this feature)_
   - Enhanced logging in previous fix

---

## Migration Notes

### For Existing Users

If you have devices that are already paired:

- **No action needed** - everything works as before
- Re-pairing same devices just re-confirms the existing pairing
- Connection behavior unchanged

### For New Deployments

- Flash both devices
- Press Right on remote
- Done! No manual NVS clearing ever needed

### For Board Swaps

- Replace hardware
- Press Right on remote
- Remote automatically switches to new MAC
- Old MAC removed from ESP-NOW peer list

---

## Future Enhancements

1. **Multi-Remote Support**

   - Extend control board to support multiple group IDs
   - Track multiple paired remotes
   - Respond to requests based on group ID

2. **Forget Button**

   - Add physical button combo to clear pairing (e.g., hold button during boot)
   - Useful for factory reset scenarios

3. **Pairing Timeout Improvement**

   - Show countdown timer during pairing
   - Auto-retry if timeout (with user confirmation)

4. **Security**
   - Add ESP-NOW encryption (requires shared key distribution)
   - Pairing PIN codes for high-security deployments
