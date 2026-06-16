#pragma once
#include <stdint.h>

namespace Engine {
namespace Platform {

/**
 * @brief Creates a shiny new window for our engine to live in.
 *
 * Hello! This function opens up a window on your desktop so we can
 * start showing you some cool graphics. Just tell it how big you want
 * it to be and what to call it!
 *
 * @param title The friendly name for our new window.
 * @param width How wide the window should be in pixels.
 * @param height How tall the window should be in pixels.
 * @return A handle to the newly created window, or NULL if something went wrong.
 *
 * @example
 * void* window = Engine::Platform::InitWindow("My Awesome Game", 1280, 720);
 */
void* InitWindow(const char* title, int32_t width, int32_t height);

/**
 * @brief Keeps the engine's heart beating by processing OS events.
 *
 * Hi there! Call this every frame to make sure we're listening to
 * what the user is doing (like moving the mouse or closing the window).
 *
 * @param ctx The engine context to update with new events.
 *
 * @example
 * while (true) {
 *     Engine::Platform::PumpEvents(ctx);
 * }
 */
void PumpEvents(struct Context* ctx);

/**
 * @brief Grabs the current time in a high-resolution format.
 *
 * Greetings! If you need to know exactly what time it is (in nanoseconds!),
 * this function is your best friend. Great for timing things!
 *
 * @return The current system time in nanoseconds.
 *
 * @example
 * uint64_t start = Engine::Platform::GetTime();
 * // ... do some work ...
 * uint64_t end = Engine::Platform::GetTime();
 */
uint64_t GetTime();

/**
 * @brief Waves goodbye to our window and cleans up its resources.
 *
 * Farewell, window! Use this when you're done with the application
 * to make sure everything is shut down properly.
 *
 * @param window_handle The handle of the window you want to close.
 *
 * @example
 * Engine::Platform::DestroyWindow(window);
 */
void DestroyWindow(void* window_handle);

} // namespace Platform
} // namespace Engine
