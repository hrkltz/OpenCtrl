#pragma once
#include <M5StackInputs.hpp>

struct DriveControls {
    int8_t steering, brake, throttle;
    uint32_t buttons;
    static DriveControls Map(const M5Stack::JoystickState& wheel, const M5Stack::JoystickState& pedals) {
        return {M5Stack::SignedAxis(wheel.x), M5Stack::PositiveTrigger(-pedals.y),
            M5Stack::PositiveTrigger(pedals.y),
            uint32_t(wheel.pressed) << 6 | uint32_t(pedals.pressed) << 7};
    }
};
