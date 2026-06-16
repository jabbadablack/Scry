#pragma once
#include <engine/engine.h>
#include <components/spatial.h>

#ifdef __cplusplus
extern "C" {
#endif

struct ecs_world_t;

ENGINE_API extern uint64_t id_ScryChunkCoord;
ENGINE_API extern uint64_t id_ScryChunkHash;

/**
 * @brief Sets up the spatial partitioning system.
 */
ENGINE_API void ScrySpatial_Init(struct ecs_world_t* world);

#ifdef __cplusplus
}
#endif
