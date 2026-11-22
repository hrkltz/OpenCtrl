#include <DisplayUtil.hpp>
#include <HIDUtil.hpp>
#include <M5StackInputs.hpp>
#include <ControllerCalibration.hpp>
#include <WiFi.h>
#include "Config.hpp"
#include "Mapping.hpp"
#include "AxisFilter.hpp"
#include "PortScan.hpp"
#include "WebFiles.hpp"
#include <algorithm>
#include <array>
#include <atomic>
#include <mutex>
#include <utility>

#ifndef WIFI_SSID
#define WIFI_SSID ""
#endif
#ifndef WIFI_PASSWORD
#define WIFI_PASSWORD ""
#endif

// One adapter of each unit type per port; the port's configured unit decides which one is read.
template <class T, size_t... Port> std::array<T, sizeof...(Port)> MakeUnits(uint8_t address, std::index_sequence<Port...>) {
  return {{T(Wire, {address, int8_t(Port), Config::hubAddress})...}};
}
auto sticks = MakeUnits<M5Stack::Joystick2>(0x63, std::make_index_sequence<Config::portCount>());
auto encoders = MakeUnits<M5Stack::Encoder>(0x40, std::make_index_sequence<Config::portCount>());
auto scrolls = MakeUnits<M5Stack::Scroll>(0x40, std::make_index_sequence<Config::portCount>());
// A turn position moves between its calibrated ends and starts at center (0).
std::array<M5Stack::AxisCalibration, Config::portCount> turns = [] {
  std::array<M5Stack::AxisCalibration, Config::portCount> values;
  values.fill({-Config::turnRange, 0, Config::turnRange, 0, false});
  return values;
}();
std::array<int32_t, Config::portCount> positions{};
std::array<bool, Config::portCount> encoderReady{};
// Changed only by calibration.Service() in this loop, so a new table applies between two cycles.
std::vector<ControllerBinding> bindings;
std::vector<uint8_t> ports(Config::portCount, NoUnit);
ControllerCalibration calibration;
ControllerFilterSettings filterSettings;
std::array<AxisFilter, Config::maxBindings> filters;
BindingMapper mapper;
ControllerInputDiagnostics inputDiagnostics;
bool started = false;
bool inputHealthy = true;

// Setup page state: the loop owns the bus, HTTP callbacks only copy this snapshot.
std::mutex portsMutex;
PortScan portScan;
std::array<int8_t, Config::portCount> portPressed; // -1: not read this cycle.
std::atomic<bool> scanRequested{false};

// Last mouse buttons and keys sent, so changes are sent once.
struct SentState {
  uint8_t mouseButtons = 0, modifiers = 0;
  std::array<uint8_t, 6> keys{};
} sent;

M5Stack::AxisCalibration& AxisSettings(size_t index) {
  const size_t port = index / Sources::axesPerPort;
  switch (index % Sources::axesPerPort) {
    case Sources::X: return sticks[port].x;
    case Sources::Y: return sticks[port].y;
    default: return turns[port];
  }
}

// Every port has X, Y, and turn settings, so changing a port's unit keeps its saved calibration.
std::vector<CalibrationAxis> CalibrationBindings() {
  constexpr size_t count = Config::portCount * Sources::axesPerPort;
  static char ids[count][12], labels[count][20];
  const char* kinds[][2] = {{"x", "X"}, {"y", "Y"}, {"turn", "turn"}};
  std::vector<CalibrationAxis> axes;
  for (size_t i = 0; i < count; ++i) {
    const auto& kind = kinds[i % Sources::axesPerPort];
    snprintf(ids[i], sizeof(ids[i]), "port%u.%s", unsigned(i / Sources::axesPerPort), kind[0]);
    snprintf(labels[i], sizeof(labels[i]), "Port %u · %s", unsigned(i / Sources::axesPerPort), kind[1]);
    // Turn positions count steps from the start position.
    if (i % Sources::axesPerPort == Sources::Turn) axes.push_back({ids[i], labels[i], &AxisSettings(i), -32768, 32767});
    else axes.push_back({ids[i], labels[i], &AxisSettings(i)});
  }
  return axes;
}

