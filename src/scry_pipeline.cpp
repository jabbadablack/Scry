#include <scry/scry_pipeline.hpp>
#include <libassert/assert.hpp>

namespace Scry {
namespace Pipeline {

ecs_entity_t ScryPhase_Input       = 0;
ecs_entity_t ScryPhase_Intent      = 0;
ecs_entity_t ScryPhase_StateUpdate = 0;
ecs_entity_t ScryPhase_StateSync   = 0;
ecs_entity_t ScryPhase_React       = 0;
ecs_entity_t ScryPhase_Cleanup     = 0;
ecs_entity_t IsIntent              = 0;

// ── Intent cleanup system ─────────────────────────────────────────────────
// The system query matches component TYPE entities that have IsIntent.
// it->entities[i] is simultaneously the "owner" entity (the type entity)
// and the component ID — so ecs_remove_all removes every instance of that
// component from every game entity, leaving no stale intent data between frames.
static void CleanupIntentsSystem(ecs_iter_t* it) {
    DEBUG_ASSERT(it != nullptr);
    for (int i = 0; i < it->count; ++i) {
        ecs_remove_all(it->world, it->entities[i]);
    }
}

// ── Helper: create a named phase entity and tag it with EcsPhase ──────────
static ecs_entity_t MakePhase(ecs_world_t* world, const char* name, ecs_entity_t depends_on) {
    ecs_entity_desc_t desc = {};
    desc.name = name;
    const ecs_entity_t phase = ecs_entity_init(world, &desc);
    DEBUG_ASSERT(phase != 0);
    // EcsPhase is a raw entity constant in Flecs v4, not a registered C type —
    // use ecs_add_id rather than the ecs_add macro which expects a type tag.
    ecs_add_id(world, phase, EcsPhase);
    if (depends_on != 0) {
        ecs_add_pair(world, phase, EcsDependsOn, depends_on);
    }
    return phase;
}

void InitPipeline(ecs_world_t* world) {
    DEBUG_ASSERT(world != nullptr);

    // ── Phase chain ───────────────────────────────────────────────────────
    // Each phase depends on its predecessor. The pipeline's cascade query
    // uses these EcsDependsOn relationships to determine execution order.
    // ScryPhase_Input is the root (no dependency) and runs first.
    ScryPhase_Input       = MakePhase(world, "ScryPhase_Input",       0);
    ScryPhase_Intent      = MakePhase(world, "ScryPhase_Intent",      ScryPhase_Input);
    ScryPhase_StateUpdate = MakePhase(world, "ScryPhase_StateUpdate",  ScryPhase_Intent);
    ScryPhase_StateSync   = MakePhase(world, "ScryPhase_StateSync",    ScryPhase_StateUpdate);
    ScryPhase_React       = MakePhase(world, "ScryPhase_React",        ScryPhase_StateSync);
    ScryPhase_Cleanup     = MakePhase(world, "ScryPhase_Cleanup",      ScryPhase_React);

    // ── IsIntent tag ──────────────────────────────────────────────────────
    ecs_entity_desc_t intent_desc = {};
    intent_desc.name = "IsIntent";
    IsIntent = ecs_entity_init(world, &intent_desc);
    DEBUG_ASSERT(IsIntent != 0);

    // ── Cleanup system ────────────────────────────────────────────────────
    // Runs in ScryPhase_Cleanup. Its query finds component TYPE entities that
    // carry IsIntent; for each, ecs_remove_all strips all game-entity instances.
    ecs_entity_desc_t cleanup_ent_desc = {};
    cleanup_ent_desc.name = "IntentCleanupSystem";
    const ecs_entity_t cleanup_ent = ecs_entity_init(world, &cleanup_ent_desc);
    DEBUG_ASSERT(cleanup_ent != 0);
    ecs_add_pair(world, cleanup_ent, EcsDependsOn, ScryPhase_Cleanup);

    ecs_system_desc_t cleanup_sys_desc = {};
    cleanup_sys_desc.entity            = cleanup_ent;
    cleanup_sys_desc.query.terms[0].id = IsIntent;
    cleanup_sys_desc.callback          = CleanupIntentsSystem;
    const ecs_entity_t cleanup_sys = ecs_system_init(world, &cleanup_sys_desc);
    DEBUG_ASSERT(cleanup_sys != 0);

    // ── Custom pipeline ───────────────────────────────────────────────────
    // Term 0 — match system entities.
    // Term 1 — order systems by traversing EcsDependsOn in cascade order,
    //           looking for EcsPhase on each phase entity.  Cascade means
    //           "root-first": Input(0) -> Intent(1) -> ... -> Cleanup(5).
    ecs_entity_desc_t pip_ent_desc = {};
    pip_ent_desc.name = "ScryPipeline";
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
    ecs_add_id(world, comp_id, IsIntent);
}

} // namespace Pipeline
} // namespace Scry
