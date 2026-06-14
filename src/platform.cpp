#define SDL_MAIN_HANDLED
#include <engine/engine_context.hpp>
#include <engine/platform.hpp>
#include <engine/input.hpp>
#include <engine/ecs.hpp>
#include <engine/job_system.hpp>
#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>
#define QUILL_ROOT_LOGGER_ONLY
#include <quill/Quill.h>
#include <mimalloc.h>
#include <libassert/assert.hpp>

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

enum class Subsystem { None, Mimalloc, Quill, EnkiTS, SDL3, Flecs };

ENGINE_API EngineError EngineRun(const AppConfig* config) {
    DEBUG_ASSERT(config != nullptr);
    DEBUG_ASSERT(config->OnInit != nullptr);
    DEBUG_ASSERT(config->OnShutdown != nullptr);
    DEBUG_ASSERT(config->window_width > 0);
    DEBUG_ASSERT(config->window_height > 0);

    Subsystem init_checklist[16] = { Subsystem::None };
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
        DEBUG_ASSERT(global_memory_pool != nullptr);
    }

    // ── 2. Quill async logging ────────────────────────────────────────────────
    std::shared_ptr<quill::Handler> log_handler = quill::stdout_handler();
    log_handler->set_pattern("%(ascii_time) [%(thread)] %(fileline:<28) LOG_%(level_name) %(message)");
    quill::Config log_cfg;
    log_cfg.default_handlers.push_back(log_handler);
    quill::configure(log_cfg);
    quill::start();
    init_checklist[init_count++] = Subsystem::Quill;

    LOG_INFO("[Engine] mimalloc active, Quill logging started");

    // ── 3. enkiTS task scheduler ──────────────────────────────────────────────
    const bool jobs_ok = Engine::Jobs::Init();
    if (!jobs_ok) {
        LOG_INFO("[Engine] FATAL: enkiTS scheduler failed to initialize");
        ret_code = ERR_JOB_SYSTEM_INIT;
        goto shutdown;
    }
    init_checklist[init_count++] = Subsystem::EnkiTS;
    LOG_INFO("[Engine] enkiTS initialized ({} task threads)", Engine::Jobs::GetTotalThreadCount() - 1u);

    // ── 4. Platform (SDL3) ────────────────────────────────────────────────
    window = Engine::Platform::InitWindow(config->title, config->window_width, config->window_height);
    if (!window) {
        LOG_INFO("[Engine] FATAL: Window creation failed");
        ret_code = ERR_PLATFORM_INIT;
        goto shutdown;
    }
    init_checklist[init_count++] = Subsystem::SDL3;
    LOG_INFO("[Engine] Window '{}' created ({}x{})", config->title ? config->title : "Engine", config->window_width, config->window_height);

    // ── 5. Flecs ECS world ────────────────────────────────────────────────────
    world = Engine::ECS::CreateWorld();
    if (!world) {
        LOG_INFO("[Engine] FATAL: Flecs world creation failed");
        ret_code = ERR_ECS_INIT;
        goto shutdown;
    }
    init_checklist[init_count++] = Subsystem::Flecs;
    LOG_INFO("[Engine] Flecs world created");

    // ── 6. Context assembly ───────────────────────────────────────────────────
    ctx.ecs_world       = world;
    ctx.window_handle   = window;
    ctx.scheduler       = Engine::Jobs::GetScheduler();
    ctx.user_data       = config->user_data;
    ctx.start_time      = Engine::Platform::GetTime();
    ctx.window_width    = config->window_width;
    ctx.window_height   = config->window_height;
    ctx.initialized     = 1;
    ctx.running         = 1;

    // ── 7. User init ──────────────────────────────────────────────────────────
    LOG_INFO("[Engine] Calling OnInit");
    config->OnInit(&ctx);
    LOG_INFO("[Engine] OnInit complete — entering main loop");

    {
        uint64_t last_tick = Engine::Platform::GetTime();

        // ── 8. Main loop ──────────────────────────────────────────────────────────
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

    // ── 9. Teardown ───────────────────────────────────────────────────────────
    LOG_INFO("[Engine] Main loop exited — calling OnShutdown");
    if (config->OnShutdown) {
        config->OnShutdown(&ctx);
    }

shutdown:
    // NASA Rule Check: Loop backward through the init_checklist array
    for (int i = init_count - 1; i >= 0; --i) {
        switch (init_checklist[i]) {
            case Subsystem::Flecs:
                if (world) ecs_fini(world);
                LOG_INFO("[Engine] Flecs world destroyed");
                break;
            case Subsystem::SDL3:
                if (window) Engine::Platform::DestroyWindow(window);
                LOG_INFO("[Engine] Platform window shut down");
                break;
            case Subsystem::EnkiTS:
                Engine::Jobs::Shutdown();
                LOG_INFO("[Engine] enkiTS shut down — all worker threads joined");
                break;
            case Subsystem::Quill:
                quill::flush();
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
        LOG_INFO("{}", msg);
    }
}

} // extern "C"
