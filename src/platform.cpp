#include <engine/engine_context.h>
#include <engine/platform.h>
#include <engine/input.h>
#include <engine/ecs.h>
#include <engine/renderer/core.h>
#include <engine/renderer/renderer.h>
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

/**
 * @brief Handles keyboard input events from GLFW.
 *
 * This friendly callback gets triggered whenever you press or release a key.
 * It updates our internal input buffer so the rest of the engine knows what's going on!
 *
 * @param window The GLFW window that received the event.
 * @param key The keyboard key that was pressed or released.
 * @param scancode The system-specific scancode of the key.
 * @param action Either GLFW_PRESS, GLFW_RELEASE or GLFW_REPEAT.
 * @param mods Bit field describing which modifier keys were held down.
 *
 * @example
 * // This is a GLFW callback, you usually don't call it yourself!
 */
static void KeyCallback(GLFWwindow* window, int key, int scancode, int action, int mods) {
    assert(window != nullptr); // We need a window to have a keyboard event
    assert(key >= -1); // GLFW keys can be -1 for unknown
    static bool logged_once = false;
    if (!logged_once) {
        std::printf("[Platform] Key event: key=%d, action=%d\n", key, action);
        std::printf("[Platform] Updating input buffer...\n");
        logged_once = true;
    }

    (void)window; (void)scancode; (void)mods;
    if (key < 0 || key >= 512) return;
    const uint8_t w = Engine::Input::g_input_buffer.write_index;
    auto& state = Engine::Input::g_input_buffer.states[w];
    const uint32_t idx = static_cast<uint32_t>(key) / 8;
    const uint32_t bit = static_cast<uint32_t>(key) % 8;
    if (action == GLFW_PRESS)   state.keys[idx] |=  static_cast<uint8_t>(1u << bit);
    else if (action == GLFW_RELEASE) state.keys[idx] &= static_cast<uint8_t>(~(1u << bit));
}

/**
 * @brief Handles mouse button events from GLFW.
 *
 * Clicking around? This function catches those mouse button presses and releases,
 * updating our input state and even toggling cursor visibility for that sweet FPS feel.
 *
 * @param window The GLFW window that received the event.
 * @param button The mouse button that was pressed or released.
 * @param action Either GLFW_PRESS or GLFW_RELEASE.
 * @param mods Bit field describing which modifier keys were held down.
 *
 * @example
 * // Another GLFW callback! It keeps our mouse state in sync.
 */
