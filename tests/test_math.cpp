#include <doctest/doctest.h>
#include <engine_core.hpp>

using engine::f32;
using engine::i32;
using engine::u32;

TEST_CASE("Math Trigonometry Verification") {
    using namespace engine::math;

    REQUIRE(PI == doctest::Approx(3.14159265f));
    REQUIRE(HALF_PI == doctest::Approx(1.57079632f));
    REQUIRE(TWO_PI == doctest::Approx(6.2831853f));

    REQUIRE(DegToRad(180.0f) == doctest::Approx(PI));
    REQUIRE(RadToDeg(PI) == doctest::Approx(180.0f));

    REQUIRE(Sin(HALF_PI) == doctest::Approx(1.0f).epsilon(0.001));
    REQUIRE(Cos(PI) == doctest::Approx(-1.0f).epsilon(0.001));
    REQUIRE(Tan(DegToRad(45.0f)) == doctest::Approx(1.0f).epsilon(0.05));

    f32 s = 0.0f;
    f32 c = 0.0f;
    SinCos(HALF_PI, s, c);
    REQUIRE(s == doctest::Approx(1.0f).epsilon(0.001));
    REQUIRE(Abs(c) < 0.1f);
}

TEST_CASE("Math Geometry Intersection Verification") {
    using namespace engine::math;

    SUBCASE("AABB Intersect AABB") {
        AABB a{Vector3(0, 0, 0), Vector3(2, 2, 2)};
        AABB b{Vector3(1, 1, 1), Vector3(3, 3, 3)};
        AABB c{Vector3(3, 3, 3), Vector3(4, 4, 4)};

        REQUIRE(Intersect(a, b) == true);
        REQUIRE(Intersect(a, c) == false);
    }

    SUBCASE("Sphere Intersect Sphere") {
        Sphere a{Vector3(0, 0, 0), 1.0f};
        Sphere b{Vector3(1.5f, 0, 0), 1.0f};
        Sphere c{Vector3(3.0f, 0, 0), 0.5f};

        REQUIRE(Intersect(a, b) == true);
        REQUIRE(Intersect(a, c) == false);
    }

    SUBCASE("Ray Intersect AABB (Slab Method)") {
        AABB aabb{Vector3(-1.0f, -1.0f, -1.0f), Vector3(1.0f, 1.0f, 1.0f)};
        
        // Ray hitting AABB from outside
        Ray ray1{Vector3(-3.0f, 0.0f, 0.0f), Vector3(1.0f, 0.0f, 0.0f)};
        f32 t1 = 0.0f;
        REQUIRE(Intersect(ray1, aabb, t1) == true);
        REQUIRE(t1 == doctest::Approx(2.0f));

        // Ray missing AABB
        Ray ray2{Vector3(-3.0f, 3.0f, 0.0f), Vector3(1.0f, 0.0f, 0.0f)};
        f32 t2 = 0.0f;
        REQUIRE(Intersect(ray2, aabb, t2) == false);

        // Ray starting inside AABB
        Ray ray3{Vector3(0.0f, 0.0f, 0.0f), Vector3(0.0f, 0.0f, 1.0f)};
        f32 t3 = 0.0f;
        REQUIRE(Intersect(ray3, aabb, t3) == true);
        REQUIRE(t3 == doctest::Approx(0.0f));
    }
}

TEST_CASE("Math Calculus Integrators Verification") {
    using namespace engine::math;

    Vector3 pos(0.0f, 0.0f, 0.0f);
    Vector3 vel(1.0f, 0.0f, 0.0f);
    Vector3 accel(0.0f, 9.8f, 0.0f);
    f32 dt = 0.1f;

    SUBCASE("Semi-Implicit Euler") {
        Integrator::SemiImplicitEuler(pos, vel, accel, dt);
        // vel = vel0 + accel * dt = 1.0, 0.98, 0.0
        // pos = pos0 + vel * dt = 0.1, 0.098, 0.0
        REQUIRE(vel.x() == doctest::Approx(1.0f));
        REQUIRE(vel.y() == doctest::Approx(0.98f));
        REQUIRE(pos.x() == doctest::Approx(0.1f));
        REQUIRE(pos.y() == doctest::Approx(0.098f));
    }

    SUBCASE("RK4 Integration") {
        Vector3 rk_pos(0.0f, 0.0f, 0.0f);
        Vector3 rk_vel(1.0f, 0.0f, 0.0f);
        
        Integrator::RK4(rk_pos, rk_vel, accel, dt);
        
        // Analytical/RK4 constant acceleration:
        // vel = vel0 + accel * dt = 1.0, 0.98, 0.0
        // pos = pos0 + vel0 * dt + 0.5 * accel * dt^2 = 0.1, 0.049, 0.0
        REQUIRE(rk_vel.x() == doctest::Approx(1.0f));
        REQUIRE(rk_vel.y() == doctest::Approx(0.98f));
        REQUIRE(rk_pos.x() == doctest::Approx(0.1f));
        REQUIRE(rk_pos.y() == doctest::Approx(0.049f));
    }
}

