
/**
 * @file HIDUtil.hpp
 * @author Oliver Herklotz (oliver-hrkltz)
 * @date 17.01.2026
 */
#pragma once

#include <USB.h>
#include <USBHID.h>
#include <USBHIDKeyboard.h>
#include <USBHIDMouse.h>
#include <USBHIDGamepad.h>

/**
 * @class HIDUtil
 * @brief Utility class for managing USB HID devices (Keyboard, Mouse, Gamepad, Pen, Touch) on XIAO ESP32S3.
 * @details This class provides static interfaces to initialize and access various USB HID devices based on the ESP32S3 USB HID library which is based on TinyUSB.
 * @see https://github.com/espressif/arduino-esp32/blob/3.3.5/libraries/USB/src/USBHID.h
 * @see https://github.com/espressif/arduino-esp32/blob/3.3.5/libraries/USB/src/USBHIDGamepad.h
 * @see https://github.com/espressif/arduino-esp32/blob/3.3.5/libraries/USB/src/USBHIDKeyboard.h
 * @see https://github.com/espressif/arduino-esp32/blob/3.3.5/libraries/USB/src/USBHIDMouse.h
 */
class HIDUtil {
  public:
    /**
     * @brief Initialize all supported USB HID devices.
     *
     * This function must be called before using any HID device.
     */
    static void Init();

    /**
     * @brief Static instance for USB HID Keyboard.
     */
    static USBHIDKeyboard keyboard;

    /**
     * @brief Static instance for USB HID Mouse.
     */
    static USBHIDMouse mouse;

    /**
     * @brief Static instance for USB HID Gamepad.
     */
    static USBHIDGamepad gamepad;

    /**
     * @brief Send a gamepad report.
     * 
     * @param x X-axis value (-128 to 127) -> Left Stick X
     * @param y Y-axis value (-128 to 127) -> Left Stick Y
     * @param z Z-axis value (-128 to 127) -> Left Trigger
     * @param rz Rotation Z-axis value (-128 to 127) -> Right Trigger 
     * @param rx X-axis rotation value (-128 to 127) -> Right Stick X
     * @param ry Y-axis rotation value (-128 to 127) -> Right Stick Y
     * @param hat Hat switch value (0-8)
     * @param buttons Button state bitmask (32 bits)
     */
    static void SendGamepad(int8_t x, int8_t y, int8_t z, int8_t rz, int8_t rx, int8_t ry, uint8_t hat, uint32_t buttons);
};
