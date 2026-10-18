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

The browser tests cover discrete transitions during pending requests, axis coalescing, mouse ordering, keyboard tap releases, authentication headers, failed requests, queue overflow, and stale acknowledgements. The native tests exercise the actual descriptor parser, HID sender, pen validation, HTTP route validation, pairing/session ownership, failed USB releases, reconnects, and watchdog expiry including clock wraparound, selectable/custom-only profiles, shared report IDs across Input/Output/Feature types, idle reports without repeated relative movement, asynchronous USB failures/timeouts, and BIOS boot-keyboard mode transitions. Additional tests cover reusable profile remapping, repeated mouse instances, BLE-only HTTP sessions, simultaneous and mirrored USB/BLE routes, per-route state, encrypted reads/writes, subscriptions, notification failures, ATT MTU limits, suspend/resume, reconnect releases, and rejection of a second BLE host. The actual BLE backend runs against the fake GATT API. `Stubs/` provides in-memory USB/BLE/network/RTOS/display adapters; these tests cannot verify real USB/BLE enumeration, RF behavior, pairing persistence, or OS drivers.

Also compile the core and each firmware example with `./Compile.sh --project <project>`. Before deploying, verify enumeration, held keys, quick gamepad taps, mouse dragging, pen contact, host Output/Feature reports, WiFi loss, browser termination, USB reconnect, BLE pairing/bonded reconnect, changing profile caches, and simultaneous USB/BLE mouse input on a physical board and the target host OS.

The `--web-usb` compatibility case checks that WebController's default profile selection still routes exclusively to USB and that BLE stays off, then exercises the standard HTTP inputs, pen reports, releases, and USB reconnect to neutral. The script also builds and runs the actual WebController sketch in USB, Bluetooth, and mixed modes. Those cases check preset routing, mode metadata and embedded page serving, pen input/releases over each transport, USB boot-keyboard support, and the authenticated second-mouse endpoint with validation and reconnect recovery. Browser tests check primary/secondary mouse ordering and mode-dependent link visibility. Compile all three firmware presets and verify actual enumeration/pairing on a board before deployment.

Run `bash Test/RunM5Inputs.sh` for the shared M5Stack library and FlyController/DriveController. It runs the actual sketches against fake I2C and USB/BLE adapters under sanitizers, checking channel isolation, register decoding, short reads, acknowledgements, calibration/range validation, axis/button mappings, sensor-loss neutral states, and unchanged inputs after reconnect. Run both transport configurations on hardware to verify wiring, actual travel, pairing, and simulator bindings.

The same script first tests all six supported units (Key U144, Scroll U186, Encoder U135, Joystick2 U024-V2, Fader U123, and Angle U005). It checks GPIO button bounce and timer wrap, analog millivolt calibration, 16/32-bit counter rollover, active-low/high button variants, recovery without replaying deltas, RGB register byte order and pixel bounds, GRB signal/latch encoding, and RMT cleanup/failure paths. The fake GPIO/ADC/RMT adapters cannot verify physical voltage, cable presence, LED timings, or installed unit firmware. The library's `examples/Units` sketch exercises the complete hardware API without HID.


Run `bash Test/RunCalibration.sh` for calibration API and storage tests under sanitizers: validation, queued commands, raw extrema captured between HTTP polls, preview without flash writes, save/reload, namespace isolation, failed writes, persisted defaults, and session expiry. `RunM5Inputs.sh` also checks the actual Fly/Drive loops pause HID during calibration, expose live readings, save settings, and resume safely. Browser UI checks use a local simulated device; flash retention, the setup WiFi network, and USB/BLE coexistence still require a board test.

Calibration tests also check Fader/Angle millivolt ranges with different per-axis bounds, rejected out-of-range limits, and saved/reloaded analog endpoints.
