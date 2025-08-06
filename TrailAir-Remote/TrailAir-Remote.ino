#include <SPI.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

#include <ezButton.h>
#include <esp_sleep.h>

#include <WiFi.h>
#include <esp_now.h>
#include <deque>
#include <ArduinoJson.h>

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
	0xff, 0xff, 0xff, 0xe0, 0xff, 0xff, 0xff, 0xff, 0xe0, 0xff, 0xff, 0xff, 0xff, 0xe0, 0xff, 0xff, 
	0xff, 0xff, 0xe0, 0xff, 0xff, 0xff, 0xff, 0xe0, 0xff, 0xff, 0xff, 0xff, 0xe0, 0xff, 0xff, 0xff, 
	0xff, 0xe0, 0xff, 0xff, 0xff, 0xff, 0xe0, 0xff, 0xff, 0xff, 0xff, 0xe0, 0xff, 0xff, 0xff, 0xff, 
	0xe0, 0xff, 0xff, 0xff, 0xff, 0xe0, 0xff, 0xff, 0xff, 0xff, 0xe0, 0xff, 0xff, 0xff, 0xff, 0xe0, 
	0xff, 0xff, 0xff, 0xff, 0xe0, 0xff, 0xff, 0xff, 0xff, 0xe0, 0xff, 0xff, 0xff, 0xff, 0xe0, 0xff, 
	0xff, 0xff, 0xff, 0xe0, 0xff, 0xff, 0xff, 0xff, 0xe0, 0xff, 0xff, 0xff, 0xff, 0xe0, 0xff, 0xff, 
	0xff, 0xff, 0xe0, 0xff, 0xff, 0xff, 0xff, 0xe0, 0xff, 0xff, 0xff, 0xff, 0xe0, 0xff, 0xff, 0xff, 
	0xff, 0xe0, 0xff, 0xff, 0xff, 0xff, 0xe0, 0xff, 0xff, 0xff, 0xff, 0xe0, 0xff, 0xff, 0xff, 0xff, 
	0xe0, 0xff, 0xff, 0xff, 0xff, 0xe0, 0xff, 0xff, 0xff, 0xff, 0xe0, 0xff, 0xff, 0xff, 0xff, 0xe0
};

// 'compressor', 20x17px
static const unsigned char PROGMEM icon_compressor [] = {
	0xff, 0xff, 0xf0, 0xff, 0xff, 0xf0, 0xff, 0xff, 0xf0, 0xff, 0xff, 0xf0, 0xff, 0xff, 0xf0, 0xff, 
	0xff, 0xf0, 0xff, 0xff, 0xf0, 0xff, 0xff, 0xf0, 0xff, 0xff, 0xf0, 0xff, 0xff, 0xf0, 0xff, 0xff, 
	0xf0, 0xff, 0xff, 0xf0, 0xff, 0xff, 0xf0, 0xff, 0xff, 0xf0, 0xff, 0xff, 0xf0, 0xff, 0xff, 0xf0, 
	0xff, 0xff, 0xf0
};
// 'vent', 20x17px
static const unsigned char PROGMEM icon_vent [] = {
	0xff, 0xff, 0xf0, 0xff, 0xff, 0xf0, 0xff, 0xff, 0xf0, 0xff, 0xff, 0xf0, 0xff, 0xff, 0xf0, 0xff, 
	0xff, 0xf0, 0xff, 0xff, 0xf0, 0xff, 0xff, 0xf0, 0xff, 0xff, 0xf0, 0xff, 0xff, 0xf0, 0xff, 0xff, 
	0xf0, 0xff, 0xff, 0xf0, 0xff, 0xff, 0xf0, 0xff, 0xff, 0xf0, 0xff, 0xff, 0xf0, 0xff, 0xff, 0xf0, 
	0xff, 0xff, 0xf0
};
#pragma endregion

#pragma region Battery Variables
#define BATTERY_PIN    2 // A0, GPIO2
float batteryVoltage = 0.0;
int batteryPercent = 0;
#pragma endregion

#pragma region Button Variables
// GPIO pins for each button
#define BTN_LEFT_PIN   10  // retry / wake
#define BTN_DOWN_PIN   9
#define BTN_UP_PIN     8
#define BTN_RIGHT_PIN  20

