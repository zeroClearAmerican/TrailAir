# TrailAir Architecture & Developer Guide

> This document orients new engineers to the TrailAir codebase. It explains the two firmware "agents" (control board + remote), the shared libraries, the message protocol, major subsystems, and recommended practices for extending the system.

---

## 1. High-Level System

TrailAir consists of **two cooperating ESP32-C3 devices** communicating over ESP-NOW:

| Agent                               | Role                                                                                                                          | Typical Hardware                                                               |
| ----------------------------------- | ----------------------------------------------------------------------------------------------------------------------------- | ------------------------------------------------------------------------------ |
| Control Board (`pio/control_board`) | Drives compressor / vent actuators, reads pressure sensor, enforces safety/state logic, reports status & errors               | Pressure sensor (analog), compressor relay / MOSFET, vent valve, optional OLED |
| Remote (`pio/remote`)               | User interface (buttons + OLED), target PSI selection, manual control streaming, initiates pairing, monitors status & battery | 4 buttons, battery sense divider, OLED                                         |

They share a **unified protocol** (fixed two‑byte frames) plus a **shared UI state machine** so user interactions evolve consistently across both devices.

---

## 2. Repository Layout (Relevant Parts)

```
TrailAir/
  AGENTS.md (this doc)
  pio/                # PlatformIO projects (authoritative build roots)
    control_board/
      platformio.ini
      lib/TA_Actuators
      lib/TA_CommsBoard
      lib/TA_Sensors
      lib/TA_StateBoard
      src/TrailAir-ControlBoard.ino
    remote/
      platformio.ini
      lib/TA_Comms
      lib/TA_State
      lib/TA_Battery
      src/TrailAir-Remote.ino
  pioLib/             # Shared libraries (UI, protocol, controller, display, input, config, errors, app orchestrators)
    TA_App/
    TA_Config/
    TA_Controller/
    TA_Display/
    TA_Errors/
    TA_Input/
    TA_Protocol/
    TA_UI/
    SmartButton/
```

> Note: There are duplicate root-level project folders (legacy) mirroring `pio/*`. Focus on the `pio/` versions for active development/build.

---

## 3. Build & Run

### Prerequisites

- VS Code + PlatformIO extension (or CLI)
- Board: Seeed XIAO ESP32C3 (configured in `platformio.ini`)

### Build (VS Code Tasks)

Use the tasks you created (or create) named e.g.:

- `PIO: Build Remote`
- `PIO: Build Control Board`

### Build (CLI)

From the agent folder:

```
# Remote
platformio run -d pio/remote
# Control Board
platformio run -d pio/control_board
```

Upload with `platformio run -t upload -d <dir>` and open serial monitor with `platformio device monitor -b 115200`.

---

## 4. Shared Core Modules

### 4.1 `TA_Config`

Defines plain structs holding default configuration values used across agents:

- `UiShared`: min/max PSI range, default target, step size, post-seek "done" hold, error auto-clear window.
- `LinkShared`: link timeouts, manual resend cadence, pairing parameters.
  Remote state uses **pointers** to these (allowing injection / test overrides while keeping headers light).

### 4.2 `TA_Protocol`

Super-lean, allocation‑free, 2‑byte messages:

- **Status → Remote**: first byte is an ASCII letter (`Idle='I'`, `AirUp='U'`, `Venting='V'`, `Checking='C'`, `Error='E'`). Second byte is either packed PSI (0.5 PSI resolution) or error code.
- **Request → Board**: first byte (`Start='S'`, `Idle='I'`, `Manual='M'`, `Ping='P'`). Second byte is packed target PSI, manual code (`0x00 vent` / `0xFF air`), or unused.
- **Pairing Frames**: `PairOp` letters (`Req='R'`, `Ack='A'`, `Busy='B'`) sharing the same framing space.
- Helpers: `psiToByte05(float)`, `byteToPsi05(uint8_t)`; typed `Request` & `Response` structs.

### 4.3 `TA_UI`

Device-agnostic UI state machine encapsulating _view logic_ and _target PSI adjustments_:
Views: `Idle`, `Manual`, `Seeking`, `Error`, `Disconnected`, `Pairing`.

- Inputs via `ButtonEvent` (Button + Action).
- Device actions abstracted by `DeviceActions` (start seek, manual vent/air, cancel, clear error).
- Maintains _done hold_ after a successful seek, Auto-clears error after configured window through `DeviceActions::clearError`.
- The remote injects connectivity (may switch to `Disconnected`). Control board always appears connected internally.

### 4.4 `TA_Display`

Rendering facade for Adafruit SSD1306; decoupled via a `DisplayModel` struct (pure data). Includes layout helpers (two-column values, centered text) and battery/link/status/error/pairing icon logic.

### 4.5 `TA_Controller`

Control Board _pressure regulation_ state machine:
States: `IDLE`, `AIRUP`, `VENTING`, `CHECKING`, `ERROR`.

