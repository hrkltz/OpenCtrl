#pragma once
#include <ControllerCalibration.hpp>
#include <M5StackInputs.hpp>
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <vector>
#include "Config.hpp"

// Unit types a PaHub port can hold. Index order matches the unit list sent to the page.
enum Unit : uint8_t { NoUnit, Joystick, Encoder, Scroll };

// Every port offers four sources; its unit decides which exist: a joystick has X, Y, and its click,
// an encoder or scroll unit has Turn and its click.
namespace Sources {
enum Kind : uint8_t { X, Y, Turn, Click };
constexpr size_t perPort = 4;
constexpr size_t axesPerPort = 3; // X, Y, and the turn position are calibrated.
inline size_t Port(uint8_t source) { return source / perPort; }
inline Kind KindOf(uint8_t source) { return Kind(source % perPort); }
inline size_t Axis(uint8_t source) { return Port(source) * axesPerPort + KindOf(source); } // Calibration axis index.
}

// One cycle's reading of a port, whatever its unit.
struct PortReading {
    M5Stack::JoystickRawState stick;
    int32_t steps = 0; // Turned since the last successful read.
    bool pressed = false;
};

enum class TargetKind : uint8_t { GamepadAxis, GamepadButton, MouseMotion, MouseButton, Key, Modifier };
struct Target {
    char id[20];
    char label[24];
    ControllerOutput output;
    TargetKind kind;
    uint8_t code; // Gamepad axis/button index, mouse axis/button bit, or HID key/modifier code.
};

inline const std::vector<Target>& Targets() {
    static const std::vector<Target> targets = [] {
        std::vector<Target> list;
        auto add = [&list](const char* id, const char* label, ControllerOutput output, TargetKind kind, uint8_t code) {
            Target target{{}, {}, output, kind, code};
            snprintf(target.id, sizeof(target.id), "%s", id);
            snprintf(target.label, sizeof(target.label), "%s", label);
            list.push_back(target);
        };
        // Order matches HIDUtil::SendGamepad(x, y, z, rz, rx, ry).
        const char* axes[][2] = {{"x", "X"}, {"y", "Y"}, {"z", "Z"}, {"rz", "Rz"}, {"rx", "Rx"}, {"ry", "Ry"}};
        char id[20], label[24];
        for (uint8_t i = 0; i < 6; ++i) {
            snprintf(id, sizeof(id), "gamepad.%s", axes[i][0]);
            snprintf(label, sizeof(label), "Gamepad %s", axes[i][1]);
            add(id, label, ControllerOutput::Axis, TargetKind::GamepadAxis, i);
        }
        for (uint8_t i = 0; i < 16; ++i) {
            snprintf(id, sizeof(id), "gamepad.button%u", i + 1);
            snprintf(label, sizeof(label), "Gamepad button %u", i + 1);
            add(id, label, ControllerOutput::Button, TargetKind::GamepadButton, i);
        }
        add("mouse.x", "Mouse X", ControllerOutput::Motion, TargetKind::MouseMotion, 0);
        add("mouse.y", "Mouse Y", ControllerOutput::Motion, TargetKind::MouseMotion, 1);
        add("mouse.wheel", "Mouse wheel", ControllerOutput::Motion, TargetKind::MouseMotion, 2);
        add("mouse.left", "Mouse left", ControllerOutput::Button, TargetKind::MouseButton, 1);
        add("mouse.right", "Mouse right", ControllerOutput::Button, TargetKind::MouseButton, 2);
        add("mouse.middle", "Mouse middle", ControllerOutput::Button, TargetKind::MouseButton, 4);
        for (char c = 'a'; c <= 'z'; ++c) {
            snprintf(id, sizeof(id), "key.%c", c);
            snprintf(label, sizeof(label), "Key %c", c - 'a' + 'A');
            add(id, label, ControllerOutput::Button, TargetKind::Key, uint8_t(0x04 + c - 'a'));
        }
        for (char c = '1'; c <= '9'; ++c) {
            snprintf(id, sizeof(id), "key.%c", c);
            snprintf(label, sizeof(label), "Key %c", c);
            add(id, label, ControllerOutput::Button, TargetKind::Key, uint8_t(0x1e + c - '1'));
        }
        add("key.0", "Key 0", ControllerOutput::Button, TargetKind::Key, 0x27);
        const struct { const char* id; const char* label; uint8_t code; } named[] = {
            {"key.enter", "Key Enter", 0x28}, {"key.escape", "Key Escape", 0x29},
            {"key.backspace", "Key Backspace", 0x2a}, {"key.tab", "Key Tab", 0x2b},
            {"key.space", "Key Space", 0x2c}, {"key.right", "Key Right", 0x4f}, {"key.left", "Key Left", 0x50},
            {"key.down", "Key Down", 0x51}, {"key.up", "Key Up", 0x52}};
        for (const auto& key : named) add(key.id, key.label, ControllerOutput::Button, TargetKind::Key, key.code);
        add("key.ctrl", "Key Ctrl", ControllerOutput::Button, TargetKind::Modifier, 0x01);
        add("key.shift", "Key Shift", ControllerOutput::Button, TargetKind::Modifier, 0x02);
        add("key.alt", "Key Alt", ControllerOutput::Button, TargetKind::Modifier, 0x04);
        return list;
    }();
    return targets;
}

