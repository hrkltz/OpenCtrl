#pragma once

#include <cstdint>

namespace M5Stack {
struct AxisCalibration {
    int32_t minimum = 0, center = 32768, maximum = 65535;
    float deadzone = 0.05f;
    bool inverted = false;
    bool Valid() const;
    float Normalize(int32_t value) const;
};

int8_t SignedAxis(float value);
int8_t PositiveTrigger(float value); // Rest -127; full positive travel +127.

struct ButtonState {
    bool pressed = false, pressedEdge = false, releasedEdge = false;
};

class ButtonDebouncer {
    uint32_t interval, changedAt = 0;
    bool candidate = false, stable = false;
public:
    explicit ButtonDebouncer(uint32_t milliseconds = 10) : interval(milliseconds) {}
    void Reset(bool pressed, uint32_t now);
    ButtonState Update(bool pressed, uint32_t now);
};

// Differences modulo the device counter width; first sample establishes a baseline.
class CounterTracker {
    uint32_t previous = 0;
    bool initialized = false;
public:
    void Reset() { initialized = false; }
    int32_t Update(uint32_t value, uint8_t bits);
};

struct RGBColor { uint8_t red = 0, green = 0, blue = 0; };
enum class ButtonPolarity { ActiveLow, ActiveHigh };
}
