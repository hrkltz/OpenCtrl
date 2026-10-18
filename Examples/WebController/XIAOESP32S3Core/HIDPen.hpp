/**
 * @file HIDPen.hpp
 * @author Oliver Herklotz (oliver-hrkltz)
 * @date 15.10.2026
 */
#pragma once

#include <HIDUtil.hpp>

/**
 * @class HIDPen
 * @brief HID digitizer pen with absolute position and pressure.
 * @details Call begin() before HIDUtil::Init() to register the descriptor and neutral state.
 * Supported by Windows, Linux, ChromeOS, and Android. macOS has no generic HID digitizer support.
 */
class HIDPen {
  private:
    uint8_t reportId = 0;
  public:
    bool begin(HIDTransport transport = HIDTransport::USB);

    /**
     * @brief Send a pen report.
     *
     * @param tip Pen touches the surface.
     * @param inRange Pen hovers over or touches the surface.
     * @param barrel Barrel (side) button pressed.
     * @param eraser Eraser end in use.
     * @param x X position (0 to 32767, left to right).
     * @param y Y position (0 to 32767, top to bottom).
     * @param pressure Tip pressure (0 to 1023).
     */
    bool send(bool tip, bool inRange, bool barrel, bool eraser, int x, int y, int pressure);

};
