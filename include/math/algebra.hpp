#ifndef ENGINE_MATH_ALGEBRA_HPP
#define ENGINE_MATH_ALGEBRA_HPP

#define ENGINE_MATH_SUB_INCLUSION
#include "math.h"
#undef ENGINE_MATH_SUB_INCLUSION

#if defined(_MSC_VER)
    #pragma warning(push)
    #pragma warning(disable : 4324)
#endif

#include <Eigen/Dense>
#include <Eigen/Geometry>

#if defined(_MSC_VER)
    #pragma warning(pop)
#endif


namespace engine::math {

    using Vector2    = Eigen::Vector2f;
    using Vector3    = Eigen::Vector3f;
    using Vector4    = Eigen::Vector4f;

    using Matrix3    = Eigen::Matrix3f;
    using Matrix4    = Eigen::Matrix4f;

    using Quaternion = Eigen::Quaternionf;

    template <typename T>
    using AlignedAllocator = Eigen::aligned_allocator<T>;

} // namespace engine::math


#ifndef ENGINE_MAKE_ALIGNED_OPERATOR_NEW
    #define ENGINE_MAKE_ALIGNED_OPERATOR_NEW EIGEN_MAKE_ALIGNED_OPERATOR_NEW
#endif

#endif // ENGINE_MATH_ALGEBRA_HPP
