#pragma once
// Mock WiFi.h for native testing

#define WIFI_STA 1

class WiFiClass {
public:
    static void mode(int m) {}
    static void disconnect() {}
};

extern WiFiClass WiFi;
