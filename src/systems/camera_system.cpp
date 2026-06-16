#include <engine/camera.h>
#include <engine/pipeline.h>
#include <engine/input.h>
#include <engine/engine.h>
#include <Eigen/Geometry>
#include <cmath>
#include <algorithm>
#include <cstring>
#include <cassert>
#include <cstdio>

namespace Engine {
namespace Camera {

ecs_entity_t id_Camera = 0;

/**
 * @brief Computes a Left-Handed LookAt view matrix.
 *
 * This function helps position our camera in the world by defining where it's looking.
 * It's like pointing your eyes in the right direction!
 *
 * @param dst Pointer to the 16-float array where the matrix will be stored.
 * @param eye The position of the camera.
 * @param at The point the camera is looking at.
 * @param up The upward direction in the world.
 *
 * @example
 * float view[16];
 * LH_LookAt(view, {0,0,0}, {0,0,1}, {0,1,0});
 */
static void LH_LookAt(float* dst, Eigen::Vector3f eye, Eigen::Vector3f at, Eigen::Vector3f up) {
    assert(dst != nullptr);
    assert(up.norm() > 0.0f);
    static bool logged_once = false;
    if (!logged_once) {
        EngineLog("LH_LookAt: Calculating view matrix.");
        EngineLog("Ensuring the camera is looking at the target.");
        logged_once = true;
    }

    Eigen::Vector3f f = (at - eye).normalized();
    Eigen::Vector3f r = f.cross(up).normalized();
    Eigen::Vector3f u = r.cross(f);

    Eigen::Matrix4f M;
    M.col(0) = Eigen::Vector4f( r.x(),  u.x(),  f.x(), 0.f);
    M.col(1) = Eigen::Vector4f( r.y(),  u.y(),  f.y(), 0.f);
    M.col(2) = Eigen::Vector4f( r.z(),  u.z(),  f.z(), 0.f);
    M.col(3) = Eigen::Vector4f(-r.dot(eye), -u.dot(eye), -f.dot(eye), 1.f);
    std::memcpy(dst, M.data(), 64);
}

/**
 * @brief Computes a Left-Handed Perspective projection matrix.
 *
 * This function defines how 3D objects are flattened onto our 2D screen,
 * taking into account field of view and aspect ratio. It's the "lens" of our camera!
 *
 * @param dst Pointer to the 16-float array where the matrix will be stored.
 * @param fovY_deg Field of view in the Y direction, in degrees.
 * @param aspect Aspect ratio (width/height).
 * @param zNear Near clipping plane distance.
 * @param zFar Far clipping plane distance.
 * 
 * @example
 * float proj[16];
 * LH_Perspective(proj, 60.0f, 1.77f, 0.1f, 1000.0f);
 */
static void LH_Perspective(float* dst, float fovY_deg, float aspect, float zNear, float zFar) {
    assert(dst != nullptr);
    assert(zNear < zFar);
    static bool logged_once = false;
    if (!logged_once) {
        EngineLog("LH_Perspective: Building projection matrix.");
        EngineLog("Setting up the camera lens properties.");
        logged_once = true;
    }

    const float fovY = fovY_deg * (3.14159265f / 180.0f);
    const float f    = 1.0f / std::tan(fovY * 0.5f);
    const float Q    = zFar / (zFar - zNear);

    Eigen::Matrix4f P = Eigen::Matrix4f::Zero();
    P(0, 0) = f / aspect;
    P(1, 1) = f;
    P(2, 2) = Q;
    P(3, 2) = 1.0f;
    P(2, 3) = -Q * zNear;
    std::memcpy(dst, P.data(), 64);
}

/**
 * @brief Initializes the camera system by registering components and input/matrix systems.
 *
 * This function gets the whole camera infrastructure ready to roll.
 * It's what makes it possible for you to fly around and see your world!
 *
 * @param world A pointer to the ECS world.
 *
 * @example
 * ecs_world_t* world = ecs_init();
 * Engine::Camera::Init(world);
 */
void Init(ecs_world_t* world) {
    assert(world != nullptr);
    assert(id_Camera == 0);
    EngineLog("Camera::Init: Setting up camera components and systems.");
    EngineLog("Ready to start capturing the world!");

    {
        ecs_entity_desc_t ed = {}; ed.name = "Camera";
        ecs_component_desc_t cd = {};
        cd.entity = ecs_entity_init(world, &ed);
        cd.type.size = sizeof(Camera);
        cd.type.alignment = alignof(Camera);
        id_Camera = ecs_component_init(world, &cd);
    }

    // CameraInputSystem (Phase_Intent): read input → update pitch/yaw/position
    {
        ecs_entity_desc_t ed = {}; ed.name = "CameraInputSystem";
        ecs_entity_t sys_ent = ecs_entity_init(world, &ed);
        ecs_add_pair(world, sys_ent, EcsDependsOn, Engine::Pipeline::Phase_Intent);

        ecs_system_desc_t s = {};
        s.entity = sys_ent;
        s.query.terms[0].id    = id_Camera;
        s.query.terms[0].inout = EcsInOut;
        /**
         * @brief System callback that handles user input to move the camera.
         * 
         * This lambda listens to your keyboard and mouse to let you navigate the 3D space.
         * It updates the camera's position and orientation based on how you move!
         * 
         * @param it The ECS iterator.
         * 
         * @example
         * // Triggered by Flecs during the Intent phase
         * s.callback(it);
         */
        s.callback = [](ecs_iter_t* it) {
            assert(it != nullptr);
            assert(it->count >= 0);
            static bool logged_once = false;
            if (!logged_once) {
                EngineLog("CameraInputSystem: Processing user movement.");
                EngineLog("Updating camera pitch, yaw, and position.");
                logged_once = true;
            }

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

    // CameraMatrixSystem (Phase_StateUpdate): recompute view/proj matrices
    {
        ecs_entity_desc_t ed = {}; ed.name = "CameraMatrixSystem";
        ecs_entity_t sys_ent = ecs_entity_init(world, &ed);
        ecs_add_pair(world, sys_ent, EcsDependsOn, Engine::Pipeline::Phase_StateUpdate);

        ecs_system_desc_t s = {};
        s.entity = sys_ent;
        s.query.terms[0].id    = id_Camera;
        s.query.terms[0].inout = EcsInOut;
        /**
         * @brief System callback that recomputes camera view and projection matrices.
         * 
         * This lambda ensures that our view and projection matrices are always up-to-date
         * with the camera's current position and orientation. Essential for rendering!
         * 
         * @param it The ECS iterator.
         * 
         * @example
         * // Triggered by Flecs during the StateUpdate phase
         * s.callback(it);
         */
        s.callback = [](ecs_iter_t* it) {
            assert(it != nullptr);
            assert(it->count >= 0);
            static bool logged_once = false;
            if (!logged_once) {
                EngineLog("CameraMatrixSystem: Updating view and projection matrices.");
                EngineLog("Ensuring the renderer has the latest camera data.");
                logged_once = true;
            }

            Camera* cam = ecs_field(it, Camera, 0);
            for (int i = 0; i < it->count; ++i) {
                Eigen::Quaternionf q =
                    Eigen::AngleAxisf(cam[i].yaw,   Eigen::Vector3f::UnitY()) *
                    Eigen::AngleAxisf(cam[i].pitch,  Eigen::Vector3f::UnitX());
                Eigen::Vector3f fwd = q * Eigen::Vector3f::UnitZ();
                Eigen::Vector3f eye = cam[i].position;
                LH_LookAt(cam[i].view, eye, eye + fwd, Eigen::Vector3f::UnitY());
                LH_Perspective(cam[i].proj, 60.0f, 1280.0f / 720.0f, 0.1f, 1000.0f);
            }
        };
        ecs_system_init(world, &s);
    }
}

} // namespace Camera
} // namespace Engine
