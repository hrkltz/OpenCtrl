# OpenCtrl / Source / XIAOESP32S3Core

A USB and Bluetooth LE HID controller hub for XIAO ESP32S3, providing keyboard, mouse, and gamepad inputs via web API. Displays the IP address on an OLED screen.

This folder is both the general purpose firmware ([XIAOESP32S3Core.ino](./XIAOESP32S3Core.ino)) and an Arduino library which examples build on:

- `HIDProfile`: reusable descriptors, neutral states, and callbacks, independent of transport.
- `HIDUtil`: registers profile instances, routes reports, and manages their held state and recovery.
- `HIDUSBTransport` / `HIDBLETransport`: USB TinyUSB and Bluetooth LE HID backends composed with the same profiles.
- `HTTPServerUtil`: web API forwarding requests to `HIDUtil`.
- `WiFiUtil`: WiFi connection using the credentials passed by the build scripts.
- `DisplayUtil`: OLED output (line 0: connection status, line 1: IP).

## Part List

- 1x [XIAO ESP32S3](https://www.seeedstudio.com/XIAO-ESP32S3-p-5627.html)
- 1x USB-C to USB-C or USB-A cable
- 1x [Seeed Studio XIAO Expansion Board](https://www.seeedstudio.com/Seeeduino-XIAO-Expansion-board-p-4746.html) (optional)

## Setup

```bash
# Get the board package for the XIAO ESP32S3.
arduino-cli --config-file arduino-cli.yaml core update-index
arduino-cli --config-file arduino-cli.yaml core install esp32:esp32@3.3.12

# Choose the right FQBN for your specific board. You may need to update the fqbn in Compile.sh in the root folder.
arduino-cli --config-file arduino-cli.yaml board listall xiao
Board Name        FQBN
XIAO_ESP32C3      esp32:esp32:XIAO_ESP32C3
XIAO_ESP32C5      esp32:esp32:XIAO_ESP32C5
XIAO_ESP32C6      esp32:esp32:XIAO_ESP32C6
XIAO_ESP32S3      esp32:esp32:XIAO_ESP32S3
XIAO_ESP32S3_PLUS esp32:esp32:XIAO_ESP32S3_Plus

# Install libraries.
## HTTP server
arduino-cli --config-file arduino-cli.yaml lib install "Async TCP@3.5.0" "ArduinoJson@7.4.3" "ESP Async WebServer@3.12.1"
## Bluetooth LE (also required to compile the shared core)
arduino-cli --config-file arduino-cli.yaml lib install NimBLE-Arduino@2.5.1
## Display
arduino-cli --config-file arduino-cli.yaml lib install U8g2@2.36.19
```

>Note: You need to set your WiFi SSID and password! The [build scripts](../../README.md#build) read them from the `HRKLTZ_OPENCTRL_WIFI_SSID` and `HRKLTZ_OPENCTRL_WIFI_PASSWORD` environment variables, which avoids leaking your network secrets when sharing code.

## Usage

Build and upload from the root with `./BuildAndDeploy.sh --project Source/XIAOESP32S3Core`.
The device displays its IP address and a new numeric pairing code from 0 to 99 on the last OLED line. The code is also printed when a USB serial console connects. Enter it in the web page's pairing bar. Pair again after restarting the board. The code is not exposed over HTTP.

The short code is easy to guess and is only suitable for a trusted local network. Do not expose the controller to an untrusted network; the HTTP API is unencrypted.

All POST routes require these headers. The `Authorization` Bearer value is the displayed 0–99 pairing code:

- `Authorization: Bearer <pairing token>`
- `X-OpenCtrl-Session: <32 hexadecimal characters identifying this controller session>`
- `Content-Type: application/json`

The first paired controller owns the session. A different session receives `409` until the owner releases or its lease expires. Send `/heartbeat` with `{}` every 500 ms while controlling the device. After 2 seconds without a valid request, WiFi loss, or loss of all HID connections, the firmware releases all profiles. Each transport also resets its own state on disconnect and retries neutral reports when it recovers. Losing one connection does not stop the other. Send `/release` with `{}` to release and relinquish the session immediately. This safety task runs independently of the sketch's loop.

Pairing grants keyboard and power-control access to the attached host. Use this plain HTTP development firmware on a trusted network; the token authenticates requests but does not encrypt them.

The standard profiles expose:

- `/mouse`: `{"dx": int, "dy": int, "wheel": int, "pan": int, "buttons": int}`. X/Y accept -4096 to 4096 and are split into valid signed-byte reports. Wheel/pan accept -127 to 127; buttons are a 5-bit held mask (0–31). Relative deltas are never automatically replayed after a partial transmission failure.
- `/keyboard`: `{"modifiers": int, "keys": [int, ...]}`. Modifiers are 0–255; up to six keyboard usages 0–101 are accepted by the standard descriptor. Reports remain held until the next report. Send `{"keys":[],"modifiers":0}` to release; a tap consists of a press followed by this release.
- `/gamepad`: `{"x": int, "y": int, "z": int, "rz": int, "rx": int, "ry": int, "hat": int, "buttons": int}`. Axes accept -127 to 127; hat is 0 (neutral) or 1–8 (up, clockwise); buttons is an unsigned 32-bit held mask. Missing stick axes default to 0; missing triggers z/rz default to -127.
- `/consumer`: `{"usage": int}` (0–1023), when enabled. Send 0 to release.
- `/system`: `{"usage": int}` (0 release, 1 power off, 2 sleep, 3 wake), when enabled.
- `/hid/report`: `{"id": int,"data": [byte, ...]}` sends any registered Input report. The payload must match the descriptor's exact length and excludes the report ID byte. Optional `"transport":"usb"` or `"transport":"bluetooth"` restricts the send to that configured transport; omission sends through the instance's configured transports.

Missing fields use their documented defaults. Wrong JSON types, ranges, oversized key arrays, and non-object bodies return `400`. Missing/wrong pairing codes return `401`. Unavailable raw profiles return `404`; an unavailable HID transport or failed transmission returns `503`. On a transmission failure, reconnect and start from neutral rather than replaying old commands.

The core firmware enables keyboard, mouse, and gamepad over USB by default. The radio starts only when a Bluetooth profile is registered. Pen is provided by the WebController example; touch can be added as a custom profile.

## Building on the Core

Examples keep their specific code and use this folder as a library. Put the sketch in a folder named `XIAOESP32S3Core/` inside the example and include core headers with angle brackets.

`HIDUtil::Init()` enables the three standard profiles. Use `HIDUtil::Init(HIDUtil::All)` to include consumer and system controls, a bitwise combination of `HIDUtil::Device` flags to select profiles, or `HIDUtil::Init(0)` for custom profiles only. Check its boolean return before starting the server. A third argument selects the standard profiles' transport, for example `HIDUtil::Init(HIDUtil::Standard, {}, HIDTransport::Bluetooth)` or `HIDTransport::Both`. An optional second `HIDProfileCallbacks` argument handles standard-profile events such as keyboard LED Output reports (ID 1).

Register custom profiles in `setup()` **before** `HIDUtil::Init()`. The build scripts select USB-OTG with CDC-on-boot disabled so descriptors are registered before USB enumeration. The core still provides a USB CDC console for the pairing code. Changing profiles requires a restart and USB re-enumeration; Bluetooth hosts may require removing their cached pairing and pairing again. When the standard keyboard is enabled, the interface also supports BIOS boot-keyboard mode. Non-keyboard reports pause while the host requests boot protocol and synchronize back to neutral when report protocol resumes.

`RegisterProfile(descriptor, length, neutralReports, callbacks)` copies the descriptor and validates its report sizes. Each Input report must have exactly one neutral payload, for example `{{reportId, {0, 0}}}` for a two-byte report. Use safe device-specific neutral values, including released contact switches and neutral axes. The firmware uses these values on timeout and transport recovery. An optional final `HIDTransport` argument routes this custom profile (USB by default).

- The core owns the TinyUSB HID interface and callbacks. Do not mix it with Arduino `USBHID*` instances; register their functionality as profiles instead.
- Every report uses an explicit nonzero ID. IDs 1–5 belong to the standard helpers; `NextReportId()` starts at 7. IDs must be unique across registered instances, including instances on different transports. Duplicate IDs and late registration are rejected.
- Each payload is limited to 63 bytes (plus the ID on USB). Combined descriptors are limited to 4096 bytes per USB interface and 512 bytes for the BLE Report Map. BLE notifications also require an effective ATT MTU of at least payload length + 3; larger reports remain unavailable until the host negotiates it. Malformed layouts, truncated items, and missing neutral states are rejected. This validates layout and size, not the host OS's interpretation of usages.
- Supply `HIDProfileCallbacks::output`, `setFeature`, and `getFeature` to handle host-to-device Output and Feature reports. Input, Output, and Feature reports may share an ID within a profile; Both transports dispatch their report types separately. IDs must remain unique across profiles.
- Callbacks run on the USB or BLE task and may run concurrently for a mirrored profile. Keep them short, synchronize shared data, and do not send reports or block from a callback. A Feature getter writes at most the supplied length and returns the number of bytes written.
- `SendInput(id, data, length)` sends through the instance's configured transports. Higher-level `SendKeyboard`, `SendMouse`, `SendGamepad`, `SendConsumer`, and `SendSystem` accept an optional final report ID, so repeated device types can be addressed independently. USB success means transfer completion; BLE success means NimBLE queued a notification, not that the host application consumed it. `Service()` retries pending neutral reports; the HTTP safety task calls it automatically. A standalone sketch without the server must call it regularly.

USB idle reports retain held state while clearing already-consumed relative movement, so a host idle request cannot repeat mouse motion.

The WebController pen composes a custom profile with a selected transport (`begin(HIDTransport::Bluetooth)` is supported). It is an example with a dynamically assigned report ID. Multiple instances can register distinct IDs; host compatibility depends on the chosen descriptors and host drivers. The Xbox/PS5 web views use the standard generic gamepad profile; device-specific protocols, authentication, and force-feedback behavior need their own implementation.

## Composing Profiles and Transports

Keep a single core for USB and Bluetooth. `HIDProfile` is a reusable value containing its descriptor, neutral reports, and callbacks. `HIDProfiles` supplies keyboard, mouse, gamepad, consumer, and system definitions. Registration copies the value and creates independent delivered state for every instance and transport. No device class needs to inherit from a USB or BLE implementation.

```cpp
const auto mouse = HIDProfiles::Mouse();
// The factory uses local report ID 1; registration remaps it and its neutral state.
if (!HIDUtil::RegisterProfile(mouse, HIDTransport::USB, 2) ||
    !HIDUtil::RegisterProfile(mouse, HIDTransport::Bluetooth, 7) ||
    !HIDUtil::Init(0)) return;

HIDUtil::SendMouse(10, 0, 0, 0, 0, 2); // USB mouse
HIDUtil::SendMouse(0, 10, 0, 0, 0, 7); // BLE mouse
```

`RegisterProfile(profile, transport, firstReportId)` remaps all local IDs to a consecutive block, including neutral states. Callbacks receive the registered IDs. Omit `firstReportId` to retain the definition's IDs. `profile.RemapReportIds(firstId)` is also available for preparing a value without registering it. The original profile remains reusable. [WebController](../../Examples/WebController) offers USB, Bluetooth-only, and mixed presets. Mixed mode adds a USB keyboard and BLE gamepad alongside independent USB/BLE mice; its pen uses ID 7 and second mouse ID 8.

`HIDTransport::Both` mirrors one instance through both connections. `SendInputTo(id, data, length, transport)` selects a configured subset. Mirroring is explicit: two different instances do not copy each other's inputs. With both connections on the same host, mirrored relative movement may be counted twice.

Sends skip disconnected or inactive routes and succeed when at least one selected route accepts the report and no active selected route fails. A partial failure can therefore return false after another route already received the input; do not blindly retry relative movement. Each route keeps independent held state and pending neutral reports. `Connected(transport)` and `Ready(transport)` accept USB, Bluetooth, or Both (the default means any). `ReadyForReport(id)` checks a specific instance; `Transports(id)` returns its configured routes.

### Bluetooth behavior

The BLE backend uses NimBLE-Arduino 2.5.1 with ESP32 Arduino 3.3.12. The bundled Arduino BLE wrapper drops repeated characteristic UUIDs on NimBLE, which breaks composite HID reports ([upstream issue](https://github.com/espressif/arduino-esp32/issues/12951)); NimBLE-Arduino supports these correctly. No device-specific BLE keyboard/mouse wrapper is needed. It exposes HID Report Map, typed Report characteristics with Report Reference descriptors, HID Information, Control Point, Device Information/PnP, and Battery services. The battery level is fixed at 100 for this externally powered firmware; it is not a battery measurement. Battery-powered hardware needs a real gauge integration before relying on this value.

Pair **OpenCtrl** in the target host's Bluetooth settings. Reports require an encrypted connection and Input subscriptions. Bluetooth pairing uses bonded Secure Connections with Just Works (no MITM protection), separately from the HTTP pairing code. A failed authentication disconnects the peer. The backend supports one connected BLE host, one advertised peripheral identity, and multiple HID collections/report IDs within that peripheral. It supports report protocol, not Bluetooth Classic or BLE boot-keyboard reports. USB retains its existing BIOS boot-keyboard support when the standard keyboard is selected.

USB and BLE can run simultaneously against the same host or different hosts. You can also register several mice on one transport. The host decides how to expose their collections: desktop systems usually merge mouse movement into one cursor, while applications using device-specific input can distinguish devices. Two HID mice do not guarantee two cursors or independently advertised Bluetooth devices.

To add authenticated endpoints, call `HTTPServerUtil::AddJsonRoute(uri, handler)` before `HTTPServerUtil::Init()`. The handler accepts a `JsonObject` and returns an HTTP status. It shares the pairing, session, watchdog, CORS, and error handling of core routes. Use `ReadInt`, `ReadBool`, and `ReadButtons` to validate JSON values before narrowing them. `GetServer()` remains available for static GET routes, which are public; do not register unprotected input routes directly.

`HTTPServerUtil::AddMouseRoute(uri, reportId)` binds another registered mouse instance to the standard mouse JSON schema, using the same validation and authenticated session handling as `/mouse`. WebController uses it for `/mouse/secondary` in mixed mode.
