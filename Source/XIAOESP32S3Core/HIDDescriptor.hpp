#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

struct HIDReportLayout {
    uint8_t id;
    size_t input = 0;
    size_t output = 0;
    size_t feature = 0;
    std::vector<uint8_t> relativeMask;
};

// Computes payload sizes (excluding report ID) and rejects malformed layouts.
bool ParseHIDDescriptor(const uint8_t* data, size_t length,
                        std::vector<HIDReportLayout>& reports, size_t maxPayload = 63);
