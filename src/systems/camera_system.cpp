#include <engine/camera.h>
#include <engine/pipeline.h>
#include <engine/input.h>
#include <engine/engine.h>
#include <cglm/cglm.h>
#include <cglm/struct.h>
#include <cmath>
#include <algorithm>
#include <cstring>
#include <cassert>
#include <cstdio>

namespace Engine {
namespace Camera {

ecs_entity_t id_Camera = 0;

static void LH_LookAt(float* dst, vec3 eye, vec3 at, vec3 up) {
    mat4 m;
    // Using generic glm_lookat since CGLM_FORCE_LEFT_HANDED is defined in math.h
    glm_lookat(eye, at, up, m);
    glm_mat4_copy(m, (float (*)[4])dst);
}

static void LH_Perspective(float* dst, float fovY_deg, float aspect, float zNear, float zFar) {
    mat4 m;
    // Using generic glm_perspective since CGLM_FORCE_LEFT_HANDED is defined in math.h
    glm_perspective(glm_rad(fovY_deg), aspect, zNear, zFar, m);
    glm_mat4_copy(m, (float (*)[4])dst);
}

void Init(ecs_world_t* world) {
    assert(world != nullptr);
    assert(id_Camera == 0);

    {
        ecs_entity_desc_t ed = {}; ed.name = "Camera";
        ecs_component_desc_t cd = {};
        cd.entity = ecs_entity_init(world, &ed);
        cd.type.size = sizeof(Camera);
        cd.type.alignment = alignof(Camera);
        id_Camera = ecs_component_init(world, &cd);
    }

    {
        ecs_entity_desc_t ed = {}; ed.name = "CameraInputSystem";
        ecs_entity_t sys_ent = ecs_entity_init(world, &ed);
        ecs_add_pair(world, sys_ent, EcsDependsOn, Engine::Pipeline::Phase_Intent);

        ecs_system_desc_t s = {};
        s.entity = sys_ent;
        s.query.terms[0].id    = id_Camera;
        s.query.terms[0].inout = EcsInOut;
        s.callback = [](ecs_iter_t* it) {
            Camera* cam = (Camera*)ecs_field(it, Camera, 0);
            const float speed       = 10.0f * it->delta_time;
            const float sensitivity = 0.002f;

            for (int i = 0; i < it->count; ++i) {
                if (Input::g_input_buffer.IsKeyDown(Input::Key::MouseR)) {
                    cam[i].yaw   += Input::g_input_buffer.states[Input::g_input_buffer.read_index].mouse_dx * sensitivity;
                    cam[i].pitch -= Input::g_input_buffer.states[Input::g_input_buffer.read_index].mouse_dy * sensitivity;
                    cam[i].pitch  = std::clamp(cam[i].pitch, -1.5f, 1.5f);
                }

                vec3 fwd, right, up = {0, 1, 0};
                fwd[0] = cosf(cam[i].pitch) * sinf(cam[i].yaw);
                fwd[1] = sinf(cam[i].pitch);
                fwd[2] = cosf(cam[i].pitch) * cosf(cam[i].yaw);
                glm_vec3_normalize(fwd);

                glm_vec3_cross(up, fwd, right);
                glm_vec3_normalize(right);

                if (Input::g_input_buffer.IsKeyDown(Input::Key::W)) {
                    vec3 move; glm_vec3_scale(fwd, speed, move);
                    glm_vec3_add(cam[i].position, move, cam[i].position);
                }
                if (Input::g_input_buffer.IsKeyDown(Input::Key::S)) {
                    vec3 move; glm_vec3_scale(fwd, speed, move);
                    glm_vec3_sub(cam[i].position, move, cam[i].position);
                }
                if (Input::g_input_buffer.IsKeyDown(Input::Key::A)) {
                    vec3 move; glm_vec3_scale(right, speed, move);
                    glm_vec3_sub(cam[i].position, move, cam[i].position);
                }
                if (Input::g_input_buffer.IsKeyDown(Input::Key::D)) {
                    vec3 move; glm_vec3_scale(right, speed, move);
                    glm_vec3_add(cam[i].position, move, cam[i].position);
                }
            }
        };
        ecs_system_init(world, &s);
    }

    {
        ecs_entity_desc_t ed = {}; ed.name = "CameraMatrixSystem";
        ecs_entity_t sys_ent = ecs_entity_init(world, &ed);
        ecs_add_pair(world, sys_ent, EcsDependsOn, Engine::Pipeline::Phase_StateUpdate);

        ecs_system_desc_t s = {};
        s.entity = sys_ent;
        s.query.terms[0].id    = id_Camera;
        s.query.terms[0].inout = EcsInOut;
        s.callback = [](ecs_iter_t* it) {
            Camera* cam = (Camera*)ecs_field(it, Camera, 0);
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
            }
        };
        ecs_system_init(world, &s);
    }
}

} // namespace Camera
} // namespace Engine
