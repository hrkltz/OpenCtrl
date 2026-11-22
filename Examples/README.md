# OpenCtrl / Examples

Concrete examples live here. Each example is one firmware that covers a whole class of input, configured at runtime rather than by flashing variants. Put reusable platform code in `../Source` and hardware libraries in `../Libraries`. Firmware examples use their core as library instead of copying its files.

## Examples

- `WebController`: phone friendly web views for all HID inputs, with USB, Bluetooth LE, and mixed profile configurations, including two independent mice.
- `M5StackController`: M5Stack Joystick2, Encoder, and Scroll units on any PaHub port. The settings page shows the hub: select a port, choose the unit connected to it, set it up, and assign gamepad, mouse, or keyboard inputs to its elements. Includes a port scanner, over USB or BLE.
