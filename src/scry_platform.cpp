#define SDL_MAIN_HANDLED
#include <scry/scry_platform.hpp>
#include <scry/scry_input.hpp>
#include <scry/scry_ecs.hpp>
#include <scry/scry_job_system.hpp>
#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>
#define QUILL_ROOT_LOGGER_ONLY
#include <quill/Quill.h>
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
    DEBUG_ASSERT(config->OnUpdate != nullptr);
    DEBUG_ASSERT(config->OnShutdown != nullptr);
    DEBUG_ASSERT(config->window_width > 0);
    DEBUG_ASSERT(config->window_height > 0);

    // Initialize Quill Asynchronous Logging
    std::shared_ptr<quill::Handler> handler = quill::stdout_handler();
    handler->set_pattern("%(ascii_time) [%(thread)] %(fileline:<28) LOG_%(level_name) %(message)");
    quill::Config cfg;
    cfg.default_handlers.push_back(handler);
    quill::configure(cfg);
    quill::start();

    SDL_SetMainReady();
    const bool init_ok = SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS);
    if (!init_ok) {
        return -1;
    }

    SDL_Window* window = SDL_CreateWindow(config->app_name ? config->app_name : "Scry Engine",
                                         config->window_width,
                                         config->window_height,
                                         0);
    if (!window) {
        SDL_Quit();
        return -2;
    }

    const bool jobs_ok = Scry::Jobs::Init();
    if (!jobs_ok) {
        SDL_DestroyWindow(window);
        SDL_Quit();
        return -3;
    }

    struct ecs_world_t* world = Scry::ECS::CreateWorld();
    if (!world) {
        Scry::Jobs::Shutdown();
        SDL_DestroyWindow(window);
        SDL_Quit();
        return -4;
    }

    LOG_INFO("[Engine] Flecs world bound to enkiTS scheduler ({} workers)",
             static_cast<int32_t>(Scry::Jobs::GetTotalThreadCount()));

    ScryContext ctx = {};
    ctx.ecs_world = world;
    ctx.window = window;
    ctx.start_time = SDL_GetTicks();
    ctx.window_width = config->window_width;
    ctx.window_height = config->window_height;
    ctx.initialized = 1;
    ctx.running = 1;

    config->OnInit(&ctx);

    uint64_t last_time = SDL_GetTicks();

    while (ctx.running) {
        Scry::Platform::ProcessInputPass(&ctx);

        const bool esc_pressed = Scry::Input::g_input_buffer.IsKeyDown(Scry::Input::Key::Escape);
        if (esc_pressed) {
            ctx.running = 0;
        }

        const uint64_t current_time = SDL_GetTicks();
        const float dt = static_cast<float>(current_time - last_time) / 1000.0f;
        last_time = current_time;

        config->OnUpdate(&ctx, dt);
        SDL_Delay(1);
    }

    config->OnShutdown(&ctx);

    ecs_fini(world);
    Scry::Jobs::Shutdown();
    SDL_DestroyWindow(window);
    SDL_Quit();

    return 0;
}

SCRY_API void RequestEngineExit(ScryContext* ctx) {
    DEBUG_ASSERT(ctx != nullptr);
    DEBUG_ASSERT(ctx->initialized == 1);

    if (ctx != nullptr) {
        ctx->running = 0;
    }
}

} // extern "C"
