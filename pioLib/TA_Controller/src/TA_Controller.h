#pragma once
#include <Arduino.h>
#include "TA_Protocol.h"
#include "TA_Actuators.h"

namespace ta {
namespace ctl {

enum class State { IDLE, AIRUP, VENTING, CHECKING, ERROR };

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
};

enum class ErrorCode : uint8_t {
  NONE = 0,
  NO_CHANGE = 1,
  EXCESSIVE_TIME = 2,
  UNKNOWN = 255
};

class Controller {
public:
  void begin(ta::act::Actuators* act, const Config& cfg) {
    act_ = act;
    cfg_ = cfg;
    reset_();
  }

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
  void enter_(State s, uint32_t now);
  void stopOutputs_();
  void scheduleBurst_(State dir, unsigned long durMs, uint32_t now);
  void enterError_(ErrorCode ec, const char* why);

  void reset_();

  ta::act::Actuators* act_ = nullptr;
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