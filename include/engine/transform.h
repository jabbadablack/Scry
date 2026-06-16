#pragma once
#include <engine/engine.h>
#include <engine/components/transform.h>
#include <flecs.h>

namespace Engine {
namespace Transform {

ENGINE_API extern ecs_entity_t id_Position;
ENGINE_API extern ecs_entity_t id_Rotation;
ENGINE_API extern ecs_entity_t id_Scale;
ENGINE_API extern ecs_entity_t id_WorldMatrix;
ENGINE_API extern ecs_entity_t id_DirtyMatrix;

ENGINE_API void Init(ecs_world_t* world);

} // namespace Transform
} // namespace Engine
