#include <cassert>
#include <iostream>
#include <cstdlib>
#include <cstring>
#include <tusb.h>
#include <NimBLEDevice.h>
#include "HIDTransportBackend.hpp"
#include "../Source/XIAOESP32S3Core/HTTPServerUtil.cpp"
#include "../Examples/WebController/XIAOESP32S3Core/XIAOESP32S3Core.ino"

void DisplayUtil::Init() {}
void DisplayUtil::Print(uint8_t, const char*) {}
void DisplayUtil::PrintError(const char*) { assert(false && "WebController initialization failed"); }
void WiFiUtil::Connect() {}
static void assertPairingCode() {
    const char* code = HTTPServerUtil::GetPairingToken();
    assert(code && code[0] && std::strlen(code) <= 2);
    for (const char* digit = code; *digit; ++digit) assert(*digit >= '0' && *digit <= '9');
    assert(std::strtol(code, nullptr, 10) <= 99);
}

static int call(const char* path, const char* json, bool authorized = true) {
    AsyncWebServerRequest req;
    if (authorized) req.headers["Authorization"] = std::string("Bearer ") + HTTPServerUtil::GetPairingToken();
    req.headers["X-OpenCtrl-Session"] = "11111111111111111111111111111111";
    JsonDocument document;
    assert(!deserializeJson(document, json));
    auto body = document.as<JsonVariant>();
    AsyncWebServer::routes.at(path)(&req, body);
    return req.status;
}

static void checkRoute(const char* path, const char* json, uint8_t id, HIDTransport transport) {
    USBMock::sent.clear(); BLEMock::sent.clear();
    assert(call(path, json) == 200);
    if (transport == HIDTransport::USB)
        assert(!USBMock::sent.empty() && USBMock::sent.back().id == id && BLEMock::sent.empty());
    else assert(!BLEMock::sent.empty() && BLEMock::sent.back().id == id && USBMock::sent.empty());
}

int main() {
    setup();
    assertPairingCode();
    const bool bluetooth = OPENCTRL_WEB_MODE != 1;
    assert(BLEMock::initialized == unsigned(bluetooth));
    assert(HIDUtil::Transports(1) == WebConfig::primaryTransport);
    assert(HIDUtil::Transports(2) == WebConfig::primaryTransport);
    assert(HIDUtil::Transports(3) == WebConfig::gamepadTransport);
    assert(HIDUtil::Transports(4) == WebConfig::primaryTransport);
    assert(HIDUtil::Transports(5) == WebConfig::primaryTransport);
    assert(HIDUtil::Transports(7) == WebConfig::penTransport && HIDUtil::InputSize(7) == 7);
    assert(HIDUtil::InputSize(8) == (WebConfig::secondaryMouse ? 5 : 0));
    assert(AsyncWebServer::routes.count("/mouse/secondary") == size_t(WebConfig::secondaryMouse));
    if (OPENCTRL_WEB_MODE == 2) assert(!HIDUtil::Connected(HIDTransport::USB));
    else {
        mockProtocol = HID_PROTOCOL_BOOT;
        assert(HIDUSBBackend().Active(1) && !HIDUSBBackend().Active(2));
        mockProtocol = HID_PROTOCOL_REPORT;
    }
    if (bluetooth) {
        BLEMock::connect(); BLEMock::authenticate();
        for (uint8_t id : {1,2,3,4,5,7,8})
            if (HIDUtil::Transports(id) == HIDTransport::Bluetooth) BLEMock::subscribe(id);
    }
    HIDUtil::Service();
    AsyncWebServerRequest info;
    AsyncWebServer::plainRoutes.at({"/controller",HTTP_GET})(&info);
    JsonDocument metadata;
    assert(info.status == 200 && !deserializeJson(metadata,info.body.c_str()));
    assert(metadata["mode"] == WebConfig::name && metadata["secondaryMouse"] == WebConfig::secondaryMouse);
    AsyncWebServerRequest page;
    AsyncWebServer::plainRoutes.at({"/index.html",HTTP_GET})(&page);
    assert(page.status == 200 && page.body == "WebController page");
    // Session acquisition releases every profile before accepting fresh input.
    assert(call("/heartbeat", "{}") == 200);
    checkRoute("/keyboard", "{\"keys\":[4]}", 1, WebConfig::primaryTransport);
    checkRoute("/mouse", "{\"dx\":10,\"buttons\":1}", 2, WebConfig::primaryTransport);
    checkRoute("/gamepad", "{\"x\":20}", 3, WebConfig::gamepadTransport);
    checkRoute("/consumer", "{\"usage\":205}", 4, WebConfig::primaryTransport);
    checkRoute("/system", "{\"usage\":3}", 5, WebConfig::primaryTransport);
    checkRoute("/pen", "{\"tip\":true,\"inRange\":true,\"x\":100,\"y\":200,\"pressure\":300}", 7, WebConfig::penTransport);
    if (WebConfig::secondaryMouse) {
        assert(call("/mouse/secondary", "{\"dx\":1}", false) == 401);
        assert(call("/mouse/secondary", "{\"dx\":4097}") == 400);
        assert(call("/mouse/secondary", "{\"buttons\":32}") == 400);
        checkRoute("/mouse/secondary", "{\"dx\":12,\"buttons\":2}", 8, HIDTransport::Bluetooth);
        assert(BLEMock::sent.back().data == std::vector<uint8_t>({2,12,0,0,0}));
        BLEMock::disconnect();
        HIDUtil::Service();
        checkRoute("/mouse", "{\"dx\":1}", 2, HIDTransport::USB);
        assert(call("/mouse/secondary", "{\"dx\":1}") == 503);
        BLEMock::connect(); BLEMock::authenticate(); BLEMock::subscribe(3); BLEMock::subscribe(8);
        HIDUtil::Service();
        checkRoute("/mouse/secondary", "{\"buttons\":0}", 8, HIDTransport::Bluetooth);
    }
    assert(call("/release", "{}") == 200);
    const auto checkPenRelease = [](const auto& released) {
        auto found = std::find_if(released.rbegin(),released.rend(),[](const auto& packet) { return packet.id == 7; });
        assert(found != released.rend() && found->data == std::vector<uint8_t>(7));
    };
    if (WebConfig::penTransport == HIDTransport::USB) checkPenRelease(USBMock::sent);
    else checkPenRelease(BLEMock::sent);
    std::cout << "WebController " << WebConfig::name << ": actual sketch, routes, profiles, browser metadata, and releases passed\n";
}
