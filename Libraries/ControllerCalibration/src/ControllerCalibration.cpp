#include "ControllerCalibration.hpp"
#include "CalibrationPage.hpp"
#include <WiFi.h>
#include <esp_random.h>
#include <algorithm>
#include <cstring>

namespace {
class Lock {
    SemaphoreHandle_t mutex;
public:
    explicit Lock(SemaphoreHandle_t mutex) : mutex(mutex) { xSemaphoreTake(mutex, portMAX_DELAY); }
    ~Lock() { xSemaphoreGive(mutex); }
};
void reply(AsyncWebServerRequest* request, int status, const char* message) {
    auto* response = request->beginResponse(status, "text/plain", message);
    response->addHeader("Cache-Control", "no-store");
    request->send(response);
}
bool same(const M5Stack::AxisCalibration& a, const M5Stack::AxisCalibration& b) {
    return a.minimum == b.minimum && a.center == b.center && a.maximum == b.maximum &&
        a.deadzone == b.deadzone && a.inverted == b.inverted;
}
}

bool ControllerCalibration::Configure(const char* name, std::vector<CalibrationAxis> bindings) {
    if (mutex || !name || !name[0] || strlen(name) > 15 || bindings.empty() || bindings.size() > 16) return false;
    for (size_t i = 0; i < bindings.size(); ++i) {
        const auto& axis = bindings[i];
        if (!axis.id || !axis.label || !axis.settings || !axis.settings->Valid() ||
            axis.settings->minimum < axis.rawMinimum || axis.settings->maximum > axis.rawMaximum) return false;
        for (size_t j = 0; j < i; ++j) if (strcmp(bindings[j].id, axis.id) == 0) return false;
    }
    mutex = xSemaphoreCreateMutex();
    if (!mutex) return false;
    axes = std::move(bindings);
    readings.resize(axes.size());
    defaults = Current();
    storage = preferences.begin(name);
    return true;
}
std::vector<M5Stack::AxisCalibration> ControllerCalibration::Current() const {
    std::vector<M5Stack::AxisCalibration> values;
    for (const auto& axis : axes) values.push_back(*axis.settings);
    return values;
}
void ControllerCalibration::Apply(const std::vector<M5Stack::AxisCalibration>& values) {
    for (size_t i = 0; i < axes.size(); ++i) *axes[i].settings = values[i];
}
bool ControllerCalibration::Decode(JsonVariantConst body, std::vector<M5Stack::AxisCalibration>& values) const {
    if (!body.is<JsonObjectConst>() || !body["version"].is<unsigned>() || body["version"].as<unsigned>() != 1 ||
        !body["axes"].is<JsonArrayConst>() || body["axes"].size() != axes.size()) return false;
    values.clear();
    for (size_t i = 0; i < axes.size(); ++i) {
        const auto item = body["axes"][i];
        if (!item["id"].is<const char*>() || strcmp(item["id"].as<const char*>(), axes[i].id) != 0 ||
            !item["minimum"].is<int32_t>() || !item["center"].is<int32_t>() || !item["maximum"].is<int32_t>() ||
            !item["deadzone"].is<float>() || !item["inverted"].is<bool>()) return false;
        M5Stack::AxisCalibration value{item["minimum"].as<int32_t>(), item["center"].as<int32_t>(),
            item["maximum"].as<int32_t>(), item["deadzone"].as<float>(), item["inverted"].as<bool>()};
        if (!value.Valid() || value.minimum < axes[i].rawMinimum || value.maximum > axes[i].rawMaximum) return false;
        values.push_back(value);
    }
    return true;
}
String ControllerCalibration::Encode(const std::vector<M5Stack::AxisCalibration>& values) const {
    JsonDocument doc;
    doc["version"] = 1;
    auto list = doc["axes"].to<JsonArray>();
    for (size_t i = 0; i < axes.size(); ++i) {
        auto item = list.add<JsonObject>();
        item["id"] = axes[i].id;
        item["minimum"] = values[i].minimum; item["center"] = values[i].center; item["maximum"] = values[i].maximum;
        item["deadzone"] = values[i].deadzone; item["inverted"] = values[i].inverted;
    }
    String json;
    serializeJson(doc, json);
    return json;
}
bool ControllerCalibration::Load() {
    if (!mutex || running || !storage) return false;
    const String json = preferences.getString("settings", "");
    JsonDocument doc;
    std::vector<M5Stack::AxisCalibration> values;
    if (!json.length() || json.length() > 8192 || deserializeJson(doc, json.c_str()) || !Decode(doc.as<JsonVariantConst>(), values)) return false;
    Apply(values);
    saved = values;
    message = "Loaded saved calibration";
    return true;
}
bool ControllerCalibration::Begin(const char* prefix, const char* controllerTitle) {
    if (!mutex || running || !prefix || strlen(prefix) > 16 || !controllerTitle) return false;
    title = controllerTitle;
    snprintf(ssid, sizeof(ssid), "%s-%04lx", prefix, static_cast<unsigned long>(esp_random() & 0xffff));
    snprintf(password, sizeof(password), "%08lx%08lx", static_cast<unsigned long>(esp_random()), static_cast<unsigned long>(esp_random()));
    WiFi.mode(WIFI_AP);
    if (!WiFi.softAP(ssid, password, 1, false, 1)) return false;
    server.on("/", HTTP_GET, [](AsyncWebServerRequest* request) {
        auto* response = request->beginResponse(200, "text/html", CALIBRATION_PAGE);
        response->addHeader("Cache-Control", "no-store");
        request->send(response);
    });
    server.on("/api/calibration", HTTP_GET, [this](AsyncWebServerRequest* request) { State(request); });
    server.on("/api/calibration/start", HTTP_POST, [this](AsyncWebServerRequest* request) { Queue(request, Action::Start); });
    server.on("/api/calibration/end", HTTP_POST, [this](AsyncWebServerRequest* request) { Queue(request, Action::End); });
    server.on("/api/calibration/capture", HTTP_POST, [this](AsyncWebServerRequest* request) { Queue(request, Action::Capture); });
    server.on("/api/calibration/stop-capture", HTTP_POST, [this](AsyncWebServerRequest* request) { Queue(request, Action::StopCapture); });
    server.on("/api/calibration/reset", HTTP_POST, [this](AsyncWebServerRequest* request) { Queue(request, Action::Reset); });
    server.on("/api/calibration/preview", HTTP_POST, [this](AsyncWebServerRequest* request, JsonVariant& body) { Queue(request, Action::Preview, body); });
    server.on("/api/calibration/save", HTTP_POST, [this](AsyncWebServerRequest* request, JsonVariant& body) { Queue(request, Action::Save, body); });
    server.begin();
    running = true;
    return true;
}
String ControllerCalibration::Address() const { return WiFi.softAPIP().toString(); }
void ControllerCalibration::UpdateRaw(size_t index, int32_t raw, bool online) {
    if (!mutex || index >= readings.size()) return;
    Lock lock(mutex);
    auto& reading = readings[index];
    reading.raw = raw;
    reading.online = online && raw >= axes[index].rawMinimum && raw <= axes[index].rawMaximum;
    if (active && capturing && reading.online) {
        reading.observedMinimum = reading.observed ? std::min(raw, reading.observedMinimum) : raw;
        reading.observedMaximum = reading.observed ? std::max(raw, reading.observedMaximum) : raw;
        reading.observed = true;
    }
}
void ControllerCalibration::State(AsyncWebServerRequest* request) {
    JsonDocument doc;
    {
        Lock lock(mutex);
        if (active) lastSeen = millis();
        const auto values = Current();
        doc["title"] = title; doc["active"] = active; doc["storage"] = storage;
        doc["capturing"] = capturing;
        doc["saved"] = values.size() == saved.size() && std::equal(values.begin(), values.end(), saved.begin(), same);
        doc["message"] = message;
        doc["completedOperation"] = completedOperation; doc["operationStatus"] = operationStatus;
        doc["version"] = 1;
        auto list = doc["axes"].to<JsonArray>();
        for (size_t i = 0; i < axes.size(); ++i) {
            auto item = list.add<JsonObject>();
            item["id"] = axes[i].id; item["label"] = axes[i].label;
            item["minimum"] = values[i].minimum; item["center"] = values[i].center; item["maximum"] = values[i].maximum;
            item["deadzone"] = values[i].deadzone; item["inverted"] = values[i].inverted;
            item["raw"] = readings[i].raw; item["online"] = readings[i].online;
            item["observedMinimum"] = readings[i].observedMinimum;
            item["observedMaximum"] = readings[i].observedMaximum;
            item["observed"] = readings[i].observed;
            item["value"] = readings[i].online ? values[i].Normalize(readings[i].raw) : 0;
        }
    }
    String json;
    serializeJson(doc, json);
    auto* response = request->beginResponse(200, "application/json", json);
    response->addHeader("Cache-Control", "no-store");
    request->send(response);
}
void ControllerCalibration::Queue(AsyncWebServerRequest* request, Action action, JsonVariantConst body) {
    if (request->header("X-OpenCtrl-Calibration") != "1") { reply(request, 403, "Use the calibration page"); return; }
    std::vector<M5Stack::AxisCalibration> values;
    if ((action == Action::Save || action == Action::Preview) && !Decode(body, values)) {
        reply(request, 400, "Each axis needs minimum < center < maximum, a valid deadzone, and an inversion flag"); return;
    }
    uint32_t id;
    {
        Lock lock(mutex);
        if (pending.action != Action::None) { reply(request, 409, "Another calibration operation is pending"); return; }
        if (action != Action::Start && !active) { reply(request, 409, "Start calibration first"); return; }
        id = ++nextOperation;
        pending = {action, id, std::move(values)};
        lastSeen = millis();
    }
    JsonDocument doc;
    doc["operation"] = id;
    String json;
    serializeJson(doc, json);
    request->send(202, "application/json", json);
}
void ControllerCalibration::Service() {
    if (!mutex) return;
    Pending command;
    {
        Lock lock(mutex);
        if (active && uint32_t(millis() - lastSeen) >= 30000 && pending.action == Action::None) {
            Apply(baseline); active = false; capturing = false; message = "Calibration session expired; preview discarded";
        }
        command = pending;
    }
    if (command.action == Action::None) return;
    bool success = true;
    if (command.action == Action::Save || command.action == Action::Reset) {
        if (command.action == Action::Reset) command.values = defaults;
        const String json = Encode(command.values);
        success = storage && preferences.putString("settings", json) == json.length();
    }
    Lock lock(mutex);
    switch (command.action) {
        case Action::Start:
            if (!active) baseline = Current();
            active = true; message = "Calibration active; HID output paused"; break;
        case Action::Capture:
            capturing = true;
            for (auto& reading : readings) {
                reading.observed = reading.online;
                reading.observedMinimum = reading.observedMaximum = reading.raw;
            }
            message = "Move both sticks through their full travel, then finish capture"; break;
        case Action::StopCapture:
            capturing = false; message = "Range captured; release both sticks and set centers"; break;
        case Action::Preview:
            Apply(command.values); message = "Preview applied; not saved"; break;
        case Action::Save: case Action::Reset:
            if (success) {
                Apply(command.values); saved = command.values; baseline = command.values;
                message = command.action == Action::Reset ? "Defaults saved" : "Calibration saved";
            } else message = "Storage write failed; settings were not changed";
            break;
        case Action::End:
            Apply(baseline); active = false; capturing = false; message = "HID output resumed; unsaved preview discarded"; break;
        default: break;
    }
    completedOperation = command.id; operationStatus = success ? 200 : 503;
    lastSeen = millis(); pending = {};
}
