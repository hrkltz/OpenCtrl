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
constexpr uint8_t primaryChannel = 2;
constexpr uint8_t secondaryChannel = 3;
constexpr uint32_t pollMilliseconds = 20;
}
