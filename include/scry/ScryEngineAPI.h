#pragma once
#include <stddef.h>
#include <stdint.h>

// Forward declare Flecs structures
struct ecs_world_t;

typedef struct ScryEngineAPI {
    struct ecs_world_t* ecs_world;
    void (*Log)(const char* msg);
    void* (*Alloc)(size_t size);
    void (*Free)(void* ptr);
} ScryEngineAPI;

#ifdef _WIN32
    #define SCRY_PLUGIN_EXPORT __declspec(dllexport)
#else
    #define SCRY_PLUGIN_EXPORT __attribute__ ((visibility ("default")))
#endif

#ifdef __cplusplus
extern "C" {
#endif

SCRY_PLUGIN_EXPORT void ScryPluginInit(const ScryEngineAPI* api);

#ifdef __cplusplus
}
#endif
