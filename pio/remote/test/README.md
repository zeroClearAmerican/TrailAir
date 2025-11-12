# TrailAir Unit Testing Guide

This directory contains unit tests for the TrailAir firmware. Tests run on your development machine using the PlatformIO native platform and Google Test framework.

## Quick Start

### Run All Tests (Remote)

From the `pio/remote` directory:

```bash
pio test -e native_test
```

### Run All Tests (Control Board)

From the `pio/control_board` directory:

```bash
pio test -e native_test
```

### Run Specific Test Suite

```bash
# Run only protocol tests
pio test -e native_test -f test_protocol

# Run only battery tests
pio test -e native_test -f test_battery

# Run only error tests
pio test -e native_test -f test_errors
```

### Run with Verbose Output

```bash
pio test -e native_test -v
```

## Test Structure

```
pio/remote/test/
├── test_protocol/
│   └── test_protocol.cpp    # Protocol pack/unpack, PSI conversion
├── test_battery/
│   └── test_battery.cpp     # Battery monitoring, percent calculation
└── test_errors/
    └── test_errors.cpp      # Error code catalog
```

## What's Tested

### ✅ Protocol (`test_protocol`)
- **PSI Conversion**: Float ↔ byte (0.5 PSI resolution)
- **Request Packing**: Start, Idle, Manual, Ping
- **Request Parsing**: Command decoding, validation
- **Response Parsing**: Status frames, error codes
- **Pairing Frames**: Req, Ack, Busy
- **Edge Cases**: Max PSI, zero PSI, invalid data
- **Round-trip**: Pack → Parse → Verify

**Coverage**: ~100% of `TA_Protocol.h`

### ✅ Battery (`test_battery`)
- **Voltage-to-Percent Math**: Calculation logic verification
- **Critical Detection**: ≤3.3V protection threshold
- **Low Battery**: Configurable threshold (default 15%)
- **Divider Ratios**: 2:1, 3:1 voltage divider correction
- **Clamping**: Below/above range handling (0-100%)
- **Config Validation**: Invalid config auto-correction
- **Realistic Scenarios**: Full discharge curve

**Note**: Battery tests focus on logic/algorithms without Arduino dependencies. 
Hardware integration (ADC, filtering) is tested manually on device.
See `BATTERY_TEST_README.md` for details.

**Coverage**: ~95% of battery logic (calculations only, not hardware)

### ✅ Errors (`test_errors`)
- **Error Codes**: All catalog constants
- **Text Mapping**: Code → display string
- **Display Constraints**: Text length validation
- **Protocol Compatibility**: Fits in uint8_t

**Coverage**: 100% of `TA_Errors.h`

## Adding New Tests

### 1. Create Test File

```cpp
// pio/remote/test/test_myfeature/test_myfeature.cpp
#include <gtest/gtest.h>
#include <MyFeature.h>

TEST(MyFeature, BasicTest) {
    EXPECT_EQ(1 + 1, 2);
}
```

### 2. Update platformio.ini (if needed)

Add include paths for new modules:

```ini
[env:native_test]
build_flags = 
    # ... existing flags
    -I../../pioLib/MyNewModule/src
```

### 3. Run Test

```bash
pio test -e native_test -f test_myfeature
```

## Mocking Hardware

For modules that depend on Arduino hardware (like `TA_Battery`), create minimal mocks:

```cpp
#ifdef UNIT_TEST
namespace {
    uint32_t mock_millis_value = 0;
    uint32_t millis() { return mock_millis_value; }
    
    int mock_sensor_value = 50;
    int analogRead(uint8_t pin) { return mock_sensor_value; }
}
#define Arduino_h // Prevent real Arduino.h inclusion
#endif
```

## Test-Driven Development Workflow

1. **Write test first** (describes desired behavior)
2. **Run test** → fails (red)
3. **Implement feature** → minimal code to pass
4. **Run test** → passes (green)
5. **Refactor** → improve code quality
6. **Run test** → still passes (green)

## CI/CD Integration

Tests can run automatically on every commit:

### GitHub Actions Example

```yaml
# .github/workflows/test.yml
name: Unit Tests
on: [push, pull_request]
jobs:
  test:
    runs-on: ubuntu-latest
    steps:
      - uses: actions/checkout@v3
      - uses: actions/setup-python@v4
        with:
          python-version: '3.x'
      - name: Install PlatformIO
        run: pip install platformio
      - name: Run Remote Tests
        run: cd pio/remote && pio test -e native_test
      - name: Run Control Board Tests
        run: cd pio/control_board && pio test -e native_test
```

## Debugging Tests

### Visual Studio Code

1. Install PlatformIO extension
2. Open test file
3. Click "Run Test" CodeLens above test function
4. Or set breakpoint and "Debug Test"

### Command Line

```bash
# Build with debug symbols
pio test -e native_test -v

# Run specific test with debugger (GDB)
cd .pio/build/native_test
gdb program
(gdb) run
```

## Coverage Analysis

Generate code coverage report (requires gcov):

```bash
pio test -e native_test --coverage
# Coverage report in .pio/build/native_test/coverage/
```

## Best Practices

### ✅ Do
- Test one thing per test function
- Use descriptive test names: `Feature_Scenario_ExpectedBehavior`
- Test edge cases: zero, max, negative, invalid
- Mock hardware dependencies
- Keep tests fast (< 1s total runtime)

### ❌ Don't
- Test Arduino library internals (assume they work)
- Include actual hardware in tests
- Write tests that depend on each other
- Test private implementation details (test behavior, not structure)

## Troubleshooting

### "undefined reference to..."
→ Missing source file in test. Add `#include` with `.cpp` or add to `platformio.ini` build sources.

### "Arduino.h: No such file"
→ Module needs mocking. See "Mocking Hardware" section above.

### Tests pass locally but fail in CI
→ Check for platform-specific assumptions (file paths, timing, endianness).

### Slow test execution
→ Reduce sample counts in test configs, mock delays, avoid real timers.

## Future Test Additions

Recommended tests to add:

- [ ] **TA_UI**: State machine transitions, button events
- [ ] **TA_Controller**: Control logic, error escalation, timing
- [ ] **TA_Config**: Default values, range validation
- [ ] **TA_Input**: Button debouncing, event generation
- [ ] **Integration**: End-to-end message flow (Remote → Board → Remote)

## Resources

- [Google Test Documentation](https://google.github.io/googletest/)
- [PlatformIO Testing Guide](https://docs.platformio.org/en/latest/advanced/unit-testing/index.html)
- [Test-Driven Development (TDD)](https://en.wikipedia.org/wiki/Test-driven_development)

---

**Questions?** See `AGENTS.md` for architecture details or create an issue.
