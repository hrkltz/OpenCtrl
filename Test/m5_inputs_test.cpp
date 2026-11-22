#include <cassert>
#include <cmath>
#include <deque>
#include <iostream>
#include <limits>
#include <M5StackInputs.hpp>
#include <tusb.h>
#include <NimBLEDevice.h>

#include "../Examples/M5StackController/XIAOESP32S3Core/XIAOESP32S3Core.ino"

void DisplayUtil::Init() {}
void DisplayUtil::Print(uint8_t, const char*) {}
void DisplayUtil::PrintError(const char*) {}

void checkAdapters() {
    using namespace M5Stack;
    AxisCalibration axis{100,300,900,0.1f,false};
    assert(axis.Valid() && axis.Normalize(100) == -1 && axis.Normalize(300) == 0 && axis.Normalize(900) == 1);
    assert(axis.Normalize(-1000) == -1 && axis.Normalize(1000) == 1);
    assert(axis.Normalize(290) == 0 && axis.Normalize(350) == 0);
    axis.inverted = true;
    assert(axis.Normalize(100) == 1);
    axis.center = 100;
    assert(!axis.Valid() && axis.Normalize(500) == 0);
    axis = {};
    axis.deadzone = std::numeric_limits<float>::quiet_NaN();
    assert(!axis.Valid());
    axis = {std::numeric_limits<int32_t>::min(),0,std::numeric_limits<int32_t>::max(),0,false};
    assert(axis.Normalize(std::numeric_limits<int32_t>::min()) == -1);
    assert(axis.Normalize(std::numeric_limits<int32_t>::max()) == 1);
    assert(SignedAxis(-10) == -127 && SignedAxis(10) == 127 && SignedAxis(0) == 0);
    assert(PositiveTrigger(-1) == -127 && PositiveTrigger(0) == -127 && PositiveTrigger(1) == 127);
    assert(SignedAxis(NAN) == 0 && PositiveTrigger(NAN) == -127);

    TwoWire bus;
    bus.SetJoystick(2,0x1234,0xabcd,true);
    bus.SetJoystick(3,32768,32768,false);
    Joystick2 first(bus,{0x63,2,0x70}), second(bus,{0x63,3,0x70});
    JoystickRawState raw;
    assert(first.ReadRaw(raw) && raw.x == 0x1234 && raw.y == 0xabcd && raw.pressed);
    assert(second.ReadRaw(raw) && !raw.pressed && raw.x == 32768);
    assert(bus.selections == std::vector<uint8_t>({4,4,8,8}));
    assert(second.CalibrateCenter(3) && second.x.center == 32768);
    bus.SetJoystick(3,0,65535,true);
    JoystickState normalized;
    assert(second.Read(normalized) && normalized.x == -1 && normalized.y == 1 && normalized.pressed);
    assert(!second.CalibrateCenter(3) && second.x.center == 32768 && second.y.center == 32768);
    bus.missingChannels.insert(3);
    assert(!second.Read(normalized) && normalized.x == 0 && normalized.y == 0 && !normalized.pressed);
    bus.missingChannels.clear();
    bus.devices[{2,0x63}][0x20] = {255};
    assert(!first.Read(normalized));
    I2CDevice device(bus,{0x63,2,0x70});
    uint8_t bytes[] = {99,99,99,99};
    bus.shortRead = true;
    assert(!device.Read(0,bytes,4) && bytes[0] == 99 && bytes[3] == 99 && !bus.available());
    bus.transmissionFailures = 1;
    assert(!device.Read(0,bytes,4) && bytes[0] == 99);
    bus.missingHub = true;
    assert(!device.Read(0,bytes,4));
    bus.missingHub = false;
    const uint8_t config[] = {42,43};
    assert(device.Write(0x30,config,2));
    assert(device.Read(0x30,bytes,2) && bytes[0] == 42 && bytes[1] == 43);
    I2CDevice invalid(bus,{0x63,8,0x70});
    assert(!invalid.Read(0,bytes,1));
    TwoWire direct;
    direct.SetJoystick(-1,32768,32768);
    Joystick2 directStick(direct,{0x63,-1,0x70});
    assert(directStick.Read(normalized) && direct.selections.empty());
}

// Report IDs: 1 keyboard, 2 mouse, 3 gamepad.
size_t reportCount(uint8_t id) {
    size_t count = 0;
    if (Config::transport == HIDTransport::USB) { for (const auto& packet : USBMock::sent) count += packet.id == id; }
    else for (const auto& packet : BLEMock::sent) count += packet.id == id;
    return count;
}
std::vector<uint8_t> latest(uint8_t id) {
    if (Config::transport == HIDTransport::USB) {
        for (auto it = USBMock::sent.rbegin(); it != USBMock::sent.rend(); ++it) if (it->id == id) return it->data;
    } else for (auto it = BLEMock::sent.rbegin(); it != BLEMock::sent.rend(); ++it) if (it->id == id) return it->data;
    return {};
}
std::vector<uint8_t> latestReport() { return latest(3); }
auto& primary = sticks[0];
auto& secondary = sticks[1];
const std::vector<uint8_t> neutral = {0,0,129,129,0,0,0,0,0,0,0};

void authorizeSettings(AsyncWebServerRequest& request) {
    request.headers["X-OpenCtrl-Calibration"] = "1";
}
JsonDocument snapshot(const char* path = "/api/calibration") {
    AsyncWebServerRequest request;
    AsyncWebServer::plainRoutes.at({path,HTTP_GET})(&request);
    JsonDocument state;
    assert(request.status == 200 && !deserializeJson(state,request.body.c_str()));
    return state;
}
int post(const char* path) {
    AsyncWebServerRequest request;
    authorizeSettings(request);
    AsyncWebServer::plainRoutes.at({path,HTTP_POST})(&request);
    return request.status;
}

