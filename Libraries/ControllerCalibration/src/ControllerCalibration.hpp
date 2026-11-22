#pragma once
#include <InputUtils.hpp>
#include "FilterSettings.hpp"
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

// A binding connects one physical source to one output. The consuming controller converts values.
// Relative sources (e.g. encoders) report movement; they bind like axes.
enum class ControllerSignal : uint8_t { Axis, Button, Relative };
enum class ControllerOutput : uint8_t { Axis, Button, Motion };
// Normal/Reversed use the whole axis. Positive/Negative use one half, e.g. a pedal or a button threshold.
enum class ControllerBindingMode : uint8_t { Normal, Reversed, Positive, Negative };
// A unit type that can be connected to a port, e.g. a joystick or an encoder.
struct ControllerUnit {
    const char* id;
    const char* label;
};
struct ControllerSource {
    const char* id;
    const char* label;
    ControllerSignal signal;
    uint8_t port = 0;
    uint32_t units = 0xffffffff; // Bit per unit index that provides this source.
};
struct ControllerTarget {
    const char* id;
    const char* label;
    ControllerOutput output;
};
struct ControllerBinding {
    uint8_t source = 0, target = 0;
    ControllerBindingMode mode = ControllerBindingMode::Normal;
    bool operator==(const ControllerBinding& other) const {
        return source == other.source && target == other.target && mode == other.mode;
    }
};
struct ControllerMapping {
    std::vector<ControllerUnit> units;
    std::vector<ControllerSource> sources;
    std::vector<ControllerTarget> targets;
    std::vector<ControllerBinding>* bindings = nullptr; // Owned and read by the input loop.
    std::vector<uint8_t>* ports = nullptr;              // Unit index per port; optional, saved with the bindings.
    size_t maximum = 16;
};
// Axes drive axes, motion, or buttons past half travel; buttons drive buttons.
inline bool ControllerBindingValid(ControllerSignal signal, ControllerOutput output, ControllerBindingMode mode) {
    const bool whole = mode == ControllerBindingMode::Normal || mode == ControllerBindingMode::Reversed;
    // Relative sources: a position for axes, movement for motion, one press per step for buttons.
    if (signal == ControllerSignal::Button) return output == ControllerOutput::Button && mode == ControllerBindingMode::Normal;
    if (output == ControllerOutput::Motion) return whole;
    if (output == ControllerOutput::Button) return !whole;
    return true;
}

struct CalibrationRawReading {
    int32_t raw = 0;
    bool online = false;
    bool sampled = true; // False when this cycle deliberately skipped the input.
};

struct ControllerInputDiagnostics {
    uint32_t readFailures = 0, recoveredReads = 0, inputLossCycles = 0, sendFailures = 0;
    // Processing budget only, excluding the planned poll wait; zero period hides timing.
    uint32_t cycleCount = 0, lastWorkMicroseconds = 0, maxWorkMicroseconds = 0;
    uint32_t overrunCycles = 0, pollMicroseconds = 0;
};

// HTTP callbacks only copy snapshots and queue commands. The input loop owns
// sensor access, settings application, and flash writes through Service().
class ControllerCalibration {
    enum class Action { None, Start, Capture, StopCapture, Preview, Save, Reset, End, SaveFilter, SaveMapping };
    struct Reading {
        int32_t raw = 0, observedMinimum = 0, observedMaximum = 0;
        bool online = false, observed = false, sampled = false;
    };
    struct Pending {
        Action action = Action::None;
        uint32_t id = 0;
        std::vector<M5Stack::AxisCalibration> values;
        ControllerFilterSettings filter;
        std::vector<ControllerBinding> bindings;
        std::vector<uint8_t> ports;
    };
    AsyncWebServer server{80};
    Preferences preferences;
    SemaphoreHandle_t mutex = nullptr;
    std::vector<CalibrationAxis> axes;
    std::vector<Reading> readings;
    std::vector<M5Stack::AxisCalibration> defaults, saved, baseline;
    Pending pending;
    ControllerInputDiagnostics diagnostics;
    bool hasDiagnostics = false;
    ControllerFilterSettings* filter = nullptr;
    ControllerMapping mapping;
    bool filterSaved = false, mappingSaved = false;
    bool storage = false, active = false, running = false, capturing = false;
    uint32_t lastSeen = 0, nextOperation = 0, completedOperation = 0;
    int operationStatus = 200;
    String message = "Ready", title;
    bool Decode(JsonVariantConst body, std::vector<M5Stack::AxisCalibration>& values) const;
    bool Fits(const ControllerBinding& binding, const std::vector<uint8_t>& ports) const;
    bool DecodeMapping(JsonVariantConst body, std::vector<uint8_t>& ports, std::vector<ControllerBinding>& values) const;
    String EncodeMapping(const std::vector<uint8_t>& ports, const std::vector<ControllerBinding>& values) const;
    String Encode(const std::vector<M5Stack::AxisCalibration>& values) const;
    void Apply(const std::vector<M5Stack::AxisCalibration>& values);
    std::vector<M5Stack::AxisCalibration> Current() const;
    void UpdateReading(size_t axis, int32_t raw, bool online, bool sampled = true);
    void State(AsyncWebServerRequest* request);
    void Options(AsyncWebServerRequest* request) const;
    void Queue(AsyncWebServerRequest* request, Action action, JsonVariantConst body = {});
public:
    bool Configure(const char* storageNamespace, std::vector<CalibrationAxis> bindings,
        ControllerFilterSettings* filterSettings = nullptr, ControllerMapping controllerMapping = {});
    bool Load();
    // Add page and controller routes before Begin(). Uses WiFi already started by the caller.
    AsyncWebServer& Server() { return server; }
    bool Begin(const char* controllerTitle);
    void UpdateRaw(size_t axis, int32_t raw, bool online);
    // Publish a complete sensor cycle and optional diagnostics under one lock.
    void UpdateInputs(const CalibrationRawReading* values, size_t count,
        const ControllerInputDiagnostics* inputDiagnostics = nullptr);
    void UpdateDiagnostics(const ControllerInputDiagnostics& values);
    void Service();
    bool Active() const { return active; } // Read from the input loop only.
    String Address() const;
};
