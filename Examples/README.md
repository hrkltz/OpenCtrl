# OpenCtrl / Examples

Concrete examples live here. Keep examples focused on a specific input surface or interaction pattern, and put reusable platform code in `../Source`. Firmware examples use their core as library instead of copying its files.

## Examples

- `WebController`: phone friendly web views for all HID inputs, with USB, Bluetooth LE, and mixed profile configurations, including two independent mice.
- `Steering001Controller`: steering wheel prototype built on the XIAO ESP32S3 core plus steering-specific sensor code.
- `FlyController`: flight axes and buttons from two M5Stack Joystick2 units, over USB or BLE.
- `DriveController`: steering, accelerator/brake, and shift buttons from two M5Stack Joystick2 units, over USB or BLE.