// Setup ezButton objects
ezButton btnLeft(BTN_LEFT_PIN, INPUT_PULLUP);
ezButton btnDown(BTN_DOWN_PIN, INPUT_PULLUP);
ezButton btnUp(BTN_UP_PIN, INPUT_PULLUP);
ezButton btnRight(BTN_RIGHT_PIN, INPUT_PULLUP);

// Track button presses for use in state logic
bool btnLeftPressed = false;
bool btnDownPressed = false;
bool btnUpPressed = false;
bool btnRightPressed = false;

const unsigned long SLEEP_TIMEOUT = 60000; // ms
unsigned long lastButtonPressedTime = 0;

// Manual sleep logic variables
unsigned long leftPressStart = 0;
bool sleepSequenceStarted = false;

// Icons for button hints
// 'arrow_down', 6x6px
static const unsigned char PROGMEM icon_arrow_down [] = {
	0x30, 0x30, 0x30, 0xfc, 0x78, 0x30
};
// 'arrow_right', 6x6px
static const unsigned char PROGMEM icon_arrow_right [] = {
	0x10, 0x18, 0xfc, 0xfc, 0x18, 0x10
};
// 'arrow_up', 6x6px
static const unsigned char PROGMEM icon_arrow_up [] = {
	0x30, 0x78, 0xfc, 0x30, 0x30, 0x30
};
// 'cancel', 6x6px
static const unsigned char PROGMEM icon_cancel [] = {
	0x84, 0x48, 0x30, 0x30, 0x48, 0x84
};
#pragma endregion 

#pragma region ESP-NOW Variables
// Replace with your control board MAC
uint8_t controlBoardAddress[] = { 0x24, 0x6F, 0x28, 0xAB, 0xCD, 0xEF }; // example

unsigned long lastPacketReceivedTime = 0;
const unsigned long CONNECTION_TIMEOUT = 5000; // ms
bool isConnected = false;
float latestPSI = 0.0;
char lastStatus[32] = "Disconnected";
bool isCompressorOn = false;
bool isVentOpen = false;

// Structure for outbound command
typedef struct {
  char cmd[16];
  float targetPSI;
} OutgoingMessage;

// JSON buffer sizes (tweak if needed)
const size_t JSON_SEND_SIZE = 64;
const size_t JSON_RECV_SIZE = 128;

struct QueuedMessage {
  String payload;
  uint8_t retriesLeft;
  unsigned long lastSentTime;
};

std::deque<QueuedMessage> messageQueue;
const int MAX_RETRIES = 5;
const unsigned long RESEND_INTERVAL = 500; // ms

float targetPSI = 20.0; // Default target PSI
#pragma endregion

#pragma region State machine variables
enum RemoteState {
  BOOT,
  DISCONNECTED,
  IDLE,
  SEEKING,
  ERROR
};

RemoteState currentState = BOOT;
RemoteState previousState = BOOT;

unsigned long stateEntryTime = 0;
#pragma endregion

void setup() {
  Serial.begin(115200);

  setupScreen();
  setupButtons();
  setupESPNOW();

  // Clear display and wipe in logo for 2s
  display.clearDisplay();
  logo_wipe(logo_bmp, LOGO_WIDTH, LOGO_HEIGHT, true, 20);
  delay(2000);
}

void setupScreen() {
  // SSD1306_SWITCHCAPVCC = generate display voltage from 3.3V internally
  if(!display.begin(SSD1306_SWITCHCAPVCC, SCREEN_ADDRESS)) {
    Serial.println(F("SSD1306 OLED screen allocation failed"));
    for(;;); // Don't proceed, loop forever
  }

  drawLogo();    // Draw the logo
}

void setupButtons() {
  btnLeft.setDebounceTime(50);
  btnDown.setDebounceTime(50);
  btnUp.setDebounceTime(50);
  btnRight.setDebounceTime(50);
}

void setupESPNOW() {
  WiFi.mode(WIFI_STA);
  WiFi.disconnect();

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

  if (!esp_now_is_peer_exist(controlBoardAddress)) {
    if (esp_now_add_peer(&peerInfo) != ESP_OK) {
      Serial.println("Failed to add peer");
      return;
    }
  }

  Serial.println("ESP-NOW initialized");
}


