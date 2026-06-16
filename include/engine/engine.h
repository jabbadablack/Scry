#pragma once

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

/* ── Export / import ─────────────────────────────────────────────────────── */

#if defined(_WIN32) || defined(__CYGWIN__)
#   ifdef ENGINE_EXPORT
#       define ENGINE_API __declspec(dllexport)
#   else
#       define ENGINE_API __declspec(dllimport)
#   endif
#else
#   if __GNUC__ >= 4
#       define ENGINE_API __attribute__((visibility("default")))
#   else
#       define ENGINE_API
#   endif
#endif

#ifdef __cplusplus
extern "C" {
#endif

/* ── Error Enums ─────────────────────────────────────────────────────────── */

typedef enum ScryError {
    SCRY_SUCCESS = 0,
    SCRY_ERR_PLATFORM_INIT = -1,
    SCRY_ERR_MEMORY_INIT = -2,
    SCRY_ERR_JOB_SYSTEM_INIT = -3,
    SCRY_ERR_ECS_INIT = -4,
    SCRY_ERR_GRAPHICS_INIT = -5
} ScryError;

/* ── Engine context ──────────────────────────────────────────────────────── */
struct ecs_world_t;

typedef struct ScryContext {
    struct ecs_world_t*  ecs_world;
    void*                window_handle;
    void*                user_data;
    uint64_t             start_time;
    int32_t              window_width;
    int32_t              window_height;
    uint8_t              initialized;
    uint8_t              running;
} ScryContext;

/* ── Application configuration ──────────────────────────────────────────── */

typedef struct ScryAppConfig {
    const char* title;          /* Window title. NULL → "Engine".               */
    int32_t     window_width;   /* Initial window width  in pixels  (must > 0). */
    int32_t     window_height;  /* Initial window height in pixels  (must > 0). */

    void (*OnInit)(ScryContext* ctx);
    void (*OnShutdown)(ScryContext* ctx);

    void* user_data;
    void (*OnLog)(const char* msg);
    size_t global_memory_pool_size;
    uint32_t thread_count;
} ScryAppConfig;

/* ── Engine API ──────────────────────────────────────────────────────────── */

ENGINE_API ScryError Scry_Run(const ScryAppConfig* config);
ENGINE_API void      Scry_RequestExit(ScryContext* ctx);
ENGINE_API void*     Scry_GetUserData(const ScryContext* ctx);
ENGINE_API const char* Scry_GetVersion(void);

ENGINE_API struct ecs_world_t* Scry_GetWorld(const ScryContext* ctx);

ENGINE_API void Scry_Log(const char* msg);

#ifdef __cplusplus
}
#endif
