#pragma once
#include "Arduino.h"
namespace PreferencesMock {
    inline std::map<std::string,std::map<std::string,String>> storage;
    inline bool failWrites = false, failOpen = false;
    inline unsigned writes = 0;
}
class Preferences {
    std::string name;
public:
    bool begin(const char* value) { name=value; return !PreferencesMock::failOpen; }
    String getString(const char* key, const char* fallback) { auto& data=PreferencesMock::storage[name]; return data.count(key) ? data[key] : String(fallback); }
    size_t putString(const char* key, const String& value) {
        if (PreferencesMock::failWrites) return 0;
        ++PreferencesMock::writes; PreferencesMock::storage[name][key] = value; return value.length();
    }
};
