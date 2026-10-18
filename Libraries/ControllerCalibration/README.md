# OpenCtrl / Libraries / ControllerCalibration

Shared calibration UI and persistent settings for local controllers. FlyController and DriveController compose this library with `M5StackInputs` and the HID core. The hardware library stays independent of networking and storage; the HID core requires no calibration-specific changes.

The calibration service includes only the hardware library's value utilities (`InputUtils.hpp`), without depending on its GPIO, I2C, or unit classes. Bind Joystick2 `x`/`y` directly, or `&fader.Calibration()` / `&angle.Calibration()` with millivolt bounds, and supply the corresponding raw values from the sensor loop. The page renders the supplied bindings automatically. Relative encoder deltas first need a controller-owned bounded position; calibrating them as absolute potentiometer values would lose that distinction.

## Browser Workflow

`Begin()` starts a WPA2 WiFi access point with a random 16-character password, a per-boot SSID suffix, and at most one WiFi client. The example displays the SSID, password, and IP on its OLED. Connect to that network and open the shown address (normally `http://192.168.4.1/`). No home WiFi credentials or internet are needed. This setup network is separate from the WebController HTTP pairing/session protocol.

The page provides live raw and normalized readings for every bound axis, full-travel capture, center capture, manual limits, deadzone, inversion, Preview, Save, Reset defaults, and Done. Extrema are recorded by the sensor loop, so a brief endpoint visit between browser polls is retained. Preview changes RAM only. Save/Reset write one versioned JSON record in Preferences/NVS and report write failures. The examples use separate `fly-cal` and `drive-cal` namespaces.

Starting calibration pauses HID through the example's input loop. Done discards changes since the last successful save and resumes local input. A session with no state requests expires after 30 seconds and behaves like Done. Saved settings load before startup center calibration; missing, incompatible, or invalid records fall back to startup defaults/calibration. A missing sensor does not prevent the page from starting.

## Composition

1. Configure stable axis IDs, labels, pointers to `AxisCalibration`, and valid raw bounds. Configuration snapshots the factory defaults.
2. Call `Load()` before enabling the web service. It applies a complete valid record, or leaves the existing settings unchanged.
3. Call `Begin(networkPrefix, title)` and show its `SSID()`, `Password()`, and `Address()` to the user.
4. In the sensor loop, call `UpdateRaw(index, value, online)` and `Service()`. Send neutral HID while `Active()` is true; otherwise map physical input normally.

HTTP callbacks only take snapshots and queue commands. Only `Service()` applies settings and writes flash; sensor reads and OLED updates remain in the main loop. Call `Load()` only before `Begin()`. Axis bindings must remain alive for the service's lifetime, and the input loop owns access to their settings.

All changes are validated before queueing: schema version, complete axis list/IDs, numeric types, raw bounds, ordered minimum/center/maximum, finite deadzones from 0 inclusive to 1 exclusive, and boolean inversion. One command can be outstanding. Success is reported only after the input loop completes it, including persistence when requested. A failed save does not apply its submitted settings.

## API

GET `/` serves the embedded page; GET `/api/calibration` returns settings, live readings, capture state, storage status, and the last completed operation. Mutation routes are POST `/api/calibration/start`, `/capture`, `/stop-capture`, `/preview`, `/save`, `/reset`, and `/end` under the same `/api/calibration` prefix. Preview and Save accept `{version:1,axes:[{id,minimum,center,maximum,deadzone,inverted},...]}`; other commands need no payload.

Every POST requires `X-OpenCtrl-Calibration: 1`. No CORS permissions are granted, so a third-party browser page cannot supply this header without a denied preflight. The private setup network's password controls network access. The API is intended for that isolated setup network, rather than an exposed public server.

Accepted commands return 202 with an operation ID. Poll the state until `completedOperation` matches; `operationStatus` and `message` identify the outcome. Malformed settings return 400, missing mutation headers 403, and inactive/conflicting commands 409. Storage failures complete with status 503. Only Start can run outside an active calibration session.

`src/CalibrationPage.hpp` is the single source for the embedded HTML/CSS/JavaScript. No external assets or CDN are needed. The firmware examples compile with the bundled ESP32 WiFi/Preferences libraries and the core's existing ArduinoJson/ESP Async WebServer dependencies.
