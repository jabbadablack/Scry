#ifndef ENGINE_MATH_RANDOM_HPP
#define ENGINE_MATH_RANDOM_HPP

#include "algebra.hpp"

namespace engine {
namespace math {

    class Random {
    public:
        static ENGINE_INLINE void Initialize(u32 seed);
        
        [[nodiscard]] static ENGINE_INLINE f32 Range(f32 min, f32 max);
        [[nodiscard]] static ENGINE_INLINE i32 RangeInt(i32 min, i32 max);
        [[nodiscard]] static ENGINE_INLINE Vector3 Direction();
    };

} // namespace math
} // namespace engine

#include "random.inl"

#endif // ENGINE_MATH_RANDOM_HPP
