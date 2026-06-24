#include <scry.hpp>
#include <entt/entt.hpp>
#include <renderer/diligent_module.hpp>
#include <intent/intent_queue.hpp>
#include <ecs/components.hpp>
#include <debug/logger.hpp>
#include <GLFW/glfw3.h>
#include <glfw/glfw_window.hpp>
#include <glfw/glfw_input.hpp>
#include <chrono>
#include <thread>

struct MyMoveIntent {
    entt::entity target;
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
        // Initialize intent queue
        m_moveQueue.Initialize(m_engine->GetFrameArena(write_state), 100);

        // IntentGeneration task
        tf::Task intent_task = dag.taskflow.emplace([this, write_state]() {
            if (m_engine->GetInput() && m_engine->GetInput()->IsKeyHeld(GLFW_KEY_W)) {
                auto view = m_engine->GetRegistry().View<engine::ecs::TransformComponent>();
                for (auto ent : view) {
                    MyMoveIntent intent;
                    intent.target = ent;
                    intent.velocity = engine::math::Vector3(0.0f, 1.0f, 0.0f);
                    m_moveQueue.Push(intent, m_engine->GetFrameArena(write_state));
                }
                ENGINE_LOG_INFO("Input has been taken");
            }
        }).name("IntentGeneration");

        // Reactor task
        tf::Task reactor_task = dag.taskflow.emplace([this]() {
            auto& registry = m_engine->GetRegistry();
            for (auto* it = m_moveQueue.begin(); it != m_moveQueue.end(); ++it) {
                if (it->state == engine::IntentState::Pending && it->data) {
                    const auto& intent = *(it->data);
                    if (registry.GetRawRegistry().valid(intent.target) && registry.HasComponent<engine::ecs::TransformComponent>(intent.target)) {
                        auto& trans = registry.GetComponent<engine::ecs::TransformComponent>(intent.target);
                        trans.previous_matrix = trans.matrix;
                        trans.matrix(0, 3) += intent.velocity.x();
                        trans.matrix(1, 3) += intent.velocity.y();
                        trans.matrix(2, 3) += intent.velocity.z();
                        ENGINE_LOG_INFO("Intent has been reacted to");
                    }
                    it->state = engine::IntentState::Consumed;
                }
            }
        }).name("Reactor");

        intent_task.precede(dag.phase_intent);     // Generate intents BEFORE the Intent Barrier
        dag.phase_intent.precede(reactor_task);    // Wait for Intent Barrier before Reacting
        reactor_task.precede(dag.phase_reactor);   // Finish Reacting BEFORE the Reactor Barrier
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
    engine.GetResourceManager().SetMesh(entt::hashed_string{"my_mesh"}, "res://objects/Hermanubis_low.fbx");
    render.mesh_id = entt::hashed_string{"my_mesh"};
    render.texture_id = entt::hashed_string{"mock_texture"};

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
