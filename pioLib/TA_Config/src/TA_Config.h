#pragma once
#include <stdint.h>

namespace ta { namespace cfg {

// Shared UI configuration used by both control board and remote
struct UiShared {
  float minPsi = 0.0f;
  float maxPsi = 50.0f;
  float defaultTargetPsi = 32.0f;
  float stepSmall = 1.0f;          // PSI increment/decrement per click
  uint32_t doneHoldMs = 1500;      // "Done" hold duration after seeking
  uint32_t errorAutoClearMs = 4000;// auto-exit Error after this window (0=disabled)
};

// Shared link configuration (timeouts, backoffs, pairing)
struct LinkShared {
  uint32_t remoteActiveTimeoutMs = 3000; // board: consider remote active if seen within this
  uint32_t connectionTimeoutMs = 5000;   // remote: lose connection after this
  // Manual resend cadence (remote manual streaming)
  uint32_t manualRepeatMs = 300;
  // Reconnect/ping backoff (remote)
  uint32_t pingBackoffStartMs = 200;
  uint32_t pingBackoffMaxMs = 2000;
  // Pairing
  uint8_t pairGroupId = 0x01;     // default group id
  uint32_t pairReqIntervalMs = 500;
  uint32_t pairTimeoutMs = 30000;
};

}} // namespace ta::cfg
