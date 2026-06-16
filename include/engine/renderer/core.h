#pragma once
#include <engine/engine.h>
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Initializes the graphics system.
 */
ENGINE_API bool ScryGraphics_Init(void* glfw_window_handle);

/**
 * @brief Shuts down the graphics system.
 */
ENGINE_API void ScryGraphics_Shutdown(void);

/**
 * @brief Begins a new frame.
 */
ENGINE_API void ScryGraphics_BeginFrame(void);

/**
 * @brief Presents the current frame to the screen.
 */
ENGINE_API void ScryGraphics_Present(void);

#ifdef __cplusplus
}
#endif
