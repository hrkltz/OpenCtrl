#pragma once
#include <cstdint>
#define portMAX_DELAY UINT32_MAX
#define pdTRUE 1
#define pdPASS 1
#define pdMS_TO_TICKS(ms) (ms)
inline int xTaskCreate(void (*)(void*), const char*, unsigned, void*, unsigned, void*) { return pdPASS; }
inline void vTaskDelay(unsigned) {}
