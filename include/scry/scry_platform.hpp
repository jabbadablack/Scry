#pragma once
// scry_platform.hpp — internal engine header.
// Provides the full definition of the ScryContext struct whose forward
// declaration is published in scry.h. Only engine source files should
// include this header. External applications must not include it.
#include <scry/scry.h>
#include <libassert/assert.hpp>

struct ecs_world_t;
struct SDL_Window;

// Full definition of the opaque ScryContext type declared in scry.h.
// Fields are internal and subject to change between engine versions.
struct ScryContext {
    struct ecs_world_t* ecs_world;
    struct SDL_Window*  window;
    void*               user_data;
    uint64_t            start_time;
    int32_t             window_width;
    int32_t             window_height;
    uint8_t             initialized;
    uint8_t             running;
};
