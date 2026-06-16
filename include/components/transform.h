#pragma once
#include <engine/math.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// Discrete SoA components — Flecs stores each in its own contiguous array.

typedef struct ScryPosition {
    ScryVec3 value;
} ScryPosition;

typedef struct ScryRotation {
    ScryVec3 value; // Euler radians: x=pitch, y=yaw, z=roll
} ScryRotation;

typedef struct ScryScale {
    ScryVec3 value;
} ScryScale;

typedef struct ScryWorldMatrix {
    ScryMat4 value;
} ScryWorldMatrix;

// Set active=1 on any entity whose spatial data changed this frame.
// ScryTransform_System reads and clears it; avoids redundant matrix math.
typedef struct ScryDirtyMatrixIntent {
    uint8_t active;
} ScryDirtyMatrixIntent;

#ifdef __cplusplus
}
#endif
