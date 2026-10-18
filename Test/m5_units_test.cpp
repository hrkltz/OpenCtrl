#include <M5StackInputs.hpp>
#include <esp32-hal-rmt.h>
#include <cassert>
#include <iostream>
#include <limits>

using namespace M5Stack;

static void setEncoder(TwoWire& wire, int channel, uint32_t counter, uint8_t button, uint8_t bytes) {
    auto& registers = wire.devices[{channel, 0x40}];
    registers[0x10].clear();
    for (uint8_t i = 0; i < bytes; ++i) registers[0x10].push_back(uint8_t(counter >> (8 * i)));
    registers[0x20] = {button};
}

static std::vector<uint8_t> decodeLEDs(int pin) {
    const auto& frame = mockRMTFrames.at(pin);
    assert(frame.size() % 24 == 1);
    std::vector<uint8_t> result;
    for (size_t offset = 0; offset + 1 < frame.size(); offset += 8) {
        uint8_t byte = 0;
        for (size_t i = 0; i < 8; ++i) {
            const auto& symbol = frame[offset + i];
            assert(symbol.level0 == 1 && symbol.level1 == 0);
            assert(symbol.duration0 + symbol.duration1 == 12);
            assert(symbol.duration0 == 3 || symbol.duration0 == 6);
            byte = uint8_t((byte << 1) | (symbol.duration0 == 6));
        }
        result.push_back(byte);
    }
    assert(frame.back().level0 == 0 && frame.back().level1 == 0);
    assert(frame.back().duration0 + frame.back().duration1 == 3000);
    return result;
}

static void checkGPIOUnits() {
    mockMillis = UINT32_MAX - 8;
    Key key(7, 8, 10);
    ButtonState button{true, true, true};
    assert(!key.Read(button) && !button.pressed && !button.pressedEdge);
    assert(key.Begin() && mockPinModes.at(7) == INPUT_PULLUP);
    mockDigital[7] = LOW;
    assert(key.Read(button) && !button.pressed);
    delay(4); mockDigital[7] = HIGH;
    assert(key.Read(button) && !button.pressed);
    delay(3); mockDigital[7] = LOW;
    assert(key.Read(button) && !button.pressed);
    delay(9); assert(key.Read(button) && !button.pressed);
    delay(1); assert(key.Read(button) && button.pressed && button.pressedEdge && !button.releasedEdge);
    assert(key.Read(button) && button.pressed && !button.pressedEdge);
    mockDigital[7] = HIGH;
    assert(key.Read(button) && button.pressed);
    delay(10); assert(key.Read(button) && !button.pressed && button.releasedEdge);
    assert(key.LEDs().SetColor(0, {0x12, 0x34, 0x56}) && !key.LEDs().SetColor(1, {}));
    assert(key.LEDs().Show());
    assert(decodeLEDs(8) == std::vector<uint8_t>({0x34, 0x12, 0x56}));
    mockRMTFailWrite = true;
    assert(!key.LEDs().Show());
    mockRMTFailWrite = false;
    key.LEDs().End();
    assert(!key.LEDs().Show() && !mockRMTActive.count(8));

    Key held(9); mockDigital[9] = LOW;
    assert(held.Begin() && held.Read(button) && button.pressed && !button.pressedEdge);
    assert(!held.LEDs().Begin());
    Key invalid(-1), conflict(7,7);
    assert(!invalid.Begin() && !conflict.Begin());
    RGBStrip tooLong(8,15), inputOnly(46,1);
    assert(!tooLong.Begin() && !inputOnly.Begin());
    mockRMTFailInit = true;
    assert(!key.LEDs().Begin());
    mockRMTFailInit = false;
    mockRMTFailEOT = true;
    assert(!key.LEDs().Begin() && !mockRMTActive.count(8));
    mockRMTFailEOT = false;

    Fader fader(1, 2);
    float value = 99;
    uint16_t raw = 99;
    assert(!fader.ReadRaw(raw) && raw == 0 && !fader.Read(value) && value == 0);
    assert(fader.Begin() && mockAttenuation.at(1) == ADC_11db);
    mockMillivolts[1] = 0; assert(fader.Read(value) && value == -1);
    mockMillivolts[1] = 1650; assert(fader.Read(value) && value == 0);
    mockMillivolts[1] = 3300; assert(fader.Read(value) && value == 1);
    fader.Calibration() = {150, 1600, 3100, 0, true};
    mockMillivolts[1] = 3100;
    assert(fader.ReadRaw(raw) && raw == 3100 && fader.Read(value) && value == -1);
    fader.Calibration().center = 3100;
    assert(!fader.Read(value) && value == 0);
    mockMillivolts[1] = UINT32_MAX;
    assert(!fader.ReadRaw(raw) && raw == 0);
    fader.LEDs().Fill({1,2,3});
    assert(fader.LEDs().SetColor(13, {4,5,6}) && !fader.LEDs().SetColor(14, {}));
    assert(fader.LEDs().Show());
    auto colors = decodeLEDs(2);
    assert(colors.size() == 42 && colors[0] == 2 && colors[39] == 5 && colors[40] == 4 && colors[41] == 6);
    {
        RGBStrip temporary(10, 1);
        assert(temporary.Begin() && mockRMTActive.count(10));
    }
    assert(!mockRMTActive.count(10));
    Angle angle(3), invalidAnalog(21);
    assert(angle.Begin() && !invalidAnalog.Begin());
    mockMillivolts[3] = 0; assert(angle.Read(value) && value == -1);
    mockMillivolts[3] = 1250; assert(angle.Read(value) && value == 0);
    mockMillivolts[3] = 2500; assert(angle.ReadRaw(raw) && raw == 2500 && angle.Read(value) && value == 1);
    angle.Calibration().inverted = true;
    assert(angle.Read(value) && value == -1);
}

