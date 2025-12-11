#pragma once
#include <Arduino.h>
#include "TA_Controller.h"
#include "TA_CommsBoard.h"
#include "TA_Input.h"
#include <TA_Display.h>
#include <TA_UI.h>
#include <TA_Config.h>

namespace trailair { namespace stateboard {

    class StateBoard {
    public:
      struct Config {
        trailair::config::UserInterfaceConfiguration ui;      // shared UI config
        trailair::config::CommunicationConfiguration link;  // shared link config (timeouts, pairing)
        float stepPsiLarge = 5.0f;       // optional extra step not used by shared UI
      };

      enum class UiState { Idle, Manual, Seeking, Error };

      // Overloads instead of default arg (avoids compiler issue)
      void begin();                // uses internal default Config()
      void begin(const Config& cfg);

      // Button events forwarded from sketch (same ordering as remote: Left, Down, Up, Right).
      void onButton(const trailair::input::ButtonEvent& ev, trailair::controller::PressureController& controller);

      // Called each loop.
      void update(uint32_t now,
                  trailair::controller::PressureController& controller,
                  const trailair::comms::BoardLink& link);

      // Fill display model (reusing existing rendering pipeline).
      void buildDisplayModel(trailair::display::DisplayModel& m,
                             const trailair::controller::PressureController& controller,
                             const trailair::comms::BoardLink& link,
                             uint32_t now) const;

      float targetPsi() const { return ui_.getTargetPSI(); }
      UiState uiState() const {
        using V = trailair::ui::ViewState;
        switch (ui_.getViewState()) {
          case V::Idle: return UiState::Idle;
          case V::Manual: return UiState::Manual;
          case V::Seeking: return UiState::Seeking;
          case V::Error: return UiState::Error;
          default: return UiState::Idle;
        }
      }

    private:
      // Bridge concrete controller to shared UI actions
      struct BoardActions : trailair::ui::DeviceActions {
        trailair::controller::PressureController* ctl = nullptr;
        bool isConnected() const override { return true; }
        void cancel() override { if (ctl) ctl->cancel(); }
        void clearError() override { if (ctl) ctl->clearError(); }
        void startSeek(float targetPsi) override { if (ctl) ctl->startSeek(targetPsi); }
        void manualVent(bool on) override { if (ctl) ctl->manualVent(on); }
        void manualAirUp(bool on) override { if (ctl) ctl->manualAirUp(on); }
      };

      Config cfg_{};
      trailair::ui::UserInterfaceStateMachine ui_{};

      // helper conversions
      static trailair::ui::ControllerState toUiCtrl_(trailair::controller::ControllerState s);
      static trailair::ui::ButtonEvent toUiBtn_(const trailair::input::ButtonEvent& ev);
    };

  } // namespace stateboard
} // namespace ta
