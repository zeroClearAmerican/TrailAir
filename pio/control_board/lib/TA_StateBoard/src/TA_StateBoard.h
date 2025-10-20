#pragma once
#include <Arduino.h>
#include "TA_Controller.h"
#include "TA_CommsBoard.h"
#include "TA_Input.h"
#include "TA_Display.h"
#include <TA_UI.h>
#include <TA_Config.h>

namespace ta {
  namespace stateboard {

    class StateBoard {
    public:
      struct Config {
        ta::cfg::UiShared ui;      // shared UI config
        ta::cfg::LinkShared link;  // shared link config (timeouts, pairing)
        float stepPsiLarge = 5.0f;       // optional extra step not used by shared UI
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

      float targetPsi() const { return ui_.targetPsi(); }
      UiState uiState() const {
        using V = ta::ui::View;
        switch (ui_.view()) {
          case V::Idle: return UiState::Idle;
          case V::Manual: return UiState::Manual;
          case V::Seeking: return UiState::Seeking;
          case V::Error: return UiState::Error;
          default: return UiState::Idle;
        }
      }

    private:
      // Bridge concrete controller to shared UI actions
      struct BoardActions : ta::ui::DeviceActions {
        ta::ctl::Controller* ctl = nullptr;
        bool isConnected() const override { return true; }
        void cancel() override { if (ctl) ctl->cancel(); }
        void clearError() override { if (ctl) ctl->clearError(); }
        void startSeek(float targetPsi) override { if (ctl) ctl->startSeek(targetPsi); }
        void manualVent(bool on) override { if (ctl) ctl->manualVent(on); }
        void manualAirUp(bool on) override { if (ctl) ctl->manualAirUp(on); }
      };

      Config cfg_{};
      ta::ui::UiStateMachine ui_{};

      // helper conversions
      static ta::ui::Ctrl toUiCtrl_(ta::ctl::State s);
      static ta::ui::ButtonEvent toUiBtn_(const ta::input::Event& ev);
    };

  } // namespace stateboard
} // namespace ta