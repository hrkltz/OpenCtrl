#include <cassert>
#include <iostream>
#include <cstdlib>
#include <cstring>
#include <tusb.h>
#include "../Source/XIAOESP32S3Core/HIDDescriptor.hpp"
#include "../Source/XIAOESP32S3Core/HIDUtil.hpp"
#include "../Examples/WebController/XIAOESP32S3Core/HIDPen.hpp"
// Include the route implementation so its watchdog can be exercised with a fake
// clock, without starting an RTOS task or waiting two real seconds per case.
#include "../Source/XIAOESP32S3Core/HTTPServerUtil.cpp"
void DisplayUtil::Print(uint8_t, const char*) {}

void assertPairingCode() {
    const char* code = HTTPServerUtil::GetPairingToken();
    assert(code && code[0] && std::strlen(code) <= 2);
    for (const char* digit = code; *digit; ++digit) assert(*digit >= '0' && *digit <= '9');
    assert(std::strtol(code, nullptr, 10) <= 99);
}

int call(const char* path, const char* json, const char* session = "11111111111111111111111111111111", bool authorized = true) {
    AsyncWebServerRequest request;
    if (authorized) request.headers["Authorization"] = std::string("Bearer ") + HTTPServerUtil::GetPairingToken();
    request.headers["X-OpenCtrl-Session"] = session;
    JsonDocument doc;
    assert(!deserializeJson(doc, json));
    JsonVariant body = doc.as<JsonVariant>();
    AsyncWebServer::routes.at(path)(&request, body);
    return request.status;
}

#include "hid_transport_test.hpp"

