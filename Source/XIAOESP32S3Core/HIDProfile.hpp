#pragma once

#include "HIDDescriptor.hpp"
#include <functional>

// A profile describes a device. Routing belongs to its registered instance.
enum class HIDTransport : uint8_t { USB = 1, Bluetooth = 2, Both = 3 };
enum class HIDReportType : uint8_t { Input = 1, Output = 2, Feature = 3 };

struct HIDNeutralReport {
    uint8_t id;
    std::vector<uint8_t> data;
};

struct HIDProfileCallbacks {
    std::function<void(uint8_t, const uint8_t*, size_t)> output;
    std::function<void(uint8_t, const uint8_t*, size_t)> setFeature;
    std::function<uint16_t(uint8_t, uint8_t*, uint16_t)> getFeature;
};

struct HIDProfile {
    std::vector<uint8_t> descriptor;
    std::vector<HIDNeutralReport> neutral;
    HIDProfileCallbacks callbacks;

    // Remaps all local IDs to a consecutive block, including neutral reports.
    // Callbacks receive the registered IDs. Leaves this value unchanged on error.
    bool RemapReportIds(uint8_t firstId);
};

// Reusable value definitions, independent of USB and Bluetooth libraries.
namespace HIDProfiles {
    HIDProfile Keyboard();
    HIDProfile Mouse();
    HIDProfile Gamepad();
    HIDProfile Consumer();
    HIDProfile System();
}
