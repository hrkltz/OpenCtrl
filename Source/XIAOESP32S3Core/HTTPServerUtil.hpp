/**
 * @file HTTPServerUtil.hpp
 * @author Oliver Herklotz (oliver-hrkltz)
 * @date 17.01.2026
 */
#pragma once

/**
 * @class HTTPServerUtil
 * @brief Utility class for initializing and managing the HTTP server on XIAO ESP32S3.
 */
class HTTPServerUtil {
public:
    /**
     * @brief Initialize the HTTP server and configure routes.
     *
     * This function must be called to start the web server functionality.
     */
    static void Init();
};
