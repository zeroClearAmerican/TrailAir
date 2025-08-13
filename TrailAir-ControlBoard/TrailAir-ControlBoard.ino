#include <WiFi.h>
#include <esp_wifi.h>
#include <esp_now.h>
#include <deque>
#include <ArduinoJson.h>

#pragma region ESP-NOW Variables

uint8_t remoteBoardAddress[] = { 0x34, 0x85, 0x18, 0x07, 0xd5, 0x2c };

#pragma endregion

#pragma region ESP-NOW Variables

// JSON buffer sizes (tweak if needed)
const size_t JSON_SEND_SIZE = 128;
const size_t JSON_RECV_SIZE = 64;

#pragma endregion

void setup() {

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
  memcpy(peerInfo.peer_addr, remoteBoardAddress, 6);
  peerInfo.channel = 0;
  peerInfo.encrypt = false;

  Serial.print("Registering peer: ");
  Serial.printf("%02x:%02x:%02x:%02x:%02x:%02x\n",
                  remoteBoardAddress[0], remoteBoardAddress[1],
                  remoteBoardAddress[2], remoteBoardAddress[3],
                  remoteBoardAddress[4], remoteBoardAddress[5]);

  // Check if peer already exists in ESP-NOW registry
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

}

void onReceiveData(const esp_now_recv_info_t *esp_now_info, const uint8_t *incomingData, int len) {
  StaticJsonDocument<JSON_RECV_SIZE> doc;
  DeserializationError error = deserializeJson(doc, incomingData, len);

  if (error) {
    Serial.println("JSON decode failed");
    return;
  }

  if (doc.containsKey("cmd")) {
    if (doc.containsKey("targetPSI")) {
      int targetPSI = doc["targetPSI"];
      Serial.printf("Received target PSI: %d\n", targetPSI);
      // Handle target PSI command
    } else {
      Serial.println("Unknown command received: " + String(doc["cmd"].as<const char*>()));
    }
  }
}

void onDataSent(const esp_now_send_info_t *tx_info, esp_now_send_status_t status) {
  Serial.printf("Last Packet Send Status: %s\n", status == ESP_NOW_SEND_SUCCESS ? "Success" : "Fail");
}