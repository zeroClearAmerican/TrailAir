# TrailAir Architecture & Developer Guide

> This document orients new engineers to the TrailAir codebase. It explains the two firmware "agents" (control board + remote), the shared libraries, the message protocol, major subsystems, and recommended practices for extending the system.

---

## 1. High-Level System

TrailAir consists of **two cooperating ESP32-C3 devices** communicating over ESP-NOW:

| Agent                               | Role                                                                                                                                      | Typical Hardware                                                       |
| ----------------------------------- | ----------------------------------------------------------------------------------------------------------------------------------------- | ---------------------------------------------------------------------- |
| Control Board (`pio/control_board`) | **Master.** Runs the UI state machine and pressure controller, drives compressor/vent, reads the sensor. Fully usable on its own buttons. | Pressure sensor (analog, via divider), compressor + vent relays, OLED, 4 buttons |
| Remote (`pio/remote`)               | **Thin client.** Forwards button events, displays the board's status. Owns only pairing, sleep and battery.                              | 4 buttons, battery sense divider, OLED                                 |

The board never depends on the remote: every screen and action works from the board's own buttons. Both screens render the same `DisplayModel` through the same code, so they match; the remote only adds its link and battery icons and its own Disconnected/Pairing screens.

---

## 2. Repository Layout

```
TrailAir/
  AGENTS.md (this doc)
  pio/                       # PlatformIO projects (build roots)
    control_board/
      lib/TA_Actuators       # compressor/vent outputs (implements IActuatorOutputs)
      lib/TA_Sensors         # pressure reading + calibration
      lib/TA_BoardUI         # board UI state machine (the master UI)
      lib/TA_CommsBoard      # BoardLink: pair accept/busy, status, remote frames
      lib/TA_App             # board orchestrator / main loop
      test/test_seek         # controller vs. simulated tire (native)
    remote/
      lib/TA_Comms           # RemoteLink: connection, reconnect pings, pairing
      lib/TA_State           # remote thin-client state
      lib/TA_Battery
      lib/TA_RemoteApp       # remote orchestrator / main loop / sleep
  pioLib/                    # shared libraries
    TA_Types/                # shared enums: ButtonId/Action/Event, ControllerState, View
    TA_Protocol/             # wire format v2
    TA_Link/                 # shared ESP-NOW core (radio, peer, NVS, rx queue)
    TA_Controller/           # pressure controller (pure logic, testable)
    TA_Display/              # SSD1306 rendering
    TA_Input/ SmartButton/   # debounced buttons
    TA_Config/ TA_Errors/ TA_Time/
```

---

## 3. Build & Run

```
platformio run -d pio/remote
platformio run -d pio/control_board
```

Upload with `platformio run -t upload -d <dir>`, monitor with `platformio device monitor -b 115200`.

Native unit tests (also run by CI): `platformio test -e native_test` from either project folder. Hardware libraries (anything using Arduino, ESP-NOW or the SSD1306) are `lib_ignore`d in `native_test`; keep logic that needs testing out of them.

**Flash both devices together** whenever `TA_Protocol` changes (frames carry a version byte; mismatched firmware ignores each other).

---

## 4. Shared Core Modules

### 4.1 `TA_Types`

The single definition of `ButtonId`, `ButtonAction`, `ButtonEvent`, `ControllerState` and `View`. Enums that go over the air use their wire character as their value (`ControllerState::AirUp = 'U'`), so encoding is a cast. Don't add parallel enums in other modules.

`View` is the screen: `Idle, Manual, Seeking, Done, Error` are owned by the board and sent to the remote; `Disconnected, Pairing` are remote-side only.

### 4.2 `TA_Protocol` (wire format v2)

Every frame is `[MAGIC, type, payload...]`, fixed length per type, validated by `parse()`:

| Direction      | Frame                                   | Bytes                                      |
| -------------- | --------------------------------------- | ------------------------------------------ |
| Remote → board | ButtonPress/Release/Click/LongHold `D U C L` | `[M, t, buttonId]`                     |
| Remote → board | Ping `P`, PairRequest `R` (broadcast)   | `[M, t]`                                   |
| Board → remote | Status `S`                              | `[M, 'S', state, view, psi, target, err]`  |
| Board → remote | PairAck `A`, PairBusy `B` (broadcast)   | `[M, t]`                                   |

PSI bytes are 0.5 PSI units. Bump `MAGIC` on incompatible changes. The board rounds PSI to whole numbers before sending, so the remote shows exactly the board's number.

### 4.3 `TA_Link`

ESP-NOW plumbing shared by both devices: radio up/down, the one paired peer (registered with ESP-NOW, persisted in NVS), frame validation, and a receive queue. The ESP-NOW callback runs on the WiFi task and only enqueues; `poll()` dispatches to the derived class's `onFrame_()` on the main loop, so all role logic is single-threaded. `RemoteLink` and `BoardLink` derive from it and add only their role.

### 4.4 `TA_Controller`

Pressure seek + manual control, pure logic with time passed in (no `millis()`), driving an `IActuatorOutputs`.

