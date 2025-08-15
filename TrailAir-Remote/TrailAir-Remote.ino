#include <SPI.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

#include <SmartButton.h>
#include <esp_sleep.h>

#include <WiFi.h>
#include <esp_wifi.h>
#include <esp_now.h>
#include <math.h>  // for roundf

#define DEBUG_SEND 1
// #define DEBUG_DRAWS 0

#pragma region Screen Variables
#define SCREEN_WIDTH 128 // OLED display width, in pixels
#define SCREEN_HEIGHT 32 // OLED display height, in pixels

// Declaration for an SSD1306 display connected to I2C (SDA, SCL pins)
#define OLED_RESET     -1 // Reset pin # (or -1 if sharing Arduino reset pin)
#define SCREEN_ADDRESS 0x3C ///< See datasheet for Address; 0x3D for 128x64, 0x3C for 128x32
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

#define NUMFLAKES     10 // Number of snowflakes in the animation example

#define LOGO_HEIGHT   32 
#define LOGO_WIDTH    35

// '128-32', 128x32px
static const unsigned char PROGMEM logo_bmp [] = {
	0xff, 0xff, 0xff, 0xff, 0xe0, 0xff, 0xff, 0xff, 0xff, 0xe0, 0xff, 0xff, 0xff, 0xff, 0xe0, 0xff, 
	0xff, 0xff, 0xff, 0xe0, 0xf0, 0x00, 0x00, 0x01, 0xe0, 0xf0, 0x00, 0x00, 0x01, 0xe0, 0xf0, 0x00, 
	0x00, 0x01, 0xe0, 0xf1, 0xf0, 0x00, 0xf9, 0xe0, 0xf1, 0xf8, 0x00, 0xf9, 0xe0, 0xf1, 0xfc, 0x00, 
	0xf9, 0xe0, 0xf1, 0xfe, 0x00, 0xf9, 0xe0, 0xf1, 0xff, 0x00, 0xf9, 0xe0, 0xf1, 0xff, 0x80, 0xf9, 
	0xe0, 0xf1, 0xff, 0xc0, 0xf9, 0xe0, 0xf1, 0xe7, 0xf0, 0xf9, 0xe0, 0xf1, 0xe3, 0xf8, 0xf9, 0xe0, 
	0xf1, 0xe1, 0xfc, 0xf9, 0xe0, 0xf1, 0xe0, 0xfe, 0xf9, 0xe0, 0xf1, 0xe0, 0x7f, 0xf9, 0xe0, 0xf1, 
	0xe0, 0x3f, 0xf9, 0xe0, 0xf1, 0xe0, 0x1f, 0xf9, 0xe0, 0xf1, 0xe0, 0x0f, 0xf9, 0xe0, 0xf1, 0xe0, 
	0x07, 0xf9, 0xe0, 0xf1, 0xe0, 0x03, 0xf9, 0xe0, 0xf1, 0xe0, 0x00, 0xf9, 0xe0, 0xf1, 0xe0, 0x00, 
	0x01, 0xe0, 0xf1, 0xe0, 0x00, 0x01, 0xe0, 0xf1, 0xe0, 0x00, 0x01, 0xe0, 0xff, 0xe3, 0xff, 0xff, 
	0xe0, 0xff, 0xe3, 0xff, 0xff, 0xe0, 0xff, 0xe3, 0xff, 0xff, 0xe0, 0xff, 0xe3, 0xff, 0xff, 0xe0
};

// 'connected_20x20', 20x20px
static const unsigned char PROGMEM icon_connected_20x20 [] = {
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x3f, 0xff, 0xc0, 0xe0, 
	0x00, 0x70, 0x00, 0x00, 0x00, 0x1f, 0xff, 0x80, 0x70, 0x00, 0xe0, 0xc0, 0x00, 0x30, 0x0f, 0xff, 
	0x00, 0x18, 0x01, 0x80, 0x30, 0x00, 0xc0, 0x03, 0xfc, 0x00, 0x06, 0x06, 0x00, 0x00, 0x00, 0x00, 
	0x00, 0xf0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
};

// 'disconnected_20x20', 20x20px
static const unsigned char PROGMEM icon_disconnected_20x20 [] = {
	0x03, 0xfc, 0x30, 0x0e, 0x07, 0x70, 0x18, 0x01, 0xe0, 0x30, 0x01, 0xc0, 0x6f, 0xff, 0xe0, 0x58, 
	0x07, 0xa0, 0xc0, 0x0e, 0x30, 0x9f, 0xff, 0x90, 0xf0, 0x38, 0xf0, 0xc0, 0x70, 0x30, 0x8f, 0xff, 
	0x10, 0x99, 0xc1, 0x90, 0x93, 0x80, 0x90, 0xc7, 0xfc, 0x30, 0x4e, 0x06, 0x20, 0x7c, 0x00, 0x60, 
	0x38, 0xf0, 0xc0, 0x78, 0x01, 0x80, 0xee, 0x07, 0x00, 0xc3, 0xfc, 0x00
};

