#include "ControllerCalibration.hpp"
#include <WiFi.h>
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
const char* filterName(ControllerFilterMode mode) {
    switch (mode) {
        case ControllerFilterMode::Off: return "off";
        case ControllerFilterMode::EMA: return "ema";
        default: return "median-3";
    }
}
bool decodeFilter(JsonVariantConst body, ControllerFilterSettings& settings) {
    if (!body.is<JsonObjectConst>() || !body["version"].is<unsigned>() || body["version"].as<unsigned>() != 1 ||
        !body["mode"].is<const char*>() || !body["alpha"].is<float>()) return false;
    const char* mode = body["mode"];
    if (strcmp(mode, "off") == 0) settings.mode = ControllerFilterMode::Off;
    else if (strcmp(mode, "median-3") == 0) settings.mode = ControllerFilterMode::Median3;
    else if (strcmp(mode, "ema") == 0) settings.mode = ControllerFilterMode::EMA;
    else return false;
    settings.alpha = body["alpha"];
    return settings.Valid();
}
const char* const modeNames[] = {"normal", "reversed", "positive", "negative"};
const char* const signalNames[] = {"axis", "button", "relative"};
const char* const outputNames[] = {"axis", "button", "motion"};
template <class T> size_t findId(const std::vector<T>& items, const char* id) {
    size_t i = 0;
    while (i < items.size() && strcmp(items[i].id, id) != 0) ++i;
    return i;
}
template <class T> bool uniqueIds(const std::vector<T>& items) {
    for (size_t i = 0; i < items.size(); ++i)
        if (!items[i].id || !items[i].label || findId(items, items[i].id) != i) return false;
    return true;
}
String encodeFilter(const ControllerFilterSettings& settings) {
    JsonDocument doc;
    doc["version"] = 1; doc["mode"] = filterName(settings.mode); doc["alpha"] = settings.alpha;
    String json;
    serializeJson(doc, json);
    return json;
}
}

