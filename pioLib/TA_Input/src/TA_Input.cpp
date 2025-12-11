#include <Arduino.h>
#include "TA_Input.h"
#include <SmartButton.h>
using namespace smartbutton;

namespace trailair {
namespace input {

namespace {
  /// Maps SmartButton events to our ButtonAction enum
  static inline ButtonAction mapEvent(SmartButton::Event event) {
    switch (event) {
      case SmartButton::Event::PRESSED:    return ButtonAction::Pressed;
      case SmartButton::Event::RELEASED:   return ButtonAction::Released;
      case SmartButton::Event::CLICK:      return ButtonAction::Click;
      case SmartButton::Event::LONG_HOLD:  return ButtonAction::LongHold;
      default:                             return ButtonAction::Click;
    }
  }
}

void ButtonManager::begin() {
  // Configure GPIO pins with pull-up resistors
  pinMode(_pins.left,  INPUT_PULLUP);
  pinMode(_pins.down,  INPUT_PULLUP);
  pinMode(_pins.up,    INPUT_PULLUP);
  pinMode(_pins.right, INPUT_PULLUP);

  // Allocate SmartButton instances and bind callbacks
  _leftButton  = new SmartButton(_pins.left);
  _downButton  = new SmartButton(_pins.down);
  _upButton    = new SmartButton(_pins.up);
  _rightButton = new SmartButton(_pins.right);

  // Setup callbacks using ButtonContext to identify which button fired
  _leftButton->begin([](SmartButton* button, SmartButton::Event event, int clickCount) {
    auto* context = static_cast<ButtonContext*>(button->getContext());
    context->self->onRawEvent(context->id, mapEvent(event), clickCount);
  }, &_leftContext);
  
  _downButton->begin([](SmartButton* button, SmartButton::Event event, int clickCount) {
    auto* context = static_cast<ButtonContext*>(button->getContext());
    context->self->onRawEvent(context->id, mapEvent(event), clickCount);
  }, &_downContext);
  
  _upButton->begin([](SmartButton* button, SmartButton::Event event, int clickCount) {
    auto* context = static_cast<ButtonContext*>(button->getContext());
    context->self->onRawEvent(context->id, mapEvent(event), clickCount);
  }, &_upContext);
  
  _rightButton->begin([](SmartButton* button, SmartButton::Event event, int clickCount) {
    auto* context = static_cast<ButtonContext*>(button->getContext());
    context->self->onRawEvent(context->id, mapEvent(event), clickCount);
  }, &_rightContext);
}

void ButtonManager::subscribe(ButtonEventCallback callback, void* context) {
  if (!callback || _subscriberCount >= MAX_SUBSCRIBERS) return;
  _subscribers[_subscriberCount++] = { callback, context };
}

void ButtonManager::unsubscribe(ButtonEventCallback callback, void* context) {
  for (int i = 0; i < _subscriberCount; ++i) {
    if (_subscribers[i].callback == callback && _subscribers[i].context == context) {
      // Compact array by shifting elements
      for (int j = i + 1; j < _subscriberCount; ++j) {
        _subscribers[j - 1] = _subscribers[j];
      }
      --_subscriberCount;
      break;
    }
  }
}

void ButtonManager::clearSubscribers() {
  _subscriberCount = 0;
  for (int i = 0; i < MAX_SUBSCRIBERS; ++i) {
    _subscribers[i] = { nullptr, nullptr };
  }
}

void ButtonManager::service() {
  SmartButton::service();
}

void ButtonManager::onRawEvent(ButtonId id, ButtonAction action, int clickCount) {
  if (_subscriberCount == 0) return;
  
  ButtonEvent event{ id, action, clickCount };
  for (int i = 0; i < _subscriberCount; ++i) {
    if (_subscribers[i].callback) {
      _subscribers[i].callback(_subscribers[i].context, event);
    }
  }
}

}  // namespace input
}  // namespace trailair