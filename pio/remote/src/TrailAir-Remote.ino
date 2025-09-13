#include <SPI.h>
#include <Wire.h>
#include <TA_Display.h>
#include <TA_DisplayIcons.h>
#include <TA_Battery.h>
#include <TA_Protocol.h>
#include <TA_Comms.h>
#include <TA_Input.h>
#include <TA_State.h>

#include <esp_sleep.h>

#include <WiFi.h>
#include <esp_wifi.h>

#pragma region Screen Variables
#define SCREEN_WIDTH 128 // OLED display width, in pixels
#define SCREEN_HEIGHT 32 // OLED display height, in pixels

// Declaration for an SSD1306 display connected to I2C (SDA, SCL pins)
#define OLED_RESET     -1 // Reset pin # (or -1 if sharing Arduino reset pin)
#define SCREEN_ADDRESS 0x3C ///< See datasheet for Address; 0x3D for 128x64, 0x3C for 128x32
Adafruit_SSD1306 d_(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);
ta::display::TA_Display ui(d_);

#pragma endregion

#pragma region Battery Variables
#define BATTERY_PIN    2 // A0, GPIO2
float batteryVoltage = 0.0;
int batteryPercent = 0;
ta::battery::TA_BatteryMonitor batteryMon;
#pragma endregion

#pragma region Button Variables
// GPIO pins for each button
#define BTN_LEFT_PIN   10  // retry / wake
#define BTN_DOWN_PIN   9
#define BTN_UP_PIN     8
#define BTN_RIGHT_PIN  20
static ta::input::Buttons buttons({ BTN_LEFT_PIN, BTN_DOWN_PIN, BTN_UP_PIN, BTN_RIGHT_PIN });

const unsigned long SLEEP_TIMEOUT = 300000; // 5 minutes
unsigned long lastButtonPressedTime = 0;

// Manual sleep logic variables
#pragma endregion 

#pragma region ESP-NOW Variables
// Replace with your control board MAC
uint8_t controlBoardAddress[] = { 0x34, 0x85, 0x18, 0x06, 0x55, 0x6C };
static ta::comms::EspNowLink espNowLink;

static ta::state::StateController state(espNowLink);

// Forward Comms status to state
static void onStatusFromBoard(void* /*ctx*/, const ta::protocol::StatusMsg& msg) {
  state.onStatus(msg);
}

static void onPairEvent(void* /*ctx*/, ta::comms::PairEvent ev, const uint8_t mac[6]) {
  state.onPairEvent(ev, mac);
}

#pragma endregion

#pragma region Setups
void setup() {
  Serial.begin(115200);

  batteryMon.begin(BATTERY_PIN, ADC_11db);
  setupScreen();
  
  // Buttons -> state
  buttons.begin(
    [](void* /*ctx*/, const ta::input::Event& e) {
      // maintain existing inactivity timer
      lastButtonPressedTime = millis();
      state.onButton(e);
    },
    nullptr
  );

  setupWakeup();
  setupESPNOW();
  state.begin();

  delay(500);
  // Wipe out logo before beginning loop
  ui.logoWipe(ta::display::Icons::logo_bmp, 
    ta::display::Icons::LogoW, 
    ta::display::Icons::LogoH, 
    false, 
    5);
}

void setupScreen() {
  // SSD1306_SWITCHCAPVCC = generate display voltage from 3.3V internally
  if(!ui.begin(SCREEN_ADDRESS, true)) {
    Serial.println(F("SSD1306 OLED screen allocation failed"));
    for(;;); // Don't proceed, loop forever
  }
}

void setupWakeup() {
  gpio_wakeup_enable(GPIO_NUM_10, GPIO_INTR_LOW_LEVEL);
  esp_err_t result = esp_sleep_enable_gpio_wakeup();

  if (result == ESP_OK) {
    Serial.println("GPIO Wake-Up set successfully.");
  } else {
    Serial.println("Failed to set GPIO Wake-Up as wake-up source.");
  }
}

void setupESPNOW() {
  // Initialize link and peer
  if (!espNowLink.begin(nullptr)) {
    Serial.println("ESP-NOW init failed");
    return;
  }
  espNowLink.setConnectionTimeoutMs(5000);
  espNowLink.setStatusCallback(&onStatusFromBoard, nullptr);
  espNowLink.setPairCallback(&onPairEvent, nullptr); // NEW
  Serial.println("ESP-NOW initialized");
}
#pragma endregion


void loop() {
  // Read buttons
  buttons.service();
  if (millis() - lastButtonPressedTime > SLEEP_TIMEOUT) {
    Serial.println("Sleep timeout exceeded.");
    goToSleep();
  }

  // Check battery voltage
  batteryMon.update();
  batteryVoltage = batteryMon.voltage();
  batteryPercent = batteryMon.percent();
  state.onBatteryPercent(batteryPercent);
  // Serial.printf("Battery: %.2f V (%d%%)\n", batteryVoltage, batteryPercent);

  // Comms service (timeout + reconnect pings)
  espNowLink.service();
  bool isConn = espNowLink.isConnected();
  bool isConnIng = espNowLink.isConnecting();

  // State + UI
  state.update(millis(), isConn, isConnIng);

  if (state.takeSleepRequest()) {
    goToSleep();
  }

  ta::display::DisplayModel dm;
  state.buildDisplayModel(dm);
  ui.render(dm); 

  delay(10);
}

#pragma region Button Functions
  
void goToSleep() {
  Serial.println("Entering light sleep...");

  // Always send a cancel before sleeping; safe in any state
  espNowLink.sendCancel();

  ui.drawLogo(ta::display::Icons::logo_bmp, 
    ta::display::Icons::LogoW, 
    ta::display::Icons::LogoH);
  delay(1000);
  ui.logoWipe(ta::display::Icons::logo_bmp, 
    ta::display::Icons::LogoW, 
    ta::display::Icons::LogoH, 
    false, 
    5);
  
  WiFi.disconnect();
  WiFi.mode(WIFI_OFF);
  esp_wifi_stop();
  esp_light_sleep_start();

  Serial.println("Woke up from sleep.");
  setupESPNOW();
  state.resetAfterWake();
  if (espNowLink.hasPeer()) {
    espNowLink.requestReconnect();
  }
}
#pragma endregion


#pragma region Communications

void sendStartCommand(float targetPSI) {
  espNowLink.sendStart(targetPSI);
}
void sendCancelCommand() {
  espNowLink.sendCancel();
}
void sendManualCommand(uint8_t code) {
  espNowLink.sendManual(code);
}
void requestReconnectLight() {
  espNowLink.requestReconnect();
}

#pragma endregion