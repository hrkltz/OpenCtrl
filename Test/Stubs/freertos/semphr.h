#pragma once
#include "FreeRTOS.h"
#include <mutex>
#include <memory>
#include <vector>
struct MockSemaphore { std::recursive_mutex mutex; bool binary=false, count=false; };
using SemaphoreHandle_t = MockSemaphore*;
inline std::vector<std::unique_ptr<MockSemaphore>> mockSemaphores;
inline SemaphoreHandle_t xSemaphoreCreateRecursiveMutex() {
    mockSemaphores.push_back(std::make_unique<MockSemaphore>());
    return mockSemaphores.back().get();
}
inline SemaphoreHandle_t xSemaphoreCreateMutex() { return xSemaphoreCreateRecursiveMutex(); }
inline SemaphoreHandle_t xSemaphoreCreateBinary() { auto* sem=xSemaphoreCreateMutex(); sem->binary=true; return sem; }
inline int xSemaphoreTakeRecursive(SemaphoreHandle_t sem, uint32_t) { sem->mutex.lock(); return pdTRUE; }
inline void xSemaphoreGiveRecursive(SemaphoreHandle_t sem) { sem->mutex.unlock(); }
inline int xSemaphoreTake(SemaphoreHandle_t sem, uint32_t timeout) {
    if (!sem->binary) return xSemaphoreTakeRecursive(sem,timeout);
    const bool count=sem->count; sem->count=false; return count ? pdTRUE : 0;
}
inline void xSemaphoreGive(SemaphoreHandle_t sem) { if(sem->binary) sem->count=true; else xSemaphoreGiveRecursive(sem); }
