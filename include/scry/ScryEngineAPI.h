#pragma once
#include <stddef.h>
#include <stdint.h>

// Forward declare Flecs structures
struct ecs_world_t;

// Callback for a parallel-for task range submitted via ScryEngineAPI::SubmitTask.
// start     — inclusive first index of the work slice assigned to this call
// end       — exclusive last index (process indices [start, end))
// thread_idx — worker thread index (0 == main thread)
// user_data — opaque pointer passed through from the submit site
typedef void (*ScryTaskFn)(uint32_t start, uint32_t end, uint32_t thread_idx, void* user_data);

typedef struct ScryEngineAPI {
    struct ecs_world_t* ecs_world;
    void (*Log)(const char* msg);
    void* (*Alloc)(size_t size);
    void (*Free)(void* ptr);
    // Submit a parallel-for over `count` work items; blocks until all ranges
    // have finished executing. Safe to call from any thread the plugin owns.
    void (*SubmitTask)(ScryTaskFn fn, void* user_data, uint32_t count);
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
