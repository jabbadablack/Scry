#include <engine/pipeline.h>
#include <flecs.h>
#include <stdio.h>
#include <assert.h>
#include <string.h>

uint64_t ScryPhase_Sense    = 0;
uint64_t ScryPhase_Evaluate = 0;
uint64_t ScryPhase_React    = 0;
uint64_t ScryPhase_Resolve  = 0;
uint64_t ScryPhase_Cleanup  = 0;
uint64_t ScryIsIntent       = 0;

void ScryPipeline_Init(struct ecs_world_t* world) {
    assert(world != NULL);

    // Declare phase entities.
    ScryPhase_Sense    = ecs_entity_init(world, &(ecs_entity_desc_t){ .name = "Phase_Sense"    });
    ScryPhase_Evaluate = ecs_entity_init(world, &(ecs_entity_desc_t){ .name = "Phase_Evaluate" });
    ScryPhase_React    = ecs_entity_init(world, &(ecs_entity_desc_t){ .name = "Phase_React"    });
    ScryPhase_Resolve  = ecs_entity_init(world, &(ecs_entity_desc_t){ .name = "Phase_Resolve"  });
    ScryPhase_Cleanup  = ecs_entity_init(world, &(ecs_entity_desc_t){ .name = "Phase_Cleanup"  });

    // Tag them so Flecs' default pipeline recognises them as structural nodes.
    ecs_add_id(world, (ecs_entity_t)ScryPhase_Sense,    EcsPhase);
    ecs_add_id(world, (ecs_entity_t)ScryPhase_Evaluate, EcsPhase);
    ecs_add_id(world, (ecs_entity_t)ScryPhase_React,    EcsPhase);
    ecs_add_id(world, (ecs_entity_t)ScryPhase_Resolve,  EcsPhase);
    ecs_add_id(world, (ecs_entity_t)ScryPhase_Cleanup,  EcsPhase);

    // Anchor to EcsOnUpdate so Flecs computes DeltaTime before the ISR starts,
    // then chain the rest topologically.
    ecs_add_pair(world, (ecs_entity_t)ScryPhase_Sense,    EcsDependsOn, EcsOnUpdate);
    ecs_add_pair(world, (ecs_entity_t)ScryPhase_Evaluate, EcsDependsOn, (ecs_entity_t)ScryPhase_Sense);
    ecs_add_pair(world, (ecs_entity_t)ScryPhase_React,    EcsDependsOn, (ecs_entity_t)ScryPhase_Evaluate);
    ecs_add_pair(world, (ecs_entity_t)ScryPhase_Resolve,  EcsDependsOn, (ecs_entity_t)ScryPhase_React);
    ecs_add_pair(world, (ecs_entity_t)ScryPhase_Cleanup,  EcsDependsOn, (ecs_entity_t)ScryPhase_Resolve);

    ScryIsIntent = ecs_entity_init(world, &(ecs_entity_desc_t){ .name = "IsIntent" });
}

static void ScryPipeline_CleanupIntentCallback(ecs_iter_t* it) {
    // O(table) bulk removal — deferred to end of ecs_progress, safe to call here.
    ecs_entity_t comp_id = (ecs_entity_t)(uintptr_t)it->ctx;
    ecs_remove_all(it->world, comp_id);
}

void ScryPipeline_RegisterIntentComponent(struct ecs_world_t* world, uint64_t comp_id) {
    ecs_entity_desc_t ed = { .name = "CleanupIntent" };
    ecs_entity_t sys = ecs_entity_init(world, &ed);

    ecs_system_desc_t sd = {
        .entity = sys,
        .phase = (ecs_entity_t)ScryPhase_Cleanup,
        .ctx = (void*)(uintptr_t)comp_id,
        .callback = ScryPipeline_CleanupIntentCallback
    };
    ecs_system_init(world, &sd);
}
