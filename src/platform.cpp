#define SDL_MAIN_HANDLED
#include <engine/engine_context.hpp>
#include <engine/platform.hpp>
#include <engine/input.hpp>
#include <engine/ecs.hpp>
#include <engine/job_system.hpp>
#include <engine/plugin.hpp>
#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>
#include <mimalloc.h>
#include <libassert/assert.hpp>
#include <cstdio>

namespace Engine {
namespace Input {
InputBuffer g_input_buffer;
}
}

namespace Engine {
namespace Platform {

static void ProcessInputEvent(const SDL_Event& event, Engine::Input::InputState& write_state) {
    DEBUG_ASSERT(&event != nullptr);
    DEBUG_ASSERT(&write_state != nullptr);

    if (event.type == SDL_EVENT_KEY_DOWN || event.type == SDL_EVENT_KEY_UP) {
        const bool is_down = (event.type == SDL_EVENT_KEY_DOWN);
        const uint32_t scancode = static_cast<uint32_t>(event.key.scancode);
        if (scancode < 512) {
            const uint32_t idx = scancode / 64;
            const uint32_t bit = scancode % 64;
            if (is_down) {
                write_state.keys[idx] |= (1ULL << bit);
            } else {
                write_state.keys[idx] &= ~(1ULL << bit);
            }
        }
    } else if (event.type == SDL_EVENT_MOUSE_MOTION) {
        write_state.mouse_x = static_cast<int16_t>(event.motion.x);
        write_state.mouse_y = static_cast<int16_t>(event.motion.y);
    } else if (event.type == SDL_EVENT_MOUSE_BUTTON_DOWN || event.type == SDL_EVENT_MOUSE_BUTTON_UP) {
        const bool is_down = (event.type == SDL_EVENT_MOUSE_BUTTON_DOWN);
        uint32_t scancode = 0;

        if (event.button.button == SDL_BUTTON_LEFT) {
            scancode = 510;
        } else if (event.button.button == SDL_BUTTON_RIGHT) {
            scancode = 511;
        }

        if (scancode != 0) {
            const uint32_t idx = scancode / 64;
            const uint32_t bit = scancode % 64;
            if (is_down) {
                write_state.keys[idx] |= (1ULL << bit);
            } else {
                write_state.keys[idx] &= ~(1ULL << bit);
            }
        }
    }
}

void* InitWindow(const char* title, int32_t width, int32_t height) {
    SDL_SetMainReady();
    if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS)) {
        return nullptr;
    }
    return SDL_CreateWindow(title ? title : "Engine", width, height, 0);
}

void PumpEvents(Context* ctx) {
    DEBUG_ASSERT(ctx != nullptr);
    DEBUG_ASSERT(ctx->initialized == 1);

    const uint8_t w_idx = Engine::Input::g_input_buffer.write_index;
    const uint8_t r_idx = Engine::Input::g_input_buffer.read_index;
    Engine::Input::g_input_buffer.states[w_idx] = Engine::Input::g_input_buffer.states[r_idx];

    SDL_Event event;
    bool has_event = SDL_PollEvent(&event);
    while (has_event) {
        if (event.type == SDL_EVENT_QUIT) {
            ctx->running = 0;
        } else if (event.type == SDL_EVENT_WINDOW_CLOSE_REQUESTED) {
            ctx->running = 0;
        } else {
            ProcessInputEvent(event, Engine::Input::g_input_buffer.states[w_idx]);
        }
        has_event = SDL_PollEvent(&event);
    }

    Engine::Input::g_input_buffer.Swap();
}

uint64_t GetTime() {
    return SDL_GetTicks();
}

void DestroyWindow(void* window_handle) {
    if (window_handle) {
        SDL_DestroyWindow(static_cast<SDL_Window*>(window_handle));
    }
    SDL_Quit();
}

} // namespace Platform
} // namespace Engine

extern "C" {

static void (*g_app_log)(const char*) = nullptr;

enum class Subsystem { None, Mimalloc, EnkiTS, SDL3, Flecs };

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
    mi_process_init();
    init_checklist[init_count++] = Subsystem::Mimalloc;

    if (config->global_memory_pool_size > 0) {
        global_memory_pool = mi_malloc(config->global_memory_pool_size);
    }

    // ── 2. enkiTS task scheduler ──────────────────────────────────────────────
    const bool jobs_ok = Engine::Jobs::Init();
    if (!jobs_ok) {
        ret_code = ERR_JOB_SYSTEM_INIT;
        goto shutdown;
    }
    init_checklist[init_count++] = Subsystem::EnkiTS;

    // ── 3. Platform (SDL3) ────────────────────────────────────────────────
    window = Engine::Platform::InitWindow(config->title, config->window_width, config->window_height);
    if (!window) {
        ret_code = ERR_PLATFORM_INIT;
        goto shutdown;
    }
    init_checklist[init_count++] = Subsystem::SDL3;

    // ── 4. Flecs ECS world ────────────────────────────────────────────────────
    world = Engine::ECS::CreateWorld();
    if (!world) {
        ret_code = ERR_ECS_INIT;
        goto shutdown;
    }
    init_checklist[init_count++] = Subsystem::Flecs;

    // ── 5. Context assembly ───────────────────────────────────────────────────
    ctx.ecs_world       = world;
    ctx.window_handle   = window;
    ctx.scheduler       = Engine::Jobs::GetScheduler();
    ctx.user_data       = config->user_data;
    ctx.start_time      = Engine::Platform::GetTime();
    ctx.window_width    = config->window_width;
    ctx.window_height   = config->window_height;
    ctx.initialized     = 1;
    ctx.running         = 1;

    // ── 6. User init ──────────────────────────────────────────────────────────
    config->OnInit(&ctx);

    {
        uint64_t last_tick = Engine::Platform::GetTime();

        // ── 7. Main loop ──────────────────────────────────────────────────────────
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

            if (!ecs_progress(world, dt)) {
                ctx.running = 0;
                break;
            }

            SDL_Delay(1);
        }
    }

    // ── 8. Teardown ───────────────────────────────────────────────────────────
    if (config->OnShutdown) {
        config->OnShutdown(&ctx);
    }

shutdown:
    // NASA Rule Check: Loop backward through the init_checklist array
    for (int i = init_count - 1; i >= 0; --i) {
        switch (init_checklist[i]) {
            case Subsystem::Flecs:
                if (world) ecs_fini(world);
                break;
            case Subsystem::SDL3:
                if (window) Engine::Platform::DestroyWindow(window);
                break;
            case Subsystem::EnkiTS:
                Engine::Jobs::Shutdown();
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

    return static_cast<EngineError>(ret_code);
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
            printf("[Engine] %s\n", msg); fflush(stdout);
        }
    }
}

} // extern "C"