void WaitForNextPoll(uint32_t pollStartedAt) {
  uint32_t elapsed = micros() - pollStartedAt;
  if (elapsed >= Config::pollMicroseconds) return;
  // Yield whole milliseconds, then finish the remaining fraction precisely.
  delay((Config::pollMicroseconds - elapsed) / 1000);
  elapsed = micros() - pollStartedAt;
  if (elapsed < Config::pollMicroseconds) delayMicroseconds(Config::pollMicroseconds - elapsed);
}

bool ReadUnit(size_t port, PortReading& reading) {
  reading = {};
  if (ports[port] == Joystick) {
    if (!sticks[port].ReadRaw(reading.stick)) return false;
    reading.pressed = reading.stick.pressed;
    return true;
  }
  if (ports[port] != Encoder && ports[port] != Scroll) return false;
  // An encoder forgets its pulse mode when it powers up again, so select it after every failure.
  if (ports[port] == Encoder && !encoderReady[port] && !(encoderReady[port] = encoders[port].Begin())) return false;
  M5Stack::EncoderState state;
  if (!(ports[port] == Encoder ? encoders[port].Read(state) : scrolls[port].Read(state))) {
    encoderReady[port] = false;
    return false;
  }
  reading.steps = state.delta;
  reading.pressed = state.pressed;
  return true;
}

bool ReadPort(size_t port, PortReading& reading, uint32_t pollStartedAt) {
  if (ReadUnit(port, reading)) return true;
  ++inputDiagnostics.readFailures;
  // Retry a transient failure with a fresh read, only while this cycle has time left.
  if (uint32_t(micros() - pollStartedAt) >= Config::pollMicroseconds || !ReadUnit(port, reading)) return false;
  ++inputDiagnostics.recoveredReads;
  return true;
}

void SendWebFile(AsyncWebServerRequest* request, const WebFile& file) {
  auto* response = request->beginResponse(200, file.contentType, file.data, file.size);
  response->addHeader("Cache-Control", "no-cache");
  if (file.gzip) response->addHeader("Content-Encoding", "gzip");
  request->send(response);
}

void SendPorts(AsyncWebServerRequest* request) {
  JsonDocument doc;
  {
    std::lock_guard<std::mutex> lock(portsMutex);
    doc["scanned"] = portScan.scanned;
    doc["hub"] = portScan.hub;
    doc["hubAddress"] = Config::hubAddress;
    auto direct = doc["direct"].to<JsonArray>();
    for (uint8_t address : portScan.direct) direct.add(address);
    auto ports = doc["ports"].to<JsonArray>();
    for (size_t port = 0; port < Config::portCount; ++port) {
      auto item = ports.add<JsonObject>();
      auto devices = item["devices"].to<JsonArray>();
      for (uint8_t address : portScan.ports[port]) devices.add(address);
      if (portPressed[port] < 0) item["pressed"] = nullptr;
      else item["pressed"] = portPressed[port] == 1;
    }
  }
  String json;
  serializeJson(doc, json);
  auto* response = request->beginResponse(200, "application/json", json);
  response->addHeader("Cache-Control", "no-store");
  request->send(response);
}

void AddRoutes() {
  AsyncWebServer& server = calibration.Server();
  for (const WebFile& file : WEB_FILES) {
    server.on(file.path, HTTP_GET, [&file](AsyncWebServerRequest* request) { SendWebFile(request, file); });
    if (strcmp(file.path, "/index.html") == 0)
      server.on("/", HTTP_GET, [&file](AsyncWebServerRequest* request) { SendWebFile(request, file); });
  }
  server.on(AsyncURIMatcher::exact("/api/ports"), HTTP_GET, SendPorts);
  server.on("/api/ports/scan", HTTP_POST, [](AsyncWebServerRequest* request) {
    if (request->header("X-OpenCtrl-Calibration") != "1") { request->send(403, "text/plain", "Use the settings page"); return; }
    scanRequested = true; // Runs on the next setup cycle.
    request->send(202, "text/plain", "");
  });
}

