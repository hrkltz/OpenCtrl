#pragma once
#include <HIDUtil.hpp>

// 1: USB, 2: Bluetooth LE, 3: different profiles on USB and Bluetooth LE.
#ifndef OPENCTRL_WEB_MODE
#define OPENCTRL_WEB_MODE 1
#endif
static_assert(OPENCTRL_WEB_MODE >= 1 && OPENCTRL_WEB_MODE <= 3, "Invalid WebController mode");

namespace WebConfig {
constexpr bool mixed = OPENCTRL_WEB_MODE == 3;
constexpr HIDTransport primaryTransport = OPENCTRL_WEB_MODE == 2 ? HIDTransport::Bluetooth : HIDTransport::USB;
constexpr HIDTransport gamepadTransport = mixed ? HIDTransport::Bluetooth : primaryTransport;
constexpr HIDTransport penTransport = primaryTransport;
constexpr bool secondaryMouse = mixed;
constexpr HIDTransport secondaryMouseTransport = HIDTransport::Bluetooth;
constexpr uint8_t secondaryMouseId = 8; // Pen retains ID 7 in every preset.
constexpr const char* name = mixed ? "Mixed USB / Bluetooth" : OPENCTRL_WEB_MODE == 2 ? "Bluetooth" : "USB";
constexpr const char* primaryMouseName = OPENCTRL_WEB_MODE == 2 ? "Bluetooth mouse" : "USB mouse";
}
