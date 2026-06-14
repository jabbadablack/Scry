#pragma once
#include <scry/core.hpp>
#include <scry/ScryEngineAPI.h>

namespace Scry {
namespace Jobs {

// Initialize the global task scheduler. Call before Flecs and before any
// plugin is loaded. Returns false only if the OS thread pool cannot be created.
SCRY_API bool Init();

// Drain all in-flight tasks and tear down all worker threads.
// Must be called after ecs_fini and after all plugins are unloaded.
SCRY_API void Shutdown();

// Submit a parallel-for over [0, count) items, blocking until all ranges
// have been processed. Matches the ScryTaskFn signature in ScryEngineAPI.h
// so the same function pointer works for both the internal and plugin API.
SCRY_API void SubmitTask(ScryTaskFn fn, void* user_data, uint32_t count);

// ── Flecs task-scheduler bridge ───────────────────────────────────────────
// Intentionally mirrors ecs_os_thread_callback_t (void*(*)(void*)) without
// pulling in flecs.h so scry_ecs.cpp can cast between the two safely.
typedef void* (*ScryFlecsTaskFn)(void* param);

// Submit a single fire-once task; starts executing immediately on an available
// worker. Returns an opaque handle that must be passed to WaitFlecsTask.
SCRY_API void* SubmitFlecsTask(ScryFlecsTaskFn callback, void* param);

// Block until the task is complete; returns the callback's return value.
// Destroys and frees the handle — do not use it after this call.
SCRY_API void* WaitFlecsTask(void* handle);

// Total threads managed by the scheduler (task threads + main thread).
// Used to configure Flecs's task-thread count to match enkiTS's pool size.
SCRY_API uint32_t GetTotalThreadCount();

} // namespace Jobs
} // namespace Scry
