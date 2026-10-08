# TrailAir C++ Coding Standards

## Purpose

This document defines naming conventions and code formatting standards for the TrailAir project. These standards aim to improve code readability and maintainability by applying C# .NET-inspired conventions adapted for embedded C++.

---

## 1. Naming Conventions

### 1.1 General Principles

- **Be Descriptive**: Names should clearly indicate purpose and intent
- **Avoid Abbreviations**: Use full words unless the abbreviation is universally understood (e.g., PSI, UI, ID)
- **Be Consistent**: Follow the same pattern throughout the codebase
- **Avoid Single Letters**: Exception: loop counters (`i`, `j`, `k`) and coordinates (`x`, `y`, `w`, `h`)

### 1.2 Classes and Structs

- **Format**: `PascalCase`
- **Rule**: Noun or noun phrase describing what the class represents
- **Examples**:

  ```cpp
  // ❌ Bad
  class TA_Display
  class UI
  class Ctrl

  // ✅ Good
  class DisplayController
  class UserInterface
  class PressureController
  ```

### 1.3 Enums and Enum Values

- **Enum Type**: `PascalCase`
- **Enum Values**: `PascalCase`
- **Rule**: Use singular nouns for enum types, descriptive values
- **Examples**:

  ```cpp
  // ❌ Bad
  enum class Link { Disconnected, Connected };
  enum class Ctrl { Idle, AirUp, Venting, Checking, Error };

  // ✅ Good
  enum class ConnectionStatus { Disconnected, Connected };
  enum class ControllerState { Idle, Inflating, Deflating, Checking, Error };
  ```

### 1.4 Functions and Methods

- **Format**: `camelCase` for private/protected, `PascalCase` for public (embedded variant)
- **Alternative**: All `camelCase` (C++ standard)
- **Rule**: Start with verb describing the action
- **Examples**:

  ```cpp
  // ❌ Bad
  void render(const DisplayModel& m);
  void drawTwoLineCentered_(...);
  int topSafe_();
  void measure_(...);

  // ✅ Good
  void renderDisplay(const DisplayModel& model);
  void drawTwoLinesCentered(...);
  int getTopSafeArea();
  void measureText(...);
  ```

### 1.5 Variables

#### Member Variables (Private/Protected)

- **Format**: Private: `camelCase` with `_` prefix.
- **Be consistent**
- **Examples**:

  ```cpp
  // ❌ Bad
  Adafruit_SSD1306& d_;
  Style style_{};
  int batteryPercent = 0;

  // ✅ Good
  Adafruit_SSD1306& _display;
  Style _displayStyle;
  int _batteryPercent = 0;
  ```

#### Local Variables and Parameters

- **Format**: `camelCase`
- **Rule**: Descriptive names, avoid single letters except for well-known contexts
- **Examples**:

  ```cpp
  // ❌ Bad
  void render(const DisplayModel& m);
  int w, h;
  uint8_t size;

  // ✅ Good
  void render(const DisplayModel& model);
  int textWidth, textHeight;
  uint8_t fontSize;
  ```

#### Constants

- **Format**: `SCREAMING_SNAKE_CASE` for compile-time constants and const variables (Google C++ style alternative)
- **Examples**:

  ```cpp
  // ❌ Bad
  const int iconSize = 6;
  const int cellW = d_.width() / 4;

  // ✅ Good
  const int ICON_SIZE = 6;
  const int CELL_WIDTH = displayWidth / 4;
  ```

### 1.6 Namespaces

- **Format**: `lowercase`
- **Rule**: Short, descriptive, usually the library/module name
- **Examples**:

  ```cpp
  // ❌ Bad
  namespace ta { namespace display { ... } }

  // ✅ Better
  namespace trailair { namespace display { ... } }
  ```

### 1.7 File Names

- **Format**: `PascalCase.cpp` / `PascalCase.h` for classes
- **Format**: Match the primary class name in the file
- **Examples**:

  ```cpp
  // ❌ Bad
  TA_Display.h / TA_Display.cpp
  TA_Errors.h

  // ✅ Good
  DisplayController.h / DisplayController.cpp
  ErrorCodes.h
  ```

---

## 2. Code Structure

### 2.1 Function Length

