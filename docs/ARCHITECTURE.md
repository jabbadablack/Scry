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
* All dynamic per-frame data (Intents, RenderPackets) MUST be allocated using `engine::ChainedArena`.
* All persistent ECS allocations are automatically routed through `engine::TrackedHeap` via our custom `EcsAllocator`.
* **Dynamic Pointer Alignment:** Allocations in `engine::ChainedArena` support arbitrary alignments (e.g. 16, 32, 64, or 128 bytes) by dynamically aligning absolute memory addresses during allocation, preventing SIMD and matrix layout faults on modern architectures.

### 4. Code Structure
* **Header-Only Core:** Low-level engine primitives should rely on `.hpp` and `.inl` inline files to maximize link-time optimization (LTO) and compiler inlining.
* **Abstract the OS:** The core `Engine` must remain blind to the OS. Windowing, Audio, and Graphics are external `IModule` implementations.

### 5. Time & OS Hardware Clocks
* **Direct OS Clock Mapping:** Standard library headers like `<chrono>` and `<thread>` are forbidden in core loop timing. Timing and sleeping MUST route through `engine::TimeManager`, which bypasses the standard library using direct OS imports (`QueryPerformanceCounter` on Windows, `clock_gettime` with `CLOCK_MONOTONIC` on Linux) via `extern "C"`.
* **Clamped Deltas:** Frame delta-time calculation is automatically clamped at a maximum of `0.1` seconds to prevent debugging breakpoints from causing spikes in game logic update cycles.

### 6. Generational Resource Handles & Bindless Graphics
* **Generational Resource Handles:** Accessing graphics resources using raw pointers is prohibited. Resources (buffers, textures, pipelines) are referenced by a 32-bit `ResourceHandle<Tag>` comprising a 20-bit index and a 12-bit generation count to prevent dangling reference bugs.
* **Lockless Intent Flushes:** Creations and deletions are deferred through main-thread intent queues, which are flushed to the renderer thread via mutex-protected vector swapping, keeping the rendering loop completely lock-free.
* **Bindless Architecture:** The renderer uses a global pipeline resource signature and a single global shader resource binding (SRB) per frame. Textures are bound directly to their corresponding handle slot index in a 1024-element global bindless array.
* **Safety Dummy Padding:** To prevent Vulkan validation/partially-bound crashes, unused slots in the bindless texture array are always bound to a `1x1` dummy fallback texture.
* **Push Constant Simulation:** Hardware push constants are simulated using a fast-mapped uniform buffer dynamic write (`MapBuffer` with discard/unmap flags) updated per draw call.