// 'connected_8x6', 8x6px
static const unsigned char PROGMEM icon_connected_8x6 [] = {
	0x3c, 0x42, 0x81, 0x3c, 0x42, 0x18
};
// 'disconnected_8x6', 8x6px
static const unsigned char PROGMEM icon_disconnected_8x6 [] = {
	0xc3, 0x66, 0x3c, 0x3c, 0x66, 0xc3
};
#pragma endregion

#pragma region Battery Variables
#define BATTERY_PIN    2 // A0, GPIO2
float batteryVoltage = 0.0;
int batteryPercent = 0;

// Battery smoothing and filtering
#define BATTERY_SAMPLES 10
int batteryMvBuffer[BATTERY_SAMPLES] = {0};
uint8_t batteryMvIndex = 0;
uint8_t batteryMvCount = 0;
long batteryMvSum = 0;           // sum of buffer in mV
int batteryFilteredMv = 0;       // filtered output in mV (battery side)
const int BATTERY_DEADBAND_MV = 50; // ignore changes smaller than 50 mV at the battery
#pragma endregion

#pragma region Button Variables
// GPIO pins for each button
#define BTN_LEFT_PIN   10  // retry / wake
#define BTN_DOWN_PIN   9
#define BTN_UP_PIN     8
#define BTN_RIGHT_PIN  20

// SmartButton objects (active-low with pull-ups)
using namespace smartbutton;
SmartButton btnLeft(BTN_LEFT_PIN);
SmartButton btnDown(BTN_DOWN_PIN);
SmartButton btnUp(BTN_UP_PIN);
SmartButton btnRight(BTN_RIGHT_PIN);

// event handlers
void leftEventCallback(SmartButton *button, SmartButton::Event event, int clickCounter);
void downEventCallback(SmartButton *button, SmartButton::Event event, int clickCounter);
void upEventCallback(SmartButton *button, SmartButton::Event event, int clickCounter);
void rightEventCallback(SmartButton *button, SmartButton::Event event, int clickCounter);

const unsigned long SLEEP_TIMEOUT = 60000; // ms
unsigned long lastButtonPressedTime = 0;

// Manual sleep logic variables
bool sleepSequenceStarted = false;

// Icons for button hints
// 'arrow_down', 6x6px
static const unsigned char PROGMEM icon_arrow_down_6x6 [] = {
	0x30, 0x30, 0x30, 0xfc, 0x78, 0x30
};

// 'arrow_right', 6x6px
static const unsigned char PROGMEM icon_arrow_right_6x6 [] = {
	0x10, 0x18, 0xfc, 0xfc, 0x18, 0x10
};

// 'arrow_up', 6x6px
static const unsigned char PROGMEM icon_arrow_up_6x6 [] = {
	0x30, 0x78, 0xfc, 0x30, 0x30, 0x30
};

// 'cancel', 6x6px
static const unsigned char PROGMEM icon_cancel_6x6 [] = {
	0x84, 0x48, 0x30, 0x30, 0x48, 0x84
};

// 'dash_6x6', 6x6px
static const unsigned char PROGMEM icon_dash_6x6 [] = {
	0x00, 0x00, 0xfc, 0xfc, 0x00, 0x00
};

// 'plus_6x6', 6x6px
static const unsigned char PROGMEM icon_plus_6x6 [] = {
	0x30, 0x30, 0xfc, 0xfc, 0x30, 0x30
};

// 'manual_control', 6x6px
const unsigned char icon_manual_control [] PROGMEM = {
	0x30, 0x48, 0x84, 0x84, 0x48, 0x30
};
#pragma endregion 

#pragma region State machine variables
// Remote
enum class RemoteState {
  DISCONNECTED,
  IDLE,
  MANUAL,
  SEEKING,
  ERROR
};

RemoteState currentState = RemoteState::DISCONNECTED;
RemoteState previousState = RemoteState::DISCONNECTED;

unsigned long stateEntryTime = 0;

// Controller
enum class ControlState {
  IDLE,
  AIRUP,
  VENTING,
  CHECKING,
  ERROR
};

ControlState currentControlState = ControlState::IDLE;
ControlState previousControlState = ControlState::IDLE;

// Short description for known error codes
static const char* errorCodeToShort(uint8_t code) {
  switch (code) {
    case 0:   return "None";
    case 1:   return "No change";
    case 2:   return "Too slow";
    case 3:   return "Sensor";
    case 4:   return "Over PSI";
    case 5:   return "Under PSI";
    case 6:   return "Conflict";
    case 255: return "Unknown";
    default:  return "Error";
  }
}

#pragma endregion

#pragma region ESP-NOW Variables
// Replace with your control board MAC
uint8_t controlBoardAddress[] = { 0x34, 0x85, 0x18, 0x06, 0x55, 0x6C };

