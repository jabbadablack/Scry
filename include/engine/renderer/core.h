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
ENGINE_API bool  ScryGraphics_Init(void* glfw_window_handle);
ENGINE_API void  ScryGraphics_Shutdown(void);
ENGINE_API void  ScryGraphics_BeginFrame(void);
ENGINE_API void  ScryGraphics_Present(void);
ENGINE_API void* ScryGraphics_GetDevice(void);
ENGINE_API void* ScryGraphics_GetContext(void);
ENGINE_API void* ScryGraphics_GetSwapChain(void);

#ifdef __cplusplus
}
#endif
