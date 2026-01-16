#include "HTTPServerUtil.hpp"
#include "DisplayUtil.hpp"
#include "HIDUtil.hpp"
#include <WiFi.h>

// --- WiFi Credentials ---
// Please define WIFI_SSID and WIFI_PASSWORD in your build environment.
#ifndef WIFI_SSID
  #define WIFI_SSID ""
#endif

#ifndef WIFI_PASSWORD
  #define WIFI_PASSWORD ""
#endif

const char* ssid = WIFI_SSID;
const char* pass = WIFI_PASSWORD;
// --- End WiFi Credentials ---

void setup() {
  Serial.begin(115200);
  delay(300);

  DisplayUtil::Init();

  // WiFi init.
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, pass);
  DisplayUtil::PrintConnectionStatus("Connecting");
  while (WiFi.status() != WL_CONNECTED) {
    DisplayUtil::PrintConnectionStatus("Connecting.");
    delay(250);
    DisplayUtil::PrintConnectionStatus("Connecting..");
    delay(250);
    DisplayUtil::PrintConnectionStatus("Connecting");
    delay(250);
  }
  DisplayUtil::PrintConnectionStatus("Connected");
  DisplayUtil::PrintIP(WiFi.localIP().toString().c_str());

  HTTPServerUtil::Init();
  HIDUtil::Init();
  DisplayUtil::PrintHeap(ESP.getFreeHeap());
}

void loop() {
  // Empty loop - all handled via web server
}