// Hold timer to show "Done!" for 2s in SEEKING state before transitioning
unsigned long seeking_done_until_ms = 0;
unsigned long disconnected_show_connected_until_ms = 0;

volatile unsigned long lastPacketReceivedTime = 0;
const unsigned long CONNECTION_TIMEOUT = 5000; // ms
bool isConnected = false;
bool isConnecting = false;

unsigned long nextPingAtMs = 0;
unsigned long pingBackoffMs = 200;      // start small
const unsigned long pingBackoffMaxMs = 2000;

float currentPSI = 0.0;
float targetPSI = 25.0; // Default target PSI

// Manual control runtime vars
static const unsigned long MANUAL_SEND_INTERVAL_MS = 500;
static unsigned long lastManualSendMs = 0;
static bool manualIsSending = false;
static uint8_t lastManualCodeSent = 0x00; // 0x00=vent, 0xFF=air

// Last error code received from control board (valid only when status == 'E')
static uint8_t lastErrorCode = 0;
#pragma endregion

#pragma region Setups
void setup() {
  Serial.begin(115200);

  analogSetPinAttenuation(BATTERY_PIN, ADC_11db);
  setupScreen();
  setupButtons();
  setupESPNOW();

  delay(500);
  // Wipe out logo before beginning loop
  logo_wipe(logo_bmp, LOGO_WIDTH, LOGO_HEIGHT, false, 10);
}

void setupScreen() {
  // SSD1306_SWITCHCAPVCC = generate display voltage from 3.3V internally
  if(!display.begin(SSD1306_SWITCHCAPVCC, SCREEN_ADDRESS)) {
    Serial.println(F("SSD1306 OLED screen allocation failed"));
    for(;;); // Don't proceed, loop forever
  }

  // Clear display and wipe in logo for 2s
  display.clearDisplay();
  logo_wipe(logo_bmp, LOGO_WIDTH, LOGO_HEIGHT, true, 10);
}

void setupButtons() {
  pinMode(BTN_LEFT_PIN, INPUT_PULLUP); // external pullup
  pinMode(BTN_DOWN_PIN, INPUT_PULLUP); // pullup resistor on-board
  pinMode(BTN_UP_PIN, INPUT_PULLUP); // external pullup
  pinMode(BTN_RIGHT_PIN, INPUT_PULLUP); // external pullup

  // The SmartButton library required changes to SmartButtonDefs.h and SmartButton.cpp
  // to compile. The changes are:
  // SmartButtonDefs.h: 
  //    16    constexpr int (*getGpioState)(uint8_t) = digitalRead;
  // SmartButton.cpp:
  //    114   s = getGpioState(this->pin) == HIGH;
  btnLeft.begin(leftEventCallback);
  btnDown.begin(downEventCallback);
  btnUp.begin(upEventCallback);
  btnRight.begin(rightEventCallback);

  gpio_wakeup_enable(GPIO_NUM_10, GPIO_INTR_LOW_LEVEL);
  esp_err_t result = esp_sleep_enable_gpio_wakeup();

  if (result == ESP_OK) {
    Serial.println("GPIO Wake-Up set successfully.");
  } else {
    Serial.println("Failed to set GPIO Wake-Up as wake-up source.");
  }
}

void setupESPNOW() {
  WiFi.mode(WIFI_STA);
  WiFi.disconnect();

  // Print MAC address of the this board
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

  // Register control board peer
  esp_now_peer_info_t peerInfo = {};
  memcpy(peerInfo.peer_addr, controlBoardAddress, 6);
  peerInfo.channel = 0;
  peerInfo.encrypt = false;

  Serial.print("Registering peer: ");
  Serial.printf("%02x:%02x:%02x:%02x:%02x:%02x\n",
                  controlBoardAddress[0], controlBoardAddress[1],
                  controlBoardAddress[2], controlBoardAddress[3],
                  controlBoardAddress[4], controlBoardAddress[5]);

  // Check if peer already exists in ESP-NOW registry
  if (!esp_now_is_peer_exist(controlBoardAddress)) {
    if (esp_now_add_peer(&peerInfo) != ESP_OK) {
      Serial.println("Failed to add peer");
    } else {
      Serial.println("Peer added successfully");
    }
  }

  Serial.println("ESP-NOW initialized");
  enterState(RemoteState::DISCONNECTED); // remain in DISCONNECTED state until first packet
}
#pragma endregion


void loop() {
  // Read buttons
  SmartButton::service();
  if (millis() - lastButtonPressedTime > SLEEP_TIMEOUT) {
    Serial.println("Sleep timeout exceeded.");
    goToSleep();
  }

  // Check battery voltage
  updateBatteryStatus();
  // Serial.printf("Battery: %.2f V (%d%%)\n", batteryVoltage, batteryPercent);

  // Update State Machine & handle display updates
  updateConnectionStatus();

  display.clearDisplay();
  updateStateMachine();
  display.display();

  // Service ESP-NOW reconnection if needed
  if (isConnecting && !isConnected) {
    serviceReconnect();
  } 

  delay(10);
}


