#pragma once
#include <stddef.h>
#include <stdint.h>

// Forward declare Flecs structures
struct ecs_world_t;

// Callback for a parallel-for task range submitted via PluginAPI::SubmitTask.
// start     — inclusive first index of the work slice assigned to this call
// end       — exclusive last index (process indices [start, end))
// thread_idx — worker thread index (0 == main thread)
// user_data — opaque pointer passed through from the submit site
typedef void (*TaskFn)(uint32_t start, uint32_t end, uint32_t thread_idx, void* user_data);

typedef struct PluginAPI {
    struct ecs_world_t* ecs_world;
    void (*Log)(const char* msg);
    void* (*Alloc)(size_t size);
    void (*Free)(void* ptr);
    // Submit a parallel-for over `count` work items; blocks until all ranges
    // have finished executing. Safe to call from any thread the plugin owns.
    void (*SubmitTask)(TaskFn fn, void* user_data, uint32_t count);
} PluginAPI;

#ifdef _WIN32
    #define PLUGIN_EXPORT __declspec(dllexport)
#else
    #define PLUGIN_EXPORT __attribute__ ((visibility ("default")))
#endif

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Welcome to the plugin world! This is where your plugin gets to say hello to the engine.
 * 
 * The engine calls this when it loads your plugin. Use the provided API to register your components and systems.
 * 
 * @param api A bundle of useful functions and pointers the engine provides just for you.
 * 
 * @example
 * PLUGIN_EXPORT void PluginInit(const PluginAPI* api) {
 *     api->Log("My awesome plugin is starting up!");
 * }
 */
PLUGIN_EXPORT void PluginInit(const PluginAPI* api);

#ifdef __cplusplus
}
#endif
