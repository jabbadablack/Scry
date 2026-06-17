#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#ifdef SCRY_DEBUG

void DebugDraw_Init    (void* device, void* swap_chain);
void DebugDraw_Shutdown(void);

void DebugDraw_SetViewProj(const float vp[16]);
void DebugDraw_AddLine    (const float a[3], const float b[3], const float color[4]);
void DebugDraw_AddAABB    (const float mn[3], const float mx[3], const float color[4]);
void DebugDraw_Render     (void);

#else

static inline void DebugDraw_Init(void* d, void* s)                                    { (void)d; (void)s; }
static inline void DebugDraw_Shutdown(void)                                             {}
static inline void DebugDraw_SetViewProj(const float* v)                               { (void)v; }
static inline void DebugDraw_AddLine(const float* a, const float* b, const float* c)   { (void)a; (void)b; (void)c; }
static inline void DebugDraw_AddAABB(const float* mn, const float* mx, const float* c) { (void)mn; (void)mx; (void)c; }
static inline void DebugDraw_Render(void)                                               {}

#endif /* SCRY_DEBUG */

#ifdef __cplusplus
}
#endif
