#pragma once

#include <Wire.h>
#include "InputUtils.hpp"
#include <array>
#include <cstddef>

namespace M5Stack {
struct I2CRoute {
    uint8_t address = 0x63;
    int8_t channel = -1; // -1: direct bus; 0..7: PCA9548 channel.
    uint8_t hubAddress = 0x70;
};

class I2CDevice {
    TwoWire& wire;
    I2CRoute route;
    bool Select();
public:
    I2CDevice(TwoWire& wire, I2CRoute route) : wire(wire), route(route) {}
    bool Read(uint8_t reg, uint8_t* data, size_t length);
    bool Write(uint8_t reg, const uint8_t* data, size_t length);
};

class GPIOButton {
    int pin;
    ButtonDebouncer debounce;
    bool started = false;
public:
    GPIOButton(int pin, uint32_t debounceMilliseconds = 10) : pin(pin), debounce(debounceMilliseconds) {}
    bool Begin();
    bool Read(ButtonState& state);
};

// Raw readings are calibrated millivolts, independent of analogReadResolution().
class AnalogInput {
    int pin;
    bool started = false;
public:
    AxisCalibration calibration;
    AnalogInput(int pin, int32_t maximumMillivolts)
        : pin(pin), calibration{0, maximumMillivolts / 2, maximumMillivolts, 0, false} {}
    bool Begin();
    bool ReadRaw(uint16_t& millivolts);
    bool Read(float& value);
};

// Bounded to the largest supported unit; no allocation or external LED library.
class RGBStrip {
    int pin;
    uint8_t count;
    bool started = false;
    std::array<RGBColor, 14> colors{};
public:
    RGBStrip(int pin, uint8_t count) : pin(pin), count(count) {}
    ~RGBStrip();
    RGBStrip(const RGBStrip&) = delete;
    RGBStrip& operator=(const RGBStrip&) = delete;
    bool Configured() const { return pin != -1; }
    bool Begin();
    void End();
    bool SetColor(uint8_t index, RGBColor color); // Zero-based; buffered until Show().
    void Fill(RGBColor color);
    bool Show();
};

enum class RGBRegisterFormat { IndexedRGB, PackedBGR };
class I2CRGB {
    I2CDevice device;
    uint8_t count;
    RGBRegisterFormat format;
public:
    I2CRGB(TwoWire& wire, I2CRoute route, uint8_t count, RGBRegisterFormat format = RGBRegisterFormat::IndexedRGB)
        : device(wire, route), count(count), format(format) {}
    bool SetColor(uint8_t index, RGBColor color); // Zero-based, writes immediately.
};

struct EncoderRawState { int32_t counter = 0; bool pressed = false; };
struct EncoderState { int32_t counter = 0, delta = 0; bool pressed = false; };

// Shared register protocol, with explicit counter width and button polarity.
class RegisterEncoder {
    I2CDevice device;
    CounterTracker tracker;
    uint8_t bytes;
    ButtonPolarity polarity;
public:
    RegisterEncoder(TwoWire& wire, I2CRoute route, uint8_t bytes, ButtonPolarity polarity)
        : device(wire, route), bytes(bytes), polarity(polarity) {}
    bool ReadRaw(EncoderRawState& state);
    bool Read(EncoderState& state);
    void ResetTracking() { tracker.Reset(); }
};
}
