#pragma once
#include <engine/engine.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Loads the project configuration from a JSON file.
 */
ENGINE_API bool ScryJSON_LoadProjectConfig(ScryContext* ctx, const char* filepath);

#ifdef __cplusplus
}
#endif
