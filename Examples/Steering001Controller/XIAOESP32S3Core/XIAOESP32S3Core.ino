#include "DisplayUtil.hpp"
#include "HIDUtil.hpp"
#include "HTTPServerUtil.hpp"
#include "TMAG5273Util.hpp"
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
  if(!TMAG5273Util::Init()) {
    DisplayUtil::PrintError("TMAG5273 Init Failed");
    return;
  }
}

void loop() {
  // Empty loop - all handled via web server
   TMAG5273Data tmag5273Data = TMAG5273Util::Read();

   DisplayUtil::Print(4, ("MagX: " + String(tmag5273Data.magX)).c_str());
   DisplayUtil::Print(5, ("MagY: " + String(tmag5273Data.magY)).c_str());
   DisplayUtil::Print(6, ("MagZ: " + String(tmag5273Data.magZ)).c_str());
}