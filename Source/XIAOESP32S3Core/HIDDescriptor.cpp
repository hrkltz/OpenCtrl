#include "HIDDescriptor.hpp"
#include <array>

bool ParseHIDDescriptor(const uint8_t* data, size_t length,
                        std::vector<HIDReportLayout>& reports, size_t maxPayload) {
    struct Globals { uint32_t size = 0, count = 0, id = 0; } current;
    std::vector<Globals> stack;
    std::vector<std::array<size_t, 3>> bits(256);
    std::vector<std::vector<uint8_t>> relativeMasks(256);
    reports.clear();
    if (!data || !length || length > 4096 || !maxPayload) return false;
    size_t collections = 0;
    for (size_t offset = 0; offset < length;) {
        const uint8_t prefix = data[offset++];
        if (prefix == 0xfe) return false; // Reserved long items have no defined HID semantics.
        const size_t size = (prefix & 3) == 3 ? 4 : prefix & 3;
        if (offset + size > length) return false;
        uint32_t value = 0;
        for (size_t i = 0; i < size; ++i) value |= uint32_t(data[offset++]) << (8 * i);
        const uint8_t type = (prefix >> 2) & 3, tag = prefix >> 4;
        if (type == 1) {
            switch (tag) {
                case 7: current.size = value; break;
                case 8:
                    if (!value || value > 255) return false;
                    current.id = value;
                    break;
                case 9: current.count = value; break;
                case 10:
                    if (size || stack.size() >= 16) return false;
                    stack.push_back(current);
                    break;
                case 11:
                    if (size || stack.empty()) return false;
                    current = stack.back(); stack.pop_back();
                    break;
            }
        } else if (type == 0) {
            if (tag == 10) {
                if (!size || ++collections > 32) return false;
            } else if (tag == 12) {
                if (size || !collections) return false;
                --collections;
            } else if (tag == 8 || tag == 9 || tag == 11) {
                if (!size || !collections || !current.id || !current.size || !current.count) return false;
                const uint64_t count = uint64_t(current.size) * current.count;
                auto& total = bits[current.id][tag == 8 ? 0 : tag == 9 ? 1 : 2];
                if (count > maxPayload * 8 || total + count > maxPayload * 8) return false;
                if (tag == 8 && (value & 4) && !(value & 1)) {
                    auto& mask = relativeMasks[current.id];
                    mask.resize((total + count + 7) / 8);
                    for (size_t bit = total; bit < total + count; ++bit) mask[bit / 8] |= 1 << (bit % 8);
                }
                total += count;
            }
        }
    }
    if (collections || !stack.empty()) return false;
    for (size_t id = 1; id < bits.size(); ++id) {
        const auto& b = bits[id];
        if (b[0] || b[1] || b[2]) {
            reports.push_back({static_cast<uint8_t>(id), (b[0] + 7) / 8, (b[1] + 7) / 8, (b[2] + 7) / 8, relativeMasks[id]});
        }
    }
    return !reports.empty();
}