- **Guideline**: Keep functions under 50 lines
- **Rule**: If longer, extract helper methods

### 2.2 Parameter Count

- **Guideline**: Maximum 4-5 parameters
- **Rule**: If more, consider a struct/class to group related parameters

### 2.3 Comments

- **Required**: Public API documentation
- **Format**: Use Doxygen-style comments for public interfaces
- **Examples**:

  ```cpp
  /**
   * @brief Renders the display with the current model data
   * @param model The display data model containing all view state
   */
  void renderDisplay(const DisplayModel& model);

  /// @brief Gets the safe drawing area below the status row
  /// @return Y-coordinate of the top safe area
  int getTopSafeArea() const;
  ```

### 2.4 Magic Numbers

- **Rule**: Replace with named constants
- **Examples**:

  ```cpp
  // ❌ Bad
  if (percent < 15) { ... }
  d_.drawRect(batteryX + batteryW, batteryY + 2, 1, 2, SSD1306_WHITE);

  // ✅ Good
  const int LOW_BATTERY_THRESHOLD = 15;
  const int BATTERY_TIP_OFFSET = 2;
  const int BATTERY_TIP_WIDTH = 1;
  const int BATTERY_TIP_HEIGHT = 2;

  if (percent < LOW_BATTERY_THRESHOLD) { ... }
  d_.drawRect(batteryX + batteryW, batteryY + BATTERY_TIP_OFFSET,
              BATTERY_TIP_WIDTH, BATTERY_TIP_HEIGHT, SSD1306_WHITE);
  ```

---

## 3. Formatting Rules

### 3.1 Indentation

- **Standard**: 2 spaces (no tabs)

### 3.2 Braces

- **Style**: K&R style (opening brace on same line)
- **Rule**: Be consistent throughout each file

### 3.3 Line Length

- **Guideline**: Soft-maximum 100-120 characters
- **Rule**: Break long lines logically, go over limit if only a little and makes sense

### 3.4 Spacing

- **After keywords**: `if (condition)` not `if(condition)`
- **Around operators**: `a + b` not `a+b`
- **After commas**: `func(a, b, c)` not `func(a,b,c)`

---

## 4. Library-Specific Patterns

### 4.1 Display-Related

```cpp
// ❌ Current naming
void drawTwoLineCentered_(const String& top, uint8_t topSize, ...);
int centerX_(int w);
void measure_(const String& s, uint8_t size, int16_t& w, int16_t& h);

// ✅ Improved naming
void drawTwoLinesCentered(const String& topLine, uint8_t topFontSize, ...);
int calculateCenterX(int width);
void measureTextDimensions(const String& text, uint8_t fontSize,
                           int16_t& width, int16_t& height);
```

### 4.2 State Management

```cpp
// ❌ Current naming
struct DisplayModel {
    int batteryPercent = 0;
    Link link = Link::Disconnected;
    Ctrl ctrl = Ctrl::Idle;
    ...
}

// ✅ Improved naming
struct DisplayState {
    int batteryPercentage = 0;
    ConnectionStatus connectionStatus = ConnectionStatus::Disconnected;
    ControllerState controllerState = ControllerState::Idle;
    ...
}
```

---

## 5. Migration Strategy

### 5.1 Phase 1: Document Current Issues

- Create a list of poorly-named items per library
- Prioritize based on usage frequency and impact

### 5.2 Phase 2: Library-by-Library Refactoring

**Order**: Start with least-used libraries first

**Shared Libraries (pioLib/):**

1. ✅ **TA_Config** - COMPLETE
2. ✅ **TA_Errors** - COMPLETE
3. ✅ **TA_Input** - COMPLETE
4. ✅ **TA_UI** - COMPLETE
5. ✅ **TA_Controller** - COMPLETE
6. ✅ **TA_Protocol** - COMPLETE
7. ✅ **TA_Time** - COMPLETE
8. ✅ **TA_Display** - COMPLETE

**Remote Libraries (pio/remote/lib/):** 9. ✅ **TA_Battery** → `trailair::battery::BatteryMonitor` - COMPLETE 10. ✅ **TA_Comms** → `trailair::comms::*` - COMPLETE 11. ✅ **TA_State** → `trailair::state::*` - COMPLETE 12. ✅ **TA_RemoteApp** → `trailair::app::*` - COMPLETE

