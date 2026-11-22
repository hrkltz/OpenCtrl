# OpenCtrl / Test

This Python 3.9 CLI tool logs HID events on macOS using a CGEventTap. By default it **suppresses** events so they do not reach the OS (safer for testing). Use `--passthrough` to allow normal OS handling.

## Requirements

- macOS (CGEventTap)
- Python 3.9
- Accessibility permission for the terminal running the script

## Setup

Please run the following commands to prepare the virtual environment with all dependencies.

```zsh
user@device Test % python3 -m venv ".venv/"
user@device Test % (source ".venv/bin/activate" && python3 -m pip install --upgrade pip)
user@device Test % (source ".venv/bin/activate" && python3 -m pip install -r "requirements.txt")
```

## Usage

```zsh
# Use '--catch' to suppress events so the OS won't handle them while logging.
# Caution: with suppression enabled, normal input may be blocked and might force you kill your computer with the power switch. The keyboad button 'q' is implemented to act as an kill-switch.
./Run.sh --type keyboard
```

>Note: If the script fails to create an event tap, grant Accessibility permission:
System Settings → Privacy & Security → Accessibility → enable your terminal app. For example if you're using VS Code you need to enable this for "Visual Studio Code.app".

## Automated HID regressions

Run `bash Test/RunHID.sh` from the project root after installing the core dependencies. Requires Node.js, a C++17 compiler with AddressSanitizer/UndefinedBehaviorSanitizer, and the local ArduinoJson library cache. No board, network, Accessibility permission, or input suppression is required.

The browser tests cover discrete transitions during pending requests, axis coalescing, mouse ordering, keyboard tap releases, session headers, failed requests, queue overflow, and stale acknowledgements. The native tests exercise the actual descriptor parser, HID sender, pen validation, HTTP route validation, session ownership, failed USB releases, reconnects, and watchdog expiry including clock wraparound, selectable/custom-only profiles, shared report IDs across Input/Output/Feature types, idle reports without repeated relative movement, asynchronous USB failures/timeouts, and BIOS boot-keyboard mode transitions. Additional tests cover reusable profile remapping, repeated mouse instances, BLE-only HTTP sessions, simultaneous and mirrored USB/BLE routes, per-route state, encrypted reads/writes, subscriptions, notification failures, ATT MTU limits, suspend/resume, reconnect releases, and rejection of a second BLE host. The actual BLE backend runs against the fake GATT API. `Stubs/` provides in-memory USB/BLE/network/RTOS/display adapters; these tests cannot verify real USB/BLE enumeration, RF behavior, pairing persistence, or OS drivers.

Also compile the core and each firmware example with `./Compile.sh --project <project>`. Before deploying, verify enumeration, held keys, quick gamepad taps, mouse dragging, pen contact, host Output/Feature reports, WiFi loss, browser termination, USB reconnect, BLE pairing/bonded reconnect, changing profile caches, and simultaneous USB/BLE mouse input on a physical board and the target host OS.

The `--web-usb` compatibility case checks that WebController's default profile selection still routes exclusively to USB and that BLE stays off, then exercises the standard HTTP inputs, pen reports, releases, and USB reconnect to neutral. The script also builds and runs the actual WebController sketch in USB, Bluetooth, and mixed modes. Those cases check preset routing, mode metadata and embedded page serving, pen input/releases over each transport, USB boot-keyboard support, and the second-mouse endpoint with validation and reconnect recovery. Browser tests check primary/secondary mouse ordering and mode-dependent link visibility. Compile all three firmware presets and verify actual enumeration/pairing on a board before deployment.

Run `bash Test/RunM5Inputs.sh` for the shared M5Stack library and M5StackController. It first tests all six supported units (Key U144, Scroll U186, Encoder U135, Joystick2 U024-V2, Fader U123, and Angle U005): GPIO button bounce and timer wrap, analog millivolt calibration, 16/32-bit counter rollover, active-low/high button variants, recovery without replaying deltas, RGB register byte order and pixel bounds, GRB signal/latch encoding, and RMT cleanup/failure paths. The fake GPIO/ADC/RMT adapters cannot verify physical voltage, cable presence, LED timings, or installed unit firmware. The library's `examples/Units` sketch exercises the complete hardware API without HID.

It then runs the actual M5StackController sketch over USB and BLE against fake I2C and HID adapters under sanitizers:

- Bindings: nothing is sent before a table exists; driving and flight tables; every port and axis driving every pedal and steering control with one read per port; separate sticks; unused ports left unconnected; validation, failed writes, and reloading the saved units and table.
- Encoders and scroll units: pulse mode selected when a port gets an Encoder, the turn position as a gamepad axis, one key press per step in each direction, wheel detents per step, the position stopping at its ends, inversion of position and steps, recentering when a port's unit changes, pulse mode selected again after a lost unit returns without replaying steps, and rejection of sources the port's unit does not have.
- Mouse and keyboard: pointer speed, reversed axes, wheel detents carried across cycles, clicks, keys pressed past half travel, modifiers, no reports while idle or for unbound devices, and occasional refreshes of held buttons and keys.
- Switching: a new table releases held keys at once and starts filters and mouse movement fresh, also when applied during setup.
- Neutral reports: sensor loss and setup send each binding's rest (centered rudder, released pedals), and unchanged inputs are resent after reconnect.
- Filtering: three-reading medians, isolated spike rejection, EMA convergence, fresh state on mode changes, and output stability around 8-bit rounding boundaries (Median-3 and EMA hold steady, Off rounds directly).
- Timing and recovery: the 4 ms cycle budget with simulated I2C costs, overruns without catch-up reports, timer rollover, slow flash writes, read retries within the budget, short frames, send failures, and the published diagnostics.
- Ports: no scans while HID runs; setup scans the hub and direct bus, lists devices per port, reports clicks, rescans on request, and reports a missing hub. Skipped reads are distinguished from failed reads.

These tests do not verify wiring, actual travel, pairing, or game bindings; run both transports on hardware for that.

Run `bash Test/RunCalibration.sh` for the ControllerCalibration API and storage under sanitizers: validation, queued commands, raw extrema captured between HTTP polls, preview without flash writes, save/reload, namespace isolation, failed writes, persisted defaults, session expiry, Fader/Angle millivolt ranges, filter settings, binding tables (allowed source/target/mode combinations, options metadata, invalid tables, failed writes, changes during setup, and loading without calibration), and units per port (unknown units, sources a port's unit does not provide, units and bindings saved and loaded together). Input publication checks verify that all axes and diagnostics use one lock and that concurrent HTTP reads see complete cycles.

Run `node --test Test/m5_controller_ui.test.js` with Playwright and its Chromium browser installed for the M5StackController settings page. Set `NODE_PATH` if Playwright is provided outside the project; `OPENCTRL_CHROMIUM` can select an existing Chromium executable, and `OPENCTRL_UI_SCREENSHOT` can save a screenshot. The test serves the real page files, including the vendored Siegel modules, to a simulated controller. It covers the hub's port tiles (units, live readings, input counts, scan results, hub status), selecting a port, choosing units and the setup areas and elements they show, dropping inputs of elements a new unit lacks, mode choices that fit each element and target, saving units and inputs together and failed saves, detected units, scans and rescans, live readings, joystick and encoder setup validation, filters, diagnostics, and layout without horizontal overflow at 320, 390, and 1280 pixels, with the unit chooser switching to a vertical list where its labels no longer fit. It also fails on any browser error. It does not connect to physical hardware.
