#pragma once
#include <stdint.h>
#include <math.h>

namespace trailair {
namespace protocol {

/**
 * @brief Communication protocol for TrailAir remote <-> control board
 * 
 * Request messages (remote → board): 2 bytes
 * Response messages (board → remote): 4 bytes (includes UI state and target PSI)
 * Uses ASCII characters for opcodes to aid debugging.
 */

/// @brief Standard request payload size (2 bytes)
static constexpr int REQUEST_LENGTH = 2;

/// @brief Standard response payload size (4 bytes - status, uiState, currentPSI, targetPSI) 
static constexpr int RESPONSE_LENGTH = 4;

/// @brief Legacy 2-byte payload constant (deprecated, use REQUEST_LENGTH or RESPONSE_LENGTH)
static constexpr int PAYLOAD_LENGTH = 2;

/**
 * @brief Status codes sent from Control Board to Remote
 * 
 * First byte of response messages indicating controller state.
 */
enum class StatusCode : uint8_t {
  Idle     = 'I',  ///< Controller idle
  AirUp    = 'U',  ///< Actively inflating (compressor on)
  Venting  = 'V',  ///< Actively deflating (vent open)
  Checking = 'C',  ///< Settling/checking pressure
  Error    = 'E'   ///< Error state
};

/**
 * @brief Command codes sent from Remote to Control Board
 * 
 * First byte of request messages.
 */
enum class CommandCode : uint8_t {
  // Button-based commands (new thin client protocol)
  ButtonPress   = 'D',  ///< Button pressed (Down)
  ButtonRelease = 'U',  ///< Button released (Up)
  ButtonClick   = 'C',  ///< Button clicked
  ButtonLongHold = 'L', ///< Button long-held
  
  // Legacy high-level commands (deprecated, for backward compatibility)
  Start  = 'S',  ///< Start seeking to target pressure
  Idle   = 'I',  ///< Cancel operation / return to idle
  Manual = 'M',  ///< Manual control mode
  Ping   = 'P'   ///< Keep-alive ping
};

/**
 * @brief Button identifier for button-based protocol
 * 
 * Maps to trailair::input::ButtonId on both remote and board.
 */
enum class ButtonId : uint8_t {
  Left  = 0,  ///< Left button (cancel/exit/manual)
  Down  = 1,  ///< Down button (decrease/vent)
  Up    = 2,  ///< Up button (increase/air)
  Right = 3   ///< Right button (start/confirm)
};

/**
 * @brief Pairing operation codes
 * 
 * Used during device pairing handshake.
 */
enum class PairingOperation : uint8_t {
  Request  = 'R',  ///< Remote broadcasts pairing request
  Acknowledge = 'A',  ///< Board acknowledges pairing (unicast)
  Busy = 'B'   ///< Board already paired, cannot accept
};

/**
 * @brief Manual control mode selector
 */
enum class ManualMode : uint8_t {
  Vent = 0x00,  ///< Manual venting (deflate)
  Air  = 0xFF   ///< Manual air up (inflate)
};

/**
 * @brief UI view state transmitted from board to remote
 * 
 * Indicates which screen/mode the control board is in.
 * Remote displays this state to stay in sync with board.
 */
enum class UIState : uint8_t {
  Idle    = 'I',  ///< Idle screen showing current/target PSI
  Manual  = 'M',  ///< Manual control mode
  Seeking = 'S',  ///< Seeking to target PSI
  Error   = 'E'   ///< Error screen
};

/**
 * @brief Convert pressure in PSI to protocol byte (0.5 PSI resolution)
 * @param psi Pressure value (clamped to 0-127.5 PSI)
 * @return Byte representing pressure * 2
 */
inline uint8_t convertPSIToByte(float psi) {
  if (psi < 0.0f) psi = 0.0f;
  if (psi > 127.5f) psi = 127.5f;
  return static_cast<uint8_t>(lroundf(psi * 2.0f));
}

/**
 * @brief Convert protocol byte to pressure in PSI (0.5 PSI resolution)
 * @param byte Protocol byte value
 * @return Pressure in PSI
 */
inline float convertByteToPSI(uint8_t byte) {
  return static_cast<float>(byte) * 0.5f;
}

/**
 * @brief Request message from Remote to Control Board
 */
struct Request {
  /**
   * @brief Type of request being made
   */
  enum class Kind {
    // Button-based commands (new thin client protocol)
    ButtonPress,    ///< Button pressed down
    ButtonRelease,  ///< Button released
    ButtonClick,    ///< Button clicked (short tap)
    ButtonLongHold, ///< Button long-held
    
    // Legacy high-level commands (deprecated)
    Idle,    ///< Cancel / return to idle
    Start,   ///< Start seeking to target
    Manual,  ///< Manual control mode
    
    Ping     ///< Keep-alive ping
  } kind = Kind::Ping;

  // Button-based protocol fields
  ButtonId button = ButtonId::Left;  ///< Which button (for button commands)
  