- **Seek:** probe bursts learn inflate/vent rates, then predicted runs aim just short of target, with settle/check pauses. Decisions use **settled** readings only (while air flows the sensor reads the hose, not the tire); live readings only enforce the max-PSI cap. Probes are sized so even `maximumExpectedRatePSIPerSecond` can't overshoot. Stalls → `NoChange`, impossible seeks → `ExcessiveTime`.
- **Manual lease:** `manualAirUp/manualVent(true, now)` start or renew a lease of `manualLeaseMilliseconds`. Callers renew while the button is held (board buttons each loop via `BoardUI::update`; the remote by repeating ButtonPress every `manualRepeatIntervalMilliseconds`). If renewals stop — a lost release, a dead remote — the controller stops itself. Manual air also stops with `OverPressure` at `maximumPSI`.
- `ControllerConfig` holds the tuning and the **only** pressure limits; the UI reads them via `getConfig()`.

### 4.5 `TA_Display`

Renders a `DisplayModel` (plain data). Redraws only when something visible changes (a full redraw is ~13 ms of blocking I2C). No heap use per frame.

### 4.6 `TA_Input` + `SmartButton`

Debounced `Pressed/Released/Click/LongHold` events; `isHeld()` for the physical state. `Click.clickCount` > 1 means rapid taps were merged — Up/Down treat each as a step; Left/Right ignore the count so a double tap can't toggle a mode twice.

### 4.7 `TA_Config`, `TA_Errors`, `TA_Time`

UI timing and link timing defaults; the error catalog (codes travel in the status frame's `err` byte); overflow-safe time helpers.

---

## 5. Board

- **`TA_BoardUI`** — the master UI. Screens and buttons:
  - Idle/Done: Left = Manual, Right = seek, Up/Down = target. Done shows "Done!" briefly after a seek, then Idle.
  - Manual: hold Up = air, hold Down = vent, Left = back.
  - Seeking: Right = cancel.
  - Error: Right = acknowledge (auto-clears after `errorAutoClearDurationMilliseconds`).

  Board and remote buttons both land in `onButton()`. `status()` is what the remote displays.
- **`TA_App`** loop: buttons → link → sensor → `ui.update` (renews leases) → `controller.update` → `actuators.service` → status → render. Status goes out every 200 ms while the remote is active, and immediately after each remote frame.
- **`TA_Actuators`** — compressor and vent are mutually exclusive; each stays off ≥500 ms before turning back on (deferred, not dropped).
- **`TA_Sensors`** — calibration constants (`DIVIDER_GAIN`, `SENSOR_ZERO_V`, `PSI_PER_SENSOR_V`) for the 0.5–4.5 V / 0–150 PSI sensor behind a 3.3k/4.7k divider. Verify against a gauge.

---

## 6. Remote

- **`TA_State`** forwards buttons while connected (releases always), shows the board's status, and re-sends ButtonPress for held Up/Down.
- **`TA_Comms` (`RemoteLink`)** — connected = heard from the board within `connectionTimeoutMilliseconds`; while paired and disconnected it pings with backoff; keep-alive while connected.
- **Sleep** — Left long-hold or 5 min without input. Releases any held manual button, turns the radio off, waits for Left to be released (it is the low-level wake source), and swallows the wake press so it never reaches the board. On wake: fresh battery reading, radio back up, reconnect.
- **Critical battery** forces sleep (radio off) until the battery recovers.

---

## 7. Pairing

1. Remote: Right click while disconnected → broadcasts PairRequest every 500 ms for up to 10 s. Right click again cancels.
2. Board: unpaired → pairs with the requester and replies PairAck; paired to that same remote → re-acks; paired to another remote → broadcasts PairBusy.
3. Remote: on Ack it saves the board and connects. On Busy it keeps asking for 2 s in case a free board answers, then shows "Device Busy". Timeout shows "No Device".
4. **New or replacement remote:** long-hold Right on the board's own buttons. The board forgets its remote and shows "Pairing" for up to 30 s (Right click closes it).

---

## 8. Coding Guidelines

- Keep shared logic headers free of `Arduino.h` so they build natively (`TA_Types`, `TA_Protocol`, `TA_Controller`, `TA_Config`, `TA_Errors`).
- New shared vocabulary goes in `TA_Types`, not a module-local enum.
- **Protocol changes:** edit `FrameType`/`pack`/`parse`, bump `MAGIC`, update `test_protocol`, then `RemoteLink`/`BoardLink::onFrame_`.
- **New error:** add it to `TA_Errors` (code + short text), raise it with `enterError_()` in the controller. It reaches both screens via the status frame.
- **UI behavior:** change `TA_BoardUI` only — the remote mirrors it automatically. Keep both screens identical apart from the remote's link/battery icons and Disconnected/Pairing screens.
- **Seek tuning:** change `ControllerConfig`, run `test_seek`, then verify on a real tire.
- Avoid heap allocation outside `begin()`.

---

## 9. Troubleshooting

| Symptom                                   | Likely Cause                                     | Action                                                            |
| ----------------------------------------- | ------------------------------------------------ | ----------------------------------------------------------------- |
| Remote stuck Disconnected                 | Board off/out of range, or paired elsewhere      | It reconnects automatically; else Right click to pair            |
| Remote shows "Device Busy"                | Board paired to another remote                   | Long-hold Right on the board, then pair again                     |
| Manual stops after ~1 s                   | Lease not renewed                                | Check remote repeat interval < `manualLeaseMilliseconds`         |
| Seek ends with "No change"                | Compressor/valve not moving air, or slow manifold | Check plumbing; lower `noChangeThresholdPSI` for multi-tire fills |
| Readings off vs. gauge                    | Calibration                                      | Adjust `TA_Sensors` constants                                     |
| Devices ignore each other after flashing  | Protocol version mismatch                        | Flash both                                                        |