int main(int argc, char** argv) {
    if (argc > 1 && std::string(argv[1]) == "--web-usb") { testWebControllerUSB(); return 0; }
    if (argc > 1 && std::string(argv[1]) == "--bluetooth-only") { testBluetoothOnly(); return 0; }
    if (argc > 1 && std::string(argv[1]) == "--mixed-transports") { testMixedTransports(); return 0; }
    if (argc > 1) {
        const bool customOnly = std::string(argv[1]) == "--custom-only";
        if (customOnly) {
            const uint8_t descriptor[]={MOCK_INPUT(7,1),0xc0};
            assert(HIDUtil::RegisterProfile(descriptor,sizeof(descriptor),{{7,{0}}}));
        }
        assert(HIDUtil::Init(customOnly ? 0 : HIDUtil::Mouse));
        assert(HIDUtil::InputSize(1)==0 && HIDUtil::InputSize(3)==0);
        assert(HIDUtil::InputSize(2)==(customOnly ? 0 : 5));
        assert(HIDUtil::InputSize(7)==(customOnly ? 1 : 0));
        assert(HTTPServerUtil::Init());
        assertPairingCode();
        assert(!AsyncWebServer::routes.count("/keyboard") && !AsyncWebServer::routes.count("/gamepad"));
        assert(AsyncWebServer::routes.count("/mouse")==size_t(!customOnly));
        assert(call("/hid/report","{\"id\":30,\"data\":[0]}")==404);
        std::cout << "Selectable/custom-only profile tests passed\n";
        return 0;
    }
    const uint8_t custom[] = {MOCK_INPUT(7,1),0x91,2,0xb1,2,0xc0};
    const uint8_t output[] = {0x05,1,0x09,1,0xa1,1,0x85,8,0x75,8,0x95,1,0x91,2,0xc0};
    const uint8_t feature[] = {0x05,1,0x09,1,0xa1,1,0x85,9,0x75,8,0x95,2,0xb1,2,0xc0};
    std::vector<HIDReportLayout> layouts;
    assert(ParseHIDDescriptor(custom,sizeof(custom),layouts));
    assert(layouts.size()==1 && layouts[0].id==7 && layouts[0].input==1);
    assert(!ParseHIDDescriptor(custom,sizeof(custom)-1,layouts));
    auto invalid=std::vector<uint8_t>(custom,custom+sizeof(custom));
    invalid[7]=0;
    assert(!ParseHIDDescriptor(invalid.data(),invalid.size(),layouts));
    invalid[7]=7; invalid[11]=64;
    assert(!ParseHIDDescriptor(invalid.data(),invalid.size(),layouts));
    // PUSH/POP restores the ID and report size; bit fields round up to bytes.
    const uint8_t pushed[]={0xa1,1,0x85,10,0x75,1,0x95,3,0x81,2,0xa4,0x85,11,0x75,8,0x95,2,0x81,2,0xb4,0x81,2,0xc0};
    assert(ParseHIDDescriptor(pushed,sizeof(pushed),layouts));
    assert(layouts.size()==2 && layouts[0].input==1 && layouts[1].input==2);
    assert(!HIDUtil::RegisterProfile(custom,sizeof(custom),{}));
    assert(!HIDUtil::RegisterProfile(custom,sizeof(custom),{{7,{0,0}}}));
        uint8_t sharedOutput=0, sharedFeature=0;
    HIDProfileCallbacks shared;
    shared.output=[&](uint8_t,const uint8_t* bytes,size_t) { sharedOutput=bytes[0]; };
    shared.setFeature=[&](uint8_t,const uint8_t* bytes,size_t) { sharedFeature=bytes[0]; };
    shared.getFeature=[](uint8_t,uint8_t* bytes,uint16_t length) { if(length) bytes[0]=99; return uint16_t(length ? 1 : 0); };
    assert(HIDUtil::RegisterProfile(custom,sizeof(custom),{{7,{0}}},shared));
    assert(!HIDUtil::RegisterProfile(custom,sizeof(custom),{{7,{0}}}));
    uint8_t outputValue=0, featureValue=0;
    HIDProfileCallbacks callbacks;
    callbacks.output=[&](uint8_t,const uint8_t* data,size_t) { outputValue=data[0]; };
    assert(HIDUtil::RegisterProfile(output,sizeof(output),{},callbacks));
    callbacks={};
    callbacks.setFeature=[&](uint8_t,const uint8_t* data,size_t) { featureValue=data[0]; };
    callbacks.getFeature=[](uint8_t,uint8_t* data,uint16_t length) { memset(data,42,length); return length; };
    assert(HIDUtil::RegisterProfile(feature,sizeof(feature),{},callbacks));
    const uint8_t relative[]={MOCK_INPUT(12,1),0x95,2,0x81,6,0xc0};
    assert(HIDUtil::RegisterProfile(relative,sizeof(relative),{{12,{0,0,0}}}));
    HIDPen pen;
    assert(pen.begin());
    uint8_t keyboardLeds=0;
    HIDProfileCallbacks standardCallbacks;
    standardCallbacks.output=[&](uint8_t id,const uint8_t* bytes,size_t) { if(id==1) keyboardLeds=bytes[0]; };
    assert(HIDUtil::Init(HIDUtil::All,standardCallbacks));
    assert(BLEMock::initialized == 0);
    HIDUtil::Service();
    assert(!HIDUtil::RegisterProfile(custom,sizeof(custom),{{7,{0}}}));
    const uint8_t data[]={12,34};
    tud_hid_set_report_cb(0,7,HID_REPORT_TYPE_OUTPUT,data,1);
    tud_hid_set_report_cb(0,7,HID_REPORT_TYPE_FEATURE,data+1,1);
    assert(sharedOutput==12 && sharedFeature==34);
    tud_hid_set_report_cb(0,1,HID_REPORT_TYPE_OUTPUT,data,1);
    assert(keyboardLeds==12);
    tud_hid_set_report_cb(0,8,HID_REPORT_TYPE_OUTPUT,data,1);
    assert(outputValue==12);
    const uint8_t interruptOutput[]={8,12};
    tud_hid_set_report_cb(0,0,HID_REPORT_TYPE_OUTPUT,interruptOutput,2);
    assert(outputValue==12);
    tud_hid_set_report_cb(0,9,HID_REPORT_TYPE_FEATURE,data,2);
    assert(featureValue==12);
    uint8_t buffer[4]={};
    assert(tud_hid_get_report_cb(0,9,HID_REPORT_TYPE_FEATURE,buffer,4)==2 && buffer[0]==42);
    assert(!HIDUtil::SendInput(7,data,2));
    assert(HIDUtil::SendInput(7,data,1));
    assert(tud_hid_get_report_cb(0,7,HID_REPORT_TYPE_INPUT,buffer,4)==1 && buffer[0]==12);
    assert(tud_hid_get_report_cb(0,7,HID_REPORT_TYPE_FEATURE,buffer,4)==1 && buffer[0]==99);
    const uint8_t relativeData[]={1,12,34};
    assert(HIDUtil::SendInput(12,relativeData,3));
    uint8_t relativeState[3]={};
    assert(tud_hid_get_report_cb(0,12,HID_REPORT_TYPE_INPUT,relativeState,3)==3);
    assert(relativeState[0]==1 && relativeState[1]==0 && relativeState[2]==0);
    assert(tud_hid_set_idle_cb(0,1));
    mockMillis+=4;
    USBMock::sent.clear();
    HIDUtil::Service();
    auto repeated=std::find_if(USBMock::sent.begin(),USBMock::sent.end(),[](const auto& p){return p.id==12;});
    assert(repeated!=USBMock::sent.end() && repeated->data==std::vector<uint8_t>({1,0,0}));
    assert(tud_hid_set_idle_cb(0,0));
    USBMock::sent.clear();
    assert(HIDUtil::SendMouse(300,-300,0,0,1));
    int dx=0,dy=0;
    assert(USBMock::sent.size()==3);
    for(const auto& packet:USBMock::sent) { dx+=int8_t(packet.data[1]); dy+=int8_t(packet.data[2]); assert(packet.data[0]==1); }
    assert(dx==300 && dy==-300);
    assert(!pen.send(true,true,false,false,65536,0,0));
    assert(!pen.send(true,true,false,false,-1,0,0));
    assert(pen.send(true,true,false,false,32767,32767,1023));
    assert(!HIDUtil::SendGamepad(-128,0,0,0,0,0,0,0));
    uint8_t keys[6]={4};
    assert(HIDUtil::SendKeyboard(2,keys));
    assert(USBMock::sent.back().id==1 && USBMock::sent.back().data[2]==4);
    USBMock::completionFailures=1;
    assert(!HIDUtil::SendKeyboard(0,keys));
    HIDUtil::Service();
    USBMock::timeouts=1;
    assert(!HIDUtil::SendKeyboard(0,keys));
    HIDUtil::Service();
    assert(USBMock::sent.back().id==1 && USBMock::sent.back().data==std::vector<uint8_t>(8));
    // A failed release is retried; a new press must not overtake it.
    USBMock::failures=2;
    assert(!HIDUtil::ReleaseAll());
    assert(!HIDUtil::SendKeyboard(0,keys));
    HIDUtil::Service();
    assert(HIDUtil::SendKeyboard(0,keys));
    USBMock::mounted=false;
    HIDUtil::Service();
    USBMock::mounted=true;
    HIDUtil::Service();
    assert(USBMock::sent.back().data==std::vector<uint8_t>({0})); // System neutral.
    mockProtocol=HID_PROTOCOL_BOOT;
    tud_hid_set_protocol_cb(0,mockProtocol);
    HIDUtil::Service();
    assert(HIDUtil::SendKeyboard(2,keys));
    assert(USBMock::sent.back().id==0 && USBMock::sent.back().data.size()==8);
    assert(!HIDUtil::SendGamepad(0,0,0,0,0,0,0,0));
    assert(HIDUtil::ReleaseAll());
    const uint8_t bootLeds[]={3};
    tud_hid_set_report_cb(0,0,HID_REPORT_TYPE_OUTPUT,bootLeds,1);
    assert(keyboardLeds==3);
    assert(tud_hid_get_report_cb(0,0,HID_REPORT_TYPE_INPUT,buffer,4)==4);
    mockProtocol=HID_PROTOCOL_REPORT;
    tud_hid_set_protocol_cb(0,mockProtocol);
    HIDUtil::Service();
    assert(HIDUtil::SendGamepad(0,0,0,0,0,0,0,0));
    assert(HTTPServerUtil::Init());
    assertPairingCode();
    assert(call("/keyboard","{}","11111111111111111111111111111111",false)==401);
    assert(call("/keyboard","{}","bad-session")==400);
    assert(call("/keyboard","{\"keys\":[4]}")==200);
    assert(USBMock::sent.back().data[2]==4); // No automatic key-up.
    assert(call("/heartbeat","{}","22222222222222222222222222222222")==409);
    assert(call("/keyboard","{\"keys\":[1,2,3,4,5,6,7]}")==400);
    assert(call("/keyboard","{\"keys\":\"4\"}")==400);
    assert(call("/keyboard","{\"modifiers\":256}")==400);
    assert(call("/keyboard","[]")==400);
    assert(call("/gamepad","{\"x\":200}")==400);
    assert(call("/gamepad","{\"x\":\"1\"}")==400);
    assert(call("/gamepad","{\"x\":1.5}")==400);
    assert(call("/gamepad","{\"hat\":9}")==400);
    assert(call("/gamepad","{\"buttons\":-1}")==400);
    assert(call("/gamepad","{\"buttons\":4294967295}")==200);
    assert(call("/mouse","{\"dx\":4097}")==400);
    assert(call("/mouse","{\"dx\":300}")==200);
    assert(call("/consumer","{\"usage\":1024}")==400);
    assert(call("/system","{\"usage\":4}")==400);
    assert(call("/hid/report","{\"id\":7,\"data\":[1,2]}")==400);
    assert(call("/hid/report","{\"id\":7,\"data\":[256]}")==400);
    assert(call("/hid/report","{\"id\":7,\"data\":[1]}")==200);
    USBMock::failures=1;
    assert(call("/keyboard","{\"keys\":[4]}")==503);
    HIDUtil::Service();
    // A heartbeat renews the lease; timeouts release every profile.
    mockMillis=1500;
    assert(call("/heartbeat","{}")==200);
    mockMillis=3000; expireSession(); assert(!owner.empty());
    mockMillis=3500; expireSession(); assert(owner.empty());
    assert(call("/heartbeat","{}","22222222222222222222222222222222")==200);
    WiFi.state=0; expireSession(); assert(owner.empty()); WiFi.state=WL_CONNECTED;
    mockMillis=UINT32_MAX-1000;
    assert(call("/heartbeat","{}")==200);
    mockMillis+=2100; expireSession(); assert(owner.empty());
    assert(call("/heartbeat","{}")==200);
    assert(call("/release","{}")==200 && owner.empty());
    std::cout << "HID descriptor, transmission, validation, pairing, and watchdog tests passed\n";
}
