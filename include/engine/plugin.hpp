#pragma once
#include <engine/engine.h>
#include <engine/PluginAPI.h>

namespace Engine {
namespace Plugin {

// Scans the "plugins/" directory, loading and initializing all plugins.
ENGINE_API bool LoadPlugins(Context* ctx);

// Loads and initializes a single plugin by path.
ENGINE_API bool LoadSinglePlugin(Context* ctx, const char* filepath);

// Closes and unloads all loaded plugin libraries.
ENGINE_API void UnloadPlugins();

} // namespace Plugin
} // namespace Engine
