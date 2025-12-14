#pragma once
#include <stdint.h>
#include <TA_Protocol.h>

// Forward declarations
namespace trailair { namespace display { struct DisplayModel; } }
namespace trailair { namespace input { struct ButtonEvent; } }
namespace trailair { namespace comms { class EspNowLink; enum class PairEvent; } }

namespace trailair { namespace state {

/**
 * @brief Thin client state controller - remote as wireless input/display device
 * 
 * This controller does NOT run its own UI state machine. Instead:
 * - Buttons are forwarded directly to the board
 * - Display shows exactly what the board reports
 * - Only remote-specific logic lives here (battery, sleep, pairing)
 */
class StateController {
public:
  explicit StateController(trailair::comms::EspNowLink& link);

  void begin();
  void update(uint32_t now, bool isConnected, bool isConnecting);

  // Input from board (master state) - board tells us what to display
  void onStatus(const trailair::protocol::Response& msg);

  // Input from local buttons → forward to board (except remote-specific actions)
  void onButton(const trailair::input::ButtonEvent& e);

  // Remote-specific inputs
  void onBatteryPercent(int percent);
  void onPairEvent(trailair::comms::PairEvent ev, const uint8_t mac[6]);

  // Sleep request (e.g. from long-hold Left). App should check and execute.
  bool takeSleepRequest();

  // Called after waking to reset connection/state visuals
  void resetAfterWake();

  // Build display model - shows board's state plus remote overlays
  void buildDisplayModel(trailair::display::DisplayModel& dm) const;

  // Accessors (for debugging/testing)
  float currentPsi() const { return currentPsi_; }
  float targetPsi() const { return targetPsi_; }
  uint8_t lastError() const { return lastErrorCode_; }

private:
  trailair::comms::EspNowLink& link_;

  // Board's state (received via onStatus - board is master)
  float currentPsi_ = 0.0f;
  float targetPsi_ = 0.0f;
  trailair::protocol::UIState boardUIState_ = trailair::protocol::UIState::Idle;
  trailair::protocol::StatusCode boardControllerStatus_ = trailair::protocol::StatusCode::Idle;
  uint8_t lastErrorCode_ = 0;

  // Remote-only state
  int batteryPercent_ = 0;
  bool isConnected_ = false;
  bool isConnecting_ = false;
  bool sleepRequested_ = false;

  // Pairing state
  bool pairingActive_ = false;
  bool pairingFailed_ = false;
  bool pairingBusy_ = false;
  uint32_t pairingFailHoldUntil_ = 0;  // Show failure message window

  // Sleep handling
  uint32_t lastButtonTime_ = 0;
  bool leftLongHoldSent_ = false;  // Prevent duplicate long-holds
};

} // namespace state
} // namespace trailair
