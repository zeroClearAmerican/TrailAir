#include "TA_Controller.h"
#include <math.h>

using namespace ta::ctl;

void Controller::reset_() {
  state_ = State::IDLE;
  prev_ = State::IDLE;
  targetPsi_ = 0;
  manualActive_ = false;
  inContinuous_ = false;
  upRate_ = downRate_ = 0;
  upSamples_ = downSamples_ = 0;
  noChangeBurstCount_ = 0;
  errorCode_ = ErrorCode::NONE;
}

void Controller::stopOutputs_() {
  if (act_) act_->stopAll();
}

char Controller::statusChar() const {
  switch (state_) {
    case State::IDLE:     return 'I';
    case State::AIRUP:    return 'U';
    case State::VENTING:  return 'V';
    case State::CHECKING: return 'C';
    case State::ERROR:    return 'E';
  }
  return 'I';
}

void Controller::enter_(State s, uint32_t now) {
  prev_ = state_;
  state_ = s;
  if (s == State::CHECKING) {
    phaseEndMs_ = now + cfg_.settleMs;
  }
}

void Controller::manualAirUp(bool active) {
  manualActive_ = active;
  lastManualRefreshMs_ = millis();
  if (!act_) return;
  if (active) {
    act_->setCompressor(true);
    state_ = State::AIRUP;
  } else {
    stopOutputs_();
    state_ = State::IDLE;
  }
}

void Controller::manualVent(bool active) {
  manualActive_ = active;
  lastManualRefreshMs_ = millis();
  if (!act_) return;
  if (active) {
    act_->setVent(true);
    state_ = State::VENTING;
  } else {
    stopOutputs_();
    state_ = State::IDLE;
  }
}

void Controller::cancel() {
  manualActive_ = false;
  inContinuous_ = false;
  stopOutputs_();
  targetPsi_ = 0;
  if (state_ != State::ERROR) state_ = State::IDLE;
}

void Controller::clearError() {
  if (state_ == State::ERROR) {
    errorCode_ = ErrorCode::NONE;
    state_ = State::IDLE;
  }
}

void Controller::startSeek(float t) {
  if (t < cfg_.minPsi) t = cfg_.minPsi;
  if (t > cfg_.maxPsi) t = cfg_.maxPsi;
  targetPsi_ = t;
  manualActive_ = false;
  inContinuous_ = false;
  upRate_ = downRate_ = 0;
  upSamples_ = downSamples_ = 0;
  noChangeBurstCount_ = 0;

  stopOutputs_();
  float diff = targetPsi_ - currentPsi_;
  if (fabsf(diff) <= cfg_.psiTol) {
    state_ = State::IDLE;
    return;
  }
  scheduleBurst_(diff > 0 ? State::AIRUP : State::VENTING, cfg_.burstMsInit, millis());
}

void Controller::scheduleBurst_(State dir, unsigned long durMs, uint32_t now) {
  phaseStartPsi_ = currentPsi_;
  phaseStartMs_ = now;
  phaseEndMs_ = now + durMs;
  inContinuous_ = false;
  if (!act_) return;
  if (dir == State::AIRUP) {
    act_->setCompressor(true);
    state_ = State::AIRUP;
  } else {
    act_->setVent(true);
    state_ = State::VENTING;
  }
}

void Controller::enterError_(ErrorCode ec, const char* /*why*/) {
  errorCode_ = ec;
  stopOutputs_();
  manualActive_ = false;
  inContinuous_ = false;
  state_ = State::ERROR;
}

void Controller::update(uint32_t now, float currentPsi) {
  currentPsi_ = currentPsi;

  // Manual watchdog
  if (manualActive_) {
    if (now - lastManualRefreshMs_ > cfg_.manualRefreshTimeoutMs) {
      manualActive_ = false;
      stopOutputs_();
      state_ = State::IDLE;
    }
  }

  if (state_ == State::ERROR || manualActive_) {
    return;
  }

  switch (state_) {
    case State::AIRUP:
    case State::VENTING: {
      float remaining = targetPsi_ - currentPsi_;
      if (fabsf(remaining) <= cfg_.psiTol) {
        stopOutputs_();
        enter_(State::CHECKING, now);
        lastBurstEndMs_ = now;
        break;
      }
      // End burst / continuous phases
      if (inContinuous_) {
        if (now >= phaseEndMs_) {
          stopOutputs_();
          enter_(State::CHECKING, now);
          lastBurstEndMs_ = now;
        }
      } else {
        if (now >= phaseEndMs_) {
          stopOutputs_();
          enter_(State::CHECKING, now);
          lastBurstEndMs_ = now;
        }
      }
      break;
    }
    case State::CHECKING: {
      if (now >= phaseEndMs_) {
        float dt = (lastBurstEndMs_ > phaseStartMs_)
                   ? (lastBurstEndMs_ - phaseStartMs_) / 1000.0f
                   : (now - phaseStartMs_) / 1000.0f;
        float dPsi = currentPsi_ - phaseStartPsi_;
        if (dt > 0.02f) {
          if (dPsi > 0.01f) {
            upRate_ = (upRate_ * upSamples_ + (fabsf(dPsi) / dt)) / (upSamples_ + 1);
            upSamples_++;
          } else if (dPsi < -0.01f) {
            downRate_ = (downRate_ * downSamples_ + (fabsf(dPsi) / dt)) / (downSamples_ + 1);
            downSamples_++;
          }
          if (!inContinuous_) {
            if (fabsf(dPsi) < cfg_.noChangeEps) {
              noChangeBurstCount_++;
              if (noChangeBurstCount_ >= cfg_.maxNoChangeBursts) {
                enterError_(ErrorCode::NO_CHANGE, "No change");
                return;
              }
            } else {
              noChangeBurstCount_ = 0;
            }
          } else {
            noChangeBurstCount_ = 0;
          }
        }

        float remaining = targetPsi_ - currentPsi_;
        if (fabsf(remaining) <= cfg_.psiTol) {
          state_ = State::IDLE;
          stopOutputs_();
        } else {
          bool needUp = remaining > 0;
            bool haveRate = needUp ? (upSamples_ >= 2 && upRate_ > 0.001f)
                                   : (downSamples_ >= 2 && downRate_ > 0.001f);
          if (haveRate) {
            float rate = needUp ? upRate_ : downRate_;
            unsigned long predictedFullMs =
              (unsigned long)(1000.0f * (fabsf(remaining) / rate));
            if (predictedFullMs > cfg_.maxContinuousMs) {
              enterError_(ErrorCode::EXCESSIVE_TIME, "Too long");
              return;
            }
            float margin = 0.2f;
            float aim = fmaxf(0.0f, fabsf(remaining) - margin);
            unsigned long runMs = (unsigned long)(1000.0f * (aim / rate));
            if (runMs < cfg_.runMinMs) runMs = cfg_.runMinMs;
            if (runMs > cfg_.runMaxMs) runMs = cfg_.runMaxMs;
            // schedule continuous
            inContinuous_ = true;
            phaseStartPsi_ = currentPsi_;
            phaseStartMs_ = now;
            phaseEndMs_ = now + runMs;
            if (needUp) { state_ = State::AIRUP; act_->setCompressor(true); }
            else        { state_ = State::VENTING; act_->setVent(true); }
          } else {
            scheduleBurst_(needUp ? State::AIRUP : State::VENTING, cfg_.burstMsInit, now);
          }
        }
      }
      break;
    }
    case State::IDLE:
    default:
      stopOutputs_();
      break;
  }
}