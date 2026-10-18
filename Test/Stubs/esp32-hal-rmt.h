#pragma once
#include "Arduino.h"
#include <cassert>
#include <set>
union rmt_data_t {
    struct { uint32_t duration0 : 15, level0 : 1, duration1 : 15, level1 : 1; };
    uint32_t val;
};
constexpr int RMT_TX_MODE = 1, RMT_MEM_NUM_BLOCKS_1 = 1;
constexpr uint32_t RMT_WAIT_FOR_EVER = UINT32_MAX;
inline std::set<int> mockRMTActive;
inline bool mockRMTFailInit = false, mockRMTFailWrite = false, mockRMTFailEOT = false;
inline std::map<int, std::vector<rmt_data_t>> mockRMTFrames;
inline bool rmtInit(int pin, int, int, uint32_t frequency) {
    assert(frequency == 10000000);
    if (mockRMTFailInit) return false;
    mockRMTActive.insert(pin); return true;
}
inline bool rmtDeinit(int pin) { mockRMTActive.erase(pin); return true; }
inline bool rmtSetEOT(int, uint8_t level) { assert(level == LOW); return !mockRMTFailEOT; }
inline bool rmtWrite(int pin, rmt_data_t* data, size_t count, uint32_t timeout) {
    assert(timeout == RMT_WAIT_FOR_EVER && mockRMTActive.count(pin));
    if (mockRMTFailWrite) return false;
    mockRMTFrames[pin] = {data, data + count}; return true;
}