void loop() {
  logo_wipe(logo_bmp, LOGO_WIDTH, LOGO_HEIGHT, true);  // wipe in from left
  delay(3000);
  logo_wipe(logo_bmp, LOGO_WIDTH, LOGO_HEIGHT, false); // wipe out from left
  
  // Read buttons
  updateButtons();

  // Check battery voltage
  updateBatteryStatus();

  // Update State Machine
  updateConnectionStatus();
  updateStateMachine();

  // Handle display updates
  drawStatusIcons();
  drawButtonHints("Retry", "PSI-", "PSI+", "Start");
  drawCompressorVentIcons();

  // Process ESP-NOW message queue
  processMessageQueue();
}


void updateBatteryStatus() {
  // Read voltage at divider
  uint32_t mv = analogReadMilliVolts(BATTERY_PIN);
  batteryVoltage = (float)mv * 2.0 / 1000.0; // Convert to volts

  // Estimate percentage (simple linear, adjust as needed)
  batteryPercent = (int)(((batteryVoltage - 3.3) / (4.2 - 3.3)) * 100.0);
  if (batteryPercent > 100) batteryPercent = 100;
  if (batteryPercent < 0) batteryPercent = 0;
}


#pragma region Button Functions
void updateButtons() {
  btnLeft.loop();
  btnDown.loop();
  btnUp.loop();
  btnRight.loop();

  btnLeftPressed  = btnLeft.isPressed();
  btnDownPressed  = btnDown.isPressed();
  btnUpPressed    = btnUp.isPressed();
  btnRightPressed = btnRight.isPressed();

  // Manual sleep logic
  if (btnLeftPressed) {
    // Mark the beginning of when the left button was pressed
    if (leftPressStart == 0) 
      leftPressStart = millis();

    // Start sleep sequence if held for 5 seconds
    if (!sleepSequenceStarted && (millis() - leftPressStart >= 5000)) {
      Serial.println("Manual sleep initiated.");
      goToDeepSleep();
    }
  } else {
    leftPressStart = 0;
    sleepSequenceStarted = false;
  }

  if (btnLeftPressed || btnRightPressed || btnUpPressed || btnDownPressed) {
    // reset sleep timer
    lastButtonPressedTime = millis();
  }

  if (millis() - lastButtonPressedTime > SLEEP_TIMEOUT) {
    Serial.println("Sleep timeout exceeded.");
    goToDeepSleep();
  }

  // TODO: long press on left button puts the remote to sleep
}

void goToDeepSleep() {
  Serial.println("Entering deep sleep...");

  sleepSequenceStarted = true;
  drawLogo();
  delay(1000);
  logo_wipe(logo_bmp, LOGO_WIDTH, LOGO_HEIGHT, false, 20);
  
  // GPIO10 = ext0 wake source (must be RTC_GPIO capable)
  esp_sleep_enable_ext0_wakeup(GPIO_NUM_10, 0);  // Wake on LOW signal
  esp_deep_sleep_start();
}
#pragma endregion


#pragma region State Machine Functions
void enterState(RemoteState newState) {
  previousState = currentState;
  currentState = newState;
  stateEntryTime = millis();

  Serial.print("Transitioning to state: ");
  switch (newState) {
    case BOOT:         Serial.println("BOOT"); break;
    case DISCONNECTED: Serial.println("DISCONNECTED"); break;
    case IDLE:         Serial.println("IDLE"); break;
    case SEEKING:      Serial.println("SEEKING"); break;
    case ERROR:        Serial.println("ERROR"); break;
  }

  // Add any enter-once logic here if needed
}

void updateStateMachine() {
  switch (currentState) {
    case BOOT:
      // Wipe out logo before transitioning
      logo_wipe(logo_bmp, LOGO_WIDTH, LOGO_HEIGHT, false, 20);
      enterState(DISCONNECTED);
      break;

    case DISCONNECTED:
      // Wait for user to press retry button (BTN_RIGHT) to reconnect
      if (btnRightPressed) {
        // Retry connection logic
        if (!isConnected) {
          Serial.println("Retrying connection...");
          setupESPNOW();
        }

        // Transition to IDLE if connected
        if (isConnected) {
          enterState(IDLE);
          break;
        }
      }

      drawDisconnectedScreen();
      break;

    case IDLE:
      // Allow user to set PSI and send command
      if (btnUpPressed) {
        targetPSI += 1.0;
        if (targetPSI > 50.0) targetPSI = 50.0; // Max PSI
      }
      if (btnDownPressed) {
        targetPSI -= 1.0;
        if (targetPSI < 5.0) targetPSI = 5.0; // Min PSI
      }
      if (btnRightPressed) {
        // Start air-up/air-down process
        sendStartCommand(targetPSI);
        enterState(SEEKING);
      }
      // If connection is lost, go back to DISCONNECTED
      if (!isConnected) {
        enterState(DISCONNECTED);
      }
      break;

    case SEEKING:
      // Cancelable by user (BTN_RIGHT)
      if (btnRightPressed) {
        sendCancelCommand();
        enterState(IDLE);
      }
      // If complete or error received, go to IDLE or ERROR
      if (!isConnected) {
        enterState(DISCONNECTED);
      } else if (strcmp(lastStatus, "done") == 0) {
        enterState(IDLE);
      } else if (strcmp(lastStatus, "error") == 0) {
        enterState(ERROR);
      }
      break;

    case ERROR:
      // Display error, wait for user action or timeout to return to IDLE
      if (btnRightPressed) {
        enterState(IDLE); // User acknowledges error
      }
      if (millis() - stateEntryTime > 3000) { // auto-clear after 3s
        enterState(IDLE);
      }
      break;
  }
}
#pragma endregion