bool ControllerCalibration::Configure(const char* name, std::vector<CalibrationAxis> bindings,
        ControllerFilterSettings* filterSettings, ControllerMapping controllerMapping) {
    if (mutex || !name || !name[0] || strlen(name) > 15 || bindings.empty() || bindings.size() > 32) return false;
    if (filterSettings && !filterSettings->Valid()) return false;
    if (!uniqueIds(bindings)) return false;
    for (const auto& axis : bindings)
        if (!axis.settings || !axis.settings->Valid() ||
            axis.settings->minimum < axis.rawMinimum || axis.settings->maximum > axis.rawMaximum) return false;
    if (!controllerMapping.sources.empty() || !controllerMapping.targets.empty() || controllerMapping.bindings) {
        const auto& m = controllerMapping;
        if (!m.bindings || m.sources.empty() || m.targets.empty() || m.sources.size() > 255 || m.targets.size() > 255 ||
            !m.maximum || m.maximum > 64 || m.bindings->size() > m.maximum ||
            !uniqueIds(m.sources) || !uniqueIds(m.targets) || !uniqueIds(m.units) || m.units.size() > 32 ||
            (m.ports && (m.units.empty() || m.ports->empty() || m.ports->size() > 255))) return false;
        if (m.ports) {
            for (uint8_t unit : *m.ports) if (unit >= m.units.size()) return false;
            for (const auto& source : m.sources) if (source.port >= m.ports->size()) return false;
        }
    }
    mapping = std::move(controllerMapping);
    if (mapping.bindings) {
        for (const auto& binding : *mapping.bindings)
            if (!Fits(binding, mapping.ports ? *mapping.ports : std::vector<uint8_t>{})) return false;
    }
    mutex = xSemaphoreCreateMutex();
    if (!mutex) return false;
    axes = std::move(bindings);
    filter = filterSettings;
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
    values = defaults;
    for (size_t i = 0; i < axes.size(); ++i) {
        const auto item = body["axes"][i];
        if (!item["id"].is<const char*>() || strcmp(item["id"].as<const char*>(), axes[i].id) != 0 ||
            !item["minimum"].is<int32_t>() || !item["center"].is<int32_t>() || !item["maximum"].is<int32_t>() ||
            !item["deadzone"].is<float>() || !item["inverted"].is<bool>()) return false;
        M5Stack::AxisCalibration value{item["minimum"].as<int32_t>(), item["center"].as<int32_t>(),
            item["maximum"].as<int32_t>(), item["deadzone"].as<float>(), item["inverted"].as<bool>()};
        if (!value.Valid() || value.minimum < axes[i].rawMinimum || value.maximum > axes[i].rawMaximum) return false;
        values[i] = value;
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
// A binding must fit its source, target, and the unit configured on the source's port.
bool ControllerCalibration::Fits(const ControllerBinding& binding, const std::vector<uint8_t>& ports) const {
    if (binding.source >= mapping.sources.size() || binding.target >= mapping.targets.size()) return false;
    const auto& source = mapping.sources[binding.source];
    if (mapping.ports && !(source.units & (1u << ports[source.port]))) return false;
    return ControllerBindingValid(source.signal, mapping.targets[binding.target].output, binding.mode);
}
bool ControllerCalibration::DecodeMapping(JsonVariantConst body, std::vector<uint8_t>& ports,
        std::vector<ControllerBinding>& values) const {
    if (!body.is<JsonObjectConst>() || !body["version"].is<unsigned>() || body["version"].as<unsigned>() != 1 ||
        !body["bindings"].is<JsonArrayConst>() || body["bindings"].size() > mapping.maximum) return false;
    ports.clear();
    if (mapping.ports) {
        if (!body["ports"].is<JsonArrayConst>() || body["ports"].size() != mapping.ports->size()) return false;
        for (const auto item : body["ports"].as<JsonArrayConst>()) {
            if (!item.is<const char*>()) return false;
            const size_t unit = findId(mapping.units, item.as<const char*>());
            if (unit == mapping.units.size()) return false;
            ports.push_back(uint8_t(unit));
        }
    }
    values.clear();
    for (const auto item : body["bindings"].as<JsonArrayConst>()) {
        if (!item["source"].is<const char*>() || !item["target"].is<const char*>() || !item["mode"].is<const char*>()) return false;
        const size_t source = findId(mapping.sources, item["source"].as<const char*>());
        const size_t target = findId(mapping.targets, item["target"].as<const char*>());
        size_t mode = 0;
        while (mode < 4 && strcmp(item["mode"].as<const char*>(), modeNames[mode]) != 0) ++mode;
        if (source >= 255 || target >= 255 || mode == 4) return false;
        const ControllerBinding binding{uint8_t(source), uint8_t(target), ControllerBindingMode(mode)};
        if (!Fits(binding, ports)) return false;
        values.push_back(binding);
    }
    return true;
}
String ControllerCalibration::EncodeMapping(const std::vector<uint8_t>& ports, const std::vector<ControllerBinding>& values) const {
    JsonDocument doc;
    doc["version"] = 1;
    if (mapping.ports) {
        auto list = doc["ports"].to<JsonArray>();
        for (uint8_t unit : ports) list.add(mapping.units[unit].id);
    }
    auto list = doc["bindings"].to<JsonArray>();
    for (const auto& binding : values) {
        auto item = list.add<JsonObject>();
        item["source"] = mapping.sources[binding.source].id;
        item["target"] = mapping.targets[binding.target].id;
        item["mode"] = modeNames[size_t(binding.mode)];
    }
    String json;
    serializeJson(doc, json);
    return json;
}
bool ControllerCalibration::Load() {
    if (!mutex || running || !storage) return false;
    if (mapping.bindings) {
        JsonDocument doc;
        std::vector<ControllerBinding> values;
        std::vector<uint8_t> ports;
        const String json = preferences.getString("mapping", "");
        if (json.length() && json.length() <= 8192 && !deserializeJson(doc, json.c_str()) &&
            DecodeMapping(doc.as<JsonVariantConst>(), ports, values)) {
            *mapping.bindings = values;
            if (mapping.ports) *mapping.ports = ports;
            mappingSaved = true;
        }
    }
    if (filter) {
        JsonDocument filterDoc;
        ControllerFilterSettings settings;
        const String json = preferences.getString("filter", "");
        if (json.length() && json.length() <= 256 && !deserializeJson(filterDoc, json.c_str()) &&
            decodeFilter(filterDoc.as<JsonVariantConst>(), settings)) {
            *filter = settings;
            filterSaved = true;
        }
    }
    const String json = preferences.getString("settings", "");
    JsonDocument doc;
    std::vector<M5Stack::AxisCalibration> values;
    if (!json.length() || json.length() > 8192 || deserializeJson(doc, json.c_str()) || !Decode(doc.as<JsonVariantConst>(), values)) return false;
    Apply(values);
    saved = values;
    message = "Loaded saved calibration";
    return true;
}
bool ControllerCalibration::Begin(const char* controllerTitle) {
    if (!mutex || running || !controllerTitle) return false;
    title = controllerTitle;
    // Exact: a plain URI would also answer every path below it, such as /api/calibration/options.
    server.on(AsyncURIMatcher::exact("/api/calibration"), HTTP_GET, [this](AsyncWebServerRequest* request) { State(request); });
    server.on("/api/calibration/start", HTTP_POST, [this](AsyncWebServerRequest* request) { Queue(request, Action::Start); });
    server.on("/api/calibration/end", HTTP_POST, [this](AsyncWebServerRequest* request) { Queue(request, Action::End); });
    server.on("/api/calibration/capture", HTTP_POST, [this](AsyncWebServerRequest* request) { Queue(request, Action::Capture); });
    server.on("/api/calibration/stop-capture", HTTP_POST, [this](AsyncWebServerRequest* request) { Queue(request, Action::StopCapture); });
    server.on("/api/calibration/reset", HTTP_POST, [this](AsyncWebServerRequest* request) { Queue(request, Action::Reset); });
    server.on("/api/calibration/preview", HTTP_POST, [this](AsyncWebServerRequest* request, JsonVariant& body) { Queue(request, Action::Preview, body); });
    server.on("/api/calibration/save", HTTP_POST, [this](AsyncWebServerRequest* request, JsonVariant& body) { Queue(request, Action::Save, body); });
    if (filter) server.on("/api/calibration/filter", HTTP_POST, [this](AsyncWebServerRequest* request, JsonVariant& body) {
        Queue(request, Action::SaveFilter, body);
    });
    if (mapping.bindings) {
        server.on("/api/calibration/options", HTTP_GET, [this](AsyncWebServerRequest* request) { Options(request); });
        server.on("/api/calibration/mapping", HTTP_POST, [this](AsyncWebServerRequest* request, JsonVariant& body) {
            Queue(request, Action::SaveMapping, body);
        });
    }
    server.begin();
    running = true;
    return true;
}
String ControllerCalibration::Address() const { return WiFi.localIP().toString(); }
void ControllerCalibration::UpdateRaw(size_t index, int32_t raw, bool online) {
    if (!mutex || index >= readings.size()) return;
    Lock lock(mutex);
    UpdateReading(index, raw, online);
}
void ControllerCalibration::UpdateInputs(const CalibrationRawReading* values, size_t count,
        const ControllerInputDiagnostics* inputDiagnostics) {
    if (!mutex || !values || count != readings.size()) return;
    Lock lock(mutex);
    for (size_t i = 0; i < count; ++i) UpdateReading(i, values[i].raw, values[i].online, values[i].sampled);
    if (inputDiagnostics) { diagnostics = *inputDiagnostics; hasDiagnostics = true; }
}
void ControllerCalibration::UpdateReading(size_t index, int32_t raw, bool online, bool sampled) {
    auto& reading = readings[index];
    reading.raw = raw;
    reading.sampled = sampled;
    reading.online = sampled && online && raw >= axes[index].rawMinimum && raw <= axes[index].rawMaximum;
    if (active && capturing && reading.online) {
        reading.observedMinimum = reading.observed ? std::min(raw, reading.observedMinimum) : raw;
        reading.observedMaximum = reading.observed ? std::max(raw, reading.observedMaximum) : raw;
        reading.observed = true;
    }
}
void ControllerCalibration::UpdateDiagnostics(const ControllerInputDiagnostics& values) {
    if (!mutex) return;
    Lock lock(mutex);
    diagnostics = values;
    hasDiagnostics = true;
}
void ControllerCalibration::State(AsyncWebServerRequest* request) {
    // Allocate before locking; only copy shared state while the input loop is blocked.
    std::vector<M5Stack::AxisCalibration> values(axes.size());
    std::vector<Reading> live(axes.size());
    std::vector<ControllerBinding> bindings;
    bindings.reserve(mapping.maximum);
    std::vector<uint8_t> ports(mapping.ports ? mapping.ports->size() : 0);
    ControllerInputDiagnostics inputDiagnostics;
    ControllerFilterSettings filtering;
    String snapshotMessage;
    snapshotMessage.reserve(128);
    bool snapshotActive, snapshotCapturing, snapshotSaved, snapshotHasDiagnostics;
    bool snapshotFilterSaved, snapshotMappingSaved;
    uint32_t snapshotOperation;
    int snapshotStatus;
    {
        Lock lock(mutex);
        if (active) lastSeen = millis();
        for (size_t i = 0; i < axes.size(); ++i) values[i] = *axes[i].settings;
        std::copy(readings.begin(), readings.end(), live.begin());
        if (mapping.bindings) bindings = *mapping.bindings;
        if (mapping.ports) std::copy(mapping.ports->begin(), mapping.ports->end(), ports.begin());
        snapshotActive = active; snapshotCapturing = capturing;
        snapshotSaved = values.size() == saved.size() && std::equal(values.begin(), values.end(), saved.begin(), same);
        snapshotHasDiagnostics = hasDiagnostics; inputDiagnostics = diagnostics;
        if (filter) filtering = *filter;
        snapshotFilterSaved = filterSaved; snapshotMappingSaved = mappingSaved;
        snapshotOperation = completedOperation; snapshotStatus = operationStatus;
        snapshotMessage = message;
    }
    JsonDocument doc;
    doc["title"] = title; doc["active"] = snapshotActive; doc["storage"] = storage;
    doc["capturing"] = snapshotCapturing; doc["saved"] = snapshotSaved;
    doc["message"] = snapshotMessage;
    doc["completedOperation"] = snapshotOperation; doc["operationStatus"] = snapshotStatus;
    doc["version"] = 1;
    if (snapshotHasDiagnostics) {
        auto counters = doc["diagnostics"].to<JsonObject>();
        counters["readFailures"] = inputDiagnostics.readFailures;
        counters["recoveredReads"] = inputDiagnostics.recoveredReads;
        counters["inputLossCycles"] = inputDiagnostics.inputLossCycles;
        counters["sendFailures"] = inputDiagnostics.sendFailures;
        if (inputDiagnostics.pollMicroseconds) {
            auto timing = counters["timing"].to<JsonObject>();
            timing["cycleCount"] = inputDiagnostics.cycleCount;
            timing["lastWorkMicroseconds"] = inputDiagnostics.lastWorkMicroseconds;
            timing["maxWorkMicroseconds"] = inputDiagnostics.maxWorkMicroseconds;
            timing["overrunCycles"] = inputDiagnostics.overrunCycles;
            timing["pollMicroseconds"] = inputDiagnostics.pollMicroseconds;
        }
    }
    if (filter) {
        auto settings = doc["filter"].to<JsonObject>();
        settings["mode"] = filterName(filtering.mode);
        settings["alpha"] = filtering.alpha; settings["saved"] = snapshotFilterSaved;
    }
    if (mapping.bindings) {
        auto settings = doc["mapping"].to<JsonObject>();
        settings["saved"] = snapshotMappingSaved;
        if (mapping.ports) {
            auto units = settings["ports"].to<JsonArray>();
            for (uint8_t unit : ports) units.add(mapping.units[unit].id);
        }
        auto list = settings["bindings"].to<JsonArray>();
        for (const auto& binding : bindings) {
            auto item = list.add<JsonObject>();
            item["source"] = mapping.sources[binding.source].id;
            item["target"] = mapping.targets[binding.target].id;
            item["mode"] = modeNames[size_t(binding.mode)];
        }
    }
    auto list = doc["axes"].to<JsonArray>();
    for (size_t i = 0; i < axes.size(); ++i) {
        auto item = list.add<JsonObject>();
        item["id"] = axes[i].id; item["label"] = axes[i].label;
        item["rawMinimum"] = axes[i].rawMinimum; item["rawMaximum"] = axes[i].rawMaximum;
        item["minimum"] = values[i].minimum; item["center"] = values[i].center; item["maximum"] = values[i].maximum;
        item["deadzone"] = values[i].deadzone; item["inverted"] = values[i].inverted;
        item["raw"] = live[i].raw; item["online"] = live[i].online; item["sampled"] = live[i].sampled;
        item["observedMinimum"] = live[i].observedMinimum;
        item["observedMaximum"] = live[i].observedMaximum; item["observed"] = live[i].observed;
        item["value"] = live[i].online ? values[i].Normalize(live[i].raw) : 0;
    }
    String json;
    serializeJson(doc, json);
    auto* response = request->beginResponse(200, "application/json", json);
    response->addHeader("Cache-Control", "no-store");
    request->send(response);
}
void ControllerCalibration::Options(AsyncWebServerRequest* request) const {
    // Static metadata, fetched once by the page instead of with every state poll.
    JsonDocument doc;
    doc["maximum"] = mapping.maximum;
    auto units = doc["units"].to<JsonArray>();
    for (const auto& unit : mapping.units) {
        auto item = units.add<JsonObject>();
        item["id"] = unit.id; item["label"] = unit.label;
    }
    auto sources = doc["sources"].to<JsonArray>();
    for (const auto& source : mapping.sources) {
        auto item = sources.add<JsonObject>();
        item["id"] = source.id; item["label"] = source.label; item["signal"] = signalNames[size_t(source.signal)];
        if (mapping.ports) {
            item["port"] = source.port;
            auto provided = item["units"].to<JsonArray>();
            for (size_t unit = 0; unit < mapping.units.size(); ++unit)
                if (source.units & (1u << unit)) provided.add(mapping.units[unit].id);
        }
    }
    auto targets = doc["targets"].to<JsonArray>();
    for (const auto& target : mapping.targets) {
        auto item = targets.add<JsonObject>();
        item["id"] = target.id; item["label"] = target.label; item["output"] = outputNames[size_t(target.output)];
    }
    String json;
    serializeJson(doc, json);
    auto* response = request->beginResponse(200, "application/json", json);
    response->addHeader("Cache-Control", "no-store");
    request->send(response);
}
void ControllerCalibration::Queue(AsyncWebServerRequest* request, Action action, JsonVariantConst body) {
    if (request->header("X-OpenCtrl-Calibration") != "1") { reply(request, 403, "Use the settings page"); return; }
    std::vector<M5Stack::AxisCalibration> values;
    ControllerFilterSettings settings;
    std::vector<ControllerBinding> bindings;
    std::vector<uint8_t> ports;
    if (action == Action::SaveMapping && (!mapping.bindings || !DecodeMapping(body, ports, bindings))) {
        reply(request, 400, "Each port needs a known unit, and each binding a source that unit provides, a known target, and a mode that fits both"); return;
    }
    if (action == Action::SaveFilter && (!filter || !decodeFilter(body, settings))) {
        reply(request, 400, "Choose Off, Median-3, or EMA with an alpha from 0.05 to 1"); return;
    }
    if ((action == Action::Save || action == Action::Preview) && !Decode(body, values)) {
        reply(request, 400, "Each axis needs minimum < center < maximum, a valid deadzone, and an inversion flag"); return;
    }
    uint32_t id;
    {
        Lock lock(mutex);
        if (pending.action != Action::None) { reply(request, 409, "Another settings operation is pending"); return; }
        if (action != Action::Start && action != Action::SaveFilter && action != Action::SaveMapping && !active) { reply(request, 409, "Start setup first"); return; }
        id = ++nextOperation;
        pending = {action, id, std::move(values), settings, std::move(bindings), std::move(ports)};
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
            Apply(baseline); active = false; capturing = false; message = "Setup session expired; preview discarded";
        }
        command = pending;
    }
    if (command.action == Action::None) return;
    bool success = true;
    if (command.action == Action::SaveMapping) {
        const String json = EncodeMapping(command.ports, command.bindings);
        success = storage && preferences.putString("mapping", json) == json.length();
    }
    if (command.action == Action::SaveFilter) {
        const String json = encodeFilter(command.filter);
        success = storage && preferences.putString("filter", json) == json.length();
    }
    if (command.action == Action::Save || command.action == Action::Reset) {
        if (command.action == Action::Reset) command.values = defaults;
        const String json = Encode(command.values);
        success = storage && preferences.putString("settings", json) == json.length();
    }
    Lock lock(mutex);
    switch (command.action) {
        case Action::SaveMapping:
            if (success) {
                *mapping.bindings = command.bindings;
                if (mapping.ports) *mapping.ports = command.ports;
                mappingSaved = true; message = "Ports and bindings applied and saved";
            } else message = "Storage write failed; ports and bindings were not changed";
            break;
        case Action::SaveFilter:
            if (success) { *filter = command.filter; filterSaved = true; message = "Filter settings applied and saved"; }
            else message = "Storage write failed; filter settings were not changed";
            break;
        case Action::Start:
            if (!active) baseline = Current();
            active = true; message = "Setup active; HID output paused"; break;
        case Action::Capture:
            capturing = true;
            for (auto& reading : readings) {
                reading.observed = reading.online;
                reading.observedMinimum = reading.observedMaximum = reading.raw;
            }
            message = "Move connected sticks through their full travel, then finish capture"; break;
        case Action::StopCapture:
            capturing = false; message = "Range captured; release connected sticks and set centers"; break;
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
