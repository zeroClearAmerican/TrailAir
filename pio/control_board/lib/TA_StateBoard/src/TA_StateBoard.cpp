#include "TA_StateBoard.h"

using namespace ta::stateboard;

void StateBoard::begin() {
  Config def; begin(def);
}

void StateBoard::begin(const Config& cfg) {
  cfg_ = cfg;
  ta::ui::UiConfig uicfg;
  uicfg.minPsi = cfg_.ui.minPsi;
  uicfg.maxPsi = cfg_.ui.maxPsi;
  uicfg.defaultTargetPsi = cfg_.ui.defaultTargetPsi;
  uicfg.stepSmall = cfg_.ui.stepSmall;
  uicfg.doneHoldMs = cfg_.ui.doneHoldMs;
  uicfg.errorAutoClearMs = cfg_.ui.errorAutoClearMs;
  ui_.begin(uicfg);
}

static ta::ui::Ctrl mapCtrl(ta::ctl::State s) {
  switch (s) {
    case ta::ctl::State::IDLE: return ta::ui::Ctrl::Idle;
    case ta::ctl::State::AIRUP: return ta::ui::Ctrl::AirUp;
    case ta::ctl::State::VENTING: return ta::ui::Ctrl::Venting;
    case ta::ctl::State::CHECKING: return ta::ui::Ctrl::Checking;
    case ta::ctl::State::ERROR: return ta::ui::Ctrl::Error;
  }
  return ta::ui::Ctrl::Idle;
}

ta::ui::Ctrl StateBoard::toUiCtrl_(ta::ctl::State s) { return mapCtrl(s); }

static ta::ui::Button toBtn(ta::input::ButtonId id) {
  switch (id) {
    case ta::input::ButtonId::Left: return ta::ui::Button::Left;
    case ta::input::ButtonId::Down: return ta::ui::Button::Down;
    case ta::input::ButtonId::Up:   return ta::ui::Button::Up;
    case ta::input::ButtonId::Right:return ta::ui::Button::Right;
  }
  return ta::ui::Button::Left;
}

static ta::ui::Action toAct(ta::input::Action a) {
  switch (a) {
    case ta::input::Action::Pressed: return ta::ui::Action::Pressed;
    case ta::input::Action::Released:return ta::ui::Action::Released;
    case ta::input::Action::Click:   return ta::ui::Action::Click;
    case ta::input::Action::LongHold:return ta::ui::Action::LongHold;
  }
  return ta::ui::Action::Click;
}

ta::ui::ButtonEvent StateBoard::toUiBtn_(const ta::input::Event& ev) {
  return ta::ui::ButtonEvent{ toBtn(ev.id), toAct(ev.action) };
}

void StateBoard::onButton(const ta::input::Event& ev, ta::ctl::Controller& controller) {
  BoardActions act; act.ctl = &controller;
  ui_.onButton(toUiBtn_(ev), act);
}

void StateBoard::update(uint32_t now,
                        ta::ctl::Controller& controller,
                        const ta::comms::BoardLink& link) {
  (void)link;
  BoardActions act; act.ctl = &controller;
  ui_.update(now, act, toUiCtrl_(controller.state()));
}

void StateBoard::buildDisplayModel(ta::display::DisplayModel& m,
                                   const ta::ctl::Controller& controller,
                                   const ta::comms::BoardLink& link,
                                   uint32_t now) const {
  // PSI
  m.currentPSI = controller.currentPsi();
  m.targetPSI  = ui_.targetPsi();

  // Link icon
  m.link = (link.isPaired() && link.isRemoteActive(cfg_.link.remoteActiveTimeoutMs))
          ? ta::display::Link::Connected
          : ta::display::Link::Disconnected;

  // Ctrl
  switch (controller.state()) {
    case ta::ctl::State::IDLE:     m.ctrl = ta::display::Ctrl::Idle; break;
    case ta::ctl::State::AIRUP:    m.ctrl = ta::display::Ctrl::AirUp; break;
    case ta::ctl::State::VENTING:  m.ctrl = ta::display::Ctrl::Venting; break;
    case ta::ctl::State::CHECKING: m.ctrl = ta::display::Ctrl::Checking; break;
    case ta::ctl::State::ERROR:    m.ctrl = ta::display::Ctrl::Error; break;
  }

  // View mapping
  switch (ui_.view()) {
    case ta::ui::View::Idle:         m.view = ta::display::View::Idle; break;
    case ta::ui::View::Manual:       m.view = ta::display::View::Manual; break;
    case ta::ui::View::Seeking:      m.view = ta::display::View::Seeking; break;
    case ta::ui::View::Error:        m.view = ta::display::View::Error; break;
    case ta::ui::View::Disconnected: m.view = ta::display::View::Idle; break; // board never disconnected view
    case ta::ui::View::Pairing:      m.view = ta::display::View::Idle; break;
  }

  // Done hold
  m.seekingShowDoneHold = ui_.isDoneHoldActive(now);

  // Error code
  if (controller.state() == ta::ctl::State::ERROR) {
    m.lastErrorCode = controller.errorByte();
  } else {
    m.lastErrorCode = 0;
  }

  // Unused
  m.batteryPercent = 0;
  m.showReconnectHint = false;
  m.pairingActive = false;
  m.pairingFailed = false;
  m.pairingBusy = false;
}