using Rows = std::vector<std::array<const char*,3>>;
// Without explicit units, a port gets an encoder when a binding turns it, otherwise a joystick.
JsonDocument bindingBody(const Rows& rows, std::vector<std::string> units = {}) {
    if (units.empty()) {
        units.assign(Config::portCount, "none");
        for (const auto& row : rows) {
            const std::string source = row[0];
            auto& unit = units[source[4] - '0'];
            if (source.find(".turn") != std::string::npos) unit = "encoder";
            else if (unit == "none") unit = "joystick2";
        }
    }
    JsonDocument body;
    body["version"] = 1;
    for (const auto& unit : units) body["ports"].add(unit);
    auto list = body["bindings"].to<JsonArray>();
    for (const auto& row : rows) {
        auto item = list.add<JsonObject>();
        item["source"] = row[0]; item["target"] = row[1]; item["mode"] = row[2];
    }
    return body;
}
int queueBindings(JsonDocument body) {
    AsyncWebServerRequest request;
    authorizeSettings(request);
    auto value = body.as<JsonVariant>();
    AsyncWebServer::routes.at("/api/calibration/mapping")(&request,value);
    return request.status;
}
// The page's presets, applied the same way.
const Rows drivePreset = {{"port0.y","gamepad.x","normal"},{"port1.y","gamepad.rz","negative"},{"port1.y","gamepad.z","positive"},
    {"port0.button","gamepad.button7","normal"},{"port1.button","gamepad.button8","normal"}};
const Rows flyPreset = {{"port2.x","gamepad.x","normal"},{"port2.y","gamepad.y","normal"},{"port3.x","gamepad.rz","normal"},
    {"port3.y","gamepad.z","negative"},{"port2.button","gamepad.button1","normal"},{"port3.button","gamepad.button2","normal"}};
// Steering, accelerator, and brake sources; shift clicks follow the steering and accelerator ports.
Rows drive(const std::string& steering, const std::string& accelerator, const char* acceleratorMode,
        const std::string& brake, const char* brakeMode) {
    static std::deque<std::string> keep; // Stable addresses for the returned pointers.
    auto text = [](std::string value) { keep.push_back(value); return keep.back().c_str(); };
    auto click = [&text](const std::string& source) { return text(source.substr(0, source.find('.')) + ".button"); };
    return {{text(steering),"gamepad.x","normal"},{text(accelerator),"gamepad.rz",acceleratorMode},
        {text(brake),"gamepad.z",brakeMode},{click(steering),"gamepad.button7","normal"},{click(accelerator),"gamepad.button8","normal"}};
}
void applyBindings(const Rows& rows, std::vector<std::string> units = {}) {
    assert(queueBindings(bindingBody(rows, units)) == 202);
    loop();
}

void checkDriveFiltering() {
    Wire.SetJoystick(0,65535,65535,true);
    Wire.SetJoystick(1,65535,0,true);
    loop(); loop(); loop();
    const auto stable = latestReport();

    Wire.SetJoystick(0,65535,0,false);
    Wire.SetJoystick(1,65535,65535,false);
    loop();
    assert(latestReport()[0] == stable[0] && latestReport()[2] == stable[2] && latestReport()[3] == stable[3]);
    assert(latestReport()[7] == 0); // Buttons respond on the first cycle, independently of axis filtering.
    auto state = snapshot();
    assert(state["axes"][1]["raw"] == 0 && state["axes"][4]["raw"] == 65535); // Port 0 Y and port 1 Y.
    assert(state["axes"][1]["value"].as<float>() == -1);

    Wire.SetJoystick(0,65535,65535,true);
    Wire.SetJoystick(1,65535,0,true);
    loop();
    assert(latestReport() == stable); // A single opposite-direction sample never reaches HID.
    loop(); loop();

    Wire.SetJoystick(0,65535,0,true);
    Wire.SetJoystick(1,65535,65535,true);
    loop();
    assert(latestReport() == stable);
    loop();
    assert(int8_t(latestReport()[0]) == -127 && latestReport()[2] == 127 && latestReport()[3] == 129);

    Wire.missingChannels.insert(1);
    loop();
    Wire.SetJoystick(0,65535,65535,true);
    Wire.SetJoystick(1,65535,0,true);
    Wire.missingChannels.clear();
    loop();
    assert(latestReport() == stable); // Recovery seeds both filters with fresh readings immediately.
}

void saveFilter(const char* mode, float alpha) {
    JsonDocument body;
    body["version"] = 1; body["mode"] = mode; body["alpha"] = alpha;
    AsyncWebServerRequest request;
    authorizeSettings(request);
    auto value = body.as<JsonVariant>();
    AsyncWebServer::routes.at("/api/calibration/filter")(&request,value);
    assert(request.status == 202);
}

void checkFilterModes() {
    saveFilter("off",0.5f);
    loop();
    assert(!calibration.Active() && filterSettings.mode == ControllerFilterMode::Off);
    Wire.SetJoystick(0,32768,0,true);
    Wire.SetJoystick(1,32768,65535,true);
    loop();
    assert(int8_t(latestReport()[0]) == -127 && latestReport()[2] == 127 && latestReport()[3] == 129);
    saveFilter("ema",0.5f);
    loop(); // Changing mode starts from the current readings, without old median history.
    assert(int8_t(latestReport()[0]) == -127);
    Wire.SetJoystick(0,32768,65535,false);
    Wire.SetJoystick(1,32768,0,false);
    loop();
    assert(latestReport()[0] == 0 && latestReport()[2] == 129 && latestReport()[3] == 129 && latestReport()[7] == 0);
    loop();
    assert(latestReport()[0] == uint8_t(M5Stack::SignedAxis(primary.y.Normalize(49151))));
    assert(latestReport()[3] == uint8_t(M5Stack::PositiveTrigger(-secondary.y.Normalize(16384))));
    saveFilter("median-3",0.35f);
    loop();
    assert(latestReport()[0] == 127 && latestReport()[3] == 127);
    // WiFi loss does not pause local HID or require browser heartbeats.
    WiFi.state = 0;
    loop();
    assert(latestReport()[0] == 127 && !calibration.Active());
    WiFi.state = WL_CONNECTED;
}