TEST_CASE("Math Interpolation Verification") {
    using namespace engine::math;

    REQUIRE(Lerp(0.0f, 10.0f, 0.5f) == doctest::Approx(5.0f));
    
    Vector3 v1(0, 0, 0);
    Vector3 v2(10, 10, 10);
    Vector3 lerped = Lerp(v1, v2, 0.25f);
    REQUIRE(lerped.x() == doctest::Approx(2.5f));
    REQUIRE(lerped.y() == doctest::Approx(2.5f));
    REQUIRE(lerped.z() == doctest::Approx(2.5f));

    Quaternion q1 = Quaternion::Identity();
    Quaternion q2 = Quaternion(Eigen::AngleAxisf(DegToRad(90.0f), Vector3::UnitZ()));
    Quaternion slerped = Slerp(q1, q2, 0.5f);
    // Angle should be 45 degrees
    f32 angle = Eigen::AngleAxisf(slerped).angle();
    REQUIRE(angle == doctest::Approx(DegToRad(45.0f)));

    REQUIRE(Clamp(5.0f, 0.0f, 3.0f) == doctest::Approx(3.0f));
    REQUIRE(Clamp(-5.0f, 0.0f, 3.0f) == doctest::Approx(0.0f));
    REQUIRE(Clamp(2.0f, 0.0f, 3.0f) == doctest::Approx(2.0f));

    REQUIRE(Smoothstep(0.0f, 10.0f, 5.0f) == doctest::Approx(0.5f));
    REQUIRE(Smoothstep(0.0f, 10.0f, -1.0f) == doctest::Approx(0.0f));
    REQUIRE(Smoothstep(0.0f, 10.0f, 11.0f) == doctest::Approx(1.0f));

    REQUIRE(EaseInOutQuad(0.0f) == doctest::Approx(0.0f));
    REQUIRE(EaseInOutQuad(1.0f) == doctest::Approx(1.0f));
    REQUIRE(EaseInOutQuad(0.5f) == doctest::Approx(0.5f));
}

TEST_CASE("Math Random RNG Verification") {
    using namespace engine::math;

    Random::Initialize(42);

    for (int i = 0; i < 100; ++i) {
        f32 val = Random::Range(1.0f, 5.0f);
        REQUIRE(val >= 1.0f);
        REQUIRE(val <= 5.0f);

        i32 val_int = Random::RangeInt(10, 20);
        REQUIRE(val_int >= 10);
        REQUIRE(val_int <= 20);

        Vector3 dir = Random::Direction();
        REQUIRE(dir.norm() == doctest::Approx(1.0f).epsilon(0.01));
    }
}

class MockGraph : public engine::math::IGraph {
public:
    f32 Heuristic(engine::math::NodeID a, engine::math::NodeID b) const override {
        return std::abs(static_cast<f32>(a) - static_cast<f32>(b));
    }
    f32 Cost(engine::math::NodeID a, engine::math::NodeID b) const override {
        return 1.0f;
    }
    std::vector<engine::math::NodeID> GetNeighbors(engine::math::NodeID node) const override {
        std::vector<engine::math::NodeID> neighbors;
        if (node > 0) neighbors.push_back(node - 1);
        neighbors.push_back(node + 1);
        return neighbors;
    }
};

TEST_CASE("Math Discrete Pathfinder AStar Verification") {
    using namespace engine::math;

    MockGraph graph;
    std::vector<NodeID> path;
    bool found = AStar::FindPath(graph, 0, 3, path);
    REQUIRE(found == true);
    REQUIRE(path.size() == 4);
    REQUIRE(path[0] == 0);
    REQUIRE(path[1] == 1);
    REQUIRE(path[2] == 2);
    REQUIRE(path[3] == 3);
}
