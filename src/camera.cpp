#include <engine/camera.hpp>
#include <engine/pipeline.hpp>
#include <engine/input.hpp>
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
            const float speed = 5.0f * it->delta_time;
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
                Eigen::Quaternionf q = 
                    Eigen::AngleAxisf(cam[i].yaw, Eigen::Vector3f::UnitY()) *
                    Eigen::AngleAxisf(cam[i].pitch, Eigen::Vector3f::UnitX());
                
                Eigen::Vector3f target = cam[i].position + (q * -Eigen::Vector3f::UnitZ());
                Eigen::Vector3f up     = q * Eigen::Vector3f::UnitY();

                Eigen::Matrix4f view = Eigen::Matrix4f::Identity();
                // Simple LookAt implementation
                Eigen::Vector3f f = (target - cam[i].position).normalized();
                Eigen::Vector3f r = f.cross(up).normalized();
                Eigen::Vector3f u = r.cross(f);

                view(0, 0) = r.x(); view(0, 1) = r.y(); view(0, 2) = r.z(); view(0, 3) = -r.dot(cam[i].position);
                view(1, 0) = u.x(); view(1, 1) = u.y(); view(1, 2) = u.z(); view(1, 3) = -u.dot(cam[i].position);
                view(2, 0) = -f.x(); view(2, 1) = -f.y(); view(2, 2) = -f.z(); view(2, 3) = f.dot(cam[i].position);
                
                // Perspective (Hardcoded 90 FOV, 16:9 for now)
                float aspect = 1280.0f / 720.0f;
                float fov = 1.57f; // ~90 deg
                float near = 0.1f;
                float far = 1000.0f;
                float t = std::tan(fov / 2.0f);
                
                Eigen::Matrix4f proj = Eigen::Matrix4f::Zero();
                proj(0, 0) = 1.0f / (aspect * t);
                proj(1, 1) = 1.0f / t;
                proj(2, 2) = -far / (far - near);
                proj(2, 3) = -(far * near) / (far - near);
                proj(3, 2) = -1.0f;

                cam[i].view_proj = proj * view;
            }
        };
        ecs_system_init(world, &s);
    }
}

} // namespace Camera
} // namespace Engine
