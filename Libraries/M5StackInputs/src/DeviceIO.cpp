#include "DeviceIO.hpp"
#include <Arduino.h>
#include <esp32-hal-rmt.h>
#include <cstring>
#include <limits>

namespace M5Stack {
bool I2CDevice::Select() {
    if (route.address < 0x08 || route.address > 0x77 || route.channel < -1 || route.channel > 7) return false;
    if (route.channel == -1) return true;
    if (route.hubAddress < 0x08 || route.hubAddress > 0x77 || route.hubAddress == route.address) return false;
    wire.beginTransmission(route.hubAddress);
    const bool written = wire.write(uint8_t(1u << route.channel)) == 1;
    const bool sent = wire.endTransmission() == 0;
    return written && sent;
}

bool I2CDevice::Read(uint8_t reg, uint8_t* data, size_t length) {
    if (!data || !length || length > 32 || !Select()) return false;
    wire.beginTransmission(route.address);
    if (wire.write(reg) != 1) { wire.endTransmission(); return false; }
    if (wire.endTransmission(false) != 0) return false;
    if (wire.requestFrom(route.address, uint8_t(length)) != length) {
        while (wire.available()) wire.read();
        return false;
    }
    uint8_t bytes[32];
    for (size_t i = 0; i < length; ++i) {
        const int value = wire.read();
        if (value < 0) return false;
        bytes[i] = uint8_t(value);
    }
    memcpy(data, bytes, length);
    return true;
}

bool I2CDevice::Write(uint8_t reg, const uint8_t* data, size_t length) {
    if (!data || !length || length > 31 || !Select()) return false;
    wire.beginTransmission(route.address);
    const bool written = wire.write(reg) == 1 && wire.write(data, length) == length;
    const bool sent = wire.endTransmission() == 0;
    return written && sent;
}

bool GPIOButton::Begin() {
    started = false;
    if (pin < 0 || !digitalPinIsValid(pin)) return false;
    pinMode(pin, INPUT_PULLUP);
    debounce.Reset(digitalRead(pin) == LOW, millis());
    started = true;
    return true;
}
bool GPIOButton::Read(ButtonState& state) {
    state = {};
    if (!started) return false;
    state = debounce.Update(digitalRead(pin) == LOW, millis());
    return true;
}

bool AnalogInput::Begin() {
    started = false;
    if (pin < 0 || !digitalPinIsValid(pin) || digitalPinToAnalogChannel(pin) < 0) return false;
    pinMode(pin, INPUT);
    analogSetPinAttenuation(pin, ADC_11db);
    started = true;
    return true;
}
bool AnalogInput::ReadRaw(uint16_t& millivolts) {
    millivolts = 0;
    if (!started) return false;
    const uint32_t value = analogReadMilliVolts(pin);
    if (value > std::numeric_limits<uint16_t>::max()) return false;
    millivolts = uint16_t(value);
    return true;
}
bool AnalogInput::Read(float& value) {
    value = 0;
    uint16_t raw;
    if (!calibration.Valid() || !ReadRaw(raw)) return false;
    value = calibration.Normalize(raw);
    return true;
}

RGBStrip::~RGBStrip() { End(); }
bool RGBStrip::Begin() {
    if (started) return true;
    if (pin < 0 || !digitalPinCanOutput(pin) || !count || count > colors.size()) return false;
    if (!rmtInit(pin, RMT_TX_MODE, RMT_MEM_NUM_BLOCKS_1, 10000000)) return false;
    if (!rmtSetEOT(pin, LOW)) { rmtDeinit(pin); return false; }
    started = true;
    return true;
}
void RGBStrip::End() {
    if (started) rmtDeinit(pin);
    started = false;
}
bool RGBStrip::SetColor(uint8_t index, RGBColor color) {
    if (index >= count || index >= colors.size()) return false;
    colors[index] = color;
    return true;
}
void RGBStrip::Fill(RGBColor color) { colors.fill(color); }
bool RGBStrip::Show() {
    if (!started) return false;
    // 100 ns ticks: SK6812/WS2812 GRB data, followed by a 300 us low latch.
    rmt_data_t symbols[14 * 24 + 1]{};
    size_t offset = 0;
    for (uint8_t i = 0; i < count; ++i) {
        const uint8_t bytes[] = {colors[i].green, colors[i].red, colors[i].blue};
        for (uint8_t byte : bytes) {
            for (uint8_t bit = 0x80; bit; bit >>= 1) {
                auto& symbol = symbols[offset++];
                symbol.level0 = 1;
                symbol.duration0 = byte & bit ? 6 : 3;
                symbol.duration1 = byte & bit ? 6 : 9;
            }
        }
    }
    symbols[offset].duration0 = 1500;
    symbols[offset++].duration1 = 1500;
    // Blocking completion keeps the stack buffer alive until RMT stops using it.
    return rmtWrite(pin, symbols, offset, RMT_WAIT_FOR_EVER);
}

bool I2CRGB::SetColor(uint8_t index, RGBColor color) {
    if (index >= count) return false;
    if (format == RGBRegisterFormat::PackedBGR) {
        if (count != 1) return false;
        const uint8_t bytes[] = {color.blue, color.green, color.red, 0};
        return device.Write(0x30, bytes, sizeof(bytes));
    }
    const uint8_t bytes[] = {uint8_t(count == 1 ? 0 : index + 1), color.red, color.green, color.blue};
    return device.Write(0x30, bytes, sizeof(bytes));
}

bool RegisterEncoder::ReadRaw(EncoderRawState& state) {
    state = {};
    uint8_t data[4], button;
    if ((bytes != 2 && bytes != 4) || !device.Read(0x10, data, bytes) ||
        !device.Read(0x20, &button, 1) || button > 1) {
        tracker.Reset();
        return false;
    }
    uint32_t counter = 0;
    for (uint8_t i = 0; i < bytes; ++i) counter |= uint32_t(data[i]) << (8 * i);
    const uint64_t modulus = uint64_t(1) << (bytes * 8);
    const int64_t signedCounter = counter < modulus / 2 ? int64_t(counter) : int64_t(counter) - int64_t(modulus);
    state = {int32_t(signedCounter), button == (polarity == ButtonPolarity::ActiveHigh ? 1 : 0)};
    return true;
}
bool RegisterEncoder::Read(EncoderState& state) {
    state = {};
    EncoderRawState raw;
    if (!ReadRaw(raw)) return false;
    state = {raw.counter, tracker.Update(uint32_t(raw.counter), bytes * 8), raw.pressed};
    return true;
}
}

