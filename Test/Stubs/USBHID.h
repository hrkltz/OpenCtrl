#pragma once
#include "USB.h"
#define CFG_TUD_HID_EP_BUFSIZE 64
#define HID_REPORT_ID(id) id
// Minimal descriptors preserve the standard profiles' actual report sizes.
#define MOCK_INPUT(id, count) 0x05,0x01,0x09,0x01,0xa1,0x01,0x85,id,0x75,8,0x95,count,0x81,2
#define TUD_HID_REPORT_DESC_KEYBOARD(id) MOCK_INPUT(id,8),0x95,1,0x91,2,0xc0
#define TUD_HID_REPORT_DESC_MOUSE(id) MOCK_INPUT(id,5),0xc0
#define TUD_HID_REPORT_DESC_CONSUMER(id) MOCK_INPUT(id,2),0xc0
#define TUD_HID_REPORT_DESC_SYSTEM_CONTROL(id) MOCK_INPUT(id,1),0xc0
