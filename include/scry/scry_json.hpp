#pragma once
#include <scry/scry.h>

namespace Scry {
namespace JSON {

// Reads the scry_project.json file, loads plugins, and initializes Flecs scene state.
SCRY_API bool LoadProjectConfig(ScryContext* ctx, const char* filepath);

} // namespace JSON
} // namespace Scry
