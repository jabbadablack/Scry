# SCRY Engine Architecture Manifesto

This codebase strictly adheres to Data-Oriented Design (DOD) and a multithreaded Intent-State-Reactor (ISR) Directed Acyclic Graph (DAG) architecture. 

When writing or modifying code for this engine, you **MUST** obey the following immutable constraints:

### 1. The Intent-State-Reactor (ISR) Paradigm
* **No In-Place Mutation:** Game logic, AI, and Input systems are **forbidden** from directly mutating `entt::registry` components. They may only read state and spawn lightweight `Intent` structs into lock-free `IntentQueues`.
* **The Reactor Rule:** Only tasks scheduled *after* the `phase_intent` barrier and *before* the `phase_reactor` barrier are legally allowed to mutate the registry.
* **The Extraction Rule:** Rendering and Audio modules may only read the registry *after* the `phase_reactor` barrier is complete. They must extract data into double-buffered lock-free queues.

### 2. Branchless by Default
* Minimize the use of `if`, `else`, and `switch` statements inside hot loops (e.g., Reactor tasks, Render Extraction).
* Prefer mathematical masking, bitwise operations, or `std::array` lookups to resolve conditions.
* Use `ENGINE_ASSERT` to guarantee data validity at the module boundary, allowing inner loops to run blindly without `if (ptr != nullptr)` checks.

### 3. Absolute Memory Control (NASA Rule 3)
* **Zero Post-Init Allocations:** `std::vector`, `std::string`, `new`, and `malloc` are strictly forbidden inside the `Tick()` loop.
* All dynamic per-frame data (Intents, RenderCommands) MUST be allocated using `engine::ChainedArena`.
* All persistent ECS allocations are automatically routed through `engine::TrackedHeap` via our custom `EcsAllocator`.

### 4. Code Structure
* **Header-Only Core:** Low-level engine primitives should rely on `.hpp` and `.inl` inline files to maximize link-time optimization (LTO) and compiler inlining.
* **Abstract the OS:** The core `Engine` must remain blind to the OS. Windowing, Audio, and Graphics are external `IModule` implementations.
