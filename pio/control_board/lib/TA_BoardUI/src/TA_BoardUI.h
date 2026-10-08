#pragma once
#include <stdint.h>
#include <TA_Types.h>
#include <TA_Config.h>
#include <TA_Controller.h>
#include <TA_Protocol.h>
#include <TA_Display.h>

namespace trailair {
namespace boardui {

/**
 * @brief The board's UI state machine: which screen is up, the target PSI, and what each
 *        button does there. Drives the controller directly.
 *
 * The board is the master UI: board buttons and remote buttons both land in onButton(), and
 * the remote just displays status(). Screens:
 *   Idle/Done  Left=Manual, Right=seek, Up/Down=target (Done is a short "Done!" after a seek)
 *   Manual     hold Up=air, hold Down=vent, Left=back
 *   Seeking    Right=cancel
 *   Error      Right=acknowledge (also auto-clears after errorAutoClearDuration)
 */
class BoardUI {
public:
  explicit BoardUI(trailair::controller::PressureController& controller) : ctl_(controller) {}

  void begin(const trailair::config::UserInterfaceConfiguration& cfg = trailair::config::UserInterfaceConfiguration{});

  void onButton(const ButtonEvent& e, uint32_t now);

  /// Call every loop, before controller.update(). upHeld/downHeld are the board's own buttons
  /// physically held: they renew the controller's manual lease (remote holds renew themselves
  /// by repeating ButtonPress frames).
  void update(uint32_t now, bool upHeld, bool downHeld);

  View view() const { return view_; }
  float targetPsi() const { return target_; }

  /// What the remote displays. PSI is rounded the same way the screen rounds it, so both
  /// screens show the same number (the wire's 0.5 PSI steps would otherwise round differently).
  trailair::protocol::Status status() const;
  /// The board's own screen: same content as the remote's, minus link/battery widgets
  void fillDisplay(trailair::display::DisplayModel& m) const;

private:
  void stepTarget_(float delta);

  trailair::controller::PressureController& ctl_;
  trailair::config::UserInterfaceConfiguration cfg_{};

  View view_ = View::Idle;
  float target_ = 0.0f;
  uint32_t doneUntil_ = 0;
  uint32_t errorSince_ = 0;
};

} // namespace boardui
} // namespace trailair
