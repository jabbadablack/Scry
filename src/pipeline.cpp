#include <engine/pipeline.hpp>
#include <libassert/assert.hpp>

namespace Engine {
namespace Pipeline {

ecs_entity_t Phase_Input       = 0;
ecs_entity_t Phase_Intent      = 0;
ecs_entity_t Phase_StateUpdate = 0;
ecs_entity_t Phase_React       = 0;
ecs_entity_t Phase_StateSync   = 0;
ecs_entity_t Phase_Cleanup     = 0;
ecs_entity_t IsIntent          = 0;

// ── Intent cleanup system ─────────────────────────────────────────────────
// Rigorously bulk-deletes all entities carrying the IsIntent tag.
// This runs once per frame and guarantees zero leftover intent memory.
static void CleanupIntentsSystem(ecs_iter_t* it) {
    DEBUG_ASSERT(it != nullptr);
    ecs_delete_with(it->world, IsIntent);
}

// ── Helper: create a named phase entity and tag it with EcsPhase ──────────
static ecs_entity_t MakePhase(ecs_world_t* world, const char* name, ecs_entity_t depends_on) {
    ecs_entity_desc_t desc = {};
    desc.name = name;
    const ecs_entity_t ent = ecs_entity_init(world, &desc);
    DEBUG_ASSERT(ent != 0);

    ecs_add_id(world, ent, EcsPhase);
    if (depends_on != 0) {
        ecs_add_pair(world, ent, EcsDependsOn, depends_on);
    }
    return ent;
}

void InitPipeline(ecs_world_t* world) {
    DEBUG_ASSERT(world != nullptr);

    // ── 1. Create phases ──────────────────────────────────────────────────
    // The Scry engine uses a strict 6-phase ISR (Intent-State-Reactor) flow.
    // Flecs' internal scheduler uses these EcsDependsOn relationships to determine execution order.
    Phase_Input       = MakePhase(world, "Phase_Input",       0);
    Phase_Intent      = MakePhase(world, "Phase_Intent",      Phase_Input);
    Phase_StateUpdate = MakePhase(world, "Phase_StateUpdate",  Phase_Intent);
    Phase_React       = MakePhase(world, "Phase_React",        Phase_StateUpdate);
    Phase_StateSync   = MakePhase(world, "Phase_StateSync",    Phase_React);
    Phase_Cleanup     = MakePhase(world, "Phase_Cleanup",      Phase_StateSync);

    // ── 2. IsIntent tag ──────────────────────────────────────────────────────
    ecs_entity_desc_t intent_desc = {};
    intent_desc.name = "IsIntent";
    IsIntent = ecs_entity_init(world, &intent_desc);
    DEBUG_ASSERT(IsIntent != 0);

    // ── 3. Cleanup system ────────────────────────────────────────────────────
    // This system has no query terms, so it runs exactly once per frame during
    // Phase_Cleanup. It performs a bulk delete of all entities that
    // carry IsIntent; this guarantees zero leftover memory.
    ecs_entity_desc_t cleanup_ent_desc = {};
    cleanup_ent_desc.name = "IntentCleanupSystem";
    const ecs_entity_t cleanup_ent = ecs_entity_init(world, &cleanup_ent_desc);
    DEBUG_ASSERT(cleanup_ent != 0);
    ecs_add_pair(world, cleanup_ent, EcsDependsOn, Phase_Cleanup);

    ecs_system_desc_t cleanup_sys_desc = {};
    cleanup_sys_desc.entity            = cleanup_ent;
    cleanup_sys_desc.callback          = CleanupIntentsSystem;
    const ecs_entity_t cleanup_sys = ecs_system_init(world, &cleanup_sys_desc);
    DEBUG_ASSERT(cleanup_sys != 0);
    (void)cleanup_sys;

    // ── 4. Custom pipeline ───────────────────────────────────────────────────
    ecs_entity_desc_t pip_ent_desc = {};
    pip_ent_desc.name = "EnginePipeline";
    const ecs_entity_t pip_entity = ecs_entity_init(world, &pip_ent_desc);
    DEBUG_ASSERT(pip_entity != 0);

    ecs_pipeline_desc_t pip_desc = {};
    pip_desc.entity = pip_entity;
    pip_desc.query.terms[0].id          = EcsSystem;
    pip_desc.query.terms[1].id          = EcsPhase;
    pip_desc.query.terms[1].trav        = EcsDependsOn;
    pip_desc.query.terms[1].src.id      = EcsCascade;

    const ecs_entity_t pipeline = ecs_pipeline_init(world, &pip_desc);
    DEBUG_ASSERT(pipeline != 0);

    ecs_set_pipeline(world, pipeline);
}

void RegisterIntentComponent(ecs_world_t* world, ecs_entity_t comp_id) {
    DEBUG_ASSERT(world != nullptr);
    DEBUG_ASSERT(comp_id != 0);
    DEBUG_ASSERT(IsIntent != 0);
    // In the new model, this is mostly for documentation or if we want to
    // automatically tag entities that receive this component.
    // For now, we manually tag intent entities.
    (void)world; (void)comp_id;
}

} // namespace Pipeline
} // namespace Engine
