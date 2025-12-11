# TrailAir ESP-IDF Migration Inventory

## Migration Status: **PLANNING PHASE**

This document inventories all Arduino framework dependencies in the TrailAir project and identifies ESP-IDF equivalents for migration away from the Arduino framework.

---

## Executive Summary

**Total Arduino Dependencies Identified**: 57 occurrences across 26 files

**Categories**:

1. Framework Configuration (2 files)
2. Core Arduino API Usage (15 files)
3. Arduino Libraries (3 libraries)
4. Arduino-specific constructs (`.ino` files, `String` class)

---

## 1. Framework Configuration

### 1.1 PlatformIO Configuration Files

| File                               | Current               | ESP-IDF Equivalent   | Notes                     |
| ---------------------------------- | --------------------- | -------------------- | ------------------------- |
| `pio/remote/platformio.ini`        | `framework = arduino` | `framework = espidf` | Change platform framework |
| `pio/control_board/platformio.ini` | `framework = arduino` | `framework = espidf` | Change platform framework |

**Migration Steps**:

- Change `framework = arduino` to `framework = espidf`
- Update board definitions if needed
- Add ESP-IDF specific build flags
- Configure `sdkconfig` files for each project

---

## 2. Arduino Core API Usage

### 2.1 Arduino.h Includes (15 files)

**Files with direct `#include <Arduino.h>`**:

- `pioLib/TA_Time/src/TA_Time.h` (conditional include)
- `pioLib/TA_Input/src/TA_Input.cpp`
- `pioLib/TA_Display/src/TA_DisplayIcons.h`
- `pioLib/TA_Display/src/TA_Display.h`
- `pioLib/TA_Controller/src/TA_Controller.cpp`
- `pioLib/SmartButton/src/SmartButtonDefs.h` (third-party)
- `pio/control_board/src/TrailAir-ControlBoard.ino`
- `pio/control_board/lib/TA_StateBoard/src/TA_StateBoard.h`
- `pio/control_board/lib/TA_Sensors/src/TA_Sensors.h`
- `pio/control_board/lib/TA_CommsBoard/src/TA_CommsBoard.h`
- `pio/control_board/lib/TA_CommsBoard/src/TA_CommsBoard.cpp`
- `pio/control_board/lib/TA_App/src/TA_App.cpp`
- `pio/control_board/lib/TA_Actuators/src/TA_Actuators.h`
- `pio/remote/lib/TA_State/src/TA_State.cpp`
- `pio/remote/lib/TA_RemoteApp/src/TA_RemoteApp.cpp`
- `pio/remote/lib/TA_Comms/src/TA_Comms.h`
- `pio/remote/lib/TA_Battery/src/TA_Battery.h`

**ESP-IDF Equivalents**:

```cpp
// Instead of: #include <Arduino.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "esp_log.h"
// Plus other specific headers as needed
```

---

## 3. Arduino API Functions

### 3.1 Timing Functions

| Arduino Function      | Usage Count                  | ESP-IDF Equivalent                    | Header            |
| --------------------- | ---------------------------- | ------------------------------------- | ----------------- |
| `millis()`            | Many (abstracted in TA_Time) | `esp_timer_get_time() / 1000`         | `esp_timer.h`     |
| `delay(ms)`           | 2 instances                  | `vTaskDelay(ms / portTICK_PERIOD_MS)` | `freertos/task.h` |
| `delayMicroseconds()` | Unknown                      | `ets_delay_us()`                      | `rom/ets_sys.h`   |

**Good News**: Your `TA_Time` abstraction layer already isolates `millis()` usage! Only need to update the implementation.

**Files to Update**:

- `pioLib/TA_Time/src/TA_Time.cpp` - Update `getMilliseconds()` implementation
- `pio/control_board/src/TrailAir-ControlBoard.ino` - Replace `delay(100)` call

### 3.2 Digital I/O Functions

