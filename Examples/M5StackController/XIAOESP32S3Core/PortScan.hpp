#pragma once
#include <Wire.h>
#include <algorithm>
#include <array>
#include <vector>
#include "Config.hpp"

// What answers on the direct bus and behind each hub port. Scanning blocks the bus for
// several milliseconds, so the controller only scans while setup pauses HID.
struct PortScan {
    bool scanned = false, hub = false;
    std::vector<uint8_t> direct;
    std::array<std::vector<uint8_t>, Config::portCount> ports;
};

inline bool OpenHubChannels(TwoWire& wire, uint8_t mask) {
    wire.beginTransmission(Config::hubAddress);
    wire.write(mask);
    return wire.endTransmission() == 0;
}

inline std::vector<uint8_t> ProbeBus(TwoWire& wire, const std::vector<uint8_t>& skip) {
    std::vector<uint8_t> found;
    for (uint8_t address = 0x08; address <= 0x77; ++address) {
        if (address == Config::hubAddress || std::find(skip.begin(), skip.end(), address) != skip.end()) continue;
        wire.beginTransmission(address);
        if (wire.endTransmission() == 0) found.push_back(address);
    }
    return found;
}

inline PortScan ScanPorts(TwoWire& wire) {
    PortScan scan;
    scan.scanned = true;
    // With every channel closed only the direct bus answers; those devices are not repeated per port.
    scan.hub = OpenHubChannels(wire, 0);
    scan.direct = ProbeBus(wire, {});
    if (!scan.hub) return scan;
    for (uint8_t port = 0; port < Config::portCount; ++port)
        if (OpenHubChannels(wire, uint8_t(1u << port))) scan.ports[port] = ProbeBus(wire, scan.direct);
    OpenHubChannels(wire, 0);
    return scan;
}
