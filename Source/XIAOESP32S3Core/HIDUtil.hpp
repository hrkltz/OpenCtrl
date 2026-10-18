#pragma once

#include "HIDProfile.hpp"

class HIDUtil {
public:
    enum Device : uint8_t {
        Keyboard = 1, Mouse = 2, Gamepad = 4, Consumer = 8, System = 16,
        Standard = Keyboard | Mouse | Gamepad,
        All = Standard | Consumer | System
    };

    // Register instances before Init. Report IDs identify instances globally.
    static bool RegisterProfile(const HIDProfile& profile, HIDTransport transport = HIDTransport::USB,
                                uint8_t firstReportId = 0);
    static bool RegisterProfile(const uint8_t* descriptor, size_t length,
                                const std::vector<HIDNeutralReport>& neutral,
                                HIDProfileCallbacks callbacks = {}, HIDTransport transport = HIDTransport::USB);
    static uint8_t NextReportId();
    static bool Init(uint8_t devices = Standard, HIDProfileCallbacks callbacks = {},
                     HIDTransport standardTransport = HIDTransport::USB);
    static bool Connected(HIDTransport transport = HIDTransport::Both);
    static bool Ready(HIDTransport transport = HIDTransport::Both);
    static bool ReadyForReport(uint8_t id);
    static size_t InputSize(uint8_t id);
    static HIDTransport Transports(uint8_t id);
    static bool SendInput(uint8_t id, const uint8_t* data, size_t length);
    // Restrict this send to a subset of the instance's configured transports.
    static bool SendInputTo(uint8_t id, const uint8_t* data, size_t length, HIDTransport transport);
    static bool ReleaseAll();
    static void Service();

    static bool SendKeyboard(uint8_t modifiers, const uint8_t keys[6], uint8_t id = 1);
    static bool SendMouse(int dx, int dy, int wheel, int pan, uint8_t buttons, uint8_t id = 2);
    static bool SendGamepad(int8_t x, int8_t y, int8_t z, int8_t rz,
                            int8_t rx, int8_t ry, uint8_t hat, uint32_t buttons, uint8_t id = 3);
    static bool SendConsumer(uint16_t usage, uint8_t id = 4);
    static bool SendSystem(uint8_t usage, uint8_t id = 5);
};
