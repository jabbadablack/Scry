#pragma once
#include <scry/core.hpp>
#include <flecs.h>

namespace Scry {
namespace JSON {

// Reads the scry_project.json file, loads plugins, and initializes Flecs scene state.
SCRY_API bool LoadProjectConfig(ecs_world_t* world, const char* filepath);

} // namespace JSON
} // namespace Scry
