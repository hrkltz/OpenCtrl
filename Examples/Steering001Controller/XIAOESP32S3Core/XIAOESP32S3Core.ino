#include <DisplayUtil.hpp>
#include <HIDUtil.hpp>
#include <HTTPServerUtil.hpp>
#include <WiFiUtil.hpp>
#include "TMAG5273Util.hpp"

void setup() {
  Serial.begin(115200);
  delay(300);

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
  if(!TMAG5273Util::Init()) {
    DisplayUtil::PrintError("TMAG5273 Init Failed");
    return;
  }
}

void loop() {
   TMAG5273Data tmag5273Data = TMAG5273Util::Read();

   DisplayUtil::Print(4, ("MagX: " + String(tmag5273Data.magX)).c_str());
   DisplayUtil::Print(5, ("MagY: " + String(tmag5273Data.magY)).c_str());
   DisplayUtil::Print(6, ("MagZ: " + String(tmag5273Data.magZ)).c_str());
}
