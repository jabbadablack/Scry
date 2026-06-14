#pragma once
#include <engine/engine.h>
#include <engine/PluginAPI.h>

namespace enki { class TaskScheduler; }

namespace Engine {
namespace Jobs {

// Initialize the global task scheduler. Call before Flecs and before any
// plugin is loaded. Returns false only if the OS thread pool cannot be created.
// thread_count: 0 = 1 worker thread; N = N worker threads.
ENGINE_API bool Init(uint32_t thread_count);

// Drain all in-flight tasks and tear down all worker threads.
// Must be called after ecs_fini and after all plugins are unloaded.
ENGINE_API void Shutdown();

// Submit a parallel-for over [0, count) items, blocking until all ranges
// have been processed. Matches the TaskFn signature in PluginAPI.h
// so the same function pointer works for both the internal and plugin API.
ENGINE_API void SubmitTask(TaskFn fn, void* user_data, uint32_t count);

// ── Flecs task-scheduler bridge ───────────────────────────────────────────
// Intentionally mirrors ecs_os_thread_callback_t (void*(*)(void*)) without
// pulling in flecs.h so ecs.cpp can cast between the two safely.
typedef void* (*FlecsTaskFn)(void* param);

// Submit a single fire-once task; starts executing immediately on an available
// worker. Returns an opaque handle that must be passed to WaitFlecsTask.
ENGINE_API void* SubmitFlecsTask(FlecsTaskFn callback, void* param);

// Block until the task is complete; returns the callback's return value.
// Destroys and frees the handle — do not use it after this call.
ENGINE_API void* WaitFlecsTask(void* handle);

// Total threads managed by the scheduler (task threads + main thread).
// Used to configure Flecs's task-thread count to match enkiTS's pool size.
ENGINE_API uint32_t GetTotalThreadCount();

// Raw scheduler pointer for storage in Context.
// Only forward-declared here; callers that need to call scheduler methods must
// include <TaskScheduler.h> directly.
ENGINE_API enki::TaskScheduler* GetScheduler();

} // namespace Jobs
} // namespace Engine
