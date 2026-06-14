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

} // namespace Jobs
} // namespace Scry
