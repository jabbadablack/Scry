#pragma once
#include <engine/engine.h>
#include <engine/PluginAPI.h>

namespace Engine {
namespace Plugin {

/**
 * @brief Controls the engine's internal logger status.
 *
 * Hello! Use this to toggle the Quill logger. It's helpful if you
 * want to pause or resume logging at runtime.
 *
 * @param active Set to true to enable logging, false to silence it.
 *
 * @example
 * Engine::Plugin::SetQuillActive(true);
 */
ENGINE_API void SetQuillActive(bool active);

/**
 * @brief Discovers and loads all plugins for the engine.
 *
 * Welcome! This function scans for available plugins and brings them
 * into the engine's fold. It's like inviting all your friends to a party!
 *
 * @param ctx The engine context to load plugins into.
 * @return True if everything went smoothly, false otherwise.
 *
 * @example
 * Context* ctx = ...;
 * if (Engine::Plugin::LoadPlugins(ctx)) {
 *     // All friends are here!
 * }
 */
ENGINE_API bool LoadPlugins(Context* ctx);

/**
 * @brief Loads a specific plugin from a file.
 *
 * Hi there! If you have a specific plugin you'd like to invite,
 * just give this function the path to its file.
 *
 * @param ctx The engine context to load the plugin into.
 * @param filepath The path to the plugin's library file.
 * @return True if the plugin was loaded successfully.
 *
 * @example
 * Engine::Plugin::LoadSinglePlugin(ctx, "path/to/my_plugin.dll");
 */
ENGINE_API bool LoadSinglePlugin(Context* ctx, const char* filepath);

/**
 * @brief Unloads all currently active plugins.
 *
 * Goodbye for now! This function cleans up and unloads all plugins
 * that were previously loaded. Always good to say goodbye properly!
 *
 * @example
 * Engine::Plugin::UnloadPlugins();
 */
ENGINE_API void UnloadPlugins();

} // namespace Plugin
} // namespace Engine
