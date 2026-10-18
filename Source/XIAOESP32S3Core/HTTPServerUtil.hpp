#pragma once

#include <ArduinoJson.h>
#include <ESPAsyncWebServer.h>
#include <functional>

class HTTPServerUtil {
public:
    static AsyncWebServer& GetServer();
    // All input routes share authentication, session ownership, and error handling.
    // Handler returns an HTTP status: 200, 400 (invalid input), or 503 (USB failure).
    static void AddJsonRoute(const char* uri, std::function<int(JsonObject)> handler);
    // Reuse standard mouse validation/session handling for another mouse instance.
    static void AddMouseRoute(const char* uri, uint8_t reportId = 2);
    static bool ReadInt(JsonObject doc, const char* key, int minimum, int maximum,
                        int& value, int fallback = 0);
    static bool ReadBool(JsonObject doc, const char* key, bool& value);
    static bool ReadButtons(JsonObject doc, uint32_t& value);
    static const char* GetPairingToken();
    static bool Init();
};
