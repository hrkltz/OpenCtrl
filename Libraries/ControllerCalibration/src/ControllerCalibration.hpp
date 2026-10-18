#pragma once
#include <InputUtils.hpp>
#include <ArduinoJson.h>
#include <ESPAsyncWebServer.h>
#include <Preferences.h>
#include <freertos/semphr.h>
#include <vector>

struct CalibrationAxis {
    const char* id;
    const char* label;
    M5Stack::AxisCalibration* settings;
    int32_t rawMinimum = 0, rawMaximum = 65535;
};

// HTTP callbacks only copy snapshots and queue commands. The input loop owns
// sensor access, settings application, and flash writes through Service().
class ControllerCalibration {
    enum class Action { None, Start, Capture, StopCapture, Preview, Save, Reset, End };
    struct Reading {
        int32_t raw = 0, observedMinimum = 0, observedMaximum = 0;
        bool online = false, observed = false;
    };
    struct Pending { Action action = Action::None; uint32_t id = 0; std::vector<M5Stack::AxisCalibration> values; };
    AsyncWebServer server{80};
    Preferences preferences;
    SemaphoreHandle_t mutex = nullptr;
    std::vector<CalibrationAxis> axes;
    std::vector<Reading> readings;
    std::vector<M5Stack::AxisCalibration> defaults, saved, baseline;
    Pending pending;
    bool storage = false, active = false, running = false, capturing = false;
    uint32_t lastSeen = 0, nextOperation = 0, completedOperation = 0;
    int operationStatus = 200;
    String message = "Ready", title;
    char ssid[25]{}, password[17]{};
    bool Decode(JsonVariantConst body, std::vector<M5Stack::AxisCalibration>& values) const;
    String Encode(const std::vector<M5Stack::AxisCalibration>& values) const;
    void Apply(const std::vector<M5Stack::AxisCalibration>& values);
    std::vector<M5Stack::AxisCalibration> Current() const;
    void State(AsyncWebServerRequest* request);
    void Queue(AsyncWebServerRequest* request, Action action, JsonVariantConst body = {});
public:
    bool Configure(const char* storageNamespace, std::vector<CalibrationAxis> bindings);
    bool Load();
    bool Begin(const char* networkPrefix, const char* controllerTitle);
    void UpdateRaw(size_t axis, int32_t raw, bool online);
    void Service();
    bool Active() const { return active; } // Read from the input loop only.
    const char* SSID() const { return ssid; }
    const char* Password() const { return password; }
    String Address() const;
};