| Arduino Function           | ESP-IDF Equivalent                       | Header          |
| -------------------------- | ---------------------------------------- | --------------- |
| `pinMode(pin, mode)`       | `gpio_config()` + `gpio_set_direction()` | `driver/gpio.h` |
| `digitalWrite(pin, value)` | `gpio_set_level(pin, value)`             | `driver/gpio.h` |
| `digitalRead(pin)`         | `gpio_get_level(pin)`                    | `driver/gpio.h` |

**Files Using Digital I/O**:

- `pio/control_board/src/TrailAir-ControlBoard.ino` - pinMode, digitalWrite
- `pio/control_board/lib/TA_Actuators/src/TA_Actuators.h` - Likely GPIO control
- `pioLib/TA_Input/src/TA_Input.cpp` - Button input (via SmartButton)

### 3.3 Analog I/O Functions

| Arduino Function              | ESP-IDF Equivalent             | Header                  |
| ----------------------------- | ------------------------------ | ----------------------- |
| `analogRead(pin)`             | `adc_oneshot_read()`           | `esp_adc/adc_oneshot.h` |
| `analogReadMilliVolts(pin)`   | `adc_cali_raw_to_voltage()`    | `esp_adc/adc_cali.h`    |
| `analogSetAttenuation(atten)` | `adc_oneshot_config_channel()` | `esp_adc/adc_oneshot.h` |

**Files Using ADC**:

- `pio/remote/lib/TA_Battery/src/TA_Battery.cpp` - Battery voltage monitoring

**Note**: The comment mentions "Arduino-ESP32 core" ADC attenuation - this is actually ESP-IDF functionality, so conversion should be straightforward.

### 3.4 Serial Communication

| Arduino Function             | ESP-IDF Equivalent               | Header          |
| ---------------------------- | -------------------------------- | --------------- |
| `Serial.begin()`             | `uart_driver_install()` + config | `driver/uart.h` |
| `Serial.print()`/`println()` | `printf()` or ESP_LOG macros     | `esp_log.h`     |

**Files Using Serial**:

- `pio/control_board/src/TrailAir-ControlBoard.ino` - Serial output
- `pio/remote/src/TrailAir-Remote.ino` - Serial output

**ESP-IDF Best Practice**: Use `ESP_LOGI()`, `ESP_LOGW()`, `ESP_LOGE()` instead of Serial.print

---

## 4. Arduino Libraries

### 4.1 Communication Libraries

| Arduino Library | Usage               | ESP-IDF Equivalent    | Notes                               |
| --------------- | ------------------- | --------------------- | ----------------------------------- |
| `WiFi.h`        | WiFi initialization | `esp_wifi.h`          | Already partially using ESP-IDF API |
| `Wire.h`        | I2C communication   | `driver/i2c.h`        | Used for display                    |
| `SPI.h`         | SPI communication   | `driver/spi_master.h` | Used in remote                      |
| `esp_now.h`     | ESP-NOW protocol    | `esp_now.h`           | **Already ESP-IDF native!**         |
| `esp_wifi.h`    | WiFi config         | `esp_wifi.h`          | **Already ESP-IDF native!**         |

**Files Using Communication**:

- `pio/control_board/src/TrailAir-ControlBoard.ino` - Wire (I2C)
- `pio/control_board/lib/TA_CommsBoard/src/TA_CommsBoard.h` - WiFi, esp_now, esp_wifi
- `pio/remote/src/TrailAir-Remote.ino` - Wire, SPI
- `pio/remote/lib/TA_Comms/src/TA_Comms.h` - ESP-NOW

**Good News**: You're already using native ESP-IDF APIs for WiFi and ESP-NOW!

### 4.2 Third-Party Arduino Libraries

| Library                | Version | Purpose             | ESP-IDF Equivalent/Strategy         |
| ---------------------- | ------- | ------------------- | ----------------------------------- |
| `Adafruit GFX Library` | 1.12.1  | Graphics primitives | Port to ESP-IDF or find alternative |
| `Adafruit SSD1306`     | 2.5.15  | OLED display driver | Port to ESP-IDF or find alternative |
| `SmartButton`          | Local   | Button debouncing   | Keep as-is (minimal Arduino deps)   |

