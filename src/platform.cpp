#include <engine/engine_context.hpp>
#include <engine/platform.hpp>
#include <engine/input.hpp>
#include <engine/ecs.hpp>
#include <engine/graphics.hpp>
#include <engine/renderer.hpp>
#include <engine/plugin.hpp>

#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>

#include <mimalloc.h>
#include <libassert/assert.hpp>
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

    const uint8_t w_idx = Engine::Input::g_input_buffer.write_index;
    auto& write_state = Engine::Input::g_input_buffer.states[w_idx];

    const uint32_t idx = static_cast<uint32_t>(key) / 8;
    const uint32_t bit = static_cast<uint32_t>(key) % 8;

    if (action == GLFW_PRESS) {
        write_state.keys[idx] |= static_cast<uint8_t>(1u << bit);
    } else if (action == GLFW_RELEASE) {
        write_state.keys[idx] &= static_cast<uint8_t>(~(1u << bit));
    }
}

static void MouseButtonCallback(GLFWwindow* window, int button, int action, int mods) {
    (void)mods;
    if (button < 0 || button > 2) return;

    const uint8_t w_idx = Engine::Input::g_input_buffer.write_index;
    auto& write_state = Engine::Input::g_input_buffer.states[w_idx];

    uint32_t scancode = 0;
    if (button == GLFW_MOUSE_BUTTON_LEFT) scancode = static_cast<uint32_t>(Engine::Input::Key::MouseL);
    else if (button == GLFW_MOUSE_BUTTON_RIGHT) scancode = static_cast<uint32_t>(Engine::Input::Key::MouseR);
    else if (button == GLFW_MOUSE_BUTTON_MIDDLE) scancode = static_cast<uint32_t>(Engine::Input::Key::MouseM);

    const uint32_t idx = scancode / 8;
    const uint32_t bit = scancode % 8;

    if (action == GLFW_PRESS) {
        write_state.keys[idx] |= static_cast<uint8_t>(1u << bit);
        if (button == GLFW_MOUSE_BUTTON_RIGHT) {
            glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
            if (glfwRawMouseMotionSupported()) {
                glfwSetInputMode(window, GLFW_RAW_MOUSE_MOTION, GLFW_TRUE);
            }
        }
    } else if (action == GLFW_RELEASE) {
        write_state.keys[idx] &= static_cast<uint8_t>(~(1u << bit));
        if (button == GLFW_MOUSE_BUTTON_RIGHT) {
            glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
        }
    }
}

static void CursorPosCallback(GLFWwindow* window, double xpos, double ypos) {
    (void)window;
    const uint8_t w_idx = Engine::Input::g_input_buffer.write_index;
    auto& write_state = Engine::Input::g_input_buffer.states[w_idx];

    static double last_x = xpos;
    static double last_y = ypos;

    write_state.mouse_dx += static_cast<float>(xpos - last_x);
    write_state.mouse_dy += static_cast<float>(ypos - last_y);

    last_x = xpos;
    last_y = ypos;

    write_state.mouse_x = static_cast<int16_t>(xpos);
    write_state.mouse_y = static_cast<int16_t>(ypos);
}

static void ErrorCallback(int error, const char* description) {
    std::fprintf(stderr, "[GLFW Error] %d: %s\n", error, description);
}

void* InitWindow(const char* title, int32_t width, int32_t height) {
    glfwSetErrorCallback(ErrorCallback);
    if (!glfwInit()) {
        return nullptr;
    }

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
    DEBUG_ASSERT(ctx != nullptr);
    DEBUG_ASSERT(ctx->initialized == 1);

    const uint8_t w_idx = Engine::Input::g_input_buffer.write_index;
    const uint8_t r_idx = Engine::Input::g_input_buffer.read_index;

    Engine::Input::g_input_buffer.states[w_idx] = Engine::Input::g_input_buffer.states[r_idx];
    Engine::Input::g_input_buffer.states[w_idx].mouse_dx = 0.0f;
    Engine::Input::g_input_buffer.states[w_idx].mouse_dy = 0.0f;

    glfwPollEvents();

    if (glfwWindowShouldClose(static_cast<GLFWwindow*>(ctx->window_handle))) {
        ctx->running = 0;
    }

    Engine::Input::g_input_buffer.Swap();
}

uint64_t GetTime() {
    return static_cast<uint64_t>(glfwGetTime() * 1000.0);
}

void DestroyWindow(void* window_handle) {
    if (window_handle) {
        glfwDestroyWindow(static_cast<GLFWwindow*>(window_handle));
    }
    glfwTerminate();
}

} // namespace Platform
} // namespace Engine

