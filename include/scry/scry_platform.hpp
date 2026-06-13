#pragma once
#include <scry/core.hpp>
#include <cstdint>

struct SDL_Window;

namespace Scry {
namespace Platform {

struct SCRY_API PlatformState {
    SDL_Window* window = nullptr;        // 8 bytes
    uint64_t start_time = 0;             // 8 bytes
    int16_t window_width = 0;            // 2 bytes
    int16_t window_height = 0;           // 2 bytes
    uint8_t initialized = 0;             // 1 byte
    // Total size: 21 bytes (padded to 24). Structured largest to smallest members.
};

struct SCRY_API ScryApp {
    bool (*Init)(void* user_data);
    void (*Tick)(void* user_data, float delta_time);
    void (*Shutdown)(void* user_data);
    void* user_data;
    // Total size: 32 bytes. No padding.
};

// Start the engine main loop.
SCRY_API bool RunEngine(ScryApp* app);

// Get platform stats.
SCRY_API PlatformState GetPlatformState();

// Request the engine loop to terminate.
SCRY_API void RequestEngineExit();


} // namespace Platform
} // namespace Scry
