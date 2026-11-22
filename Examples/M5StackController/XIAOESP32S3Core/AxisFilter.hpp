#pragma once
#include <FilterSettings.hpp>
#include "MedianFilter.hpp"

class AxisFilter {
    MedianFilter3 median;
    ControllerFilterSettings previous;
    float average = 0;
    bool initialized = false;
public:
    void Reset() { initialized = false; median.Reset(); }
    uint16_t Update(uint16_t value, const ControllerFilterSettings& settings) {
        if (!initialized || previous.mode != settings.mode || previous.alpha != settings.alpha) {
            Reset();
            previous = settings;
            average = value;
            initialized = true;
        } else if (settings.mode == ControllerFilterMode::EMA) average += settings.alpha * (float(value) - average);
        switch (settings.mode) {
            case ControllerFilterMode::Off: return value;
            case ControllerFilterMode::EMA: return uint16_t(std::lround(average));
            default: return median.Update(value);
        }
    }
};