  // Legacy fields (deprecated, for backward compatibility)
  float targetPSI = 0.0f;              ///< Target pressure (used when kind==Start or Ping)
  ManualMode manualMode = ManualMode::Vent;  ///< Manual mode (used when kind==Manual)
};

/**
 * @brief Response message from Control Board to Remote (4 bytes)
 */
struct Response {
  StatusCode status = StatusCode::Idle;  ///< Current controller status
  UIState uiState = UIState::Idle;       ///< Current UI view state on board
  uint8_t value = 0;  ///< Current PSI (0.5 units) for non-Error; error code if status==Error
  uint8_t targetPSI = 0;  ///< Target PSI (0.5 units) - allows remote to sync target
};

/**
 * @brief Pairing message structure
 */
struct PairingMessage {
  PairingOperation operation;  ///< Pairing operation type
  uint8_t value;               ///< Group ID or reason code
};

/**
 * @brief Serialize request to 2-byte payload
 * @param output Output buffer (must be at least PAYLOAD_LENGTH bytes)
 * @param request Request to serialize
 */
inline void packRequest(uint8_t output[PAYLOAD_LENGTH], const Request& request) {
  switch (request.kind) {
    // Button-based commands (new protocol)
    case Request::Kind::ButtonPress:
      output[0] = static_cast<uint8_t>(CommandCode::ButtonPress);
      output[1] = static_cast<uint8_t>(request.button);
      break;
    case Request::Kind::ButtonRelease:
      output[0] = static_cast<uint8_t>(CommandCode::ButtonRelease);
      output[1] = static_cast<uint8_t>(request.button);
      break;
    case Request::Kind::ButtonClick:
      output[0] = static_cast<uint8_t>(CommandCode::ButtonClick);
      output[1] = static_cast<uint8_t>(request.button);
      break;
    case Request::Kind::ButtonLongHold:
      output[0] = static_cast<uint8_t>(CommandCode::ButtonLongHold);
      output[1] = static_cast<uint8_t>(request.button);
      break;
    
    // Legacy commands (backward compatibility)
    case Request::Kind::Idle:
      output[0] = static_cast<uint8_t>(CommandCode::Idle);
      output[1] = 0;
      break;
    case Request::Kind::Start:
      output[0] = static_cast<uint8_t>(CommandCode::Start);
      output[1] = convertPSIToByte(request.targetPSI);
      break;
    case Request::Kind::Manual:
      output[0] = static_cast<uint8_t>(CommandCode::Manual);
      output[1] = static_cast<uint8_t>(request.manualMode);
      break;
    case Request::Kind::Ping:
      output[0] = static_cast<uint8_t>(CommandCode::Ping);
      output[1] = convertPSIToByte(request.targetPSI);  // Include target PSI in pings
      break;
  }
}

/**
 * @brief Parse 2-byte payload into Request
 * @param data Input buffer
 * @param length Buffer length (must be PAYLOAD_LENGTH)
 * @param output Parsed request output
 * @return true if parse successful, false otherwise
 */
inline bool parseRequest(const uint8_t* data, int length, Request& output) {
  if (length != PAYLOAD_LENGTH) return false;
  switch (static_cast<CommandCode>(data[0])) {
    // Button-based commands (new protocol)
    case CommandCode::ButtonPress:
      output.kind = Request::Kind::ButtonPress;
      output.button = static_cast<ButtonId>(data[1]);
      break;
    case CommandCode::ButtonRelease:
      output.kind = Request::Kind::ButtonRelease;
      output.button = static_cast<ButtonId>(data[1]);
      break;
    case CommandCode::ButtonClick:
      output.kind = Request::Kind::ButtonClick;
      output.button = static_cast<ButtonId>(data[1]);
      break;
    case CommandCode::ButtonLongHold:
      output.kind = Request::Kind::ButtonLongHold;
      output.button = static_cast<ButtonId>(data[1]);
      break;
    
    // Legacy commands (backward compatibility)
    case CommandCode::Idle:
      output.kind = Request::Kind::Idle;
      output.targetPSI = 0.0f;
      break;
    case CommandCode::Start:
      output.kind = Request::Kind::Start;
      output.targetPSI = convertByteToPSI(data[1]);
      break;
    case CommandCode::Manual:
      output.kind = Request::Kind::Manual;
      output.manualMode = static_cast<ManualMode>(data[1]);
      break;
    case CommandCode::Ping:
      output.kind = Request::Kind::Ping;
      output.targetPSI = convertByteToPSI(data[1]);  // Extract target PSI from ping
      break;
    default:
      return false;
  }
  return true;
}

/**
 * @brief Parse response payload into Response struct
 * @param data Input buffer
 * @param length Buffer length
 * @param output Parsed response output
 * @return true if parse successful, false otherwise
 * 
 * Supports multiple format versions for backward compatibility:
 * - 2 bytes (legacy): status + value
 * - 3 bytes: status + uiState + value
 * - 4 bytes (current): status + uiState + currentPSI + targetPSI
 */
inline bool parseResponse(const uint8_t* data, int length, Response& output) {
  // Legacy 2-byte format: status + value (no UI state)
  if (length == PAYLOAD_LENGTH) {
    switch (data[0]) {
      case 'I': output.status = StatusCode::Idle; output.uiState = UIState::Idle; break;
      case 'U': output.status = StatusCode::AirUp; break;
      case 'V': output.status = StatusCode::Venting; break;
      case 'C': output.status = StatusCode::Checking; break;
      case 'E': output.status = StatusCode::Error; output.uiState = UIState::Error; break;
      default: return false;
    }
    output.value = data[1];
    output.targetPSI = 0;
    // Infer UI state from controller status for old format
    if (output.status != StatusCode::Error && output.status != StatusCode::Idle) {
      if (output.uiState == UIState::Idle) output.uiState = UIState::Seeking;
    }
    return true;
  } 
  // 3-byte format: status + uiState + value (no target PSI)
  else if (length == 3) {
    switch (data[0]) {
      case 'I': output.status = StatusCode::Idle; break;
      case 'U': output.status = StatusCode::AirUp; break;
      case 'V': output.status = StatusCode::Venting; break;
      case 'C': output.status = StatusCode::Checking; break;
      case 'E': output.status = StatusCode::Error; break;
      default: return false;
    }
    switch (data[1]) {
      case 'I': output.uiState = UIState::Idle; break;
      case 'M': output.uiState = UIState::Manual; break;
      case 'S': output.uiState = UIState::Seeking; break;
      case 'E': output.uiState = UIState::Error; break;
      default: return false;
    }
    output.value = data[2];
    output.targetPSI = 0;
    return true;
  }
  // Current 4-byte format: status + uiState + currentPSI + targetPSI
  else if (length == RESPONSE_LENGTH) {
    switch (data[0]) {
      case 'I': output.status = StatusCode::Idle; break;
      case 'U': output.status = StatusCode::AirUp; break;
      case 'V': output.status = StatusCode::Venting; break;
      case 'C': output.status = StatusCode::Checking; break;
      case 'E': output.status = StatusCode::Error; break;
      default: return false;
    }
    switch (data[1]) {
      case 'I': output.uiState = UIState::Idle; break;
      case 'M': output.uiState = UIState::Manual; break;
      case 'S': output.uiState = UIState::Seeking; break;
      case 'E': output.uiState = UIState::Error; break;
      default: return false;
    }
    output.value = data[2];
    output.targetPSI = data[3];
    return true;
  }
  return false;
}

/**
 * @brief Pack pairing request message
 * @param output Output buffer (must be at least PAYLOAD_LENGTH bytes)
 * @param groupId Pairing group identifier
 */
inline void packPairingRequest(uint8_t output[PAYLOAD_LENGTH], uint8_t groupId) {
  output[0] = static_cast<uint8_t>(PairingOperation::Request);
  output[1] = groupId;
}

/**
 * @brief Pack pairing acknowledgement message
 * @param output Output buffer (must be at least PAYLOAD_LENGTH bytes)
 * @param groupId Pairing group identifier
 */
inline void packPairingAcknowledge(uint8_t output[PAYLOAD_LENGTH], uint8_t groupId) {
  output[0] = static_cast<uint8_t>(PairingOperation::Acknowledge);
  output[1] = groupId;
}

/**
 * @brief Pack pairing busy message
 * @param output Output buffer (must be at least PAYLOAD_LENGTH bytes)
 * @param reason Reason code (default 1)
 */
inline void packPairingBusy(uint8_t output[PAYLOAD_LENGTH], uint8_t reason = 1) {
  output[0] = static_cast<uint8_t>(PairingOperation::Busy);
  output[1] = reason;
}

/**
 * @brief Check if frame is a pairing message
 * @param data Input buffer
 * @param length Buffer length
 * @return true if frame contains pairing opcode
 */
inline bool isPairingFrame(const uint8_t* data, int length) {
  if (length != PAYLOAD_LENGTH) return false;
  switch (data[0]) {
    case 'R':  // Request
    case 'A':  // Acknowledge
    case 'B':  // Busy
      return true;
    default:
      return false;
  }
}

/**
 * @brief Parse pairing message
 * @param data Input buffer
 * @param length Buffer length (must be PAYLOAD_LENGTH)
 * @param output Parsed pairing message
 * @return true if parse successful, false otherwise
 */
inline bool parsePairingMessage(const uint8_t* data, int length, PairingMessage& output) {
  if (length != PAYLOAD_LENGTH) return false;
  switch (data[0]) {
    case 'R':
      output.operation = PairingOperation::Request;
      break;
    case 'A':
      output.operation = PairingOperation::Acknowledge;
      break;
    case 'B':
      output.operation = PairingOperation::Busy;
      break;
    default:
      return false;
  }
  output.value = data[1];
  return true;
}

} // namespace protocol
} // namespace trailair