- Accepts `startSeek(targetPsi)`, `manualVent(on)`, `manualAirUp(on)`, `cancel()`, `clearError()`.
- Uses `IOutputs` abstraction; default `ActuatorAdapter` wraps `TA_Actuators` (compressor, vent).
- Tracks run phases (bursts + settling), checks progress vs tolerance & timeouts, escalates to error codes (`TA_Errors`).

### 4.6 `TA_Errors`

Central error catalog mapping numeric codes to short display strings. Controller’s internal `ErrorCode` enumerants map directly to catalog constants for wire/display consistency.

### 4.7 `TA_Input` + `SmartButton`

Button abstraction producing debounced semantic events: `Pressed`, `Released`, `Click`, `LongHold` through a lightweight event bus (`subscribe` / `unsubscribe`). It wraps `SmartButton` instances but removes global singletons by attaching context to callbacks.

### 4.8 `TA_App` / `TA_RemoteApp`

Orchestrators that wire subsystems together:

- **App (Board)**: Initializes actuators, sensors, controller, comms (`BoardLink`), UI state (`StateBoard`), display.
- **RemoteApp**: Initializes battery monitor, buttons, comms (`EspNowLink`), remote state (`StateController`), display, sleep/wakeup logic.
  They contain the main loops for each device’s sketch.

### 4.9 `TA_State` (Remote)

`StateController` integrates:

- UI state machine (shared)
- Link status + pairing flow
- Manual streaming resend cadence (`manualRepeatMs`)
- Sleep request detection (Left long-hold)
- Battery/error/current PSI aggregation for display
  Transitions remote-specific states (`DISCONNECTED`, `PAIRING`, etc.) and synchronizes them with UI `View`.

### 4.10 `TA_StateBoard` (Board)

Simplified wrapper to adapt controller state + board link to the shared UI machine (mirrors remote’s visual logic but excludes remote-specific pairing shortcuts). Builds a `DisplayModel` for on-board display if present.

### 4.11 `TA_Comms` (Remote) / `TA_CommsBoard` (Board)

ESP-NOW wrappers:

- **Remote (`EspNowLink`)**: Peer persistence (NVS), connection attempt / ping backoff, pairing initiation (group ID), status callback & pair event callback registration, request send helpers (`sendStart`, `sendManual`, `sendCancel`, `sendPing`).
- **Board (`BoardLink`)**: Receives requests, sends periodic `Status` or `Error`, persists single peer, responds to pairing requests with `Ack` or `Busy`, exposes `isRemoteActive(timeout)`.
  Both sides keep packet payload fixed-size (2 bytes) and wrap encoding/decoding with typed structs.

### 4.12 `TA_Actuators`

Thin façade around hardware control (set compressor, vent). Exposed to controller via `ActuatorAdapter` implementing `IOutputs`.

### 4.13 `TA_Sensors` / `PressureFilter`

Reads analog sensor → converts mV to PSI, applies rolling average / smoothing, noise threshold to zero out low noise. Supplies filtered PSI to the controller.

### 4.14 `TA_Battery`

Remote-only battery monitor:

- Rolling average & deadband
- Divider ratio scaling
- Percent computation with configurable voltage curve (vEmpty / vFull / low threshold)
  Feeds display via `StateController::buildDisplayModel`.

---

## 5. Pairing Flow Summary

1. Remote enters pairing (user Right click while disconnected) → `startPairing(groupId, timeoutMs)`.
2. Remote periodically transmits `PairOp::Req (R, groupId)`. Board responds:
   - If free: saves remote MAC, persists to NVS, replies `Ack (A)` and future status frames use peer.
   - If already paired (different MAC): replies `Busy (B)`.
3. Remote handles `Ack` (sets peer + reconnect) or `Busy` (shows busy/failure transient). Timeout auto-cancels.
4. Remote manual reconnect attempt occurs if user Right clicks while disconnected and pairing not desired.

---

## 6. Manual Control Streaming

While in _Manual_ view:

- UI toggles vent/compressor via `DeviceActions::manualVent / manualAirUp` → sets `manualSending_` flag.
- `StateController::update` resends last manual code every `manualRepeatMs` to maintain board action.
- Leaving manual (error or other view) triggers an immediate cancel.

---

## 7. Error Lifecycle

- Board detects condition (e.g., no pressure change, excessive time) → enters controller `ERROR` state, sends `Status::Error` + code.
- Remote: `onStatus` updates `cState_`, sets last error code, UI view becomes `Error`.
- UI _auto-clear_ occurs after configured `errorAutoClearMs` by issuing a `cancel`/`clearError` once.
- Subsequent recovery status (non-Error) returns view to `Idle`.

---

## 8. Display Data Flow

```
Controller / Sensors / Battery / Link
        ↓ (status + metrics)
   StateController / StateBoard
        ↓ (fills DisplayModel)
         TA_Display::render()
        ↓ (Adafruit SSD1306 API)
             Screen
```

Separation allows simulation / unit testing of logic without hardware display.

