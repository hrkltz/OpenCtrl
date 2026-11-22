#pragma once
#include <cmath>

enum class ControllerFilterMode { Off, Median3, EMA };

struct ControllerFilterSettings {
    ControllerFilterMode mode = ControllerFilterMode::Median3;
    float alpha = 0.35f; // EMA weight of the newest reading.
    bool Valid() const {
        return (mode == ControllerFilterMode::Off || mode == ControllerFilterMode::Median3 ||
            mode == ControllerFilterMode::EMA) && std::isfinite(alpha) && alpha >= 0.05f && alpha <= 1;
    }
};
