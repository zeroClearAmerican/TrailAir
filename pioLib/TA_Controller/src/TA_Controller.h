#pragma once
#include <stdint.h>
#include "TA_Protocol.h"
#include <TA_Errors.h>

namespace ta {
namespace act { class Actuators; }
namespace ctl {

enum class State { IDLE, AIRUP, VENTING, CHECKING, ERROR };

// Keep internal enum but map values to shared catalog for wire/display compatibility
enum class ErrorCode : uint8_t {
  NONE = ta::errors::NONE,
  NO_CHANGE = ta::errors::NO_CHANGE,
  EXCESSIVE_TIME = ta::errors::EXCESSIVE_TIME,
  // Additional internal codes can be added; default mapping uses raw byte
  UNKNOWN = ta::errors::UNKNOWN
};

// Injectable outputs for unit testing or alternative drivers
struct IOutputs {
  virtual ~IOutputs() = default;
  virtual void setCompressor(bool on) = 0;
  virtual void setVent(bool open) = 0;
  virtual void stopAll() = 0;
};

struct Config {
  float minPsi = 5.0f;
  float maxPsi = 50.0f;
  float psiTol = 0.1f;
  unsigned long settleMs = 1000;
  unsigned long burstMsInit = 5000;
  unsigned long runMinMs = 1000;
  unsigned long runMaxMs = 4000;
  unsigned long manualRefreshTimeoutMs = 1000;
  unsigned long maxContinuousMs = 30UL * 60UL * 1000UL; // 30 minutes
  float noChangeEps = 0.02f;
  int maxNoChangeBursts = 3;
  // Newly exposed tuning parameters (formerly magic numbers)
  float aimMarginPsi = 0.2f;      // margin before final approach
  float dPsiNoiseEps = 0.01f;     // noise threshold when computing rates
  float rateMinEps = 0.001f;      // minimal rate to consider valid
  float checkDtMinSec = 0.02f;    // minimal time window to consider (seconds)
};

class Controller {
public:
  // Backward compatible: use board Actuators through adapter
  void begin(ta::act::Actuators* act, const Config& cfg);

  // New: directly inject an outputs implementation
  void begin(IOutputs* outputs, const Config& cfg);

  void update(uint32_t nowMs, float currentPsi);

  // Commands
  void startSeek(float targetPsi);
  void manualAirUp(bool active);
  void manualVent(bool active);
  void cancel();
  void clearError();

  // Accessors
  State state() const { return state_; }
  ErrorCode error() const { return errorCode_; }
  float targetPsi() const { return targetPsi_; }
  float currentPsi() const { return currentPsi_; }

  char statusChar() const; // Map state to protocol char
  uint8_t errorByte() const { return (uint8_t)errorCode_; }

private:
  // Per-state handlers
  void handleRunPhase_(State runState, uint32_t now);
  void handleChecking_(uint32_t now);
  void handleIdle_(uint32_t now);
  
  void enter_(State s, uint32_t now);
  void stopOutputs_();
  void scheduleBurst_(State dir, unsigned long durMs, uint32_t now);
  void enterError_(ErrorCode ec, const char* why);

  void reset_();

  // Outputs
  struct ActuatorAdapter : IOutputs {
    ta::act::Actuators* hw = nullptr;
    void setCompressor(bool on) override;
    void setVent(bool open) override;
    void stopAll() override;
  } actAdapter_;

  IOutputs* out_ = nullptr;

  Config cfg_{};

  State state_ = State::IDLE;
  State prev_ = State::IDLE;

  // Runtime
  float targetPsi_ = 0;
  float currentPsi_ = 0;
  bool manualActive_ = false;
  uint32_t lastManualRefreshMs_ = 0;

  // Phases
  bool inContinuous_ = false;
  uint32_t phaseStartMs_ = 0;
  uint32_t phaseEndMs_ = 0;
  uint32_t lastBurstEndMs_ = 0;
  float phaseStartPsi_ = 0;

  // Learned rates
  float upRate_ = 0;
  float downRate_ = 0;
  int upSamples_ = 0;
  int downSamples_ = 0;

  // Errors
  ErrorCode errorCode_ = ErrorCode::NONE;
  int noChangeBurstCount_ = 0;
};

} // namespace ctl
} // namespace ta