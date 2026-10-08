#pragma once
#include <stdint.h>
#include <math.h>
#include <TA_Types.h>

namespace trailair {
namespace protocol {

/**
 * Wire format v2 (remote <-> control board over ESP-NOW).
 *
 * Every frame is [MAGIC, type, payload...] with a fixed length per type:
 *
 *   Remote -> board   ButtonPress/Release/Click/LongHold  [M, 'D'|'U'|'C'|'L', buttonId]           3
 *                     Ping (keep-alive)                  [M, 'P']                                 2
 *                     PairRequest (broadcast)            [M, 'R']                                 2
 *   Board -> remote   Status                             [M, 'S', state, view, psi, target, err]  7
 *                     PairAck / PairBusy                 [M, 'A'|'B']                             2
 *
 * state/view are the wire characters of ControllerState/View. PSI bytes are 0.5 PSI units
 * (0..127.5). err is a trailair::errors::ErrorCode (0 = none).
 *
 * While a manual button is held, the remote re-sends ButtonPress every ~300 ms; the board
 * treats each as a renewal of its manual-mode lease (see PressureController).
 */

/// Leading byte of every frame. Change it on incompatible format changes so old firmware
/// and unrelated ESP-NOW traffic are rejected instead of misread.
constexpr uint8_t MAGIC = 0xA2;
constexpr int MAX_FRAME_LENGTH = 7;

enum class FrameType : uint8_t {
  ButtonPress    = 'D',
  ButtonRelease  = 'U',
  ButtonClick    = 'C',
  ButtonLongHold = 'L',
  Ping           = 'P',
  Status         = 'S',
  PairRequest    = 'R',
  PairAck        = 'A',
  PairBusy       = 'B'
};

struct Status {
  ControllerState state = ControllerState::Idle;
  View view = View::Idle;
  float currentPSI = 0.0f;
  float targetPSI = 0.0f;
  uint8_t errorCode = 0;
};

struct Frame {
  FrameType type = FrameType::Ping;
  ButtonId button = ButtonId::Left;  ///< Button frames only
  Status status;                     ///< Status frames only
};

/// PSI -> wire byte (0.5 PSI resolution, clamped to 0..127.5)
inline uint8_t psiToByte(float psi) {
  if (psi < 0.0f) psi = 0.0f;
  if (psi > 127.5f) psi = 127.5f;
  return static_cast<uint8_t>(lroundf(psi * 2.0f));
}

inline float byteToPsi(uint8_t b) {
  return static_cast<float>(b) * 0.5f;
}

/// Byte length of a frame of this type, or 0 if the type is unknown
inline int frameLength(FrameType t) {
  switch (t) {
    case FrameType::ButtonPress:
    case FrameType::ButtonRelease:
    case FrameType::ButtonClick:
    case FrameType::ButtonLongHold: return 3;
    case FrameType::Status:         return 7;
    case FrameType::Ping:
    case FrameType::PairRequest:
    case FrameType::PairAck:
    case FrameType::PairBusy:       return 2;
    default:                        return 0;
  }
}

inline FrameType buttonFrameType(ButtonAction a) {
  switch (a) {
    case ButtonAction::Pressed:  return FrameType::ButtonPress;
    case ButtonAction::Released: return FrameType::ButtonRelease;
    case ButtonAction::LongHold: return FrameType::ButtonLongHold;
    case ButtonAction::Click:
    default:                     return FrameType::ButtonClick;
  }
}

/// Button frame -> action. Returns false for non-button frames.
inline bool buttonAction(FrameType t, ButtonAction& out) {
  switch (t) {
    case FrameType::ButtonPress:    out = ButtonAction::Pressed;  return true;
    case FrameType::ButtonRelease:  out = ButtonAction::Released; return true;
    case FrameType::ButtonClick:    out = ButtonAction::Click;    return true;
    case FrameType::ButtonLongHold: out = ButtonAction::LongHold; return true;
    default:                        return false;
  }
}

/// Serialize into out (at least MAX_FRAME_LENGTH bytes). Returns the frame length.
inline int pack(uint8_t* out, const Frame& f) {
  out[0] = MAGIC;
  out[1] = static_cast<uint8_t>(f.type);
  switch (f.type) {
    case FrameType::ButtonPress:
    case FrameType::ButtonRelease:
    case FrameType::ButtonClick:
    case FrameType::ButtonLongHold:
      out[2] = static_cast<uint8_t>(f.button);
      break;
    case FrameType::Status:
      out[2] = static_cast<uint8_t>(f.status.state);
      out[3] = static_cast<uint8_t>(f.status.view);
      out[4] = psiToByte(f.status.currentPSI);
      out[5] = psiToByte(f.status.targetPSI);
      out[6] = f.status.errorCode;
      break;
    default:
      break;
  }
  return frameLength(f.type);
}

namespace detail {
  inline bool isControllerState(uint8_t c) {
    return c == 'I' || c == 'U' || c == 'V' || c == 'C' || c == 'E';
  }
  /// Views the board may send (Disconnected/Pairing are remote-local)
  inline bool isBoardView(uint8_t c) {
    return c == 'I' || c == 'M' || c == 'S' || c == 'D' || c == 'E';
  }
}

/// Parse and validate a received frame. Rejects bad magic, unknown types, wrong lengths and
/// out-of-range fields, so callers can trust every field of out.
inline bool parse(const uint8_t* data, int length, Frame& out) {
  if (length < 2 || data[0] != MAGIC) return false;
  FrameType t = static_cast<FrameType>(data[1]);
  int expected = frameLength(t);
  if (expected == 0 || length != expected) return false;

  out.type = t;
  switch (t) {
    case FrameType::ButtonPress:
    case FrameType::ButtonRelease:
    case FrameType::ButtonClick:
    case FrameType::ButtonLongHold:
      if (data[2] >= BUTTON_COUNT) return false;
      out.button = static_cast<ButtonId>(data[2]);
      break;
    case FrameType::Status:
      if (!detail::isControllerState(data[2]) || !detail::isBoardView(data[3])) return false;
      out.status.state      = static_cast<ControllerState>(data[2]);
      out.status.view       = static_cast<View>(data[3]);
      out.status.currentPSI = byteToPsi(data[4]);
      out.status.targetPSI  = byteToPsi(data[5]);
      out.status.errorCode  = data[6];
      break;
    default:
      break;
  }
  return true;
}

} // namespace protocol
} // namespace trailair
