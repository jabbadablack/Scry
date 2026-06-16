#pragma once
#include <engine/math.h>
#include <cstdint>

namespace Engine {
namespace Transform {

// Discrete SoA components — Flecs stores each in its own contiguous array.
// A system iterating only Position touches a pure dense Vec3 array.

struct Position          { Math::ScryVec3 value; };
struct Rotation          { Math::ScryVec3 value; };  // Euler radians: x=pitch, y=yaw, z=roll
struct Scale             { Math::ScryVec3 value; };
struct WorldMatrix       { Math::ScryMat4 value; };

// Set active=1 on any entity whose spatial data changed this frame.
// TransformSystem reads and clears it; avoids redundant matrix math.
struct DirtyMatrixIntent { uint8_t active; };

} // namespace Transform
} // namespace Engine
