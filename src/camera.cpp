#include <engine/camera.hpp>
#include <engine/pipeline.hpp>
#include <engine/input.hpp>
#include <Eigen/Geometry>
#include <cmath>
#include <algorithm>
#include <cstring>

namespace Engine {
namespace Camera {

ecs_entity_t id_Camera = 0;

// Left-handed lookAt (column-major). Forward = +Z, stored into dst[16].
static void LH_LookAt(float* dst, Eigen::Vector3f eye, Eigen::Vector3f at, Eigen::Vector3f up) {
    Eigen::Vector3f f = (at - eye).normalized();        // +Z forward (LH)
    Eigen::Vector3f r = f.cross(up).normalized();       // right  (LH: forward × up)
    Eigen::Vector3f u = r.cross(f);                     // recomputed up

    // Column-major view matrix: v = M * p
    Eigen::Matrix4f M;
    M.col(0) = Eigen::Vector4f( r.x(),  u.x(),  f.x(), 0.f);
    M.col(1) = Eigen::Vector4f( r.y(),  u.y(),  f.y(), 0.f);
    M.col(2) = Eigen::Vector4f( r.z(),  u.z(),  f.z(), 0.f);
    M.col(3) = Eigen::Vector4f(-r.dot(eye), -u.dot(eye), -f.dot(eye), 1.f);
    std::memcpy(dst, M.data(), 64);
}

// Left-handed perspective, depth [0,1], Y-up clip space (D3D/Diligent convention).
static void LH_Perspective(float* dst, float fovY_deg, float aspect, float zNear, float zFar) {
    const float fovY = fovY_deg * (3.14159265f / 180.0f);
    const float f    = 1.0f / std::tan(fovY * 0.5f);
    const float Q    = zFar / (zFar - zNear);

    Eigen::Matrix4f P = Eigen::Matrix4f::Zero();
    P(0, 0) = f / aspect;
    P(1, 1) = f;
    P(2, 2) = Q;
    P(3, 2) = 1.0f;        // LH: clip_w = view_z
    P(2, 3) = -Q * zNear;
    std::memcpy(dst, P.data(), 64);
}

void Init(ecs_world_t* world) {
    {
        ecs_entity_desc_t ed = {}; ed.name = "Camera";
        ecs_component_desc_t cd = {};
        cd.entity = ecs_entity_init(world, &ed);
        cd.type.size = sizeof(Camera);
        cd.type.alignment = alignof(Camera);
        id_Camera = ecs_component_init(world, &cd);
    }

    // Camera input (Phase_Intent)
    {
        ecs_entity_desc_t ed = {}; ed.name = "CameraInputSystem";
        ecs_entity_t sys_ent = ecs_entity_init(world, &ed);
        ecs_add_pair(world, sys_ent, EcsDependsOn, Engine::Pipeline::Phase_Intent);

        ecs_system_desc_t s = {};
        s.entity = sys_ent;
        s.query.terms[0].id    = id_Camera;
        s.query.terms[0].inout = EcsInOut;
        s.callback = [](ecs_iter_t* it) {
            Camera* cam = ecs_field(it, Camera, 0);
            const float speed       = 10.0f * it->delta_time;
            const float sensitivity = 0.002f;

            for (int i = 0; i < it->count; ++i) {
                if (Input::g_input_buffer.IsKeyDown(Input::Key::MouseR)) {
                    cam[i].yaw   -= Input::g_input_buffer.states[Input::g_input_buffer.read_index].mouse_dx * sensitivity;
                    cam[i].pitch += Input::g_input_buffer.states[Input::g_input_buffer.read_index].mouse_dy * sensitivity;
                    cam[i].pitch  = std::clamp(cam[i].pitch, -1.5f, 1.5f);
                }
                Eigen::Quaternionf q =
                    Eigen::AngleAxisf(cam[i].yaw,   Eigen::Vector3f::UnitY()) *
                    Eigen::AngleAxisf(cam[i].pitch,  Eigen::Vector3f::UnitX());
                Eigen::Vector3f fwd   = q * Eigen::Vector3f::UnitZ();
                Eigen::Vector3f right = q * Eigen::Vector3f::UnitX();

                if (Input::g_input_buffer.IsKeyDown(Input::Key::W)) cam[i].position += fwd   * speed;
                if (Input::g_input_buffer.IsKeyDown(Input::Key::S)) cam[i].position -= fwd   * speed;
                if (Input::g_input_buffer.IsKeyDown(Input::Key::A)) cam[i].position += right * speed;
                if (Input::g_input_buffer.IsKeyDown(Input::Key::D)) cam[i].position -= right * speed;
            }
        };
        ecs_system_init(world, &s);
    }

    // Camera matrix (Phase_StateUpdate)
    {
        ecs_entity_desc_t ed = {}; ed.name = "CameraMatrixSystem";
        ecs_entity_t sys_ent = ecs_entity_init(world, &ed);
        ecs_add_pair(world, sys_ent, EcsDependsOn, Engine::Pipeline::Phase_StateUpdate);

        ecs_system_desc_t s = {};
        s.entity = sys_ent;
        s.query.terms[0].id    = id_Camera;
        s.query.terms[0].inout = EcsInOut;
        s.callback = [](ecs_iter_t* it) {
            Camera* cam = ecs_field(it, Camera, 0);
            for (int i = 0; i < it->count; ++i) {
                Eigen::Quaternionf q =
                    Eigen::AngleAxisf(cam[i].yaw,   Eigen::Vector3f::UnitY()) *
                    Eigen::AngleAxisf(cam[i].pitch,  Eigen::Vector3f::UnitX());
                Eigen::Vector3f fwd = q * Eigen::Vector3f::UnitZ();

                Eigen::Vector3f eye = cam[i].position;
                Eigen::Vector3f at  = eye + fwd;

                LH_LookAt(cam[i].view, eye, at, Eigen::Vector3f::UnitY());
                LH_Perspective(cam[i].proj, 60.0f, 1280.0f / 720.0f, 0.1f, 1000.0f);
            }
        };
        ecs_system_init(world, &s);
    }
}

} // namespace Camera
} // namespace Engine
