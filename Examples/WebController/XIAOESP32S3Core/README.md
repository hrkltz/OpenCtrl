# OpenCtrl / Examples / WebController / XIAOESP32S3Core

Firmware which serves the [web views](../www) from its own flash and selects USB, Bluetooth, or mixed HID routing through [Config.hpp](Config.hpp). [Profiles.hpp](Profiles.hpp) composes the shared core profiles. See the [mode table and mouse routing](../README.md#usb-bluetooth-and-mixed-modes).

On top of the keyboard, mouse, and gamepad of the core it adds:

- Consumer control (media keys; send 0 to release) at `/consumer`: `{"usage": int}`, e.g. `0xCD` for play/pause.
- System control (send 0 to release) at `/system`: `{"usage": int}`, `1` power off, `2` standby, `3` wake host.
- Pen ([HIDPen](./HIDPen.hpp)) at `/pen`: `{"tip": bool, "inRange": bool, "barrel": bool, "eraser": bool, "x": 0-32767, "y": 0-32767, "pressure": 0-1023}`.

>Note: The pen is a HID digitizer, which Windows, Linux, ChromeOS, and Android support. macOS has no generic HID digitizer support and ignores it.

## Part List

- 1x [XIAO ESP32S3](https://www.seeedstudio.com/XIAO-ESP32S3-p-5627.html)
- 1x USB-C to USB-C or USB-A cable
- 1x [Seeed Studio XIAO Expansion Board](https://www.seeedstudio.com/Seeeduino-XIAO-Expansion-board-p-4746.html) (optional)

## Setup

This version is based on the general [XIAOESP32S3Core](../../../Source/XIAOESP32S3Core) and needs no additional libraries. Set up the core first.

## Usage

Run `./BuildAndDeploy.sh --project Examples/WebController` from the root folder to compile and upload. The build embeds all files in [www/](../www) as `WebFiles.hpp`, generated outside the project, so changed pages are included with every build. Then open the IP shown on the display in a browser.

All POST endpoints use the core’s pairing and session protocol. See the [core API](../../../Source/XIAOESP32S3Core/README.md#usage). The browser supplies heartbeat requests automatically. Pen coordinates and pressure are validated before conversion; the Bluetooth preset registers the pen over BLE along with all standard profiles. Its registered neutral report releases tip, barrel, eraser, and proximity on disconnect. Mixed mode keeps the pen on USB and adds `/mouse/secondary` for the BLE mouse. Public GET `/controller` lets the pages display the mode and available mouse link.
