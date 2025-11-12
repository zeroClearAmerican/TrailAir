# TrailAir Testing

## Test Suite Overview

**Total: 137 unit tests** across both projects

### Remote Tests (69 tests)

- **test_protocol** (45 tests): Protocol encoding/decoding, pairing
- **test_errors** (12 tests): Error codes and text mapping
- **test_battery_simple** (8 tests): Battery voltage/percentage calculations
- **test_ui** (68 tests): UI state machine and button handling

### Control Board Tests (68 tests)

- **test_controller** (68 tests): State machine, PSI seeking, error handling, manual control

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
