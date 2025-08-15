#include <stdint.h> // for uint8_t type

// Enums must be defined before Arduino's auto-generated prototypes
enum class ControlState { IDLE, AIRUP, VENTING, CHECKING, ERROR };
enum class ErrorCode : uint8_t {
  NONE = 0,
  NO_CHANGE = 1,
  EXCESSIVE_TIME = 2,
  SENSOR_FAULT = 3,      // reserved
  OVERPRESSURE = 4,      // reserved
  UNDERPRESSURE = 5,     // reserved
  OUTPUT_CONFLICT = 6,   // reserved
  UNKNOWN = 255
};

#include <Arduino.h>
#include <WiFi.h>
#include <esp_wifi.h>
#include <esp_now.h>
#include <deque>
#include <math.h>
#include <string.h>

#define DEBUG_SEND 0

#pragma region ESP-NOW Variables

uint8_t remoteBoardAddress[] = { 0x34, 0x85, 0x18, 0x07, 0xd5, 0x2c };

// Status/command and PSI encoding helpers (0.5 PSI resolution)
static inline uint8_t psiToByte05(float psi) {
  if (psi < 0) psi = 0;
  if (psi > 127.5f) psi = 127.5f;
  return (uint8_t)roundf(psi * 2.0f);
}
static inline float byteToPsi05(uint8_t b) {
  return (float)b * 0.5f;
}

#pragma endregion

// Control pins
static const int COMPRESSOR_PIN = 9;   // HIGH = enabled
static const int VENT_PIN       = 10;  // HIGH = open
static const int PRESSURE_PIN   = 3;   // analog sensor input

// Control state
static ControlState controlState = ControlState::IDLE;
static float targetPsi = 0.0f;
static float currentPsi = 0.0f;

// Error codes (sent when status='E')
static uint8_t errorCode = (uint8_t)ErrorCode::NONE;

// Status send interval
static const unsigned long STATUS_INTERVAL_MS = 100;
static unsigned long lastStatusSentMs = 0;

// Pressure smoothing buffer (rolling average) used by getSmoothedPressure()
static const int NUM_SAMPLES = 10;
static const float NOISE_THRESHOLD_PSI = 0.5f;
static float pressureSamples[NUM_SAMPLES] = {0};
static int sampleIndex = 0;
static bool bufferFilled = false;

// Seeking control parameters and learned rates
static const float PSI_TOL = 0.1f;               // done when within 0.1 PSI of target
static const unsigned long BURST_MS_INIT = 2000;  // initial short burst
static const unsigned long CHECK_SETTLE_MS = 1000; // settle time before reading
static const unsigned long RUN_MIN_MS = 1000;
static const unsigned long RUN_MAX_MS = 4000;

static float upRatePsiPerSec = 0.0f;   static int upRateSamples = 0;
static float downRatePsiPerSec = 0.0f; static int downRateSamples = 0;

static float phaseStartPsi = 0.0f;
static unsigned long phaseStartMs = 0;
static unsigned long phaseEndMs = 0;          // end of burst/settle
static unsigned long lastBurstEndMs = 0;      // timestamp when actuator stopped (end of burst)
static bool inContinuousRun = false;
static unsigned long continuousEndMs = 0;     // end of continuous run
static bool manualActive = false;             // manual while-held override from remote
static const unsigned long MANUAL_HOLD_TIMEOUT_MS = 1000; // manual must refresh within this window
static unsigned long lastManualRefreshMs = 0;

// Error handling constants
static const unsigned long MAX_CONTINUOUS_MS = 30UL * 60UL * 1000UL; // 30 minutes
static const float NO_CHANGE_EPS = 0.02f;       // psi change considered negligible per burst
static const int MAX_NOCHANGE_BURSTS = 3;       // consecutive negligible bursts before error
static int noChangeBurstCount = 0;

// Forward declarations for functions used before definition
float getSmoothedPressure();
void sendStatus();
void onReceiveData(const esp_now_recv_info_t *esp_now_info, const uint8_t *incomingData, int len);
void onDataSent(const esp_now_send_info_t *tx_info, esp_now_send_status_t status);

void setup() {
  Serial.begin(115200);
  Serial.println("TrailAir Control Board Starting...");

  pinMode(COMPRESSOR_PIN, OUTPUT);
  pinMode(VENT_PIN, OUTPUT);
  digitalWrite(COMPRESSOR_PIN, LOW);
  digitalWrite(VENT_PIN, LOW);

  // Pressure pin is analog input on GPIO3; no pinMode needed for ADC

  setupESPNOW();
}

