#include <Arduino.h>
#include "TA_Input.h"
#include <SmartButton.h>
using namespace smartbutton;

namespace trailair {
namespace input {

void ButtonManager::begin() {
  const uint8_t pins[BUTTON_COUNT] = { _pins.left, _pins.down, _pins.up, _pins.right };

  for (uint8_t i = 0; i < BUTTON_COUNT; ++i) {
    pinMode(pins[i], INPUT_PULLUP);
    _contexts[i] = { this, static_cast<ButtonId>(i) };
    _buttons[i] = new SmartButton(pins[i]);
    _buttons[i]->begin([](SmartButton* button, SmartButton::Event event, int clickCount) {
      ButtonAction action;
      switch (event) {
        case SmartButton::Event::PRESSED:   action = ButtonAction::Pressed;  break;
        case SmartButton::Event::RELEASED:  action = ButtonAction::Released; break;
        case SmartButton::Event::CLICK:     action = ButtonAction::Click;    break;
        case SmartButton::Event::LONG_HOLD: action = ButtonAction::LongHold; break;
        default: return;  // HOLD, HOLD_REPEAT, LONG_HOLD_REPEAT: not needed
      }
      auto* ctx = static_cast<ButtonContext*>(button->getContext());
      ctx->self->publish_(ctx->id, action, clickCount);
    }, &_contexts[i]);
  }
}

void ButtonManager::subscribe(ButtonEventCallback callback, void* context) {
  if (!callback || _subscriberCount >= MAX_SUBSCRIBERS) return;
  _subscribers[_subscriberCount++] = { callback, context };
}

void ButtonManager::service() {
  SmartButton::service();
}

bool ButtonManager::isHeld(ButtonId id) const {
  SmartButton* b = _buttons[static_cast<uint8_t>(id)];
  return b && b->isPressedDebounced();
}

void ButtonManager::publish_(ButtonId id, ButtonAction action, int clickCount) {
  ButtonEvent event{ id, action, clickCount };
  for (int i = 0; i < _subscriberCount; ++i) {
    _subscribers[i].callback(_subscribers[i].context, event);
  }
}

}  // namespace input
}  // namespace trailair
