/**
 * @file HIDUtil.cpp
 * @author Oliver Herklotz (oliver-hrkltz)
 * @date 17.01.2026
 */
#include "HIDUtil.hpp"

USBHIDKeyboard HIDUtil::keyboard;
USBHIDMouse HIDUtil::mouse;
USBHIDGamepad HIDUtil::gamepad;

void HIDUtil::Init() {
    USB.begin();
    keyboard.begin();
    mouse.begin();
    gamepad.begin();
}

void HIDUtil::SendGamepad(int8_t x, int8_t y, int8_t z, int8_t rz, int8_t rx, int8_t ry, uint8_t hat, uint32_t buttons) {
    gamepad.send(x, y, z, rz, rx, ry, hat, buttons);
}
