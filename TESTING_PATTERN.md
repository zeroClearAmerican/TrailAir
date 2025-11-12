# Unit Test Pattern Guide

This document describes the consistent testing pattern used across all TrailAir unit tests.

## File Structure Pattern

```cpp
/**
 * Unit tests for [Module Name]
 * Brief description of what this tests
 */

#include <gtest/gtest.h>
#include <ModuleHeader.h>
// Additional includes as needed

using namespace appropriate::namespace;

// ============================================================================
// Mock Classes - Injectable Dependencies
// ============================================================================
class MockDependency : public InterfaceClass {
public:
    // Tracking variables
    int callCount = 0;
    Type lastValue;

    // Override interface methods
    void method(Type param) override {
        callCount++;
        lastValue = param;
    }

    // Reset helper for test setup
    void reset() {
        callCount = 0;
        lastValue = DefaultValue;
    }
};

// ============================================================================
// Test Fixture - Common Setup
// ============================================================================
class ModuleTest : public ::testing::Test {
protected:
    MockDependency mock;
    ModuleUnderTest module;
    Config cfg;

    void SetUp() override {
        // Configure with fast/test-friendly values
        cfg.timeout = 100;
        cfg.minValue = 0.0f;

        module.begin(&mock, cfg);
        mock.reset();
    }

    // Helper methods
    Type makeTestData(...) {
        return Type{...};
    }
};

// ============================================================================
// Test Category - Initialization
// ============================================================================
TEST_F(ModuleTest, InitialState) {
    EXPECT_EQ(module.state(), Expected);
    EXPECT_EQ(module.value(), 0);
}

// ============================================================================
// Test Category - Feature Area
// ============================================================================
TEST_F(ModuleTest, Feature_Action_ExpectedResult) {
    // Arrange
    module.setValue(10);

    // Act
    module.doSomething();

    // Assert
    EXPECT_EQ(module.getValue(), 10);
    EXPECT_EQ(mock.callCount, 1);
}

// ============================================================================
// Main function - Standard Entry Point
// ============================================================================
int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
```

## Naming Conventions

### Test Names

Format: `Category_Action_ExpectedResult`

Examples:

- `Idle_UpButton_IncreasesTarget`
- `Manual_DownPress_ActivatesVent`
- `StartSeek_ClampsMaxPsi`
- `Error_AutoClear_AfterTimeout`

### Categories

Organized by functional area with clear section headers:

- Initialization Tests
- [Feature] Tests
- State Transition Tests
- Error Condition Tests
- Edge Cases

### Mock Classes

- Prefix with `Mock`: `MockOutputs`, `MockDeviceActions`
- Track call counts: `callCount`, `methodCalls`
- Store last values: `lastValue`, `lastState`
- Provide `reset()` method

### Test Fixtures

- Suffix with `Test`: `ControllerTest`, `UiTest`
- Inherit from `::testing::Test`
- Use `SetUp()` for initialization
- Provide helper methods for common operations

## Best Practices

### 1. **Fast Execution**

- Use small timeout values (100ms instead of 1000ms)
- Avoid delays - advance time explicitly
- Keep test data minimal

### 2. **Clear Arrange-Act-Assert**

```cpp
TEST_F(ModuleTest, Feature_Action_Result) {
    // Arrange - Set up preconditions
    module.setValue(initial);

    // Act - Perform the action
    module.doThing();

    // Assert - Verify results
    EXPECT_EQ(module.getValue(), expected);
}
```

### 3. **Test One Thing**

Each test should verify one behavior:

```cpp
// Good
TEST_F(UiTest, Idle_UpButton_IncreasesTarget) {
    float initial = ui.targetPsi();
    ui.onButton(makeEvent(Button::Up, Action::Click), device);
    EXPECT_FLOAT_EQ(ui.targetPsi(), initial + cfg.stepSmall);
}

// Avoid testing multiple unrelated things in one test
```

### 4. **Use Fixtures for Common Setup**

```cpp
class ControllerTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Common initialization
        controller.begin(&outputs, cfg);
        outputs.reset();
    }
};
```

### 5. **Mock External Dependencies**

Never depend on:

- Real hardware (GPIO, timers, sensors)
- Network/file system
- Random values
- System time (inject it)

Instead, create mock implementations:

```cpp
class MockOutputs : public IOutputs {
    // Track calls instead of actually doing I/O
};
```

### 6. **Test Edge Cases**

- Boundary values (min, max, zero)
- Invalid inputs (negative, overflow)
- State transitions (null → initialized)
- Timing edge cases (exactly at timeout)

### 7. **Organize with Headers**

```cpp
// ============================================================================
// Category Name
// ============================================================================
```

Makes it easy to navigate and understand test structure.

## Example Test Counts

Well-structured test files should have:

- **10-20 tests**: Small focused module
- **30-50 tests**: Medium complexity (state machine)
- **60-80 tests**: Complex with many paths (controller, UI)

Our test files:

- `test_protocol.cpp`: 45 tests (protocol conversion + parsing)
- `test_errors.cpp`: 12 tests (simple catalog)
- `test_battery_simple.cpp`: 8 tests (calculation logic)
- `test_controller.cpp`: 68 tests (complex state machine)
- `test_ui.cpp`: 68 tests (complex UI state machine)

## Integration with CI/CD

Tests must:

1. Include own `main()` function
2. Return 0 on success, non-zero on failure
3. Compile in `native` platform
4. Avoid Arduino-specific dependencies
5. Be deterministic (no randomness)

See `.github/workflows/test.yml` for CI configuration.
