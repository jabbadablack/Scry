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

    ENGINE_INLINE Matrix4 PerspectiveFovLH(f32 fov, f32 aspect, f32 zNear, f32 zFar) {
        Matrix4 m = Matrix4::Zero();
        f32 tanHalfFov = engine::math::Tan(fov * 0.5F);
        m(0, 0) = 1.0F / (aspect * tanHalfFov);
        m(1, 1) = 1.0F / tanHalfFov;
        m(2, 2) = zFar / (zFar - zNear);
        m(3, 2) = -(zFar * zNear) / (zFar - zNear);
        m(2, 3) = 1.0F;
        return m;
    }

    ENGINE_INLINE Matrix4 LookAtLH(const Vector3& eye, const Vector3& target, const Vector3& up) {
        Vector3 zAxis = (target - eye).normalized();
        Vector3 xAxis = up.cross(zAxis).normalized();
        Vector3 yAxis = zAxis.cross(xAxis);
        Matrix4 m = Matrix4::Identity();
        m(0, 0) = xAxis.x(); m(1, 0) = xAxis.y(); m(2, 0) = xAxis.z(); m(3, 0) = -xAxis.dot(eye);
        m(0, 1) = yAxis.x(); m(1, 1) = yAxis.y(); m(2, 1) = yAxis.z(); m(3, 1) = -yAxis.dot(eye);
        m(0, 2) = zAxis.x(); m(1, 2) = zAxis.y(); m(2, 2) = zAxis.z(); m(3, 2) = -zAxis.dot(eye);
        return m;
    }

} // namespace engine::math

#endif // ENGINE_MATH_GEOMETRY_INL
