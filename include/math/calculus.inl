#ifndef ENGINE_MATH_CALCULUS_INL
#define ENGINE_MATH_CALCULUS_INL


namespace engine::math {

    ENGINE_INLINE void Integrator::SemiImplicitEuler(Vector3& position, Vector3& velocity, const Vector3& acceleration, f32 dt) {
        velocity += acceleration * dt;
        position += velocity * dt;
    }

    ENGINE_INLINE void Integrator::RK4(Vector3& position, Vector3& velocity, const Vector3& acceleration, f32 dt) {
        Vector3 k1_vel = acceleration * dt;
        Vector3 k1_pos = velocity * dt;

        Vector3 k2_vel = acceleration * dt; // Assuming constant accel for simplicity
        Vector3 k2_pos = (velocity + k1_vel * 0.5f) * dt;

        Vector3 k3_vel = acceleration * dt;
        Vector3 k3_pos = (velocity + k2_vel * 0.5f) * dt;

        Vector3 k4_vel = acceleration * dt;
        Vector3 k4_pos = (velocity + k3_vel) * dt;

        velocity += (k1_vel + 2.0f * k2_vel + 2.0f * k3_vel + k4_vel) / 6.0f;
        position += (k1_pos + 2.0f * k2_pos + 2.0f * k3_pos + k4_pos) / 6.0f;
    }

} // namespace engine::math


#endif // ENGINE_MATH_CALCULUS_INL
