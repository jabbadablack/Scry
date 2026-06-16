#pragma once
#include <engine/engine.h>
#include <engine/renderer/mesh.h>

#ifdef __cplusplus
extern "C" {
#endif

struct ecs_world_t;

ENGINE_API extern uint64_t id_ScryMeshData;
ENGINE_API extern uint64_t id_ScryAABB;
ENGINE_API extern uint64_t id_ScryEntityIntent;
ENGINE_API extern uint64_t id_ScryMaterial;

/**
 * @brief Wakes up the renderer system.
 */
ENGINE_API void ScryRenderer_Init(struct ecs_world_t* world);

/**
 * @brief Puts the renderer system to sleep.
 */
ENGINE_API void ScryRenderer_Shutdown(void);

#ifdef __cplusplus
}
#endif
