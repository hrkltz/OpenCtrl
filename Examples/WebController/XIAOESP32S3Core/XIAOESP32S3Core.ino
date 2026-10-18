#include <ArduinoJson.h>
#include <DisplayUtil.hpp>
#include <HIDUtil.hpp>
#include <HTTPServerUtil.hpp>
#include <WiFiUtil.hpp>
#include "Profiles.hpp"
#include "WebFiles.hpp"

HIDPen pen;

static void sendWebFile(AsyncWebServerRequest* request, const WebFile& file) {
  AsyncWebServerResponse* response = request->beginResponse(200, file.contentType, file.data, file.size);
  response->addHeader("Cache-Control", "no-cache");
  request->send(response);
}

void setup() {
  Serial.begin(115200);
  delay(300);

  DisplayUtil::Init();
  if (!WebController::InitProfiles(pen)) {
    DisplayUtil::PrintError("HID Init Failed");
    return;
  }
  WiFiUtil::Connect();

  // Routes registered before HTTPServerUtil::Init() take precedence over the core routes.
  AsyncWebServer& server = HTTPServerUtil::GetServer();
  server.on("/controller", HTTP_GET, [](AsyncWebServerRequest* request) {
    JsonDocument doc;
    doc["mode"] = WebConfig::name;
    doc["primaryMouse"] = WebConfig::primaryMouseName;
    doc["secondaryMouse"] = WebConfig::secondaryMouse;
    String body;
    serializeJson(doc, body);
    AsyncWebServerResponse* response = request->beginResponse(200, "application/json", body);
    response->addHeader("Cache-Control", "no-store");
    response->addHeader("Access-Control-Allow-Origin", "*");
    request->send(response);
  });
  if (WebConfig::secondaryMouse)
    HTTPServerUtil::AddMouseRoute("/mouse/secondary", WebConfig::secondaryMouseId);
  for (const WebFile& file : WEB_FILES) {
    server.on(file.path, HTTP_GET, [&file](AsyncWebServerRequest* request) {
      sendWebFile(request, file);
    });
    if (strcmp(file.path, "/index.html") == 0) {
      server.on("/", HTTP_GET, [&file](AsyncWebServerRequest* request) {
        sendWebFile(request, file);
      });
    }
  }

  HTTPServerUtil::AddJsonRoute("/pen", [](JsonObject doc) {
    bool tip, inRange, barrel, eraser;
    int x, y, pressure;
    if (!HTTPServerUtil::ReadBool(doc, "tip", tip) || !HTTPServerUtil::ReadBool(doc, "inRange", inRange) ||
        !HTTPServerUtil::ReadBool(doc, "barrel", barrel) || !HTTPServerUtil::ReadBool(doc, "eraser", eraser) ||
        !HTTPServerUtil::ReadInt(doc, "x", 0, 32767, x) || !HTTPServerUtil::ReadInt(doc, "y", 0, 32767, y) ||
        !HTTPServerUtil::ReadInt(doc, "pressure", 0, 1023, pressure)) return 400;
    return pen.send(tip, inRange, barrel, eraser, x, y, pressure) ? 200 : 503;
  });

  if (!HTTPServerUtil::Init()) DisplayUtil::PrintError("HTTP Init Failed");
}

void loop() {
  // All input is handled by the HTTP server.
  delay(1000);
}