void checkOutputStability() {
    AxisQuantizer quantizer;
    assert(quantizer.Update(20.49f,0,true) == 20);
    assert(quantizer.Update(20.51f,0,true) == 20);
    assert(quantizer.Update(20.8f,0,true) == 21);
    assert(quantizer.Update(20.49f,0,true) == 21);
    assert(quantizer.Update(20.2f,0,true) == 20);
    quantizer.Reset();
    assert(quantizer.Update(-20.49f,0,true) == -20);
    assert(quantizer.Update(-20.51f,0,true) == -20);
    assert(quantizer.Update(-20.8f,0,true) == -21);
    assert(quantizer.Update(-20.49f,0,true) == -21);
    assert(quantizer.Update(0,0,true) == 0);
    assert(quantizer.Update(127,0,true) == 127);
    assert(quantizer.Update(-127,0,true) == -127);
    assert(quantizer.Update(20.49f,0,false) == 20);
    assert(quantizer.Update(20.51f,0,false) == 21); // Off retains direct rounding.
    quantizer.Reset();
    assert(quantizer.Update(20.51f,0,true) == 21);
    assert(quantizer.Update(-127,-127,true) == -127); // Pedal rest bypasses hysteresis.

    auto rawForAxis = [](const M5Stack::AxisCalibration& calibration, float normalized) {
        if (calibration.inverted) normalized = -normalized;
        const float magnitude = calibration.deadzone + std::abs(normalized) * (1 - calibration.deadzone);
        return calibration.center + (normalized < 0 ? -magnitude * (calibration.center - calibration.minimum) :
            magnitude * (calibration.maximum - calibration.center));
    };
    const auto steeringRaw = uint16_t(std::lround(rawForAxis(primary.y,20.5f / 127)));
    for (int direction : {-1,1}) {
        const auto pedalRaw = uint16_t(std::lround(rawForAxis(secondary.y,direction * (50.5f + 127) / 254)));
        const int pedalIndex = direction < 0 ? 3 : 2; // Negative travel accelerates (Rz), positive brakes (Z).
        for (const char* mode : {"off","median-3","ema"}) {
            saveFilter(mode,0.06f);
            int steeringChanges = 0, pedalChanges = 0;
            std::vector<uint8_t> previous;
            for (int i = 0; i < 1256; ++i) {
                const int noise = i % 2 ? 24 : -24;
                Wire.SetJoystick(0,32768,steeringRaw + noise);
                Wire.SetJoystick(1,32768,pedalRaw + noise);
                loop();
                const auto current = latestReport();
                if (i >= 256) {
                    steeringChanges += current[0] != previous[0];
                    pedalChanges += current[pedalIndex] != previous[pedalIndex];
                }
                previous = current;
            }
            if (std::string(mode) == "off") assert(steeringChanges > 900 && pedalChanges > 900);
            else assert(steeringChanges == 0 && pedalChanges == 0);
        }
    }
    Wire.SetJoystick(0,32768,65535);
    Wire.SetJoystick(1,32768,0);
    for (int i = 0; i < 300; ++i) loop();
    assert(latestReport()[0] == 127 && latestReport()[3] == 127);
    Wire.SetJoystick(0,32768,primary.y.center);
    Wire.SetJoystick(1,32768,secondary.y.center);
    for (int i = 0; i < 300; ++i) loop();
    assert(latestReport()[0] == 0 && latestReport()[2] == 129 && latestReport()[3] == 129);
    saveFilter("median-3",0.35f);
    loop();
}