void updateBatteryStatus() {
  // Read voltage at divider (mV at pin)
  uint32_t mv_pin = analogReadMilliVolts(BATTERY_PIN);
  int mv_batt = (int)mv_pin * 2; // Convert to battery mV (divider is 1/2)

  // Update rolling average buffer
  if (batteryMvCount < BATTERY_SAMPLES) {
    batteryMvCount++;
    batteryMvSum += mv_batt;
    batteryMvBuffer[batteryMvIndex] = mv_batt;
    batteryMvIndex = (batteryMvIndex + 1) % BATTERY_SAMPLES;
  } else {
    batteryMvSum -= batteryMvBuffer[batteryMvIndex];
    batteryMvSum += mv_batt;
    batteryMvBuffer[batteryMvIndex] = mv_batt;
    batteryMvIndex = (batteryMvIndex + 1) % BATTERY_SAMPLES;
  }

  int avgMv = (int)(batteryMvSum / (batteryMvCount == 0 ? 1 : batteryMvCount));

  // Deadband filter: only update if outside threshold or on first measurement
  if (batteryMvCount == 1 || abs(avgMv - batteryFilteredMv) >= BATTERY_DEADBAND_MV) {
    batteryFilteredMv = avgMv;
  }

  // Publish filtered voltage and percent
  batteryVoltage = batteryFilteredMv / 1000.0f; // volts
  batteryPercent = (int)(((batteryVoltage - 3.3f) / (4.2f - 3.3f)) * 100.0f);
  if (batteryPercent > 100) batteryPercent = 100;
  if (batteryPercent < 0) batteryPercent = 0;
}


#pragma region Button Functions
void leftEventCallback(SmartButton *button, SmartButton::Event event, int clickCounter) {
  lastButtonPressedTime = millis();

  if (event == SmartButton::Event::LONG_HOLD) {
    Serial.println("Left button long-pressed, going to sleep...");
    goToSleep();
  }

  if (event == SmartButton::Event::CLICK) {
    Serial.println("Left button pressed");

    switch (currentState) {
      case RemoteState::IDLE:
        // Enter manual control mode
        lastManualSendMs = 0;
        manualIsSending = false;
        lastManualCodeSent = 0x00;
        enterState(RemoteState::MANUAL);
        break;
      case RemoteState::MANUAL:
        // Cancel manual and return to IDLE
        sendCancelCommand();
        manualIsSending = false;
        enterState(RemoteState::IDLE);
        break;
      case RemoteState::DISCONNECTED:
      case RemoteState::SEEKING:
      case RemoteState::ERROR:
        break;
    }
  }
}

void downEventCallback(SmartButton *button, SmartButton::Event event, int clickCounter) {
  lastButtonPressedTime = millis();

  if (event == SmartButton::Event::PRESSED) {
    // If not manually controlled, start sending manual venting command and everything that goes with that
    if (currentState == RemoteState::MANUAL && !manualIsSending) {
      lastManualCodeSent = 0x00; // VENTING
      manualIsSending = true;
      lastManualSendMs = millis();
      sendManualCommand(lastManualCodeSent);
    }
  }

  if (event == SmartButton::Event::RELEASED) {
    // If manually controlled, stop and send idle command
    if (currentState == RemoteState::MANUAL && manualIsSending) {
      manualIsSending = false;
      sendCancelCommand();
    }
  }

  if (event == SmartButton::Event::CLICK) {
    Serial.println("Down button pressed");

    switch (currentState) {
      case RemoteState::IDLE:
        targetPSI -= 1.0;
        if (targetPSI < 20.0) targetPSI = 20.0; // Min PSI
        break;
    }
  }
}

void upEventCallback(SmartButton *button, SmartButton::Event event, int clickCounter) {
  lastButtonPressedTime = millis();

  if (event == SmartButton::Event::PRESSED) {
    // If not manually controlled, start sending manual airup command and everything that goes with that
    if (currentState == RemoteState::MANUAL && !manualIsSending) {
      lastManualCodeSent = 0xFF; // AIRUP
      manualIsSending = true;
      lastManualSendMs = millis();
      sendManualCommand(lastManualCodeSent);
    }
  }

  if (event == SmartButton::Event::RELEASED) {
    // If manually controlled, stop and send idle command
    if (currentState == RemoteState::MANUAL && manualIsSending) {
      manualIsSending = false;
      sendCancelCommand();
    }
  }
  
  if (event == SmartButton::Event::CLICK) {
    Serial.println("Up button pressed");

    switch (currentState) {
      case RemoteState::IDLE:
        targetPSI += 1.0;
        if (targetPSI > 45.0) targetPSI = 45.0; // Max PSI
        break;
    }
  }
}

