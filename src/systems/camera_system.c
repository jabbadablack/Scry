#include <engine/camera.h>
#include <engine/pipeline.h>
#include <engine/input.h>
#include <engine/engine.h>
#include <engine/spatial.h>
#include <flecs.h>
#include <assert.h>
#include <math.h>
#include <string.h>

ENGINE_API uint64_t id_ScryCamera = 0;

static void LH_LookAt(float* dst, vec3 eye, vec3 at, vec3 up) {
    mat4 m;
    glm_lookat_lh(eye, at, up, m);
    glm_mat4_copy(m, (float (*)[4])dst);
}

static void LH_Perspective(float* dst, float fovY_deg, float aspect, float zNear, float zFar) {
    mat4 m;
    glm_perspective_lh_zo(glm_rad(fovY_deg), aspect, zNear, zFar, m);
    glm_mat4_copy(m, (float (*)[4])dst);
}

static void CameraInputSystemCallback(ecs_iter_t* it) {
    ScryCamera* cam = ecs_field(it, ScryCamera, 0);
    const float speed = 10.0f * it->delta_time;
    const float sensitivity = 0.002f;

    for (int i = 0; i < it->count; ++i) {
        if (ScryInput_IsKeyDown(SCRY_KEY_MOUSER)) {
            cam[i].yaw   += g_ScryInput.states[g_ScryInput.read_index].mouse_dx * sensitivity;
            cam[i].pitch -= g_ScryInput.states[g_ScryInput.read_index].mouse_dy * sensitivity;
            if (cam[i].pitch > 1.5f) cam[i].pitch = 1.5f;
            if (cam[i].pitch < -1.5f) cam[i].pitch = -1.5f;
        }

        vec3 fwd, right, up = {0, 1, 0};
        fwd[0] = cosf(cam[i].pitch) * sinf(cam[i].yaw);
        fwd[1] = sinf(cam[i].pitch);
        fwd[2] = cosf(cam[i].pitch) * cosf(cam[i].yaw);
        glm_vec3_normalize(fwd);

        glm_vec3_cross(up, fwd, right);
        glm_vec3_normalize(right);

        if (ScryInput_IsKeyDown(SCRY_KEY_W)) {
            vec3 move; glm_vec3_scale(fwd, speed, move);
            glm_vec3_add(cam[i].position, move, cam[i].position);
        }
        if (ScryInput_IsKeyDown(SCRY_KEY_S)) {
            vec3 move; glm_vec3_scale(fwd, speed, move);
            glm_vec3_sub(cam[i].position, move, cam[i].position);
        }
        if (ScryInput_IsKeyDown(SCRY_KEY_A)) {
            vec3 move; glm_vec3_scale(right, speed, move);
            glm_vec3_sub(cam[i].position, move, cam[i].position);
        }
        if (ScryInput_IsKeyDown(SCRY_KEY_D)) {
            vec3 move; glm_vec3_scale(right, speed, move);
            glm_vec3_add(cam[i].position, move, cam[i].position);
        }
    }
}

static void CameraMatrixSystemCallback(ecs_iter_t* it) {
    ScryCamera* cam = ecs_field(it, ScryCamera, 0);
    for (int i = 0; i < it->count; ++i) {
        vec3 fwd;
        fwd[0] = cosf(cam[i].pitch) * sinf(cam[i].yaw);
        fwd[1] = sinf(cam[i].pitch);
        fwd[2] = cosf(cam[i].pitch) * cosf(cam[i].yaw);

        vec3 eye, at, up = {0, 1, 0};
        glm_vec3_copy(cam[i].position, eye);
        glm_vec3_add(eye, fwd, at);

        LH_LookAt((float*)cam[i].view, eye, at, up);
        LH_Perspective((float*)cam[i].proj, 60.0f, 1280.0f / 720.0f, 0.1f, 1000.0f);

        // Vulkan [0,1] depth frustum extraction (column-major VP, column-vector convention).
        // Row k of VP in cglm[col][row] notation = (vp[0][k], vp[1][k], vp[2][k], vp[3][k]).
        // Left/Right: row3 ± row0  Bottom/Top: row3 ± row1
        // Near: row2 (not row2+row3 as in OpenGL [-1,1])  Far: row3 - row2
        mat4 vp;
        glm_mat4_mul((float (*)[4])cam[i].proj, (float (*)[4])cam[i].view, vp);

        float (*p)[4] = cam[i].frustum_planes;
        for (int c = 0; c < 4; ++c) {
            p[0][c] = vp[c][3] + vp[c][0];
            p[1][c] = vp[c][3] - vp[c][0];
            p[2][c] = vp[c][3] + vp[c][1];
            p[3][c] = vp[c][3] - vp[c][1];
            p[4][c] = vp[c][2];
            p[5][c] = vp[c][3] - vp[c][2];
        }
        for (int k = 0; k < 6; ++k) {
            float len = sqrtf(p[k][0]*p[k][0] + p[k][1]*p[k][1] + p[k][2]*p[k][2]);
            if (len > 1e-6f) {
                float inv = 1.0f / len;
                p[k][0] *= inv; p[k][1] *= inv; p[k][2] *= inv; p[k][3] *= inv;
            }
        }
    }
}

void ScryCamera_Init(struct ecs_world_t* world) {
    assert(world != NULL);

    {
        ecs_entity_desc_t ed = { .name = "ScryCamera" };
        ecs_component_desc_t cd = {
            .entity = ecs_entity_init(world, &ed),
            .type.size = sizeof(ScryCamera),
            .type.alignment = _Alignof(ScryCamera)
        };
        id_ScryCamera = ecs_component_init(world, &cd);
    }

    {
        ecs_system_desc_t s = {0};
        s.entity = ecs_entity_init(world, &(ecs_entity_desc_t){ .name = "CameraInputSystem" });
        s.query.terms[0].id = (ecs_entity_t)id_ScryCamera;
        s.callback = CameraInputSystemCallback;
        ecs_add_pair(world, s.entity, EcsDependsOn, (ecs_entity_t)ScryPhase_Evaluate);
        ecs_system_init(world, &s);
    }

    {
        ecs_system_desc_t s = {0};
        s.entity = ecs_entity_init(world, &(ecs_entity_desc_t){ .name = "CameraMatrixSystem" });
        s.query.terms[0].id = (ecs_entity_t)id_ScryCamera;
        s.callback = CameraMatrixSystemCallback;
        ecs_add_pair(world, s.entity, EcsDependsOn, (ecs_entity_t)ScryPhase_React);
        ecs_system_init(world, &s);
    }
}
