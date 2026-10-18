#include "DisplayUtil.hpp"
#include "HIDUtil.hpp"
#include "HTTPServerUtil.hpp"
#include "WiFiUtil.hpp"

void setup() {
  Serial.begin(115200);
  delay(300);

  // Register the HID profiles and start USB while WiFi connects.
  DisplayUtil::Init();
  if (!HIDUtil::Init()) {
    DisplayUtil::PrintError("HID Init Failed");
    return;
  }
  WiFiUtil::Connect();
  if (!HTTPServerUtil::Init()) {
    DisplayUtil::PrintError("HTTP Init Failed");
    return;
  }
}

void loop() {
  // All input is handled by the HTTP server.
  delay(1000);
}
