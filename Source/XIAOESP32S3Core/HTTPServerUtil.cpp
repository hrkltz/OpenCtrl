#include "HTTPServerUtil.hpp"
#include "HIDTransportBackend.hpp"
#include "HIDUtil.hpp"
#include "DisplayUtil.hpp"
#include <WiFi.h>
#include <USBCDC.h>
#include <esp_random.h>
#include <freertos/semphr.h>
#include <cctype>

namespace {
AsyncWebServer server(80);
char token[3] = {};
String owner;
uint32_t lastSeen = 0;
constexpr uint32_t sessionTimeout = 2000;
SemaphoreHandle_t sessionMutex = nullptr;

void respond(AsyncWebServerRequest* request, int status) {
    const char* message = status == 204 ? "" : status == 200 ? "OK\n" : status == 400 ? "Invalid report\n" :
        status == 401 ? "Pair with the controller first\n" : status == 409 ? "Controller is in use\n" :
        status == 404 ? "HID profile unavailable\n" : "HID transport unavailable; reconnect and try again\n";
    auto* response = request->beginResponse(status, "text/plain", message);
    response->addHeader("Access-Control-Allow-Origin", "*");
    response->addHeader("Access-Control-Allow-Methods", "GET, POST, OPTIONS");
    response->addHeader("Access-Control-Allow-Headers", "Content-Type, Authorization, X-OpenCtrl-Session");
    response->addHeader("Cache-Control", "no-store");
    request->send(response);
}

bool authenticated(AsyncWebServerRequest* request) {
    const String auth = request->header("Authorization");
    const size_t tokenLength = strlen(token);
    if (auth.length() != 7 + tokenLength || !auth.startsWith("Bearer ") || !tokenLength) return false;
    uint8_t difference = 0;
    for (size_t i = 0; i < tokenLength; ++i) difference |= auth[i + 7] ^ token[i];
    return difference == 0;
}

void expireSession() {
    if (owner.length() && (uint32_t(millis() - lastSeen) >= sessionTimeout ||
                           WiFi.status() != WL_CONNECTED || !HIDUtil::Connected())) {
        HIDUtil::ReleaseAll();
        owner = "";
    }
}

int acquire(AsyncWebServerRequest* request) {
    if (!authenticated(request)) return 401;
    const String session = request->header("X-OpenCtrl-Session");
    if (session.length() != 32) return 400;
    for (char c : session) if (!isxdigit(static_cast<unsigned char>(c))) return 400;
    expireSession();
    if (owner.length() && owner != session) return 409;
    if (!owner.length()) {
        if (!HIDUtil::ReleaseAll()) return 503;
        owner = session;
    }
    lastSeen = millis();
    return 200;
}

void service(void*) {
    bool consoleConnected = false;
    for (;;) {
        xSemaphoreTake(sessionMutex, portMAX_DELAY);
        expireSession();
        HIDUtil::Service();
        auto& pairingConsole = HIDUSBConsole();
        const bool connected = bool(pairingConsole);
        if (connected && !consoleConnected) {
            pairingConsole.print("OpenCtrl pairing code: ");
            pairingConsole.println(token);
        }
        consoleConnected = connected;
        xSemaphoreGive(sessionMutex);
        vTaskDelay(pdMS_TO_TICKS(2));
    }
}
}

AsyncWebServer& HTTPServerUtil::GetServer() { return server; }
const char* HTTPServerUtil::GetPairingToken() { return token; }

bool HTTPServerUtil::ReadInt(JsonObject doc, const char* key, int minimum, int maximum,
                             int& value, int fallback) {
    if (doc[key].isUnbound()) { value = fallback; return true; }
    if (!doc[key].is<int>()) return false;
    value = doc[key].as<int>();
    return value >= minimum && value <= maximum;
}
bool HTTPServerUtil::ReadBool(JsonObject doc, const char* key, bool& value) {
    if (doc[key].isUnbound()) { value = false; return true; }
    if (!doc[key].is<bool>()) return false;
    value = doc[key].as<bool>();
    return true;
}
bool HTTPServerUtil::ReadButtons(JsonObject doc, uint32_t& value) {
    if (doc["buttons"].isUnbound()) { value = 0; return true; }
    if (!doc["buttons"].is<uint32_t>()) return false;
    value = doc["buttons"].as<uint32_t>();
    return true;
}

void HTTPServerUtil::AddJsonRoute(const char* uri, std::function<int(JsonObject)> handler) {
    server.on(uri, HTTP_POST, [handler](AsyncWebServerRequest* request, JsonVariant& json) {
        xSemaphoreTake(sessionMutex, portMAX_DELAY);
        int status = acquire(request);
        if (status == 200) status = json.is<JsonObject>() ? handler(json.as<JsonObject>()) : 400;
        xSemaphoreGive(sessionMutex);
        respond(request, status);
    });
    server.on(uri, HTTP_OPTIONS, [](AsyncWebServerRequest* request) { respond(request, 204); });
}

