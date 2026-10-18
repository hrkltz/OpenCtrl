#pragma once
#include <NimBLEDevice.h>
#include "HIDTransportBackend.hpp"

void testWebControllerUSB() {
    // Use the WebController's exact profile selection and default transport.
    HIDPen pen;
    assert(pen.begin());
    assert(HIDUtil::Init(HIDUtil::All));
    assert(BLEMock::initialized == 0 && !HIDUtil::Connected(HIDTransport::Bluetooth));
    const size_t sizes[] = {8,5,11,2,1};
    for (uint8_t id = 1; id <= 5; ++id) {
        assert(HIDUtil::InputSize(id) == sizes[id - 1]);
        assert(HIDUtil::Transports(id) == HIDTransport::USB);
    }
    assert(HIDUtil::InputSize(7) == 7 && HIDUtil::Transports(7) == HIDTransport::USB);
    assert(HTTPServerUtil::Init());
    assert(call("/keyboard", "{\"keys\":[4],\"modifiers\":2}") == 200);
    assert(USBMock::sent.back().id == 1 && USBMock::sent.back().data[2] == 4);
    assert(call("/mouse", "{\"dx\":10,\"buttons\":1}") == 200);
    assert(USBMock::sent.back().id == 2 && USBMock::sent.back().data == std::vector<uint8_t>({1,10,0,0,0}));
    assert(call("/gamepad", "{\"x\":20,\"buttons\":1}") == 200);
    assert(USBMock::sent.back().id == 3 && USBMock::sent.back().data[0] == 20);
    assert(call("/consumer", "{\"usage\":205}") == 200 && USBMock::sent.back().id == 4);
    assert(call("/system", "{\"usage\":3}") == 200 && USBMock::sent.back().id == 5);
    assert(pen.send(true,true,false,false,100,200,300));
    assert(USBMock::sent.back().id == 7 && USBMock::sent.back().data[0] == 9);
    assert(call("/release", "{}") == 200);
    auto releasedPen = std::find_if(USBMock::sent.rbegin(), USBMock::sent.rend(), [](const auto& packet) { return packet.id == 7; });
    assert(releasedPen != USBMock::sent.rend() && releasedPen->data == std::vector<uint8_t>(7));
    USBMock::mounted = false;
    HIDUtil::Service();
    assert(!HIDUtil::Ready() && !HIDUtil::SendMouse(1,0,0,0,0));
    USBMock::sent.clear();
    USBMock::mounted = true;
    HIDUtil::Service();
    assert(USBMock::sent.size() == 6 && BLEMock::sent.empty());
    for (const auto& packet : USBMock::sent) {
        auto expected = std::vector<uint8_t>(packet.data.size());
        if (packet.id == 3) expected[2] = expected[3] = 129;
        assert(packet.data == expected);
    }
    assert(call("/mouse", "{\"dx\":1}") == 200);
    std::cout << "WebController USB profiles, HTTP inputs, pen, releases, and reconnect compatibility passed\n";
}

void testProfileValues() {
    auto mouse = HIDProfiles::Mouse();
    auto second = mouse;
    assert(second.RemapReportIds(42));
    assert(mouse.neutral[0].id == 1 && second.neutral[0].id == 42);
    std::vector<HIDReportLayout> layouts;
    assert(ParseHIDDescriptor(second.descriptor.data(), second.descriptor.size(), layouts));
    assert(layouts.size() == 1 && layouts[0].id == 42 && layouts[0].input == 5);
    assert(layouts[0].relativeMask == std::vector<uint8_t>({0,255,255,255,255}));
    const auto unchanged = second.descriptor;
    assert(!second.RemapReportIds(0) && unchanged == second.descriptor);
    HIDProfile multiple{{0xa1,1,0x85,1,0x75,8,0x95,1,0x81,2,0x85,2,0x81,2,0xc0}, {{1,{0}},{2,{0}}}, {}};
    assert(!multiple.RemapReportIds(255));
    assert(multiple.RemapReportIds(20) && multiple.neutral[1].id == 21);
    HIDProfile unused{{0xa1,1,0x85,1,0x85,2,0x75,8,0x95,1,0x81,2,0xc0}, {{2,{0}}}, {}};
    assert(unused.RemapReportIds(20) && unused.neutral[0].id == 21);
    assert(ParseHIDDescriptor(unused.descriptor.data(), unused.descriptor.size(), layouts));
    // ATT attributes are limited to 512 bytes; USB can use a longer map.
    auto huge = mouse;
    std::vector<uint8_t> padding;
    for (size_t i = 0; i < 256; ++i) { padding.push_back(0x05); padding.push_back(1); }
    huge.descriptor.insert(huge.descriptor.begin(), padding.begin(), padding.end());
    assert(ParseHIDDescriptor(huge.descriptor.data(), huge.descriptor.size(), layouts));
    assert(!HIDUtil::RegisterProfile(huge, HIDTransport::Bluetooth, 200));
}

