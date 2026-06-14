#pragma once
#include <Eigen/Dense>

namespace Engine {
namespace Math {

// Using Eigen::DontAlign to prevent memory alignment faults in arbitrary components/structs used by Flecs.
using ScryVec2 = Eigen::Matrix<float, 2, 1, Eigen::DontAlign>;
using ScryVec3 = Eigen::Matrix<float, 3, 1, Eigen::DontAlign>;
using ScryVec4 = Eigen::Matrix<float, 4, 1, Eigen::DontAlign>;
using ScryMat4 = Eigen::Matrix<float, 4, 4, Eigen::ColMajor | Eigen::DontAlign>;

static_assert(sizeof(ScryVec2) ==  8, "ScryVec2 padding leak");
static_assert(sizeof(ScryVec3) == 12, "ScryVec3 padding leak");
static_assert(sizeof(ScryVec4) == 16, "ScryVec4 padding leak");
static_assert(sizeof(ScryMat4) == 64, "ScryMat4 padding leak");

} // namespace Math
} // namespace Engine
