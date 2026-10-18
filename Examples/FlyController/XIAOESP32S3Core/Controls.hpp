#pragma once
#include <M5StackInputs.hpp>
#include <HIDProfile.hpp>

struct FlightControls {
    int8_t roll, pitch, throttle, rudder;
    uint32_t buttons;
    static HIDProfile Profile() {
        auto profile = HIDProfiles::Gamepad();
        profile.neutral[0].data[3] = 0; // Rz is a centered rudder, not a resting trigger.
        return profile;
    }
    static FlightControls Map(const M5Stack::JoystickState& stick, const M5Stack::JoystickState& auxiliary) {
        return {M5Stack::SignedAxis(stick.x), M5Stack::SignedAxis(stick.y),
            M5Stack::PositiveTrigger(auxiliary.y), M5Stack::SignedAxis(auxiliary.x),
            uint32_t(stick.pressed) | uint32_t(auxiliary.pressed) << 1};
    }
};