// Rounds to 8-bit output with a quarter-count margin, so values near a rounding boundary do not chatter.
class AxisQuantizer {
    int8_t output = 0;
    bool initialized = false;
public:
    void Reset() { initialized = false; }
    int8_t Update(float value, int8_t neutral, bool stabilize) {
        value = std::isfinite(value) ? std::clamp(value, -127.0f, 127.0f) : neutral;
        if (!initialized || !stabilize || value == neutral || value == -127 || value == 127 ||
            std::abs(value - output) >= 0.75f) {
            output = int8_t(std::lround(value));
            initialized = true;
        }
        return output;
    }
};

// The HID state of one cycle. Flags tell which devices the bindings use.
struct ControllerReport {
    std::array<int8_t, 6> axes{0, 0, -127, -127, 0, 0}; // Gamepad profile neutral: triggers rest at -127.
    uint32_t buttons = 0;
    int dx = 0, dy = 0, wheel = 0;
    uint8_t mouseButtons = 0, modifiers = 0;
    std::array<uint8_t, 6> keys{};
    bool gamepad = false, mouse = false, keyboard = false;
};

// One reading per binding: a normalized axis or turn position in [-1, +1], the steps turned this
// cycle, or a button state.
struct BindingInput {
    float value = 0;
    int32_t steps = 0;
    bool pressed = false;
};

// Turns steps into short presses: one press per step, held and released for two cycles each.
class StepPulse {
    uint8_t pending = 0, timer = 0;
public:
    void Reset() { pending = timer = 0; }
    bool Update(int32_t steps) {
        pending = uint8_t(std::min<int32_t>(pending + std::max<int32_t>(steps, 0), 8)); // Drop a fast spin's excess.
        if (!timer && pending) { --pending; timer = 4; }
        const bool pressed = timer > 2;
        if (timer) --timer;
        return pressed;
    }
};

