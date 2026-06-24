#ifndef ENGINE_MATH_INTERPOLATION_INL
#define ENGINE_MATH_INTERPOLATION_INL

namespace engine {
namespace math {

    ENGINE_INLINE f32 Clamp(f32 value, f32 min, f32 max) {
        return (value < min) ? min : (value > max) ? max : value;
    }

    ENGINE_INLINE f32 Smoothstep(f32 edge0, f32 edge1, f32 x) {
        x = Clamp((x - edge0) / (edge1 - edge0), 0.0f, 1.0f);
        return x * x * (3.0f - 2.0f * x);
    }

    ENGINE_INLINE f32 EaseInOutQuad(f32 t) {
        if (t < 0.5f) return 2.0f * t * t;
        f32 inv = -2.0f * t + 2.0f;
        return 1.0f - (inv * inv) / 2.0f;
    }

} // namespace math
} // namespace engine

#endif // ENGINE_MATH_INTERPOLATION_INL
