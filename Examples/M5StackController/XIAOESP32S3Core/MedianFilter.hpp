#pragma once

#include <algorithm>
#include <array>
#include <cstdint>

class MedianFilter3 {
    std::array<uint16_t, 3> samples{};
    uint8_t next = 0;
    bool initialized = false;
public:
    void Reset() { initialized = false; next = 0; }
    uint16_t Update(uint16_t value) {
        if (!initialized) {
            samples.fill(value);
            initialized = true;
        }
        samples[next] = value;
        next = (next + 1) % samples.size();
        auto sorted = samples;
        std::sort(sorted.begin(), sorted.end());
        return sorted[1];
    }
};
