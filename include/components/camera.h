#pragma once
#include <engine/math.h>

namespace Engine {
namespace Camera {

struct Camera {
    Math::ScryVec3 position;
    float          pitch;
    float          yaw;
    float          view[16];
    float          proj[16];
};

} // namespace Camera
} // namespace Engine
