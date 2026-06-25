#include <scry.hpp>
#include <renderer/diligent_module.hpp>
#include <ecs/components.hpp>

namespace {

struct CameraIntent {
    engine::ecs::Entity   target;
    engine::math::Vector3 translationDelta;
    engine::f64           yawDelta;
    engine::f64           pitchDelta;
};

class GameLogicModule : public engine::IModule {
public:
    const char* GetName() const override { return "GameLogicModule"; }

    bool Initialize(engine::Engine& engine) override {
        m_engine = &engine;
        return true;
    }

    void BuildGraph(tf::Taskflow& taskflow) override {}

    void CompileFrameGraph(engine::FrameDAG& dag) override {
        int write_state = dag.write_state;
        m_camQueue.Initialize(m_engine->GetFrameArena(write_state), 10);

        engine::SystemBuilder(dag)
            .AddIntent("InputIntent", [this, write_state]() {
                auto* input = m_engine->GetInput();
                if (!input) { return;}

                engine::math::Vector3 moveDelta = engine::math::Vector3::Zero();
                if (input->IsKeyHeld(engine::Key::W)) moveDelta.z() += 1.0F;
                if (input->IsKeyHeld(engine::Key::S)) moveDelta.z() -= 1.0F;
                if (input->IsKeyHeld(engine::Key::A)) moveDelta.x() -= 1.0F;
                if (input->IsKeyHeld(engine::Key::D)) moveDelta.x() += 1.0F;

                engine::f64 dx = 0, dy = 0;
                if (input->IsMouseButtonHeld(engine::MouseButton::Right)) {
                    input->GetMouseDelta(dx, dy);
                }

                auto view = m_engine->GetRegistry().View<engine::ecs::CameraComponent>();
                for (auto ent : view) {
                    m_camQueue.Push({ent, moveDelta, dx, dy}, m_engine->GetFrameArena(write_state));
                }
            })
            .AddReactor("CameraReactor", engine::Process(m_camQueue, [this](const CameraIntent& intent) {
                auto& reg = m_engine->GetRegistry();
                if (reg.HasComponent<engine::ecs::CameraComponent>(intent.target) && reg.HasComponent<engine::ecs::TransformComponent>(intent.target)) {
                    auto& cam   = reg.GetComponent<engine::ecs::CameraComponent>(intent.target);
                    auto& trans = reg.GetComponent<engine::ecs::TransformComponent>(intent.target);

                    // Accumulate rotation
                    static engine::f32 yaw   = 0.0F;
                    static engine::f32 pitch = 0.0F;
                    yaw += static_cast<engine::f32>(intent.yawDelta) * 0.005F;
                    pitch += static_cast<engine::f32>(intent.pitchDelta) * 0.005F;
                    pitch = engine::math::Clamp(pitch, -1.5F, 1.5F);

                    engine::math::Vector3 forward(
                        engine::math::Cos(pitch) * engine::math::Sin(yaw),
                        engine::math::Sin(pitch),
                        engine::math::Cos(pitch) * engine::math::Cos(yaw)
                    );
                    engine::math::Vector3 right = engine::math::Vector3(0, 1, 0).cross(forward).normalized();
                    engine::math::Vector3 up    = forward.cross(right).normalized();

                    engine::math::Vector3 pos(trans.matrix(0, 3), trans.matrix(1, 3), trans.matrix(2, 3));
                    pos += (forward * intent.translationDelta.z() + right * intent.translationDelta.x()) * 10.0F * static_cast<engine::f32>(m_engine->GetTime().GetDeltaTime());

                    trans.matrix                  = engine::math::Matrix4::Identity();
                    trans.matrix(0, 3) = pos.x();
                    trans.matrix(1, 3) = pos.y();
                    trans.matrix(2, 3) = pos.z();

                    engine::math::Matrix4 view = engine::math::LookAtLH(pos, pos + forward, up);
                    engine::math::Matrix4 proj = engine::math::PerspectiveFovLH(engine::math::DegToRad(cam.fov), 800.0F / 600.0F, cam.near_plane, cam.far_plane);
                    cam.view_proj              = view * proj;
                }
            }));
    }
    void Shutdown() override {}

private:
    engine::Engine*                   m_engine = nullptr;
    engine::IntentQueue<CameraIntent> m_camQueue;
};

void GenerateGridMesh(engine::Engine& engine) {
    auto grid = std::make_shared<engine::io::Mesh>();
    // Simple 4-vertex quad scaled up
    grid->vertices.push_back({ { -50.0F, 0.0F, 50.0F }, { 0, 1, 0 }, { 0.0F, 0.0F } });
    grid->vertices.push_back({ { 50.0F, 0.0F, 50.0F }, { 0, 1, 0 }, { 10.0F, 0.0F } });
    grid->vertices.push_back({ { 50.0F, 0.0F, -50.0F }, { 0, 1, 0 }, { 10.0F, 10.0F } });
    grid->vertices.push_back({ { -50.0F, 0.0F, -50.0F }, { 0, 1, 0 }, { 0.0F, 10.0F } });
    grid->indices = { 0, 1, 2, 2, 3, 0 };

    engine.GetResourceManager().GetRawMeshes().load<engine::io::DirectLoader<engine::io::Mesh>>(engine::StringHash{"GridMesh"}, engine::io::DirectLoader<engine::io::Mesh>{ grid });
}

} // namespace

int main() {
    engine::Engine engine;
    engine.RegisterModule<engine::renderer::DiligentModule>();
    engine.RegisterModule<GameLogicModule>();

    if (!engine.Initialize(800, 600, "SCRY Engine: Level & Hierarchy")) { return -1;}

    GenerateGridMesh(engine);

    auto& reg = engine.GetRegistry();

    // 1. Create Level
    auto levelEnt = reg.CreateEntity();
    reg.AddComponent<engine::ecs::TagComponent>(levelEnt).tag = engine::StringHash{"Level"};
    reg.AddComponent<engine::ecs::HierarchyComponent>(levelEnt);

    // 2. Create Grid (Child of Level)
    auto gridEnt = reg.CreateEntity();
    reg.AddComponent<engine::ecs::TransformComponent>(gridEnt);
    auto& gridRender   = reg.AddComponent<engine::ecs::RenderComponent>(gridEnt);
    gridRender.mesh_id = engine::StringHash{"GridMesh"};

    auto& gridHier  = reg.AddComponent<engine::ecs::HierarchyComponent>(gridEnt);
    gridHier.parent = levelEnt;
    reg.GetComponent<engine::ecs::HierarchyComponent>(levelEnt).first_child = gridEnt;

    // 3. Create Free-Fly Camera
    auto camEnt = reg.CreateEntity();
    auto& camTrans = reg.AddComponent<engine::ecs::TransformComponent>(camEnt);
    camTrans.matrix(0, 3) = 0.0F;
    camTrans.matrix(1, 3) = 2.0F;
    camTrans.matrix(2, 3) = -10.0F; // Back up 10 units
    reg.AddComponent<engine::ecs::CameraComponent>(camEnt);

    engine.RebuildExecutionGraph();
    engine.Run();

    return 0;
}
