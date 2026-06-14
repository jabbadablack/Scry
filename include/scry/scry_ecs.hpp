#pragma once
#include <scry/scry.h>
#include <flecs.h>
#include <libassert/assert.hpp>

namespace Scry {
namespace ECS {

// Custom phases exposed globally
SCRY_API extern ecs_entity_t OnIntentPhase;
SCRY_API extern ecs_entity_t OnStateUpdatePhase;
SCRY_API extern ecs_entity_t OnReactPhase;

// Initialize the global Flecs OS API overrides (Memory, Threading, Atomics)
SCRY_API void InitOSAPI();

// Create a new baseline ECS World with our custom pipeline phases
SCRY_API ecs_world_t* CreateWorld();

// Double buffered system template
template <typename T>
struct DoubleBuffered {
    T read;
    T write;
};

// Helper to register a double-buffer sync system for component type T
template <typename T>
void RegisterDoubleBufferSync(ecs_world_t* world, ecs_entity_t component_id) {
    DEBUG_ASSERT(world != nullptr);
    DEBUG_ASSERT(component_id != 0);

    if (world == nullptr) {
        return;
    }

    ecs_entity_desc_t ent_desc = {};
    ent_desc.name = "SyncDoubleBuffer";
    
    const ecs_entity_t sys_ent = ecs_entity_init(world, &ent_desc);
    DEBUG_ASSERT(sys_ent != 0);

    ecs_add_pair(world, sys_ent, EcsDependsOn, OnReactPhase);

    ecs_system_desc_t sys_desc = {};
    sys_desc.entity = sys_ent;
    sys_desc.query.terms[0].id = component_id;
    sys_desc.callback = [](ecs_iter_t* it) {
        DEBUG_ASSERT(it != nullptr);
        DEBUG_ASSERT(it->count >= 0);

        if (it == nullptr) {
            return;
        }

        DoubleBuffered<T>* db = ecs_field(it, DoubleBuffered<T>, 0);
        DEBUG_ASSERT(db != nullptr);
        if (db != nullptr) {
            for (int i = 0; i < it->count; ++i) {
                db[i].read = db[i].write;
            }
        }
    };

    const ecs_entity_t final_sys = ecs_system_init(world, &sys_desc);
    DEBUG_ASSERT(final_sys != 0);
}

} // namespace ECS
} // namespace Scry
