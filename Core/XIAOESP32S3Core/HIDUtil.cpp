#include "HIDUtil.hpp"

//TODO: // Define custom descriptors (simplified examples; adjust for full spec)
//TODO: static const uint8_t penDescriptor[] = {
//TODO:     // HID descriptor for digitizer/pen (basic)
//TODO:     0x05, 0x0D,  // Usage Page (Digitizers)
//TODO:     0x09, 0x02,  // Usage (Pen)
//TODO:     0xA1, 0x01,  // Collection (Application)
//TODO:     0x09, 0x32,  // Usage (In Range)
//TODO:     0x15, 0x00,  // Logical Minimum (0)
//TODO:     0x25, 0x01,  // Logical Maximum (1)
//TODO:     0x75, 0x01,  // Report Size (1)
//TODO:     0x95, 0x01,  // Report Count (1)
//TODO:     0x81, 0x02,  // Input (Data,Var,Abs)
//TODO:     // Add more fields for x, y, pressure, etc.
//TODO:     0xC0         // End Collection
//TODO: };

//TODO: static const uint8_t touchDescriptor[] = {
//TODO:     // HID descriptor for touch (basic)
//TODO:     0x05, 0x0D,  // Usage Page (Digitizers)
//TODO:     0x09, 0x04,  // Usage (Touch Screen)
//TODO:     0xA1, 0x01,  // Collection (Application)
//TODO:     0x09, 0x51,  // Usage (Contact ID)
//TODO:     0x15, 0x00,  // Logical Minimum (0)
//TODO:     0x26, 0xFF, 0x00,  // Logical Maximum (255)
//TODO:     0x75, 0x08,  // Report Size (8)
//TODO:     0x95, 0x01,  // Report Count (1)
//TODO:     0x81, 0x02,  // Input (Data,Var,Abs)
//TODO:     // Add more for x, y, etc.
//TODO:     0xC0         // End Collection
//TODO: };

USBHIDKeyboard HIDUtil::keyboard;
USBHIDMouse HIDUtil::mouse;
USBHIDGamepad HIDUtil::gamepad;
//TODO: USBHIDDevice HIDUtil::penDevice(penDescriptor, sizeof(penDescriptor));
//TODO: USBHIDDevice HIDUtil::touchDevice(touchDescriptor, sizeof(touchDescriptor));

void HIDUtil::Init() {
    USB.begin();
    keyboard.begin();
    mouse.begin();
    gamepad.begin();
    //TODO: penDevice.begin();
    //TODO: touchDevice.begin();
}