#pragma region Display Functions
void drawLogo(void) {
  display.clearDisplay();
  display.drawBitmap(0, 0, logo_bmp, LOGO_WIDTH, LOGO_HEIGHT, 1);
  display.display();
}

void logo_wipe(const uint8_t *bitmap, uint8_t bmp_width, uint8_t bmp_height, bool wipe_in = true, uint16_t delay_ms = 20) {
  int x = (SCREEN_WIDTH - bmp_width) / 2;
  int y = (SCREEN_HEIGHT - bmp_height) / 2;

  display.clearDisplay();

  for (int w = 0; w <= bmp_width; w++) {
    if (wipe_in) {
      // Draw progressively wider slices of the bitmap
      display.drawBitmap(x, y, bitmap, w, bmp_height, WHITE);
    } else {
      // Erase by drawing black over the area
      display.drawBitmap(x, y, bitmap, bmp_width, bmp_height, WHITE); // full image
      display.fillRect(x, y, w, bmp_height, BLACK); // mask from left
    }

    display.display();
    delay(delay_ms);
  }
}

void drawBatteryIcon() {
  // Battery icon (top left)
  int batteryX = 0;
  int batteryY = 0;
  int batteryW = 20;
  int batteryH = 10;
  int fillW = (int)((batteryPercent / 100.0) * (batteryW - 4));
  display.drawRect(batteryX, batteryY, batteryW, batteryH, WHITE); // battery outline
  display.drawRect(batteryX + batteryW, batteryY + 3, 2, 4, WHITE); // battery nub
  display.fillRect(batteryX + 2, batteryY + 2, fillW, batteryH - 4, WHITE); // battery fill

  if (batteryPercent < 15) {
    display.setTextSize(1);
    display.setTextColor(WHITE);
    display.setCursor(batteryX + batteryW + 5, batteryY);
    display.print("!"); // exclamation mark
  }
}

void drawConnectionIcon() {
  // Connection icon (top right)
  int connX = SCREEN_WIDTH - 16;
  int connY = 2;

  // Airwave cone (3 arcs)
  display.drawPixel(connX, connY + 7, WHITE);
  display.drawCircle(connX, connY + 7, 3, WHITE);
  display.drawCircle(connX, connY + 7, 6, WHITE);

  if (!isConnected) 
    display.drawLine(connX - 8, connY, connX + 8, connY + 14, WHITE); // diagonal slash
}

void drawButtonHints(const char* left, const char* down, const char* up, const char* right) {
  int iconW = 32;
  int iconY = SCREEN_HEIGHT - 10;
  display.setTextSize(1);
  display.setTextColor(WHITE);
  display.setCursor(2, iconY);
  display.print(left);
  display.setCursor(34, iconY);
  display.print(down);
  display.setCursor(66, iconY);
  display.print(up);
  display.setCursor(98, iconY);
  display.print(right);
}

void drawButtonHints(const uint8_t *left, const uint8_t *down, const uint8_t *up, const uint8_t *right) {
  int iconW = 32;
  int iconY = 6;
  
  display.drawBitmap(2, SCREEN_HEIGHT - iconY, left, 6, 6, WHITE);
  display.drawBitmap(34, SCREEN_HEIGHT - iconY, up, 6, 6, WHITE);
  display.drawBitmap(66, SCREEN_HEIGHT - iconY, down, 6, 6, WHITE);
  display.drawBitmap(98, SCREEN_HEIGHT - iconY, right, 6, 6, WHITE);
}