void checkTiming() {
    assert(Wire.clock == 400000);
    auto cycleDuration = [] {
        const uint32_t before = micros();
        loop();
        return uint32_t(micros() - before);
    };
    // Twelve bus transactions per cycle; processing time must count toward the period.
    const auto cyclesBefore = inputDiagnostics.cycleCount, overrunsBefore = inputDiagnostics.overrunCycles;
    Wire.transactionMicroseconds = 275;
    assert(cycleDuration() == 4000);
    assert(inputDiagnostics.lastWorkMicroseconds == 3300 && inputDiagnostics.cycleCount == cyclesBefore + 1);
    assert(inputDiagnostics.overrunCycles == overrunsBefore);
    Wire.transactionMicroseconds = 400;
    assert(cycleDuration() == 4800);
    assert(inputDiagnostics.lastWorkMicroseconds == 4800 && inputDiagnostics.maxWorkMicroseconds >= 4800);
    assert(inputDiagnostics.overrunCycles == overrunsBefore + 1); // An overrun must not cause an unsigned wait underflow.
    Wire.transactionMicroseconds = 0;
    assert(cycleDuration() == 4000); // No burst of catch-up reports after an overrun.
    mockMillis = UINT32_MAX / 1000;
    mockMicrosecondRemainder = 0;
    Wire.transactionMicroseconds = 275;
    assert(cycleDuration() == 4000); // micros() wraps during input processing.
    assert(inputDiagnostics.lastWorkMicroseconds == 3300 && inputDiagnostics.overrunCycles == overrunsBefore + 1);
    Wire.transactionMicroseconds = 0;
    const auto previous = inputDiagnostics;
    loop();
    const auto state = snapshot();
    auto timing = state["diagnostics"]["timing"];
    assert(timing["cycleCount"] == previous.cycleCount && timing["lastWorkMicroseconds"] == previous.lastWorkMicroseconds);
    assert(timing["maxWorkMicroseconds"] == inputDiagnostics.maxWorkMicroseconds);
    assert(timing["overrunCycles"] == inputDiagnostics.overrunCycles && timing["pollMicroseconds"] == 4000);
    // Saving settings contributes to measured work; exactly meeting the budget is not an overrun.
    const auto overruns = inputDiagnostics.overrunCycles;
    PreferencesMock::writeMicroseconds = 4000;
    saveFilter("median-3",0.35f);
    assert(cycleDuration() == 4000 && inputDiagnostics.lastWorkMicroseconds == 4000);
    assert(inputDiagnostics.overrunCycles == overruns);
    PreferencesMock::writeMicroseconds = 6000;
    saveFilter("median-3",0.35f);
    assert(cycleDuration() == 6000 && inputDiagnostics.lastWorkMicroseconds == 6000);
    assert(inputDiagnostics.maxWorkMicroseconds >= 6000 && inputDiagnostics.overrunCycles == overruns + 1);
    PreferencesMock::writeMicroseconds = 0;
}

void checkReadRecovery() {
    saveFilter("off",0.35f);
    Wire.SetJoystick(0,32768,50000,true);
    Wire.SetJoystick(1,32768,20000,true);
    loop();
    auto active = latestReport();
    const auto before = inputDiagnostics;
    auto count = reportCount(3);
    Wire.transmissionFailures = 1;
    loop();
    assert(inputHealthy && latestReport() == active && reportCount(3) == count + 1);
    assert(inputDiagnostics.readFailures == before.readFailures + 1);
    assert(inputDiagnostics.recoveredReads == before.recoveredReads + 1);
    assert(inputDiagnostics.inputLossCycles == before.inputLossCycles);

    // A short frame must be reread completely and use the latest values, never stale data.
    Wire.SetJoystick(0,32768,45000,false);
    Wire.shortRead = true;
    count = reportCount(3);
    loop();
    assert(inputHealthy && reportCount(3) == count + 1);
    assert(latestReport()[0] == uint8_t(M5Stack::SignedAxis(primary.y.Normalize(45000))));
    assert(inputDiagnostics.recoveredReads == before.recoveredReads + 2);
    active = latestReport();

    Wire.transmissionFailures = 2;
    loop();
    assert(!inputHealthy && latestReport()[0] == 0 && latestReport()[7] == 0);
    assert(inputDiagnostics.inputLossCycles == before.inputLossCycles + 1);
    loop();
    assert(inputHealthy && latestReport() == active);
    const auto failures = inputDiagnostics.readFailures;
    Wire.missingChannels.insert(1);
    loop();
    assert(!inputHealthy && latestReport()[0] == 0);
    assert(inputDiagnostics.readFailures == failures + 1);
    Wire.missingChannels.clear();
    loop();
    assert(inputHealthy && latestReport() == active);

    PortReading raw;
    const auto recovered = inputDiagnostics.recoveredReads;
    const auto failed = inputDiagnostics.readFailures;
    Wire.transmissionFailures = 1;
    assert(!ReadPort(0,raw,micros() - Config::pollMicroseconds));
    assert(inputDiagnostics.readFailures == failed + 1 && inputDiagnostics.recoveredReads == recovered);
    assert(Wire.transmissionFailures == 0 && raw.stick.x == 0); // No retry when the cycle budget is exhausted.

    saveFilter("ema",0.06f);
    Wire.SetJoystick(0,32768,40000,false);
    loop();
    Wire.SetJoystick(0,32768,60000,false);
    Wire.transmissionFailures = 1;
    count = reportCount(3);
    loop();
    assert(inputHealthy && reportCount(3) == count + 1);
    assert(latestReport()[0] == uint8_t(M5Stack::SignedAxis(primary.y.Normalize(41200))));
    // The recovered read advances the existing EMA instead of reseeding it to 60000.
    saveFilter("off",0.35f);
    loop();
    active = latestReport();

    const auto losses = inputDiagnostics.inputLossCycles, sends = inputDiagnostics.sendFailures;
    if (Config::transport == HIDTransport::USB) USBMock::failures = 1;
    else BLEMock::failures = 1;
    loop();
    assert(inputHealthy && inputDiagnostics.sendFailures == sends + 1 && inputDiagnostics.inputLossCycles == losses);
    loop();
    assert(latestReport() == active);
    auto state = snapshot();
    assert(state["diagnostics"]["inputLossCycles"] == inputDiagnostics.inputLossCycles);
    assert(state["diagnostics"]["sendFailures"] == sends + 1);
    saveFilter("median-3",0.35f);
    loop();
}

