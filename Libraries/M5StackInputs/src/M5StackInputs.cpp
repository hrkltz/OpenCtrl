#include "M5StackInputs.hpp"
#include <Arduino.h>
#include <algorithm>
#include <array>

namespace M5Stack {
bool Encoder::Begin() {
    input.ResetTracking();
    const uint8_t pulseMode = 0;
    return device.Write(0x00, &pulseMode, 1);
}
bool Joystick2::ReadRaw(JoystickRawState& state) {
    state = {};
    uint8_t axes[4], button;
    if (!device.Read(0x00, axes, sizeof(axes)) || !device.Read(0x20, &button, 1) || button > 1) return false;
    state = {uint16_t(axes[0] | uint16_t(axes[1]) << 8), uint16_t(axes[2] | uint16_t(axes[3]) << 8), button == 0};
    return true;
}
bool Joystick2::Read(JoystickState& state) {
    state = {};
    JoystickRawState raw;
    if (!x.Valid() || !y.Valid() || !ReadRaw(raw)) return false;
    state = {x.Normalize(raw.x), y.Normalize(raw.y), raw.pressed};
    return true;
}
bool Joystick2::CalibrateCenter(uint8_t samples) {
    if (samples < 3 || samples > 31 || !x.Valid() || !y.Valid()) return false;
    std::array<uint16_t, 31> xs{}, ys{};
    for (size_t i = 0; i < samples; ++i) {
        JoystickRawState raw;
        if (!ReadRaw(raw)) return false;
        xs[i] = raw.x; ys[i] = raw.y;
        delay(5);
    }
    std::sort(xs.begin(), xs.begin() + samples);
    std::sort(ys.begin(), ys.begin() + samples);
    auto nextX = x, nextY = y;
    nextX.center = xs[samples / 2]; nextY.center = ys[samples / 2];
    if (!nextX.Valid() || !nextY.Valid()) return false;
    x = nextX; y = nextY;
    return true;
}
}