**Critical Dependencies**: Display libraries

**Migration Options**:

1. **Option A**: Use ESP-IDF compatible SSD1306 library (e.g., `esp-idf-ssd1306`)
2. **Option B**: Port Adafruit libraries (minimal Arduino compatibility layer)
3. **Option C**: Write custom display driver using ESP-IDF I2C

**Recommendation**: Option A - Use existing ESP-IDF SSD1306 component

**Potential Libraries**:

- https://github.com/nopnop2002/esp-idf-ssd1306 (ESP-IDF native)
- https://github.com/yanbe/ssd1306-esp-idf-i2c (ESP-IDF I2C)

### 4.3 Preferences Library

| Arduino Library | ESP-IDF Equivalent | Header                  |
| --------------- | ------------------ | ----------------------- |
| `Preferences.h` | `nvs_flash.h`      | `nvs_flash.h` + `nvs.h` |

**Files Using Preferences**:

- `pio/control_board/lib/TA_CommsBoard/src/TA_CommsBoard.h`

**Good News**: This is just a wrapper around ESP-IDF's NVS!

---

## 5. Arduino-Specific Constructs

### 5.1 .ino Files

| File                        | Purpose            | Migration Strategy                 |
| --------------------------- | ------------------ | ---------------------------------- |
| `TrailAir-ControlBoard.ino` | Control board main | Rename to `.cpp`, add `app_main()` |
| `TrailAir-Remote.ino`       | Remote main        | Rename to `.cpp`, add `app_main()` |

**ESP-IDF Structure**:

```cpp
// Instead of setup() and loop():
extern "C" void app_main(void) {
    // Initialize NVS
    nvs_flash_init();

    // Your setup code here
    app.begin();

    // Main loop
    while(1) {
        app.loop();
        vTaskDelay(1 / portTICK_PERIOD_MS); // Yield to FreeRTOS
    }
}
```

### 5.2 Arduino String Class

**Usage Count**: ~6 instances in `TA_Display.cpp`

**ESP-IDF Equivalents**:

- `std::string` (C++ standard)
- `char[]` buffers with `snprintf()`
- `std::stringstream`

**Files Using Arduino String**:

- `pioLib/TA_Display/src/TA_Display.cpp`

**Migration**: Replace `String` with `std::string` or C-style formatting

---

## 6. Hardware Abstraction Dependencies

### 6.1 GPIO Pin Definitions

**Current**: Arduino pin numbers
**ESP-IDF**: GPIO numbers (usually the same on ESP32-C3)

**Files**:

- `TrailAir-ControlBoard.ino` - Pins 9, 10 (actuators)
- `TrailAir-Remote.ino` - Pins 10, 9, 8, 20, 2 (buttons)

**Action**: Verify pin numbers match between Arduino and ESP-IDF for XIAO ESP32-C3

### 6.2 ADC Configuration

**Current**: `analogReadMilliVolts()` with attenuation
**Files**: `TA_Battery.cpp`

**ESP-IDF Migration**:

```cpp
#include "esp_adc/adc_oneshot.h"
#include "esp_adc/adc_cali.h"

adc_oneshot_unit_handle_t adc1_handle;
adc_cali_handle_t adc1_cali_handle;

// Configuration
adc_oneshot_unit_init_cfg_t init_config = {
    .unit_id = ADC_UNIT_1,
};
adc_oneshot_new_unit(&init_config, &adc1_handle);

adc_oneshot_chan_cfg_t config = {
    .atten = ADC_ATTEN_DB_11,
    .bitwidth = ADC_BITWIDTH_DEFAULT,
};
adc_oneshot_config_channel(adc1_handle, ADC_CHANNEL_0, &config);

// Reading
int raw;
adc_oneshot_read(adc1_handle, ADC_CHANNEL_0, &raw);
```

---

## 7. Build System Changes

### 7.1 PlatformIO Configuration

**Current**:

