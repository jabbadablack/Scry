#define SDL_MAIN_HANDLED
#include <scry/scry_platform.hpp>
#include <scry/scry_input.hpp>
#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>


namespace Scry {
namespace Input {
// Instantiate the global input double-buffer
InputBuffer g_input_buffer;
}
}

namespace Scry {
namespace Platform {

static PlatformState g_platform_state;

PlatformState GetPlatformState() {
    return g_platform_state;
}

// Translate SDL3 events to Scry hardware input bitmask
static void ProcessInputEvent(const SDL_Event& event, Scry::Input::InputState& write_state) {
    if (event.type == SDL_EVENT_KEY_DOWN || event.type == SDL_EVENT_KEY_UP) {
        bool is_down = (event.type == SDL_EVENT_KEY_DOWN);
        uint32_t scancode = static_cast<uint32_t>(event.key.scancode);
        if (scancode < 512) {
            uint32_t idx = scancode / 64;
            uint32_t bit = scancode % 64;
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
        bool is_down = (event.type == SDL_EVENT_MOUSE_BUTTON_DOWN);
        uint32_t scancode = 0;
        
        if (event.button.button == SDL_BUTTON_LEFT) {
            scancode = 510; // MouseL
        } else if (event.button.button == SDL_BUTTON_RIGHT) {
            scancode = 511; // MouseR
        }

        if (scancode != 0) {
            uint32_t idx = scancode / 64;
            uint32_t bit = scancode % 64;
            if (is_down) {
                write_state.keys[idx] |= (1ULL << bit);
            } else {
                write_state.keys[idx] &= ~(1ULL << bit);
            }
        }
    }
}

bool RunEngine(ScryApp* app) {
    if (!app) {
        return false;
    }

    // Set SDL_MAIN_HANDLED to indicate we manage our own entry point
    SDL_SetMainReady();

    // Initialize SDL3 subsystems
    if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS)) {
        return false;
    }


    // Create abstractions layer window
    g_platform_state.window = SDL_CreateWindow("Scry Engine Sandbox", 800, 600, 0);
    if (!g_platform_state.window) {
        SDL_Quit();
        return false;
    }

    g_platform_state.window_width = 800;
    g_platform_state.window_height = 600;
    g_platform_state.start_time = SDL_GetTicks();
    g_platform_state.initialized = 1;

    // Run User Initialization
    if (app->Init) {
        if (!app->Init(app->user_data)) {
            SDL_DestroyWindow(g_platform_state.window);
            SDL_Quit();
            g_platform_state.window = nullptr;
            g_platform_state.initialized = 0;
            return false;
        }
    }

    bool running = true;
    uint64_t last_time = SDL_GetTicks();

    // Core Engine Frame Loop - ZERO heap allocations
    while (running) {
        // 1. Prepare input write buffer with baseline state from read buffer
        uint8_t w_idx = Scry::Input::g_input_buffer.write_index;
        uint8_t r_idx = Scry::Input::g_input_buffer.read_index;
        Scry::Input::g_input_buffer.states[w_idx] = Scry::Input::g_input_buffer.states[r_idx];

        // 2. Poll & process SDL3 Event Queue contiguously at the start of frame (ISR Input Pass)
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_EVENT_QUIT) {
                running = false;
            } else if (event.type == SDL_EVENT_WINDOW_CLOSE_REQUESTED) {
                running = false;
            } else {
                ProcessInputEvent(event, Scry::Input::g_input_buffer.states[w_idx]);
            }
        }

        // 3. Swap input buffers so Tick reads the stable, newly processed inputs
        Scry::Input::g_input_buffer.Swap();

        // Check if Escape key was pressed to exit
        if (Scry::Input::g_input_buffer.IsKeyDown(Scry::Input::Key::Escape)) {
            running = false;
        }

        // 4. Calculate delta time (in seconds)
        uint64_t current_time = SDL_GetTicks();
        float dt = static_cast<float>(current_time - last_time) / 1000.0f;
        last_time = current_time;

        // 5. App Tick
        if (app->Tick) {
            app->Tick(app->user_data, dt);
        }

        // Frame limiter/delay to prevent maxing out a CPU core
        SDL_Delay(1);
    }

    // Run User Shutdown
    if (app->Shutdown) {
        app->Shutdown(app->user_data);
    }

    // Cleanup platform layer
    SDL_DestroyWindow(g_platform_state.window);
    SDL_Quit();

    g_platform_state.window = nullptr;
    g_platform_state.initialized = 0;

    return true;
}

void RequestEngineExit() {
    SDL_Event quit_event;
    SDL_zero(quit_event);
    quit_event.type = SDL_EVENT_QUIT;
    SDL_PushEvent(&quit_event);
}

} // namespace Platform
} // namespace Scry
