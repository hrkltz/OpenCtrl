#include "HIDUtil.hpp"
#include "HIDTransportBackend.hpp"
#include <Arduino.h>
#include <freertos/semphr.h>
#include <algorithm>
#include <array>

namespace {
constexpr uint8_t bits(HIDTransport transport) { return static_cast<uint8_t>(transport); }
bool valid(HIDTransport transport) { return bits(transport) && !(bits(transport) & ~bits(HIDTransport::Both)); }
struct DeliveryState {
    std::vector<uint8_t> current;
    bool releasePending = true;
    uint32_t lastSent = 0;
};
struct ReportState {
    HIDReportLayout layout;
    std::vector<uint8_t> neutral;
    HIDProfileCallbacks callbacks;
    HIDTransport transports;
    std::array<DeliveryState, 2> delivery;
};
struct Binding {
    HIDTransport type;
    HIDTransportBackend* driver;
    std::vector<uint8_t> descriptor;
    std::vector<HIDReportLayout> layouts;
    bool connected = false;
    uint32_t generation = 0;
};
struct Registry {
    std::vector<ReportState> reports;
    SemaphoreHandle_t mutex = nullptr;
    bool started = false;
    std::array<Binding, 2> bindings{{{HIDTransport::USB, &HIDUSBBackend()}, {HIDTransport::Bluetooth, &HIDBLEBackend()}}};
    ReportState* find(uint8_t id) {
        for (auto& report : reports) if (report.layout.id == id) return &report;
        return nullptr;
    }
};
Registry& registry() { static Registry instance; return instance; }
portMUX_TYPE reportMutex = portMUX_INITIALIZER_UNLOCKED;
class Lock {
    SemaphoreHandle_t mutex;
public:
    Lock() : mutex(registry().mutex) { if (mutex) xSemaphoreTakeRecursive(mutex, portMAX_DELAY); }
    ~Lock() { if (mutex) xSemaphoreGiveRecursive(mutex); }
};
bool uses(const ReportState& report, size_t index) {
    return bits(report.transports) & bits(registry().bindings[index].type);
}
void store(ReportState& report, size_t index, const uint8_t* data) {
    auto& current = report.delivery[index].current;
    portENTER_CRITICAL(&reportMutex);
    memcpy(current.data(), data, current.size());
    for (size_t i = 0; i < report.layout.relativeMask.size(); ++i) current[i] &= ~report.layout.relativeMask[i];
    portEXIT_CRITICAL(&reportMutex);
}
uint16_t getReport(size_t index, uint8_t id, HIDReportType type, uint8_t* data, uint16_t length) {
    auto* report = registry().find(id);
    if (!report || !uses(*report, index) || !data || !length) return 0;
    if (type == HIDReportType::Input && report->layout.input) {
        const uint16_t limit = std::min<size_t>(length, report->layout.input);
        portENTER_CRITICAL(&reportMutex);
        memcpy(data, report->delivery[index].current.data(), limit);
        portEXIT_CRITICAL(&reportMutex);
        return limit;
    }
    if (type == HIDReportType::Feature && report->layout.feature && report->callbacks.getFeature) {
        const uint16_t limit = std::min<size_t>(length, report->layout.feature);
        return std::min(limit, report->callbacks.getFeature(id, data, limit));
    }
    return 0;
}
void setReport(size_t index, uint8_t id, HIDReportType type, const uint8_t* data, uint16_t length) {
    auto* report = registry().find(id);
    if (!report || !uses(*report, index) || !data) return;
    if (type == HIDReportType::Output && report->layout.output && length == report->layout.output && report->callbacks.output)
        report->callbacks.output(id, data, length);
    else if (type == HIDReportType::Feature && report->layout.feature && length == report->layout.feature && report->callbacks.setFeature)
        report->callbacks.setFeature(id, data, length);
}
}

