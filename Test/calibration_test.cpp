#include <cassert>
#include <iostream>
#include <array>
#include <atomic>
#include <thread>
#include <ControllerCalibration.hpp>
#include <WiFi.h>

AsyncWebServerRequest request(const char* path, int method, const JsonDocument* doc = nullptr, bool allowed = true) {
    AsyncWebServerRequest req;
    if (allowed) req.headers["X-OpenCtrl-Calibration"] = "1";
    if (doc) {
        JsonDocument copy = *doc;
        auto body = copy.as<JsonVariant>();
        AsyncWebServer::routes.at(path)(&req, body);
    } else AsyncWebServer::plainRoutes.at({path,method})(&req);
    return req;
}
JsonDocument state() {
    const auto result = request("/api/calibration", HTTP_GET);
    assert(result.status == 200);
    JsonDocument doc;
    assert(!deserializeJson(doc, result.body.c_str()));
    return doc;
}
void apply(ControllerCalibration& service, const char* route, const JsonDocument* doc = nullptr) {
    auto result = request(route, HTTP_POST, doc);
    assert(result.status == 202);
    service.Service();
}
void checkInputSnapshots() {
    M5Stack::AxisCalibration x, y;
    ControllerCalibration calibration;
    assert(calibration.Configure("snapshots",{{"x","X",&x},{"y","Y",&y}}));
    auto* mutex = mockSemaphores.back().get();
    assert(calibration.Begin("Snapshots"));
    CalibrationRawReading inputs[] = {{100,true},{65535,true}};
    ControllerInputDiagnostics diagnostics;
    diagnostics.pollMicroseconds = 4000;
    diagnostics.cycleCount = 100;
    diagnostics.lastWorkMicroseconds = 3500;
    diagnostics.maxWorkMicroseconds = 5000;
    diagnostics.overrunCycles = 2;
    auto takes = mutex->takes;
    calibration.UpdateInputs(inputs,2,&diagnostics);
    assert(mutex->takes == takes + 1); // One lock publishes readings and diagnostics together.
    auto snapshot = state();
    assert(snapshot["axes"][0]["raw"] == 100 && snapshot["axes"][1]["raw"] == 65535);
    assert(snapshot["axes"][0]["sampled"] == true);
    assert(snapshot["diagnostics"]["timing"]["cycleCount"] == 100);
    assert(snapshot["diagnostics"]["timing"]["lastWorkMicroseconds"] == 3500);
    assert(snapshot["diagnostics"]["timing"]["maxWorkMicroseconds"] == 5000);
    assert(snapshot["diagnostics"]["timing"]["overrunCycles"] == 2);
    assert(snapshot["diagnostics"]["timing"]["pollMicroseconds"] == 4000);
    inputs[0] = {-1,true};
    calibration.UpdateInputs(inputs,1,&diagnostics); // Incomplete cycles are ignored.
    calibration.UpdateInputs(nullptr,2,&diagnostics);
    assert(state()["axes"][0]["raw"] == 100);
    calibration.UpdateInputs(inputs,2);
    assert(state()["axes"][0]["online"] == false);
    assert(state()["diagnostics"]["timing"]["cycleCount"] == 100); // Optional diagnostics are retained.
    inputs[0] = {100,true,false};
    calibration.UpdateInputs(inputs,2);
    snapshot = state();
    assert(snapshot["axes"][0]["sampled"] == false && snapshot["axes"][0]["online"] == false);
    calibration.UpdateRaw(0,100,true);
    assert(state()["axes"][0]["sampled"] == true);
    inputs[0] = {100,true};
    calibration.UpdateInputs(inputs,2);
    apply(calibration,"/api/calibration/start");
    apply(calibration,"/api/calibration/capture");
    inputs[0] = {60000,true}; inputs[1] = {500,false};
    calibration.UpdateInputs(inputs,2);
    inputs[0] = {32000,true}; inputs[1] = {32000,true};
    calibration.UpdateInputs(inputs,2);
    snapshot = state();
    assert(snapshot["axes"][0]["observedMinimum"] == 100 && snapshot["axes"][0]["observedMaximum"] == 60000);
    assert(snapshot["axes"][1]["observedMinimum"] == 32000); // Offline samples do not enter range capture.
    apply(calibration,"/api/calibration/end");

    // HTTP readers must see one whole cycle even while the input loop keeps publishing.
    inputs[0] = {0,true}; inputs[1] = {65535,true}; diagnostics.cycleCount = 0;
    calibration.UpdateInputs(inputs,2,&diagnostics);
    std::atomic<bool> writing{true};
    std::thread writer([&] {
        for (uint32_t i = 1; i <= 1000; ++i) {
            CalibrationRawReading cycle[] = {{int32_t(i),true},{int32_t(65535-i),true}};
            ControllerInputDiagnostics counters;
            counters.pollMicroseconds = 4000; counters.cycleCount = i;
            calibration.UpdateInputs(cycle,2,&counters);
            std::this_thread::yield();
        }
        writing = false;
    });
    unsigned reads = 0;
    do {
        const auto current = state();
        const uint32_t cycle = current["diagnostics"]["timing"]["cycleCount"];
        assert(current["axes"][0]["raw"] == cycle && current["axes"][1]["raw"] == 65535-cycle);
        ++reads;
    } while (writing || reads < 100);
    writer.join();
    assert(state()["diagnostics"]["timing"]["cycleCount"] == 1000);
}
void checkBindings() {
    using S = ControllerSignal; using O = ControllerOutput; using M = ControllerBindingMode;
    // Axes drive anything; buttons only drive buttons; half-travel modes make an axis a button.
    assert(ControllerBindingValid(S::Axis,O::Axis,M::Normal) && ControllerBindingValid(S::Axis,O::Axis,M::Negative));
    assert(ControllerBindingValid(S::Axis,O::Motion,M::Reversed) && !ControllerBindingValid(S::Axis,O::Motion,M::Positive));
    assert(ControllerBindingValid(S::Axis,O::Button,M::Positive) && !ControllerBindingValid(S::Axis,O::Button,M::Normal));
    assert(ControllerBindingValid(S::Button,O::Button,M::Normal) && !ControllerBindingValid(S::Button,O::Button,M::Positive));
    assert(!ControllerBindingValid(S::Button,O::Axis,M::Normal) && !ControllerBindingValid(S::Button,O::Motion,M::Normal));

    M5Stack::AxisCalibration x;
    std::vector<ControllerBinding> bindings{{0,0,M::Normal}};
    auto mapping = [&bindings] {
        ControllerMapping value;
        value.sources = {{"stick.x","Stick X",S::Axis},{"stick.button","Stick click",S::Button}};
        value.targets = {{"pad.x","Pad X",O::Axis},{"pad.a","Pad A",O::Button},{"mouse.x","Mouse X",O::Motion}};
        value.bindings = &bindings;
        value.maximum = 3;
        return value;
    };
    ControllerCalibration invalid;
    auto duplicate = mapping();
    duplicate.targets[1].id = "pad.x";
    assert(!invalid.Configure("binding-test",{{"stick.x","Stick X",&x}},nullptr,duplicate));
    auto wrongDefault = mapping();
    bindings = {{1,0,M::Normal}}; // A button cannot drive an axis.
    assert(!invalid.Configure("binding-test",{{"stick.x","Stick X",&x}},nullptr,wrongDefault));
    bindings = {{0,0,M::Normal}};

    ControllerCalibration service;
    assert(service.Configure("binding-test",{{"stick.x","Stick X",&x}},nullptr,mapping()));
    assert(!service.Load());
    assert(service.Begin("Bindings"));
    // Every route is reachable on the real server, which also matches paths below a plain URI.
    for (const auto& [path, method] : {std::pair<const char*,int>{"/api/calibration",HTTP_GET}, {"/api/calibration/options",HTTP_GET},
            {"/api/calibration/mapping",HTTP_POST}, {"/api/calibration/start",HTTP_POST}, {"/api/calibration/stop-capture",HTTP_POST}})
        assert(AsyncWebServer::Resolve(path,method) == path);
    auto options = request("/api/calibration/options",HTTP_GET);
    JsonDocument metadata;
    assert(options.status == 200 && !deserializeJson(metadata,options.body.c_str()));
    assert(metadata["maximum"] == 3 && metadata["sources"][1]["signal"] == "button" && metadata["targets"][2]["output"] == "motion");
    auto current = state();
    assert(current["mapping"]["saved"] == false && current["mapping"]["bindings"].size() == 1);
    assert(current["mapping"]["bindings"][0]["target"] == "pad.x" && current["mapping"]["bindings"][0]["mode"] == "normal");

    JsonDocument body;
    body["version"] = 1;
    auto list = body["bindings"].to<JsonArray>();
    auto add = [&list](const char* source, const char* target, const char* mode) {
        auto item = list.add<JsonObject>(); item["source"] = source; item["target"] = target; item["mode"] = mode;
    };
    add("stick.x","pad.x","negative"); add("stick.button","pad.a","normal"); add("stick.x","mouse.x","reversed");
    for (int i = 0; i < 9; ++i) {
        auto bad = body;
        switch (i) {
            case 0: bad["version"] = 2; break;
            case 1: bad["bindings"][0]["source"] = "stick.y"; break;
            case 2: bad["bindings"][0]["target"] = "pad.z"; break;
            case 3: bad["bindings"][0]["mode"] = "sideways"; break;
            case 4: bad["bindings"][1]["target"] = "pad.x"; break;       // Button to axis.
            case 5: bad["bindings"][2]["mode"] = "positive"; break;      // Half travel on motion.
            case 6: bad["bindings"][0].remove("mode"); break;
            case 7: bad["bindings"][0]["source"] = 0; break;
            case 8: bad["bindings"].add<JsonObject>().set(bad["bindings"][0]); break; // Over the maximum.
        }
        assert(request("/api/calibration/mapping",HTTP_POST,&bad).status == 400);
    }
    assert(request("/api/calibration/mapping",HTTP_POST,&body,false).status == 403);
    PreferencesMock::failWrites = true;
    apply(service,"/api/calibration/mapping",&body);
    assert(state()["operationStatus"] == 503 && bindings.size() == 1);
    PreferencesMock::failWrites = false;
    auto queued = request("/api/calibration/mapping",HTTP_POST,&body);
    assert(queued.status == 202 && bindings.size() == 1); // Applied by Service() in the input loop.
    service.Service();
    assert(bindings.size() == 3 && bindings[0] == (ControllerBinding{0,0,M::Negative}));
    assert(bindings[1] == (ControllerBinding{1,1,M::Normal}) && bindings[2] == (ControllerBinding{0,2,M::Reversed}));
    assert(state()["mapping"]["saved"] == true && state()["mapping"]["bindings"][2]["target"] == "mouse.x");
    JsonDocument empty;
    empty["version"] = 1; empty["bindings"].to<JsonArray>();

    // Saved bindings load without axis calibration; broken records keep the defaults.
    bindings = {};
    ControllerCalibration rebooted;
    assert(rebooted.Configure("binding-test",{{"stick.x","Stick X",&x}},nullptr,mapping()));
    assert(!rebooted.Load() && bindings.size() == 3 && bindings[2].mode == M::Reversed);
    PreferencesMock::storage["binding-test"]["mapping"] = "{\"version\":1,\"bindings\":[{\"source\":\"stick.button\",\"target\":\"pad.x\",\"mode\":\"normal\"}]}";
    bindings = {{0,0,M::Normal}};
    ControllerCalibration fallback;
    assert(fallback.Configure("binding-test",{{"stick.x","Stick X",&x}},nullptr,mapping()));
    assert(!fallback.Load() && bindings.size() == 1 && bindings[0] == (ControllerBinding{0,0,M::Normal}));
    apply(service,"/api/calibration/start");
    apply(service,"/api/calibration/mapping",&empty); // Allowed during setup.
    assert(bindings.empty() && state()["mapping"]["bindings"].size() == 0 && service.Active());
    apply(service,"/api/calibration/end");
}

