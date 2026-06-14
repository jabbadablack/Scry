#pragma once
#include <Eigen/Dense>

namespace Engine {
namespace Math {

// Using Eigen::DontAlign to prevent memory alignment faults in arbitrary components/structs used by Flecs.
using ScryVec2 = Eigen::Matrix<float, 2, 1, Eigen::DontAlign>;
using ScryVec3 = Eigen::Matrix<float, 3, 1, Eigen::DontAlign>;
using ScryVec4 = Eigen::Matrix<float, 4, 1, Eigen::DontAlign>;
using ScryMat4 = Eigen::Matrix<float, 4, 4, Eigen::ColMajor | Eigen::DontAlign>;

} // namespace Math
} // namespace Engine
