#pragma once
#include <stdint.h>
#include <math.h>

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

        // Manual codes
        enum class ManualCode : uint8_t { Vent = 0x00, Air = 0xFF };

        // 0.5 PSI resolution helpers
        inline uint8_t psiToByte05(float psi) {
            if (psi < 0) psi = 0;
            if (psi > 127.5f) psi = 127.5f;
            return static_cast<uint8_t>(lroundf(psi * 2.0f));
        }
        inline float byteToPsi05(uint8_t b) { return static_cast<float>(b) * 0.5f; }

        // Unified typed messages
        struct Request {
            enum class Kind { Idle, Start, Manual, Ping } kind = Kind::Idle;
            float targetPsi = 0.0f;     // used when kind==Start
            ManualCode manual = ManualCode::Vent; // used when kind==Manual
        };

        struct Response {
            // Same payload as legacy StatusMsg
            Status status = Status::Idle;
            uint8_t value = 0; // PSI in 0.5 units for non-Error, or error code if status==Error
        };

        // Backward name (alias) to ease migration in code comments
        using StatusMsg = Response;

        // Serialize outbound requests (always 2 bytes)
        inline void packRequest(uint8_t out[kPayloadLen], const Request& r) {
            switch (r.kind) {
                case Request::Kind::Idle:
                    out[0] = static_cast<uint8_t>(Cmd::Idle); out[1] = 0; break;
                case Request::Kind::Start:
                    out[0] = static_cast<uint8_t>(Cmd::Start); out[1] = psiToByte05(r.targetPsi); break;
                case Request::Kind::Manual:
                    out[0] = static_cast<uint8_t>(Cmd::Manual); out[1] = static_cast<uint8_t>(r.manual); break;
                case Request::Kind::Ping:
                    out[0] = static_cast<uint8_t>(Cmd::Ping); out[1] = 0; break;
            }
        }
        inline bool parseRequest(const uint8_t* data, int len, Request& out) {
            if (len != kPayloadLen) return false;
            switch (static_cast<Cmd>(data[0])) {
                case Cmd::Idle:   out.kind = Request::Kind::Idle;   out.targetPsi = 0; break;
                case Cmd::Start:  out.kind = Request::Kind::Start;  out.targetPsi = byteToPsi05(data[1]); break;
                case Cmd::Manual: out.kind = Request::Kind::Manual; out.manual = static_cast<ManualCode>(data[1]); break;
                case Cmd::Ping:   out.kind = Request::Kind::Ping;   break;
                default: return false;
            }
            return true;
        }

        // Serialize outbound responses
        inline void packResponseStatus(uint8_t out[kPayloadLen], Status s, float psiOrUnused) {
            out[0] = static_cast<uint8_t>(s);
            out[1] = (s == Status::Error) ? static_cast<uint8_t>(psiOrUnused) : psiToByte05(psiOrUnused);
        }
        inline void packResponseError(uint8_t out[kPayloadLen], uint8_t errorCode) {
            out[0] = static_cast<uint8_t>(Status::Error);
            out[1] = errorCode;
        }
        inline bool parseResponse(const uint8_t* data, int len, Response& out) {
            if (len != kPayloadLen) return false;
            switch (data[0]) {
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

        // Legacy helpers (wrap new API)
        inline void packStart(uint8_t out[kPayloadLen], float targetPsi) { Request r; r.kind=Request::Kind::Start; r.targetPsi=targetPsi; packRequest(out,r); }
        inline void packCancel(uint8_t out[kPayloadLen]) { Request r; r.kind=Request::Kind::Idle; packRequest(out,r); }
        inline void packManual(uint8_t out[kPayloadLen], uint8_t code) { Request r; r.kind=Request::Kind::Manual; r.manual=static_cast<ManualCode>(code); packRequest(out,r); }
        inline void packPing(uint8_t out[kPayloadLen]) { Request r; r.kind=Request::Kind::Ping; packRequest(out,r); }
        inline bool parseStatus(const uint8_t* data, int len, StatusMsg& out) { return parseResponse(data,len,out); }

        // Parsed pairing message
        struct PairMsg {
            PairOp op;
            uint8_t value; // groupId or reason
        };

        // Pack pairing frames (explicit group id required)
        inline void packPairReq (uint8_t out[kPayloadLen], uint8_t groupId) { out[0] = (uint8_t)PairOp::Req;  out[1] = groupId; }
        inline void packPairAck (uint8_t out[kPayloadLen], uint8_t groupId) { out[0] = (uint8_t)PairOp::Ack;  out[1] = groupId; }
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