#pragma once
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// Forward declare Flecs structures
struct ecs_world_t;

typedef void (*TaskFn)(uint32_t start, uint32_t end, uint32_t thread_idx, void* user_data);

typedef struct PluginAPI {
    struct ecs_world_t* ecs_world;
    void (*Log)(const char* msg);
    void* (*Alloc)(size_t size);
    void (*Free)(void* ptr);
    void (*SubmitTask)(TaskFn fn, void* user_data, uint32_t count);
} PluginAPI;

#ifdef _WIN32
    #define PLUGIN_EXPORT __declspec(dllexport)
#else
    #define PLUGIN_EXPORT __attribute__ ((visibility ("default")))
#endif

PLUGIN_EXPORT void PluginInit(const PluginAPI* api);

#ifdef __cplusplus
}
#endif
