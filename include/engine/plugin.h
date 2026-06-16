#pragma once
#include <engine/engine.h>
#include <engine/PluginAPI.h>

namespace Engine {
namespace Plugin {

ENGINE_API void SetQuillActive(bool active);
ENGINE_API bool LoadPlugins(Context* ctx);
ENGINE_API bool LoadSinglePlugin(Context* ctx, const char* filepath);
ENGINE_API void UnloadPlugins();

} // namespace Plugin
} // namespace Engine