extern "C" {

static void (*g_app_log)(const char*) = nullptr;

enum class Subsystem { None, Mimalloc, Platform, Graphics, Flecs, Renderer };

ENGINE_API EngineError EngineRun(const AppConfig* config) {
    if (config == nullptr) return ERR_PLATFORM_INIT;
    if (config->OnInit == nullptr || config->OnShutdown == nullptr) return ERR_PLATFORM_INIT;

    g_app_log = config->OnLog;

    Subsystem init_checklist[16];
    for(int i=0; i<16; ++i) init_checklist[i] = Subsystem::None;
    int init_count = 0;
    EngineError ret_code = SUCCESS;

    void* global_memory_pool = nullptr;
    void* window = nullptr;
    struct ecs_world_t* world = nullptr;
    Context ctx = {};

    // ── 1. mimalloc ──────────────────────────────────────────────────────────
    mi_option_set(mi_option_show_errors, 0);
    mi_process_init();
    init_checklist[init_count++] = Subsystem::Mimalloc;

    if (config->global_memory_pool_size > 0) {
        global_memory_pool = mi_malloc(config->global_memory_pool_size);
    }

    // ── 2. Platform (GLFW) ────────────────────────────────────────────────
    window = Engine::Platform::InitWindow(config->title, config->window_width, config->window_height);
    if (!window) {
        ret_code = ERR_PLATFORM_INIT;
        goto shutdown;
    }
    init_checklist[init_count++] = Subsystem::Platform;

    // ── 3. Graphics (BGFX) ────────────────────────────────────────────────
    if (!Engine::Graphics::Init(window)) {
        ret_code = ERR_GRAPHICS_INIT;
        goto shutdown;
    }
    init_checklist[init_count++] = Subsystem::Graphics;

    // ── 4. Flecs ECS world ────────────────────────────────────────────────────
    world = Engine::ECS::CreateWorld();
    if (!world) {
        ret_code = ERR_ECS_INIT;
        goto shutdown;
    }
    init_checklist[init_count++] = Subsystem::Flecs;

    // ── 5. Renderer ───────────────────────────────────────────────────────────
    Engine::Renderer::Init(world);
    init_checklist[init_count++] = Subsystem::Renderer;

    // ── 6. Context assembly ───────────────────────────────────────────────────
    ctx.ecs_world       = world;
    ctx.window_handle   = window;
    ctx.user_data       = config->user_data;
    ctx.start_time      = Engine::Platform::GetTime();
    ctx.window_width    = config->window_width;
    ctx.window_height   = config->window_height;
    ctx.initialized     = 1;
    ctx.running         = 1;

    // ── 7. User init ──────────────────────────────────────────────────────────
    config->OnInit(&ctx);

    {
        uint64_t last_tick = Engine::Platform::GetTime();

        while (ctx.running) {
            Engine::Platform::PumpEvents(&ctx);

            if (!ctx.running) break;

            if (Engine::Input::g_input_buffer.IsKeyDown(Engine::Input::Key::Escape)) {
                ctx.running = 0;
                break;
            }

            const uint64_t now = Engine::Platform::GetTime();
            const float dt = static_cast<float>(now - last_tick) / 1000.0f;
            last_tick = now;

            Engine::Graphics::BeginFrame();

            if (!ecs_progress(world, dt)) {
                ctx.running = 0;
                break;
            }

            Engine::Graphics::Present();

            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
    }

    // ── 8. Teardown ───────────────────────────────────────────────────────────
    if (config->OnShutdown) {
        config->OnShutdown(&ctx);
    }

shutdown:
    for (int i = init_count - 1; i >= 0; --i) {
        switch (init_checklist[i]) {
            case Subsystem::Renderer:
                Engine::Renderer::Shutdown();
                break;
            case Subsystem::Flecs:
                if (world) ecs_fini(world);
                Engine::ECS::ShutdownOSAPI();
                break;
            case Subsystem::Graphics:
                Engine::Graphics::Shutdown();
                break;
            case Subsystem::Platform:
                if (window) Engine::Platform::DestroyWindow(window);
                break;
            case Subsystem::Mimalloc:
                if (global_memory_pool) {
                    mi_free(global_memory_pool);
                    global_memory_pool = nullptr;
                }
                break;
            case Subsystem::None:
            default:
                break;
        }
    }

    return ret_code;
}

ENGINE_API void RequestExit(Context* ctx) {
    DEBUG_ASSERT(ctx != nullptr);
    DEBUG_ASSERT(ctx->initialized == 1);
    if (ctx != nullptr) {
        ctx->running = 0;
    }
}

ENGINE_API void* GetUserData(const Context* ctx) {
    DEBUG_ASSERT(ctx != nullptr);
    if (ctx == nullptr) {
        return nullptr;
    }
    return ctx->user_data;
}

ENGINE_API struct ecs_world_t* GetWorld(const Context* ctx) {
    DEBUG_ASSERT(ctx != nullptr);
    if (ctx == nullptr) {
        return nullptr;
    }
    return ctx->ecs_world;
}

ENGINE_API void EngineLog(const char* msg) {
    DEBUG_ASSERT(msg != nullptr);
    if (msg != nullptr) {
        if (g_app_log) {
            g_app_log(msg);
        } else {
            std::printf("[Engine] %s\n", msg); std::fflush(stdout);
        }
    }
}

} // extern "C"
