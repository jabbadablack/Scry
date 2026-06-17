#include <engine/pipeline.h>
#include <flecs.h>
#include <stdio.h>
#include <assert.h>
#include <string.h>

uint64_t ScryPhase_Sense    = 0;
uint64_t ScryPhase_Evaluate = 0;
uint64_t ScryPhase_React    = 0;
uint64_t ScryPhase_Resolve  = 0;
uint64_t ScryIsIntent       = 0;

static ecs_entity_t RegPhase(ecs_world_t* world, const char* name, ecs_entity_t parent) {
    ecs_entity_desc_t ed = { .name = name };
    ecs_entity_t ent = ecs_entity_init(world, &ed);
    if (parent) ecs_add_pair(world, ent, EcsDependsOn, parent);
    return ent;
}

void ScryPipeline_Init(struct ecs_world_t* world) {
    assert(world != NULL);

    ScryPhase_Sense    = RegPhase(world, "Phase_Sense",    EcsOnLoad);
    ScryPhase_Evaluate = RegPhase(world, "Phase_Evaluate", (ecs_entity_t)ScryPhase_Sense);
    ScryPhase_React    = RegPhase(world, "Phase_React",    (ecs_entity_t)ScryPhase_Evaluate);
    ScryPhase_Resolve  = RegPhase(world, "Phase_Resolve",  (ecs_entity_t)ScryPhase_React);

    ecs_entity_desc_t ed = { .name = "IsIntent" };
    ScryIsIntent = ecs_entity_init(world, &ed);
}

static void ScryPipeline_CleanupIntentCallback(ecs_iter_t* it) {
    size_t sz = (size_t)it->ctx;
    void* data = ecs_field_w_size(it, sz, 0);
    memset(data, 0, sz * (size_t)it->count);
}

void ScryPipeline_RegisterIntentComponent(struct ecs_world_t* world, uint64_t comp_id) {
    const ecs_type_info_t* type = ecs_get_type_info(world, (ecs_entity_t)comp_id);
    size_t sz = type ? type->size : 0;

    ecs_entity_desc_t ed = { .name = "CleanupIntent" };
    ecs_entity_t sys = ecs_entity_init(world, &ed);
    ecs_add_pair(world, sys, EcsDependsOn, (ecs_entity_t)ScryPhase_Resolve);

    ecs_system_desc_t sd = {
        .entity = sys,
        .query.terms[0] = { .id = (ecs_entity_t)comp_id, .inout = EcsInOut },
        .query.terms[1] = { .id = (ecs_entity_t)ScryIsIntent },
        .ctx = (void*)sz,
        .callback = ScryPipeline_CleanupIntentCallback
    };
    ecs_system_init(world, &sd);
}
