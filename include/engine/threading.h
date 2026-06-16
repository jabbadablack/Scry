#pragma once
#include <engine/engine.h>
#include <flecs.h>

namespace Engine {
namespace Threading {

ENGINE_API void Init(int num_threads);
ENGINE_API void Shutdown();

/* Patches ecs_os_api.task_new_/task_join_ to dispatch through the pool.
 * Must be called after ecs_os_set_api_defaults() and before ecs_init(). */
ENGINE_API void SetFlecsOSAPI();

} // namespace Threading
} // namespace Engine
