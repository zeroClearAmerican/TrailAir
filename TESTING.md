# TrailAir Testing

## Test Suite Overview

**Total: 174 unit tests** across both projects (actively tested in CI)

### Remote Tests (107 tests)

- **test_protocol** (45 tests): Protocol encoding/decoding, pairing
- **test_errors** (12 tests): Error codes and text mapping
- **test_battery_simple** (8 tests): Battery voltage/percentage calculations
- **test_ui** (68 tests): UI state machine and button handling (✅ Bug fixed: Disconnected→Idle)
- **test_time** (38 tests): Overflow-safe timeout utilities + MockTime abstraction (✅ NEW)

### Control Board Tests (67 tests)

- **test_controller** (67 tests): State machine, PSI seeking, error handling, manual control

### Additional Tests (Created, Not Yet in CI)

- **test_comms** (16 tests): ESP-NOW ISR safety, connection management - _Requires ESP32 HAL mocking_
- **test_comms_board** (19 tests): Board-side ESP-NOW safety - _Requires ESP32 HAL mocking_

**Note:** Comms tests validate ISR safety hardening applied to production code. They require complex ESP32 hardware abstraction mocking and will be enabled in Phase 2.

## Code Hardening

### ISR Safety (Completed ✅)

**See:** [ISR_SAFETY_HARDENING.md](ISR_SAFETY_HARDENING.md)

Production code hardened against race conditions in ESP-NOW communication layer:

- Added FreeRTOS spinlocks (`portMUX_TYPE`) for ISR-safe access
- Protected shared state (`lastSeenMs_`, `lastRxMs_`) with critical sections
- Made shared variables `volatile`
- Zero functional impact, significant safety improvement

### Time/Timeout Overflow Safety (Completed ✅)

**See:** [TIME_OVERFLOW_HARDENING.md](TIME_OVERFLOW_HARDENING.md)

Production code hardened against `millis()` overflow (occurs every ~49.7 days):

- Created overflow-safe timeout utilities (`TA_Time.h`)
- Replaced unsafe `>=` comparisons with overflow-safe `hasElapsed()` / `isTimeFor()`
- 35 comprehensive tests covering normal and overflow scenarios
- Zero functional impact, prevents connection loss after 49 days of uptime

## Running Tests

### GitHub Actions (Recommended)

Tests run automatically on push/PR to `main` or `develop` branches. Check the Actions tab for results.

### Local Testing

**Note:** Windows native platform has a known bug. Use GitHub Actions or Linux/WSL.

```bash
cd pio/remote
pio test -e native_test

cd pio/control_board
pio test -e native_test
```

## Test Structure

Each test file follows a consistent pattern:

1. **Mock/Fixture Setup** - Mock dependencies and test fixtures
2. **Test Categories** - Organized by feature area
3. **Clear Naming** - `Category_Action_ExpectedResult`
4. **Main Function** - Standard Google Test entry point

## Coverage

### Well Tested ✅

- Protocol encoding/decoding
- Error catalog
- Battery protection logic
- UI state machine (views, buttons, transitions)
- Controller state machine (seeking, manual, errors)

### Not Yet Tested ⏳

- Display rendering (visual testing needed)
- Hardware abstractions (integration testing needed)
- Communications (ESP-NOW pairing/messaging)
- SmartButton (requires time mocking)