void setupESPNOW() {
  Serial.println("Setting up ESP-NOW...");

  WiFi.mode(WIFI_STA);
  WiFi.disconnect();

  // Print MAC address of this board
  uint8_t baseMac[6];
  esp_err_t ret = esp_wifi_get_mac(WIFI_IF_STA, baseMac);
  if (ret == ESP_OK) {
    Serial.print(">>> MAC Address: ");
    Serial.printf("%02x:%02x:%02x:%02x:%02x:%02x\n",
                  baseMac[0], baseMac[1], baseMac[2],
                  baseMac[3], baseMac[4], baseMac[5]);
  } else {
    Serial.println("Failed to read MAC address");
  }

  if (esp_now_init() != ESP_OK) {
    Serial.println("ESP-NOW init failed");
    return;
  }

  esp_now_register_recv_cb(onReceiveData);
  esp_now_register_send_cb(onDataSent);

  // Register remote peer
  esp_now_peer_info_t peerInfo = {};
  memcpy(peerInfo.peer_addr, remoteBoardAddress, 6);
  peerInfo.channel = 0;
  peerInfo.encrypt = false;

  Serial.print("Registering peer: ");
  Serial.printf("%02x:%02x:%02x:%02x:%02x:%02x\n",
                remoteBoardAddress[0], remoteBoardAddress[1],
                remoteBoardAddress[2], remoteBoardAddress[3],
                remoteBoardAddress[4], remoteBoardAddress[5]);

  if (!esp_now_is_peer_exist(remoteBoardAddress)) {
    if (esp_now_add_peer(&peerInfo) != ESP_OK) {
      Serial.println("Failed to add peer");
    } else {
      Serial.println("Peer added successfully");
    }
  }

  Serial.println("ESP-NOW initialized");
}

