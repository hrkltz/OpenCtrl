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
class AsyncWebServer {
public:
    using JsonHandler = std::function<void(AsyncWebServerRequest*, JsonVariant&)>;
    inline static std::map<std::string,JsonHandler> routes;
    inline static std::map<std::pair<std::string,int>,std::function<void(AsyncWebServerRequest*)>> plainRoutes;
    explicit AsyncWebServer(int) {}
    void on(const char* uri, int, JsonHandler handler) { routes[uri] = handler; }
    void on(const char* uri, int method, std::function<void(AsyncWebServerRequest*)> handler) { plainRoutes[{uri,method}] = handler; }
    void begin() {}
};
