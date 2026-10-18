# OpenCtrl / Libraries / M5StackInputs

Optional Arduino-ESP32 3.x library for M5Stack input components. It uses Arduino `Wire`, GPIO/ADC, and the ESP32 RMT peripheral, with no additional vendor or LED library. It has no HID, Bluetooth, WiFi, HTTP, display, or storage dependency. FlyController and DriveController continue using their existing two-Joystick2 setup.

## Supported units

These are the exact models, rather than interchangeable drivers for similarly named units:

| Adapter | Unit / SKU | Connection | Input | RGB LEDs |
| --- | --- | --- | --- | --- |
| `Key` | Mechanical Key U144 | GPIO | Active-low, debounced button | 1 |
| `Scroll` | Scroll EC10E U186 | I2C `0x40` | Signed 32-bit counter and active-low button | 1 |
| `Encoder` | Encoder U135 | I2C `0x40` | Signed 16-bit counter and button | 2 |
| `Joystick2` | Hall joystick U024-V2 / STM32G030 | I2C `0x63` | 16-bit X/Y and active-low button | 1 |
| `Fader` | B10K Fader U123 | ADC + GPIO | Absolute slider position | 14 |
| `Angle` | Mini Angle U005 | ADC | Absolute potentiometer position | None |

Angle is an absolute potentiometer, not an incremental rotary encoder. Scroll's hardware counts quadrature edges; its 12 pulses per revolution do not imply 12 returned counter increments. Encoder `Begin()` selects pulse mode (30 pulses per revolution); its raw AB mode is deliberately not exposed as another interpretation of the same input.

## Composition

The public adapters are small concrete classes, with no inheritance hierarchy:

- `InputUtils.hpp/.cpp` contains independent value utilities: `AxisCalibration`, `ButtonDebouncer`, `CounterTracker`, report-value conversions, and `RGBColor`.
- `DeviceIO.hpp/.cpp` contains reusable hardware operations: checked `I2CDevice`, `GPIOButton`, calibrated-millivolt `AnalogInput`, RMT-backed `RGBStrip`, `I2CRGB`, and `RegisterEncoder`.
- `M5StackInputs.hpp/.cpp` composes those helpers into the six model-specific adapters. Include this header for the complete API; existing Joystick2 code remains compatible.

Key composes a GPIO button and one RGB strip. Fader composes an analog input and a 14-pixel strip; Angle composes the same analog helper. Scroll and Encoder compose the same register and counter helpers, supplying their own counter widths. Joystick2 reuses I2C access, calibration, and RGB register output. Hardware interfaces do not know how inputs map to HID actions.

## Wiring

Grove colors below refer to these units' original cables. Use board GPIO numbers, not another M5Stack board's Port B pin numbers. Supply the units with 5V and share ground. The documented Key signal is 3.3V; Fader outputs 0–3.3V and Angle 0–2.5V when supplied appropriately. ESP32 GPIOs are not 5V tolerant. Use 3.3V-compatible I2C pull-ups.

| Unit | Black | Red | Yellow | White |
| --- | --- | --- | --- | --- |
| Scroll / Encoder / Joystick2 | GND | 5V | SDA | SCL |
| Key | GND | 5V | LED data | Button input |
| Fader | GND | 5V | LED data | Analog output |
| Angle | GND | 5V | Unconnected | Analog output |

Only the three I2C units connect through **PaHub**. Key, Fader, and Angle need direct GPIO/ADC connections; a PaHub does not multiplex those signals. This library does not implement PbHub. Scroll and Encoder share address `0x40`, so put them on separate PaHub channels (or separate I2C buses). Multiple Joystick2 units at `0x63` likewise need isolated channels.

