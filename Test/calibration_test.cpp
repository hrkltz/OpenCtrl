#include <cassert>
#include <iostream>
#include <ControllerCalibration.hpp>

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
int main() {
    M5Stack::AxisCalibration x, y;
    y.inverted = true;
    ControllerCalibration calibration;
    assert(calibration.Configure("fly-cal", {{"x","Roll",&x},{"y","Pitch",&y}}));
    assert(!calibration.Load());
    assert(calibration.Begin("FlyCtrl","FlyController"));
    assert(strlen(calibration.Password()) == 16 && calibration.Address() == "192.168.4.1");
    assert(request("/", HTTP_GET).body.find("Calibrate your controls") != std::string::npos);
    assert(request("/api/calibration/start", HTTP_POST, nullptr, false).status == 403);
    calibration.UpdateRaw(0,32000,true);
    calibration.UpdateRaw(1,33000,true);
    auto live = state();
    assert(live["axes"][0]["raw"] == 32000 && live["axes"][1]["online"] == true && live["active"] == false);
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
    assert(state()["saved"] == false);
    apply(calibration,"/api/calibration/end");
    assert(x.center == 32768 && !calibration.Active());
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
    assert(rebooted.Configure("fly-cal", {{"x","Roll",&restoredX},{"y","Pitch",&restoredY}}));
    assert(rebooted.Load() && restoredX.center == 32000 && restoredY.center == 33000 && restoredY.inverted);
    M5Stack::AxisCalibration otherX, otherY;
    ControllerCalibration other;
    assert(other.Configure("drive-cal", {{"x","Steering",&otherX},{"y","Pedals",&otherY}}) && !other.Load());
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
    PreferencesMock::storage["fly-cal"]["settings"] = "{broken";
    assert(!rebooted.Load() && restoredX.center == 32000);

    // Analog adapters use millivolts; each binding supplies its own raw range.
    M5Stack::AxisCalibration fader{0,1650,3300,0,false}, angle{0,1250,2500,0,false};
    ControllerCalibration analog;
    assert(analog.Configure("analog-cal", {{"fader","Throttle",&fader,0,3300},{"angle","Trim",&angle,0,2500}}));
    assert(analog.Begin("Analog","Analog inputs"));
    analog.UpdateRaw(0,3000,true);
    analog.UpdateRaw(1,2400,true);
    auto analogState = state();
    assert(analogState["axes"][0]["raw"] == 3000 && analogState["axes"][1]["raw"] == 2400);
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
    std::cout << "Calibration page/API, validation, preview, persistence, reset, and session expiry passed\n";
}