void drawCompressorVentIcons() {
  int centerX = (SCREEN_WIDTH - 44) / 2;
  int centerY = (SCREEN_HEIGHT - 17) / 2;
  if (isCompressorOn) {
    display.drawBitmap(centerX, centerY, icon_compressor, 20, 17, WHITE);
  }
  if (isVentOpen) {
    display.drawBitmap(centerX + 24, centerY, icon_vent, 20, 17, WHITE);
  }
}

void drawDisconnectedScreen() {
  display.clearDisplay();
  drawBatteryIcon();

  // Centered text
  display.setTextSize(2);
  display.setTextColor(WHITE);
  int16_t x, y;
  uint16_t w, h;
  if (!isConnected) {
    display.getTextBounds("Disconnected", 0, 0, &x, &y, &w, &h);
    display.setCursor((SCREEN_WIDTH - w) / 2, (SCREEN_HEIGHT - h) / 2);
    display.print("Disconnected");
    // Button hint: right arrow
    drawButtonHints("", "", "", icon_arrow_right);
  } else {
    display.getTextBounds("Connecting...", 0, 0, &x, &y, &w, &h);
    display.setCursor((SCREEN_WIDTH - w) / 2, (SCREEN_HEIGHT - h) / 2);
    display.print("Connecting...");
    drawButtonHints("", "", "", "");
  }
  display.display();
}
#pragma endregion


#pragma region Communications
void sendStartCommand(float targetPSI) {
  StaticJsonDocument<JSON_SEND_SIZE> doc;
  doc["cmd"] = "start";
  doc["target_psi"] = targetPSI;

  char payload[JSON_SEND_SIZE];
  serializeJson(doc, payload);

  queueMessage(doc);
}

void sendCancelCommand() {
  StaticJsonDocument<JSON_SEND_SIZE> doc;
  doc["cmd"] = "cancel";

  char payload[JSON_SEND_SIZE];
  serializeJson(doc, payload);

  queueMessage(doc);
}

void onReceiveData(const uint8_t *mac, const uint8_t *incomingData, int len) {
  lastPacketReceivedTime = millis(); // update timestamp

  StaticJsonDocument<JSON_RECV_SIZE> doc;
  DeserializationError error = deserializeJson(doc, incomingData, len);

  if (error) {
    Serial.println("JSON decode failed");
    return;
  }

  if (doc.containsKey("psi")) {
    latestPSI = doc["psi"];
  }

  if (doc.containsKey("comp")) {
    isCompressorOn = doc["comp"];
  }

  if (doc.containsKey("vent")) {
    isVentOpen = doc["vent"];
  }

  if (doc.containsKey("status")) {
    strncpy(lastStatus, doc["status"], sizeof(lastStatus));
    isConnected = true;
    Serial.printf("Received status: %s\n", lastStatus);
  }
}

void onDataSent(const uint8_t *mac_addr, esp_now_send_status_t status) {
  Serial.printf("Last Packet Send Status: %s\n", status == ESP_NOW_SEND_SUCCESS ? "Success" : "Fail");
}

void queueMessage(const JsonDocument& doc) {
  String payload;
  serializeJson(doc, payload);

  QueuedMessage msg = {
    .payload = payload,
    .retriesLeft = MAX_RETRIES,
    .lastSentTime = 0
  };

  messageQueue.push_back(msg);
  Serial.println("Message queued:");
  Serial.println(payload);
}

void processMessageQueue() {
  if (messageQueue.empty()) return;

  QueuedMessage& msg = messageQueue.front();
  unsigned long now = millis();

  if (now - msg.lastSentTime < RESEND_INTERVAL) return;

  esp_err_t result = esp_now_send(controlBoardAddress, (uint8_t*)msg.payload.c_str(), msg.payload.length());
  msg.lastSentTime = now;

  if (result == ESP_OK) {
    Serial.println("Message sent successfully:");
    Serial.println(msg.payload);
    messageQueue.pop_front();
  } else {
    Serial.println("Send failed. Will retry...");
    msg.retriesLeft--;
    if (msg.retriesLeft == 0) {
      Serial.println("Max retries reached. Dropping message:");
      Serial.println(msg.payload);
      messageQueue.pop_front();
    }
  }
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