void checkPortReadStatus() {
    for (size_t port = 0; port < Config::portCount; ++port)
        Wire.SetJoystick(port,40000 + port * 1000,45000 + port * 1000);
    applyBindings(drivePreset,{"joystick2","joystick2","joystick2","joystick2","joystick2","none"});
    auto state = snapshot();
    for (size_t port = 0; port < Config::portCount; ++port) {
        assert(state["axes"][port * 3]["sampled"] == (port < 2));
        assert(state["axes"][port * 3]["online"] == (port < 2));
    }
    assert(post("/api/calibration/start") == 202);
    loop();
    state = snapshot();
    for (size_t port = 0; port < Config::portCount; ++port) {
        // Setup reads every port with a unit, and only the axes that unit has.
        assert(state["axes"][port * 3 + 2]["sampled"] == false);
        if (port == 5) { assert(state["axes"][port * 3]["sampled"] == false); continue; }
        assert(state["axes"][port * 3]["sampled"] == true && state["axes"][port * 3]["online"] == true);
        assert(state["axes"][port * 3]["raw"] == 40000 + port * 1000);
        assert(state["axes"][port * 3 + 1]["raw"] == 45000 + port * 1000);
    }
    assert(post("/api/calibration/end") == 202);
    loop();
    applyBindings(drive("port0.y","port1.y","positive","port2.y","positive"));
    state = snapshot();
    assert(inputHealthy);
    for (size_t port = 0; port < Config::portCount; ++port) {
        assert(state["axes"][port * 3]["sampled"] == (port < 3));
        assert(state["axes"][port * 3]["online"] == (port < 3));
    }
    Wire.missingChannels.insert(2);
    loop();
    state = snapshot();
    assert(state["axes"][6]["sampled"] == true && state["axes"][6]["online"] == false);
    assert(state["axes"][9]["sampled"] == false); // Skipped ports differ from failed reads.
    Wire.missingChannels.clear();
    applyBindings(drivePreset);
}

void checkFlexibleMapping() {
    saveFilter("off",0.35f);
    loop();
    const auto axes = CalibrationBindings();
    // Every physical port and axis can drive all three controls, including shared sources.
    for (size_t stickAxis = 0; stickAxis < Config::portCount * 2; ++stickAxis) {
        const size_t axis = stickAxis / 2 * 3 + stickAxis % 2; // Calibration index of port X or Y.
        for (size_t port = 0; port < Config::portCount; ++port)
            Wire.SetJoystick(port,sticks[port].x.center,sticks[port].y.center);
        const auto& settings = AxisSettings(axis);
        const uint16_t end = settings.inverted ? settings.minimum : settings.maximum;
        Wire.SetJoystick(stickAxis / 2,stickAxis % 2 ? sticks[stickAxis / 2].x.center : end,
            stickAxis % 2 ? end : sticks[stickAxis / 2].y.center,true);
        assert(queueBindings(bindingBody(drive(axes[axis].id,axes[axis].id,"positive",axes[axis].id,"positive"))) == 202);
        Wire.selections.clear();
        loop();
        assert(latestReport()[0] == 127 && latestReport()[2] == 127 && latestReport()[3] == 127);
        assert(latestReport()[7] == 192);
        // One sensor read (two transactions) even when five bindings share it.
        assert(Wire.selections == std::vector<uint8_t>({uint8_t(1 << (stickAxis / 2)),uint8_t(1 << (stickAxis / 2))}));
    }
    // Three separate sticks: steering 5 X, accelerator 2 Y, brake 4 X (negative).
    Wire.SetJoystick(5,65535,32768,true);
    Wire.SetJoystick(2,32768,65535,true);
    Wire.SetJoystick(4,0,32768,false);
    Wire.missingChannels = {0,1,3}; // Unused ports need not be connected.
    const auto separate = drive("port5.x","port2.y","positive","port4.x","negative");
    applyBindings(separate);
    assert(inputHealthy && latestReport()[0] == 127 && latestReport()[2] == 127 && latestReport()[3] == 127);
    assert(latestReport()[7] == 192);
    Wire.SetJoystick(2,32768,32768);
    loop();
    assert(latestReport()[2] == 127 && latestReport()[3] == 129 && latestReport()[7] == 64);
    Wire.SetJoystick(4,32768,32768);
    loop();
    assert(latestReport()[2] == 129 && latestReport()[3] == 129);
    Wire.missingChannels.insert(4);
    loop();
    assert(!inputHealthy && latestReport()[0] == 0 && latestReport()[2] == 129 && latestReport()[7] == 0);
    Wire.missingChannels.erase(4);
    loop();
    assert(inputHealthy && latestReport()[0] == 127);
    const auto applied = bindings;
    for (int invalid = 0; invalid < 4; ++invalid) {
        auto bad = bindingBody(separate);
        switch (invalid) {
            case 0: bad["bindings"][0]["source"] = "port6.x"; break;
            case 1: bad["bindings"][3]["target"] = "gamepad.x"; break; // A click cannot drive an axis.
            case 2: bad["bindings"][0]["target"] = "key.f1"; break;
            case 3: for (int i = 0; i < 12; ++i) bad["bindings"].add<JsonObject>().set(bad["bindings"][0]); break;
        }
        assert(queueBindings(bad) == 400);
        assert(bindings == applied);
    }
    PreferencesMock::failWrites = true;
    applyBindings(drivePreset);
    assert(bindings == applied);
    PreferencesMock::failWrites = false;
    // The saved table loads independently of calibration.
    std::vector<ControllerBinding> restored;
    std::vector<uint8_t> restoredPorts(Config::portCount, NoUnit);
    ControllerCalibration rebooted;
    assert(rebooted.Configure("m5-ctrl",CalibrationBindings(),nullptr,MakeMapping(&restored,&restoredPorts)));
    const auto axisRecord = PreferencesMock::storage["m5-ctrl"]["settings"];
    PreferencesMock::storage["m5-ctrl"].erase("settings");
    assert(!rebooted.Load() && restored == applied && restoredPorts == ports);
    PreferencesMock::storage["m5-ctrl"]["settings"] = axisRecord;
    // A new table resets smoothing rather than carrying readings across sources.
    saveFilter("ema",0.05f);
    loop();
    Wire.SetJoystick(5,0,65535);
    applyBindings(drive("port5.y","port2.y","positive","port4.x","negative"));
    assert(latestReport()[0] == 127);
    assert(post("/api/calibration/start") == 202);
    loop();
    assert(calibration.Active() && queueBindings(bindingBody(separate)) == 202); // Units can change during setup.
    loop();
    assert(bindings == applied && calibration.Active());
    assert(post("/api/calibration/end") == 202);
    loop();
    Wire.missingChannels.clear();
    saveFilter("off",0.35f);
    loop();
}

