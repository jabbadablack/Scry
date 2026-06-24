#ifndef ENGINE_MATH_TRIGONOMETRY_HPP
#define ENGINE_MATH_TRIGONOMETRY_HPP

#include "algebra.hpp"


namespace engine::math {

    constexpr f32 PI = 3.14159265358979323846F;
    constexpr f32 HALF_PI = 1.57079632679489661923F;
    constexpr f32 TWO_PI = 6.28318530717958647692F;

    [[nodiscard]] ENGINE_INLINE constexpr f32 DegToRad(f32 degrees) {
        return degrees * (PI / 180.0F);
    }

    [[nodiscard]] ENGINE_INLINE constexpr f32 RadToDeg(f32 radians) {
        return radians * (180.0F / PI);
    }

    [[nodiscard]] ENGINE_INLINE f32 Abs(f32 x) {
        return x < 0.0F ? -x : x;
    }

    [[nodiscard]] ENGINE_INLINE f32 Fmod(f32 x, f32 y) {
        return x - (static_cast<i32>(x / y) * y);
    }

    [[nodiscard]] ENGINE_INLINE f32 Sqrt(f32 x) {
        if (x <= 0.0F) { return 0.0F;
}
        union { f32 f; u32 i; } conv = {x};
        conv.i = 0x5f3759df - (conv.i >> 1);
        conv.f *= 1.5F - (x * 0.5F * conv.f * conv.f);
        return x * conv.f; // x * (1 / sqrt(x)) = sqrt(x)
    }

    [[nodiscard]] ENGINE_INLINE f32 Sin(f32 x) {
        x = Fmod(x + PI, TWO_PI);
        if (x < 0.0F) x += TWO_PI;
        x -= PI;
        f32 x2 = x * x;
        return x * (1.0F - (x2 / 6.0F) + ((x2 * x2) / 120.0F) - ((x2 * x2 * x2) / 5040.0F));
    }

    [[nodiscard]] ENGINE_INLINE f32 Cos(f32 x) {
        return Sin(x + HALF_PI);
    }

    [[nodiscard]] ENGINE_INLINE f32 Tan(f32 x) {
        return Sin(x) / Cos(x);
    }

    inline void SinCos(f32 radians, f32& out_sin, f32& out_cos) noexcept {
        out_sin = Sin(radians);
        out_cos = Cos(radians);
    }

} // namespace engine::math


#endif // ENGINE_MATH_TRIGONOMETRY_HPP
