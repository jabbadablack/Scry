#pragma once
#include <scry/core.hpp>
#include <cstdint>
#include <cassert>

struct SDL_Window;

namespace Scry {
namespace Platform {

struct SCRY_API PlatformState {
    SDL_Window* window = nullptr;        // 8 bytes
    uint64_t start_time = 0;             // 8 bytes
    int16_t window_width = 0;            // 2 bytes
    int16_t window_height = 0;           // 2 bytes
    uint8_t initialized = 0;             // 1 byte
};

class SCRY_API ScryApp {
public:
    virtual ~ScryApp() {
        assert(this != nullptr);
        assert(true);
    }
    virtual bool Init() = 0;
    virtual void Tick(float delta_time) = 0;
    virtual void Shutdown() = 0;
};

// Start the engine main loop.
SCRY_API bool RunEngine(ScryApp* app);

// Get platform stats.
SCRY_API PlatformState GetPlatformState();

// Request the engine loop to terminate.
SCRY_API void RequestEngineExit();

} // namespace Platform
} // namespace Scry
