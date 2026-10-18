#pragma once

#include "DeviceIO.hpp"

namespace M5Stack {
struct JoystickRawState {
    uint16_t x = 0, y = 0;
    bool pressed = false;
};
struct JoystickState {
    float x = 0, y = 0;
    bool pressed = false;
};

class Joystick2 {
    I2CDevice device;
    I2CRGB led;
public:
    AxisCalibration x, y;
    Joystick2(TwoWire& wire, I2CRoute route = {})
        : device(wire, route), led(wire, route, 1, RGBRegisterFormat::PackedBGR) {}
    bool SetColor(RGBColor color) { return led.SetColor(0, color); }
    bool ReadRaw(JoystickRawState& state);
    bool Read(JoystickState& state);
    // Keep the stick centered. Explicit endpoints are retained on recalibration.
    bool CalibrateCenter(uint8_t samples = 21);
};

class Key {
    GPIOButton button;
    RGBStrip leds;
    bool distinctPins;
public:
    Key(int buttonPin, int ledPin = -1, uint32_t debounceMilliseconds = 10)
        : button(buttonPin, debounceMilliseconds), leds(ledPin, 1), distinctPins(buttonPin != ledPin) {}
    bool Begin() { return distinctPins && button.Begin() && (!leds.Configured() || leds.Begin()); }
    bool Read(ButtonState& state) { return button.Read(state); }
    RGBStrip& LEDs() { return leds; }
};

class Fader {
    AnalogInput input;
    RGBStrip leds;
    bool distinctPins;
public:
    Fader(int analogPin, int ledPin = -1)
        : input(analogPin, 3300), leds(ledPin, 14), distinctPins(analogPin != ledPin) {}
    bool Begin() { return distinctPins && input.Begin() && (!leds.Configured() || leds.Begin()); }
    bool ReadRaw(uint16_t& millivolts) { return input.ReadRaw(millivolts); }
    bool Read(float& value) { return input.Read(value); }
    AxisCalibration& Calibration() { return input.calibration; }
    RGBStrip& LEDs() { return leds; }
};

class Angle {
    AnalogInput input;
public:
    explicit Angle(int analogPin) : input(analogPin, 2500) {}
    bool Begin() { return input.Begin(); }
    bool ReadRaw(uint16_t& millivolts) { return input.ReadRaw(millivolts); }
    bool Read(float& value) { return input.Read(value); }
    AxisCalibration& Calibration() { return input.calibration; }
};

class Encoder {
    I2CDevice device;
    RegisterEncoder input;
    I2CRGB leds;
public:
    Encoder(TwoWire& wire, I2CRoute route = {0x40, -1, 0x70},
            ButtonPolarity polarity = ButtonPolarity::ActiveLow)
        : device(wire, route), input(wire, route, 2, polarity), leds(wire, route, 2) {}
    bool Begin(); // Select pulse mode; leaves the hardware counter unchanged.
    bool ReadRaw(EncoderRawState& state) { return input.ReadRaw(state); }
    bool Read(EncoderState& state) { return input.Read(state); }
    void ResetTracking() { input.ResetTracking(); }
    I2CRGB& LEDs() { return leds; }
};

class Scroll {
    RegisterEncoder input;
    I2CRGB leds;
public:
    Scroll(TwoWire& wire, I2CRoute route = {0x40, -1, 0x70})
        : input(wire, route, 4, ButtonPolarity::ActiveLow), leds(wire, route, 1) {}
    bool ReadRaw(EncoderRawState& state) { return input.ReadRaw(state); }
    bool Read(EncoderState& state) { return input.Read(state); }
    void ResetTracking() { input.ResetTracking(); }
    I2CRGB& LEDs() { return leds; }
};
}
