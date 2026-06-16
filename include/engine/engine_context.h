#pragma once
#include <stdint.h>

struct ecs_world_t;

struct Context {
    struct ecs_world_t*  ecs_world;
    void*                window_handle;
    void*                user_data;
    uint64_t             start_time;
    int32_t              window_width;
    int32_t              window_height;
    uint8_t              initialized;
    uint8_t              running;
};
