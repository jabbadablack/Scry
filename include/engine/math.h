#pragma once

#ifndef CGLM_FORCE_LEFT_HANDED
#  define CGLM_FORCE_LEFT_HANDED
#endif
#ifndef CGLM_FORCE_DEPTH_ZERO_TO_ONE
#  define CGLM_FORCE_DEPTH_ZERO_TO_ONE
#endif

#include <cglm/cglm.h>
#include <cglm/struct.h>

namespace Engine {
namespace Math {

typedef vec2 ScryVec2;
typedef vec3 ScryVec3;
typedef vec4 ScryVec4;
typedef mat4 ScryMat4;

// cglm types are arrays, so sizeof is straightforward.
// mat4 is 16 floats = 64 bytes. vec3 is 3 floats = 12 bytes.
static_assert(sizeof(ScryVec2) ==  8, "ScryVec2 padding leak");
static_assert(sizeof(ScryVec3) == 12, "ScryVec3 padding leak");
static_assert(sizeof(ScryVec4) == 16, "ScryVec4 padding leak");
static_assert(sizeof(ScryMat4) == 64, "ScryMat4 padding leak");

} // namespace Math
} // namespace Engine
