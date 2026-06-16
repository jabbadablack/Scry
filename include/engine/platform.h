#pragma once
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

struct ScryContext;

/**
 * @brief Creates a window.
 */
void* ScryPlatform_InitWindow(const char* title, int32_t width, int32_t height);

/**
 * @brief Processes OS events.
 */
void ScryPlatform_PumpEvents(struct ScryContext* ctx);

/**
 * @brief Grabs the current time in high-resolution format.
 */
uint64_t ScryPlatform_GetTime(void);

/**
 * @brief Waves goodbye to our window.
 */
void ScryPlatform_DestroyWindow(void* window_handle);

/**
 * @brief Cross-platform sleep.
 */
void Scry_Sleep(uint32_t ms);

#ifdef __cplusplus
}
#endif
