#include <scry.hpp>
#include <renderer/diligent_module.hpp>
#include <intent/intent_queue.hpp>
#include <ecs/components.hpp>
#include <debug/logger.hpp>

namespace {

struct MyMoveIntent {
    engine::ecs::Entity target;
    engine::math::Vector3 velocity;
};

class GameLogicModule : public engine::IModule {
public:
    GameLogicModule() = default;

    void RegisterReflection() override {
        engine::ecs::Meta<engine::ecs::TransformComponent>()
            .type(engine::ecs::Hash("TransformComponent"))
            .data<&engine::ecs::TransformComponent::matrix>(engine::ecs::Hash("matrix"))
            .data<&engine::ecs::TransformComponent::previous_matrix>(engine::ecs::Hash("previous_matrix"));

        engine::ecs::Meta<MyMoveIntent>()
            .type(engine::ecs::Hash("MyMoveIntent"))
            .data<&MyMoveIntent::target>(engine::ecs::Hash("target"))
            .data<&MyMoveIntent::velocity>(engine::ecs::Hash("velocity"));

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
                if (m_engine->GetInput() && m_engine->GetInput()->IsKeyHeld(engine::Key::W)) {
                    auto view = m_engine->GetRegistry().View<engine::ecs::TransformComponent>();
                    for (auto ent : view) {
                        m_moveQueue.Push({.target=ent, .velocity={0.0F, 1.0F, 0.0F}}, m_engine->GetFrameArena(write_state));
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

} // anonymous namespace

int main() {
    engine::Engine engine;

    engine.RegisterModule<engine::renderer::DiligentModule>();
    engine.RegisterModule<GameLogicModule>();

    if (!engine.Initialize(800, 600, "SCRY Standalone Runtime")) {
        return -1;
    }

    auto entity = engine.GetRegistry().CreateEntity();
    auto& transform = engine.GetRegistry().AddComponent<engine::ecs::TransformComponent>(entity);
    transform.matrix = engine::math::Matrix4::Identity();

    auto& render = engine.GetRegistry().AddComponent<engine::ecs::RenderComponent>(entity);
    engine.GetResourceManager().SetMesh(engine::StringHash{"my_mesh"}, "res://objects/Hermanubis_low.fbx");
    render.mesh_id = engine::StringHash{"my_mesh"};
    render.texture_id = engine::StringHash{"mock_texture"};

    engine.RebuildExecutionGraph();
    
    // Let the engine take control!
    engine.Run();

    return 0;
}