void testBluetoothOnly() {
    testProfileValues();
    assert(HIDUtil::Init(HIDUtil::All, {}, HIDTransport::Bluetooth));
    assert(BLEMock::initialized == 1);
    assert(!HIDUtil::Connected() && !HIDUtil::Ready());
    assert(!HIDUtil::Connected(HIDTransport::USB));
    assert(BLEMock::bonding && BLEMock::secureConnections && !BLEMock::mitm);
    BLEMock::connect();
    assert(BLEMock::forceAuthentication);
    BLEMock::subscribe(2);
    assert(!HIDUtil::ReadyForReport(2));
    assert(!HIDUtil::SendMouse(1, 0, 0, 0, 0));
    BLEMock::authenticate();
    HIDUtil::Service();
    assert(HIDUtil::Ready(HIDTransport::Bluetooth) && !HIDUtil::Ready(HIDTransport::USB));
    assert(BLEMock::sent.back().id == 2 && BLEMock::sent.back().data == std::vector<uint8_t>(5));
    USBMock::mounted = false;
    assert(HTTPServerUtil::Init());
    assert(call("/mouse", "{\"dx\":10,\"buttons\":1}") == 200);
    mockMillis += 500;
    expireSession();
    assert(!owner.empty()); // A missing USB cable must not expire a BLE session.
    assert(call("/heartbeat", "{}") == 200);
    const auto count = BLEMock::sent.size();
    BLEMock::disconnect();
    expireSession();
    assert(owner.empty() && !HIDUtil::Connected());
    assert(BLEMock::sent.size() == count);
    BLEMock::connect();
    BLEMock::authenticate();
    BLEMock::subscribe(2);
    HIDUtil::Service();
    assert(BLEMock::sent.back().data == std::vector<uint8_t>(5));
    const auto* identity = BLEMock::characteristic(0x2a50);
    assert(identity && identity->value == std::vector<uint8_t>({2,0x86,0x28,0x56,0,0,1}));
    BLEMock::disconnect();
    BLEMock::connect();
    BLEMock::authenticate(1);
    assert(!HIDUtil::Connected(HIDTransport::Bluetooth));
    assert(NimBLEDevice::server.disconnected.back() == BLEMock::peer.getConnHandle());
    std::cout << "Bluetooth-only pairing, HTTP, and reconnect tests passed\n";
}