// Sends only the devices the bindings use. Gamepad state is absolute and resent every cycle;
// mouse and keyboard reports go out on change and are refreshed while something is held.
void Send(const ControllerReport& report, uint32_t cycle) {
  const bool refresh = cycle % Config::refreshCycles == 0;
  const bool connected = HIDUtil::Connected(Config::transport);
  auto count = [connected](bool ok) { if (!ok && connected) ++inputDiagnostics.sendFailures; };
  if (report.gamepad)
    count(HIDUtil::SendGamepad(report.axes[0], report.axes[1], report.axes[2], report.axes[3],
        report.axes[4], report.axes[5], 0, report.buttons));
  if (report.mouse && (report.dx || report.dy || report.wheel || report.mouseButtons != sent.mouseButtons ||
      (report.mouseButtons && refresh))) {
    const bool ok = HIDUtil::SendMouse(report.dx, report.dy, report.wheel, 0, report.mouseButtons);
    if (ok) sent.mouseButtons = report.mouseButtons;
    count(ok);
  }
  const bool held = report.modifiers || report.keys[0];
  if (report.keyboard && (report.modifiers != sent.modifiers || report.keys != sent.keys || (held && refresh))) {
    const bool ok = HIDUtil::SendKeyboard(report.modifiers, report.keys.data());
    if (ok) { sent.modifiers = report.modifiers; sent.keys = report.keys; }
    count(ok);
  }
}

void setup() {
  Serial.begin(115200);
  portPressed.fill(-1);
  if (!Wire.begin(SDA, SCL)) return;
  Wire.setClock(Config::i2cFrequency);
  Wire.setTimeOut(20);
  DisplayUtil::Init();
  DisplayUtil::Print(0, "M5StackController");
  DisplayUtil::Print(2, "Center sticks");
  delay(500);

  if (!calibration.Configure("m5-ctrl", CalibrationBindings(), &filterSettings, MakeMapping(&bindings, &ports))) {
    DisplayUtil::PrintError("Settings init");
    return;
  }
  // Saved values take precedence; absent/invalid settings use startup calibration.
  if (!calibration.Load()) {
    for (size_t port = 0; port < Config::portCount; ++port)
      if (ports[port] == Joystick) sticks[port].CalibrateCenter();
  }
  if (!HIDUtil::Init(HIDUtil::Standard, {}, Config::transport)) {
    DisplayUtil::PrintError("HID init");
    return;
  }
  DisplayUtil::Print(1, Config::transport == HIDTransport::USB ? "USB HID" :
      Config::transport == HIDTransport::Bluetooth ? "BLE: OpenCtrl" : "USB + BLE");
  WiFi.mode(WIFI_STA);
  WiFi.setAutoReconnect(true);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  AddRoutes();
  if (calibration.Begin("M5StackController")) {
    DisplayUtil::Print(2, "WiFi connecting");
    DisplayUtil::Print(4, "Open IP to set up");
  } else DisplayUtil::Print(2, "Setup WiFi failed");
  inputDiagnostics.pollMicroseconds = Config::pollMicroseconds;
  started = true;
}