// Converts readings through the binding table into HID state. All-rest inputs give the neutral report.
class BindingMapper {
    std::array<AxisQuantizer, 6> quantizers;
    std::array<float, 3> motion{}; // Fractional mouse counts carried between cycles.
    std::array<StepPulse, Config::maxBindings> pulses;
public:
    void Reset() {
        for (auto& quantizer : quantizers) quantizer.Reset();
        for (auto& pulse : pulses) pulse.Reset();
        motion.fill(0);
    }
    ControllerReport Map(const std::vector<ControllerBinding>& bindings, const BindingInput* inputs, bool stabilize) {
        ControllerReport report;
        std::array<float, 6> value{}, rest{};
        std::array<bool, 6> bound{};
        std::array<float, 3> moved{};
        size_t keys = 0;
        for (size_t i = 0; i < bindings.size(); ++i) {
            const auto& binding = bindings[i];
            const auto& target = Targets()[binding.target];
            const float v = inputs[i].value;
            const auto kind = Sources::KindOf(binding.source);
            const bool half = binding.mode == ControllerBindingMode::Positive || binding.mode == ControllerBindingMode::Negative;
            const bool reversed = binding.mode == ControllerBindingMode::Reversed || binding.mode == ControllerBindingMode::Negative;
            const float travel = std::clamp(reversed ? -v : v, 0.0f, 1.0f);
            // Sticks press past half travel; a turn presses once per step in the chosen direction.
            const bool pressed = kind == Sources::Click ? inputs[i].pressed :
                kind == Sources::Turn ? half && pulses[i].Update(reversed ? -inputs[i].steps : inputs[i].steps) :
                half && travel > 0.5f;
            switch (target.kind) {
                case TargetKind::GamepadAxis: {
                    // Pedals rest at -1 and reach +1 at full travel; several bindings on one axis use the largest deflection.
                    const float output = half ? travel * 2 - 1 : reversed ? -v : v;
                    const float neutral = half ? -1 : 0;
                    auto& axis = target.code;
                    if (!bound[axis] || std::abs(output - neutral) > std::abs(value[axis] - rest[axis])) {
                        value[axis] = output; rest[axis] = neutral;
                    }
                    bound[axis] = true;
                    report.gamepad = true;
                    break;
                }
                case TargetKind::GamepadButton:
                    report.buttons |= uint32_t(pressed) << target.code;
                    report.gamepad = true;
                    break;
                case TargetKind::MouseMotion: {
                    // Sticks move at a speed set by deflection; a turn moves a fixed amount per step.
                    const float amount = kind == Sources::Turn ?
                        inputs[i].steps * (target.code < 2 ? Config::stepMouseCounts : Config::stepWheelDetents) :
                        v * (target.code < 2 ? Config::mouseSpeed : Config::wheelSpeed);
                    moved[target.code] += reversed ? -amount : amount;
                    report.mouse = true;
                    break;
                }
                case TargetKind::MouseButton:
                    if (pressed) report.mouseButtons |= target.code;
                    report.mouse = true;
                    break;
                case TargetKind::Key:
                    if (pressed && keys < report.keys.size() &&
                        std::find(report.keys.begin(), report.keys.begin() + keys, target.code) == report.keys.begin() + keys)
                        report.keys[keys++] = target.code;
                    report.keyboard = true;
                    break;
                case TargetKind::Modifier:
                    if (pressed) report.modifiers |= target.code;
                    report.keyboard = true;
                    break;
            }
        }
        for (size_t axis = 0; axis < 6; ++axis)
            if (bound[axis]) report.axes[axis] = quantizers[axis].Update(value[axis] * 127, int8_t(rest[axis] * 127), stabilize);
        int* counts[] = {&report.dx, &report.dy, &report.wheel};
        for (size_t i = 0; i < 3; ++i) {
            motion[i] += moved[i];
            *counts[i] = std::clamp(int(motion[i]), -127, 127); // Truncation keeps the remainder for later cycles.
            motion[i] -= *counts[i];
        }
        return report;
    }
};

inline ControllerMapping MakeMapping(std::vector<ControllerBinding>* bindings, std::vector<uint8_t>* ports) {
    static char sourceIds[Config::portCount * Sources::perPort][16];
    static char sourceLabels[Config::portCount * Sources::perPort][24];
    constexpr uint32_t joystick = 1u << Joystick, turning = 1u << Encoder | 1u << Scroll;
    const struct { const char* id; const char* label; ControllerSignal signal; uint32_t units; } inputs[] = {
        {"x", "X", ControllerSignal::Axis, joystick}, {"y", "Y", ControllerSignal::Axis, joystick},
        {"turn", "turn", ControllerSignal::Relative, turning}, {"button", "click", ControllerSignal::Button, joystick | turning}};
    ControllerMapping mapping;
    mapping.units = {{"none", "Nothing"}, {"joystick2", "Joystick2 U024-V2"}, {"encoder", "Encoder U135"}, {"scroll", "Scroll U186"}};
    for (size_t i = 0; i < Config::portCount * Sources::perPort; ++i) {
        const auto& input = inputs[i % Sources::perPort];
        const auto port = uint8_t(i / Sources::perPort);
        snprintf(sourceIds[i], sizeof(sourceIds[i]), "port%u.%s", unsigned(port), input.id);
        snprintf(sourceLabels[i], sizeof(sourceLabels[i]), "Port %u · %s", unsigned(port), input.label);
        mapping.sources.push_back({sourceIds[i], sourceLabels[i], input.signal, port, input.units});
    }
    for (const auto& target : Targets()) mapping.targets.push_back({target.id, target.label, target.output});
    mapping.bindings = bindings;
    mapping.ports = ports;
    mapping.maximum = Config::maxBindings;
    return mapping;
}
