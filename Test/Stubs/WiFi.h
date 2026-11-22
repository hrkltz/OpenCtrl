#pragma once
#include "Arduino.h"
constexpr int WL_CONNECTED = 3;
constexpr int WIFI_AP = 2;
constexpr int WIFI_STA = 1;
struct MockIP { String toString() const { return "192.168.4.1"; } };
struct MockWiFi {
    int state = WL_CONNECTED;
    int selectedMode = 0;
    bool autoReconnect = false, stationStarted = false, accessPointStarted = false;
    int status() const { return state; }
    void mode(int value) { selectedMode = value; }
    void setAutoReconnect(bool value) { autoReconnect = value; }
    void begin(const char*, const char*) { stationStarted = true; }
    bool softAP(const char*, const char* password, int, bool, int maximum) { accessPointStarted = true; return strlen(password) >= 8 && maximum == 1; }
    MockIP softAPIP() const { return {}; }
    MockIP localIP() const { return {}; }
};
inline MockWiFi WiFi;