bool HIDUtil::RegisterProfile(const HIDProfile& definition, HIDTransport transports, uint8_t firstReportId) {
    auto& state = registry();
    if (state.started || !valid(transports)) return false;
    HIDProfile profile = definition;
    if (firstReportId && !profile.RemapReportIds(firstReportId)) return false;
    const auto& descriptor = profile.descriptor;
    if (descriptor.empty() || descriptor.size() > 4094) return false;
    for (const auto& binding : state.bindings) {
        if ((bits(transports) & bits(binding.type)) && binding.descriptor.size() + descriptor.size() + 2 >
            (binding.type == HIDTransport::Bluetooth ? 512 : 4096)) return false;
    }
    std::vector<HIDReportLayout> layouts;
    if (!ParseHIDDescriptor(descriptor.data(), descriptor.size(), layouts)) return false;
    size_t inputs = 0;
    for (const auto& layout : layouts) {
        if (state.find(layout.id)) return false;
        if (layout.input) {
            ++inputs;
            size_t matches = 0;
            for (const auto& neutral : profile.neutral) if (neutral.id == layout.id) {
                if (neutral.data.size() != layout.input) return false;
                for (size_t i = 0; i < layout.relativeMask.size(); ++i)
                    if (neutral.data[i] & layout.relativeMask[i]) return false;
                ++matches;
            }
            if (matches != 1) return false;
        }
    }
    if (inputs != profile.neutral.size()) return false;
    for (const auto& layout : layouts) {
        std::vector<uint8_t> neutral;
        for (const auto& value : profile.neutral) if (value.id == layout.id) neutral = value.data;
        state.reports.push_back({layout, neutral, profile.callbacks, transports,
            {{{neutral, layout.input != 0, 0}, {neutral, layout.input != 0, 0}}}});
    }
    for (auto& binding : state.bindings) if (bits(transports) & bits(binding.type)) {
        binding.descriptor.push_back(0xa4); // Isolate the profile's HID globals.
        binding.descriptor.insert(binding.descriptor.end(), descriptor.begin(), descriptor.end());
        binding.descriptor.push_back(0xb4);
        binding.layouts.insert(binding.layouts.end(), layouts.begin(), layouts.end());
    }
    return true;
}
bool HIDUtil::RegisterProfile(const uint8_t* descriptor, size_t length,
                              const std::vector<HIDNeutralReport>& neutral,
                              HIDProfileCallbacks callbacks, HIDTransport transport) {
    if (!descriptor || !length || length > 4094) return false;
    return RegisterProfile({{descriptor, descriptor + length}, neutral, callbacks}, transport);
}
uint8_t HIDUtil::NextReportId() {
    for (unsigned id = 7; id <= 255; ++id) if (!registry().find(id)) return id;
    return 0;
}
bool HIDUtil::Init(uint8_t devices, HIDProfileCallbacks callbacks, HIDTransport standardTransport) {
    auto& state = registry();
    if (state.started || (devices & ~All) || !valid(standardTransport)) return false;
    const std::array<HIDProfile (*)(), 5> factories{{HIDProfiles::Keyboard, HIDProfiles::Mouse,
        HIDProfiles::Gamepad, HIDProfiles::Consumer, HIDProfiles::System}};
    for (size_t i = 0; i < factories.size(); ++i) if (devices & (1 << i)) {
        auto profile = factories[i]();
        profile.callbacks = callbacks;
        if (!RegisterProfile(profile, standardTransport, i + 1)) return false;
    }
    if (state.reports.empty()) return false;
    state.mutex = xSemaphoreCreateRecursiveMutex();
    if (!state.mutex) return false;
    state.started = true; // Descriptors and callbacks are immutable from here on.
    for (size_t i = 0; i < state.bindings.size(); ++i) {
        auto& binding = state.bindings[i];
        HIDTransportConfiguration config;
        config.descriptor = binding.descriptor;
        config.reports = binding.layouts;
        config.bootKeyboard = i == 0 && (devices & Keyboard) && (bits(standardTransport) & bits(HIDTransport::USB));
        config.getReport = [i](uint8_t id, HIDReportType type, uint8_t* data, uint16_t length) {
            return getReport(i, id, type, data, length);
        };
        config.setReport = [i](uint8_t id, HIDReportType type, const uint8_t* data, uint16_t length) {
            setReport(i, id, type, data, length);
        };
        if (!binding.driver->Begin(std::move(config))) return false;
    }
    return true;
}
bool HIDUtil::Connected(HIDTransport transports) {
    if (!registry().started || !valid(transports)) return false;
    for (const auto& binding : registry().bindings)
        if ((bits(transports) & bits(binding.type)) && binding.driver->Connected()) return true;
    return false;
}
bool HIDUtil::Ready(HIDTransport transports) {
    if (!registry().started || !valid(transports)) return false;
    for (const auto& binding : registry().bindings)
        if ((bits(transports) & bits(binding.type)) && binding.driver->Ready()) return true;
    return false;
}
bool HIDUtil::ReadyForReport(uint8_t id) {
    auto* report = registry().find(id);
    if (!registry().started || !report) return false;
    for (size_t i = 0; i < registry().bindings.size(); ++i)
        if (uses(*report, i) && registry().bindings[i].driver->Ready(id)) return true;
    return false;
}
size_t HIDUtil::InputSize(uint8_t id) { auto* report = registry().find(id); return report ? report->layout.input : 0; }
HIDTransport HIDUtil::Transports(uint8_t id) {
    auto* report = registry().find(id);
    return report ? report->transports : static_cast<HIDTransport>(0);
}
bool HIDUtil::SendInput(uint8_t id, const uint8_t* data, size_t length) {
    return SendInputTo(id, data, length, Transports(id));
}
bool HIDUtil::SendInputTo(uint8_t id, const uint8_t* data, size_t length, HIDTransport targets) {
    Lock lock;
    auto* report = registry().find(id);
    if (!report || !registry().started || !data || !length || length != report->layout.input ||
        !valid(targets) || (bits(targets) & ~bits(report->transports))) return false;
    Service();
    bool delivered = false, failed = false;
    for (size_t i = 0; i < registry().bindings.size(); ++i) {
        auto& binding = registry().bindings[i];
        auto& driver = *binding.driver;
        if (!(bits(targets) & bits(binding.type)) || !driver.Connected() || !driver.Active(id)) continue;
        bool pending = false;
        for (const auto& candidate : registry().reports)
            if (uses(candidate, i) && candidate.delivery[i].releasePending && driver.Ready(candidate.layout.id)) pending = true;
        if (pending || !driver.Ready(id) || !driver.Send(id, data, length)) {
            report->delivery[i].releasePending = true;
            failed = true;
            continue;
        }
        store(*report, i, data);
        report->delivery[i].lastSent = millis();
        delivered = true;
    }
    return delivered && !failed;
}
bool HIDUtil::ReleaseAll() {
    Lock lock;
    for (auto& report : registry().reports) for (size_t i = 0; i < report.delivery.size(); ++i)
        if (uses(report, i) && report.layout.input) report.delivery[i].releasePending = true;
    Service();
    for (const auto& report : registry().reports) for (size_t i = 0; i < report.delivery.size(); ++i)
        if (uses(report, i) && report.delivery[i].releasePending && registry().bindings[i].driver->Ready(report.layout.id)) return false;
    return Ready();
}
void HIDUtil::Service() {
    Lock lock;
    if (!registry().started) return;
    for (size_t i = 0; i < registry().bindings.size(); ++i) {
        auto& binding = registry().bindings[i];
        auto& driver = *binding.driver;
        driver.Service();
        const bool connected = driver.Connected();
        const uint32_t generation = driver.Generation();
        if (!connected || connected != binding.connected || generation != binding.generation) {
            for (auto& report : registry().reports) if (uses(report, i) && report.layout.input) {
                report.delivery[i].releasePending = true;
                store(report, i, report.neutral.data());
            }
        }
        binding.connected = connected;
        binding.generation = generation;
        if (!connected) continue;
        for (auto& report : registry().reports) {
            if (!uses(report, i) || !report.layout.input || !driver.Ready(report.layout.id)) continue;
            auto& delivery = report.delivery[i];
            if (delivery.releasePending) {
                if (!driver.Send(report.layout.id, report.neutral.data(), report.neutral.size())) break;
                store(report, i, report.neutral.data());
                delivery.releasePending = false;
                delivery.lastSent = millis();
            }
            const uint32_t period = driver.IdlePeriod();
            if (period && uint32_t(millis() - delivery.lastSent) >= period) {
                if (!driver.Send(report.layout.id, delivery.current.data(), delivery.current.size())) {
                    delivery.releasePending = true;
                    break;
                }
                delivery.lastSent = millis();
            }
        }
    }
}
bool HIDUtil::SendKeyboard(uint8_t modifiers, const uint8_t keys[6], uint8_t id) {
    if (!keys) return false;
    uint8_t report[8] = {modifiers, 0};
    memcpy(report + 2, keys, 6);
    return SendInput(id, report, sizeof(report));
}
bool HIDUtil::SendMouse(int dx, int dy, int wheel, int pan, uint8_t buttons, uint8_t id) {
    if (buttons > 31) return false;
    Lock lock;
    do {
        const int8_t x = std::clamp(dx, -127, 127), y = std::clamp(dy, -127, 127);
        const int8_t w = std::clamp(wheel, -127, 127), p = std::clamp(pan, -127, 127);
        const uint8_t report[] = {buttons, uint8_t(x), uint8_t(y), uint8_t(w), uint8_t(p)};
        if (!SendInput(id, report, sizeof(report))) return false;
        dx -= x; dy -= y; wheel -= w; pan -= p;
    } while (dx || dy || wheel || pan);
    return true;
}
bool HIDUtil::SendGamepad(int8_t x, int8_t y, int8_t z, int8_t rz,
                         int8_t rx, int8_t ry, uint8_t hat, uint32_t buttons, uint8_t id) {
    if (x == -128 || y == -128 || z == -128 || rz == -128 || rx == -128 || ry == -128 || hat > 8) return false;
    const uint8_t report[] = {uint8_t(x), uint8_t(y), uint8_t(z), uint8_t(rz), uint8_t(rx), uint8_t(ry), hat,
        uint8_t(buttons), uint8_t(buttons >> 8), uint8_t(buttons >> 16), uint8_t(buttons >> 24)};
    return SendInput(id, report, sizeof(report));
}
bool HIDUtil::SendConsumer(uint16_t usage, uint8_t id) {
    if (usage > 0x3ff) return false;
    const uint8_t report[] = {uint8_t(usage), uint8_t(usage >> 8)};
    return SendInput(id, report, sizeof(report));
}
bool HIDUtil::SendSystem(uint8_t usage, uint8_t id) { return usage <= 3 && SendInput(id, &usage, 1); }
