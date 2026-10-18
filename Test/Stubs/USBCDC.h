#pragma once
#include "Arduino.h"
struct USBCDC : MockConsole {
    explicit USBCDC(int) {}
    void begin(int) {}
    explicit operator bool() const { return false; }
};
