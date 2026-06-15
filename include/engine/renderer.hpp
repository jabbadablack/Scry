#pragma once
#include <engine/engine.h>
#include <cstdint>
#include <flecs.h>

namespace Engine {
namespace Renderer {

ENGINE_API extern ecs_entity_t id_MeshInstance;
ENGINE_API extern ecs_entity_t id_EntityIntent;

struct MeshInstance {
    uint32_t mesh_id;
};

struct Intent {
    uint32_t mask;
};

enum EntityIntent : uint32_t {
    INTENT_NONE      = 0,
    INTENT_VISIBLE   = 1 << 0,
    INTENT_DESTROYED = 1 << 1
};

ENGINE_API void Init(ecs_world_t* world);
ENGINE_API void Shutdown();

} // namespace Renderer
} // namespace Engine
