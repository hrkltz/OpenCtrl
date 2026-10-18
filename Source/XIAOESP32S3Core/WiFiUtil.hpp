/**
 * @file WiFiUtil.hpp
 * @author Oliver Herklotz (oliver-hrkltz)
 * @date 15.10.2026
 */
#pragma once

/**
 * @class WiFiUtil
 * @brief Utility class for connecting the XIAO ESP32S3 to WiFi.
 * @details Uses the WIFI_SSID and WIFI_PASSWORD build flags set by Build.sh.
 */
class WiFiUtil {
  public:
    /**
     * @brief Connect to WiFi and block until connected.
     *
     * Shows the connection status on display line 0 and the IP address on line 1.
     * DisplayUtil::Init() must be called first.
     */
    static void Connect();
};
