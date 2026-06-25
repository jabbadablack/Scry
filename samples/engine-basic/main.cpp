// clang-format off
#include <scry.hpp>
#include <renderer/diligent_module.hpp>
#include <ecs/components.hpp>
// clang-format on

namespace {

struct CameraIntent {
    engine::ecs::Entity target;
    engine::math::Vector3 translationDelta;
    engine::f64 yawDelta;
    engine::f64 pitchDelta;
};

struct EditorToggleIntent {
    engine::ecs::Entity target;
};

class GameLogicModule : public engine::IModule {
public:
    const char* GetName() const override { return "GameLogicModule"; }

    bool Initialize(engine::Engine& engine) override {
        m_engine = &engine;
        return true;
    }

    void BuildGraph(tf::Taskflow& taskflow) override {}

    void RegisterReflection() override {
        using CurrentType = CameraIntent;
        ENGINE_REFLECT_CLASS(CameraIntent)
            .ENGINE_REFLECT_FIELD(target)
            .ENGINE_REFLECT_FIELD(translationDelta)
            .ENGINE_REFLECT_FIELD(yawDelta)
            .ENGINE_REFLECT_FIELD(pitchDelta);
    }

    void CompileFrameGraph(engine::FrameDAG& dag) override {
        const int* p_write = dag.p_write_state;

        engine::SystemBuilder(dag)
            .AddIntent("InputIntent",
                       [this, p_write]() {
                           int write_state = *p_write;
                           m_camQueue.Initialize(m_engine->GetFrameArena(write_state), 10);
                           m_editorQueue.Initialize(m_engine->GetFrameArena(write_state), 5);
                           auto* input = m_engine->GetInput();
                           if (!input) {
                               return;
                           }

                           if (input->IsKeyPressed(engine::Key::V)) {
                               input->SetCursorVisible(!input->IsCursorVisible());
                               ENGINE_LOG_INFO(std::string("Cursor visibility toggled to: ") +
                                               (input->IsCursorVisible() ? "visible" : "hidden"));
                           }
                           if (input->IsKeyPressed(engine::Key::C)) {
                               input->SetCursorConfined(!input->IsCursorConfined());
                               ENGINE_LOG_INFO(std::string("Cursor confinement toggled to: ") +
                                               (input->IsCursorConfined() ? "confined" : "unconfined"));
                           }

                           engine::math::Vector3 moveDelta = engine::math::Vector3::Zero();
                           if (input->IsKeyHeld(engine::Key::W)) {
                               moveDelta.z() += 1.0F;}
                           if (input->IsKeyHeld(engine::Key::S)) {
                               moveDelta.z() -= 1.0F;}
                           if (input->IsKeyHeld(engine::Key::A)) {
                               moveDelta.x() -= 1.0F;}
                           if (input->IsKeyHeld(engine::Key::D)) {
                               moveDelta.x() += 1.0F;}

                           engine::f64 dx = 0;
                           engine::f64 dy = 0;
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
                            if (reg.HasComponent<engine::ecs::CameraComponent>(intent.target) &&
                                reg.HasComponent<engine::ecs::TransformComponent>(intent.target)) {
                                auto& cam = reg.GetComponent<engine::ecs::CameraComponent>(intent.target);
                                auto& trans = reg.GetComponent<engine::ecs::TransformComponent>(intent.target);

                                // Accumulate rotation
                                static engine::f32 yaw = 0.0F;
                                static engine::f32 pitch = 0.0F;
                                yaw += static_cast<engine::f32>(intent.yawDelta) * 0.005F;
                                pitch -= static_cast<engine::f32>(intent.pitchDelta) * 0.005F;
                                pitch = engine::math::Clamp(pitch, -1.5F, 1.5F);

                                engine::math::Vector3 forward(engine::math::Cos(pitch) * engine::math::Sin(yaw),
                                                              engine::math::Sin(pitch),
                                                              engine::math::Cos(pitch) * engine::math::Cos(yaw));
                                engine::math::Vector3 right =
                                    engine::math::Vector3(0, 1, 0).cross(forward).normalized();
                                engine::math::Vector3 up = forward.cross(right).normalized();

                                engine::math::Vector3 pos(trans.matrix(0, 3), trans.matrix(1, 3), trans.matrix(2, 3));
                                pos += (forward * intent.translationDelta.z() + right * intent.translationDelta.x()) *
                                       10.0F * static_cast<engine::f32>(m_engine->GetTime().GetDeltaTime());

                                trans.matrix = engine::math::Matrix4::Identity();
                                trans.matrix(0, 3) = pos.x();
                                trans.matrix(1, 3) = pos.y();
                                trans.matrix(2, 3) = pos.z();

                                engine::math::Matrix4 view = engine::math::LookAtLH(pos, pos + forward, up);
                                engine::math::Matrix4 proj = engine::math::PerspectiveFovLH(
                                    engine::math::DegToRad(cam.fov), 800.0F / 600.0F, cam.near_plane, cam.far_plane);
                                cam.view_proj = view * proj;
                            }
                        }));
    }
    void Shutdown() override {}

private:
    engine::Engine* m_engine = nullptr;
    engine::IntentQueue<CameraIntent> m_camQueue;
    engine::IntentQueue<EditorToggleIntent> m_editorQueue;
};

