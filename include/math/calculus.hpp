#ifndef ENGINE_MATH_CALCULUS_HPP
#define ENGINE_MATH_CALCULUS_HPP

#include "algebra.hpp"

namespace engine {
namespace math {

    class Integrator {
    public:
        static ENGINE_INLINE void SemiImplicitEuler(Vector3& position, Vector3& velocity, const Vector3& acceleration, f32 dt);
        static ENGINE_INLINE void RK4(Vector3& position, Vector3& velocity, const Vector3& acceleration, f32 dt);
    };

} // namespace math
} // namespace engine

#include "calculus.inl"

#endif // ENGINE_MATH_CALCULUS_HPP