void loop() {
  const uint32_t pollStartedAt = micros();
  HIDUtil::Service();
  if (!started) { WaitForNextPoll(pollStartedAt); return; }
  static bool wifiConnected = false;
  const bool wifiOnline = WiFi.status() == WL_CONNECTED;
  if (wifiOnline != wifiConnected) {
    DisplayUtil::Print(2, wifiOnline ? "http://" : "WiFi connecting");
    DisplayUtil::Print(3, wifiOnline ? calibration.Address().c_str() : "");
    wifiConnected = wifiOnline;
  }
  calibration.Service();

  // A new table starts clean: release everything and drop old smoothing. A port with a new unit
  // starts its turn position at center.
  static std::vector<ControllerBinding> previousBindings;
  static std::vector<uint8_t> previousPorts(Config::portCount, NoUnit);
  if (bindings != previousBindings || ports != previousPorts) {
    for (auto& filter : filters) filter.Reset();
    mapper.Reset();
    HIDUtil::ReleaseAll();
    sent = {};
    for (size_t port = 0; port < Config::portCount; ++port) {
      if (ports[port] == previousPorts[port]) continue;
      positions[port] = 0;
      encoderReady[port] = false;
      encoders[port].ResetTracking();
      scrolls[port].ResetTracking();
    }
    previousBindings = bindings;
    previousPorts = ports;
  }

  // Scan ports while setup pauses HID: on entering setup, on request, and periodically.
  static bool wasActive = false;
  static uint32_t lastScan = 0;
  if (calibration.Active() && (!wasActive || scanRequested || millis() - lastScan >= Config::scanMilliseconds)) {
    scanRequested = false;
    auto scan = ScanPorts(Wire);
    lastScan = millis();
    std::lock_guard<std::mutex> lock(portsMutex);
    portScan = std::move(scan);
  }
  wasActive = calibration.Active();

  // Read only ports with bindings; setup reads every port with a unit for live readings.
  std::array<bool, Config::portCount> needed{};
  for (const auto& binding : bindings) needed[Sources::Port(binding.source)] = true;
  std::array<PortReading, Config::portCount> reading{};
  std::array<bool, Config::portCount> online{};
  std::array<CalibrationRawReading, Config::portCount * Sources::axesPerPort> readings{};
  std::array<int8_t, Config::portCount> pressed;
  bool healthy = true;
  for (size_t port = 0; port < Config::portCount; ++port) {
    const bool present = ports[port] != NoUnit;
    if (needed[port]) online[port] = ReadPort(port, reading[port], pollStartedAt);
    else if (calibration.Active() && present) online[port] = ReadUnit(port, reading[port]);
    if (needed[port] && !online[port]) healthy = false;
    // The position stays between the calibrated ends; Normalize() applies inversion to it,
    // and steps for mouse and button bindings follow the same direction.
    if (online[port]) positions[port] = std::clamp(positions[port] + reading[port].steps, turns[port].minimum, turns[port].maximum);
    if (turns[port].inverted) reading[port].steps = -reading[port].steps;
    const bool sampled = present && (needed[port] || calibration.Active());
    const bool joystick = ports[port] == Joystick;
    readings[port * 3 + Sources::X] = {reading[port].stick.x, online[port], sampled && joystick};
    readings[port * 3 + Sources::Y] = {reading[port].stick.y, online[port], sampled && joystick};
    readings[port * 3 + Sources::Turn] = {positions[port], online[port], sampled && !joystick};
    pressed[port] = sampled && online[port] ? reading[port].pressed : -1;
  }
  if (healthy != inputHealthy) {
    DisplayUtil::Print(5, healthy ? "Inputs ready" : "Input lost");
    inputHealthy = healthy;
  }

  std::array<BindingInput, Config::maxBindings> inputs{};
  if (!healthy || calibration.Active()) {
    // Neutral report from all-rest inputs, e.g. centered rudder but released pedals.
    if (!healthy) ++inputDiagnostics.inputLossCycles;
    for (auto& filter : filters) filter.Reset();
    mapper.Reset();
  } else {
    for (size_t i = 0; i < bindings.size(); ++i) {
      const uint8_t source = bindings[i].source;
      const size_t port = Sources::Port(source);
      switch (Sources::KindOf(source)) {
        case Sources::Click: inputs[i].pressed = reading[port].pressed; break;
        case Sources::Turn:
          // Steps are exact, so turn positions need no smoothing.
          inputs[i].value = turns[port].Normalize(positions[port]);
          inputs[i].steps = reading[port].steps;
          break;
        default: {
          const size_t axis = Sources::Axis(source);
          const uint16_t value = Sources::KindOf(source) == Sources::Y ? reading[port].stick.y : reading[port].stick.x;
          inputs[i].value = AxisSettings(axis).Normalize(filters[i].Update(value, filterSettings));
        }
      }
    }
  }
  Send(mapper.Map(bindings, inputs.data(), filterSettings.mode != ControllerFilterMode::Off), inputDiagnostics.cycleCount);

  calibration.UpdateInputs(readings.data(), readings.size(), &inputDiagnostics);
  {
    std::lock_guard<std::mutex> lock(portsMutex);
    portPressed = pressed;
  }
  // Publish timing next cycle so the measurement includes the snapshot above.
  const uint32_t workMicroseconds = micros() - pollStartedAt;
  ++inputDiagnostics.cycleCount;
  inputDiagnostics.lastWorkMicroseconds = workMicroseconds;
  inputDiagnostics.maxWorkMicroseconds = std::max(inputDiagnostics.maxWorkMicroseconds, workMicroseconds);
  if (workMicroseconds > Config::pollMicroseconds) ++inputDiagnostics.overrunCycles;
  WaitForNextPoll(pollStartedAt);
}
