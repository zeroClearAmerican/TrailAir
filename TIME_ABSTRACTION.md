# Time Abstraction & Mocking

## Overview

The `TA_Time` library provides an abstraction layer for `millis()` to enable deterministic testing without Arduino dependencies.

## Production Use

In production code, replace `millis()` with `ta::time::getMillis()`:

```cpp
#include "TA_Time.h"

void myFunction() {
    uint32_t now = ta::time::getMillis();  // Instead of millis()

    // Use with overflow-safe helpers
    if (ta::time::hasElapsed(now, startTime, 5000)) {
        // 5 seconds have passed
    }
}
```

## Test Mode

Enable test mode by defining `TA_TIME_TEST_MODE` before including `TA_Time.h`:

```cpp
#define TA_TIME_TEST_MODE
#include "TA_Time.h"
#include "TA_Time_test.h"

TEST(MyTest, TimeoutBehavior) {
    ta::time::test::MockTime mockTime;

    // Set initial time
    mockTime.set(1000);
    EXPECT_EQ(ta::time::getMillis(), 1000);

    // Simulate time passing
    mockTime.advance(5000);
    EXPECT_EQ(ta::time::getMillis(), 6000);

    // Test overflow scenarios
    mockTime.set(0xFFFFFFF0);  // Near max
    mockTime.advance(0x20);     // Cross overflow boundary
    EXPECT_EQ(ta::time::getMillis(), 0x00000010);
}
```

## Benefits

1. **No Arduino dependency in tests** - Run tests on any platform
2. **Deterministic** - Control exact time values
3. **Fast** - No actual delays needed
4. **Overflow testing** - Easily test 49-day overflow scenarios
5. **Zero production overhead** - Inlined to direct `millis()` call

## Implementation

- **Production**: `getMillis()` inlines to `millis()` (zero overhead)
- **Test**: `getMillis()` calls mockable function pointer
- **Conditional compilation**: Uses `#ifdef TA_TIME_TEST_MODE`

## Migration

Replace these patterns:

| Old Pattern                       | New Pattern                                                        |
| --------------------------------- | ------------------------------------------------------------------ |
| `millis()`                        | `ta::time::getMillis()`                                            |
| `uint32_t now = millis();`        | `uint32_t now = ta::time::getMillis();`                            |
| `if (millis() - start > timeout)` | `if (ta::time::hasElapsed(ta::time::getMillis(), start, timeout))` |

## Files Updated

Production code using `getMillis()`:

- `pio/remote/lib/TA_Comms/src/TA_Comms.cpp`
- `pio/remote/lib/TA_RemoteApp/src/TA_RemoteApp.cpp`
- More to be migrated...

## Future Work

- Migrate remaining `millis()` calls across codebase
- Add `MockTime` to existing test suites
- Consider adding `delay()` abstraction for sleep testing
