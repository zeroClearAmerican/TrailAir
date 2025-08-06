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

  // Invert and restore display, pausing in-between
  display.invertDisplay(true);
  delay(500);
  display.invertDisplay(false);
  delay(500);

  display.clearDisplay();
  display.display();

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

  // 1. Get values from control board (ESP-NOW, async)
  
  // 2. Read buttons
  updateButtons();

  // 3. Update State Machine
  updateConnectionStatus();
  updateStateMachine();

  // 4. Handle display updates

  // 5. Send data to control board (ESP-NOW)
  processMessageQueue();
}


void updateButtons() {
  btnLeft.loop();
  btnDown.loop();
  btnUp.loop();
  btnRight.loop();

  btnLeftPressed  = btnLeft.isPressed();
  btnDownPressed  = btnDown.isPressed();
  btnUpPressed    = btnUp.isPressed();
  btnRightPressed = btnRight.isPressed();

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
      // Initialization done, move to disconnected to wait for communication
      enterState(DISCONNECTED);
      break;

    case DISCONNECTED:
      // Wait for user to press retry button (BTN1) to reconnect
      // If reconnect is successful, transition to IDLE
      if (isConnected) {
        enterState(IDLE);
      }
      break;

    case IDLE:
      // Allow user to set PSI and send command
      // If a command is queued (e.g., start), enter SEEKING
      // If connection is lost, go back to DISCONNECTED
      if (!isConnected) {
        enterState(DISCONNECTED);
      }
      break;

    case SEEKING:
      // Waiting for target PSI to be reached
      // Cancelable by user
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
      if (millis() - stateEntryTime > 3000) { // auto-clear after 3s
        enterState(IDLE);
      }
      break;
  }
}


void goToDeepSleep() {
  Serial.println("Entering deep sleep...");
  
  // GPIO10 = ext0 wake source (must be RTC_GPIO capable)
  esp_sleep_enable_ext0_wakeup(GPIO_NUM_10, 0);  // Wake on LOW signal
  esp_deep_sleep_start();
}


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
