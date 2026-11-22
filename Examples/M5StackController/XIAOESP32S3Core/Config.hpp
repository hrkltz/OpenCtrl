#pragma once
#include <HIDUtil.hpp>

// Override with 1 (USB), 2 (Bluetooth LE), or 3 (both) when compiling.
#ifndef OPENCTRL_CONTROLLER_TRANSPORT
#define OPENCTRL_CONTROLLER_TRANSPORT 1
#endif
namespace Config {
constexpr HIDTransport transport = static_cast<HIDTransport>(OPENCTRL_CONTROLLER_TRANSPORT);
static_assert(OPENCTRL_CONTROLLER_TRANSPORT >= 1 && OPENCTRL_CONTROLLER_TRANSPORT <= 3, "Invalid HID transport");
constexpr uint8_t hubAddress = 0x70;
constexpr uint8_t portCount = 6;             // Physical PaHub ports 0–5.
constexpr uint32_t i2cFrequency = 400000;
constexpr uint32_t pollMicroseconds = 4000;  // 250 Hz, including input reads and HID output.
constexpr size_t maxBindings = 16;
constexpr float mouseSpeed = 8;              // Mouse counts per cycle at full deflection.
constexpr float wheelSpeed = 0.1f;           // Wheel detents per cycle at full deflection.
constexpr float stepMouseCounts = 4;         // Mouse counts per encoder step.
constexpr float stepWheelDetents = 1;        // Wheel detents per encoder step.
constexpr int32_t turnRange = 24;            // Default encoder steps from the start position to either end.
constexpr uint32_t refreshCycles = 50;       // Resend held keys and mouse buttons every 200 ms.
constexpr uint32_t scanMilliseconds = 3000;  // Port scan interval during setup.
}
