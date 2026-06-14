#pragma once

/*
 * scry.h — Public C API for the Scry Engine.
 *
 * This is the only header an external application needs to include.
 * It is valid C11 and C++11. No C++ standard library headers are pulled in.
 */

#include <stdint.h>
#include <stdbool.h>

/* ── Export / import ─────────────────────────────────────────────────────── */

#if defined(_WIN32) || defined(__CYGWIN__)
#   ifdef SCRY_EXPORT
#       define SCRY_API __declspec(dllexport)
#   else
#       define SCRY_API __declspec(dllimport)
#   endif
#else
#   if __GNUC__ >= 4
#       define SCRY_API __attribute__((visibility("default")))
#   else
#       define SCRY_API
#   endif
#endif

/* ── Opaque engine context ───────────────────────────────────────────────── */
/*
 * ScryContext holds the engine's internal state: the Flecs world, the enkiTS
 * task scheduler, the platform window, and any other subsystem handles.
 * The full struct definition is private to the engine. External code must
 * interact with it exclusively through the API functions declared below.
 */
typedef struct ScryContext ScryContext;

/* ── Application configuration ──────────────────────────────────────────── */

typedef struct ScryAppConfig {
    const char* title;          /* Window title. NULL → "Scry Engine".          */
    int32_t     window_width;   /* Initial window width  in pixels  (must > 0). */
    int32_t     window_height;  /* Initial window height in pixels  (must > 0). */

    /* Lifecycle callbacks. OnInit and OnShutdown must be non-NULL.
       All per-frame game logic belongs in ECS systems registered during OnInit;
       the ISR pipeline (Intent → State → React → Cleanup) runs automatically
       inside ecs_progress on every frame. There is no OnUpdate callback. */
    void (*OnInit)(ScryContext* ctx);
    void (*OnShutdown)(ScryContext* ctx);

    /* Opaque application state forwarded into the context at startup.
       Retrieve it at any time with ScryGetUserData(ctx). */
    void* user_data;
} ScryAppConfig;

/* ── Engine API ──────────────────────────────────────────────────────────── */

#ifdef __cplusplus
extern "C" {
#endif

/*
 * ScryRun — start the engine.
 *
 * Initialises logging, the job scheduler, SDL, and the ECS world in that
 * order, then calls config->OnInit. Enters the main loop: each iteration pumps
 * SDL events and calls ecs_progress, which runs the full ISR pipeline, until
 * RequestEngineExit() is called or the window is closed. Calls
 * config->OnShutdown before tearing down subsystems.
 *
 * All per-frame game logic runs as ECS systems registered during OnInit.
 *
 * Blocks until the application exits.
 * Returns 0 on a clean exit; a negative value on a startup failure.
 */
SCRY_API int ScryRun(const ScryAppConfig* config);

/*
 * RequestEngineExit — signal the main loop to stop at the end of the
 * current frame. Safe to call from OnUpdate or from a plugin callback.
 */
SCRY_API void RequestEngineExit(ScryContext* ctx);

/*
 * ScryGetUserData — retrieve the user_data pointer originally set in
 * ScryAppConfig. Returns NULL if ctx is NULL or no user_data was set.
 */
SCRY_API void* ScryGetUserData(const ScryContext* ctx);

/* Engine version as a null-terminated semver string (e.g. "0.1.0"). */
SCRY_API const char* ScryGetVersion(void);

/* ScryGetWorld — returns the raw Flecs world pointer stored in ctx.
 * The caller must include <flecs.h> to use any ECS functions on the result.
 * Returns NULL if ctx is NULL. */
struct ecs_world_t;
SCRY_API struct ecs_world_t* ScryGetWorld(const ScryContext* ctx);

/* ScryLog — route a pre-formatted message through the engine's async Quill
 * logger (the one initialised in ScryRun).  Thread-safe; safe to call from
 * ECS system callbacks, observers, and plugin code.
 *
 * Game code must use this instead of LOG_INFO directly because Quill is
 * statically linked into the engine DLL.  Each binary (DLL, EXE, plugin)
 * carries its own copy of the Quill globals; only the DLL's copy is
 * initialised by ScryRun.  Calling LOG_INFO from a different binary before
 * its own quill::start() fires the _g_root_logger nullptr assertion. */
SCRY_API void ScryLog(const char* msg);

#ifdef __cplusplus
}
#endif
