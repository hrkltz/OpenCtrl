# OpenCtrl / Examples / FlyController

A local flight controller using two M5Stack Joystick2 units and the shared ESP32 HID core. USB is the default; Bluetooth LE or both connections can be selected in [Config.hpp](XIAOESP32S3Core/Config.hpp) by changing `OPENCTRL_CONTROLLER_TRANSPORT` to 2 or 3. It exposes the core's generic gamepad profile (report ID 3); bind its axes in the simulator.

## Hardware

- XIAO ESP32S3, USB power/data cable, and expansion board with OLED.
- M5Stack PaHub v2.1 (U040-B-V21), address `0x70`, connected to the board's SDA/SCL.
- Two Joystick2 units (U024-V2), both address `0x63`, on hub channels 2 and 3.
- Power and wire the units according to their M5Stack pinouts; I2C logic at the ESP must remain compatible with 3.3 V.

Channels and hub address are configurable. The shared [M5StackInputs library](../../Libraries/M5StackInputs) owns hardware access and calibration; the HID core owns the descriptor and transport. No additional vendor joystick or hub libraries are needed for this example.

## Mapping

| Physical input | Gamepad field | Action |
| --- | --- | --- |
| Channel 2 X | X | Roll |
| Channel 2 Y | Y | Pitch |
| Channel 3 X | Rz | Rudder |
| Channel 3 Y, inverted | Z | Throttle, positive half only |
| Channel 2 click | Button 1 (bit 0) | Primary action |
| Channel 3 click | Button 2 (bit 1) | Secondary action |

Saved calibration loads at power-on. Without valid saved settings, keep both sticks centered for startup calibration. Default endpoints are 0–65535 with a 5% deadzone; the calibration page can change each axis's limits, center, deadzone, and inversion. Pitch direction can also be reversed in the simulator.

The spring-centered auxiliary stick gives zero throttle at center and full throttle on its positive half after inversion. Its other half also gives zero throttle. The shared library's Fader U123 or Angle U005 can provide full-travel absolute throttle after adding wiring, calibration bindings, and a mapping from [-1, +1] across the complete throttle range. The current example retains its two joysticks and does not latch throttle after releasing the stick.

The descriptor reuses `HIDProfiles::Gamepad()`. The composed profile sets the Rz neutral state to 0 for the rudder, so sensor loss and reconnect release it to center rather than full rudder.

## Build and Run

Install the [core dependencies](../../Source/XIAOESP32S3Core/README.md#setup), then run from the root:

```bash
./Compile.sh --project Examples/FlyController
./BuildAndDeploy.sh --project Examples/FlyController
```

For BLE, set the transport to 2, rebuild, and pair **OpenCtrl** in the host's Bluetooth settings. USB mode needs no Bluetooth pairing. Switching profiles/transports requires a restart; remove old cached BLE pairings if the host retains an earlier descriptor.

The loop samples at roughly 50 Hz and sends current absolute state. A missing input releases all controls; recovered inputs resume with fresh values. It calls `HIDUtil::Service()` directly. Its separate calibration web service cannot send controller input and does not use the WebController heartbeat/session protocol. It creates its own WiFi setup network, so no home-network credentials are needed. In both mode, the report is mirrored to connected hosts.


## Calibration Page

After flashing, connect your phone or computer to **FlyCtrl-xxxx**, using the per-boot WiFi password shown on OLED line 5 (below “WiFi password”). Open **http://192.168.4.1/**, or the address shown on the display. Stay connected even if the phone reports that this WiFi network has no internet. One setup client can connect at a time. USB/BLE HID uses the selected transport independently.

1. Choose **Start calibration**. HID output pauses at its safe neutral state.
2. Choose **Capture range**, move both sticks through full travel, then **Finish capture**. The ESP records extrema at the sensor polling rate, independently of browser refresh speed.
3. Release the sticks and choose **Set centers**. Adjust deadzone percentages and inversion if needed.
4. **Preview** applies the settings to the live on-screen response without writing flash.
5. **Save to controller** stores the complete validated calibration in Preferences/NVS. **Reset defaults** saves factory defaults after confirmation.
6. Choose **Done / resume controls**. Any preview not saved is discarded. Closing/disconnecting the page expires calibration after 30 seconds, discards its unsaved preview, and resumes fresh local input.

Saved settings survive power cycles and are kept separately for FlyController and DriveController. Invalid/missing saved data falls back to startup calibration. The page remains available when a joystick is missing, so hardware recovery does not require reflashing. A failed flash write is shown as an error and does not apply the submitted settings.

The shared [ControllerCalibration library](../../Libraries/ControllerCalibration) owns the page, API, and storage. [M5StackInputs](../../Libraries/M5StackInputs) remains independent of WiFi and storage.
