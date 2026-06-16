#pragma once
#include <engine/engine.h>
#include <engine/pipeline.h>
#include <flecs.h>
#include <cassert>

namespace Engine {
namespace ECS {

ENGINE_API void InitOSAPI();
ENGINE_API ecs_world_t* CreateWorld();
ENGINE_API void ShutdownOSAPI();

template <typename T>
struct DoubleBuffered {
    T read;
    T write;
};

template <typename T>
void RegisterDoubleBufferSync(ecs_world_t* world, ecs_entity_t component_id) {
    assert(world != nullptr);
    assert(component_id != 0);

    ecs_entity_desc_t ent_desc = {};
    ent_desc.name = "SyncDoubleBuffer";
    const ecs_entity_t sys_ent = ecs_entity_init(world, &ent_desc);
    assert(sys_ent != 0);

    ecs_add_pair(world, sys_ent, EcsDependsOn, Engine::Pipeline::Phase_StateSync);

    ecs_system_desc_t sys_desc         = {};
    sys_desc.entity                    = sys_ent;
    sys_desc.query.terms[0].id         = component_id;
    sys_desc.callback = [](ecs_iter_t* it) {
        assert(it != nullptr);
        DoubleBuffered<T>* db = ecs_field(it, DoubleBuffered<T>, 0);
        assert(db != nullptr);
        for (int i = 0; i < it->count; ++i) db[i].read = db[i].write;
    };

    const ecs_entity_t final_sys = ecs_system_init(world, &sys_desc);
    assert(final_sys != 0);
    (void)final_sys;
}

} // namespace ECS
} // namespace Engine
