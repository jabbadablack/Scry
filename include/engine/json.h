#pragma once
#include <engine/engine.h>

namespace Engine {
namespace JSON {

/**
 * @brief Loads the project configuration from a JSON file.
 * 
 * Hey there! This function helps you get your project up and running by reading
 * its configuration from a specified file path.
 * 
 * @param ctx The engine context.
 * @param filepath The path to the JSON configuration file.
 * @return true if the config was loaded successfully, false otherwise.
 * 
 * @example
 * Engine::Context ctx;
 * if (Engine::JSON::LoadProjectConfig(&ctx, "assets/project.json")) {
 *     // Config loaded!
 * }
 */
ENGINE_API bool LoadProjectConfig(Context* ctx, const char* filepath);

} // namespace JSON
} // namespace Engine
