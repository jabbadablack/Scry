#pragma once
#include <engine/engine.h>
#include <components/transform.h>

#ifdef __cplusplus
extern "C" {
#endif

struct ecs_world_t;

ENGINE_API extern uint64_t id_ScryPosition;
ENGINE_API extern uint64_t id_ScryRotation;
ENGINE_API extern uint64_t id_ScryScale;
ENGINE_API extern uint64_t id_ScryWorldMatrix;
ENGINE_API extern uint64_t id_ScryDirtyMatrix;

/**
 * @brief Initializes the transform system.
 */
ENGINE_API void ScryTransform_Init(struct ecs_world_t* world);

#ifdef __cplusplus
}
#endif