void checkFlyPreset() {
    applyBindings(flyPreset);
    Wire.SetJoystick(2,65535,0,true);
    Wire.SetJoystick(3,0,0,false);
    loop();
    // Roll right, pitch full negative, rudder left, throttle full (negative half), button 1.
    auto report = latestReport();
    assert(report[0] == 127 && int8_t(report[1]) == -127 && report[2] == 127 && int8_t(report[3]) == -127 && report[7] == 1);
    Wire.missingChannels.insert(3);
    loop();
    // Neutral follows the bindings: centered rudder (0), released throttle (-127).
    assert(!inputHealthy && latestReport() == std::vector<uint8_t>({0,0,129,0,0,0,0,0,0,0,0}));
    Wire.missingChannels.clear();
    loop();
    assert(inputHealthy && latestReport() == report);
}

void checkMouseAndKeyboard() {
    if (Config::transport == HIDTransport::Bluetooth) { BLEMock::subscribe(1); BLEMock::subscribe(2); }
    applyBindings({{"port0.x","mouse.x","normal"},{"port0.y","mouse.y","reversed"},{"port0.button","mouse.left","normal"},
        {"port1.y","mouse.wheel","normal"},{"port1.y","key.w","negative"},{"port1.button","key.shift","normal"}});
    const auto gamepads = reportCount(3), mice = reportCount(2), keyboards = reportCount(1);
    Wire.SetJoystick(0,primary.x.center,primary.y.center);
    Wire.SetJoystick(1,secondary.x.center,secondary.y.center);
    loop(); loop();
    assert(reportCount(2) == mice && reportCount(1) == keyboards); // Idle inputs send nothing.
    assert(reportCount(3) == gamepads); // Unbound devices are never sent.
    Wire.SetJoystick(0,65535,0,true);
    loop();
    assert(latest(2) == std::vector<uint8_t>({1,8,8,0,0})); // Full deflection moves 8 counts per cycle.
    Wire.SetJoystick(0,primary.x.center,primary.y.center,true);
    const auto held = reportCount(2);
    for (int i = 0; i < 10; ++i) loop();
    assert(reportCount(2) <= held + 1); // A held button is only refreshed occasionally.
    Wire.SetJoystick(0,primary.x.center,primary.y.center,false);
    loop();
    assert(latest(2) == std::vector<uint8_t>({0,0,0,0,0}));
    // The wheel scrolls one detent per ten cycles at full deflection.
    Wire.SetJoystick(1,secondary.x.center,65535);
    const auto before = reportCount(2);
    for (int i = 0; i < 10; ++i) loop();
    assert(reportCount(2) == before + 1 && latest(2)[3] == 1);
    // Past half travel presses W; the click holds Shift.
    Wire.SetJoystick(1,secondary.x.center,0,true);
    loop();
    assert(latest(1) == std::vector<uint8_t>({2,0,0x1a,0,0,0,0,0}));
    const auto pressed = reportCount(1);
    for (int i = 0; i < 60; ++i) loop();
    // Held keys are not resent every cycle, only refreshed every 50 cycles.
    assert(reportCount(1) - pressed >= 1 && reportCount(1) - pressed <= 2 && latest(1)[2] == 0x1a);
    // Switching tables releases held keys at once and starts the new devices cleanly.
    applyBindings(drivePreset);
    assert(latest(1) == std::vector<uint8_t>({0,0,0,0,0,0,0,0}));
    assert(reportCount(3) > gamepads);
    Wire.SetJoystick(1,secondary.x.center,secondary.y.center);
    loop();

    // Several bindings on one axis use the largest deflection; neutral follows each binding's rest.
    BindingMapper mapper;
    std::vector<ControllerBinding> table{{1,0,ControllerBindingMode::Normal},{4,0,ControllerBindingMode::Reversed},
        {1,3,ControllerBindingMode::Positive}};
    BindingInput inputs[] = {{0.2f,false},{0.6f,false},{0.2f,false}};
    auto report = mapper.Map(table,inputs,false);
    assert(report.axes[0] == -76 && report.axes[3] == int8_t(std::lround((0.4f - 1) * 127)) && report.gamepad);
    BindingInput rest[3] = {};
    report = mapper.Map(table,rest,false);
    assert(report.axes[0] == 0 && report.axes[3] == -127 && report.axes[2] == -127 && !report.mouse && !report.keyboard);
}

void setEncoder(int port, uint32_t counter, bool pressed, uint8_t bytes) {
    auto& registers = Wire.devices[{port,0x40}];
    registers[0x10].clear();
    for (uint8_t i = 0; i < bytes; ++i) registers[0x10].push_back(uint8_t(counter >> (8 * i)));
    registers[0x20] = {uint8_t(!pressed)};
}

uint8_t encoderMode(int port) { return Wire.devices[std::make_pair(port,uint8_t(0x40))][0x00].at(0); }