void loop() {
  // Update current pressure
  currentPsi = getSmoothedPressure();

  unsigned long now = millis();

  // Manual override: maintain outputs and skip seek state machine
  if (manualActive) {
    // Watchdog: if manual command not refreshed recently, stop for safety
    if (now - lastManualRefreshMs > MANUAL_HOLD_TIMEOUT_MS) {
      manualActive = false;
      controlState = ControlState::IDLE;
      stopOutputs();
      Serial.println("Manual control timeout. Stopping outputs.");
    } else {
      if (controlState == ControlState::AIRUP) {
        setCompressor(true);
      } else if (controlState == ControlState::VENTING) {
        setVent(true);
      } else {
        stopOutputs();
      }
    }
  } else {
    // Non-blocking control routine
    switch (controlState) {
      case ControlState::AIRUP:
      case ControlState::VENTING: {
        // Early stop if target reached during a burst/continuous run to avoid overshoot
        float remaining = targetPsi - currentPsi;
        if (fabsf(remaining) <= PSI_TOL) {
          stopOutputs();
          controlState = ControlState::CHECKING;
          lastBurstEndMs = now;                 // record when actuation stopped
          phaseEndMs = now + CHECK_SETTLE_MS;   // settle before confirming
          break;
        }

        // If in continuous run, stop at time; else stop at end of short burst
        if (inContinuousRun) {
          if (now >= continuousEndMs) {
            stopOutputs();
            controlState = ControlState::CHECKING;
            lastBurstEndMs = now;               // record burst end
            phaseEndMs = now + CHECK_SETTLE_MS; // reuse as settle end
          }
        } else {
          if (now >= phaseEndMs) {
            stopOutputs();
            controlState = ControlState::CHECKING;
            lastBurstEndMs = now;               // record burst end
            phaseEndMs = now + CHECK_SETTLE_MS; // reuse as settle end
          }
        }
        break;
      }

      case ControlState::CHECKING: {
        if (now >= phaseEndMs) {
          // Learn rate using actuator time only; delta PSI uses settled value
          float dtSec = (lastBurstEndMs > phaseStartMs)
                        ? (lastBurstEndMs - phaseStartMs) / 1000.0f
                        : (now - phaseStartMs) / 1000.0f;
          if (dtSec > 0.02f) {
            float dPsi = currentPsi - phaseStartPsi;
            float rate = fabsf(dPsi) / dtSec; // psi/sec (net effect)
            if (dPsi > 0.01f) { // went up
              upRatePsiPerSec = (upRatePsiPerSec * upRateSamples + rate) / (upRateSamples + 1);
              upRateSamples++;
            } else if (dPsi < -0.01f) { // went down
              downRatePsiPerSec = (downRatePsiPerSec * downRateSamples + rate) / (downRateSamples + 1);
              downRateSamples++;
            }

            // Detect no-change after burst cycles
            if (!inContinuousRun) {
              if (fabsf(dPsi) < NO_CHANGE_EPS) {
                noChangeBurstCount++;
                if (noChangeBurstCount >= MAX_NOCHANGE_BURSTS) {
                  enterError(ErrorCode::NO_CHANGE, "No pressure change detected after bursts");
                }
              } else {
                noChangeBurstCount = 0; // reset on meaningful change
              }
            } else {
              noChangeBurstCount = 0; // reset if we were in continuous run
            }
          }

          if (controlState == ControlState::ERROR) {
            break;
          }

          float remaining = targetPsi - currentPsi;
          if (fabsf(remaining) <= PSI_TOL) {
            controlState = ControlState::IDLE;
            stopOutputs();
          } else {
            bool needUp = remaining > 0;
            bool haveRate = needUp ? (upRateSamples >= 2 && upRatePsiPerSec > 0.001f)
                                   : (downRateSamples >= 2 && downRatePsiPerSec > 0.001f);
            if (haveRate) {
              float rate = needUp ? upRatePsiPerSec : downRatePsiPerSec;

              // Abort if the estimated time to target exceeds 30 minutes
              unsigned long predictedFullMs = (unsigned long)(1000.0f * (fabsf(remaining) / rate));
              if (predictedFullMs > MAX_CONTINUOUS_MS) {
                enterError(ErrorCode::EXCESSIVE_TIME, "Estimated run exceeds 30 minutes");
                break;
              }

              float margin = 0.2f; // leave a bit to avoid overshoot, finish with check
              float aim = fmaxf(0.0f, fabsf(remaining) - margin);
              unsigned long runMs = (unsigned long)(1000.0f * (aim / rate));
              if (runMs < RUN_MIN_MS) runMs = RUN_MIN_MS;
              if (runMs > RUN_MAX_MS) runMs = RUN_MAX_MS;
              // Schedule continuous run
              inContinuousRun = true;
              phaseStartPsi = currentPsi;
              phaseStartMs = now;
              continuousEndMs = now + runMs;
              if (needUp) { controlState = ControlState::AIRUP; setCompressor(true); }
              else        { controlState = ControlState::VENTING; setVent(true); }
            } else {
              // Probe again with short burst to learn rate
              scheduleBurst(needUp ? ControlState::AIRUP : ControlState::VENTING, BURST_MS_INIT);
            }
          }
        }
        break;
      }

      case ControlState::IDLE:
      case ControlState::ERROR:
      default:
        stopOutputs();
        break;
    }
  }

  // Periodic status back to remote (every 100 ms)
  if (now - lastStatusSentMs >= STATUS_INTERVAL_MS) {
    sendStatus();
    lastStatusSentMs = now;
  }
}

#pragma region Control helpers
void setCompressor(bool on) {
  digitalWrite(COMPRESSOR_PIN, on ? HIGH : LOW);
  if (on) digitalWrite(VENT_PIN, LOW); // ensure mutual exclusion
}

void setVent(bool open) {
  digitalWrite(VENT_PIN, open ? HIGH : LOW);
  if (open) digitalWrite(COMPRESSOR_PIN, LOW); // ensure mutual exclusion
}

char stateToStatusChar(ControlState s) {
  switch (s) {
    case ControlState::IDLE:    return 'I';
    case ControlState::AIRUP:   return 'U';
    case ControlState::VENTING: return 'V';
    case ControlState::ERROR:   return 'E';
    case ControlState::CHECKING: default: return 'I';
  }
}

void stopOutputs() {
  setCompressor(false);
  setVent(false);
}

void scheduleBurst(ControlState dir, unsigned long durMs) {
  phaseStartPsi = currentPsi;
  phaseStartMs = millis();
  phaseEndMs = phaseStartMs + durMs;
  inContinuousRun = false;
  if (dir == ControlState::AIRUP) {
    controlState = ControlState::AIRUP;
    setCompressor(true);
  } else {
    controlState = ControlState::VENTING;
    setVent(true);
  }
}

