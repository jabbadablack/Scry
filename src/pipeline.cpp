#include <engine/pipeline.h>
#include <engine/engine.h>
#include <cassert>
#include <cstring>
#include <cstdio>

namespace Engine {
namespace Pipeline {

ecs_entity_t Phase_Input       = 0;
ecs_entity_t Phase_Intent      = 0;
ecs_entity_t Phase_StateUpdate = 0;
ecs_entity_t Phase_React       = 0;
ecs_entity_t Phase_StateSync   = 0;
ecs_entity_t Phase_Cleanup     = 0;
ecs_entity_t IsIntent          = 0;

/**
 * @brief Let's create a new phase for our pipeline!
 * 
 * Phases help us organize when things happen in our engine.
 * 
 * @param world The Flecs world we're working in.
 * @param name A friendly name for this phase.
 * @param depends_on Another phase that should happen before this one.
 * @return The ID of our shiny new phase.
 * 
 * @example
 * ecs_entity_t my_phase = MakePhase(world, "MyPhase", 0);
 */
static ecs_entity_t MakePhase(ecs_world_t* world, const char* name, ecs_entity_t depends_on) {
    assert(world != nullptr);
    assert(name != nullptr);
    EngineLog("[Pipeline] Creating a new phase...");
    
    ecs_entity_desc_t desc = {};
    desc.name = name;
    const ecs_entity_t ent = ecs_entity_init(world, &desc);
    assert(ent != 0);

    ecs_add_id(world, ent, EcsPhase);
    if (depends_on != 0) {
        ecs_add_pair(world, ent, EcsDependsOn, depends_on);
    }

#ifndef NDEBUG
    char buf[64];
    std::snprintf(buf, sizeof(buf), "[Pipeline] Phase created: %s (id=%llu)", name, (unsigned long long)ent);
    EngineLog(buf);
#endif
    EngineLog("[Pipeline] Phase initialization complete.");

    return ent;
}

/**
 * @brief Time to get the pipeline up and running!
 * 
 * This sets up all the standard phases we use for our frame logic.
 * 
 * @param world The Flecs world where the pipeline will live.
 * 
 * @example
 * InitPipeline(world);
 */
void InitPipeline(ecs_world_t* world) {
    assert(world != nullptr);
    assert(ecs_get_world_info(world) != nullptr);
    EngineLog("[Pipeline] Initializing engine pipeline...");

    Phase_Input       = MakePhase(world, "Phase_Input",       0);
    Phase_Intent      = MakePhase(world, "Phase_Intent",      Phase_Input);
    Phase_StateUpdate = MakePhase(world, "Phase_StateUpdate", Phase_Intent);
    Phase_React       = MakePhase(world, "Phase_React",       Phase_StateUpdate);
    Phase_StateSync   = MakePhase(world, "Phase_StateSync",   Phase_React);
    Phase_Cleanup     = MakePhase(world, "Phase_Cleanup",     Phase_StateSync);

    assert(Phase_Input       != 0);
    assert(Phase_Intent      != 0);
    assert(Phase_StateUpdate != 0);
    assert(Phase_React       != 0);
    assert(Phase_StateSync   != 0);
    assert(Phase_Cleanup     != 0);

    ecs_entity_desc_t intent_desc = {};
    intent_desc.name = "IsIntent";
    IsIntent = ecs_entity_init(world, &intent_desc);
    assert(IsIntent != 0);

#ifndef NDEBUG
    EngineLog("[Pipeline] IsIntent tag created");
#endif

    ecs_entity_desc_t pip_ent_desc = {};
    pip_ent_desc.name = "EnginePipeline";
    const ecs_entity_t pip_entity = ecs_entity_init(world, &pip_ent_desc);
    assert(pip_entity != 0);

    ecs_pipeline_desc_t pip_desc = {};
    pip_desc.entity                     = pip_entity;
    pip_desc.query.terms[0].id          = EcsSystem;
    pip_desc.query.terms[1].id          = EcsPhase;
    pip_desc.query.terms[1].trav        = EcsDependsOn;
    pip_desc.query.terms[1].src.id      = EcsCascade;

    const ecs_entity_t pipeline = ecs_pipeline_init(world, &pip_desc);
    assert(pipeline != 0);
    if (pipeline == 0) {
        EngineLog("[Pipeline] FATAL: ecs_pipeline_init() returned 0");
        return;
    }

    ecs_set_pipeline(world, pipeline);

#ifndef NDEBUG
    EngineLog("[Pipeline] Custom ISR pipeline installed");
#endif
    EngineLog("[Pipeline] Pipeline initialization finished.");
}

/**
 * @brief Let's mark a component as an 'Intent' component.
 * 
 * This helps the engine know how to handle these special components.
 * 
 * @param world The Flecs world.
 * @param comp_id The ID of the component we're marking.
 * 
 * @example
 * RegisterIntentComponent(world, my_comp_id);
 */
void RegisterIntentComponent(ecs_world_t* world, ecs_entity_t comp_id) {
    assert(world != nullptr);
    assert(comp_id != 0);
    EngineLog("[Pipeline] Registering intent component...");

    ecs_add_id(world, comp_id, IsIntent);

    ecs_entity_desc_t ent_desc = {};
    const ecs_entity_t sys_ent = ecs_entity_init(world, &ent_desc);
    assert(sys_ent != 0);

    ecs_add_pair(world, sys_ent, EcsDependsOn, Phase_Cleanup);

    ecs_system_desc_t sys_desc        = {};
    sys_desc.entity                   = sys_ent;
    sys_desc.query.terms[0].id        = comp_id;
    sys_desc.query.terms[0].inout     = EcsInOut;
    sys_desc.callback = [](ecs_iter_t* it) {
        const ecs_size_t sz = it->sizes[0];
        assert(sz > 0);
        void* data = ecs_field_w_size(it, static_cast<size_t>(sz), 0);
        assert(data != nullptr);
        if (data && sz > 0) {
            std::memset(data, 0, static_cast<size_t>(sz) * static_cast<size_t>(it->count));
        }
    };

    const ecs_entity_t final_sys = ecs_system_init(world, &sys_desc);
    assert(final_sys != 0);

#ifndef NDEBUG
    char buf[96];
    std::snprintf(buf, sizeof(buf),
        "[Pipeline] Intent cleanup system registered (comp=%llu, sys=%llu)",
        (unsigned long long)comp_id, (unsigned long long)final_sys);
    EngineLog(buf);
#endif
    EngineLog("[Pipeline] Intent registration complete.");
}

} // namespace Pipeline
} // namespace Engine