void rightEventCallback(SmartButton *button, SmartButton::Event event, int clickCounter) {
  lastButtonPressedTime = millis();
  
  if (event == SmartButton::Event::CLICK) {
    Serial.println("Right button pressed");

    switch (currentState) {
      case RemoteState::IDLE: {
        // Send start command with current target PSI
        Serial.printf("Sending start command with target PSI: %.1f\n", targetPSI);
        sendStartCommand(targetPSI);
        break;
      }

      case RemoteState::SEEKING: {
        // Send cancel command
        sendCancelCommand();
        break;
      }

      case RemoteState::DISCONNECTED: {
        Serial.println("User requested reconnect.");

        // Retry connection logic
        if (!isConnected) {
          Serial.println("Retrying connection...");
          requestReconnectLight();
        }

        // Transition to IDLE if connected
        if (isConnected) {
          Serial.println("Reconnected successfully.");
          enterState(RemoteState::IDLE);
        }
        break;
      }

      case RemoteState::ERROR: {
        enterState(RemoteState::IDLE); // User acknowledges error
        break;
      }
    }
  }
}

void goToSleep() {
  Serial.println("Entering light sleep...");

  // If sleeping from MANUAL, send a final cancel to stop outputs
  if (currentState == RemoteState::MANUAL && manualIsSending) {
    sendCancelCommand();
    manualIsSending = false;
  }

  sleepSequenceStarted = true;
  drawLogo();
  delay(1000);
  logo_wipe(logo_bmp, LOGO_WIDTH, LOGO_HEIGHT, false, 10);
  
  WiFi.disconnect();
  WiFi.mode(WIFI_OFF);
  esp_wifi_stop();
  esp_light_sleep_start();

  Serial.println("Woke up from sleep.");
  setupESPNOW();
  enterState(RemoteState::DISCONNECTED);
}
#pragma endregion


#pragma region State Machine Functions
void enterState(RemoteState newState) {
  previousState = currentState;
  currentState = newState;
  stateEntryTime = millis();

  // If leaving MANUAL, send a single cancel and reset manual flags
  if (previousState == RemoteState::MANUAL && newState != RemoteState::MANUAL) {
    if (manualIsSending) {
      sendCancelCommand();
      manualIsSending = false;
    }
    lastManualSendMs = 0;
    lastManualCodeSent = 0x00;
  }

  Serial.print("Transitioning to state: ");
  Serial.println((int)newState);

  // Add any enter-once logic here if needed
}

void updateStateMachine() {
  switch (currentState) {
    case RemoteState::DISCONNECTED: {
      // Draw display
      drawDisconnectedScreen();
#ifdef DEBUG_DRAWS
      Serial.println("Drew Disconnected screen");
#endif

      // If connection comes up while in DISCONNECTED, start a 1s hold to show the connected icon
      if (isConnected) {
        if (disconnected_show_connected_until_ms == 0) {
          disconnected_show_connected_until_ms = millis() + 1000; // 1 second
        }
        // After 1s of showing the connected icon, transition to IDLE
        if (millis() >= disconnected_show_connected_until_ms) {
          disconnected_show_connected_until_ms = 0;
          enterState(RemoteState::IDLE);
          break;
        }
      } else {
        // If connection is not up, ensure the timer is cleared
        disconnected_show_connected_until_ms = 0;
      }
      break;
    }
    case RemoteState::IDLE: {
      // Draw IDLE screen
      drawIdleScreen();
#ifdef DEBUG_DRAWS
      Serial.println("Drew IDLE screen");
#endif
      
      // If connection is lost, go back to DISCONNECTED
      if (!isConnected) {
        enterState(RemoteState::DISCONNECTED);
        break;
      }

      break;
    }
    case RemoteState::MANUAL: {
      drawManualScreen();
#ifdef DEBUG_DRAWS
      Serial.println("Drew Manual screen");
#endif

      // If connection is lost, go back to DISCONNECTED
      if (!isConnected) {
        enterState(RemoteState::DISCONNECTED);
        break;
      }

      if (manualIsSending && millis() - lastManualSendMs >= MANUAL_SEND_INTERVAL_MS) {
        sendManualCommand(lastManualCodeSent);
        lastManualSendMs = millis();
      }
      break;
    }
    case RemoteState::SEEKING: {
      // Draw SEEKING screen
      drawSeekingScreen();
#ifdef DEBUG_DRAWS
      Serial.println("Drew Seeking screen");
#endif
      
      // Start or maintain the 2-second Done! hold if status is done
      if (currentControlState == ControlState::IDLE && seeking_done_until_ms == 0) {
        seeking_done_until_ms = millis() + 2000;
      }

      // If complete or error received, go to IDLE or ERROR after hold
      if (!isConnected) {
        enterState(RemoteState::DISCONNECTED);
      } else if (currentControlState == ControlState::ERROR) {
        enterState(RemoteState::ERROR);
      } else if (seeking_done_until_ms != 0 && millis() >= seeking_done_until_ms) {
        seeking_done_until_ms = 0;
        enterState(RemoteState::IDLE);
        break;
      }

      break;
    }
    case RemoteState::ERROR: {
      // Display error, wait for user action or timeout to return to IDLE
      drawErrorScreen();
#ifdef DEBUG_DRAWS
      Serial.println("Drew Error screen");
#endif
      
      if (millis() - stateEntryTime > 5000) { // auto-clear after time
        enterState(RemoteState::IDLE);
      }
      break;
    }
  }
}
#pragma endregion


