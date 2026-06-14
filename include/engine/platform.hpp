#pragma once
#include <stdint.h>

namespace Engine {
namespace Platform {

void* InitWindow(const char* title, int32_t width, int32_t height);
void PumpEvents(struct Context* ctx);
uint64_t GetTime();
void DestroyWindow(void* window_handle);

} // namespace Platform
} // namespace Engine
