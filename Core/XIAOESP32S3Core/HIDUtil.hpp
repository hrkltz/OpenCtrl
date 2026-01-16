#pragma once

#include <USB.h>
#include <USBHID.h>
#include <USBHIDKeyboard.h>
#include <USBHIDMouse.h>
#include <USBHIDGamepad.h>

class HIDUtil {
  public:
    static void Init();
    static USBHIDKeyboard keyboard;
    static USBHIDMouse mouse;
    static USBHIDGamepad gamepad;
    // Custom for Pen and Touch
    //TODO: static USBHIDDevice penDevice;
    //TODO: static USBHIDDevice touchDevice;
};
