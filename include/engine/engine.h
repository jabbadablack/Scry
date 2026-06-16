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

/**
 * @brief Time to get things moving! This function starts up the whole engine.
 * 
 * It'll take care of all the boring stuff like logging and SDL so you can focus on your game.
 * 
 * @param config Your application's settings and callbacks.
 * @return Returns SUCCESS if everything went smoothly, or an error code if something went wrong.
 * 
 * @example
 * AppConfig config = {0};
 * config.OnInit = MyInit;
 * config.OnShutdown = MyShutdown;
 * EngineRun(&config);
 */
ENGINE_API EngineError EngineRun(const AppConfig* config);

/**
 * @brief Ready to call it a day? Use this to tell the engine to stop nicely.
 * 
 * It'll finish the current frame before shutting everything down.
 * 
 * @param ctx The engine context you want to stop.
 * 
 * @example
 * RequestExit(my_context);
 */
ENGINE_API void RequestExit(Context* ctx);

/**
 * @brief Looking for your stuff? This retrieves the user data you passed in at startup.
 * 
 * @param ctx The engine context.
 * @return A pointer to your user data, or NULL if it's not there.
 * 
 * @example
 * MyAppData* data = (MyAppData*)GetUserData(ctx);
 */
ENGINE_API void* GetUserData(const Context* ctx);

/**
 * @brief Want to know which version of Scry you're using? Just ask!
 * 
 * @return A nice string with the version number.
 * 
 * @example
 * printf("Running Scry version %s\n", EngineGetVersion());
 */
ENGINE_API const char* EngineGetVersion(void);

/**
 * @brief Gets you direct access to the Flecs world. Power at your fingertips!
 * 
 * @param ctx The engine context.
 * @return The raw Flecs world pointer.
 * 
 * @example
 * ecs_world_t* world = GetWorld(ctx);
 */
struct ecs_world_t;
ENGINE_API struct ecs_world_t* GetWorld(const Context* ctx);

/**
 * @brief Shout it from the rooftops! (Or just the log file).
 * 
 * @param msg The message you want to log.
 * 
 * @example
 * EngineLog("Something cool happened!");
 */
ENGINE_API void EngineLog(const char* msg);

#ifdef __cplusplus
}
#endif
