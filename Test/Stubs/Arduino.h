#pragma once
#include <cstdint>
#include <cstring>
#include <string>
#include <mutex>
#include <map>
#include <vector>
#include <algorithm>

class String : public std::string {
public:
    using std::string::string;
    using std::string::operator=;
    String(std::string value) : std::string(std::move(value)) {}
    bool startsWith(const char* prefix) const { return rfind(prefix, 0) == 0; }
    size_t write(uint8_t value) { push_back(char(value)); return 1; }
    size_t write(const uint8_t* value, size_t length) { append(reinterpret_cast<const char*>(value), length); return length; }
};
inline uint32_t mockMillis = 0;
inline uint32_t millis() { return mockMillis; }
inline void delay(uint32_t value) { mockMillis += value; }
constexpr int SDA = 5, SCL = 6;
struct MockConsole {
    void begin(unsigned long) {}
    template<class T> void print(T) {}
    template<class T> void println(T) {}
};
inline MockConsole Serial;
constexpr int LOW = 0, HIGH = 1, INPUT = 1, INPUT_PULLUP = 5;
constexpr int ADC_11db = 3;
inline std::map<int, int> mockPinModes, mockDigital, mockAttenuation;
inline std::map<int, uint32_t> mockMillivolts;
inline bool digitalPinIsValid(int pin) { return pin >= 0 && pin <= 48 && pin != 22; }
inline bool digitalPinCanOutput(int pin) { return digitalPinIsValid(pin) && pin != 46; }
inline int8_t digitalPinToAnalogChannel(int pin) { return pin >= 1 && pin <= 20 ? int8_t(pin - 1) : -1; }
inline void pinMode(int pin, int mode) { mockPinModes[pin] = mode; }
inline int digitalRead(int pin) { return mockDigital.count(pin) ? mockDigital[pin] : HIGH; }
inline void analogSetPinAttenuation(int pin, int value) { mockAttenuation[pin] = value; }
inline uint32_t analogReadMilliVolts(int pin) { return mockMillivolts[pin]; }
using portMUX_TYPE = std::mutex;
#define portMUX_INITIALIZER_UNLOCKED {}
#define portENTER_CRITICAL(mutex) (mutex)->lock()
#define portEXIT_CRITICAL(mutex) (mutex)->unlock()
