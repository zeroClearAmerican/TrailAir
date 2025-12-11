#pragma once
#include <stdint.h>
#include <math.h>

namespace trailair {
namespace protocol {

/**
 * @brief Communication protocol for TrailAir remote <-> control board
 * 
 * All messages are exactly 2 bytes for simplicity and reliability.
 * Uses ASCII characters for opcodes to aid debugging.
 */

/// @brief Standard payload size for all protocol messages (2 bytes)
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
  Start  = 'S',  ///< Start seeking to target pressure
  Idle   = 'I',  ///< Cancel operation / return to idle
  Manual = 'M',  ///< Manual control mode
  Ping   = 'P'   ///< Keep-alive ping
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
    Idle,    ///< Cancel / return to idle
    Start,   ///< Start seeking to target
    Manual,  ///< Manual control mode
    Ping     ///< Keep-alive ping
  } kind = Kind::Idle;

  float targetPSI = 0.0f;              ///< Target pressure (used when kind==Start)
  ManualMode manualMode = ManualMode::Vent;  ///< Manual mode (used when kind==Manual)
};

/**
 * @brief Response message from Control Board to Remote
 */
struct Response {
  StatusCode status = StatusCode::Idle;  ///< Current controller status
  uint8_t value = 0;  ///< PSI (0.5 units) for non-Error; error code if status==Error
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
      output[1] = 0;
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
      break;
    default:
      return false;
  }
  return true;
}

/**
 * @brief Parse 2-byte payload into Response
 * @param data Input buffer
 * @param length Buffer length (must be PAYLOAD_LENGTH)
 * @param output Parsed response output
 * @return true if parse successful, false otherwise
 */
inline bool parseResponse(const uint8_t* data, int length, Response& output) {
  if (length != PAYLOAD_LENGTH) return false;
  switch (data[0]) {
    case 'I': output.status = StatusCode::Idle; break;
    case 'U': output.status = StatusCode::AirUp; break;
    case 'V': output.status = StatusCode::Venting; break;
    case 'C': output.status = StatusCode::Checking; break;
    case 'E': output.status = StatusCode::Error; break;
    default: return false;
  }
  output.value = data[1];
  return true;
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