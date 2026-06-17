#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#ifdef SCRY_DEBUG

void DebugUI_Init(void* window, void* device, void* context);
void DebugUI_Toggle(void);
void DebugUI_NewFrame(void);
void DebugUI_Render(void);
void DebugUI_Shutdown(void);

#else

static inline void DebugUI_Init(void* w, void* d, void* c) { (void)w; (void)d; (void)c; }
static inline void DebugUI_Toggle(void)   {}
static inline void DebugUI_NewFrame(void) {}
static inline void DebugUI_Render(void)   {}
static inline void DebugUI_Shutdown(void) {}

#endif /* SCRY_DEBUG */

#ifdef __cplusplus
}
#endif
