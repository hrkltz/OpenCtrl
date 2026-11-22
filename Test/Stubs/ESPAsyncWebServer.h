#pragma once
#include "Arduino.h"
#include <ArduinoJson.h>
#include <functional>
#define HTTP_GET 1
#define HTTP_POST 2
#define HTTP_OPTIONS 3
class AsyncWebServerResponse {
public:
    int status;
    std::map<std::string,std::string> headers;
    String body;
    explicit AsyncWebServerResponse(int code) : status(code) {}
    void addHeader(const char* key, const char* value) { headers[key]=value; }
};
class AsyncWebServerRequest {
public:
    std::map<std::string,std::string> headers;
    int status = 0;
    String header(const char* key) { return headers[key]; }
    String body;
    AsyncWebServerResponse* beginResponse(int status, const char*, const String& content) { auto* response = new AsyncWebServerResponse(status); response->body = content; return response; }
    AsyncWebServerResponse* beginResponse(int status, const char* type, const uint8_t* data, size_t length) {
        return beginResponse(status, type, String(reinterpret_cast<const char*>(data), length));
    }
    void send(AsyncWebServerResponse* response) { status=response->status; body=response->body; delete response; }
    void send(int code, const char*, const String& content) { status=code; body=content; }
};
// Like ESPAsyncWebServer: a plain URI also matches every path below it; exact() does not.
struct AsyncURIMatcher {
    std::string uri;
    bool exactOnly = false;
    AsyncURIMatcher(const char* value) : uri(value) {}
    static AsyncURIMatcher exact(const char* value) { AsyncURIMatcher matcher(value); matcher.exactOnly = true; return matcher; }
    bool Matches(const std::string& path) const { return path == uri || (!exactOnly && path.rfind(uri + "/", 0) == 0); }
};
class AsyncWebServer {
public:
    using JsonHandler = std::function<void(AsyncWebServerRequest*, JsonVariant&)>;
    inline static std::map<std::string,JsonHandler> routes;
    inline static std::map<std::pair<std::string,int>,std::function<void(AsyncWebServerRequest*)>> plainRoutes;
    inline static std::vector<std::pair<AsyncURIMatcher,int>> order; // Registration order, as the server tries them.
    explicit AsyncWebServer(int) {}
    void on(AsyncURIMatcher uri, int method, JsonHandler handler) { routes[uri.uri] = handler; order.push_back({uri, method}); }
    void on(AsyncURIMatcher uri, int method, std::function<void(AsyncWebServerRequest*)> handler) {
        plainRoutes[{uri.uri,method}] = handler; order.push_back({uri, method});
    }
    // The route a request reaches: the first registered one that matches its path and method.
    static std::string Resolve(const std::string& path, int method) {
        for (const auto& [matcher, routeMethod] : order) if (routeMethod == method && matcher.Matches(path)) return matcher.uri;
        return "";
    }
    void begin() {}
};
