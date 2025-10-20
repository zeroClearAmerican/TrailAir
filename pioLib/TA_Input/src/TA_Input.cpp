#include <Arduino.h>
#include "TA_Input.h"
#include <SmartButton.h>
using namespace smartbutton;

namespace ta {
namespace input {

namespace {
  // mapping table for SmartButton::Event -> Action
  static inline Action mapEv(SmartButton::Event ev) {
    switch (ev) {
      case SmartButton::Event::PRESSED:    return Action::Pressed;
      case SmartButton::Event::RELEASED:   return Action::Released;
      case SmartButton::Event::CLICK:      return Action::Click;
      case SmartButton::Event::LONG_HOLD:  return Action::LongHold;
      default:                             return Action::Click;
    }
  }
}

void Buttons::begin() {
  // Configure pins
  pinMode(pins_.left,  INPUT_PULLUP);
  pinMode(pins_.down,  INPUT_PULLUP);
  pinMode(pins_.up,    INPUT_PULLUP);
  pinMode(pins_.right, INPUT_PULLUP);

  // Allocate SmartButtons and bind instance-aware callbacks via context
  bLeft_  = new SmartButton(pins_.left);
  bDown_  = new SmartButton(pins_.down);
  bUp_    = new SmartButton(pins_.up);
  bRight_ = new SmartButton(pins_.right);

  // Use the SmartButton context to pass our BtnCtx pointer
  bLeft_->begin([](SmartButton* b, SmartButton::Event ev, int clicks){
    auto* ctx = static_cast<BtnCtx*>(b->getContext());
    ctx->self->onRawEvent_(ctx->id, mapEv(ev), clicks);
  }, &ctxLeft_);
  bDown_->begin([](SmartButton* b, SmartButton::Event ev, int clicks){
    auto* ctx = static_cast<BtnCtx*>(b->getContext());
    ctx->self->onRawEvent_(ctx->id, mapEv(ev), clicks);
  }, &ctxDown_);
  bUp_->begin([](SmartButton* b, SmartButton::Event ev, int clicks){
    auto* ctx = static_cast<BtnCtx*>(b->getContext());
    ctx->self->onRawEvent_(ctx->id, mapEv(ev), clicks);
  }, &ctxUp_);
  bRight_->begin([](SmartButton* b, SmartButton::Event ev, int clicks){
    auto* ctx = static_cast<BtnCtx*>(b->getContext());
    ctx->self->onRawEvent_(ctx->id, mapEv(ev), clicks);
  }, &ctxRight_);
}

void Buttons::subscribe(ButtonCallback cb, void* ctx) {
  if (!cb || subCount_ >= kMaxSubs_) return;
  subs_[subCount_++] = { cb, ctx };
}

void Buttons::unsubscribe(ButtonCallback cb, void* ctx) {
  for (int i = 0; i < subCount_; ++i) {
    if (subs_[i].cb == cb && subs_[i].ctx == ctx) {
      // Compact array
      for (int j = i + 1; j < subCount_; ++j) subs_[j-1] = subs_[j];
      --subCount_;
      break;
    }
  }
}

void Buttons::clearSubscribers() {
  subCount_ = 0;
  for (int i = 0; i < kMaxSubs_; ++i) subs_[i] = { nullptr, nullptr };
}

void Buttons::service() {
  SmartButton::service();
}

void Buttons::onRawEvent_(ButtonId id, Action a, int clicks) {
  if (subCount_ == 0) return;
  Event e{ id, a, clicks };
  for (int i = 0; i < subCount_; ++i) {
    if (subs_[i].cb) subs_[i].cb(subs_[i].ctx, e);
  }
}

} // namespace input
} // namespace ta