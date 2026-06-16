#pragma once
#include <engine/engine.h>
#include <engine/renderer/mesh.h>
#include <flecs.h>

namespace Engine {
namespace Renderer {

ENGINE_API extern ecs_entity_t id_MeshData;
ENGINE_API extern ecs_entity_t id_AABB;
ENGINE_API extern ecs_entity_t id_EntityIntent;
ENGINE_API extern ecs_entity_t id_Material;

/**
 * @brief Wakes up the renderer system.
 */
ENGINE_API void Init(ecs_world_t* world);

/**
 * @brief Puts the renderer system to sleep.
 */
ENGINE_API void Shutdown();

} // namespace Renderer
} // namespace Engine
