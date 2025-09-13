#include "TA_Input.h"
using namespace smartbutton;

namespace ta {
namespace input {

Buttons* Buttons::s_instance_ = nullptr;

void Buttons::begin(ButtonCallback cb, void* ctx) {
  s_instance_ = this;
  cb_ = cb; cbCtx_ = ctx;

  pinMode(pins_.left,  INPUT_PULLUP);
  pinMode(pins_.down,  INPUT_PULLUP);
  pinMode(pins_.up,    INPUT_PULLUP);
  pinMode(pins_.right, INPUT_PULLUP);

  bLeft_.begin(&Buttons::leftCb_);
  bDown_.begin(&Buttons::downCb_);
  bUp_.begin(&Buttons::upCb_);
  bRight_.begin(&Buttons::rightCb_);
}

void Buttons::service() {
  SmartButton::service();
}

static inline ta::input::Action mapAction_(SmartButton::Event ev) {
  switch (ev) {
    case SmartButton::Event::PRESSED:    return ta::input::Action::Pressed;
    case SmartButton::Event::RELEASED:   return ta::input::Action::Released;
    case SmartButton::Event::CLICK:      return ta::input::Action::Click;
    case SmartButton::Event::LONG_HOLD:  return ta::input::Action::LongHold;
    default:                             return ta::input::Action::Click;
  }
}

void Buttons::dispatch_(ButtonId id, SmartButton::Event ev, int clicks) {
  if (!cb_) return;
  Event e{ id, mapAction_(ev), clicks };
  cb_(cbCtx_, e);
}

void Buttons::leftCb_(SmartButton* /*b*/, SmartButton::Event ev, int clicks)  { if (s_instance_) s_instance_->dispatch_(ButtonId::Left,  ev, clicks); }
void Buttons::downCb_(SmartButton* /*b*/, SmartButton::Event ev, int clicks)  { if (s_instance_) s_instance_->dispatch_(ButtonId::Down,  ev, clicks); }
void Buttons::upCb_  (SmartButton* /*b*/, SmartButton::Event ev, int clicks)  { if (s_instance_) s_instance_->dispatch_(ButtonId::Up,    ev, clicks); }
void Buttons::rightCb_(SmartButton* /*b*/, SmartButton::Event ev, int clicks) { if (s_instance_) s_instance_->dispatch_(ButtonId::Right, ev, clicks); }

} // namespace input
} // namespace ta