void GenerateGridMesh(engine::Engine& engine) {
    auto grid = std::make_shared<engine::io::Mesh>();
    float size = 50.0F;
    float step = 2.0F;
    uint32_t index = 0;

    for (float i = -size; i <= size; i += step) {
        // Line parallel to Z axis (fixed X)
        grid->vertices.push_back({{i, 0.0F, -size}, {0, 1, 0}, {0.0F, 0.0F}});
        grid->vertices.push_back({{i, 0.0F, size}, {0, 1, 0}, {0.0F, 0.0F}});
        grid->indices.push_back(index++);
        grid->indices.push_back(index++);

        // Line parallel to X axis (fixed Z)
        grid->vertices.push_back({{-size, 0.0F, i}, {0, 1, 0}, {0.0F, 0.0F}});
        grid->vertices.push_back({{size, 0.0F, i}, {0, 1, 0}, {0.0F, 0.0F}});
        grid->indices.push_back(index++);
        grid->indices.push_back(index++);
    }

    engine.GetResourceManager().GetRawMeshes().load<engine::io::DirectLoader<engine::io::Mesh>>(
        engine::StringHash{"GridMesh"}, engine::io::DirectLoader<engine::io::Mesh>{grid});
}

} // namespace

int main() {
    engine::Engine engine;
    engine.RegisterModule<engine::renderer::DiligentModule>();
    engine.RegisterModule<GameLogicModule>();

    if (!engine.Initialize(800, 600, "SCRY")) {
        return -1;
    }

    ENGINE_LOG_INFO("Serializing Assets into data.pak...");
    const std::string assets_physical = engine.GetVFS().GetMountPath("res://");
    if (assets_physical.empty()) {
        ENGINE_LOG_ERROR("res:// not mounted — cannot build data.pak");
    } else if (!engine.GetVFS().PackDirectory("res://", assets_physical, "data.pak")) {
        ENGINE_LOG_ERROR("PackDirectory failed — check that the assets folder exists");
    } else if (engine.GetVFS().MountPak("data.pak")) {
        ENGINE_LOG_INFO("Successfully mounted data.pak into VFS Memory");
    } else {
        ENGINE_LOG_ERROR("Failed to mount data.pak");
    }

    GenerateGridMesh(engine);

    auto& reg = engine.GetRegistry();

    // 1. Create Level
    auto levelEnt = reg.CreateEntity();
    reg.AddComponent<engine::ecs::TagComponent>(levelEnt).tag = engine::StringHash{"Level"};
    reg.AddComponent<engine::ecs::HierarchyComponent>(levelEnt);
    reg.AddComponent<engine::ecs::EnvironmentComponent>(levelEnt);

    // 2. Create Grid (Child of Level)
    auto gridEnt = reg.CreateEntity();
    reg.AddComponent<engine::ecs::TransformComponent>(gridEnt);
    auto& gridRender = reg.AddComponent<engine::ecs::RenderComponent>(gridEnt);
    gridRender.mesh_id = engine::StringHash{"GridMesh"};
    gridRender.topology = engine::graphics::PrimitiveTopology::LineList;

    auto& gridHier = reg.AddComponent<engine::ecs::HierarchyComponent>(gridEnt);
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
