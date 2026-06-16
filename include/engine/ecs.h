#pragma once
#include <engine/engine.h>
#include <engine/pipeline.h>
#include <flecs.h>
#include <cassert>
#include <cstdio>

namespace Engine {
namespace ECS {

/**
 * @brief Initializes the OS-specific API for ECS.
 * 
 * Hi there! This function sets up the necessary OS-level hooks so flecs 
 * can play nicely with our platform layer.
 * 
 * @example
 * Engine::ECS::InitOSAPI();
 */
ENGINE_API void InitOSAPI();

/**
 * @brief Creates a new ECS world.
 * 
 * Ready to build your universe? This function creates and initializes a 
 * brand new flecs world for all your entities and systems.
 * 
 * @return Pointer to the newly created ecs_world_t.
 * 
 * @example
 * ecs_world_t* world = Engine::ECS::CreateWorld();
 */
ENGINE_API ecs_world_t* CreateWorld();

/**
 * @brief Shuts down the OS-specific API for ECS.
 * 
 * Time to clean up! This function removes the OS hooks we set up earlier.
 * 
 * @example
 * Engine::ECS::ShutdownOSAPI();
 */
ENGINE_API void ShutdownOSAPI();

template <typename T>
struct DoubleBuffered {
    T read;
    T write;
};

/**
 * @brief Registers a synchronization system for double-buffered components.
 * 
 * Need to sync your data? This handy template registers a system that 
 * automatically copies 'write' data to 'read' data at the right time in the 
 * pipeline.
 * 
 * @tparam T The type of the component.
 * @param world The ECS world.
 * @param component_id The ID of the component to sync.
 * 
 * @example
 * Engine::ECS::RegisterDoubleBufferSync<MyComponent>(world, ecs_id(MyComponent));
 */
template <typename T>
void RegisterDoubleBufferSync(ecs_world_t* world, ecs_entity_t component_id) {
    assert(world != nullptr);
    assert(component_id != 0);
    static bool logged_once_register = false;
    if (!logged_once_register) {
        std::printf("ECS: Registering double buffer sync for component %llu\n", component_id);
        logged_once_register = true;
    }

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
        assert(it->world != nullptr);
        static bool logged_once_callback = false;
        if (!logged_once_callback) {
            std::printf("ECS: Running double buffer sync callback for %d entities\n", it->count);
            logged_once_callback = true;
        }

        DoubleBuffered<T>* db = ecs_field(it, DoubleBuffered<T>, 0);
        assert(db != nullptr);
        for (int i = 0; i < it->count; ++i) db[i].read = db[i].write;

        static bool logged_once_finished = false;
        if (!logged_once_finished) {
            std::printf("ECS: Finished syncing %d entities\n", it->count);
            logged_once_finished = true;
        }
    };

    const ecs_entity_t final_sys = ecs_system_init(world, &sys_desc);
    assert(final_sys != 0);
    (void)final_sys;

    static bool logged_once_success = false;
    if (!logged_once_success) {
        std::printf("ECS: Successfully registered sync system %llu\n", final_sys);
        logged_once_success = true;
    }
}

} // namespace ECS
} // namespace Engine
