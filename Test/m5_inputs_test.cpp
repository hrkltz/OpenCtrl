#include <cassert>
#include <cmath>
#include <iostream>
#include <limits>
#include <M5StackInputs.hpp>
#include <tusb.h>
#include <NimBLEDevice.h>

#if TEST_FLIGHT
#include "../Examples/FlyController/XIAOESP32S3Core/XIAOESP32S3Core.ino"
#else
#include "../Examples/DriveController/XIAOESP32S3Core/XIAOESP32S3Core.ino"
#endif

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

const std::vector<uint8_t>& latestReport() {
    if (Config::transport == HIDTransport::USB) return USBMock::sent.back().data;
    return BLEMock::sent.back().data;
}

int main() {
    checkAdapters();
    Wire.SetJoystick(2,32768,32768);
    Wire.SetJoystick(3,32768,32768);
    setup();
    assert(started && Wire.beginSDA == SDA && Wire.beginSCL == SCL);
    assert(primary.x.center == 32768 && secondary.y.center == 32768);
    assert(HIDUtil::Transports(3) == Config::transport && HIDUtil::InputSize(3) == 11);
    if (Config::transport == HIDTransport::Bluetooth) {
        assert(BLEMock::initialized == 1 && !HIDUtil::Connected(HIDTransport::USB));
        BLEMock::connect(); BLEMock::authenticate(); BLEMock::subscribe(3);
    } else assert(BLEMock::initialized == 0);
    loop();
    std::vector<uint8_t> neutral = {0,0,129,129,0,0,0,0,0,0,0};
#if TEST_FLIGHT
    neutral[3] = 0;
#endif
    assert(latestReport() == neutral);
    Wire.SetJoystick(2,65535,0,true);
    Wire.SetJoystick(3,65535,0,true);
    loop();
    const auto active = latestReport();
    assert(active[0] == 127);
#if TEST_FLIGHT
    assert(int8_t(active[1]) == -127 && active[2] == 127 && active[3] == 127 && active[7] == 3);
#else
    assert(active[2] == 129 && active[3] == 127 && active[7] == 192);
    Wire.SetJoystick(3,32768,65535,false);
    loop();
    assert(latestReport()[2] == 127 && latestReport()[3] == 129);
#endif
    Wire.missingChannels.insert(2);
    loop();
    assert(!inputHealthy && latestReport() == neutral);
    Wire.missingChannels.clear();
    Wire.SetJoystick(3,65535,0,true);
    loop();
    assert(inputHealthy && latestReport() == active);
    AsyncWebServerRequest enter;
    enter.headers["X-OpenCtrl-Calibration"] = "1";
    AsyncWebServer::plainRoutes.at({"/api/calibration/start",HTTP_POST})(&enter);
    assert(enter.status == 202);
    loop();
    assert(calibration.Active() && latestReport() == neutral);
    AsyncWebServerRequest live;
    AsyncWebServer::plainRoutes.at({"/api/calibration",HTTP_GET})(&live);
    JsonDocument settings;
    assert(!deserializeJson(settings, live.body.c_str()));
    assert(settings["axes"][0]["raw"] == 65535);
    settings["axes"][0]["center"] = 32000;
    auto body = settings.as<JsonVariant>();
    AsyncWebServerRequest save;
    save.headers["X-OpenCtrl-Calibration"] = "1";
    AsyncWebServer::routes.at("/api/calibration/save")(&save,body);
    assert(save.status == 202);
    loop();
    assert(primary.x.center == 32000 && calibration.Active() && latestReport() == neutral);
    AsyncWebServerRequest leave;
    leave.headers["X-OpenCtrl-Calibration"] = "1";
    AsyncWebServer::plainRoutes.at({"/api/calibration/end",HTTP_POST})(&leave);
    assert(leave.status == 202);
    loop();
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
    std::cout << (TEST_FLIGHT ? "FlyController" : "DriveController") << " "
        << (Config::transport == HIDTransport::USB ? "USB" : "BLE")
        << " adapters, mapping, lost-input release, and reconnect passed\n";
}
