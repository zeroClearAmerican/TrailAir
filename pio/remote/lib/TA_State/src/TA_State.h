#pragma once
#include <stdint.h>
#include <TA_UI.h>
#include <TA_Protocol.h>

// Forward declarations to avoid including Arduino and heavy headers here
namespace ta { namespace display { struct DisplayModel; } }
namespace ta { namespace input { struct Event; } }
namespace ta { namespace cfg { struct UiShared; struct LinkShared; } }
namespace ta { namespace comms { class EspNowLink; enum class PairEvent; } }

namespace ta {
namespace state {

enum class RemoteState { DISCONNECTED, IDLE, MANUAL, SEEKING, ERROR, PAIRING };
enum class ControlState { IDLE, AIRUP, VENTING, CHECKING, ERROR };

struct Config {
  const ta::cfg::UiShared* ui = nullptr;           // shared UI config (pointer to avoid heavy include)
  const ta::cfg::LinkShared* link = nullptr;       // shared link config (pointer to avoid heavy include)
};

class StateController {
public:
  StateController(ta::comms::EspNowLink& link, const Config& cfg = Config{});

  void begin();

  // App loop
  void update(uint32_t now, bool isConnected, bool isConnecting);

  // Inputs
  void onStatus(const ta::protocol::Response& msg);
  void onBatteryPercent(int percent);
  void onButton(const ta::input::Event& e);
  void onPairEvent(ta::comms::PairEvent ev, const uint8_t mac[6]);
  bool canStartPairing() const; 

  // UI
  void buildDisplayModel(ta::display::DisplayModel& dm) const;

  // Sleep request (e.g. from long-hold Left). Main should check and execute.
  bool takeSleepRequest();

  // Called after waking to reset connection/state visuals
  void resetAfterWake();

  // Accessors (if needed elsewhere)
  RemoteState remoteState() const { return rState_; }
  ControlState controlState() const { return cState_; }
  float currentPsi() const { return currentPsi_; }
  float targetPsi() const { return ui_.targetPsi(); }
  uint8_t lastError() const { return lastErrorCode_; }

private:
  void enter_(RemoteState s, uint32_t now);
  void handleButtonsDisconnected_(const ta::input::Event& e, uint32_t now);

  // Device action bridge for UI layer
  struct RemoteActions : ta::ui::DeviceActions {
    StateController* self = nullptr;
    bool isConnected() const override { return self ? self->isConnected_ : true; }
    void cancel() override;
    void clearError() override { cancel(); }
    void startSeek(float targetPsi) override;
    void manualVent(bool on) override;
    void manualAirUp(bool on) override;
  };

  static inline ta::ui::Ctrl toUiCtrl(ControlState s) {
    switch (s) {
      case ControlState::IDLE: return ta::ui::Ctrl::Idle;
      case ControlState::AIRUP: return ta::ui::Ctrl::AirUp;
      case ControlState::VENTING: return ta::ui::Ctrl::Venting;
      case ControlState::CHECKING: return ta::ui::Ctrl::Checking;
      case ControlState::ERROR: return ta::ui::Ctrl::Error;
    }
    return ta::ui::Ctrl::Idle;
  }

private:
  ta::comms::EspNowLink& link_;
  Config cfg_;

  // Shared UI state machine
  ta::ui::UiStateMachine ui_{};

  // Domain state
  RemoteState rState_ = RemoteState::DISCONNECTED;
  RemoteState rPrev_  = RemoteState::DISCONNECTED;
  ControlState cState_ = ControlState::IDLE;

  float currentPsi_ = 0.0f;

  // Manual
  bool manualSending_ = false;
  uint8_t manualCode_ = 0x00; // 0x00=vent, 0xFF=air
  uint32_t lastManualSentMs_ = 0;

  // Errors and battery
  uint8_t lastErrorCode_ = 0;
  int batteryPercent_ = 0;

  // Connection flags mirrored each update
  bool isConnected_ = false;
  bool isConnecting_ = false;

  // Timing
  uint32_t stateEntryMs_ = 0;

  // Sleep
  bool sleepRequested_ = false;

  // Suppress spurious Left click after a LongHold (prevents Manual flicker)
  uint32_t suppressLeftClicksUntil_ = 0;

  // Pairing
  bool pairingFailed_ = false;
  bool pairingBusy_ = false;
  uint32_t pairingFailHoldUntil_ = 0;  // show failure msg window

  // Error clear request and sleep hold flags
  bool errorClearRequested_ = false;
  bool leftSleepHold_ = false;      // suppress any Left-derived actions after sleep hold
  bool leftLongHoldActive_ = false;   // latch after Left long-hold (sleep), swallows later Left events

  // Left button press state
  bool leftPressed_ = false;          // true between Left Pressed and Released
};

} // namespace state
} // namespace ta