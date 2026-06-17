#include <engine/transform.h>
#include <engine/pipeline.h>
#include <engine/engine.h>
#include <engine/debug/profiler.h>
#include <flecs.h>
#include <assert.h>
#include <math.h>

ENGINE_API uint64_t id_ScryPosition = 0;
ENGINE_API uint64_t id_ScryRotation = 0;
ENGINE_API uint64_t id_ScryScale = 0;
ENGINE_API uint64_t id_ScryWorldMatrix = 0;
ENGINE_API uint64_t id_ScryDirtyMatrix = 0;

static uint64_t RegComp(ecs_world_t* world, const char* name, size_t sz, size_t align) {
    ecs_entity_desc_t ed = { .name = name };
    ecs_component_desc_t cd = {
        .entity = ecs_entity_init(world, &ed),
        .type.size = (ecs_size_t)sz,
        .type.alignment = (ecs_size_t)align
    };
    return ecs_component_init(world, &cd);
}

static void TransformSystemCallback(ecs_iter_t* it) {
    SCRY_PROFILE_ZONE(Phase_React_Transform);
    ScryPosition* pos = ecs_field(it, ScryPosition, 0);
    ScryRotation* rot = ecs_field(it, ScryRotation, 1);
    ScryScale* scl = ecs_field(it, ScryScale, 2);
    ScryWorldMatrix* wm = ecs_field(it, ScryWorldMatrix, 3);
    ScryDirtyMatrixIntent* dirty = ecs_field(it, ScryDirtyMatrixIntent, 4);

    for (int i = 0; i < it->count; ++i) {
        if (!dirty[i].active) continue;
        dirty[i].active = 0;

        mat4 m, rot_mat;
        glm_mat4_identity(m);
        glm_translate(m, (float*)pos[i].value);
        glm_euler_zyx((float*)rot[i].value, rot_mat);
        glm_mat4_mul(m, rot_mat, m);
        glm_scale(m, (float*)scl[i].value);
        glm_mat4_copy(m, wm[i].value);
    }
    SCRY_PROFILE_ZONE_END(Phase_React_Transform);
}

void ScryTransform_Init(struct ecs_world_t* world) {
    assert(world != NULL);

    id_ScryPosition = RegComp(world, "ScryPosition", sizeof(ScryPosition), _Alignof(ScryPosition));
    id_ScryRotation = RegComp(world, "ScryRotation", sizeof(ScryRotation), _Alignof(ScryRotation));
    id_ScryScale = RegComp(world, "ScryScale", sizeof(ScryScale), _Alignof(ScryScale));
    id_ScryWorldMatrix = RegComp(world, "ScryWorldMatrix", sizeof(ScryWorldMatrix), _Alignof(ScryWorldMatrix));
    id_ScryDirtyMatrix = RegComp(world, "ScryDirtyMatrix", sizeof(ScryDirtyMatrixIntent), _Alignof(ScryDirtyMatrixIntent));

    {
        ecs_system_desc_t s = {0};
        s.entity = ecs_entity_init(world, &(ecs_entity_desc_t){ .name = "TransformSystem" });
        s.query.terms[0].id = (ecs_entity_t)id_ScryPosition;    s.query.terms[0].inout = EcsIn;
        s.query.terms[1].id = (ecs_entity_t)id_ScryRotation;    s.query.terms[1].inout = EcsIn;
        s.query.terms[2].id = (ecs_entity_t)id_ScryScale;       s.query.terms[2].inout = EcsIn;
        s.query.terms[3].id = (ecs_entity_t)id_ScryWorldMatrix; s.query.terms[3].inout = EcsOut;
        s.query.terms[4].id = (ecs_entity_t)id_ScryDirtyMatrix; s.query.terms[4].inout = EcsInOut;
        s.callback = TransformSystemCallback;
        s.multi_threaded = true;

        ecs_add_pair(world, s.entity, EcsDependsOn, (ecs_entity_t)ScryPhase_React);
        ecs_system_init(world, &s);
    }
}