void HTTPServerUtil::AddMouseRoute(const char* uri, uint8_t reportId) {
    AddJsonRoute(uri, [reportId](JsonObject doc) {
        int dx, dy, wheel, pan, buttons;
        if (!ReadInt(doc, "dx", -4096, 4096, dx) || !ReadInt(doc, "dy", -4096, 4096, dy) ||
            !ReadInt(doc, "wheel", -127, 127, wheel) || !ReadInt(doc, "pan", -127, 127, pan) ||
            !ReadInt(doc, "buttons", 0, 31, buttons)) return 400;
        return HIDUtil::SendMouse(dx, dy, wheel, pan, buttons, reportId) ? 200 : 503;
    });
}

bool HTTPServerUtil::Init() {
    if (sessionMutex) return false;
    sessionMutex = xSemaphoreCreateMutex();
    if (!sessionMutex) return false;
    snprintf(token, sizeof(token), "%u", static_cast<unsigned>(esp_random() % 100));
    HIDUSBConsole().begin(115200);
    Serial.print("OpenCtrl pairing code: ");
    Serial.println(token);
    DisplayUtil::Print(6, "Pairing code");
    DisplayUtil::Print(7, token);

    server.on("/", HTTP_GET, [](AsyncWebServerRequest* request) {
        request->send(200, "text/plain", "OpenCtrl: pair using the code on the display or USB serial console.\n");
    });
    AddJsonRoute("/heartbeat", [](JsonObject) { return HIDUtil::Ready() ? 200 : 503; });
    AddJsonRoute("/release", [](JsonObject) {
        const bool released = HIDUtil::ReleaseAll();
        owner = "";
        return released ? 200 : 503;
    });
    if (HIDUtil::InputSize(2)) AddMouseRoute("/mouse");
    if (HIDUtil::InputSize(1)) AddJsonRoute("/keyboard", [](JsonObject doc) {
        int modifiers;
        if (!ReadInt(doc, "modifiers", 0, 255, modifiers)) return 400;
        uint8_t keys[6] = {};
        if (!doc["keys"].isUnbound()) {
            if (!doc["keys"].is<JsonArray>()) return 400;
            JsonArray array = doc["keys"];
            if (array.size() > 6) return 400;
            for (size_t i = 0; i < array.size(); ++i) {
                if (!array[i].is<unsigned int>() || array[i].as<unsigned int>() > 0x65) return 400;
                keys[i] = array[i].as<uint8_t>();
            }
        }
        return HIDUtil::SendKeyboard(modifiers, keys) ? 200 : 503;
    });
    if (HIDUtil::InputSize(3)) AddJsonRoute("/gamepad", [](JsonObject doc) {
        int x, y, z, rz, rx, ry, hat;
        uint32_t buttons;
        if (!ReadInt(doc, "x", -127, 127, x) || !ReadInt(doc, "y", -127, 127, y) ||
            !ReadInt(doc, "z", -127, 127, z, -127) || !ReadInt(doc, "rz", -127, 127, rz, -127) ||
            !ReadInt(doc, "rx", -127, 127, rx) || !ReadInt(doc, "ry", -127, 127, ry) ||
            !ReadInt(doc, "hat", 0, 8, hat) || !ReadButtons(doc, buttons)) return 400;
        return HIDUtil::SendGamepad(x, y, z, rz, rx, ry, hat, buttons) ? 200 : 503;
    });
    if (HIDUtil::InputSize(4)) AddJsonRoute("/consumer", [](JsonObject doc) {
        int usage;
        if (!ReadInt(doc, "usage", 0, 0x3ff, usage)) return 400;
        return HIDUtil::SendConsumer(usage) ? 200 : 503;
    });
    if (HIDUtil::InputSize(5)) AddJsonRoute("/system", [](JsonObject doc) {
        int usage;
        if (!ReadInt(doc, "usage", 0, 3, usage)) return 400;
        return HIDUtil::SendSystem(usage) ? 200 : 503;
    });
    AddJsonRoute("/hid/report", [](JsonObject doc) {
        int id;
        if (!ReadInt(doc, "id", 1, 255, id) || !id || !doc["data"].is<JsonArray>()) return 400;
        const size_t length = HIDUtil::InputSize(id);
        if (!length) return 404;
        JsonArray data = doc["data"];
        if (data.size() != length) return 400;
        std::vector<uint8_t> bytes;
        for (JsonVariant value : data) {
            if (!value.is<unsigned int>() || value.as<unsigned int>() > 255) return 400;
            bytes.push_back(value.as<uint8_t>());
        }
        HIDTransport target = HIDUtil::Transports(id);
        if (!doc["transport"].isUnbound()) {
            if (!doc["transport"].is<const char*>()) return 400;
            const String name = doc["transport"].as<const char*>();
            if (name == "usb") target = HIDTransport::USB;
            else if (name == "bluetooth") target = HIDTransport::Bluetooth;
            else return 400;
            if (!(uint8_t(target) & uint8_t(HIDUtil::Transports(id)))) return 400;
        }
        return HIDUtil::SendInputTo(id, bytes.data(), bytes.size(), target) ? 200 : 503;
    });
    if (xTaskCreate(service, "openctrl-hid", 4096, nullptr, 1, nullptr) != pdPASS) return false;
    server.begin();
    return true;
}
