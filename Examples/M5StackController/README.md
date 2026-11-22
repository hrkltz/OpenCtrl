# OpenCtrl / Examples / M5StackController

A local controller built from M5Stack units on a PaHub. The settings page shows the hub with its six ports. Select a port, then:

1. **Connected unit:** what is plugged in: nothing, a Joystick2, an Encoder, or a Scroll unit.
2. **Setup:** the matching setup area with live readings: joystick axis ranges, or an encoder's turn range.
3. **HID inputs:** for each element of the unit (X, Y, Turn, Click), any number of HID inputs: a gamepad axis or button, the mouse, or a keyboard key.

One firmware therefore covers a steering wheel, a flight stick, a mouse, WASD keys, or knobs, without reflashing.

USB is the default; Bluetooth LE or both connections can be selected in [Config.hpp](XIAOESP32S3Core/Config.hpp) by changing `OPENCTRL_CONTROLLER_TRANSPORT` to 2 or 3. The firmware registers the core's keyboard, mouse, and gamepad profiles (report IDs 1–3), so changing units or inputs never changes the HID descriptor. It does not implement a proprietary racing-wheel protocol or force feedback.

## Hardware

- XIAO ESP32S3, USB power/data cable, and expansion board with OLED.
- M5Stack PaHub v2.1 (U040-B-V21), address `0x70`, connected to the board's SDA/SCL. Change `hubAddress` in `Config.hpp` for another address.
- On ports 0–5, any of: Joystick2 (U024-V2, `0x63`), Encoder (U135, `0x40`), Scroll (U186, `0x40`). Key, Fader, and Angle need direct GPIO pins and are not offered.
- Power and wire the units according to their M5Stack pinouts; I2C logic at the ESP must remain compatible with 3.3 V.

The shared [M5StackInputs library](../../Libraries/M5StackInputs) owns hardware access; the [ControllerCalibration library](../../Libraries/ControllerCalibration) owns the settings API and storage; the HID core owns descriptors and transports.

## Units and HID Inputs

| Unit | Elements | Setup |
| --- | --- | --- |
| Joystick2 | X, Y (axes), Click | Minimum, center, maximum, deadzone, and inversion per axis |
| Encoder, Scroll | Turn, Click | How many steps the turn position may move below and above where it starts, deadzone, and direction |

An HID input is a target and a mode. The targets are gamepad X, Y, Z, Rz, Rx, Ry and buttons 1–16; mouse X, Y, wheel, left, right, middle; and keys A–Z, 0–9, Enter, Escape, Backspace, Tab, Space, the arrows, Ctrl, Shift, and Alt. Which modes fit depends on the element and target:

| Element → target | Modes |
| --- | --- |
| Axis → gamepad axis | **Full range** (normal or reversed), or **half → full range**: one half of the axis mapped to the full range, resting at -127 (a pedal or throttle) |
| Axis → mouse X/Y/wheel | **By deflection**, normal or reversed: full deflection moves `mouseSpeed` counts (8) or `wheelSpeed` detents (0.1) per cycle |
| Axis → button or key | **Past half toward +/−**: pressed beyond half travel, so a stick can press WASD |
| Turn → gamepad axis | The turn position, in the same four modes as an axis |
| Turn → mouse X/Y/wheel | **Per step**, normal or reversed: `stepMouseCounts` (4) or `stepWheelDetents` (1) per step |
| Turn → button or key | **Press per step +/−**: one short press per step in that direction (two cycles held, two released; at most 8 queued) |
| Click → button or key | **Pressed** |

The turn position starts at 0 when the controller starts or a port gets a new unit, and stays between its ends, so turning back responds at once. For a throttle that rests at the bottom, set its minimum to -1 and use the positive half. Several inputs may use one element; the port is still read once per cycle. When several inputs drive the same gamepad axis, the one furthest from its rest wins. Up to 16 inputs are supported (`maxBindings`). A new controller has no units or inputs and sends nothing until they are saved.

For example, a driving setup uses joysticks on ports 0 and 1: port 0 Y → gamepad X (steering); port 1 Y negative half → Rz (accelerator) and positive half → Z (brake); the clicks → buttons 7 and 8 (shifting). Button numbers shown in a game may differ, so check its binding screen.

## Speed and Switching

The loop targets 250 Hz (one cycle every 4 ms) and resends absolute gamepad state every cycle, including unchanged held controls after reconnect. Only ports with HID inputs are read. Mapping the readings costs microseconds, while each unit read takes about 0.3 ms at 400 kHz, so the mapping adds no measurable latency.

Only devices with inputs are sent. Gamepad reports go out every cycle; mouse reports only while moving or when a button changes; keyboard reports when keys change. Held mouse buttons and keys are refreshed every 50 cycles (200 ms), which restores them after a reconnect without sending a report per cycle.

Saved units and inputs take effect between two cycles: the controller releases every HID input, clears filter history, fractional mouse movement, and queued step presses, then starts the new table from fresh readings. Nothing from the old table stays held.

Input reads, settings servicing, and HID output count toward the 4 ms period; only the remaining time is spent waiting. Late cycles continue without catch-up reports. A failed read gets one complete fresh retry if the cycle still has time remaining. If the retry fails or the budget is exhausted, the controller sends its neutral report and resets filter history; recovery resumes with fresh values. An encoder is switched to pulse mode again after every failed read, since it forgets that mode when it powers up; steps turned while it was unreachable are not replayed. The neutral report follows the inputs: full-range axes center at 0, half-range axes rest at -127, buttons and keys release. A centered rudder therefore stays centered on sensor loss. Unbound gamepad axes keep the core's profile neutral.

