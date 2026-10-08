#pragma once
#include <stdint.h>
#include <TA_Types.h>

namespace smartbutton { class SmartButton; }

namespace trailair {
namespace input {

using ButtonEventCallback = void(*)(void* context, const ButtonEvent& event);

/// GPIO pins, in ButtonId order
struct ButtonPins {
  uint8_t left;
  uint8_t down;
  uint8_t up;
  uint8_t right;
};

/**
 * @brief Four debounced buttons publishing Pressed/Released/Click/LongHold events.
 *
 * Wraps SmartButton. HOLD and the *_REPEAT events are dropped: consumers only need discrete
 * events, and LongHold fires once per hold.
 */
class ButtonManager {
public:
  explicit ButtonManager(const ButtonPins& pins) : _pins(pins) {}

  /// Configure GPIO and start the buttons. Call once in setup.
  void begin();

  /// Register for events (up to MAX_SUBSCRIBERS)
  void subscribe(ButtonEventCallback callback, void* context);

  /// Process button state. Call every loop.
  void service();

  /// True while the button is physically held (debounced)
  bool isHeld(ButtonId id) const;

private:
  struct ButtonContext {
    ButtonManager* self;
    ButtonId id;
  };

  void publish_(ButtonId id, ButtonAction action, int clickCount);

  ButtonPins _pins;

  static constexpr int MAX_SUBSCRIBERS = 4;
  struct Subscriber {
    ButtonEventCallback callback;
    void* context;
  };
  Subscriber _subscribers[MAX_SUBSCRIBERS]{};
  int _subscriberCount = 0;

  // Allocated in begin() to keep SmartButton out of this header
  smartbutton::SmartButton* _buttons[BUTTON_COUNT] = {};
  ButtonContext _contexts[BUTTON_COUNT];
};

}  // namespace input
}  // namespace trailair
