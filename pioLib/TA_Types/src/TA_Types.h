#pragma once
#include <stdint.h>

/**
 * @file TA_Types.h
 * @brief The shared vocabulary of both devices: buttons, controller state, screens.
 *
 * One definition each, used by input, protocol, controller, UI and display. Enums that go
 * over the air use their wire character as their value, so encoding is a cast.
 */

namespace trailair {

/// Physical button. Values are the wire encoding.
enum class ButtonId : uint8_t { Left = 0, Down = 1, Up = 2, Right = 3 };
constexpr uint8_t BUTTON_COUNT = 4;

enum class ButtonAction : uint8_t {
  Pressed,   ///< Went down
  Released,  ///< Came up
  Click,     ///< Short press and release; clickCount says how many quick taps were merged
  LongHold   ///< Held past the long-hold threshold (fires once per hold)
};

struct ButtonEvent {
  ButtonId id;
  ButtonAction action;
  int clickCount;  ///< Valid for Click
};

/// What the pressure controller is doing. Values are the wire encoding.
enum class ControllerState : uint8_t {
  Idle     = 'I',
  AirUp    = 'U',  ///< Compressor on
  Venting  = 'V',  ///< Vent open
  Checking = 'C',  ///< Settling between bursts
  Error    = 'E'
};

/// Which screen is shown. The board owns Idle..Error and sends them to the remote;
/// Disconnected and Pairing are remote-side screens and never go over the air.
enum class View : uint8_t {
  Idle         = 'I',  ///< Current vs target PSI
  Manual       = 'M',  ///< Direct air/vent control
  Seeking      = 'S',  ///< Automatic seek in progress
  Done         = 'D',  ///< Seek just finished ("Done!")
  Error        = 'E',
  Disconnected = 'X',
  Pairing      = 'P'
};

} // namespace trailair
