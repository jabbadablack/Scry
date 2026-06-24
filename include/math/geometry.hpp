#ifndef ENGINE_MATH_GEOMETRY_HPP
#define ENGINE_MATH_GEOMETRY_HPP

#include "algebra.hpp"


namespace engine::math {

    struct Ray {
        Vector3 origin;
        Vector3 direction;
    };

    struct Plane {
        Vector3 normal;
        f32 distance;
    };

    struct AABB {
        Vector3 min;
        Vector3 max;

        ENGINE_INLINE Vector3 Center() const {
            return (min + max) * 0.5f;
        }

        ENGINE_INLINE Vector3 Extents() const {
            return (max - min) * 0.5f;
        }
    };

    struct Sphere {
        Vector3 center;
        f32 radius;
    };

    [[nodiscard]] ENGINE_INLINE bool Intersect(const AABB& a, const AABB& b);
    [[nodiscard]] ENGINE_INLINE bool Intersect(const Sphere& a, const Sphere& b);
    [[nodiscard]] ENGINE_INLINE bool Intersect(const Ray& ray, const AABB& aabb, f32& out_t);

} // namespace engine::math


#include "geometry.inl"

#endif // ENGINE_MATH_GEOMETRY_HPP
