#pragma once
#include "tusb.h"
#define USB_INTERFACE_HID 2
#define ESP_OK 0
#define TUD_HID_INOUT_DESC_LEN 32
#define CFG_TUD_ENDPOINT_SIZE 64
#define HID_ITF_PROTOCOL_NONE 0
#define HID_ITF_PROTOCOL_KEYBOARD 1
#define TUD_HID_INOUT_DESCRIPTOR(...) 0
inline int tinyusb_enable_interface2(int,uint16_t,uint16_t (*)(uint8_t*,uint8_t*),bool) { return ESP_OK; }
inline uint8_t tinyusb_get_free_in_endpoint() { return 1; }
inline uint8_t tinyusb_get_free_out_endpoint() { return 1; }
inline uint8_t tinyusb_add_string_descriptor(const char*) { return 1; }
