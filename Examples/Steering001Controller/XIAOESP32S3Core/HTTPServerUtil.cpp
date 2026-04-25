/**
 * @file HTTPServerUtil.cpp
 * @author Oliver Herklotz (oliver-hrkltz)
 * @date 17.01.2026
 */
#include "HIDUtil.hpp"
#include "HTTPServerUtil.hpp"
#include <Arduino.h>
#include <ArduinoJson.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>

static AsyncWebServer server(80);

// Helper to add CORS headers to every response
void addCORS(AsyncWebServerResponse* response) {
    response->addHeader("Access-Control-Allow-Origin", "*");
    response->addHeader("Access-Control-Allow-Methods", "GET, POST, OPTIONS");
    response->addHeader("Access-Control-Allow-Headers", "Content-Type");
}

// CORS preflight handler for all endpoints
auto corsOptionsHandler = [](AsyncWebServerRequest* request) {
    AsyncWebServerResponse* response = request->beginResponse(204);
    addCORS(response);
    request->send(response);
};

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
      AsyncWebServerResponse* response = request->beginResponse(200, "text/plain", "Mouse report received\n");
      addCORS(response);
      request->send(response);
    });
    server.on("/mouse", HTTP_OPTIONS, corsOptionsHandler);
    
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
      AsyncWebServerResponse* response = request->beginResponse(200, "text/plain", "Keyboard report received\n");
      addCORS(response);
      request->send(response);
    });
    server.on("/keyboard", HTTP_OPTIONS, corsOptionsHandler);

    // Handle gamepad report.
    // Expects JSON body like:
    // {
    //   "x": 0,
    //   "y": 0,
    //   "z": 0,
    //   "rz": 0,
    //   "rx": 0,
    //   "ry": 0,
    //   "hat": 0,
    //   "buttons": 0
    // }
    server.on("/gamepad", HTTP_POST, [](AsyncWebServerRequest *request, JsonVariant &json){ 
      if (json.is<JsonObject>()) {
        // Parse JSON body into C++ variables.
        JsonObject doc = json.as<JsonObject>();
        int x = doc["x"].as<int>();
        int y = doc["y"].as<int>();
        int z = doc["z"].as<int>();
        int rz = doc["rz"].as<int>();
        int rx = doc["rx"].as<int>();
        int ry = doc["ry"].as<int>();
        uint8_t hat = doc["hat"].as<uint8_t>();
        uint32_t buttons = doc["buttons"].as<uint32_t>();

        // Send gamepad report.
        //bool send(int8_t x, int8_t y, int8_t z, int8_t rz, int8_t rx, int8_t ry, uint8_t hat, uint32_t buttons);
        HIDUtil::SendGamepad(
          static_cast<int8_t>(x),
          static_cast<int8_t>(y),
          static_cast<int8_t>(z),
          static_cast<int8_t>(rz),
          static_cast<int8_t>(rx),
          static_cast<int8_t>(ry),
          hat,
          buttons
        );
      }
      AsyncWebServerResponse* response = request->beginResponse(200, "text/plain", "Gamepad report received\n");
      addCORS(response);
      request->send(response);
    });
    server.on("/gamepad", HTTP_OPTIONS, corsOptionsHandler);
    
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