#pragma region Display Functions
void drawLogo(void) {
  display.clearDisplay();
  display.drawBitmap(0, 0, logo_bmp, LOGO_WIDTH, LOGO_HEIGHT, 1);
  display.display();
}

void logo_wipe(const uint8_t *bitmap, uint8_t bmp_width, uint8_t bmp_height, bool wipe_in, uint16_t delay_ms) {
  int x = (SCREEN_WIDTH - bmp_width) / 2;
  int y = (SCREEN_HEIGHT - bmp_height) / 2;

  display.clearDisplay();

  for (int w = 0; w <= bmp_width; w++) {
    display.drawBitmap(x, y, bitmap, bmp_width, bmp_height, WHITE);

    if (wipe_in) {
      // Mask the right side, revealing only the left w pixels
      display.fillRect(x + w, y, bmp_width - w, bmp_height, BLACK);
    } else {
      // Mask the left side, hiding the left w pixels
      display.fillRect(x, y, w, bmp_height, BLACK);
    }

    display.display();
    delay(delay_ms);
  }
}

void drawBatteryIcon() {
  // Battery icon (top left)
  int batteryX = 0;
  int batteryY = 0;
  int batteryW = 12;
  int batteryH = 6;
  int fillW = (int)((batteryPercent / 100.0) * (batteryW - 2));
  display.drawRect(batteryX, batteryY, batteryW, batteryH, WHITE); // battery outline
  display.drawRect(batteryX + batteryW, batteryY + 2, 1, 2, WHITE); // battery nub
  display.fillRect(batteryX + 1, batteryY + 1, fillW, batteryH - 2, WHITE); // battery fill

  if (batteryPercent < 15) {
    display.setTextSize(1);
    display.setTextColor(WHITE);
    display.setCursor(batteryX + batteryW + 2, batteryY);
    display.print("!"); // exclamation mark
  }
}

void drawConnectionIcon() {
  // Connection icon (top right)
  int connX = SCREEN_WIDTH - 8;
  int connY = 1;

  if (!isConnected) 
    display.drawBitmap(connX, connY, icon_disconnected_8x6, 8, 6, WHITE); 
  else
    display.drawBitmap(connX, connY, icon_connected_8x6, 8, 6, WHITE); 
}

void drawButtonHints(const uint8_t *left, const uint8_t *down, const uint8_t *up, const uint8_t *right) {
  int iconW = 32;
  int iconY = SCREEN_HEIGHT - 6;
  int offset = (iconW - 6) / 2;

  if (left)  display.drawBitmap(0  + offset, iconY, left, 6, 6, WHITE);
  if (down)  display.drawBitmap(32 + offset, iconY, down, 6, 6, WHITE);
  if (up)    display.drawBitmap(64 + offset, iconY, up, 6, 6, WHITE);
  if (right) display.drawBitmap(96 + offset, iconY, right, 6, 6, WHITE);
}

void drawDisconnectedScreen() {
  drawBatteryIcon();
  
  // Centered 20x20 icon
  const uint8_t* bmp = icon_disconnected_20x20;
  const uint8_t w = 20;
  const uint8_t h = 20;
  const int16_t x = (128 - w) / 2;
  const int16_t y = (32 - h) / 2;

  // If we're within the post-connect grace period, show the connected icon instead
  if (isConnected && disconnected_show_connected_until_ms != 0 && millis() < disconnected_show_connected_until_ms) {
    bmp = icon_connected_20x20;
  }

  display.drawBitmap(x, y, bmp, w, h, SSD1306_WHITE);

  // Right button hint (retry) when disconnected and not actively reconnecting
  if (!isConnected && !isConnecting) {
    drawButtonHints(nullptr, nullptr, nullptr, icon_arrow_right_6x6);
  }
}