```ini
framework = arduino
lib_deps =
    adafruit/Adafruit GFX Library@^1.12.1
    adafruit/Adafruit SSD1306@^2.5.15
```

**ESP-IDF**:

```ini
framework = espidf
lib_deps =
    # Will use ESP-IDF components instead
```

### 7.2 Component Configuration

**New Files Needed**:

- `pio/remote/sdkconfig` - ESP-IDF configuration
- `pio/control_board/sdkconfig` - ESP-IDF configuration
- `CMakeLists.txt` files for each component (if using IDF component system)

### 7.3 Library Organization

**Current**: PlatformIO libraries in `pioLib/`
**ESP-IDF**: Can continue using PlatformIO, or migrate to ESP-IDF components

**Recommendation**: Keep PlatformIO structure, use ESP-IDF framework

---

## 8. Testing Considerations

### 8.1 Native Tests

**Status**: ✅ **Already platform-independent!**

Your test suite already:

- Doesn't depend on Arduino.h (uses mocks)
- Uses Google Test
- Runs on native platform

**No changes needed for testing!**

### 8.2 Test Mocks

**Files**:

- `pio/remote/test/test_display/Arduino_mock.h`
- Test implementations with ESP-NOW mocks

**Action**: Update mocks if display library API changes

---

## 9. Migration Priority & Dependencies

### Phase 1: Core Framework (No external dependencies)

1. ✅ Update `platformio.ini` files
2. ✅ Replace `Arduino.h` with ESP-IDF headers
3. ✅ Update timing functions (`millis()`, `delay()`)
4. ✅ Update GPIO functions (pinMode, digitalWrite, digitalRead)
5. ✅ Update Serial to ESP_LOG
6. ✅ Rename `.ino` to `.cpp` and add `app_main()`

### Phase 2: Communication (Minimal changes - already using ESP-IDF)

1. ✅ Update WiFi initialization (if needed)
2. ✅ Update Preferences to native NVS
3. ✅ Verify ESP-NOW still works

### Phase 3: Peripherals (Some complexity)

1. ⚠️ Replace Wire.h with ESP-IDF I2C driver
2. ⚠️ Replace SPI.h with ESP-IDF SPI driver
3. ⚠️ Update ADC code in TA_Battery

### Phase 4: Display Libraries (Highest complexity)

1. ❌ Find/port SSD1306 driver to ESP-IDF
2. ❌ Update TA_Display to use new driver API
3. ❌ Replace Arduino String class
4. ❌ Test display functionality

### Phase 5: Polish

1. ✅ Update all documentation
2. ✅ Run full test suite
3. ✅ Flash and test on hardware

---

## 10. Detailed File-by-File Checklist

### Shared Libraries (pioLib/)

| Library       | Files to Update             | Changes Needed                           | Complexity |
| ------------- | --------------------------- | ---------------------------------------- | ---------- |
| TA_Time       | TA_Time.cpp                 | Replace millis() with esp_timer          | Low        |
| TA_Input      | TA_Input.cpp                | Update GPIO via SmartButton              | Low        |
| TA_Display    | TA_Display.h, .cpp, Icons.h | Replace Adafruit libs, Arduino.h, String | **High**   |
| TA_Controller | TA_Controller.cpp           | Remove Arduino.h, verify TA_Actuators    | Low        |
| SmartButton   | SmartButtonDefs.h           | May need Arduino compat layer            | Medium     |
| Others        | Various                     | Remove Arduino.h includes                | Low        |

### Remote Libraries

| Library      | Files to Update    | Changes Needed                | Complexity |
| ------------ | ------------------ | ----------------------------- | ---------- |
| TA_Battery   | TA_Battery.h, .cpp | Update ADC functions          | Medium     |
| TA_Comms     | TA_Comms.h         | Verify ESP-NOW (should be OK) | Low        |
| TA_State     | TA_State.cpp       | Remove Arduino.h              | Low        |
| TA_RemoteApp | TA_RemoteApp.cpp   | Remove Arduino.h              | Low        |

### Control Board Libraries

