#pragma once
#include <engine/engine.h>
#include <engine/PluginAPI.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Discover and load all plugins.
 */
ENGINE_API bool ScryPlugin_LoadPlugins(ScryContext* ctx);

/**
 * @brief Load a specific plugin from a file.
 */
ENGINE_API bool ScryPlugin_LoadSinglePlugin(ScryContext* ctx, const char* filepath);

/**
 * @brief Unload all plugins.
 */
ENGINE_API void ScryPlugin_UnloadPlugins(void);

#ifdef __cplusplus
}
#endif
