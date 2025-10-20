#pragma once
#include <stdint.h>

namespace smartbutton { class SmartButton; }

namespace ta {
namespace input {

enum class ButtonId { Left, Down, Up, Right };
enum class Action   { Pressed, Released, Click, LongHold };

struct Event {
  ButtonId id;
  Action action;
  int clicks; // valid for Click
};

using ButtonCallback = void(*)(void* ctx, const Event& e);

struct Pins {
  uint8_t left, down, up, right;
};

class Buttons {
public:
  explicit Buttons(const Pins& pins) : pins_(pins) {}

  // Initialize hardware; callbacks are subscribed via subscribe()
  void begin();
  // Subscribe to events (simple event bus, supports multiple listeners)
  void subscribe(ButtonCallback cb, void* ctx);
  // Unsubscribe a previously registered listener
  void unsubscribe(ButtonCallback cb, void* ctx);
  // Remove all subscribers
  void clearSubscribers();

  void service();

private:
  // Per-button SmartButton callback context
  struct BtnCtx { Buttons* self; ButtonId id; };
  // Raw event hook from SmartButton callbacks (defined in .cpp)
  void onRawEvent_(ButtonId id, Action a, int clicks);

private:
  Pins pins_;

  // Subscribers
  static constexpr int kMaxSubs_ = 4;
  struct Sub { ButtonCallback cb; void* ctx; };
  Sub subs_[kMaxSubs_]{};
  int subCount_ = 0;

  // SmartButton instances (allocated in begin to avoid header dependency)
  smartbutton::SmartButton* bLeft_  = nullptr;
  smartbutton::SmartButton* bDown_  = nullptr;
  smartbutton::SmartButton* bUp_    = nullptr;
  smartbutton::SmartButton* bRight_ = nullptr;
  BtnCtx ctxLeft_{this, ButtonId::Left};
  BtnCtx ctxDown_{this, ButtonId::Down};
  BtnCtx ctxUp_{this, ButtonId::Up};
  BtnCtx ctxRight_{this, ButtonId::Right};
};

} // namespace input
} // namespace ta