static void checkI2CUnits() {
    TwoWire wire;
    setEncoder(wire, 0, 32767, 1, 2);
    setEncoder(wire, 1, 65535, 0, 4);
    Encoder encoder(wire, {0x40,0,0x70});
    Scroll scroll(wire, {0x40,1,0x70});
    EncoderState state;
    assert(encoder.Begin() && wire.devices.at({0,0x40}).at(0x00) == std::vector<uint8_t>({0}));
    assert(encoder.Read(state) && state.counter == 32767 && state.delta == 0 && !state.pressed);
    setEncoder(wire, 0, 32768, 0, 2);
    assert(encoder.Read(state) && state.counter == -32768 && state.delta == 1 && state.pressed);
    setEncoder(wire, 0, 32767, 1, 2);
    assert(encoder.Read(state) && state.delta == -1);
    encoder.ResetTracking();
    setEncoder(wire, 0, 65535, 1, 2);
    assert(encoder.Read(state) && state.counter == -1 && state.delta == 0);
    setEncoder(wire, 0, 0, 1, 2);
    assert(encoder.Read(state) && state.delta == 1);
    assert(scroll.Read(state) && state.counter == 65535 && state.delta == 0 && state.pressed);
    setEncoder(wire, 1, 65536, 1, 4);
    assert(scroll.Read(state) && state.counter == 65536 && state.delta == 1 && !state.pressed);
    scroll.ResetTracking();
    setEncoder(wire, 1, 0x7fffffff, 1, 4);
    assert(scroll.Read(state) && state.delta == 0);
    setEncoder(wire, 1, 0x80000000, 0, 4);
    assert(scroll.Read(state) && state.counter == INT32_MIN && state.delta == 1);
    setEncoder(wire, 1, 0x7fffffff, 0, 4);
    assert(scroll.Read(state) && state.delta == -1);
    scroll.ResetTracking();
    setEncoder(wire, 1, UINT32_MAX, 1, 4);
    assert(scroll.Read(state) && state.counter == -1 && state.delta == 0);
    setEncoder(wire, 1, 0, 1, 4);
    assert(scroll.Read(state) && state.delta == 1);

    wire.devices[{1,0x40}][0x20] = {}; // Failure after reading the counter.
    assert(!scroll.Read(state) && state.delta == 0 && state.counter == 0 && !state.pressed);
    setEncoder(wire, 1, 500, 1, 4);
    assert(scroll.Read(state) && state.delta == 0); // No replay after a failed read.
    wire.devices[{1,0x40}][0x20] = {2};
    assert(!scroll.Read(state));
    wire.shortRead = true;
    assert(!encoder.Read(state));
    wire.missingChannels.insert(0);
    assert(!encoder.Begin() && !encoder.LEDs().SetColor(0, {1,2,3}));
    wire.missingChannels.clear();
    setEncoder(wire, 0, 900, 1, 2);
    assert(encoder.Read(state) && state.delta == 0);
    assert(encoder.LEDs().SetColor(0, {0x12,0x34,0x56}));
    assert(wire.devices.at({0,0x40}).at(0x30) == std::vector<uint8_t>({1,0x12,0x34,0x56}));
    assert(encoder.LEDs().SetColor(1, {3,4,5}) && !encoder.LEDs().SetColor(2, {}));
    assert(wire.devices.at({0,0x40}).at(0x30) == std::vector<uint8_t>({2,3,4,5}));
    assert(scroll.LEDs().SetColor(0, {6,7,8}) && !scroll.LEDs().SetColor(1, {}));
    assert(wire.devices.at({1,0x40}).at(0x30) == std::vector<uint8_t>({0,6,7,8}));
    Encoder activeHigh(wire, {0x40,0,0x70}, ButtonPolarity::ActiveHigh);
    assert(activeHigh.Read(state) && state.pressed);
    wire.SetJoystick(2,0,65535,true);
    Joystick2 joystick(wire, {0x63,2,0x70});
    assert(joystick.SetColor({0x12,0x34,0x56}));
    assert(wire.devices.at({2,0x63}).at(0x30) == std::vector<uint8_t>({0x56,0x34,0x12,0}));
    JoystickRawState stick;
    assert(joystick.ReadRaw(stick) && stick.x == 0 && stick.y == 65535 && stick.pressed);
    TwoWire direct;
    setEncoder(direct, -1, 10, 1, 4);
    Scroll directScroll(direct);
    assert(directScroll.Read(state) && state.counter == 10 && direct.selections.empty());
}

int main() {
    checkGPIOUnits();
    checkI2CUnits();
    std::cout << "Six M5Stack units: GPIO debounce, analog calibration, counters, channel isolation, recovery, and RGB passed\n";
}