| Library       | Files to Update       | Changes Needed             | Complexity |
| ------------- | --------------------- | -------------------------- | ---------- |
| TA_CommsBoard | TA_CommsBoard.h, .cpp | Update Preferences to NVS  | Medium     |
| TA_StateBoard | TA_StateBoard.h       | Remove Arduino.h           | Low        |
| TA_Sensors    | TA_Sensors.h          | Update sensor reading APIs | Medium     |
| TA_Actuators  | TA_Actuators.h        | Update GPIO functions      | Low        |
| TA_App        | TA_App.h, .cpp        | Remove Arduino.h           | Low        |

### Main Applications

| File                      | Changes Needed                               | Complexity |
| ------------------------- | -------------------------------------------- | ---------- |
| TrailAir-ControlBoard.ino | Rename to .cpp, app_main(), GPIO, Serial     | Medium     |
| TrailAir-Remote.ino       | Rename to .cpp, app_main(), SPI, I2C, Serial | Medium     |

---

## 11. Risk Assessment

### Low Risk (Easy Migration)

- Timing functions (abstracted)
- Digital GPIO
- Serial output (use ESP_LOG)
- ESP-NOW (already native)
- WiFi (already native)
- String replacements

### Medium Risk (Some Work Required)

- ADC functions
- NVS/Preferences
- I2C/SPI initialization
- SmartButton library compatibility
- File structure changes (.ino → .cpp)

### High Risk (Significant Work)

- **Display libraries** - Largest dependency
- **Adafruit GFX/SSD1306** - Need ESP-IDF alternatives
- **Testing on hardware** - Ensure everything works

---

## 12. Recommended Approach

### Strategy: **Incremental Migration**

1. **Create ESP-IDF branch** - Don't break existing code
2. **Start with one project** - Migrate Remote first (simpler)
3. **Keep tests passing** - Unit tests should work throughout
4. **Test frequently on hardware** - Don't accumulate untested changes
5. **Document issues** - Create migration log for lessons learned

### Timeline Estimate

- **Phase 1-2** (Core + Comms): 1-2 days
- **Phase 3** (Peripherals): 1-2 days
- **Phase 4** (Display): 3-5 days (most complex)
- **Phase 5** (Testing/Polish): 2-3 days

**Total**: 7-12 days (assuming no major issues)

---

## 13. ESP-IDF Resources

### Documentation

- ESP-IDF Programming Guide: https://docs.espressif.com/projects/esp-idf/
- API Reference: https://docs.espressif.com/projects/esp-idf/en/latest/esp32c3/api-reference/
- Migration Guide: https://docs.espressif.com/projects/esp-idf/en/latest/esp32c3/api-guides/porting-from-arduino.html

### Example Projects

- ESP-IDF Examples: https://github.com/espressif/esp-idf/tree/master/examples
- ESP32-C3 specific: https://github.com/espressif/esp-idf/tree/master/examples/get-started

### Community Libraries

- Awesome ESP-IDF: https://github.com/igrr/awesome-esp-idf
- SSD1306 drivers: Search GitHub for "esp-idf ssd1306"

---

## 14. Next Steps

1. **Review this inventory** - Verify completeness
2. **Choose display library** - Critical path item
3. **Set up ESP-IDF environment** - Install toolchain
4. **Create migration branch** - Keep main stable
5. **Start Phase 1** - Framework basics
6. **Test continuously** - Both unit tests and hardware

---

## Notes

- ✅ Good: Already using ESP-IDF native APIs (ESP-NOW, WiFi)
- ✅ Good: Time abstraction makes millis() migration easy
- ✅ Good: Test suite is platform-independent
- ⚠️ Challenge: Display library dependencies
- ⚠️ Challenge: SmartButton third-party library
- ℹ️ Benefit: ESP-IDF is more performant and has better docs
- ℹ️ Benefit: Direct access to FreeRTOS features
- ℹ️ Benefit: Better control over hardware and configuration

---

**Last Updated**: December 10, 2025
**Status**: Planning/Inventory Phase
**Next Review**: After choosing display library strategy
