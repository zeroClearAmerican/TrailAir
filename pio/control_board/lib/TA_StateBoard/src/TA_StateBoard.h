#pragma once
#include <Arduino.h>
#include "TA_Controller.h"
#include "TA_CommsBoard.h"
#include "TA_Input.h"
#include "TA_Display.h"

namespace ta {
  namespace stateboard {

    class StateBoard {
    public:
      struct Config {
        float minPsi = 0.0f;
        float maxPsi = 50.0f;
        float defaultTargetPsi = 32.0f;
        uint32_t doneHoldMs = 1500;
        uint32_t errorAutoClearMs = 4000;
        uint32_t remoteTimeoutMs = 3000;
        float stepPsiSmall = 1.0f;
        float stepPsiLarge = 5.0f;
      };

      enum class UiState { Idle, Manual, Seeking, Error };

      // Overloads instead of default arg (avoids compiler issue)
      void begin();                // uses internal default Config()
      void begin(const Config& cfg);

      // Button events forwarded from sketch (same ordering as remote: Left, Down, Up, Right).
      void onButton(const ta::input::Event& ev, ta::ctl::Controller& controller);

      // Called each loop.
      void update(uint32_t now,
                  ta::ctl::Controller& controller,
                  const ta::comms::BoardLink& link);

      // Fill display model (reusing existing rendering pipeline).
      void buildDisplayModel(ta::display::DisplayModel& m,
                             const ta::ctl::Controller& controller,
                             const ta::comms::BoardLink& link,
                             uint32_t now) const;

      float targetPsi() const { return targetPsi_; }
      UiState uiState() const { return uiState_; }

    private:
      Config cfg_;
      UiState uiState_ = UiState::Idle;

      float targetPsi_ = 32.0f;

      // Seeking / done-hold
      bool seenSeekingActivity_ = false;
      bool showDoneHold_ = false;
      uint32_t doneHoldUntil_ = 0;

      // Error auto-clear
      uint32_t errorEntryMs_ = 0;

      // Manual flags (while button held)
      bool manualVentActive_ = false;
      bool manualAirActive_ = false;

      void clampTarget();
      void enterManual(ta::ctl::Controller& controller);
      void exitManual(ta::ctl::Controller& controller);
      void startSeek(ta::ctl::Controller& controller);
      void cancelSeek(ta::ctl::Controller& controller);
      void handleControllerState(uint32_t now, ta::ctl::Controller& controller);
      void handleErrorAutoClear(uint32_t now, ta::ctl::Controller& controller);
    };

  } // namespace stateboard
} // namespace ta