#include <engine/engine_context.h>
#include <engine/platform.h>
#include <engine/input.h>
#include <engine/ecs.h>
#include <engine/graphics.h>
#include <engine/renderer.h>
#include <engine/plugin.h>

#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>

#include <cassert>
#include <cstdlib>
#include <cstdio>
#include <thread>
#include <chrono>

namespace Engine {
namespace Input {
InputBuffer g_input_buffer;
}
}

namespace Engine {
namespace Platform {

static void KeyCallback(GLFWwindow* window, int key, int scancode, int action, int mods) {
    (void)window; (void)scancode; (void)mods;
    if (key < 0 || key >= 512) return;
    const uint8_t w = Engine::Input::g_input_buffer.write_index;
    auto& state = Engine::Input::g_input_buffer.states[w];
    const uint32_t idx = static_cast<uint32_t>(key) / 8;
    const uint32_t bit = static_cast<uint32_t>(key) % 8;
    if (action == GLFW_PRESS)   state.keys[idx] |=  static_cast<uint8_t>(1u << bit);
    else if (action == GLFW_RELEASE) state.keys[idx] &= static_cast<uint8_t>(~(1u << bit));
}

static void MouseButtonCallback(GLFWwindow* window, int button, int action, int mods) {
    (void)mods;
    if (button < 0 || button > 2) return;
    const uint8_t w = Engine::Input::g_input_buffer.write_index;
    auto& state = Engine::Input::g_input_buffer.states[w];

    uint32_t sc = 0;
    if (button == GLFW_MOUSE_BUTTON_LEFT)   sc = static_cast<uint32_t>(Engine::Input::Key::MouseL);
    else if (button == GLFW_MOUSE_BUTTON_RIGHT)  sc = static_cast<uint32_t>(Engine::Input::Key::MouseR);
    else if (button == GLFW_MOUSE_BUTTON_MIDDLE) sc = static_cast<uint32_t>(Engine::Input::Key::MouseM);

    const uint32_t idx = sc / 8, bit = sc % 8;
    if (action == GLFW_PRESS) {
        state.keys[idx] |= static_cast<uint8_t>(1u << bit);
        if (button == GLFW_MOUSE_BUTTON_RIGHT) {
            glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
            if (glfwRawMouseMotionSupported())
                glfwSetInputMode(window, GLFW_RAW_MOUSE_MOTION, GLFW_TRUE);
        }
    } else if (action == GLFW_RELEASE) {
        state.keys[idx] &= static_cast<uint8_t>(~(1u << bit));
        if (button == GLFW_MOUSE_BUTTON_RIGHT)
            glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
    }
}

static void CursorPosCallback(GLFWwindow* window, double xpos, double ypos) {
    (void)window;
    const uint8_t w = Engine::Input::g_input_buffer.write_index;
    auto& state = Engine::Input::g_input_buffer.states[w];
    static double last_x = xpos, last_y = ypos;
    state.mouse_dx += static_cast<float>(xpos - last_x);
    state.mouse_dy += static_cast<float>(ypos - last_y);
    last_x = xpos; last_y = ypos;
    state.mouse_x = static_cast<int16_t>(xpos);
    state.mouse_y = static_cast<int16_t>(ypos);
}

static void ErrorCallback(int error, const char* description) {
    std::fprintf(stderr, "[GLFW Error] %d: %s\n", error, description);
}

void* InitWindow(const char* title, int32_t width, int32_t height) {
    glfwSetErrorCallback(ErrorCallback);
    if (!glfwInit()) return nullptr;
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE);
    GLFWwindow* window = glfwCreateWindow(width, height, title ? title : "Engine", nullptr, nullptr);
    if (window) {
        glfwSetKeyCallback(window, KeyCallback);
        glfwSetMouseButtonCallback(window, MouseButtonCallback);
        glfwSetCursorPosCallback(window, CursorPosCallback);
    }
    return window;
}

void PumpEvents(Context* ctx) {
    assert(ctx && ctx->initialized == 1);
    const uint8_t w = Engine::Input::g_input_buffer.write_index;
    const uint8_t r = Engine::Input::g_input_buffer.read_index;
    Engine::Input::g_input_buffer.states[w] = Engine::Input::g_input_buffer.states[r];
    Engine::Input::g_input_buffer.states[w].mouse_dx = 0.0f;
    Engine::Input::g_input_buffer.states[w].mouse_dy = 0.0f;
    glfwPollEvents();
    if (glfwWindowShouldClose(static_cast<GLFWwindow*>(ctx->window_handle)))
        ctx->running = 0;
    Engine::Input::g_input_buffer.Swap();
}

uint64_t GetTime() {
    return static_cast<uint64_t>(glfwGetTime() * 1000.0);
}

void DestroyWindow(void* handle) {
    if (handle) glfwDestroyWindow(static_cast<GLFWwindow*>(handle));
    glfwTerminate();
}

} // namespace Platform
} // namespace Engine

