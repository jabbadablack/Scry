#include <engine/ecs.h>
#include <engine/pipeline.h>
#include <flecs.h>
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif

void ScryECS_InitOSAPI(void) {
    ecs_os_set_api_defaults();
}

void ScryECS_ShutdownOSAPI(void) {
}

struct ecs_world_t* ScryECS_CreateWorld(void) {
    ecs_log_set_level(0); // Trace
    struct ecs_world_t* world = ecs_init();
    if (!world) return NULL;

    int32_t hw = 4;
#ifdef _WIN32
    SYSTEM_INFO sysinfo;
    GetSystemInfo(&sysinfo);
    hw = (int32_t)sysinfo.dwNumberOfProcessors;
#endif

    if (hw > 1) {
        ecs_set_threads(world, hw);
    }

    return world;
}

typedef struct {
    size_t component_size;
} SyncData;

static void SyncCallback(ecs_iter_t* it) {
    SyncData* sd = (SyncData*)it->ctx;
    size_t sz = sd->component_size;
    void* data = ecs_field_w_size(it, sz * 2, 1);
    
    for (int i = 0; i < it->count; ++i) {
        char* base = (char*)data + (i * sz * 2);
        memcpy(base, base + sz, sz); // read = write
    }
}

void ScryECS_RegisterDoubleBufferSync(struct ecs_world_t* world, uint64_t component_id, size_t component_size) {
    char name[128];
    snprintf(name, sizeof(name), "SyncDoubleBuffer_%llu", (unsigned long long)component_id);
    
    ecs_entity_desc_t ed = {0};
    ed.name = name;
    const ecs_entity_t sys_ent = ecs_entity_init(world, &ed);
    
    ecs_add_pair(world, sys_ent, EcsDependsOn, (ecs_entity_t)ScryPhase_StateSync);

    SyncData* sd = (SyncData*)malloc(sizeof(SyncData));
    sd->component_size = component_size;

    ecs_system_desc_t sys_desc = {0};
    sys_desc.entity = sys_ent;
    sys_desc.query.terms[0].id = (ecs_entity_t)component_id;
    sys_desc.ctx = sd;
    sys_desc.callback = SyncCallback;

    ecs_system_init(world, &sys_desc);
}