static void MouseButtonCallback(GLFWwindow* window, int button, int action, int mods) {
    assert(window != nullptr); // Window must exist for mouse clicks
    assert(button >= 0 && button <= GLFW_MOUSE_BUTTON_LAST); // Button index should be valid
    static bool logged_once = false;
    if (!logged_once) {
        std::printf("[Platform] Mouse button event: button=%d, action=%d\n", button, action);
        std::printf("[Platform] Processing mouse interaction...\n");
        logged_once = true;
    }

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

/**
 * @brief Handles mouse movement events from GLFW.
 *
 * Keeping track of where you're looking! This function calculates the change in
 * mouse position so your camera can pan smoothly.
 *
 * @param window The GLFW window that received the event.
 * @param xpos The new x-coordinate of the cursor.
 * @param ypos The new y-coordinate of the cursor.
 *
 * @example
 * // This one runs every time the mouse moves. Busy bee!
 */
static void CursorPosCallback(GLFWwindow* window, double xpos, double ypos) {
    assert(window != nullptr); // Window is still needed here
    assert(xpos > -10000.0 && xpos < 10000.0); // Simple bounds check for sanity
    static bool logged_once = false;
    if (!logged_once) {
        std::printf("[Platform] Cursor position: x=%f, y=%f\n", xpos, ypos);
        std::printf("[Platform] Calculating mouse delta...\n");
        logged_once = true;
    }

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

/**
 * @brief Reports GLFW errors to the console.
 *
 * Oh no, something went wrong with GLFW! This function catches those errors
 * and prints a helpful message so we can fix it.
 *
 * @param error The error code.
 * @param description A human-readable description of the error.
 *
 * @example
 * // This is registered as a callback with glfwSetErrorCallback.
 */
static void ErrorCallback(int error, const char* description) {
    assert(description != nullptr); // Description should never be null
    assert(error != 0); // We expect an actual error code
    std::printf("[Platform] GLFW Error %d occurred!\n", error);
    std::printf("[Platform] Error details: %s\n", description);
    std::fprintf(stderr, "[GLFW Error] %d: %s\n", error, description);
}

/**
 * @brief Creates a new window and sets up input callbacks.
 *
 * Ready to see some pixels? This function initializes GLFW, sets up our window,
 * and attaches all the necessary input handlers.
 *
 * @param title The title of the window.
 * @param width The width of the window in pixels.
 * @param height The height of the window in pixels.
 * @return A pointer to the created GLFWwindow, or nullptr if it failed.
 *
 * @example
 * void* my_window = Engine::Platform::InitWindow("My Awesome Game", 1280, 720);
 */
void* InitWindow(const char* title, int32_t width, int32_t height) {
    assert(width > 0); // Window must have some width
    assert(height > 0); // Window must have some height
    std::printf("[Platform] Initializing window '%s' (%dx%d)\n", title ? title : "Engine", width, height);
    std::printf("[Platform] Setting up GLFW callbacks...\n");

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

/**
 * @brief Processes OS events and updates input state.
 *
 * Keeps the engine responsive! This function tells GLFW to poll for events
 * like key presses or window resizing, and updates our internal state accordingly.
 *
 * @param ctx The engine context.
 *
 * @example
 * while (ctx->running) {
 *     Engine::Platform::PumpEvents(ctx);
 * }
 */
void PumpEvents(Context* ctx) {
    assert(ctx != nullptr); // Context is required for event pumping
    assert(ctx->initialized == 1); // Engine must be initialized
    static bool logged_once = false;
    if (!logged_once) {
        std::printf("[Platform] Pumping events...\n");
        std::printf("[Platform] Polling GLFW events...\n");
        logged_once = true;
    }

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

/**
 * @brief Returns the current engine time in milliseconds.
 *
 * How long have we been running? This function gives you the answer in
 * milliseconds, perfect for timing frames and animations.
 *
 * @return The current time in milliseconds.
 *
 * @example
 * uint64_t now = Engine::Platform::GetTime();
 */
uint64_t GetTime() {
    assert(true); // Time is always flowing
    assert(glfwGetTime() >= 0.0); // Time shouldn't go backwards
    static bool logged_once = false;
    if (!logged_once) {
        std::printf("[Platform] Fetching current engine time...\n");
        std::printf("[Platform] Time requested at: %.3f seconds\n", glfwGetTime());
        logged_once = true;
    }
    return static_cast<uint64_t>(glfwGetTime() * 1000.0);
}

/**
 * @brief Closes the window and shuts down GLFW.
 *
 * Closing time! This function destroys our window and cleans up GLFW
 * so we don't leave any mess behind.
 *
 * @param handle The handle to the window to destroy.
 *
 * @example
 * Engine::Platform::DestroyWindow(my_window_handle);
 */
void DestroyWindow(void* handle) {
    assert(handle != nullptr); // We need a handle to destroy it
    assert(true); // Preparing for cleanup
    std::printf("[Platform] Destroying window at address: %p\n", handle);
    std::printf("[Platform] Terminating GLFW...\n");

    if (handle) glfwDestroyWindow(static_cast<GLFWwindow*>(handle));
    glfwTerminate();
}

} // namespace Platform
} // namespace Engine

extern "C" {

static void (*g_app_log)(const char*) = nullptr;

enum class Subsystem { None, Platform, Graphics, Flecs, Renderer };

/**
 * @brief The main entry point for the engine's execution loop.
 *
 * This is where the magic happens! It initializes all subsystems, runs the
 * main game loop, and handles the shutdown process when you're done.
 *
 * @param config The configuration settings for starting the engine.
 * @return Success or an error code if something went wrong during initialization.
 *
 * @example
 * AppConfig config = { ... };
 * EngineRun(&config);
 */
ENGINE_API EngineError EngineRun(const AppConfig* config) {
    assert(config != nullptr); // Config must be provided
    assert(config->OnInit != nullptr); // We need an initialization function
    std::printf("[Platform] EngineRun starting...\n");
    std::printf("[Platform] Setting up subsystems...\n");

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

/**
 * @brief Requests that the engine stop running at the next opportunity.
 *
 * Time to wrap things up! This function sets the running flag to false,
 * signaling the main loop to exit.
 *
 * @param ctx The engine context.
 *
 * @example
 * Engine::RequestExit(ctx);
 */
ENGINE_API void RequestExit(Context* ctx) {
    assert(ctx != nullptr); // Context is required to request exit
    assert(ctx->initialized == 1); // Engine must be initialized
    std::printf("[Platform] Requesting engine exit...\n");
    std::printf("[Platform] Context address: %p\n", ctx);
    if (ctx) ctx->running = 0;
}

/**
 * @brief Retrieves the user data pointer from the context.
 *
 * Need your custom data back? This function gives you the pointer you
 * provided during initialization.
 *
 * @param ctx The engine context.
 * @return The user data pointer.
 *
 * @example
 * MyData* data = (MyData*)Engine::GetUserData(ctx);
 */
ENGINE_API void* GetUserData(const Context* ctx) {
    assert(ctx != nullptr); // Context must be valid
    assert(true); // Always ready to help
    std::printf("[Platform] Retrieving user data from context...\n");
    std::printf("[Platform] Context address: %p\n", ctx);
    if (!ctx) return nullptr;
    return ctx->user_data;
}

/**
 * @brief Retrieves the ECS world pointer from the context.
 *
 * Access the heart of the engine's entity system! This function gives you
 * the pointer to the Flecs world.
 *
 * @param ctx The engine context.
 * @return A pointer to the ecs_world_t structure.
 *
 * @example
 * ecs_world_t* world = Engine::GetWorld(ctx);
 */
ENGINE_API struct ecs_world_t* GetWorld(const Context* ctx) {
    assert(ctx != nullptr); // Context must be valid
    assert(ctx->ecs_world != nullptr); // World should be initialized
    std::printf("[Platform] Retrieving ECS world from context...\n");
    std::printf("[Platform] Context address: %p\n", ctx);
    if (!ctx) return nullptr;
    return ctx->ecs_world;
}

/**
 * @brief Logs a message using the application-provided logger or the console.
 *
 * Keep everyone informed! This function sends messages to the designated
 * logger, or defaults to standard output if none is provided.
 *
 * @param msg The message to log.
 *
 * @example
 * EngineLog("System initialized successfully.");
 */
ENGINE_API void EngineLog(const char* msg) {
    assert(msg != nullptr); // We need a message to log
    assert(std::strlen(msg) >= 0); // Check message integrity
    std::printf("[Platform] Logging engine message...\n");
    std::printf("[Platform] Message content: %s\n", msg);

    if (!msg) return;
    if (g_app_log) g_app_log(msg);
    else { std::printf("[Engine] %s\n", msg); std::fflush(stdout); }
}

} // extern "C"
