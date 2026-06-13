#pragma once
#include <scry/core.hpp>
#include <scry/scry_platform.hpp>

namespace Scry {
namespace JSON {

// Reads the scry_project.json file, loads plugins, and initializes Flecs scene state.
SCRY_API bool LoadProjectConfig(ScryContext* ctx, const char* filepath);

} // namespace JSON
} // namespace Scry
