#pragma once
#include <Arduino.h>
#include <SmartButton.h>

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
  explicit Buttons(const Pins& pins) : pins_(pins),
    bLeft_(pins.left), bDown_(pins.down), bUp_(pins.up), bRight_(pins.right) {}

  void begin(ButtonCallback cb, void* ctx);
  void service();

private:
  static Buttons* s_instance_;
  static void leftCb_(smartbutton::SmartButton* b, smartbutton::SmartButton::Event ev, int clicks);
  static void downCb_(smartbutton::SmartButton* b, smartbutton::SmartButton::Event ev, int clicks);
  static void upCb_  (smartbutton::SmartButton* b, smartbutton::SmartButton::Event ev, int clicks);
  static void rightCb_(smartbutton::SmartButton* b, smartbutton::SmartButton::Event ev, int clicks);

  void dispatch_(ButtonId id, smartbutton::SmartButton::Event ev, int clicks);

private:
  Pins pins_;
  ButtonCallback cb_ = nullptr;
  void* cbCtx_ = nullptr;

  using SB = smartbutton::SmartButton;
  SB bLeft_, bDown_, bUp_, bRight_;
};

} // namespace input
} // namespace ta