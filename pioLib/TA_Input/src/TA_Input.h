#pragma once
#include <stdint.h>

namespace smartbutton { class SmartButton; }

namespace trailair {
namespace input {

/**
 * @brief Identifies which physical button triggered an event
 */
enum class ButtonId {
  Left,
  Down,
  Up,
  Right
};

/**
 * @brief Type of button action that occurred
 */
enum class ButtonAction {
  Pressed,   /// Button was pressed down
  Released,  /// Button was released
  Click,     /// Short press and release (tap)
  LongHold   /// Button held down for extended period
};

/**
 * @brief Button event data
 * 
 * Contains information about a button interaction including
 * which button, what action, and click count for multi-click detection.
 */
struct ButtonEvent {
  ButtonId id;
  ButtonAction action;
  int clickCount;  /// Number of clicks detected (valid for Click action)
};

/**
 * @brief Callback function signature for button events
 * 
 * @param context User-provided context pointer
 * @param event The button event that occurred
 */
using ButtonEventCallback = void(*)(void* context, const ButtonEvent& event);

/**
 * @brief GPIO pin assignments for four buttons
 */
struct ButtonPins {
  uint8_t left;
  uint8_t down;
  uint8_t up;
  uint8_t right;
};

/**
 * @brief Manages four physical buttons with debouncing and event publishing
 * 
 * Handles button input from four GPIO pins, provides debouncing via SmartButton,
 * and publishes events to multiple subscribers. Supports press, release, click,
 * and long-hold detection.
 */
class ButtonManager {
public:
  explicit ButtonManager(const ButtonPins& pins) : _pins(pins) {}

  /**
   * @brief Initialize button hardware and configure GPIO pins
   * 
   * Must be called once during setup before using buttons.
   */
  void begin();
  
  /**
   * @brief Subscribe to button events
   * 
   * @param callback Function to call when button events occur
   * @param context User pointer passed to callback
   */
  void subscribe(ButtonEventCallback callback, void* context);
  
  /**
   * @brief Unsubscribe from button events
   * 
   * @param callback Previously registered callback function
   * @param context Previously registered context pointer
   */
  void unsubscribe(ButtonEventCallback callback, void* context);
  
  /**
   * @brief Remove all event subscribers
   */
  void clearSubscribers();

  /**
   * @brief Process button state - must be called regularly from main loop
   */
  void service();

private:
  /// Context passed to SmartButton callbacks
  struct ButtonContext {
    ButtonManager* self;
    ButtonId id;
  };
  
  /// Internal handler for raw SmartButton events
  void onRawEvent(ButtonId id, ButtonAction action, int clickCount);

private:
  ButtonPins _pins;

  // Subscriber management
  static constexpr int MAX_SUBSCRIBERS = 4;
  struct Subscriber {
    ButtonEventCallback callback;
    void* context;
  };
  Subscriber _subscribers[MAX_SUBSCRIBERS]{};
  int _subscriberCount = 0;

  // SmartButton instances (allocated in begin() to avoid header dependency)
  smartbutton::SmartButton* _leftButton  = nullptr;
  smartbutton::SmartButton* _downButton  = nullptr;
  smartbutton::SmartButton* _upButton    = nullptr;
  smartbutton::SmartButton* _rightButton = nullptr;
  
  ButtonContext _leftContext{this, ButtonId::Left};
  ButtonContext _downContext{this, ButtonId::Down};
  ButtonContext _upContext{this, ButtonId::Up};
  ButtonContext _rightContext{this, ButtonId::Right};
};

}  // namespace input
}  // namespace trailair