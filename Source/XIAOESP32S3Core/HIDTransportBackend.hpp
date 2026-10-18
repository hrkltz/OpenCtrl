#pragma once

#include "HIDProfile.hpp"

struct HIDTransportConfiguration {
    std::vector<uint8_t> descriptor;
    std::vector<HIDReportLayout> reports;
    bool bootKeyboard = false;
    std::function<uint16_t(uint8_t, HIDReportType, uint8_t*, uint16_t)> getReport;
    std::function<void(uint8_t, HIDReportType, const uint8_t*, uint16_t)> setReport;
};

// Profiles compose with a transport; device definitions never inherit from it.
class HIDTransportBackend {
public:
    virtual ~HIDTransportBackend() = default;
    virtual bool Begin(HIDTransportConfiguration configuration) = 0;
    virtual bool Connected() const = 0;
    virtual bool Ready(uint8_t reportId = 0) const = 0;
    virtual bool Active(uint8_t reportId) const { return true; }
    virtual bool Send(uint8_t reportId, const uint8_t* data, size_t length) = 0;
    virtual void Service() {}
    virtual uint32_t Generation() const { return 0; }
    virtual uint32_t IdlePeriod() const { return 0; }
};

class USBCDC;
HIDTransportBackend& HIDUSBBackend();
HIDTransportBackend& HIDBLEBackend();
USBCDC& HIDUSBConsole();
