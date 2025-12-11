#include "TA_StateBoard.h"

using namespace trailair::stateboard;

void StateBoard::begin() {
  Config def; begin(def);
}

void StateBoard::begin(const Config& cfg) {
  cfg_ = cfg;
  trailair::ui::UserInterfaceConfig uicfg;
  uicfg.minimumPSI = cfg_.ui.minimumPressurePSI;
  uicfg.maximumPSI = cfg_.ui.maximumPressurePSI;
  uicfg.defaultTargetPSI = cfg_.ui.defaultTargetPressurePSI;
  uicfg.stepSize = cfg_.ui.pressureStepSmallPSI;
  uicfg.doneHoldDurationMilliseconds = cfg_.ui.doneHoldDurationMilliseconds;
  uicfg.errorAutoClearDurationMilliseconds = cfg_.ui.errorAutoClearDurationMilliseconds;
  ui_.begin(uicfg);
}

static trailair::ui::ControllerState mapCtrl(trailair::controller::ControllerState s) {
  switch (s) {
    case trailair::controller::ControllerState::Idle: return trailair::ui::ControllerState::Idle;
    case trailair::controller::ControllerState::AirUp: return trailair::ui::ControllerState::AirUp;
    case trailair::controller::ControllerState::Venting: return trailair::ui::ControllerState::Venting;
    case trailair::controller::ControllerState::Checking: return trailair::ui::ControllerState::Checking;
    case trailair::controller::ControllerState::Error: return trailair::ui::ControllerState::Error;
  }
  return trailair::ui::ControllerState::Idle;
}

trailair::ui::ControllerState StateBoard::toUiCtrl_(trailair::controller::ControllerState s) { return mapCtrl(s); }

static trailair::ui::ButtonId toBtn(trailair::input::ButtonId id) {
  switch (id) {
    case trailair::input::ButtonId::Left: return trailair::ui::ButtonId::Left;
    case trailair::input::ButtonId::Down: return trailair::ui::ButtonId::Down;
    case trailair::input::ButtonId::Up:   return trailair::ui::ButtonId::Up;
    case trailair::input::ButtonId::Right:return trailair::ui::ButtonId::Right;
  }
  return trailair::ui::ButtonId::Left;
}

static trailair::ui::ButtonAction toAct(trailair::input::ButtonAction a) {
  switch (a) {
    case trailair::input::ButtonAction::Pressed: return trailair::ui::ButtonAction::Pressed;
    case trailair::input::ButtonAction::Released:return trailair::ui::ButtonAction::Released;
    case trailair::input::ButtonAction::Click:   return trailair::ui::ButtonAction::Click;
    case trailair::input::ButtonAction::LongHold:return trailair::ui::ButtonAction::LongHold;
  }
  return trailair::ui::ButtonAction::Click;
}

trailair::ui::ButtonEvent StateBoard::toUiBtn_(const trailair::input::ButtonEvent& ev) {
  return trailair::ui::ButtonEvent{ toBtn(ev.id), toAct(ev.action) };
}

void StateBoard::onButton(const trailair::input::ButtonEvent& ev, trailair::controller::PressureController& controller) {
  BoardActions act; act.ctl = &controller;
  ui_.onButton(toUiBtn_(ev), act);
}

void StateBoard::update(uint32_t now,
                        trailair::controller::PressureController& controller,
                        const trailair::comms::BoardLink& link) {
  (void)link;
  BoardActions act; act.ctl = &controller;
  ui_.update(now, act, toUiCtrl_(controller.getState()));
}

void StateBoard::buildDisplayModel(trailair::display::DisplayModel& m,
                                   const trailair::controller::PressureController& controller,
                                   const trailair::comms::BoardLink& link,
                                   uint32_t now) const {
  // PSI
  m.currentPressurePSI = controller.getCurrentPSI();
  m.targetPressurePSI  = ui_.getTargetPSI();

  // Link icon
  m.connectionStatus = (link.isPaired() && link.isRemoteActive(cfg_.link.remoteActiveTimeoutMilliseconds))
          ? trailair::display::ConnectionStatus::Connected
          : trailair::display::ConnectionStatus::Disconnected;

  // Ctrl
  switch (controller.getState()) {
    case trailair::controller::ControllerState::Idle:     m.controllerActivity = trailair::display::ControllerActivity::Idle; break;
    case trailair::controller::ControllerState::AirUp:    m.controllerActivity = trailair::display::ControllerActivity::AirUp; break;
    case trailair::controller::ControllerState::Venting:  m.controllerActivity = trailair::display::ControllerActivity::Venting; break;
    case trailair::controller::ControllerState::Checking: m.controllerActivity = trailair::display::ControllerActivity::Checking; break;
    case trailair::controller::ControllerState::Error:    m.controllerActivity = trailair::display::ControllerActivity::Error; break;
  }

  // View mapping: Override UI state machine view with controller-derived view
  // This ensures the display matches what's actually happening even when controlled remotely
  trailair::ui::ViewState uiView = ui_.getViewState();
  
  // If controller is in error, always show error view
  if (controller.getState() == trailair::controller::ControllerState::Error) {
    m.viewType = trailair::display::ViewType::Error;
  }
  // If controller is actively seeking but UI doesn't know it (remote command), show seeking
  else if ((controller.getState() == trailair::controller::ControllerState::AirUp ||
            controller.getState() == trailair::controller::ControllerState::Venting ||
            controller.getState() == trailair::controller::ControllerState::Checking) &&
           uiView == trailair::ui::ViewState::Idle) {
    // Controller is active but UI thinks we're idle - must be remote-initiated seeking
    m.viewType = trailair::display::ViewType::Seeking;
  }
  // Normal UI state mapping
  else {
    switch (uiView) {
      case trailair::ui::ViewState::Idle:         m.viewType = trailair::display::ViewType::Idle; break;
      case trailair::ui::ViewState::Manual:       m.viewType = trailair::display::ViewType::Manual; break;
      case trailair::ui::ViewState::Seeking:      m.viewType = trailair::display::ViewType::Seeking; break;
      case trailair::ui::ViewState::Error:        m.viewType = trailair::display::ViewType::Error; break;
      case trailair::ui::ViewState::Disconnected: m.viewType = trailair::display::ViewType::Idle; break; // board never disconnected view
      case trailair::ui::ViewState::Pairing:      m.viewType = trailair::display::ViewType::Idle; break;
    }
  }

  // Done hold
  m.seekingShowDoneHold = ui_.isDoneHoldActive(now);

  // Error code
  if (controller.getState() == trailair::controller::ControllerState::Error) {
    m.lastErrorCode = controller.getErrorByte();
  } else {
    m.lastErrorCode = 0;
  }

  // Unused
  m.batteryPercentage = 0;
  m.showBatteryIcon = false;  // Control board is externally powered, no battery
  m.showReconnectHint = false;
  m.pairingActive = false;
  m.pairingFailed = false;
  m.pairingBusy = false;
}