void checkEncoders() {
    // Port 2: Encoder U135 as a centered axis and per-step arrow keys. Port 3: Scroll U186 as the wheel.
    setEncoder(2,100,false,2);
    setEncoder(3,7,false,4);
    Wire.devices[{2,0x40}][0x00] = {9};
    const Rows rows = {{"port2.turn","gamepad.x","normal"},{"port2.turn","key.right","positive"},{"port2.turn","key.left","negative"},
        {"port3.turn","mouse.wheel","normal"},{"port2.button","gamepad.button1","normal"}};
    applyBindings(rows,{"none","none","encoder","scroll","none","none"});
    assert(ports[2] == Encoder && ports[3] == Scroll);
    assert(encoderMode(2) == 0); // Pulse mode selected.
    loop();
    assert(inputHealthy && latestReport()[0] == 0); // The position starts at center.
    const auto mice = reportCount(2);
    setEncoder(2,106,true,2);
    setEncoder(3,9,false,4);
    loop();
    // Six steps of 24 to the end: a quarter of full deflection; two wheel detents at once.
    assert(latestReport()[0] == 32 && latestReport()[7] == 1);
    assert(reportCount(2) == mice + 1 && latest(2)[3] == 2);
    // Each step presses Right once: two cycles held, two released.
    std::vector<bool> right;
    for (int i = 0; i < 24; ++i) {
        right.push_back(latest(1)[2] == 0x4f);
        loop();
    }
    assert(right[0] && right[1] && !right[2] && !right[3] && right[4]);
    assert(std::count(right.begin(),right.end(),true) == 12 && !right.back());
    // The position stops at the calibrated end, so turning back responds at once.
    setEncoder(2,300,false,2);
    loop();
    assert(latestReport()[0] == 127 && positions[2] == 24);
    setEncoder(2,299,false,2);
    loop(); loop();
    assert(positions[2] == 23 && latestReport()[0] == uint8_t(std::lround(23.0f / 24 * 127)));
    for (int i = 0; i < 4 && latest(1)[2] != 0x50; ++i) loop();
    assert(latest(1)[2] == 0x50); // One step back presses Left.
    for (int i = 0; i < 40; ++i) loop();
    // A lost encoder releases its controls; reconnecting selects pulse mode again without replaying steps.
    Wire.missingChannels.insert(2);
    loop();
    assert(!inputHealthy && latestReport()[0] == 0);
    Wire.devices[{2,0x40}][0x00] = {9};
    setEncoder(2,500,false,2);
    Wire.missingChannels.clear();
    loop();
    assert(inputHealthy && encoderMode(2) == 0 && positions[2] == 23);
    // Setup shows the turn position; inverting it reverses the position and the step direction.
    assert(post("/api/calibration/start") == 202);
    loop();
    auto state = snapshot();
    assert(state["axes"][8]["id"] == "port2.turn" && state["axes"][8]["raw"] == 23 && state["axes"][8]["sampled"] == true);
    assert(state["axes"][6]["sampled"] == false && state["axes"][11]["online"] == true); // No X on an encoder; scroll on port 3.
    assert(state["axes"][8]["rawMinimum"] == -32768 && state["axes"][8]["minimum"] == -24);
    state["axes"][8]["inverted"] = true;
    state["axes"][8]["minimum"] = -1;
    auto body = state.as<JsonVariant>();
    AsyncWebServerRequest save;
    authorizeSettings(save);
    AsyncWebServer::routes.at("/api/calibration/save")(&save,body);
    assert(save.status == 202);
    loop();
    assert(post("/api/calibration/end") == 202);
    loop();
    assert(int8_t(latestReport()[0]) == -122); // 23 of 24 steps, reversed.
    setEncoder(2,501,false,2);
    loop();
    assert(positions[2] == 24 && latest(1)[2] == 0x50); // A positive step now counts as negative.
    // A new unit on a port starts at center.
    applyBindings({{"port2.button","gamepad.button1","normal"}},{"none","none","scroll","none","none","none"});
    applyBindings(rows,{"none","none","encoder","scroll","none","none"});
    assert(positions[2] == 0);
    // A port without its unit's source is rejected: a joystick has no turn.
    assert(queueBindings(bindingBody(rows,{"none","none","joystick2","scroll","none","none"})) == 400);
    Wire.devices.erase({2,0x40}); Wire.devices.erase({3,0x40});
    applyBindings(drivePreset);
}

void checkPortScan() {
    Wire.devices[{-1,0x3c}] = {}; // OLED on the direct bus.
    Wire.devices[{4,0x40}] = {};  // Encoder on port 4.
    applyBindings(drivePreset,{"joystick2","joystick2","joystick2","joystick2","none","none"});
    auto ports = snapshot("/api/ports");
    // No scan while HID runs: the earlier setup scan is kept, and unused ports are not read.
    assert(ports["scanned"] == true && ports["direct"].size() == 0 && ports["ports"][2]["pressed"].isNull());
    Wire.SetJoystick(2,32768,32768,true);
    Wire.selections.clear();
    assert(post("/api/calibration/start") == 202);
    loop();
    ports = snapshot("/api/ports");
    assert(ports["scanned"] == true && ports["hub"] == true && ports["hubAddress"] == 0x70);
    assert(ports["direct"].size() == 1 && ports["direct"][0] == 0x3c);
    assert(ports["ports"][0]["devices"][0] == 0x63 && ports["ports"][0]["devices"].size() == 1);
    assert(ports["ports"][4]["devices"].size() == 2 && ports["ports"][4]["devices"][0] == 0x40);
    assert(ports["ports"][2]["pressed"] == true && ports["ports"][3]["pressed"] == false);
    assert(latestReport() == neutral && calibration.Active());
    // Rescan on request picks up newly connected units; requests need the settings header.
    AsyncWebServerRequest anonymous;
    AsyncWebServer::plainRoutes.at({"/api/ports/scan",HTTP_POST})(&anonymous);
    assert(anonymous.status == 403);
    Wire.devices[{5,0x41}] = {};
    loop();
    assert(snapshot("/api/ports")["ports"][5]["devices"].size() == 1); // Periodic scans wait 3 s.
    assert(post("/api/ports/scan") == 202);
    loop();
    assert(snapshot("/api/ports")["ports"][5]["devices"].size() == 2);
    assert(post("/api/calibration/end") == 202);
    loop();
    Wire.selections.clear();
    for (int i = 0; i < 1000; ++i) loop();
    assert(std::count(Wire.selections.begin(),Wire.selections.end(),0) == 0); // No scans once controls resume.
    Wire.devices.erase({-1,0x3c}); Wire.devices.erase({4,0x40}); Wire.devices.erase({5,0x41});
    Wire.missingHub = true;
    assert(post("/api/calibration/start") == 202);
    loop();
    assert(snapshot("/api/ports")["hub"] == false);
    Wire.missingHub = false;
    assert(post("/api/calibration/end") == 202);
    loop();
}

