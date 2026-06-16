#pragma once
#include <engine/engine.h>
#include <engine/components/mesh.h>
#include <flecs.h>

namespace Engine {
namespace Renderer {

ENGINE_API extern ecs_entity_t id_MeshData;
ENGINE_API extern ecs_entity_t id_AABB;
ENGINE_API extern ecs_entity_t id_EntityIntent;
ENGINE_API extern ecs_entity_t id_Material;

/**
 * @brief Wakes up the renderer system.
 *
 * Greetings! This function brings our rendering engine to life by
 * registering essential components and preparing the world for drawing
 * beautiful meshes and materials.
 *
 * @param world The Flecs world to connect the renderer to.
 *
 * @example
 * ecs_world_t* world = ecs_init();
 * Engine::Renderer::Init(world);
 */
ENGINE_API void Init(ecs_world_t* world);

/**
 * @brief Puts the renderer system to sleep.
 *
 * Farewell! Call this when you're done to gracefully clean up
 * renderer resources. It's always polite to tidy up after ourselves!
 *
 * @example
 * Engine::Renderer::Shutdown();
 */
ENGINE_API void Shutdown();

} // namespace Renderer
} // namespace Engine