void drawIdleScreen() {
  // Top status
  drawBatteryIcon();
  drawConnectionIcon();

  // Button hints: Left = manual, Down = decrease, Up = increase, Right = start
  drawButtonHints(icon_manual_control, icon_dash_6x6, icon_plus_6x6, icon_arrow_right_6x6);

  // Prepare strings
  String currentStr = String((int)currentPSI);
  String targetStr  = String((int)targetPSI);

  // Text settings
  display.setTextColor(WHITE);
  display.setTextSize(2);

  // Measure text bounds (uses current text size)
  int16_t bx, by; uint16_t bw, bh;
  display.getTextBounds(currentStr, 0, 0, &bx, &by, &bw, &bh);
  uint16_t curW = bw, curH = bh;
  display.getTextBounds(targetStr, 0, 0, &bx, &by, &bw, &bh);
  uint16_t tgtW = bw, tgtH = bh;

  // Layout regions
  int leftX0 = 0, leftX1 = (SCREEN_WIDTH / 2) - 8;   // leave space near center for arrow
  int rightX0 = (SCREEN_WIDTH / 2) + 8, rightX1 = SCREEN_WIDTH;
  int centerY = (SCREEN_HEIGHT - 16) / 2; // for size 2 text (approx 16px height)
  if (centerY < 0) centerY = 0;

  // Center-left current PSI
  int curX = leftX0 + (leftX1 - leftX0 - (int)curW) / 2;
  if (curX < 0) curX = 0;
  display.setCursor(curX, centerY);
  display.print(currentStr);

  // Center-right target PSI
  int tgtX = rightX0 + (rightX1 - rightX0 - (int)tgtW) / 2;
  if (tgtX < rightX0) tgtX = rightX0;
  display.setCursor(tgtX, centerY);
  display.print(targetStr);

  // Underline target PSI
  int underlineY = centerY + (int)tgtH;
  if (underlineY < SCREEN_HEIGHT) {
    display.drawLine(tgtX, underlineY, tgtX + (int)tgtW, underlineY, WHITE);
  }

  // Separator arrow pointing right (center of screen)
  int ax = (SCREEN_WIDTH / 2) - 5;
  int ay = SCREEN_HEIGHT / 2;
  display.fillTriangle(ax, ay - 5, ax, ay + 5, ax + 9, ay, WHITE);
}

void drawSeekingScreen() {
  // Top status
  drawBatteryIcon();
  drawConnectionIcon();

  // Button hints: Right = Cancel
  drawButtonHints(nullptr, nullptr, nullptr, icon_cancel_6x6);

  // If we're showing the Done! hold message, center it big and return
  if (seeking_done_until_ms != 0) {
    const char* doneTxt = "Done!";
    display.setTextColor(WHITE);
    display.setTextSize(2);
    int16_t bx, by; uint16_t bw, bh;
    display.getTextBounds(doneTxt, 0, 0, &bx, &by, &bw, &bh);
    int x = (SCREEN_WIDTH - (int)bw) / 2;
    int y = (SCREEN_HEIGHT - (int)bh) / 2;
    if (y < 8) y = 8; // keep clear of status icons
    display.setCursor(x, y);
    display.print(doneTxt);
    return;
  }

  // Determine verb text
  const char* verb = nullptr;
  switch (currentControlState) {
    case ControlState::IDLE:      verb = "Ready"; break;
    case ControlState::AIRUP:     verb = "Inflating..."; break;
    case ControlState::VENTING:   verb = "Deflating..."; break;
    case ControlState::CHECKING:  verb = "Checking..."; break;
    case ControlState::ERROR:     verb = "Error"; break;
  }

  // Build PSI string
  String psiStr = String((int)currentPSI) + " PSI";

  // Measure bounds for centered layout
  int16_t bx, by; 
  uint16_t bw1, bh1, bw2, bh2;
  display.setTextColor(WHITE);
  display.setTextSize(1);
  display.getTextBounds(verb, 0, 0, &bx, &by, &bw1, &bh1);
  display.setTextSize(2);
  display.getTextBounds(psiStr, 0, 0, &bx, &by, &bw2, &bh2);

  int totalH = (int)bh1 + 2 + (int)bh2;
  int yStart = (SCREEN_HEIGHT - totalH) / 2;
  if (yStart < 8) yStart = 8; // avoid top icons

  // Draw verb (size 1) centered
  int xVerb = (SCREEN_WIDTH - (int)bw1) / 2;
  display.setTextSize(1);
  display.setCursor(xVerb, yStart);
  display.print(verb);

  // Draw PSI (size 2) centered below
  int xPSI = (SCREEN_WIDTH - (int)bw2) / 2;
  int yPSI = yStart + (int)bh1 + 2;
  display.setTextSize(2);
  display.setCursor(xPSI, yPSI);
  display.print(psiStr);
}

