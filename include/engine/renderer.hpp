#pragma once
#include <engine/engine.h>
#include <engine/math.hpp>
#include <cstdint>
#include <flecs.h>

/* ── GPU-driven ECS renderer ─────────────────────────────────────────────────
 * Call Engine::Renderer::Init(world) once from EngineRun after both
 * Graphics::Init() and ECS::CreateWorld() have succeeded.
 *
 * Attach MeshInstance to any entity that also has a WorldMatrix component.
 * The Phase_React RenderSystem maps a dynamic GPU instance buffer, memcpys
 * the contiguous WorldMatrix array into it, and issues one instanced draw
 * call per unique mesh_handle — zero per-entity CPU draw calls.
 */

namespace Engine {
namespace Renderer {

static constexpr uint32_t MAX_ENTITIES = 10000;
static constexpr uint32_t MAX_MESHES   = 256;

enum EntityIntent : uint32_t {
    INTENT_NONE      = 0,
    INTENT_VISIBLE   = 1 << 0,
    INTENT_DESTROYED = 1 << 1
};

#pragma pack(push, 1)
struct EntityState {
    Engine::Math::ScryMat4 transform;
    uint32_t               mesh_id;
    uint32_t               pad0;
    uint32_t               pad1;
    uint32_t               pad2;
};
#pragma pack(pop)

ENGINE_API extern ecs_entity_t id_MeshInstance;
ENGINE_API extern ecs_entity_t id_EntityIntent;

struct MeshInstance {
    uint32_t mesh_id;
};

struct Intent {
    uint32_t mask;
};

// Global pointers for the ECS to write directly into mapped GPU memory.
// These are set by the Renderer every frame.
ENGINE_API extern EntityState* g_MappedStateBuffer;
ENGINE_API extern uint32_t*    g_MappedIntentBuffer;

ENGINE_API void Init(ecs_world_t* world);
ENGINE_API void Shutdown();

} // namespace Renderer
} // namespace Engine
