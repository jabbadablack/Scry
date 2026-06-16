#pragma once
#include <engine/engine.h>
#include <engine/pipeline.h>
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

struct ecs_world_t;

/**
 * @brief Initializes the OS-specific API for ECS.
 */
ENGINE_API void ScryECS_InitOSAPI(void);

/**
 * @brief Creates a new ECS world.
 */
ENGINE_API struct ecs_world_t* ScryECS_CreateWorld(void);

/**
 * @brief Shuts down the OS-specific API for ECS.
 */
ENGINE_API void ScryECS_ShutdownOSAPI(void);

/**
 * @brief Simple double buffering structure.
 * 
 * We use macros in C to emulate the template functionality.
 */
#define SCRY_ECS_DOUBLE_BUFFERED(T) \
    struct { \
        T read; \
        T write; \
    }

/**
 * @brief Registers a synchronization system for double-buffered components.
 * 
 * Since we can't use templates in C, we'll provide a generic function that takes
 * the component size and ID.
 */
ENGINE_API void ScryECS_RegisterDoubleBufferSync(struct ecs_world_t* world, uint64_t component_id, size_t component_size);

#ifdef __cplusplus
}
#endif
