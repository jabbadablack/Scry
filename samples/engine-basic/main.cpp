#include <scry.hpp>
#include <renderer/diligent_module.hpp>
#include <intent/intent_queue.hpp>
#include <ecs/components.hpp>
#include <debug/logger.hpp>
#include <GLFW/glfw3.h>
#include <OS/glfw/glfw_window.hpp>
#include <OS/glfw/glfw_input.hpp>
#include <OS/glfw/glfw_impl.inl>
#include <chrono>
#include <thread>

struct MyMoveIntent {
    engine::ecs::Entity target;
    engine::math::Vector3 velocity;
};

class GameLogicModule : public engine::IModule {
public:
    GameLogicModule() = default;

    void RegisterReflection() override {
        using namespace entt::literals;
        // Reflect the core engine Transform Component so the future Editor can see it
        entt::meta<engine::ecs::TransformComponent>()
            .type("TransformComponent"_hs)
            .data<&engine::ecs::TransformComponent::matrix>("matrix"_hs)
            .data<&engine::ecs::TransformComponent::previous_matrix>("previous_matrix"_hs);

        // Reflect the Custom Intent
        entt::meta<MyMoveIntent>()
            .type("MyMoveIntent"_hs)
            .data<&MyMoveIntent::target>("target"_hs)
            .data<&MyMoveIntent::velocity>("velocity"_hs);

        ENGINE_LOG_INFO("GameLogicModule: Reflection registered");
    }

    const char* GetName() const override {
        return "GameLogicModule";
    }

    bool Initialize(engine::Engine& engine) override {
        m_engine = &engine;
        return true;
    }

    void BuildGraph(tf::Taskflow& taskflow) override {}

    void CompileFrameGraph(engine::FrameDAG& dag) override {
        int write_state = dag.write_state;
        m_moveQueue.Initialize(m_engine->GetFrameArena(write_state), 100);

        engine::SystemBuilder(dag)
            .AddIntent("IntentGeneration", [this, write_state]() {
                if (m_engine->GetInput() && m_engine->GetInput()->IsKeyHeld(GLFW_KEY_W)) {
                    auto view = m_engine->GetRegistry().View<engine::ecs::TransformComponent>();
                    for (auto ent : view) {
                        m_moveQueue.Push({ent, {0.0f, 1.0f, 0.0f}}, m_engine->GetFrameArena(write_state));
                    }
                    ENGINE_LOG_INFO("Input has been taken");
                }
            })
            .AddReactor("Reactor", engine::Process(m_moveQueue, [this](const MyMoveIntent& intent) {
                auto& registry = m_engine->GetRegistry();
                if (registry.GetRawRegistry().valid(intent.target) && registry.HasComponent<engine::ecs::TransformComponent>(intent.target)) {
                    auto& trans = registry.GetComponent<engine::ecs::TransformComponent>(intent.target);
                    trans.previous_matrix = trans.matrix;
                    trans.matrix(0, 3) += intent.velocity.x();
                    trans.matrix(1, 3) += intent.velocity.y();
                    trans.matrix(2, 3) += intent.velocity.z();
                    ENGINE_LOG_INFO("Intent has been reacted to");
                }
            }));
    }

    void Shutdown() override {}

private:
    engine::Engine* m_engine = nullptr;
    engine::IntentQueue<MyMoveIntent> m_moveQueue;
};

int main() {
    engine::GlfwWindow window;
    window.Initialize();
    window.CreateWindow(800, 600, "Standalone Runtime");

    engine::GlfwInput input;
    input.Initialize(window.GetRawWindow());

    engine::Engine engine;
    engine.GetWindowManager().SetMainWindow(&window);

    // Register modules
    auto& renderer = engine.RegisterModule<engine::renderer::DiligentModule>();
    auto& logic = engine.RegisterModule<GameLogicModule>();

    ENGINE_LOG_INFO("[SAMPLE] Registered module: " + std::string(renderer.GetName()));
    ENGINE_LOG_INFO("[SAMPLE] Registered module: " + std::string(logic.GetName()));

    // Initialize the engine
    if (!engine.Initialize(&input)) {
        ENGINE_LOG_ERROR("[SAMPLE] Failed to initialize engine");
        return -1;
    }

    // Create a mock renderable entity
    auto entity = engine.GetRegistry().CreateEntity();

    // Attach components
    auto& transform = engine.GetRegistry().AddComponent<engine::ecs::TransformComponent>(entity);
    transform.matrix = engine::math::Matrix4::Identity();

    auto& render = engine.GetRegistry().AddComponent<engine::ecs::RenderComponent>(entity);
    engine.GetResourceManager().SetMesh(engine::StringHash{"my_mesh"}, "res://objects/Hermanubis_low.fbx");
    render.mesh_id = engine::StringHash{"my_mesh"};
    render.texture_id = engine::StringHash{"mock_texture"};

    // Rebuild the execution graph to register all modules
    engine.RebuildExecutionGraph();

    using Clock = std::chrono::high_resolution_clock;
    auto time_start = Clock::now();
    double accumulator = 0.0;
    constexpr double target_dt = 1.0 / 60.0; // 60 Hz dt

    while (!engine.GetWindowManager().ShouldClose()) {
        auto time_now = Clock::now();
        std::chrono::duration<double> duration = time_now - time_start;
        double dt = duration.count();
        time_start = time_now;

        accumulator += dt;

        while (accumulator >= target_dt) {
            engine.Tick();
            accumulator -= target_dt;
        }

        engine.SetInterpolationAlpha(accumulator / target_dt);
        engine.GetWindowManager().PollAllEvents();
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }

    ENGINE_LOG_INFO("[SAMPLE] Engine run loop exited");

    return 0;
}
