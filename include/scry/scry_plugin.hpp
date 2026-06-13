#pragma once
#include <scry/core.hpp>
#include <scry/ScryEngineAPI.h>

namespace Scry {
namespace Plugin {

// Scans the "plugins/" directory, loading and initializing all plugins.
SCRY_API bool LoadPlugins(ecs_world_t* world);

// Closes and unloads all loaded plugin libraries.
SCRY_API void UnloadPlugins();

} // namespace Plugin
} // namespace Scry
