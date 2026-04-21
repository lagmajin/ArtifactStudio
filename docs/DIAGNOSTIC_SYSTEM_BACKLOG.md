# Backlog: Diagnostic System Implementation Progress

## Overall Progress
- [x] Phase 1: Telemetry (100%)
- [ ] Phase 2: Render Graph Visualizer (0%)
- [ ] Phase 3: Acoustic Spectrogram (0%)

---

## Task Details

### [Phase 1: Telemetry]
- [x] **1.1 Define `AcousticSnapshot` in `Acoustic.ixx`**
- [x] **1.2 Implement `AcousticSystem::GetDebugSnapshot()`**
- [x] **1.3 Add `RenderNode::GetDebugInfo()` and State Enum**
- [x] **1.4 Implement `DiagnosticRegistry` for thread-safe data access**

### [Phase 2: Render Graph Visualizer (Diligent)]
- [ ] **2.1 Implement `RenderGraphDebugPass` (Diligent Overlay Pass)**
- [ ] **2.2 Write `GraphOverlay.hlsl` (Quads for nodes, Lines for links)**
- [ ] **2.3 Node layout algorithm (Layer-based mapping to GPU NDC)**

### [Phase 3: Acoustic Spectrogram (Diligent)]
- [ ] **3.1 Implement `AcousticSpectrogramPass`**
- [ ] **3.2 Write `AcousticPoints.hlsl`**
