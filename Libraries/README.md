# OpenCtrl / Libraries

Optional Arduino libraries shared by examples. Hardware adapters belong here, independently of the ESP32 HID core. `Compile.sh` and `BuildAndDeploy.sh` discover libraries in this folder; only libraries included by a sketch are compiled.

- `M5StackInputs`: Key, Scroll, Encoder, Joystick2, Fader, and Angle adapters, sharing GPIO/analog/I2C helpers, optional PaHub routing, calibration, and RGB outputs.

- `ControllerCalibration`: shared calibration page, live readings, and persistent settings for hardware controllers.
