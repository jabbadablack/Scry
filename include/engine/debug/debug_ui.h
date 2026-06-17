#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#ifdef SCRY_DEBUG

void DebugUI_Init     (void* window, void* device, void* context);
void DebugUI_Shutdown (void);
void DebugUI_Toggle   (void);
void DebugUI_SetWorld (void* ecs_world);  // optional: enables ECS stat panel
bool DebugUI_IsVisible(void);
void DebugUI_NewFrame (void);
void DebugUI_Render   (void);

#else

static inline void DebugUI_Init     (void* w, void* d, void* c)  { (void)w; (void)d; (void)c; }
static inline void DebugUI_Shutdown (void)                        {}
static inline void DebugUI_Toggle   (void)                        {}
static inline void DebugUI_SetWorld (void* w)                     { (void)w; }
static inline bool DebugUI_IsVisible(void)                        { return false; }
static inline void DebugUI_NewFrame (void)                        {}
static inline void DebugUI_Render   (void)                        {}

#endif /* SCRY_DEBUG */

#ifdef __cplusplus
}
#endif
