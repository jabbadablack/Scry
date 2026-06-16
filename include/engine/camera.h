#pragma once
#include <engine/engine.h>
#include <components/camera.h>

#ifdef __cplusplus
extern "C" {
#endif

struct ecs_world_t;

ENGINE_API extern uint64_t id_ScryCamera;

/**
 * @brief Sets up the camera system.
 */
ENGINE_API void ScryCamera_Init(struct ecs_world_t* world);

#ifdef __cplusplus
}
#endif
