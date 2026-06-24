#ifndef ENGINE_MATH_INTERPOLATION_HPP
#define ENGINE_MATH_INTERPOLATION_HPP

#include "algebra.hpp"

namespace engine {
namespace math {

    template<typename T>
    [[nodiscard]] ENGINE_INLINE T Lerp(const T& a, const T& b, f32 t) {
        return a + (b - a) * t;
    }

    [[nodiscard]] ENGINE_INLINE Quaternion Slerp(const Quaternion& a, const Quaternion& b, f32 t) {
        return a.slerp(t, b);
    }

    [[nodiscard]] ENGINE_INLINE f32 Clamp(f32 value, f32 min, f32 max);
    [[nodiscard]] ENGINE_INLINE f32 Smoothstep(f32 edge0, f32 edge1, f32 x);
    [[nodiscard]] ENGINE_INLINE f32 EaseInOutQuad(f32 t);

} // namespace math
} // namespace engine

#include "interpolation.inl"

#endif // ENGINE_MATH_INTERPOLATION_HPP