**Control Board Libraries (pio/control_board/lib/):** 13. ✅ **TA_Sensors** → `trailair::sensors::*` - COMPLETE 14. ✅ **TA_App** → `trailair::app::*` - COMPLETE 15. ✅ **TA_CommsBoard** → `trailair::comms::*` - COMPLETE 16. ✅ **TA_StateBoard** → `trailair::stateboard::*` - COMPLETE 17. ⏭️ **TA_Actuators** → Keeping `ta::act::*` (board-specific hardware)

**🎉 ALL APPLICATION LIBRARIES REFACTORED! 🎉**

### 5.3 Phase 3: Test After Each Library

- Run all unit tests
- Verify builds for both control_board and remote
- Update documentation

---

## 6. Common Abbreviations to Expand

| ❌ Current | ✅ Improved                   | Context                                  |
| ---------- | ----------------------------- | ---------------------------------------- |
| `d_`       | `_display`                    | Display object reference                 |
| `m`        | `model`                       | DisplayModel parameter                   |
| `w`, `h`   | `width`, `height`             | Dimensions (exception: very local scope) |
| `s`        | `text` or `string`            | String parameters                        |
| `Ctrl`     | `ControllerState`             | Enum type                                |
| `Link`     | `ConnectionStatus`            | Enum type                                |
| `PSI`      | Keep (universally understood) | Pressure unit                            |
| `UI`       | Keep (universally understood) | User Interface                           |
| `btn`      | `button`                      | Button-related                           |
| `conn`     | `connection`                  | Connection-related                       |
| `batt`     | `battery`                     | Battery-related                          |
| `msg`      | `message`                     | Message strings                          |

---

## 7. Documentation Requirements

### 7.1 Every Public Method Needs

- Brief description of what it does
- Parameter descriptions
- Return value description (if applicable)
- Example usage (for complex methods)

### 7.2 Every Class Needs

- Purpose statement
- Usage example
- Dependencies noted

### 7.3 Every Enum Needs

- Purpose statement
- Value meanings documented

---

## 8. Tools and Enforcement

### 8.1 Recommended Tools

- **clang-format**: Auto-formatting (configuration TBD)
- **cpplint**: Style checking
- **Doxygen**: Documentation generation

### 8.2 Pre-Commit Checklist

- [ ] All names follow conventions
- [ ] No magic numbers
- [ ] Comments updated
- [ ] Tests pass
- [ ] No compiler warnings

---

## Notes

This is a living document. Update as we discover patterns and edge cases during development.

---

## 9. Refactoring Status

**Status**: ✅ **COMPLETE** - All application libraries refactored (December 10, 2025)

### Completed Libraries (16/16)

**Shared Libraries (pioLib/) - 8:**

- TA_Config → `trailair::config`
- TA_Errors → `trailair::errors`
- TA_Input → `trailair::input`
- TA_UI → `trailair::ui`
- TA_Controller → `trailair::controller`
- TA_Protocol → `trailair::protocol`
- TA_Time → `trailair::time`
- TA_Display → `trailair::display`

**Remote Libraries (pio/remote/lib/) - 4:**

- TA_Battery → `trailair::battery`
- TA_Comms → `trailair::comms`
- TA_State → `trailair::state`
- TA_RemoteApp → `trailair::app`

**Control Board Libraries (pio/control_board/lib/) - 4:**

- TA_Sensors → `trailair::sensors`
- TA_App → `trailair::app`
- TA_CommsBoard → `trailair::comms`
- TA_StateBoard → `trailair::stateboard`

**Intentionally Preserved:**

- TA_Actuators → `ta::act` (board-specific hardware abstraction)
- SmartButton → Third-party library (unmodified)

### Key Achievements

✅ All namespaces migrated from `ta::*` to `trailair::*`  
✅ All classes renamed to descriptive PascalCase  
✅ All enum values converted to PascalCase  
✅ All member variables use `_` prefix convention  
✅ All functions use camelCase with descriptive verbs  
✅ Comprehensive Doxygen documentation added  
✅ Both projects (remote + control_board) building successfully  
✅ All cross-references updated across 16 libraries
