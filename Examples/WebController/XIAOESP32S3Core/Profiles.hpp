#pragma once
#include "Config.hpp"
#include "HIDPen.hpp"

namespace WebController {
inline bool InitProfiles(HIDPen& pen) {
    if (!pen.begin(WebConfig::penTransport)) return false;
    uint8_t standard = HIDUtil::All;
    if (WebConfig::gamepadTransport != WebConfig::primaryTransport) {
        if (!HIDUtil::RegisterProfile(HIDProfiles::Gamepad(), WebConfig::gamepadTransport, 3)) return false;
        standard &= ~HIDUtil::Gamepad;
    }
    if (WebConfig::secondaryMouse &&
        !HIDUtil::RegisterProfile(HIDProfiles::Mouse(), WebConfig::secondaryMouseTransport, WebConfig::secondaryMouseId)) return false;
    // Keep the standard keyboard registration, including USB boot protocol.
    return HIDUtil::Init(standard, {}, WebConfig::primaryTransport);
}
}
