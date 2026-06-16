#pragma once
#include <engine/engine.h>
#include <cstdint>

namespace Engine {
namespace Graphics {

/**
 * @brief Initializes the graphics system.
 */
ENGINE_API bool Init(void* glfw_window_handle);

/**
 * @brief Shuts down the graphics system.
 */
ENGINE_API void Shutdown();

/**
 * @brief Begins a new frame.
 */
ENGINE_API void BeginFrame();

/**
 * @brief Presents the current frame to the screen.
 */
ENGINE_API void Present();

} // namespace Graphics
} // namespace Engine
