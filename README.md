# SCRY ENGINE

### ROADMAP
- [ ] fix vert quant
- [ ] textures
- [ ] materials
- [ ] refactor entities and components
- [ ] find that pesky mem leak
- [ ] tbd

### THEORETICAL ARCHITECHTURE

```mermaid
graph TD
    subgraph "Application Layer"
        Sandbox[Sandbox Application]
        Plugins[External Plugins]
    end

    subgraph "Core Orchestration"
        Context[Engine Context]
        ECS[Flecs ECS World]
        Pipeline[Phase Management]
    end

    subgraph "Execution Phases (Pipeline)"
        Intent[Phase: Intent]
        StateUpdate[Phase: StateUpdate]
        React[Phase: React]
        PostUpdate[Phase: PostUpdate]
        PreRender[Phase: PreRender]
        Render[Phase: Render]
    end

    subgraph "Engine Subsystems"
        subgraph "Graphics"
            Renderer[Renderer]
            Diligent[DiligentCore Backend]
            Vulkan[Vulkan API]
        end

        subgraph "Platform & Input"
            GLFW[GLFW / Win32]
            Input[Input Buffer]
        end

        subgraph "Spatial & Physics"
            Spatial[Spatial System]
            Transform[Transform System]
        end
    end

    subgraph "Asset Pipeline"
        Cooker[Asset Cooker]
        Raw[FBX / Shaders]
        Cooked[.scrymesh / Binary]
    end

    subgraph "Data & Utils"
        JSON[yyjson Parser]
        Math[Eigen / SIMD]
        Memory[Arena/Pool Allocators]
    end

    %% Relationships
    Sandbox --> Context
    Context --> ECS
    Context --> Pipeline
    Plugins -.-> ECS

    Pipeline --> Intent
    Pipeline --> StateUpdate
    Pipeline --> React
    Pipeline --> PostUpdate
    Pipeline --> PreRender
    Pipeline --> Render

    %% System Mapping
    Intent --> Input
    StateUpdate --> Transform
    React --> Spatial
    Render --> Renderer
    Renderer --> Diligent
    Diligent --> Vulkan

    %% Data Flow
    Cooker --> Raw
    Raw --> Cooked
    Renderer --> Cooked
    ECS --> Memory
    Transform --> Math
    Context --> JSON
```