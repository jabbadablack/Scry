#pragma once
#include <scry/scry.h>
#include <scry/ScryEngineAPI.h>

namespace Scry {
namespace Plugin {

// Scans the "plugins/" directory, loading and initializing all plugins.
SCRY_API bool LoadPlugins(ScryContext* ctx);

// Loads and initializes a single plugin by path.
SCRY_API bool LoadSinglePlugin(ScryContext* ctx, const char* filepath);

// Closes and unloads all loaded plugin libraries.
SCRY_API void UnloadPlugins();

} // namespace Plugin
} // namespace Scry
