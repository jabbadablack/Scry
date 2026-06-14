#pragma once
#include <engine/engine.h>

namespace Engine {
namespace JSON {

// Reads the project.json file, loads plugins, and initializes Flecs scene state.
ENGINE_API bool LoadProjectConfig(Context* ctx, const char* filepath);

} // namespace JSON
} // namespace Engine
