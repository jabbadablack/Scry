#include <engine/camera.hpp>
#include <engine/pipeline.hpp>
#include <engine/input.hpp>
#include <bx/math.h>
#include <bgfx/bgfx.h>
#include <Eigen/Geometry>
#include <cmath>
#include <algorithm>

namespace Engine {
namespace Camera {

ecs_entity_t id_Camera = 0;

void Init(ecs_world_t* world) {
    // ── Register Component ───────────────────────────────────────────────────
    {
        ecs_entity_desc_t e = {};
        e.name = "Camera";
        ecs_component_desc_t c = {};
        c.entity = ecs_entity_init(world, &e);
        c.type.size = sizeof(Camera);
        c.type.alignment = alignof(Camera);
        id_Camera = ecs_component_init(world, &c);
    }

    // ── Camera Input System (Phase_Intent) ──────────────────────────────────
    {
        ecs_entity_desc_t ed = {};
        ed.name = "CameraInputSystem";
        ecs_entity_t sys_ent = ecs_entity_init(world, &ed);
        ecs_add_pair(world, sys_ent, EcsDependsOn, Engine::Pipeline::Phase_Intent);

        ecs_system_desc_t s = {};
        s.entity = sys_ent;
        s.query.terms[0].id = id_Camera;
        s.query.terms[0].inout = EcsInOut;
        s.callback = [](ecs_iter_t* it) {
            Camera* cam = ecs_field(it, Camera, 0);
            const float speed = 10.0f * it->delta_time;
            const float sensitivity = 0.002f;

            for (int i = 0; i < it->count; ++i) {
                if (Input::g_input_buffer.IsKeyDown(Input::Key::MouseR)) {
                    cam[i].yaw   -= Input::g_input_buffer.states[Input::g_input_buffer.read_index].mouse_dx * sensitivity;
                    cam[i].pitch -= Input::g_input_buffer.states[Input::g_input_buffer.read_index].mouse_dy * sensitivity;
                    cam[i].pitch = std::clamp(cam[i].pitch, -1.5f, 1.5f);
                }

                Eigen::Quaternionf q = 
                    Eigen::AngleAxisf(cam[i].yaw, Eigen::Vector3f::UnitY()) *
                    Eigen::AngleAxisf(cam[i].pitch, Eigen::Vector3f::UnitX());
                
                Eigen::Vector3f forward = q * -Eigen::Vector3f::UnitZ();
                Eigen::Vector3f right   = q * Eigen::Vector3f::UnitX();

                if (Input::g_input_buffer.IsKeyDown(Input::Key::W)) cam[i].position += forward * speed;
                if (Input::g_input_buffer.IsKeyDown(Input::Key::S)) cam[i].position -= forward * speed;
                if (Input::g_input_buffer.IsKeyDown(Input::Key::A)) cam[i].position -= right * speed;
                if (Input::g_input_buffer.IsKeyDown(Input::Key::D)) cam[i].position += right * speed;
            }
        };
        ecs_system_init(world, &s);
    }

    // ── Camera Matrix System (Phase_StateUpdate) ─────────────────────────────
    {
        ecs_entity_desc_t ed = {};
        ed.name = "CameraMatrixSystem";
        ecs_entity_t sys_ent = ecs_entity_init(world, &ed);
        ecs_add_pair(world, sys_ent, EcsDependsOn, Engine::Pipeline::Phase_StateUpdate);

        ecs_system_desc_t s = {};
        s.entity = sys_ent;
        s.query.terms[0].id = id_Camera;
        s.query.terms[0].inout = EcsInOut;
        s.callback = [](ecs_iter_t* it) {
            Camera* cam = ecs_field(it, Camera, 0);
            for (int i = 0; i < it->count; ++i) {
                bx::Vec3 eye = { cam[i].position.x(), cam[i].position.y(), cam[i].position.z() };
                
                Eigen::Quaternionf q = 
                    Eigen::AngleAxisf(cam[i].yaw, Eigen::Vector3f::UnitY()) *
                    Eigen::AngleAxisf(cam[i].pitch, Eigen::Vector3f::UnitX());
                Eigen::Vector3f fwd = q * -Eigen::Vector3f::UnitZ();
                
                bx::Vec3 at = { 
                    cam[i].position.x() + fwd.x(), 
                    cam[i].position.y() + fwd.y(), 
                    cam[i].position.z() + fwd.z() 
                };
                
                bx::mtxLookAt(cam[i].view, eye, at);
                bx::mtxProj(cam[i].proj, 60.0f, 1280.0f/720.0f, 0.1f, 1000.0f, bgfx::getCaps()->homogeneousDepth);
            }
        };
        ecs_system_init(world, &s);
    }
}

} // namespace Camera
} // namespace Engine
