#ifndef ENGINE_MATH_RANDOM_INL
#define ENGINE_MATH_RANDOM_INL

#include "trigonometry.hpp"


namespace engine::math {

    inline thread_local u32 s[4] = { 123456789, 362436069, 521288629, 88675123 };

    inline u32 rotl(const u32 x, int k) {
        return (x << k) | (x >> (32 - k));
    }

    inline u32 next_u32() {
        const u32 result = rotl(s[1] * 5, 7) * 9;
        const u32 t = s[1] << 9;
        s[2] ^= s[0]; s[3] ^= s[1]; s[1] ^= s[2]; s[0] ^= s[3];
        s[2] ^= t; s[3] = rotl(s[3], 11);
        return result;
    }

    ENGINE_INLINE void Random::Initialize(u32 seed) {
        u32 val = seed;
        for (int i = 0; i < 4; ++i) {
            val += 0x9e3779b9;
            u32 z = val;
            z = (z ^ (z >> 16)) * 0x85ebca6b;
            z = (z ^ (z >> 13)) * 0xc2b2ae35;
            s[i] = z ^ (z >> 16);
        }
        if (s[0] == 0 && s[1] == 0 && s[2] == 0 && s[3] == 0) {
            s[0] = 123456789;
            s[1] = 362436069;
            s[2] = 521288629;
            s[3] = 88675123;
        }
    }

    ENGINE_INLINE f32 Random::Range(f32 min, f32 max) {
        return min + (max - min) * (static_cast<f32>(next_u32()) / static_cast<f32>(0xFFFFFFFF));
    }

    ENGINE_INLINE i32 Random::RangeInt(i32 min, i32 max) {
        return min + (next_u32() % (max - min + 1));
    }

    ENGINE_INLINE Vector3 Random::Direction() {
        f32 theta = Random::Range(0.0f, TWO_PI);
        f32 z = Random::Range(-1.0f, 1.0f);
        f32 radius = engine::math::Sqrt(1.0f - z * z);
        return Vector3(radius * engine::math::Cos(theta), radius * engine::math::Sin(theta), z);
    }

} // namespace engine::math


#endif // ENGINE_MATH_RANDOM_INL
