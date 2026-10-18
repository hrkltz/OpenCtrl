#pragma once
#include "USBHID.h"
#define HID_PROTOCOL_BOOT 0
#define HID_PROTOCOL_REPORT 1
inline uint8_t mockProtocol=HID_PROTOCOL_REPORT;
inline uint8_t tud_hid_n_get_protocol(uint8_t) { return mockProtocol; }
enum hid_report_type_t { HID_REPORT_TYPE_INPUT=1, HID_REPORT_TYPE_OUTPUT=2, HID_REPORT_TYPE_FEATURE=3 };
extern "C" {
const uint8_t* tud_hid_descriptor_report_cb(uint8_t);
uint16_t tud_hid_get_report_cb(uint8_t,uint8_t,hid_report_type_t,uint8_t*,uint16_t);
void tud_hid_set_report_cb(uint8_t,uint8_t,hid_report_type_t,const uint8_t*,uint16_t);
void tud_hid_report_complete_cb(uint8_t,const uint8_t*,uint16_t);
bool tud_hid_set_idle_cb(uint8_t,uint8_t);
void tud_hid_set_protocol_cb(uint8_t,uint8_t);
void tud_hid_report_failed_cb(uint8_t,hid_report_type_t,const uint8_t*,uint16_t);
}
inline bool tud_mounted() { return USBMock::mounted; }
inline bool tud_hid_n_ready(uint8_t) { return USBMock::mounted && USBMock::ready; }
inline bool tud_hid_n_report(uint8_t,uint8_t id,const void* data,uint16_t length) {
    const auto* bytes=static_cast<const uint8_t*>(data);
    USBMock::Packet packet{id,{bytes,bytes+length}};
    USBMock::attempts.push_back(packet);
    if(USBMock::failures) { --USBMock::failures; return false; }
    USBMock::sent.push_back(packet);
    if (USBMock::timeouts) --USBMock::timeouts;
    else if (USBMock::completionFailures) {
        --USBMock::completionFailures;
        tud_hid_report_failed_cb(0,HID_REPORT_TYPE_INPUT,bytes,length);
    } else tud_hid_report_complete_cb(0,bytes,length);
    return true;
}
