#define SDL_MAIN_HANDLED
#include <scry/scry_platform.hpp>
#include <scry/scry_input.hpp>
#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>
#include <cassert>

namespace Scry {
namespace Input {
InputBuffer g_input_buffer;
}
}

namespace Scry {
namespace Platform {

static PlatformState g_platform_state;

PlatformState GetPlatformState() {
    assert(g_platform_state.initialized <= 1);
    assert(g_platform_state.window_width >= 0);
    return g_platform_state;
}

static void ProcessInputEvent(const SDL_Event& event, Scry::Input::InputState& write_state) {
    assert(&event != nullptr);
    assert(&write_state != nullptr);

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

static bool InitPlatformWindow() {
    assert(g_platform_state.initialized == 0);
    assert(g_platform_state.window == nullptr);

    SDL_SetMainReady();

    const bool init_ok = SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS);
    if (!init_ok) {
        return false;
    }

    g_platform_state.window = SDL_CreateWindow("Scry Engine Sandbox", 800, 600, 0);
    if (!g_platform_state.window) {
        SDL_Quit();
        return false;
    }

    g_platform_state.window_width = 800;
    g_platform_state.window_height = 600;
    g_platform_state.start_time = SDL_GetTicks();
    g_platform_state.initialized = 1;

    return true;
}

static void PlatformTeardown() {
    assert(g_platform_state.initialized == 1);
    assert(g_platform_state.window != nullptr);

    SDL_DestroyWindow(g_platform_state.window);
    SDL_Quit();

    g_platform_state.window = nullptr;
    g_platform_state.initialized = 0;
}

static void ProcessInputPass(bool* running) {
    assert(running != nullptr);
    assert(g_platform_state.initialized == 1);

    const uint8_t w_idx = Scry::Input::g_input_buffer.write_index;
    const uint8_t r_idx = Scry::Input::g_input_buffer.read_index;
    Scry::Input::g_input_buffer.states[w_idx] = Scry::Input::g_input_buffer.states[r_idx];

    SDL_Event event;
    bool has_event = SDL_PollEvent(&event);
    while (has_event) {
        if (event.type == SDL_EVENT_QUIT) {
            *running = false;
        } else if (event.type == SDL_EVENT_WINDOW_CLOSE_REQUESTED) {
            *running = false;
        } else {
            ProcessInputEvent(event, Scry::Input::g_input_buffer.states[w_idx]);
        }
        has_event = SDL_PollEvent(&event);
    }

    Scry::Input::g_input_buffer.Swap();
}

bool RunEngine(ScryApp* app) {
    assert(app != nullptr);
    assert(g_platform_state.initialized == 0);
    if (app == nullptr) {
        return false;
    }

    const bool win_ok = InitPlatformWindow();
    if (!win_ok) {
        return false;
    }

    const bool app_init_ok = app->Init();
    if (!app_init_ok) {
        PlatformTeardown();
        return false;
    }

    bool running = true;
    uint64_t last_time = SDL_GetTicks();

    while (running) {
        ProcessInputPass(&running);

        const bool esc_pressed = Scry::Input::g_input_buffer.IsKeyDown(Scry::Input::Key::Escape);
        if (esc_pressed) {
            running = false;
        }

        const uint64_t current_time = SDL_GetTicks();
        const float dt = static_cast<float>(current_time - last_time) / 1000.0f;
        last_time = current_time;

        app->Tick(dt);
        SDL_Delay(1);
    }

    app->Shutdown();
    PlatformTeardown();

    return true;
}

void RequestEngineExit() {
    assert(g_platform_state.initialized == 1);
    assert(g_platform_state.window != nullptr);

    SDL_Event quit_event;
    SDL_zero(quit_event);
    quit_event.type = SDL_EVENT_QUIT;
    
    const bool push_ok = SDL_PushEvent(&quit_event);
    assert(push_ok == true);
}

} // namespace Platform
} // namespace Scry
