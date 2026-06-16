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

/**
 * @brief Initializes the transform system components and observers.
 *
 * Hello there! This function sets up all the essential transform bits like
 * Position, Rotation, and Scale, and ensures that world matrices are
 * automatically recalculated when things move. Just call it once at startup!
 *
 * @param world The Flecs world where the transform magic should happen.
 *
 * @example
 * ecs_world_t* world = ecs_init();
 * Engine::Transform::Init(world);
 */
ENGINE_API void Init(ecs_world_t* world);

} // namespace Transform
} // namespace Engine