extern "C" {

static void (*g_app_log)(const char*) = nullptr;

enum class Subsystem { None, Platform, Graphics, Flecs, Renderer };

ENGINE_API EngineError EngineRun(const AppConfig* config) {
    if (!config || !config->OnInit || !config->OnShutdown) return ERR_PLATFORM_INIT;
    g_app_log = config->OnLog;

    Subsystem init_checklist[16] = {};
    int init_count = 0;
    EngineError ret_code = SUCCESS;

    void* global_memory_pool = nullptr;
    void* window = nullptr;
    struct ecs_world_t* world = nullptr;
    Context ctx = {};

    if (config->global_memory_pool_size > 0)
        global_memory_pool = std::malloc(config->global_memory_pool_size);

    window = Engine::Platform::InitWindow(config->title, config->window_width, config->window_height);
    if (!window) { ret_code = ERR_PLATFORM_INIT; goto shutdown; }
    init_checklist[init_count++] = Subsystem::Platform;

    if (!Engine::Graphics::Init(window)) { ret_code = ERR_GRAPHICS_INIT; goto shutdown; }
    init_checklist[init_count++] = Subsystem::Graphics;

    world = Engine::ECS::CreateWorld();
    if (!world) { ret_code = ERR_ECS_INIT; goto shutdown; }
    init_checklist[init_count++] = Subsystem::Flecs;

    Engine::Renderer::Init(world);
    init_checklist[init_count++] = Subsystem::Renderer;

    ctx.ecs_world     = world;
    ctx.window_handle = window;
    ctx.user_data     = config->user_data;
    ctx.start_time    = Engine::Platform::GetTime();
    ctx.window_width  = config->window_width;
    ctx.window_height = config->window_height;
    ctx.initialized   = 1;
    ctx.running       = 1;

    config->OnInit(&ctx);

    {
        uint64_t last_tick = Engine::Platform::GetTime();
        while (ctx.running) {
            auto frame_start = std::chrono::steady_clock::now();

            Engine::Platform::PumpEvents(&ctx);
            if (!ctx.running) break;
            if (Engine::Input::g_input_buffer.IsKeyDown(Engine::Input::Key::Escape)) {
                ctx.running = 0; break;
            }
            const uint64_t now = Engine::Platform::GetTime();
            const float dt = static_cast<float>(now - last_tick) / 1000.0f;
            last_tick = now;

            Engine::Graphics::BeginFrame();
            if (!ecs_progress(world, dt)) { ctx.running = 0; break; }
            Engine::Graphics::Present();

            // Kernel-level yield until 16ms have elapsed since frame start.
            // sleep_for(1ms) suspends at the OS scheduler level, keeping the core
            // near 0% rather than letting the Vulkan driver spin-lock on VSync.
            while (true) {
                const float elapsed_ms = std::chrono::duration<float, std::milli>(
                    std::chrono::steady_clock::now() - frame_start).count();
                if (elapsed_ms >= 16.0f) break;
                if (elapsed_ms < 15.0f)
                    std::this_thread::sleep_for(std::chrono::milliseconds(1));
            }
        }
    }

    if (config->OnShutdown) config->OnShutdown(&ctx);

shutdown:
    for (int i = init_count - 1; i >= 0; --i) {
        switch (init_checklist[i]) {
            case Subsystem::Renderer: Engine::Renderer::Shutdown(); break;
            case Subsystem::Flecs:
                if (world) ecs_fini(world);
                Engine::ECS::ShutdownOSAPI();
                break;
            case Subsystem::Graphics: Engine::Graphics::Shutdown(); break;
            case Subsystem::Platform:
                if (window) Engine::Platform::DestroyWindow(window);
                break;
            default: break;
        }
    }

    if (global_memory_pool) { std::free(global_memory_pool); global_memory_pool = nullptr; }
    return ret_code;
}

ENGINE_API void RequestExit(Context* ctx) {
    assert(ctx && ctx->initialized == 1);
    if (ctx) ctx->running = 0;
}

ENGINE_API void* GetUserData(const Context* ctx) {
    if (!ctx) return nullptr;
    return ctx->user_data;
}

ENGINE_API struct ecs_world_t* GetWorld(const Context* ctx) {
    if (!ctx) return nullptr;
    return ctx->ecs_world;
}

ENGINE_API void EngineLog(const char* msg) {
    if (!msg) return;
    if (g_app_log) g_app_log(msg);
    else { std::printf("[Engine] %s\n", msg); std::fflush(stdout); }
}

} // extern "C"
