#pragma once
#include <engine/engine.h>

#ifdef __cplusplus
extern "C" {
#endif

ENGINE_API void ScryThreading_Init(int num_threads);
ENGINE_API void ScryThreading_Shutdown(void);

/* Patches ecs_os_api for task pool dispatch. */
ENGINE_API void ScryThreading_SetFlecsOSAPI(void);

#ifdef __cplusplus
}
#endif
