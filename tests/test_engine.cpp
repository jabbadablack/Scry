#include <doctest/doctest.h>
#include <engine.hpp>
#include <OS/glfw/glfw_window.hpp>
#include <OS/glfw/glfw_input.hpp>
#include <IO/vfs.hpp>
#include <GLFW/glfw3.h>

class MockModule : public engine::IModule {
public:
    bool init_called = false;
    bool build_graph_called = false;
    bool shutdown_called = false;

    bool Initialize(engine::Engine& engine) override {
        init_called = true;
        // Verify we can access engine components
        REQUIRE(engine.GetWindowManager().GetMainWindow() != nullptr);
        REQUIRE(&engine.GetResourceManager() != nullptr);
        REQUIRE(&engine.GetRegistry() != nullptr);
        return true;
    }

    void BuildGraph(tf::Taskflow& taskflow) override {
        build_graph_called = true;
        // Inject a simple task flow node
        taskflow.emplace([]() {
            // No-op mock frame job
        });
    }

    void Shutdown() override {
        shutdown_called = true;
    }

    const char* GetName() const override {
        return "MockModule";
    }
};

TEST_CASE("Engine Module Lifecycle Integration") {
    engine::GlfwWindow window;
    window.Initialize();
    glfwWindowHint(GLFW_VISIBLE, GLFW_FALSE);
    window.CreateWindow(100, 100, "ENGINE Test Engine");

    engine::GlfwInput input;
    input.Initialize(window.GetRawWindow());

    engine::Engine engine;

    // Register our MockModule
    MockModule& mock = engine.RegisterModule<MockModule>();

    REQUIRE(mock.init_called == false);
    REQUIRE(mock.build_graph_called == false);
    REQUIRE(mock.shutdown_called == false);

    // Initialize the engine with our decoupled window and input interfaces
    engine.GetWindowManager().SetMainWindow(&window);
    bool init_result = engine.Initialize(&input);
    REQUIRE(init_result == true);

    // Verify lifecycle stages post-initialization
    REQUIRE(mock.init_called == true);
    REQUIRE(mock.build_graph_called == true);
    REQUIRE(mock.shutdown_called == false);

    // Verify execution graph rebuild can run dynamically without crashing
    engine.RebuildExecutionGraph();

    // Shutdown the engine explicitly
    engine.Shutdown();

    // Verify shutdown lifecycle stage
    REQUIRE(mock.shutdown_called == true);
}

TEST_CASE("Intent Queue Operations") {
    engine::ChainedArena arena(64 * 1024);
    
    struct MockIntent {
        int x;
        float y;
    };
    
    engine::IntentQueue<MockIntent> queue;
    queue.Initialize(arena, 100);
    
    // Test initial state
    auto begin_it = queue.begin();
    auto end_it = queue.end();
    REQUIRE(begin_it == end_it);
    
    // Test push
    MockIntent intent1{42, 3.14f};
    auto handle1 = queue.Push(intent1, arena);
    
    REQUIRE(handle1 != nullptr);
    REQUIRE(handle1->data != nullptr);
    REQUIRE(handle1->data->x == 42);
    REQUIRE(handle1->data->y == doctest::Approx(3.14f));
    REQUIRE(handle1->state == engine::IntentState::Pending);
    
    // Test count after push
    begin_it = queue.begin();
    end_it = queue.end();
    REQUIRE(end_it - begin_it == 1);
    REQUIRE(begin_it[0].data->x == 42);
    
    // Test multiple pushes
    MockIntent intent2{100, 1.23f};
    auto handle2 = queue.Push(intent2, arena);
    REQUIRE(handle2 != nullptr);
    REQUIRE(handle2->data->x == 100);
    
    begin_it = queue.begin();
    end_it = queue.end();
    REQUIRE(end_it - begin_it == 2);
    REQUIRE(begin_it[1].data->x == 100);
}

class DynamicMockModule : public engine::IModule {
public:
    bool compile_called = false;

    bool Initialize(engine::Engine& engine) override {
        return true;
    }

    void BuildGraph(tf::Taskflow& taskflow) override {}

    void CompileFrameGraph(engine::FrameDAG& dag) override {
        compile_called = true;
        dag.taskflow.emplace([]() {
            // Mock dynamic frame task
        }).name("DynamicMockTask");
    }

    void Shutdown() override {}

    const char* GetName() const override {
        return "DynamicMockModule";
    }
};

TEST_CASE("Dynamic Module Frame Graph Compilation") {
    engine::GlfwWindow window;
    window.Initialize();
    glfwWindowHint(GLFW_VISIBLE, GLFW_FALSE);
    window.CreateWindow(100, 100, "ENGINE Test Dynamic Compiler");

    engine::GlfwInput input;
    input.Initialize(window.GetRawWindow());

    engine::Engine engine;

    DynamicMockModule& mock = engine.RegisterModule<DynamicMockModule>();

    engine.GetWindowManager().SetMainWindow(&window);
    bool init_result = engine.Initialize(&input);
    REQUIRE(init_result == true);

    // Let's run a single simulation iteration of the frame compiler
    engine.GetTaskflow().clear();
    tf::Task phase_intent = engine.GetTaskflow().emplace([](){});
    tf::Task phase_reactor = engine.GetTaskflow().emplace([](){});
    tf::Task phase_extract = engine.GetTaskflow().emplace([](){});
    engine::FrameDAG dag{engine.GetTaskflow(), phase_intent, phase_reactor, phase_extract, engine.GetWriteState(), engine.GetWriteState()};
    mock.CompileFrameGraph(dag);
    
    REQUIRE(mock.compile_called == true);
    REQUIRE(engine.GetTaskflow().empty() == false);

    engine.GetJobSystem().GetExecutor().run(engine.GetTaskflow()).wait();

    engine.Shutdown();
}

TEST_CASE("Virtual File System Resolution") {
    engine::VirtualFileSystem vfs;
    vfs.Mount("res://", "C:/project/assets/");

    std::string out_path;
    bool result = vfs.Resolve("res://textures/image.png", out_path);
    REQUIRE(result == true);
    REQUIRE(out_path == "C:/project/assets/textures/image.png");

    result = vfs.Resolve("invalid://file.txt", out_path);
    REQUIRE(result == false);
}


