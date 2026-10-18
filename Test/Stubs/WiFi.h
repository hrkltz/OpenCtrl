#pragma once
#include "Arduino.h"
constexpr int WL_CONNECTED = 3;
constexpr int WIFI_AP = 2;
struct MockIP { String toString() const { return "192.168.4.1"; } };
struct MockWiFi {
    int state = WL_CONNECTED;
    int status() const { return state; }
    void mode(int) {}
    bool softAP(const char*, const char* password, int, bool, int maximum) { return strlen(password) >= 8 && maximum == 1; }
    MockIP softAPIP() const { return {}; }
};
inline MockWiFi WiFi;
