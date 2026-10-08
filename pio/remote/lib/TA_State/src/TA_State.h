#pragma once
#include <stdint.h>
#include <TA_Types.h>
#include <TA_Protocol.h>

namespace trailair { namespace display { struct DisplayModel; } }
namespace trailair { namespace comms { class RemoteLink; enum class PairEvent; } }

namespace trailair {
namespace state {

/**
 * @brief Remote as a thin client: buttons go to the board, the screen shows the board's status.
 *
 * Only remote-specific behavior lives here: sleep (Left long-hold), pairing (Right click while
 * disconnected), battery, and renewing manual holds (re-sending ButtonPress while Up/Down is
 * held, which the board treats as a lease renewal).
 */
class StateController {
public:
  explicit StateController(trailair::comms::RemoteLink& link) : link_(link) {}

  void update(uint32_t now);
  void onStatus(const trailair::protocol::Status& status);
  void onButton(const ButtonEvent& e);
  void onBatteryPercent(int percent);
  void onPairEvent(trailair::comms::PairEvent ev);

  /// Left long-hold asked for sleep. The app checks and executes.
  bool takeSleepRequest();
  /// Clear transient state after waking
  void resetAfterWake();

  void buildDisplayModel(trailair::display::DisplayModel& dm) const;

private:
  trailair::comms::RemoteLink& link_;

  trailair::protocol::Status status_{};  ///< Latest from the board (the board is master)
  int batteryPercent_ = 0;
  bool isConnected_ = false;
  bool sleepRequested_ = false;

  // Pairing failure message (Timeout/Busy) shows briefly
  bool pairingFailed_ = false;
  bool pairingBusy_ = false;
  uint32_t pairingFailHoldUntil_ = 0;

  // Up/Down held on the board: re-send ButtonPress so the board's manual lease stays renewed
  uint8_t heldMask_ = 0;
  uint32_t nextHoldRepeatAt_ = 0;
};

} // namespace state
} // namespace trailair
