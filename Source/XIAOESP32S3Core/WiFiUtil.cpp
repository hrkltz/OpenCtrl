/**
 * @file WiFiUtil.cpp
 * @author Oliver Herklotz (oliver-hrkltz)
 * @date 15.10.2026
 */
#include "WiFiUtil.hpp"
#include "DisplayUtil.hpp"
#include <WiFi.h>

// Please define WIFI_SSID and WIFI_PASSWORD in your build environment.
#ifndef WIFI_SSID
  #define WIFI_SSID ""
#endif

#ifndef WIFI_PASSWORD
  #define WIFI_PASSWORD ""
#endif

void WiFiUtil::Connect() {
  static const char* connectingTexts[] = {"Connecting", "Connecting.", "Connecting..", "Connecting..."};

  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  for (uint8_t i = 0; WiFi.status() != WL_CONNECTED; i = (i + 1) % 4) {
    DisplayUtil::PrintConnectionStatus(connectingTexts[i]);
    delay(250);
  }
  DisplayUtil::PrintConnectionStatus("Connected");
  DisplayUtil::PrintIP(WiFi.localIP().toString().c_str());
}
