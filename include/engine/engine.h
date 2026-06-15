#pragma once

/*
 * engine.h — Public C API for the Engine.
 *
 * This is the only header an external application needs to include.
 * It is valid C11 and C++11. No C++ standard library headers are pulled in.
 */

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

/* ── Error Enums ─────────────────────────────────────────────────────────── */

typedef enum EngineError {
    SUCCESS = 0,
    ERR_PLATFORM_INIT = -1,
    ERR_MEMORY_INIT = -2,
    ERR_JOB_SYSTEM_INIT = -3,
    ERR_ECS_INIT = -4,
    ERR_GRAPHICS_INIT = -5
} EngineError;

/* ── Opaque engine context ───────────────────────────────────────────────── */
/*
 * Context holds the engine's internal state.
 * The full struct definition is private to the engine. External code must
 * interact with it exclusively through the API functions declared below.
 */
typedef struct Context Context;

/* ── Application configuration ──────────────────────────────────────────── */

typedef struct AppConfig {
    const char* title;          /* Window title. NULL → "Engine".               */
    int32_t     window_width;   /* Initial window width  in pixels  (must > 0). */
    int32_t     window_height;  /* Initial window height in pixels  (must > 0). */

    /* Lifecycle callbacks. OnInit and OnShutdown must be non-NULL.
       All per-frame game logic belongs in ECS systems registered during OnInit;
       the ISR pipeline (Intent → State → React → Cleanup) runs automatically
       inside ecs_progress on every frame. */
    void (*OnInit)(Context* ctx);
    void (*OnShutdown)(Context* ctx);

    /* Opaque application state forwarded into the context at startup.
       Retrieve it at any time with GetUserData(ctx). */
    void* user_data;

    /* Application-provided logging callback. If NULL, engine prints to stdout. */
    void (*OnLog)(const char* msg);

    /* NASA Rule #3: The engine must pre-allocate this exact amount of memory
       at the very start of EngineRun. */
    size_t global_memory_pool_size;

    /* Worker thread count for the enkiTS task scheduler.
       0 = default minimum (1 worker thread).
       N = exactly N worker threads. */
    uint32_t thread_count;
} AppConfig;

/* ── Engine API ──────────────────────────────────────────────────────────── */

#ifdef __cplusplus
extern "C" {
#endif

/*
 * EngineRun — start the engine.
 *
 * Initialises logging, the job scheduler, SDL, and the ECS world in that
 * order, then calls config->OnInit. Enters the main loop: each iteration pumps
 * SDL events and calls ecs_progress, which runs the full ISR pipeline, until
 * RequestExit() is called or the window is closed. Calls
 * config->OnShutdown before tearing down subsystems.
 *
 * Blocks until the application exits.
 * Returns SUCCESS on a clean exit; an error enum on a startup failure.
 */
ENGINE_API EngineError EngineRun(const AppConfig* config);

/*
 * RequestExit — signal the main loop to stop at the end of the
 * current frame. Safe to call from a plugin callback.
 */
ENGINE_API void RequestExit(Context* ctx);

/*
 * GetUserData — retrieve the user_data pointer originally set in
 * AppConfig. Returns NULL if ctx is NULL or no user_data was set.
 */
ENGINE_API void* GetUserData(const Context* ctx);

/* Engine version as a null-terminated semver string (e.g. "0.1.0").
 * Named EngineGetVersion to avoid collision with WINAPI GetVersion(). */
ENGINE_API const char* EngineGetVersion(void);

/* GetWorld — returns the raw Flecs world pointer stored in ctx.
 * The caller must include <flecs.h> to use any ECS functions on the result.
 * Returns NULL if ctx is NULL. */
struct ecs_world_t;
ENGINE_API struct ecs_world_t* GetWorld(const Context* ctx);

/* EngineLog — route a pre-formatted message through the engine's async Quill
 * logger. */
ENGINE_API void EngineLog(const char* msg);

#ifdef __cplusplus
}
#endif
