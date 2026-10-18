# OpenCtrl / Examples / DriveController

A local driving controller using two M5Stack Joystick2 units and the shared ESP32 HID core. USB is the default; Bluetooth LE or both connections can be selected in [Config.hpp](XIAOESP32S3Core/Config.hpp) by changing `OPENCTRL_CONTROLLER_TRANSPORT` to 2 or 3. It exposes the core's generic gamepad profile (report ID 3); bind its axes in the game. It does not implement a proprietary racing-wheel protocol or force feedback.

## Hardware

- XIAO ESP32S3, USB power/data cable, and expansion board with OLED.
- M5Stack PaHub v2.1 (U040-B-V21), address `0x70`, connected to the board's SDA/SCL.
- Two Joystick2 units (U024-V2), both address `0x63`, on hub channels 2 and 3.
- Power and wire the units according to their M5Stack pinouts; I2C logic at the ESP must remain compatible with 3.3 V.

Channels and hub address are configurable. The shared [M5StackInputs library](../../Libraries/M5StackInputs) owns hardware access and calibration; the HID core owns the descriptor and transport. No additional vendor joystick or hub libraries are needed for this example.

## Mapping

| Physical input | Gamepad field | Action |
| --- | --- | --- |
| Channel 2 X | X | Steering |
| Channel 3 Y positive, after inversion | Rz | Accelerator |
| Channel 3 Y negative, after inversion | Z | Brake |
| Channel 2 click | Button 7 (bit 6) | Downshift |
| Channel 3 click | Button 8 (bit 7) | Upshift |

At center both pedals rest at -127; full travel reaches +127 on the active pedal. Accelerator and brake cannot be applied simultaneously with this one-stick mapping. Channel 2 Y and channel 3 X remain available for later controls. Button numbers shown in a game may differ, so check its binding screen.

Saved calibration loads at power-on. Without valid saved settings, keep both sticks centered for startup calibration. Default endpoints are 0–65535 with a 5% deadzone; adjust limits, center, deadzone, and inversion through the calibration page. Steering uses channel 2 X; change [Controls.hpp](XIAOESP32S3Core/Controls.hpp) if your physical mounting requires the Y axis instead.

The shared library also supports Encoder U135, Scroll U186, Fader U123, Angle U005, and Key U144. They can replace or extend these inputs through explicit wiring, calibration bindings, and action mappings without changing the HID backend. This example retains the two-joystick setup. Fader/Angle provide absolute positions; the rotary encoders provide relative motion that needs a bounded position mapping for steering or pedals.

## Build and Run

Install the [core dependencies](../../Source/XIAOESP32S3Core/README.md#setup), then run from the root:

```bash
./Compile.sh --project Examples/DriveController
./BuildAndDeploy.sh --project Examples/DriveController
```

For BLE, set the transport to 2, rebuild, and pair **OpenCtrl** in the host's Bluetooth settings. USB mode needs no Bluetooth pairing. The report can also be mirrored to both connections using value 3.

The loop samples at roughly 50 Hz and resends absolute state, including unchanged held controls after reconnect. Failed sensor reads release all controls, and recovery resumes with fresh values. The example calls `HIDUtil::Service()` itself. Its separate calibration service creates a WiFi setup network without home-network credentials; it does not use browser input routes or heartbeats.


## Calibration Page

After flashing, connect your phone or computer to **DriveCtrl-xxxx**, using the per-boot WiFi password shown on OLED line 5 (below “WiFi password”). Open **http://192.168.4.1/**, or the address shown on the display. Stay connected even if the phone reports that this WiFi network has no internet. One setup client can connect at a time. USB/BLE HID uses the selected transport independently.

1. Choose **Start calibration**. HID output pauses at its safe neutral state.
2. Choose **Capture range**, move both sticks through full travel, then **Finish capture**. The ESP records extrema at the sensor polling rate, independently of browser refresh speed.
3. Release the sticks and choose **Set centers**. Adjust deadzone percentages and inversion if needed.
4. **Preview** applies the settings to the live on-screen response without writing flash.
5. **Save to controller** stores the complete validated calibration in Preferences/NVS. **Reset defaults** saves factory defaults after confirmation.
6. Choose **Done / resume controls**. Any preview not saved is discarded. Closing/disconnecting the page expires calibration after 30 seconds, discards its unsaved preview, and resumes fresh local input.

Saved settings survive power cycles and are kept separately for FlyController and DriveController. Invalid/missing saved data falls back to startup calibration. The page remains available when a joystick is missing, so hardware recovery does not require reflashing. A failed flash write is shown as an error and does not apply the submitted settings.

The shared [ControllerCalibration library](../../Libraries/ControllerCalibration) owns the page, API, and storage. [M5StackInputs](../../Libraries/M5StackInputs) remains independent of WiFi and storage.