void checkPorts() {
    using S = ControllerSignal; using O = ControllerOutput; using M = ControllerBindingMode;
    // Port 0 or 1 holds a stick (X, click) or a knob (turn, click); unit 0 means nothing connected.
    M5Stack::AxisCalibration x;
    std::vector<ControllerBinding> bindings;
    std::vector<uint8_t> ports{1, 0};
    auto mapping = [&] {
        ControllerMapping value;
        value.units = {{"none","Nothing"},{"stick","Stick"},{"knob","Knob"}};
        value.sources = {{"p0.x","Port 0 X",S::Axis,0,1u << 1},{"p0.turn","Port 0 turn",S::Relative,0,1u << 2},
            {"p0.click","Port 0 click",S::Button,0,1u << 1 | 1u << 2},{"p1.turn","Port 1 turn",S::Relative,1,1u << 2}};
        value.targets = {{"pad.x","Pad X",O::Axis},{"pad.a","Pad A",O::Button},{"wheel","Wheel",O::Motion}};
        value.bindings = &bindings;
        value.ports = &ports;
        return value;
    };
    ControllerCalibration invalid;
    bindings = {{1,2,M::Normal}}; // Port 0 holds a stick, which has no turn.
    assert(!invalid.Configure("ports-test",{{"x","X",&x}},nullptr,mapping()));
    bindings = {};
    ports = {3, 0}; // Unknown unit.
    assert(!invalid.Configure("ports-test",{{"x","X",&x}},nullptr,mapping()));
    ports = {1, 0};
    ControllerCalibration service;
    assert(service.Configure("ports-test",{{"x","X",&x}},nullptr,mapping()));
    assert(!service.Load() && service.Begin("Ports"));
    auto options = request("/api/calibration/options",HTTP_GET);
    JsonDocument metadata;
    assert(!deserializeJson(metadata,options.body.c_str()));
    assert(metadata["units"][2]["id"] == "knob" && metadata["sources"][1]["signal"] == "relative");
    assert(metadata["sources"][2]["port"] == 0 && metadata["sources"][2]["units"].size() == 2);
    assert(state()["mapping"]["ports"][0] == "stick" && state()["mapping"]["ports"][1] == "none");

    JsonDocument body;
    body["version"] = 1;
    body["ports"].add("knob"); body["ports"].add("knob");
    auto list = body["bindings"].to<JsonArray>();
    auto add = [&list](const char* source, const char* target, const char* mode) {
        auto item = list.add<JsonObject>(); item["source"] = source; item["target"] = target; item["mode"] = mode;
    };
    add("p0.turn","pad.x","positive"); add("p1.turn","wheel","reversed"); add("p0.turn","pad.a","negative"); add("p0.click","pad.a","normal");
    for (int i = 0; i < 6; ++i) {
        auto bad = body;
        switch (i) {
            case 0: bad.remove("ports"); break;
            case 1: bad["ports"].as<JsonArray>().remove(1); break;
            case 2: bad["ports"][0] = "slider"; break;
            case 3: bad["ports"][0] = "stick"; break;  // A stick has no turn.
            case 4: bad["ports"][1] = "none"; break;   // Nothing on port 1 to turn.
            case 5: bad["ports"][0] = 2; break;
        }
        assert(request("/api/calibration/mapping",HTTP_POST,&bad).status == 400);
    }
    PreferencesMock::failWrites = true;
    apply(service,"/api/calibration/mapping",&body);
    assert(ports == std::vector<uint8_t>({1, 0}) && bindings.empty());
    PreferencesMock::failWrites = false;
    apply(service,"/api/calibration/mapping",&body);
    // Ports and bindings change together.
    assert(ports == std::vector<uint8_t>({2, 2}) && bindings.size() == 4 && bindings[0] == (ControllerBinding{1,0,M::Positive}));
    assert(state()["mapping"]["ports"][1] == "knob");
    ports = {0, 0}; bindings = {};
    ControllerCalibration rebooted;
    assert(rebooted.Configure("ports-test",{{"x","X",&x}},nullptr,mapping()));
    assert(!rebooted.Load() && ports == std::vector<uint8_t>({2, 2}) && bindings.size() == 4);
}
int main() {
    M5Stack::AxisCalibration x, y;
    y.inverted = true;
    ControllerCalibration calibration;
    assert(calibration.Configure("axis-cal", {{"x","Roll",&x},{"y","Pitch",&y}}));
    assert(!calibration.Load());
    assert(calibration.Begin("Axes") && calibration.Address() == "192.168.4.1");
    assert(!AsyncWebServer::plainRoutes.count({"/",HTTP_GET})); // The consuming controller serves its page.
    assert(request("/api/calibration/start", HTTP_POST, nullptr, false).status == 403);
    calibration.UpdateRaw(0,32000,true);
    calibration.UpdateRaw(1,33000,true);
    auto live = state();
    assert(live["axes"][0]["raw"] == 32000 && live["axes"][1]["online"] == true && live["active"] == false);
    assert(live["axes"][0]["rawMinimum"] == 0 && live["axes"][0]["rawMaximum"] == 65535);
    assert(live["diagnostics"].isUnbound());
    calibration.UpdateDiagnostics({2,4,1,5});
    auto diagnosticState = state();
    assert(diagnosticState["diagnostics"]["readFailures"] == 2);
    assert(diagnosticState["diagnostics"]["recoveredReads"] == 4);
    assert(diagnosticState["diagnostics"]["inputLossCycles"] == 1);
    assert(diagnosticState["diagnostics"]["sendFailures"] == 5);
    assert(diagnosticState["diagnostics"]["timing"].isUnbound());
    assert(PreferencesMock::writes == 0);
    auto queue = request("/api/calibration/start", HTTP_POST);
    assert(queue.status == 202 && !calibration.Active());
    assert(request("/api/calibration/start", HTTP_POST).status == 409);
    calibration.Service();
    assert(calibration.Active());
    apply(calibration,"/api/calibration/capture");
    calibration.UpdateRaw(0,100,true);
    calibration.UpdateRaw(0,65000,true);
    calibration.UpdateRaw(0,32000,true);
    auto captured = state();
    assert(captured["capturing"] == true && captured["axes"][0]["observedMinimum"] == 100 && captured["axes"][0]["observedMaximum"] == 65000);
    apply(calibration,"/api/calibration/stop-capture");
    assert(state()["capturing"] == false);

    auto candidate = state();
    candidate["axes"][0]["center"] = 32000;
    candidate["axes"][0]["minimum"] = 1000;
    candidate["axes"][0]["maximum"] = 64000;
    candidate["axes"][0]["deadzone"] = 0.025f;
    candidate["axes"][1]["center"] = 33000;
    for (int invalid = 0; invalid < 9; ++invalid) {
        auto bad = candidate;
        switch (invalid) {
            case 0: bad["axes"][0]["center"] = 0; break;
            case 1: bad["axes"][0]["minimum"] = -1; break;
            case 2: bad["axes"][0]["maximum"] = 65536; break;
            case 3: bad["axes"][0]["deadzone"] = 1; break;
            case 4: bad["axes"][0]["inverted"] = "true"; break;
            case 5: bad["axes"][0]["id"] = "unknown"; break;
            case 6: bad["version"] = 2; break;
            case 7: bad["axes"][0].remove("center"); break;
            case 8: bad["axes"].as<JsonArray>().remove(1); break;
        }
        assert(request("/api/calibration/save", HTTP_POST, &bad).status == 400);
        assert(x.center == 32768 && PreferencesMock::writes == 0);
    }
    apply(calibration,"/api/calibration/preview",&candidate);
    assert(x.center == 32000 && y.center == 33000 && PreferencesMock::writes == 0);
    assert(x.minimum == 1000 && x.maximum == 64000 && std::abs(x.deadzone - 0.025f) < 0.000001f);
    assert(state()["saved"] == false);
    apply(calibration,"/api/calibration/end");
    assert(x.center == 32768 && !calibration.Active());
    assert(x.minimum == 0 && x.maximum == 65535 && x.deadzone == 0.05f);
    assert(request("/api/calibration/save",HTTP_POST,&candidate).status == 409);
    apply(calibration,"/api/calibration/start");
    PreferencesMock::failWrites = true;
    apply(calibration,"/api/calibration/save",&candidate);
    assert(x.center == 32768 && state()["operationStatus"] == 503);
    PreferencesMock::failWrites = false;
    apply(calibration,"/api/calibration/save",&candidate);
    assert(x.center == 32000 && y.center == 33000 && state()["saved"] == true && PreferencesMock::writes == 1);
    apply(calibration,"/api/calibration/end");
    assert(x.center == 32000);

    M5Stack::AxisCalibration restoredX, restoredY;
    ControllerCalibration rebooted;
    assert(rebooted.Configure("axis-cal", {{"x","Roll",&restoredX},{"y","Pitch",&restoredY}}));
    assert(rebooted.Load() && restoredX.center == 32000 && restoredY.center == 33000 && restoredY.inverted);
    assert(restoredX.minimum == 1000 && restoredX.maximum == 64000 && std::abs(restoredX.deadzone - 0.025f) < 0.000001f);
    M5Stack::AxisCalibration otherX, otherY;
    ControllerCalibration other;
    assert(other.Configure("other-cal", {{"x","Steering",&otherX},{"y","Pedals",&otherY}}) && !other.Load());
    assert(otherX.center == 32768);
    apply(calibration,"/api/calibration/start");
    auto preview = candidate;
    preview["axes"][0]["center"] = 31000;
    apply(calibration,"/api/calibration/preview",&preview);
    mockMillis += 30001;
    calibration.Service();
    assert(!calibration.Active() && x.center == 32000 && PreferencesMock::writes == 1);
    apply(calibration,"/api/calibration/start");
    apply(calibration,"/api/calibration/reset");
    assert(x.center == 32768 && y.inverted && PreferencesMock::writes == 2 && state()["saved"] == true);
    calibration.UpdateRaw(0,-1,true);
    assert(state()["axes"][0]["online"] == false);
    // Malformed/incompatible persisted settings never partially apply.
    PreferencesMock::storage["axis-cal"]["settings"] = "{broken";
    assert(!rebooted.Load() && restoredX.center == 32000);

    // Analog adapters use millivolts; each binding supplies its own raw range.
    M5Stack::AxisCalibration fader{0,1650,3300,0,false}, angle{0,1250,2500,0,false};
    ControllerCalibration analog;
    assert(analog.Configure("analog-cal", {{"fader","Throttle",&fader,0,3300},{"angle","Trim",&angle,0,2500}}));
    assert(analog.Begin("Analog inputs"));
    analog.UpdateRaw(0,3000,true);
    analog.UpdateRaw(1,2400,true);
    auto analogState = state();
    assert(analogState["axes"][0]["raw"] == 3000 && analogState["axes"][1]["raw"] == 2400);
    assert(analogState["axes"][0]["rawMaximum"] == 3300 && analogState["axes"][1]["rawMaximum"] == 2500);
    apply(analog,"/api/calibration/start");
    auto analogSettings = state();
    analogSettings["axes"][0]["maximum"] = 3100;
    analogSettings["axes"][1]["maximum"] = 2400;
    auto outOfRange = analogSettings;
    outOfRange["axes"][1]["maximum"] = 2501;
    assert(request("/api/calibration/save",HTTP_POST,&outOfRange).status == 400);
    apply(analog,"/api/calibration/save",&analogSettings);
    assert(fader.maximum == 3100 && angle.maximum == 2400 && state()["saved"] == true);
    apply(analog,"/api/calibration/end");
    M5Stack::AxisCalibration loadedFader{0,1650,3300,0,false}, loadedAngle{0,1250,2500,0,false};
    ControllerCalibration analogRebooted;
    assert(analogRebooted.Configure("analog-cal", {{"fader","Throttle",&loadedFader,0,3300},{"angle","Trim",&loadedAngle,0,2500}}));
    assert(analogRebooted.Load() && loadedFader.maximum == 3100 && loadedAngle.maximum == 2400);

    // Home-network settings need no pairing; mutations retain their custom header.
    M5Stack::AxisCalibration steering;
    ControllerFilterSettings filter;
    ControllerCalibration network;
    assert(network.Configure("filter-test", {{"steering","Steering",&steering}}, &filter));
    assert(!network.Load());
    assert(network.Begin("Filters"));
    assert(request("/api/calibration",HTTP_GET,nullptr,false).status == 200);
    assert(request("/api/calibration/start",HTTP_POST,nullptr,false).status == 403);
    auto defaults = state();
    assert(!defaults["filter"]["saved"].as<bool>() && defaults["filter"]["mode"] == "median-3");
    assert(defaults["pairingCode"].isUnbound());
    JsonDocument filterBody;
    filterBody["version"] = 1; filterBody["mode"] = "ema"; filterBody["alpha"] = 0.5f;
    for (int invalid = 0; invalid < 8; ++invalid) {
        auto bad = filterBody;
        switch (invalid) {
            case 0: bad["mode"] = "unknown"; break;
            case 1: bad["alpha"] = 0; break;
            case 2: bad["alpha"] = 1.01; break;
            case 3: bad["alpha"] = "0.5"; break;
            case 4: bad["alpha"] = true; break;
            case 5: bad.remove("alpha"); break;
            case 6: bad["version"] = 2; break;
            case 7: bad["alpha"] = NAN; break;
        }
        assert(request("/api/calibration/filter",HTTP_POST,&bad).status == 400);
        assert(filter.mode == ControllerFilterMode::Median3);
    }
    assert(request("/api/calibration/filter",HTTP_POST,&filterBody,false).status == 403);
    auto queuedFilter = request("/api/calibration/filter",HTTP_POST,&filterBody);
    assert(queuedFilter.status == 202 && filter.mode == ControllerFilterMode::Median3);
    assert(request("/api/calibration/filter",HTTP_POST,&filterBody).status == 409);
    network.Service();
    assert(!network.Active() && filter.mode == ControllerFilterMode::EMA && filter.alpha == 0.5f);
    assert(state()["filter"]["saved"] == true && state()["operationStatus"] == 200);
    filterBody["mode"] = "off";
    PreferencesMock::failWrites = true;
    apply(network,"/api/calibration/filter",&filterBody);
    assert(filter.mode == ControllerFilterMode::EMA && state()["operationStatus"] == 503);
    PreferencesMock::failWrites = false;
    apply(network,"/api/calibration/filter",&filterBody);
    assert(filter.mode == ControllerFilterMode::Off);
    M5Stack::AxisCalibration restoredSteering;
    ControllerFilterSettings restoredFilter;
    ControllerCalibration restoredNetwork;
    assert(restoredNetwork.Configure("filter-test", {{"steering","Steering",&restoredSteering}}, &restoredFilter));
    assert(!restoredNetwork.Load()); // No axis calibration yet, but the independent filter setting loads.
    assert(restoredFilter.mode == ControllerFilterMode::Off && restoredFilter.alpha == 0.5f);
    apply(network,"/api/calibration/start");
    apply(network,"/api/calibration/reset");
    apply(network,"/api/calibration/end");
    assert(filter.mode == ControllerFilterMode::Off); // Calibration reset/expiry do not change filtering.
    PreferencesMock::storage["filter-test"]["filter"] = "{broken";
    ControllerFilterSettings fallbackFilter;
    ControllerCalibration fallback;
    assert(fallback.Configure("filter-test", {{"steering","Steering",&restoredSteering}}, &fallbackFilter));
    assert(fallback.Load() && fallbackFilter.mode == ControllerFilterMode::Median3);
    checkBindings();
    checkPorts();
    checkInputSnapshots();
    std::cout << "Calibration page/API, validation, preview, persistence, reset, and session expiry passed\n";
}
