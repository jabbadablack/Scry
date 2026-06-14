#define SDL_MAIN_HANDLED
#include <scry/scry_platform.hpp>
#include <scry/scry_input.hpp>
#include <scry/scry_ecs.hpp>
#include <scry/scry_job_system.hpp>
#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>
#define QUILL_ROOT_LOGGER_ONLY
#include <quill/Quill.h>
#include <mimalloc.h>
#include <libassert/assert.hpp>

namespace Scry {
namespace Input {
InputBuffer g_input_buffer;
}
}

namespace Scry {
namespace Platform {

static void ProcessInputEvent(const SDL_Event& event, Scry::Input::InputState& write_state) {
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

static void ProcessInputPass(ScryContext* ctx) {
    DEBUG_ASSERT(ctx != nullptr);
    DEBUG_ASSERT(ctx->initialized == 1);

    const uint8_t w_idx = Scry::Input::g_input_buffer.write_index;
    const uint8_t r_idx = Scry::Input::g_input_buffer.read_index;
    Scry::Input::g_input_buffer.states[w_idx] = Scry::Input::g_input_buffer.states[r_idx];

    SDL_Event event;
    bool has_event = SDL_PollEvent(&event);
    while (has_event) {
        if (event.type == SDL_EVENT_QUIT) {
            ctx->running = 0;
        } else if (event.type == SDL_EVENT_WINDOW_CLOSE_REQUESTED) {
            ctx->running = 0;
        } else {
            ProcessInputEvent(event, Scry::Input::g_input_buffer.states[w_idx]);
        }
        has_event = SDL_PollEvent(&event);
    }

    Scry::Input::g_input_buffer.Swap();
}

} // namespace Platform
} // namespace Scry

extern "C" {

SCRY_API int ScryRun(const ScryAppConfig* config) {
    DEBUG_ASSERT(config != nullptr);
    DEBUG_ASSERT(config->OnInit != nullptr);
    DEBUG_ASSERT(config->OnShutdown != nullptr);
    DEBUG_ASSERT(config->window_width > 0);
    DEBUG_ASSERT(config->window_height > 0);

    // ── 1. mimalloc ──────────────────────────────────────────────────────────
    // Explicit process init ensures the heap and arena metadata are fully ready
    // before any subsystem allocates. With MI_OVERRIDE ON this is already done
    // at DLL load time; calling it again is a documented no-op.
    mi_process_init();

    // ── 2. Quill async logging ────────────────────────────────────────────────
    // Must start before any LOG_* macro is used. The backend thread runs for the
    // lifetime of the process; quill::flush() drains it before we return.
    std::shared_ptr<quill::Handler> log_handler = quill::stdout_handler();
    log_handler->set_pattern(
        "%(ascii_time) [%(thread)] %(fileline:<28) LOG_%(level_name) %(message)");
    quill::Config log_cfg;
    log_cfg.default_handlers.push_back(log_handler);
    quill::configure(log_cfg);
    quill::start();

    LOG_INFO("[Engine] mimalloc active, Quill logging started");

    // ── 3. enkiTS task scheduler ──────────────────────────────────────────────
    // Must be running before Flecs so that CreateWorld can call ecs_set_task_threads
    // and the task_new_/task_join_ OS-API hooks resolve to live workers.
    const bool jobs_ok = Scry::Jobs::Init();
    DEBUG_ASSERT(jobs_ok);
    if (!jobs_ok) {
        LOG_INFO("[Engine] FATAL: enkiTS scheduler failed to initialize");
        return -2;
    }
    LOG_INFO("[Engine] enkiTS initialized ({} task threads)",
             Scry::Jobs::GetTotalThreadCount() - 1u);

    // ── 4. SDL3 platform layer ────────────────────────────────────────────────
    SDL_SetMainReady();
    const bool sdl_ok = SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS);
    if (!sdl_ok) {
        LOG_INFO("[Engine] FATAL: SDL_Init failed — {}", SDL_GetError());
        Scry::Jobs::Shutdown();
        return -3;
    }

    SDL_Window* window = SDL_CreateWindow(
        config->title ? config->title : "Scry Engine",
        config->window_width,
        config->window_height,
        0);
    if (!window) {
        LOG_INFO("[Engine] FATAL: SDL_CreateWindow failed — {}", SDL_GetError());
        SDL_Quit();
        Scry::Jobs::Shutdown();
        return -4;
    }
    LOG_INFO("[Engine] SDL3 window '{}' created ({}x{})",
             config->title ? config->title : "Scry Engine",
             config->window_width, config->window_height);

    // ── 5. Flecs ECS world ────────────────────────────────────────────────────
    // CreateWorld installs the custom OS-API (mimalloc allocator, enkiTS task
    // hooks, SDL3 mutexes/condvars), imports FlecsMeta, builds the 6-phase
    // custom pipeline, and calls ecs_set_task_threads to bind to enkiTS.
    struct ecs_world_t* world = Scry::ECS::CreateWorld();
    DEBUG_ASSERT(world != nullptr);
    if (!world) {
        LOG_INFO("[Engine] FATAL: Flecs world creation failed");
        SDL_DestroyWindow(window);
        SDL_Quit();
        Scry::Jobs::Shutdown();
        return -5;
    }
    LOG_INFO("[Engine] Flecs world created and bound to enkiTS ({} total threads)",
             static_cast<int32_t>(Scry::Jobs::GetTotalThreadCount()));

    // ── 6. Context assembly ───────────────────────────────────────────────────
    // ScryContext is defined in scry_platform.hpp (internal only). External
    // code sees only the opaque forward declaration from scry.h.
    ScryContext ctx     = {};
    ctx.ecs_world       = world;
    ctx.window          = window;
    ctx.scheduler       = Scry::Jobs::GetScheduler();
    ctx.user_data       = config->user_data;
    ctx.start_time      = SDL_GetTicks();
    ctx.window_width    = config->window_width;
    ctx.window_height   = config->window_height;
    ctx.initialized     = 1;
    ctx.running         = 1;

    // ── 7. User init ──────────────────────────────────────────────────────────
    LOG_INFO("[Engine] Calling OnInit");
    config->OnInit(&ctx);
    LOG_INFO("[Engine] OnInit complete — entering main loop");

    uint64_t last_tick = SDL_GetTicks();

    // ── 8. Main loop ──────────────────────────────────────────────────────────
    // Each iteration:
    //   a) SDL event pump → fills the double-buffered input state
    //   b) Escape / window-close quick exit
    //   c) ecs_progress — runs the full 6-phase pipeline:
    //        ScryPhase_Input → Intent → StateUpdate → StateSync → React → Cleanup
    //   d) config->OnUpdate (optional) — per-frame user code that sees the
    //        fully-ticked ECS state (double buffers synced, Intents cleaned up)
    while (ctx.running) {
        Scry::Platform::ProcessInputPass(&ctx);

        if (!ctx.running) {
            break;
        }

        if (Scry::Input::g_input_buffer.IsKeyDown(Scry::Input::Key::Escape)) {
            ctx.running = 0;
            break;
        }

        const uint64_t now = SDL_GetTicks();
        const float dt = static_cast<float>(now - last_tick) / 1000.0f;
        last_tick = now;

        // Step the ECS pipeline. Returns false only if ecs_quit() was called
        // from inside a system, which we treat as a clean exit signal.
        if (!ecs_progress(world, dt)) {
            ctx.running = 0;
            break;
        }

        // Per-frame user callback — executes after the full ECS pipeline tick
        // so it observes stable, synced state. OnUpdate is optional; games that
        // drive all logic through ECS systems may leave it null.
        if (config->OnUpdate) {
            config->OnUpdate(&ctx, dt);
        }

        SDL_Delay(1);
    }

    // ── 9. Teardown ───────────────────────────────────────────────────────────
    LOG_INFO("[Engine] Main loop exited — calling OnShutdown");
    if (config->OnShutdown) {
        config->OnShutdown(&ctx);
    }

    // Reverse init order: Flecs → SDL → enkiTS.
    // ecs_fini drains all in-flight tasks (task_join_) before enkiTS is shut down.
    ecs_fini(world);
    LOG_INFO("[Engine] Flecs world destroyed");

    SDL_DestroyWindow(window);
    SDL_Quit();
    LOG_INFO("[Engine] SDL3 shut down");

    Scry::Jobs::Shutdown();
    LOG_INFO("[Engine] enkiTS shut down — all worker threads joined");

    quill::flush();
    return 0;
}

SCRY_API void RequestEngineExit(ScryContext* ctx) {
    DEBUG_ASSERT(ctx != nullptr);
    DEBUG_ASSERT(ctx->initialized == 1);
    if (ctx != nullptr) {
        ctx->running = 0;
    }
}

SCRY_API void* ScryGetUserData(const ScryContext* ctx) {
    DEBUG_ASSERT(ctx != nullptr);
    if (ctx == nullptr) {
        return nullptr;
    }
    return ctx->user_data;
}

SCRY_API struct ecs_world_t* ScryGetWorld(const ScryContext* ctx) {
    DEBUG_ASSERT(ctx != nullptr);
    if (ctx == nullptr) {
        return nullptr;
    }
    return ctx->ecs_world;
}

SCRY_API void ScryLog(const char* msg) {
    DEBUG_ASSERT(msg != nullptr);
    if (msg != nullptr) {
        LOG_INFO("{}", msg);
    }
}

} // extern "C"
