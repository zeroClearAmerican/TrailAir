#pragma once
#include <stdint.h>

namespace trailair {
namespace config {

/**
 * @brief Board UI behavior.
 *
 * Pressure limits are not here: they live in controller::ControllerConfig (the controller
 * enforces them), and the UI reads them from there so the two can't disagree.
 */
struct UserInterfaceConfiguration {
  /// Target pressure on startup
  float defaultTargetPressurePSI = 32.0f;

  /// Target change per Up/Down click
  float pressureStepPSI = 1.0f;

  /// How long "Done!" shows after a seek completes
  uint32_t doneHoldDurationMilliseconds = 1500;

  /// Auto-clear the error screen after this long (0 = only on Right click)
  uint32_t errorAutoClearDurationMilliseconds = 4000;
};

/**
 * @brief Remote <-> board link timing, shared by both devices.
 */
struct CommunicationConfiguration {
  /// Remote: keep-alive ping interval while connected
  uint32_t keepAliveIntervalMilliseconds = 2000;

  /// Remote: no status for this long = disconnected (> 2x status/keep-alive cadence)
  uint32_t connectionTimeoutMilliseconds = 5000;

  /// Board: no frame from the remote for this long = remote inactive (stop sending status)
  uint32_t remoteActiveTimeoutMilliseconds = 5000;

  /// Remote: re-send ButtonPress this often while Up/Down is held. Must be well under
  /// controller::ControllerConfig::manualLeaseMilliseconds or manual mode stutters.
  uint32_t manualRepeatIntervalMilliseconds = 300;

  /// Remote: reconnect ping backoff (doubles from start up to max)
  uint32_t pingBackoffStartMilliseconds = 200;
  uint32_t pingBackoffMaximumMilliseconds = 2000;

  /// Remote: pairing request broadcast interval and overall timeout
  uint32_t pairingRequestIntervalMilliseconds = 500;
  uint32_t pairingTimeoutMilliseconds = 10000;

  /// Remote: after a Busy reply, keep asking this long for a free board before giving up
  uint32_t pairingBusyGraceMilliseconds = 2000;

  /// Board: how long the "Pairing" screen stays up after forgetting the remote
  uint32_t boardPairingWindowMilliseconds = 30000;
};

}  // namespace config
}  // namespace trailair
