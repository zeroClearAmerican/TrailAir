#pragma once
#include <Arduino.h>

namespace ta {
    namespace protocol {

        // Two-byte payload size (all messages are 2 bytes)
        static constexpr int kPayloadLen = 2;

        // Status codes sent from Control Board -> Remote (first byte)
        enum class Status : uint8_t {
            Idle     = 'I',
            AirUp    = 'U',
            Venting  = 'V',
            Checking = 'C',
            Error    = 'E'
        };

        // Commands sent from Remote -> Control Board (first byte)
        enum class Cmd : uint8_t {
            Start  = 'S',
            Idle   = 'I',  // cancel/idle
            Manual = 'M',
            Ping   = 'P'
        };

        // Pairing opcodes (share the 2-byte frame space, distinct from Status/Cmd letters)
        enum class PairOp : uint8_t {
            Req  = 'R',  // Remote -> broadcast
            Ack  = 'A',  // Board  -> Remote (unicast)
            // Cfm  = 'C',  // Remote -> Board (optional)
            Busy = 'B'   // Board  -> Remote (board already paired)
        };

        // 0.5 PSI resolution helpers
        inline uint8_t psiToByte05(float psi) {
            if (psi < 0) psi = 0;
            if (psi > 127.5f) psi = 127.5f;
            return static_cast<uint8_t>(lroundf(psi * 2.0f));
        }
        inline float byteToPsi05(uint8_t b) {
            return static_cast<float>(b) * 0.5f;
        }

        // Inbound (from control board) parsed status
        struct StatusMsg {
            Status status;
            uint8_t value; // PSI in 0.5 units for non-Error, or error code if status==Error
        };

        // Parsed pairing message
        struct PairMsg {
            PairOp op;
            uint8_t value; // groupId or reason
        };

        // Serialize outbound commands (always 2 bytes)
        inline void packStart(uint8_t out[kPayloadLen], float targetPsi) {
            out[0] = static_cast<uint8_t>(Cmd::Start);
            out[1] = psiToByte05(targetPsi);
        }
        inline void packCancel(uint8_t out[kPayloadLen]) {
            out[0] = static_cast<uint8_t>(Cmd::Idle);
            out[1] = 0;
        }
        inline void packManual(uint8_t out[kPayloadLen], uint8_t code) {
            out[0] = static_cast<uint8_t>(Cmd::Manual);
            out[1] = code; // 0x00 = vent, 0xFF = air
        }
        inline void packPing(uint8_t out[kPayloadLen]) {
            out[0] = static_cast<uint8_t>(Cmd::Ping);
            out[1] = 0;
        }

        // Parse inbound status (2 bytes) into StatusMsg
        inline bool parseStatus(const uint8_t* data, int len, StatusMsg& out) {
            if (len != kPayloadLen) return false;
            uint8_t s = data[0];
            switch (s) {
                case 'I': out.status = Status::Idle; break;
                case 'U': out.status = Status::AirUp; break;
                case 'V': out.status = Status::Venting; break;
                case 'C': out.status = Status::Checking; break;
                case 'E': out.status = Status::Error; break;
                default: return false;
            }
            out.value = data[1];
            return true;
        }

        // Compile-time group identifier to avoid cross-garage collisions.
        // Later: move into a shared config header or build flag.
        static constexpr uint8_t kPairGroupId = 0x01;

        // Pack pairing frames
        inline void packPairReq (uint8_t out[kPayloadLen], uint8_t groupId = kPairGroupId) { out[0] = (uint8_t)PairOp::Req;  out[1] = groupId; }
        inline void packPairAck (uint8_t out[kPayloadLen], uint8_t groupId = kPairGroupId) { out[0] = (uint8_t)PairOp::Ack;  out[1] = groupId; }
        // inline void packPairCfm (uint8_t out[kPayloadLen], uint8_t groupId = kPairGroupId) { out[0] = (uint8_t)PairOp::Cfm;  out[1] = groupId; }
        inline void packPairBusy(uint8_t out[kPayloadLen], uint8_t reason = 1)            { out[0] = (uint8_t)PairOp::Busy; out[1] = reason;  }

        // Quick classifier: returns true if first byte is a pairing opcode
        inline bool isPairingFrame(const uint8_t* data, int len) {
            if (len != kPayloadLen) return false;
            switch (data[0]) {
                case 'R': // Req
                case 'A': // Ack
                case 'B': // Busy
                    return true;
                default:
                    return false;
            }
        }

        inline bool parsePair(const uint8_t* data, int len, PairMsg& out) {
            if (len != kPayloadLen) return false;
            switch (data[0]) {
                case 'R': out.op = PairOp::Req;  break;
                case 'A': out.op = PairOp::Ack;  break;
                case 'B': out.op = PairOp::Busy; break;
                default: return false;
            }
            out.value = data[1];
            return true;
        }

    } // namespace protocol
} // namespace ta