void startSeek(float psi) {
  // Clamp target to supported range
  if (psi < 5.0f) psi = 5.0f;
  if (psi > 50.0f) psi = 50.0f;

  // Reset learning and counters for a fresh seek
  upRatePsiPerSec = 0.0f; upRateSamples = 0;
  downRatePsiPerSec = 0.0f; downRateSamples = 0;
  noChangeBurstCount = 0;
  inContinuousRun = false;
  errorCode = (uint8_t)ErrorCode::NONE; // clear any previous error

  targetPsi = psi;
  stopOutputs();
  float diff = targetPsi - currentPsi;
  if (fabsf(diff) <= PSI_TOL) {
    controlState = ControlState::IDLE;
    return;
  }
  scheduleBurst(diff > 0 ? ControlState::AIRUP : ControlState::VENTING, BURST_MS_INIT);
}

void enterError(ErrorCode code, const char* reason) {
  errorCode = (uint8_t)code;
  controlState = ControlState::ERROR;
  stopOutputs();
  Serial.print("ERROR [");
  Serial.print((int)errorCode);
  Serial.print("]: ");
  Serial.println(reason);
}

#pragma endregion

float getSmoothedPressure() {
  // Take reading in mV
  int mV = analogReadMilliVolts(PRESSURE_PIN);
  float volts = mV / 1000.0;

  // Convert volts to PSI — adjust 0.5–4.5V range if divided
  float psi = (volts - 0.5) * (150.0 / 4.0);
  psi = constrain(psi, 0, 150);  // Clamp to valid range

  // Add to rolling average buffer
  pressureSamples[sampleIndex] = psi;
  sampleIndex = (sampleIndex + 1) % NUM_SAMPLES;
  if (sampleIndex == 0) bufferFilled = true;

  // Compute average
  float sum = 0;
  int count = bufferFilled ? NUM_SAMPLES : sampleIndex;
  for (int i = 0; i < count; i++) sum += pressureSamples[i];
  float avgPsi = sum / count;

  // Noise filter: if pressure is close to zero, just return 0
  if (avgPsi < NOISE_THRESHOLD_PSI) return 0.0;
  return avgPsi;
}


#pragma region Communication
void sendStatus() {
  uint8_t payload[2];
  payload[0] = (uint8_t)stateToStatusChar(controlState);
  payload[1] = (controlState == ControlState::ERROR)
                 ? errorCode
                 : psiToByte05(currentPsi);
  esp_now_send(remoteBoardAddress, payload, sizeof(payload));
}

void onReceiveData(const esp_now_recv_info_t *esp_now_info, const uint8_t *incomingData, int len) {
  if (len < 2) return;

  char cmd = (char)incomingData[0];
  uint8_t psiB = incomingData[1];
  float psi = byteToPsi05(psiB);

  switch (cmd) {
    case 'I': // Idle/cancel
      targetPsi = 0.0f;
      controlState = ControlState::IDLE;
      inContinuousRun = false;
      manualActive = false; // stop manual override on release
      errorCode = (uint8_t)ErrorCode::NONE; // clear error on explicit idle
      stopOutputs();
      Serial.println("CMD: IDLE");
      break;
    case 'S': // Seek to target
      Serial.printf("CMD: SEEK to %.1f PSI\n", psi);
      manualActive = false; // disable manual if it was active
      startSeek(psi);
      break;
    case 'M': { // Manual while-held control
      inContinuousRun = false;
      lastManualRefreshMs = millis();
      if (psiB == 0) {
        manualActive = true;
        controlState = ControlState::VENTING;
        setVent(true);
        Serial.println("CMD: MANUAL VENT");
      } else if (psiB == 255) {
        manualActive = true;
        controlState = ControlState::AIRUP;
        setCompressor(true);
        Serial.println("CMD: MANUAL AIRUP");
      } else {
        // Unknown manual code; ignore
        Serial.printf("CMD: MANUAL unknown code %u\n", (unsigned)psiB);
      }
      break;
    }
    case 'P': // Ping
    default:
      // No state change; respond via normal 100ms status
      Serial.println("CMD: PING/UNKNOWN");
      break;
  }
}

void onDataSent(const esp_now_send_info_t *tx_info, esp_now_send_status_t status) {
#if DEBUG_SEND
  Serial.printf("Last Packet Send Status: %s\n", status == ESP_NOW_SEND_SUCCESS ? "Success" : "Fail");
#endif
}
#pragma endregion