#include "InputUtils.hpp"
#include <algorithm>
#include <cmath>

namespace M5Stack {
bool AxisCalibration::Valid() const {
    return minimum < center && center < maximum && std::isfinite(deadzone) && deadzone >= 0 && deadzone < 1;
}
float AxisCalibration::Normalize(int32_t value) const {
    if (!Valid()) return 0;
    const double delta = double(value) - center;
    float normalized = std::clamp(delta / (delta < 0 ? double(center) - minimum : double(maximum) - center), -1.0, 1.0);
    const float magnitude = std::abs(normalized);
    normalized = magnitude <= deadzone ? 0 : std::copysign((magnitude - deadzone) / (1 - deadzone), normalized);
    return inverted ? -normalized : normalized;
}
int8_t SignedAxis(float value) {
    return std::isfinite(value) ? int8_t(std::lround(std::clamp(value, -1.0f, 1.0f) * 127)) : 0;
}
int8_t PositiveTrigger(float value) {
    return std::isfinite(value) ? int8_t(std::lround(std::clamp(value, 0.0f, 1.0f) * 254 - 127)) : -127;
}

void ButtonDebouncer::Reset(bool pressed, uint32_t now) {
    candidate = stable = pressed;
    changedAt = now;
}
ButtonState ButtonDebouncer::Update(bool pressed, uint32_t now) {
    if (pressed != candidate) { candidate = pressed; changedAt = now; }
    ButtonState state{stable, false, false};
    if (candidate != stable && uint32_t(now - changedAt) >= interval) {
        stable = candidate;
        state = {stable, stable, !stable};
    }
    return state;
}

int32_t CounterTracker::Update(uint32_t value, uint8_t bits) {
    if (bits != 16 && bits != 32) { Reset(); return 0; }
    const uint64_t modulus = uint64_t(1) << bits;
    const uint32_t mask = uint32_t(modulus - 1);
    value &= mask;
    const uint32_t difference = (value - previous) & mask;
    const int64_t delta = difference < modulus / 2 ? int64_t(difference) : int64_t(difference) - int64_t(modulus);
    const bool ready = initialized;
    previous = value;
    initialized = true;
    return ready ? int32_t(delta) : 0;
}
}

