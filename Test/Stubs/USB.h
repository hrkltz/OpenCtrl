#pragma once
#include "Arduino.h"
#define ARDUINO_USB_ON_BOOT 0
namespace USBMock {
    inline bool mounted = false;
    inline bool ready = true;
    inline unsigned failures = 0;
    inline unsigned completionFailures = 0;
    inline unsigned timeouts = 0;
    struct Packet { uint8_t id; std::vector<uint8_t> data; };
    inline std::vector<Packet> sent;
    inline std::vector<Packet> attempts;
}
struct MockUSB { bool begin() { USBMock::mounted = true; return true; } };
inline MockUSB USB;
