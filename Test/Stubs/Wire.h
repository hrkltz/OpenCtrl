#pragma once
#include "Arduino.h"
#include <set>
#include <deque>

class TwoWire {
    uint8_t address = 0, reg = 0;
    std::vector<uint8_t> transmission;
    std::deque<uint8_t> received;
public:
    int channel = -1;
    int beginSDA = -1, beginSCL = -1;
    bool missingHub = false, shortRead = false;
    unsigned transmissionFailures = 0;
    std::set<int> missingChannels;
    using Registers = std::map<uint8_t, std::vector<uint8_t>>;
    std::map<std::pair<int,uint8_t>, Registers> devices;
    std::vector<uint8_t> selections;
    bool begin(int sda = SDA, int scl = SCL) { beginSDA = sda; beginSCL = scl; return true; }
    void setClock(uint32_t) {}
    void setTimeOut(uint16_t) {}
    void beginTransmission(uint8_t value) { address = value; transmission.clear(); }
    size_t write(uint8_t value) { transmission.push_back(value); return 1; }
    size_t write(const uint8_t* data, size_t length) {
        transmission.insert(transmission.end(), data, data + length); return length;
    }
    uint8_t endTransmission(bool = true) {
        if (transmissionFailures) { --transmissionFailures; return 4; }
        if (address == 0x70) {
            if (missingHub || transmission.size() != 1) return 2;
            const uint8_t mask = transmission[0];
            if (!mask || (mask & (mask - 1))) return 3;
            selections.push_back(mask);
            for (int i = 0; i < 8; ++i) if (mask == (1 << i)) channel = i;
            return 0;
        }
        if (missingChannels.count(channel) || !devices.count({channel,address})) return 2;
        if (transmission.empty()) return 0;
        reg = transmission[0];
        if (transmission.size() > 1) devices[{channel,address}][reg] = {transmission.begin() + 1, transmission.end()};
        return 0;
    }
    size_t requestFrom(uint8_t value, uint8_t length) {
        received.clear();
        if (missingChannels.count(channel) || !devices.count({channel,value})) return 0;
        const auto& bytes = devices[{channel,value}][reg];
        const size_t count = std::min<size_t>(bytes.size(), shortRead ? length - 1 : length);
        shortRead = false;
        received.insert(received.end(), bytes.begin(), bytes.begin() + count);
        return count;
    }
    int available() { return received.size(); }
    int read() { if (received.empty()) return -1; const auto value = received.front(); received.pop_front(); return value; }
    void SetJoystick(int port, uint16_t x, uint16_t y, bool pressed = false) {
        auto& registers = devices[{port,0x63}];
        registers[0] = {uint8_t(x), uint8_t(x >> 8), uint8_t(y), uint8_t(y >> 8)};
        registers[0x20] = {uint8_t(!pressed)};
    }
};
inline TwoWire Wire;
