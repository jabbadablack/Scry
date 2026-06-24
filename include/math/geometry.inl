#ifndef ENGINE_MATH_GEOMETRY_INL
#define ENGINE_MATH_GEOMETRY_INL

#include <limits>
#include <algorithm>

namespace engine::math {

    ENGINE_INLINE bool Intersect(const AABB& a, const AABB& b) {
        return (a.min.x() <= b.max.x() && a.max.x() >= b.min.x()) &&
               (a.min.y() <= b.max.y() && a.max.y() >= b.min.y()) &&
               (a.min.z() <= b.max.z() && a.max.z() >= b.min.z());
    }

    ENGINE_INLINE bool Intersect(const Sphere& a, const Sphere& b) {
        return (a.center - b.center).squaredNorm() <= (a.radius + b.radius) * (a.radius + b.radius);
    }

    ENGINE_INLINE bool Intersect(const Ray& ray, const AABB& aabb, f32& out_t) {
        f32 tmin = -std::numeric_limits<f32>::infinity();
        f32 tmax = std::numeric_limits<f32>::infinity();

        for (int i = 0; i < 3; ++i) {
            f32 dir = ray.direction[i];
            f32 ori = ray.origin[i];
            f32 aabb_min = aabb.min[i];
            f32 aabb_max = aabb.max[i];

            if (engine::math::Abs(dir) < 1e-6f) {
                if (ori < aabb_min || ori > aabb_max) {
                    return false;
                }
            } else {
                f32 t1 = (aabb_min - ori) / dir;
                f32 t2 = (aabb_max - ori) / dir;

                if (t1 > t2) {
                    std::swap(t1, t2);
                }

                if (t1 > tmin) tmin = t1;
                if (t2 < tmax) tmax = t2;

                if (tmin > tmax) {
                    return false;
                }
            }
        }

        if (tmax < 0.0f) {
            return false;
        }

        out_t = (tmin < 0.0f) ? 0.0f : tmin;
        return true;
    }

} // namespace engine::math

#endif // ENGINE_MATH_GEOMETRY_INL