[Config.hpp](XIAOESP32S3Core/Config.hpp) sets the cycle period (`pollMicroseconds = 4000`) and I2C clock (`i2cFrequency = 400000`). At 400 kHz, two read units take about 0.675 ms of bus clock time before transaction overhead. If your wiring requires 100 kHz, lower `i2cFrequency`; two units then take about 2.7 ms, leaving less margin for 250 Hz. USB is configured for a 1 ms polling interval. BLE delivery follows the host-negotiated connection timing.

## Build and Run

Install the [core dependencies](../../Source/XIAOESP32S3Core/README.md#setup) and configure `HRKLTZ_OPENCTRL_WIFI_SSID` and `HRKLTZ_OPENCTRL_WIFI_PASSWORD`, as for WebController. The build scripts pass these credentials to the firmware and embed [www/](www) into it, gzipped. Then run from the root:

```bash
./Compile.sh --project Examples/M5StackController
./BuildAndDeploy.sh --project Examples/M5StackController
```

For BLE, set the transport to 2, rebuild, and pair **OpenCtrl** in the host's Bluetooth settings. USB mode needs no Bluetooth pairing. Value 3 mirrors the reports to both connections. Remove old cached BLE pairings if the host retains an earlier descriptor.

WiFi joins the configured network in the background with automatic reconnection. Local HID continues when WiFi is unavailable and does not require browser heartbeats.

## Settings Page

After flashing, connect your phone or computer to the same WiFi as the controller and open **http://<controller IP>/**, using the address shown on the OLED. Settings open directly without a pairing code. Anyone who can reach the controller can change them, so use a trusted local network.

The page is built with [OpenHanse Siegel](https://github.com/OpenHanse/Siegel); a subset is vendored in [www/siegel/](www/siegel). It follows the system's light or dark theme and sizes its controls for mouse or touch.

The **PaHub** panel draws ports 0–2 above the hub and 3–5 below it. Each port tile shows its unit, live readings (joystick position or encoder position, and the click), and how many HID inputs it drives; an empty port shows what the scan found there. The hub itself shows whether it was found. Selecting a tile opens that port below.

1. **Start setup** pauses HID at neutral, reads every port that has a unit, and scans the PaHub. The selected port then lists the I2C devices found on it with guessed names, and offers **Use detected …** when the found unit differs from the chosen one. The scan repeats every 3 seconds and on **Rescan ports**; it never runs while HID is active.
2. Select a port and choose its connected unit. The port shows the setup area and the elements to assign.
3. Set up the units. Joysticks: **Capture range**, move the sticks through full travel, **Finish capture**, release them, **Set centers**. Encoders: set their minimum and maximum steps from the start. **Preview** applies the values without saving; **Save setup** keeps them.
4. Assign HID inputs to the elements with **+ Assign HID input**, then **Apply & save ports and inputs**. This also works during setup.
5. **Done / resume controls**.

Closing the page during setup expires it after 30 seconds, discards unsaved previews, and resumes fresh local input. Saved units, inputs, filter, and setup load at power-on; without saved setup, keep the sticks centered for startup calibration. Failed flash writes are reported and keep the previous settings. Results of each action appear as notifications.

**Input filtering** chooses Off, Median-3 (default), or EMA with an adjustable response (0.05–1) for joystick axes. Median-3 rejects isolated spikes and delays a sustained step by one cycle; EMA's lower values smooth more. Both also stabilize the 8-bit gamepad axes with a quarter-count hysteresis margin, so small fluctuations near a rounding boundary do not alternate between adjacent values. Off bypasses that as well. Turn positions, buttons, keys, and the setup readings are unfiltered.

**Input connection health** shows sensor read errors, reads recovered on retry, input-loss cycles (neutral reports caused by a missing unit), and HID send failures, plus input loop timing: the target cycle, last and maximum processing time, cycles over target, and the measured count.

To try the page without flashing, serve [www/](www) with a server that answers the API below.

## API

Besides the [ControllerCalibration API](../../Libraries/ControllerCalibration/README.md#api), the firmware serves:

- GET `/`, `/app.js`, and `/siegel/…`: the page, gzipped.
- GET `/api/ports`: `{scanned, hub, hubAddress, direct:[address], ports:[{devices:[address], pressed}]}`. `pressed` is the click of a port read this cycle, otherwise `null`.
- POST `/api/ports/scan` with `X-OpenCtrl-Calibration: 1`: rescan on the next setup cycle.

Units and inputs are saved together through POST `/api/calibration/mapping` as `{version:1,ports:["joystick2","encoder","none",…],bindings:[{source:"port0.y",target:"gamepad.x",mode:"normal"},…]}`. Sources are `portN.x`, `portN.y`, `portN.turn`, and `portN.button`; each must exist on its port's unit. GET `/api/calibration/options` lists units, sources, and targets. Setup axes are `portN.x`, `portN.y`, and `portN.turn` for every port, so a port keeps its setup when its unit changes.

## Extending

[Mapping.hpp](XIAOESP32S3Core/Mapping.hpp) owns the units, sources, and targets and converts readings into HID state; the sketch reads each port according to its unit. A new target needs one table entry. A new I2C unit (e.g. a TMAG5273 angle sensor on a Qwiic-to-Grove cable) needs a unit entry, a reader in `ReadUnit()`, and its elements on the page.
