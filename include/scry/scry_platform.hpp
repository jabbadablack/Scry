#pragma once
#include <scry/core.hpp>
#include <cstdint>
#include <libassert/assert.hpp>

struct ecs_world_t;
struct SDL_Window;

// Flat context struct holding entire engine state
typedef struct ScryContext {
    struct ecs_world_t* ecs_world;      // 8 bytes
    struct SDL_Window* window;          // 8 bytes
    uint64_t start_time;                 // 8 bytes
    int32_t window_width;                // 4 bytes
    int32_t window_height;               // 4 bytes
    uint8_t initialized;                 // 1 byte
    uint8_t running;                     // 1 byte
} ScryContext;

// App config struct
typedef struct ScryAppConfig {
    void (*OnInit)(ScryContext* ctx);
    void (*OnUpdate)(ScryContext* ctx, float delta_time);
    void (*OnShutdown)(ScryContext* ctx);
    int32_t window_width;
    int32_t window_height;
    const char* app_name;
} ScryAppConfig;

#ifdef __cplusplus
extern "C" {
#endif

// Run the engine
SCRY_API int ScryRun(const ScryAppConfig* config);

// Request exit
SCRY_API void RequestEngineExit(ScryContext* ctx);

#ifdef __cplusplus
}
#endif
