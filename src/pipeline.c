#include <engine/pipeline.h>
#include <flecs.h>
#include <stdio.h>
#include <assert.h>
#include <string.h>

uint64_t ScryPhase_Input = 0;
uint64_t ScryPhase_Intent = 0;
uint64_t ScryPhase_StateUpdate = 0;
uint64_t ScryPhase_StateSync = 0;
uint64_t ScryPhase_React = 0;
uint64_t ScryPhase_Cleanup = 0;
uint64_t ScryIsIntent = 0;

static ecs_entity_t RegPhase(ecs_world_t* world, const char* name, ecs_entity_t parent) {
    ecs_entity_desc_t ed = { .name = name };
    ecs_entity_t ent = ecs_entity_init(world, &ed);
    if (parent) ecs_add_pair(world, ent, EcsDependsOn, parent);
    return ent;
}

void ScryPipeline_Init(struct ecs_world_t* world) {
    assert(world != NULL);

    ScryPhase_Input = RegPhase(world, "Phase_Input", EcsOnLoad);
    ScryPhase_Intent = RegPhase(world, "Phase_Intent", (ecs_entity_t)ScryPhase_Input);
    ScryPhase_StateUpdate = RegPhase(world, "Phase_StateUpdate", (ecs_entity_t)ScryPhase_Intent);
    ScryPhase_StateSync = RegPhase(world, "Phase_StateSync", (ecs_entity_t)ScryPhase_StateUpdate);
    ScryPhase_React = RegPhase(world, "Phase_React", (ecs_entity_t)ScryPhase_StateSync);
    ScryPhase_Cleanup = RegPhase(world, "Phase_Cleanup", (ecs_entity_t)ScryPhase_React);

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
    ecs_add_pair(world, sys, EcsDependsOn, (ecs_entity_t)ScryPhase_Cleanup);

    ecs_system_desc_t sd = {
        .entity = sys,
        .query.terms[0] = { .id = (ecs_entity_t)comp_id, .inout = EcsInOut },
        .query.terms[1] = { .id = (ecs_entity_t)ScryIsIntent },
        .ctx = (void*)sz,
        .callback = ScryPipeline_CleanupIntentCallback
    };
    ecs_system_init(world, &sd);
}