The [PaHub v2.1](https://docs.m5stack.com/en/unit/unit-PaHub%20v2.1) has six physical ports on a PCA9548AP. `I2CRoute` supports the chip's eight channel bits; use ports present on your hub. Every transaction selects exactly one channel and checks writes, acknowledgements, and read length. Failed reads leave the destination unchanged. Transfers are limited to 32 bytes, including the register byte for writes.

The caller initializes Wire with board-specific pins and a timeout. The library never resets the bus to GPIO 21/22. Channel `-1` uses the current direct bus without touching a hub; avoid address conflicts with any selected downstream device. Access a shared bus/hub from one task. Give each GPIO/RMT pin one owner, and keep ADC inputs on ADC1 when WiFi is used. On XIAO ESP32S3, the example uses GPIO 1 and 3 for analog inputs, and SDA/SCL are GPIO 5/6.

## Use

The project build scripts discover this library through `--libraries Libraries`. It also works as a normal Arduino library or with `--library Libraries/M5StackInputs`.

```cpp
#include <M5StackInputs.hpp>

M5Stack::Joystick2 stick(Wire, {0x63, 2, 0x70});
M5Stack::Encoder knob(Wire, {0x40, 0, 0x70});
M5Stack::Scroll wheel(Wire, {0x40, 1, 0x70});
M5Stack::Key key(7, 8);       // Button GPIO, LED GPIO.
M5Stack::Fader fader(1, 2);   // ADC GPIO, LED GPIO.
M5Stack::Angle angle(3);     // ADC GPIO.

// setup(): Wire.begin(SDA, SCL); Wire.setTimeOut(20);
// Check knob.Begin(), key.Begin(), fader.Begin(), and angle.Begin().
// Joystick2 and Scroll need no initialization transaction.
// An omitted LED pin (-1) enables input-only use of Key/Fader.
```

`Read()` and `ReadRaw()` return success/failure; inspect that before using their output. Failed unit reads clear the state. A GPIO/analog unit cannot detect an unplugged cable: an open button resembles a released key, and ADC zero is a valid reading, including when the ESP32 ADC API reports a conversion failure. Their success result validates initialization/configuration, not cable presence.

`Key::Read(ButtonState&)` provides `pressed`, `pressedEdge`, and `releasedEdge`. Poll it frequently (e.g. every 1–5 ms). Default debounce is 10 ms and can be changed in the constructor. A held key at `Begin()` is reported as pressed without a synthetic press edge. Debounce uses elapsed unsigned time and handles `millis()` rollover. Joystick/encoder button reads expose the unit's current register state rather than applying the GPIO debounce filter.

### Axes and calibration

`Joystick2::ReadRaw(JoystickRawState&)` reads two little-endian 16-bit axes at `0x00` and the button at `0x20`. `Read(JoystickState&)` normalizes them to [-1, +1] through public `x` and `y` calibration settings. `CalibrateCenter()` takes median samples and retains endpoints; failed calibration leaves both axes unchanged.

Fader and Angle share `ReadRaw(uint16_t&)` in **millivolts**, `Read(float&)` in [-1, +1], and `Calibration()` returning a mutable `AxisCalibration&`. Millivolt readings use the ESP32's ADC calibration and per-pin 11 dB attenuation without changing global `analogReadResolution()`. The nominal default ranges are 0–3300 mV (Fader) and 0–2500 mV (Angle), with midpoint center and zero deadzone. Measure actual endpoints before use: ESP32-S3 ADC saturation can occur below the Fader's maximum voltage, so nominal endpoints may not reach a normalized +1. Inversion, deadzone, asymmetric travel, and clipped endpoints use the same helper as Joystick2.

For a full-travel pedal/slider from a normalized axis, use `(axis + 1) / 2` to get [0, 1]. `PositiveTrigger` uses only the positive half of a centered stick; it is not a full-travel slider conversion. `SignedAxis` converts [-1, +1] to [-127, +127]. Invalid ranges and non-finite deadzones are rejected.

The separate [ControllerCalibration library](../ControllerCalibration) can bind `&fader.Calibration()` / `&angle.Calibration()` with millivolt raw bounds, alongside Joystick2's axis settings. Feed it raw readings from the controller loop. That library owns the website and NVS persistence. The supplied Fly/Drive calibration pages retain their four joystick axes; adding physical units requires explicit bindings and action mappings in the consuming project.

### Encoders

`ReadRaw(EncoderRawState&)` returns the signed hardware counter and button. `Read(EncoderState&)` also returns `delta`, calculated from the non-destructive counter register `0x10`. No read-to-clear increment register is used, so a failed button transaction does not consume motion. Scroll reads all four counter bytes exposed by its firmware; U135 reads two.

The first successful `Read()` after construction, `ResetTracking()`, or a failed unit read produces delta zero. Later reads handle counter rollover in both directions. Poll often enough that movement is less than half the counter range between samples (32768 counts for U135). Reset tracking after intentionally resetting or power-cycling hardware; device reboot cannot otherwise be distinguished from counter motion. Raw reads do not advance the delta baseline. Neither adapter resets the hardware counter on startup.

Encoder defaults to active-low, matching M5Stack's Arduino example and current MicroPython driver. M5Stack's older protocol PDF instead describes active-high. For firmware with that behavior, construct `Encoder(wire, route, M5Stack::ButtonPolarity::ActiveHigh)`. Both interpretations are tested; verify pressed/released values with your unit.

### LEDs

Every color is `RGBColor{red, green, blue}` with byte channels. Pixel indices are **zero-based** everywhere. For Key/Fader, use `LEDs().SetColor(index, color)` or `Fill(color)` and then check `LEDs().Show()`. The strip buffers colors and sends GRB via ESP32 RMT, including a low reset/latch interval. `Begin()` can fail if a pin is invalid or RMT resources are unavailable; `End()` and destruction release RMT resources. Strip ownership is non-copyable.

Encoder/Scroll `LEDs().SetColor(index, color)` writes immediately via I2C. Joystick2 uses `SetColor(color)`. RGB helpers translate model-specific indexing and register byte order. All hardware operations report failure. GPIO LED outputs require Arduino-ESP32 3.x and an ESP32 with RMT, including the project's ESP32-S3.

## Example and tests

[examples/Units/Units.ino](examples/Units/Units.ino) exercises all six models with serial readings and low-brightness RGB output. Its pins and hub channels are a concrete XIAO ESP32S3 wiring example, independent of the Fly/Drive HID mappings. Edit them to match your hardware.

```bash
# From Source/XIAOESP32S3Core, using this project's Arduino CLI configuration:
arduino-cli --config-file arduino-cli.yaml compile \
  --fqbn esp32:esp32:XIAO_ESP32S3:USBMode=default,CDCOnBoot=cdc \
  --libraries ../../Libraries --build-path /tmp/openctrl-units-build \
  ../../Libraries/M5StackInputs/examples/Units

# From the repository root:
bash Test/RunM5Inputs.sh
```

Host tests cover all six units, register widths, button polarity, hub isolation, failures and recovery, rollover, button bounce and timer wrap, millivolt calibration, RGB byte/signal encoding, bounds, and RMT cleanup. They also exercise the actual Fly/Drive USB/BLE sketches. Real wiring, ADC travel, LED timing, unit firmware variants, and target-host behavior still need hardware validation.

## Protocol references

- [Key U144](https://docs.m5stack.com/en/unit/Unit_Key) and [official Key example](https://github.com/m5stack/M5Stack/blob/master/examples/Unit/KEY/KEY-DEMO1/KEY-DEMO1.ino).
- [Scroll U186](https://docs.m5stack.com/en/unit/Unit_Scroll), [driver](https://github.com/m5stack/M5Unit-Scroll), and [internal firmware](https://github.com/m5stack/M5Unit-Scroll-Internal-FW/blob/main/code/APP/Encoder_EVQ/Core/Src/main.c) (32-bit counter and active-low button).
- [Encoder U135](https://docs.m5stack.com/en/unit/Unit_Encoder), [Arduino example](https://github.com/m5stack/M5Unit-Encoder/blob/master/examples/Unit_Encoder_M5Core/Unit_Encoder_M5Core.ino), [MicroPython driver](https://github.com/m5stack/uiflow-micropython/blob/master/m5stack/libs/unit/encoder.py), and [older protocol PDF](https://m5stack-doc.oss-cn-shenzhen.aliyuncs.com/801/Unit-Encoder_Protocol.pdf).
- [Joystick2 U024-V2](https://docs.m5stack.com/en/unit/Unit_Joystick2), [driver](https://github.com/m5stack/M5Unit-Joystick2), and [internal firmware](https://github.com/m5stack/M5Unit-Joystick2-Internal-FW).
- [Fader U123](https://docs.m5stack.com/en/unit/Unit_Fader) and [Angle U005](https://docs.m5stack.com/en/unit/Unit_Angle).
- [Espressif ADC](https://docs.espressif.com/projects/arduino-esp32/en/latest/api/adc.html) and [RMT APIs](https://docs.espressif.com/projects/arduino-esp32/en/latest/api/rmt.html).