---

## 9. Coding Guidelines

### 9.1 Header Hygiene

- Avoid including `Arduino.h` in shared protocol / logic headers; prefer `<stdint.h>` and forward declarations.
- Use forward declarations for cross-module types in high-fanout headers (`TA_State.h`, `TA_StateBoard.h`).

### 9.2 Extending Protocol

1. Decide if change fits 2-byte frame; keep first byte semantic (command/status/pair op). If not, introduce a _new_ frame kind while maintaining backward compatibility.
2. Update `TA_Protocol` enums + pack/parse helpers.
3. Propagate to `EspNowLink` send helpers and `BoardLink` receive switch.
4. Update `StateController::onStatus` / board `App::onRequest_` as needed.
5. Adjust UI if user-visible.

### 9.3 Adding an Error Code

- Extend `TA_Errors` enumeration & `shortText` switch.
- Map controller internal `ErrorCode` to new catalog value.
- Ensure display or logs reference it via `lastErrorCode`.

### 9.4 UI Changes

- Prefer modifying shared `TA_UI` state logic rather than forking remote/board differences (device-specific overrides happen through `DeviceActions` or pre-filtering button events in state wrappers).

### 9.5 Timing / Config Tweaks

- Add fields to `UiShared` or `LinkShared`—keep defaults sensible.
- Remote: inject custom config by passing pointers in `StateController::Config` (currently defaulted inside constructor).

### 9.6 Testing Considerations

- Swap `IOutputs` implementation to mock actuator effects.
- Simulate reception of `Response` frames by calling `StateController::onStatus` directly.
- Inject synthetic button events to verify UI transitions.

### 9.7 Memory / Real-Time

- All protocol frames fixed at 2 bytes to minimize airtime & latency.
- Avoid dynamic allocation in hot paths; only a few single allocations (e.g., display wrapper, SmartButtons) occur during `begin`.

---

## 10. Common Extension Scenarios

| Goal                       | Where to Touch                                                          |
| -------------------------- | ----------------------------------------------------------------------- |
| New UI view/behavior       | `TA_UI` (enum `View`, update logic & rendering in `TA_Display`)         |
| New controller safety rule | `TA_Controller` (add detection & enterError\_)                          |
| New manual action type     | `TA_Protocol` (manual code), remote `RemoteActions`, controller mapping |
| Faster manual stream       | Adjust `LinkShared.manualRepeatMs`                                      |
| Alternate sensor scaling   | `TA_Sensors::PressureFilter::readPsi()`                                 |
| Battery percentage curve   | `TA_BatteryMonitor::recomputePercent_()` logic / config                 |

---

## 11. Sleep / Power (Remote)

- Long-hold Left triggers `sleepRequested_` within `StateController`.
- `RemoteApp::loop` checks `state_.takeSleepRequest()` then performs graceful sleep (logo animation, cancel manual, WiFi/ESP-NOW shutdown, light sleep, wake re-init).
- Inactivity timeout (5 mins) auto-sleeps.

---

## 12. Known Simplifications / Future Ideas

- Board currently hardcodes some config defaults (option to persist target PSI or tuning parameters in NVS).
- Expand pairing to multi-remote or multi-board scenarios (would require peer list & additional protocol bytes or index mapping).
- Add CRC / integrity (currently relying on ESP-NOW reliability + short frame length).
- Extend protocol for richer telemetry (temperature, voltage) — may need a variable-length mode or multiplexed opcodes.

---

## 13. Quick Start (Remote Dev Cycle)

1. Wire buttons + OLED per pin defines in `TrailAir-Remote.ino`.
2. Flash control board first (so it can Ack pairing).
3. Power remote, open serial monitor, Right click to pair (status shown on OLED).
4. Adjust target PSI with Up/Down in Idle; Right click to start; Left for manual.

---

## 14. Troubleshooting

| Symptom                          | Likely Cause                                  | Action                                                      |
| -------------------------------- | --------------------------------------------- | ----------------------------------------------------------- |
| Remote stuck Disconnected        | Peer not paired / lost                        | Right click to reconnect or re-pair                         |
| Manual stops after a few seconds | Missed repeat due to timing                   | Check `manualRepeatMs` and ensure `update()` runs (< ~50ms) |
| Error screen persists            | `errorAutoClearMs=0` or new error re-tripping | Validate error code stream / auto-clear setting             |
| Status PSI frozen                | Sensor noise threshold or board not sending   | Inspect board serial, verify `readPsi()` output             |

---

## 15. Glossary

- **Seek**: Automated regulation toward target PSI (AIRUP/VENTING bursts + CHECKING).
- **Manual Mode**: Direct control streaming (vent or air) until user releases.
- **Done Hold**: Short visual hold after successful seek completion.
- **Pairing Busy**: Board already paired with another MAC.

---

## 16. License / Ownership

(Insert licensing / attribution details if applicable.)

---

Happy hacking! Keep protocol lean, UI unified, and headers light.
