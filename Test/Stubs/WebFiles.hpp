#pragma once
#include <cstddef>
#include <cstdint>
struct WebFile { const char* path; const char* contentType; const uint8_t* data; size_t size; };
inline const uint8_t mockWebIndex[] = "WebController page";
inline const WebFile WEB_FILES[] = {{"/index.html", "text/html", mockWebIndex, sizeof(mockWebIndex) - 1}};