void testMixedTransports() {
    testProfileValues();
    const auto mouse = HIDProfiles::Mouse();
    assert(HIDUtil::RegisterProfile(mouse, HIDTransport::USB, 2));
    assert(HIDUtil::RegisterProfile(mouse, HIDTransport::Bluetooth, 7));
    assert(HIDUtil::RegisterProfile(mouse, HIDTransport::USB, 10));
    assert(HIDUtil::RegisterProfile(mouse, HIDTransport::Bluetooth, 11));
    assert(HIDUtil::RegisterProfile(HIDProfiles::Keyboard(), HIDTransport::Bluetooth, 8));
    // A mirrored custom instance exercises all report types and a larger ATT payload.
    HIDProfile custom{{0xa1,1,0x85,1,0x75,8,0x95,32,0x81,2,0x95,1,0x91,2,0xb1,2,0xc0},
        {{1,std::vector<uint8_t>(32)}}, {}};
    unsigned outputCalls = 0, featureCalls = 0;
    custom.callbacks.output = [&](uint8_t id, const uint8_t* data, size_t length) {
        assert(id == 9 && length == 1 && data[0] == 5); ++outputCalls;
    };
    custom.callbacks.setFeature = [&](uint8_t id, const uint8_t*, size_t length) {
        assert(id == 9 && length == 1); ++featureCalls;
    };
    custom.callbacks.getFeature = [](uint8_t id, uint8_t* data, uint16_t length) {
        assert(id == 9 && length == 1); data[0] = 42; return uint16_t(1);
    };
    assert(HIDUtil::RegisterProfile(custom, HIDTransport::Both, 9));
    assert(!HIDUtil::RegisterProfile(mouse, HIDTransport::Bluetooth, 2));
    assert(HIDUtil::Init(0));
    assert(HIDUtil::Ready(HIDTransport::USB) && !HIDUtil::Ready(HIDTransport::Bluetooth));
    assert(HIDUtil::SendMouse(15, 0, 0, 0, 1, 2));
    assert(HIDUtil::SendMouse(20, 0, 0, 0, 2, 10));
    uint8_t state[32]{};
    assert(tud_hid_get_report_cb(0,2,HID_REPORT_TYPE_INPUT,state,5) == 5 && state[0] == 1 && state[1] == 0);
    assert(tud_hid_get_report_cb(0,10,HID_REPORT_TYPE_INPUT,state,5) == 5 && state[0] == 2);
    assert(!HIDUtil::SendMouse(1, 0, 0, 0, 0, 7));
    BLEMock::connect();
    BLEMock::authenticate();
    BLEMock::subscribe(7);
    BLEMock::subscribe(11);
    BLEMock::subscribe(8);
    BLEMock::subscribe(9);
    HIDUtil::Service();
    assert(HIDUtil::ReadyForReport(7) && !HIDUtil::ReadyForReport(1));
    assert(!HIDBLEBackend().Ready(9)); // 32-byte report cannot fit the default MTU.
    NimBLEDevice::server.callbacks->onMTUChange(68, BLEMock::peer);
    HIDUtil::Service();
    assert(HIDBLEBackend().Ready(9));
    assert(HIDUtil::SendMouse(30, -30, 0, 0, 4, 7));
    assert(HIDUtil::SendMouse(-10, 0, 0, 0, 2, 11));
    auto* secondMouse = BLEMock::report(11);
    secondMouse->callbacks->onRead(secondMouse, BLEMock::peer);
    assert(secondMouse->value == std::vector<uint8_t>({2,0,0,0,0}));
    auto* bleMouse = BLEMock::report(7);
    bleMouse->callbacks->onRead(bleMouse, BLEMock::peer);
    assert(bleMouse->value == std::vector<uint8_t>({4,0,0,0,0}));
    assert(tud_hid_get_report_cb(0,2,HID_REPORT_TYPE_INPUT,state,5) == 5 && state[0] == 1);
    std::vector<uint8_t> bytes(32, 3);
    assert(HIDUtil::SendInput(9, bytes.data(), bytes.size()));
    assert(USBMock::sent.back().id == 9 && BLEMock::sent.back().id == 9);
    bytes[0] = 8;
    const auto before = BLEMock::sent.size();
    assert(HIDUtil::SendInputTo(9, bytes.data(), bytes.size(), HIDTransport::USB));
    assert(BLEMock::sent.size() == before);
    auto* mirrored = BLEMock::report(9);
    mirrored->callbacks->onRead(mirrored, BLEMock::peer);
    assert(mirrored->value[0] == 3);
    assert(tud_hid_get_report_cb(0,9,HID_REPORT_TYPE_INPUT,state,32) == 32 && state[0] == 8);
    assert(!HIDUtil::SendInputTo(7, state, 5, HIDTransport::USB));
    auto* output = BLEMock::report(9, 2);
    uint8_t value = 5;
    output->setValue(&value, 1);
    NimBLEConnInfo unauthorized{1,{false}};
    output->callbacks->onWrite(output, unauthorized);
    assert(outputCalls == 0);
    output->callbacks->onWrite(output, BLEMock::peer);
    tud_hid_set_report_cb(0,9,HID_REPORT_TYPE_OUTPUT,&value,1);
    assert(outputCalls == 2);
    value = 6;
    auto* feature = BLEMock::report(9, 3);
    feature->setValue(&value, 1);
    feature->callbacks->onWrite(feature, BLEMock::peer);
    assert(featureCalls == 1);
    feature->callbacks->onRead(feature, BLEMock::peer);
    assert(feature->value[0] == 42);
    const uint8_t invalid[] = {5,5};
    output->setValue(invalid, 2);
    output->callbacks->onWrite(output, BLEMock::peer);
    assert(outputCalls == 2);
    assert((output->properties & NIMBLE_PROPERTY::WRITE_ENC) != 0);
    NimBLEConnInfo secondHost{2,{true}};
    NimBLEDevice::server.callbacks->onConnect(&NimBLEDevice::server, secondHost);
    assert(NimBLEDevice::server.disconnected.back() == 2);
    NimBLEDevice::server.callbacks->onDisconnect(&NimBLEDevice::server, secondHost, 0);
    assert(HIDUtil::Ready(HIDTransport::Bluetooth));
    BLEMock::failures = 2;
    assert(!HIDUtil::SendMouse(5, 0, 0, 0, 1, 7));
    assert(HIDUtil::SendMouse(5, 0, 0, 0, 1, 2)); // Failed BLE neutral cannot block USB.
    HIDUtil::Service();
    assert(BLEMock::sent.back().id == 7 && BLEMock::sent.back().data == std::vector<uint8_t>(5));
    auto* control = BLEMock::characteristic(0x2a4c);
    value = 0;
    control->setValue(&value, 1);
    control->callbacks->onWrite(control, BLEMock::peer);
    assert(!HIDUtil::Ready(HIDTransport::Bluetooth));
    assert(HIDUtil::SendMouse(1, 0, 0, 0, 1, 2));
    value = 1;
    control->setValue(&value, 1);
    control->callbacks->onWrite(control, BLEMock::peer);
    HIDUtil::Service();
    assert(HIDUtil::Ready(HIDTransport::Bluetooth));
    BLEMock::subscribe(7, false);
    assert(!HIDUtil::SendMouse(1, 0, 0, 0, 0, 7));
    BLEMock::disconnect();
    HIDUtil::Service();
    assert(HIDUtil::ReadyForReport(2) && !HIDUtil::ReadyForReport(7));
    assert(tud_hid_get_report_cb(0,2,HID_REPORT_TYPE_INPUT,state,5) == 5 && state[0] == 1);
    bytes[0] = 11;
    assert(HIDUtil::SendInput(9, bytes.data(), bytes.size())); // Mirroring skips an absent link.
    BLEMock::connect();
    BLEMock::authenticate();
    BLEMock::subscribe(7);
    HIDUtil::Service();
    assert(BLEMock::sent.back().data == std::vector<uint8_t>(5));
    BLEMock::disconnect();
    mockMillis += 1000;
    HIDUtil::Service();
    assert(NimBLEDevice::advertising.starts == 2);
    assert(HTTPServerUtil::Init());
    assert(call("/hid/report", "{\"id\":9,\"data\":[0]}") == 400);
    assert(call("/hid/report", "{\"id\":7,\"data\":[0,0,0,0,0],\"transport\":\"usb\"}") == 400);
    assert(call("/hid/report", "{\"id\":2,\"data\":[0,1,0,0,0],\"transport\":\"usb\"}") == 200);
    assert(call("/hid/report", "{\"id\":2,\"data\":[0,1,0,0,0],\"transport\":4}") == 400);
    assert(call("/hid/report", "{\"id\":2,\"data\":[0,1,0,0,0],\"transport\":\"other\"}") == 400);
    std::cout << "Composed profiles, multiple mice, mixed/mirrored transports, BLE GATT, and recovery tests passed\n";
}
