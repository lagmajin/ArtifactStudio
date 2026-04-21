# Milestone: Diagnostic System (Acoustic & Render Graph)

## Goal
Establish a real-time diagnostic layer for the AE-style compositing engine to visualize acoustic physics and render graph dependencies.

---

## Phase 1: Telemetry & Instrumentation (Data Collection)
- [x] **Acoustic Telemetry**: Implement `AcousticSnapshot` to capture per-frame tasks.
- [x] **Graph Instrumentation**: Add state tracking (`Dirty`, `Clean`, `Evaluating`) to the Render Graph nodes.
- [x] **Debug Interface**: Create a shared registry to store and retrieve telemetry data without locking the render thread.

## Phase 2: Render Graph Dependency Visualizer (Visualization)
- [ ] **Diligent Overlay Pass**: Implement a dedicated render pass using `DiligentEngine` to draw directly on the viewport.
- [ ] **Graph HLSL Shaders**: Write shaders for rendering nodes as quads and links as line primitives.
- [ ] **Dynamic Graph Buffer**: Implement per-frame upload of node positions, states (Clean/Dirty), and link topology to GPU.

## Phase 3: Acoustic Task Spectrogram (Visualization)
- [ ] **GPU Point Rendering**: Write HLSL shaders to render thousands of audio tasks as points.
- [ ] **Spectrogram Logic**: Map frequency and time to GPU coordinate space (NDC).

## Phase 4: Integration & Optimization
- [ ] **Sub-frame Timing**: Precise profiling of acoustic vs. visual updates.
- [ ] **LOD Feedback**: Visualize which sounds were culled by the LOD manager.
