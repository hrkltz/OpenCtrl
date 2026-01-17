#include "HTTPServerUtil.hpp"
#include "HIDUtil.hpp"
#include <ArduinoJson.h>

static AsyncWebServer server(80);

void HTTPServerUtil::Init() {
    // Root endpoint.
    server.on("/", HTTP_GET, [](AsyncWebServerRequest* request) {
      AsyncWebServerResponse* response = request->beginResponse(200, "text/plain", "ESP32 Async Web Server OK\n");
      response->addHeader("Cache-Control", "no-store");
      request->send(response);
    });

    // Handle mouse report.
    // Expects JSON body like:
    // {
    //   "dx": 10,
    //   "dy": -5,
    //   "wheel": 0,
    //   "buttons": 1
    // }
    server.on("/mouse", HTTP_POST, [](AsyncWebServerRequest *request, JsonVariant &json) {
      if (json.is<JsonObject>()) {
        JsonObject doc = json.as<JsonObject>();
        int dx = doc["dx"] | 0;
        int dy = doc["dy"] | 0;
        int wheel = doc["wheel"] | 0;
        int buttons = doc["buttons"] | 0;
        HIDUtil::mouse.move(dx, dy, wheel, buttons);
      }
      request->send(200, "text/plain", "Mouse report received\n");
    });
    
    // Handle keyboard report.
    // Expects JSON body like:
    // {
    //   "modifiers": 2,
    //   "keys": [4, 5, 0, 0, 0, 0]
    // }
    server.on("/keyboard", HTTP_POST, [](AsyncWebServerRequest *request, JsonVariant &json) {
      if (json.is<JsonObject>()) {
        JsonObject doc = json.as<JsonObject>();
        int modifiers = doc["modifiers"] | 0;
        JsonArray keys = doc["keys"];
        KeyReport report = {0};
        report.modifiers = modifiers;
        for (size_t i = 0; i < keys.size() && i < 6; i++) {
            report.keys[i] = keys[i];
        }
        HIDUtil::keyboard.sendReport(&report);
        HIDUtil::keyboard.releaseAll();
      }
      request->send(200, "text/plain", "Keyboard report received\n");
    });

    // Handle gamepad report.
    // Expects JSON body like:
    // {
    //   "buttons": 1,
    //   "leftX": 0,
    //   "leftY": 0,
    //   "rightX": 0,
    //   "rightY": 0,
    //   "leftTrigger": 0,
    //   "rightTrigger": 0,
    //   "hat": 0
    // }
    server.on("/gamepad", HTTP_POST, [](AsyncWebServerRequest *request, JsonVariant &json){ 
      if (json.is<JsonObject>()) {
        JsonObject doc = json.as<JsonObject>();
        uint32_t buttons = doc["buttons"] | 0;
        int8_t leftX = doc["leftX"] | 0;
        int8_t leftY = doc["leftY"] | 0;
        int8_t rightX = doc["rightX"] | 0;
        int8_t rightY = doc["rightY"] | 0;
        int8_t leftTrigger = doc["leftTrigger"] | 0;
        int8_t rightTrigger = doc["rightTrigger"] | 0;
        uint8_t hat = doc["hat"] | 0;
        HIDUtil::gamepad.send(leftX, leftY, rightX, rightY, leftTrigger, rightTrigger, hat, buttons);
      }
      request->send(200, "text/plain", "Gamepad report received\n");
    });
    
    //TODO: // Handle pen report
    //TODO: server.on("/pen", HTTP_POST, [](AsyncWebServerRequest *request, JsonVariant &json){
    //TODO:   if (json.is<JsonObject>()) {
    //TODO:     JsonObject doc = json.as<JsonObject>();
    //TODO:     bool tip = doc["tip"] | false;
    //TODO:     bool inRange = doc["inRange"] | false;
    //TODO:     int x = doc["x"] | 0;
    //TODO:     int y = doc["y"] | 0;
    //TODO:     int pressure = doc["pressure"] | 0;
    //TODO:     // Send custom report (adjust based on descriptor)
    //TODO:     uint8_t report[8] = {inRange, x & 0xFF, (x >> 8) & 0xFF, y & 0xFF, (y >> 8) & 0xFF, pressure & 0xFF, (pressure >> 8) & 0xFF, tip};
    //TODO:     HIDUtil::penDevice.sendReport(0, report, sizeof(report));
    //TODO:   }
    //TODO:   request->send(200, "text/plain", "Pen report received\n");
    //TODO: });

    //TODO: // Handle touch report
    //TODO: server.on("/touch", HTTP_POST, [](AsyncWebServerRequest *request, JsonVariant &json) {
    //TODO:   if (json.is<JsonObject>()) {
    //TODO:     JsonObject doc = json.as<JsonObject>();
    //TODO:     bool tip = doc["tip"] | false;
    //TODO:     bool inRange = doc["inRange"] | false;
    //TODO:     int contactId = doc["contactId"] | 0;
    //TODO:     int x = doc["x"] | 0;
    //TODO:     int y = doc["y"] | 0;
    //TODO:     int contactCount = doc["contactCount"] | 0;
    //TODO:     // Send custom report (adjust based on descriptor)
    //TODO:     uint8_t report[8] = {contactId, x & 0xFF, (x >> 8) & 0xFF, y & 0xFF, (y >> 8) & 0xFF, contactCount, inRange, tip};
    //TODO:     HIDUtil::touchDevice.sendReport(0, report, sizeof(report));
    //TODO:   }
    //TODO:   request->send(200, "text/plain", "Touch report received\n");
    //TODO: });

    server.begin();
}