int main() {
    checkAdapters();
    for (size_t port = 0; port < Config::portCount; ++port) Wire.SetJoystick(port,32768,32768);
    setup();
    assert(started && Wire.beginSDA == SDA && Wire.beginSCL == SCL);
    assert(WiFi.selectedMode == WIFI_STA && WiFi.stationStarted && WiFi.autoReconnect);
    assert(primary.x.center == 32768 && secondary.y.center == 32768);
    assert(HIDUtil::Transports(3) == Config::transport && HIDUtil::InputSize(3) == 11);
    // Every route is reachable on the real server, which also matches paths below a plain URI.
    for (const auto& [path, method] : {std::pair<const char*,int>{"/",HTTP_GET}, {"/index.html",HTTP_GET}, {"/api/calibration",HTTP_GET},
            {"/api/calibration/options",HTTP_GET}, {"/api/ports",HTTP_GET}, {"/api/ports/scan",HTTP_POST}, {"/api/calibration/mapping",HTTP_POST}})
        assert(AsyncWebServer::Resolve(path,method) == path);
    if (Config::transport == HIDTransport::Bluetooth) {
        assert(BLEMock::initialized == 1 && !HIDUtil::Connected(HIDTransport::USB));
        BLEMock::connect(); BLEMock::authenticate(); BLEMock::subscribe(3);
    } else assert(BLEMock::initialized == 0);
    loop();
    const auto initial = reportCount(3); // The core may release on connect.
    for (int i = 0; i < 5; ++i) loop();
    assert(bindings.empty() && reportCount(3) == initial); // Nothing is sent until bindings exist.

    applyBindings(drivePreset);
    assert(latestReport() == neutral);
    Wire.SetJoystick(0,65535,65535,true);
    Wire.SetJoystick(1,65535,0,true);
    loop();
    assert(latestReport()[0] == 0 && latestReport()[3] == 129 && latestReport()[7] == 192);
    loop(); // A sustained axis change reaches the median on the second cycle.
    const auto active = latestReport();
    assert(active[0] == 127 && active[2] == 129 && active[3] == 127 && active[7] == 192);
    Wire.SetJoystick(1,32768,65535,false);
    loop();
    assert(latestReport()[2] == 129 && latestReport()[3] == 127 && latestReport()[7] == 64);
    loop();
    assert(latestReport()[2] == 127 && latestReport()[3] == 129);
    Wire.missingChannels.insert(0);
    loop();
    assert(!inputHealthy && latestReport() == neutral);
    Wire.missingChannels.clear();
    Wire.SetJoystick(1,65535,0,true);
    loop();
    assert(inputHealthy && latestReport() == active);
    assert(post("/api/calibration/start") == 202);
    loop();
    assert(calibration.Active() && latestReport() == neutral);
    auto settings = snapshot();
    assert(settings["axes"][0]["raw"] == 65535);
    settings["axes"][0]["center"] = 32000;
    auto body = settings.as<JsonVariant>();
    AsyncWebServerRequest save;
    authorizeSettings(save);
    AsyncWebServer::routes.at("/api/calibration/save")(&save,body);
    assert(save.status == 202);
    loop();
    assert(primary.x.center == 32000 && calibration.Active() && latestReport() == neutral);
    Wire.SetJoystick(0,65535,0,true);
    Wire.SetJoystick(1,65535,65535,true);
    assert(post("/api/calibration/end") == 202);
    loop();
    assert(!calibration.Active() && int8_t(latestReport()[0]) == -127 && latestReport()[2] == 127);
    Wire.SetJoystick(0,65535,65535,true);
    Wire.SetJoystick(1,65535,0,true);
    loop(); loop();
    assert(!calibration.Active() && latestReport() == active);
    if (Config::transport == HIDTransport::USB) {
        USBMock::mounted = false; loop();
        USBMock::mounted = true;
    } else {
        BLEMock::disconnect(); loop();
        BLEMock::connect(); BLEMock::authenticate(); BLEMock::subscribe(3);
    }
    loop();
    assert(latestReport() == active); // Unchanged physical inputs are resent after reconnect.
    checkDriveFiltering();
    checkFilterModes();
    checkOutputStability();
    checkReadRecovery();
    checkTiming();
    checkPortReadStatus();
    checkFlexibleMapping();
    checkFlyPreset();
    checkMouseAndKeyboard();
    checkEncoders();
    checkPortScan();
    std::cout << "M5StackController " << (Config::transport == HIDTransport::USB ? "USB" : "BLE")
        << " units, bindings, encoders, mouse/keyboard, port scan, lost-input release, and reconnect passed\n";
}
