#pragma once
#include <engine/engine.h>
#include <engine/components/mesh.h>
#include <flecs.h>

namespace Engine {
namespace Renderer {

ENGINE_API extern ecs_entity_t id_MeshInstance;
ENGINE_API extern ecs_entity_t id_EntityIntent;
ENGINE_API extern ecs_entity_t id_Material;

ENGINE_API void Init(ecs_world_t* world);
ENGINE_API void Shutdown();

} // namespace Renderer
} // namespace Engine
