#pragma once
#include <Arduino.h>
#include <TA_Protocol.h>
#include <TA_Comms.h>
#include <TA_Display.h>
#include <TA_Input.h>

namespace ta {
namespace state {

enum class RemoteState { DISCONNECTED, IDLE, MANUAL, SEEKING, ERROR, PAIRING };
enum class ControlState { IDLE, AIRUP, VENTING, CHECKING, ERROR };

struct Config {
  float minPsi = 20.0f;
  float maxPsi = 45.0f;
  float defaultTargetPsi = 25.0f;

  uint32_t connectionHoldMs = 1000;  // show connected before leaving DISCONNECTED
  uint32_t doneHoldMs = 2000;        // “Done!” display hold in SEEKING
  uint32_t errorAutoClearMs = 5000;  // auto exit ERROR
  uint32_t manualRepeatMs = 500;     // resend manual command
};

class StateController {
public:
  StateController(ta::comms::EspNowLink& link, const Config& cfg = Config{});

  void begin();

  // App loop
  void update(uint32_t now, bool isConnected, bool isConnecting);

  // Inputs
  void onStatus(const ta::protocol::StatusMsg& msg);
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
  float targetPsi() const { return targetPsi_; }
  uint8_t lastError() const { return lastErrorCode_; }

private:
  void enter_(RemoteState s, uint32_t now);
  void handleButtonsIdle_(const ta::input::Event& e, uint32_t now);
  void handleButtonsManual_(const ta::input::Event& e, uint32_t now);
  void handleButtonsSeeking_(const ta::input::Event& e, uint32_t now);
  void handleButtonsDisconnected_(const ta::input::Event& e, uint32_t now);
  void handleButtonsError_(const ta::input::Event& e, uint32_t now);

private:
  ta::comms::EspNowLink& link_;
  Config cfg_;

  // Domain state
  RemoteState rState_ = RemoteState::DISCONNECTED;
  RemoteState rPrev_  = RemoteState::DISCONNECTED;
  ControlState cState_ = ControlState::IDLE;

  float currentPsi_ = 0.0f;
  float targetPsi_  = 25.0f;

  bool seekingActiveSeen_ = false;
  uint32_t seekingDoneUntil_ = 0;
  uint32_t disconnectedHoldUntil_ = 0;

  // Manual
  bool manualSending_ = false;
  uint8_t manualCode_ = 0x00; // 0x00=vent, 0xFF=air
  uint32_t lastManualSentMs_ = 0;
  uint32_t manualRepeatMs_ = 300;

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