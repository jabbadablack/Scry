#pragma once
#include <engine/engine.h>
#include <engine/components/spatial.h>
#include <flecs.h>

namespace Engine {
namespace Spatial {

ENGINE_API extern ecs_entity_t id_ChunkCoord;
ENGINE_API extern ecs_entity_t id_ChunkHash;

/**
 * @brief Sets up the spatial partitioning system.
 *
 * Hi! This function gets the spatial system ready to go, initializing
 * components for chunked coordinate management. It's a key part of
 * keeping our world organized and efficient!
 *
 * @param world The Flecs world to initialize the spatial system in.
 *
 * @example
 * ecs_world_t* world = ecs_init();
 * Engine::Spatial::Init(world);
 */
ENGINE_API void Init(ecs_world_t* world);

} // namespace Spatial
} // namespace Engine