void drawManualScreen() {
  // Top status
  drawBatteryIcon();
  drawConnectionIcon();

  // Button hints: Left = cancel, Down = vent, Up = airup
  drawButtonHints(icon_cancel_6x6, icon_arrow_down_6x6, icon_arrow_up_6x6, nullptr);

  // Center label
  const char* txt = "Manual";
  display.setTextColor(WHITE);
  display.setTextSize(2);
  int16_t bx, by; uint16_t bw, bh;
  display.getTextBounds(txt, 0, 0, &bx, &by, &bw, &bh);
  int x = (SCREEN_WIDTH - (int)bw) / 2;
  int y = (SCREEN_HEIGHT - (int)bh) / 2;
  if (y < 8) y = 8;
  display.setCursor(x, y);
  display.print(txt);
}

void drawErrorScreen() {
  // Top status
  drawBatteryIcon();
  drawConnectionIcon();

  // Right = acknowledge
  drawButtonHints(nullptr, nullptr, nullptr, icon_arrow_right_6x6);

  // Build message: prefer short description, fallback to code
  const char* desc = errorCodeToShort(lastErrorCode);

  // Compose "E:<code>" fallback if description is generic "Error"
  String msg = desc;
  if (strcmp(desc, "Error") == 0) {
    msg = "E:";
    msg += String((int)lastErrorCode);
  }

  // Center message, fallback to size 1 if width > screen
  display.setTextColor(WHITE);
  display.setTextSize(2);
  int16_t bx, by; uint16_t bw, bh;
  display.getTextBounds(msg, 0, 0, &bx, &by, &bw, &bh);
  if (bw > SCREEN_WIDTH) {
    display.setTextSize(1);
    display.getTextBounds(msg, 0, 0, &bx, &by, &bw, &bh);
  }
  int x = (SCREEN_WIDTH - (int)bw) / 2;
  int y = (SCREEN_HEIGHT - (int)bh) / 2;
  if (y < 8) y = 8;
  display.setCursor(x, y);
  display.print(msg);
}
#pragma endregion


#pragma region Communications
// Compact ESPNOW payload helpers (0.5 PSI resolution)
static inline uint8_t psiToByte05(float psi) {
  if (psi < 0) psi = 0;
  if (psi > 127.5f) psi = 127.5f;
  return (uint8_t)roundf(psi * 2.0f);
}

static inline float byteToPsi05(uint8_t b) {
  return (float)b * 0.5f;
}

void requestReconnectLight() {
  if (isConnected) return;
  isConnecting = true;
  pingBackoffMs = 200;
  nextPingAtMs = 0; // send immediately
}

void serviceReconnect() {
  if (!isConnecting || isConnected) return;

  unsigned long now = millis();
  if (now < nextPingAtMs) return;

  uint8_t ping[2] = { 'P', 0 }; // tiny ping
  esp_now_send(controlBoardAddress, ping, sizeof(ping));

  nextPingAtMs = now + pingBackoffMs;
  pingBackoffMs = min(pingBackoffMs * 2, pingBackoffMaxMs);
}

void sendStartCommand(float targetPSI) {
  uint8_t payload[2];
  payload[0] = 'S';
  payload[1] = psiToByte05(targetPSI);
  esp_now_send(controlBoardAddress, payload, sizeof(payload));
}

void sendCancelCommand() {
  uint8_t payload[2] = { 'I', 0 };
  esp_now_send(controlBoardAddress, payload, sizeof(payload));
}

void sendManualCommand(uint8_t code) {
  uint8_t payload[2] = { 'M', code };
  esp_now_send(controlBoardAddress, payload, sizeof(payload));
}

void onReceiveData(const esp_now_recv_info_t *esp_now_info, const uint8_t *incomingData, int len) {
  lastPacketReceivedTime = millis(); // update timestamp
  if (len != 2) return;

  char status = (char)incomingData[0];
  uint8_t psiB = incomingData[1];

  // Only update PSI when not in ERROR state; in ERROR, second byte is an error code
  if (status != 'E') {
    currentPSI = byteToPsi05(psiB);
  } else {
    lastErrorCode = psiB; // store error code for ERROR screen
  }

  switch (status) {
    case 'I': currentControlState = ControlState::IDLE;     break;
    case 'U': currentControlState = ControlState::AIRUP;    break;
    case 'V': currentControlState = ControlState::VENTING;  break;
    case 'C': currentControlState = ControlState::CHECKING; break;
    case 'E': currentControlState = ControlState::ERROR;    break;
    default:  
      currentControlState = ControlState::ERROR;   
      Serial.println("Unknown control state: " + String(status));
      break;
  }

  isConnected = true;
  isConnecting = false;
}

void onDataSent(const esp_now_send_info_t *tx_info, esp_now_send_status_t status) {
#if DEBUG_SEND
  Serial.printf("Last Packet Send Status: %s\n", status == ESP_NOW_SEND_SUCCESS ? "Success" : "Fail");
#endif
}

void updateConnectionStatus() {
  if (millis() - lastPacketReceivedTime > CONNECTION_TIMEOUT) {
    if (isConnected) {
      Serial.println("Connection lost.");
    }
    isConnected = false;
  }
}
#pragma endregion