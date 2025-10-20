#pragma once
#include <stdint.h>

namespace ta { namespace ui {

// Device-independent UI states for TrailAir
enum class View { Idle, Manual, Seeking, Error, Disconnected, Pairing };

struct UiConfig {
  float minPsi = 0.0f;
  float maxPsi = 50.0f;
  float defaultTargetPsi = 32.0f;
  float stepSmall = 1.0f;
  uint32_t doneHoldMs = 1500;       // "Done" flash after seeking
  uint32_t errorAutoClearMs = 4000; // optional auto-clear window
};

// Inputs common to both devices
enum class Button { Left, Down, Up, Right };
enum class Action { Pressed, Released, Click, LongHold };

struct ButtonEvent { Button id; Action action; };

// Abstract device strategy for actions; implemented by board and remote
struct DeviceActions {
  virtual ~DeviceActions() = default;
  virtual void cancel() = 0;         // cancel manual/seek
  virtual void clearError() = 0;     // clear/acknowledge error
  virtual void startSeek(float targetPsi) = 0;
  virtual void manualVent(bool on) = 0;
  virtual void manualAirUp(bool on) = 0;
  virtual bool isConnected() const { return true; }
};

// Controller activity (mapped from concrete controllers)
enum class Ctrl { Idle, AirUp, Venting, Checking, Error };

struct UiModel {
  // inputs for rendering layer
  float currentPsi = 0.0f;
  float targetPsi = 0.0f;
  Ctrl ctrl = Ctrl::Idle;
  View view = View::Idle;
  bool showDoneHold = false;
  uint8_t lastErrorCode = 0;
  // optional fields the board/remote can ignore/fill
  bool isConnected = true;
  int batteryPercent = 0;
  bool showReconnectHint = false;
  bool pairingActive = false;
  bool pairingFailed = false;
  bool pairingBusy = false;
};

class UiStateMachine {
public:
  UiStateMachine() = default;
  explicit UiStateMachine(const UiConfig& cfg) : cfg_(cfg), targetPsi_(cfg.defaultTargetPsi) {}
  
  void begin(const UiConfig& cfg) { cfg_ = cfg; targetPsi_ = cfg_.defaultTargetPsi; clampTarget_(); }

  void update(uint32_t now, DeviceActions& dev, Ctrl ctrlState);
  void onButton(const ButtonEvent& e, DeviceActions& dev);

  void setTargetPsi(float psi) { targetPsi_ = psi; clampTarget_(); }
  float targetPsi() const { return targetPsi_; }
  View view() const { return view_; }
  float minPsi() const { return cfg_.minPsi; }
  float maxPsi() const { return cfg_.maxPsi; }

  // expose done-hold flag for model building
  bool isDoneHoldActive(uint32_t now) const { return showDoneHold_ && now < doneHoldUntil_; }

private:
  void clampTarget_() { if (targetPsi_ < cfg_.minPsi) targetPsi_ = cfg_.minPsi; if (targetPsi_ > cfg_.maxPsi) targetPsi_ = cfg_.maxPsi; }
  
  UiConfig cfg_{};
  View view_ = View::Idle;
  float targetPsi_ = 32.0f;

  // seeking/done
  bool seenSeekingActivity_ = false;
  bool showDoneHold_ = false;
  uint32_t doneHoldUntil_ = 0;

  // error autoclear
  uint32_t errorEntryMs_ = 0;

  // manual flags
  bool manualVentActive_ = false;
  bool manualAirActive_ = false;
};

}} // namespace ta::ui
