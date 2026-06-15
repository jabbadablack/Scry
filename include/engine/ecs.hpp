#pragma once
#include <engine/engine.h>
#include <engine/pipeline.hpp>
#include <flecs.h>
#include <libassert/assert.hpp>

namespace Engine {
namespace ECS {

// Initialize the Flecs OS API overrides (memory, threading, atomics).
ENGINE_API void InitOSAPI();

// Create a new ECS world: installs the OS API, imports FlecsMeta, builds
// the custom Scry pipeline, and binds the enkiTS task scheduler.
ENGINE_API ecs_world_t* CreateWorld();

// Shutdown the Flecs OS API and free resources.
ENGINE_API void ShutdownOSAPI();

// ── Double-buffer template ────────────────────────────────────────────────

template <typename T>
struct DoubleBuffered {
    T read;
    T write;
};

// Register a read<-write sync system for component T in Phase_StateSync.
// After every StateUpdate, this copies write -> read so the next frame's
// Intent and Input systems see a stable snapshot.
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

    ecs_add_pair(world, sys_ent, EcsDependsOn, Engine::Pipeline::Phase_StateSync);

    ecs_system_desc_t sys_desc      = {};
    sys_desc.entity                 = sys_ent;
    sys_desc.query.terms[0].id      = component_id;
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
    (void)final_sys;
}

} // namespace ECS
} // namespace Engine
