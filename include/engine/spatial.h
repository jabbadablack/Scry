#pragma once
#include <engine/engine.h>
#include <engine/components/spatial.h>
#include <flecs.h>

namespace Engine {
namespace Spatial {

ENGINE_API extern ecs_entity_t id_ChunkCoord;
ENGINE_API extern ecs_entity_t id_ChunkHash;

ENGINE_API void Init(ecs_world_t* world);

} // namespace Spatial
} // namespace Engine
