#include "TA_State.h"
#include <Arduino.h>
#include <TA_Comms.h>
#include <TA_Config.h>
#include <TA_Display.h>
#include <TA_Time.h>

namespace trailair {
namespace state {

namespace {
  constexpr uint32_t kPairingFailHoldMs = 2000;
  const trailair::config::CommunicationConfiguration kLinkCfg{};

  uint8_t bitFor(ButtonId id) { return 1u << static_cast<uint8_t>(id); }
  bool isStepButton(ButtonId id) { return id == ButtonId::Up || id == ButtonId::Down; }
}

void StateController::resetAfterWake() {
  sleepRequested_ = false;
  pairingFailed_ = false;
  pairingBusy_ = false;
  heldMask_ = 0;
}

bool StateController::takeSleepRequest() {
  bool r = sleepRequested_;
  sleepRequested_ = false;
  return r;
}

void StateController::onBatteryPercent(int percent) {
  batteryPercent_ = constrain(percent, 0, 100);
}

void StateController::onStatus(const trailair::protocol::Status& status) {
  status_ = status;
}

void StateController::update(uint32_t now) {
  isConnected_ = link_.isConnected();

  if (pairingFailed_ && trailair::time::isTimeFor(now, pairingFailHoldUntil_)) {
    pairingFailed_ = false;
    pairingBusy_ = false;
  }

  if (heldMask_ && isConnected_ && trailair::time::isTimeFor(now, nextHoldRepeatAt_)) {
    for (uint8_t i = 0; i < BUTTON_COUNT; ++i) {
      if (heldMask_ & (1u << i)) link_.sendButton(ButtonAction::Pressed, static_cast<ButtonId>(i));
    }
    nextHoldRepeatAt_ = trailair::time::calculateFutureTime(now, kLinkCfg.manualRepeatIntervalMilliseconds);
  }
}

void StateController::onButton(const ButtonEvent& e) {
  // Left long-hold = sleep (remote only). SmartButton fires LongHold once per hold.
  if (e.id == ButtonId::Left && e.action == ButtonAction::LongHold) {
    sleepRequested_ = true;
    return;
  }

  // Right click: cancels an active pairing, or starts pairing when disconnected
  if (e.id == ButtonId::Right && e.action == ButtonAction::Click) {
    if (link_.isPairing()) {
      link_.cancelPairing();
      return;
    }
    if (!isConnected_) {
      link_.startPairing();
      return;
    }
  }

  if (isStepButton(e.id)) {
    if (e.action == ButtonAction::Released) {
      heldMask_ &= ~bitFor(e.id);
    } else if (e.action == ButtonAction::Pressed && isConnected_) {
      heldMask_ |= bitFor(e.id);
      nextHoldRepeatAt_ = trailair::time::calculateFutureTime(trailair::time::getMilliseconds(),
                                                              kLinkCfg.manualRepeatIntervalMilliseconds);
    }
  }

  // The board only gets input while linked. Releases always go out so it is never left "held".
  if ((!isConnected_ || link_.isPairing()) && e.action != ButtonAction::Released) return;

  // SmartButton merges rapid taps into one Click with clickCount=N; the board takes one tap per
  // frame. Only Up/Down (target steps) repeat: a double-tap on Left/Right must not toggle twice.
  int repeats = (e.action == ButtonAction::Click && isStepButton(e.id) && e.clickCount > 1) ? e.clickCount : 1;
  for (int i = 0; i < repeats; ++i) link_.sendButton(e.action, e.id);
}

void StateController::onPairEvent(trailair::comms::PairEvent ev) {
  using trailair::comms::PairEvent;
  pairingBusy_ = (ev == PairEvent::Busy);
  // Timeout/Busy show a failure message briefly; a user cancel or success just moves on
  pairingFailed_ = (ev == PairEvent::Busy || ev == PairEvent::Timeout);
  if (pairingFailed_) {
    pairingFailHoldUntil_ = trailair::time::calculateFutureTime(trailair::time::getMilliseconds(), kPairingFailHoldMs);
  }
}

void StateController::buildDisplayModel(trailair::display::DisplayModel& dm) const {
  // The board's state (master)
  dm.view = status_.view;
  dm.controllerState = status_.state;
  dm.currentPressurePSI = status_.currentPSI;
  dm.targetPressurePSI = status_.targetPSI;
  dm.errorCode = status_.errorCode;

  // Remote overlays
  dm.connected = isConnected_;
  dm.showBatteryIcon = true;
  dm.batteryPercentage = batteryPercent_;
  dm.showReconnectHint = !link_.hasPeer();  // paired remotes reconnect on their own
  dm.pairingFailed = pairingFailed_;
  dm.pairingBusy = pairingBusy_;

  if (link_.isPairing() || pairingFailed_) dm.view = View::Pairing;
  else if (!isConnected_) dm.view = View::Disconnected;
}

} // namespace state
} // namespace trailair
