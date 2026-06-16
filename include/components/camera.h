#pragma once
#include <engine/math.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct ScryCamera {
    ScryVec3 position;
    float    pitch;
    float    yaw;
    float    view[16];
    float    proj[16];
    float    frustum_planes[6][4]; // pre-normalized, Vulkan [0,1] depth
} ScryCamera;

#ifdef __cplusplus
}
#endif
