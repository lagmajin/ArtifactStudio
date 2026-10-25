**最終更新:** 2026-10-25

# Insight Register

## 2026-10-25: clang-cl must not inherit MSVC-only flags
- **関連:** `Artifact/DockFoundation/CMakeLists.txt`, root `CMakeLists.txt`, clang-cl P1689 scans
- **確認できた事実:** The old generated `DockManager.ixx` scan command contained `/Zf` and failed with `no such file or directory: '/Zf'`; its standalone CMake used broad `if(MSVC)`. The guard now requires compiler ID `MSVC`. A fresh CMake 4.4.4 Ninja scanner run succeeded and emitted `Dock.Manager`, then the full `ArtifactDockFoundation` static library linked. Root optional target `/Zf` flags now use a compiler-ID generator expression, and the always-configured VideoPreviewBench plus standalone GPU smoke target also require actual MSVC. With Plugin Host enabled, fresh clang-cl output omits `/Zf` and MSVC-only standard flags from scan and compile commands, keeps `-std=c++23`, and its direct P1689 scan emitted seven module requirements. Both clang-cl and upstream Clang 22 GNU frontend cleanly built `ArtifactCoreEnvironment`, including migrated `EnvironmentVariable.cpp`. Current-tree P1689 scanner commands for EnvironmentVariable, DockManager, AudioMiniWidget, ApplicationManager, and PreviewCompositionPipeline succeeded under both GNU-frontend Clang and clang-cl; their `.ddi` dependency names match for all five commands.
- **価値または懸念:** Dedicated library conditions and root optional-target flags can both bypass compiler-family gates and inject MSVC-only options into Clang scanning. The identified production-library and optional-root cases are now gated, but broad compile coverage is still incomplete.
- **次に確認すべきこと:** Build the optional Plugin Host object after its imported module BMIs are available; a Ninja target request currently expands to a multi-thousand-action OpenUSD dependency graph.

## 2026-10-25: clang-cl module units cannot inherit the MSVC Qt forced include
- **関連:** `ArtifactCore/CMakeLists.txt`, `include/Compat/QtListForward.h`, clang-cl module compilation
- **確認できた事実:** In the clang-cl build, the target-wide `/FI.../QtListForward.h` was applied before module interfaces and produced `module; ... can appear only at the start of the translation unit`. Restricting this forced include to `ARTIFACT_COMPILER_MSVC_CL` allowed clang-cl to compile the migrated `src/Animation/AnimatableTransform2D.cpp` and its module prerequisites. The compiler still warns that `/std:c++latest` and `/experimental:module` are unused under this lane.
- **価値または懸念:** Other clang-cl module source units may rely on declarations currently supplied only by the forced include, so one successful target does not prove the full ArtifactCore or application build. Clang 22 P1689 scan succeeded across the selected dependency set after excluding `/FI`.
- **次に確認すべきこと:** Expand clang-cl builds across the remaining ArtifactCore and Artifact targets; then replace any newly exposed implicit Qt declarations with direct includes in the affected source units and remove MSVC-only switches from the clang-cl option branch.

## 2026-10-25: GNU Clang 22 repeatedly fails reading a large module PCM
- **関連:** `Artifact/src/Preview/ArtifactPreviewCompositionPipeline.cpp`, GNU Clang 22.1.3, generated Artifact module PCMs
- **確認できた事実:** The source's P1689 scan succeeds and records 21 imported modules. Compilation failed twice with `malformed or corrupted precompiled file: ran out of source locations`, including once after reconfiguring with sccache disabled. The unit was not part of the `.cppm` migration. The generated diagnostic identifies a Clang frontend crash; the exact imported PCM or compiler defect is not yet isolated.
- **価値または懸念:** The remaining GNU Clang build gap is not explained by sccache or parallel execution alone. Treating the partial build as fully passing would hide an unresolved compiler/module artifact issue.
- **次に確認すべきこと:** Isolate the failing imported PCM or reproduce with a newer/alternate upstream Clang build, then resume the GNU Clang app build and verify the final link.

## 2026-10-25: Preview pipeline import set reaches Clang source-location limit
- **関連:** `Artifact/src/Preview/ArtifactPreviewCompositionPipeline.cpp`, `Artifact/src/Preview/ArtifactPreviewCloneSelectionOverlay.cpp`
- **確認できた事実:** Direct compilation after removing imports already provided transitively by the primary interface still fails while importing the concrete `Solid2D` layer module. A reduced import probe reported about 2.13 GB of source locations loaded from AST files (99% of available space). Moving clone selection overlay into a second implementation unit did not resolve the main unit's failure. A standalone compile of the new unit could not be evaluated because its generated `.modmap` is absent from the partially built tree.
- **価値または懸念:** The failure is cumulative across the translation unit's imported AST graph; removing one direct import is insufficient. Any further splitting needs dependency-graph evidence and a fresh generated build tree.
- **次に確認すべきこと:** The temporary helper split did not reduce the main unit's source-location failure and was reverted because no module map or compile evidence was available. Measure import subsets in a generated tree before attempting another split; prefer compiler-supported source-location growth or a partition that demonstrably lowers cumulative AST imports.

## 2026-10-25: Clone layer implementation uses the existing renderer abstraction
- **関連:** `Artifact/src/Layer/ArtifactCloneLayer.cppm` → `.cpp`、`Artifact/include/Layer/ArtifactCloneLayer.ixx`、`Artifact/cmake/ArtifactSources.cmake`
- **確認できた事実:** The implementation declares `module Artifact.Layer.Clone;`, matching the public interface, and imports clone effect, mesh, composition, layer, and renderer modules. It includes `Graphics/InstanceData.h` but has no direct Diligent interface calls; the source path was migrated without changing clone generation or rendering behavior. LF endings are enforced. Clang scan/compile remain unverified.
- **価値または懸念:** The migration retains the current renderer abstraction and module API while allowing standard `.cpp` dependency scanning.
- **次に確認すべきこと:** Verify P1689 scanning and object compilation with Clone effect and Mesh module artifacts available.

## 2026-10-25: Content gizmo compile evidence was limited by missing provider PCMs
- **関連:** `Artifact/src/Widgets/Render/ArtifactContentGizmo.cppm` → `.cpp`、`Artifact/include/Widgets/Render/ArtifactContentGizmo.ixx`、`Artifact/cmake/ArtifactSources.cmake`
- **確認できた事実:** The implementation declares `module Artifact.Widgets.ContentGizmo;` and imports CompositionGizmoUndoCommands and Undo.UndoManager. A prior standalone compile could not proceed because those provider PCMs were absent; this was not a source diagnostic. The implementation path and manifest were moved to `.cpp`, with behavior unchanged and LF endings.
- **価値または懸念:** The normal Clang `.cpp` scan path should expose the imports to the generated target dependency graph, but full provider ordering remains unverified until the generated build graph is inspected.
- **次に確認すべきこと:** Verify a fresh P1689 scan and ensure the generated target schedules both provider interfaces before compiling the implementation.

## 2026-10-25: Composition audio mixer has two implementation units in one module
- **関連:** `Artifact/src/Widgets/ArtifactCompositionAudioMixerPresentation.cppm` → `.cpp`、`Artifact/src/Widgets/ArtifactCompositionAudioMixerWidget.cpp`、`Artifact/include/Widgets/ArtifactCompositionAudioMixerWidget.ixx`
- **確認できた事実:** Both source units declare `module Artifact.Widgets.CompositionAudioMixer;`; the presentation unit imports the existing audio mixer, service, event, playback, and undo modules. Its path was migrated without changing the shared module declaration, imports, presentation, or service calls. LF endings are enforced. Clang scan/compile remain unverified.
- **価値または懸念:** CMake module dependency scanning must treat both `.cpp` files as implementation units belonging to the same named module; only the `.ixx` remains the importable interface.
- **次に確認すべきこと:** Verify both implementation scans and target compilation together when build execution is authorized.

## 2026-10-25: Audio mixer widget stays behind its established module interface
- **関連:** `Artifact/src/Widgets/AudioMixerWidget.cppm` → `.cpp`、`Artifact/include/Widgets/AudioMixerWidget.ixx`、`Artifact/cmake/ArtifactSources.cmake`
- **確認できた事実:** The implementation declares `module Artifact.Widgets.AudioMixer;`, matching its public interface, and imports existing audio buses/effects/mixer plus VST and CLAP host modules. The source path was migrated; plugin editor UI, mixer behavior, and service calls were not edited. LF endings are enforced. Clang scan/compile remain unverified.
- **価値または懸念:** This puts the existing widget under normal `.cpp` Clang classification without changing audio functionality.
- **次に確認すべきこと:** Verify P1689 scanning and compile with the VST/CLAP and audio provider module artifacts available.

## 2026-10-25: Software render inspectors remain a UI adapter over the existing compositor
- **関連:** `Artifact/src/Widgets/Render/ArtifactSoftwareRenderInspectors.cppm` → `.cpp`、`Artifact/include/Widgets/Render/ArtifactSoftwareRenderInspectors.ixx`、`Artifact/cmake/ArtifactSources.cmake`
- **確認できた事実:** The implementation declares `module Artifact.Widgets.SoftwareRenderInspectors;`, matching its interface, and imports the existing SoftwareCompositor and layer/project modules. The implementation path was migrated; no inspector or rendering behavior was edited. LF endings are enforced. Clang scan/compile remain unverified.
- **価値または懸念:** This moves the existing inspector implementation to standard `.cpp` classification without adding software rendering features.
- **次に確認すべきこと:** Verify P1689 scanning and compilation when build execution is authorized.

## 2026-10-25: Playback control test widget keeps its existing service usage
- **関連:** `Artifact/src/Widgets/Control/ArtifactPlaybackControlTestWidget.cppm` → `.cpp`、`Artifact/include/Widgets/Control/ArtifactPlaybackControlTestWidget.ixx`、`Artifact/cmake/ArtifactSources.cmake`
- **確認できた事実:** The implementation declares `module Artifact.Widgets.PlaybackControlTestWidget;`, matching the public interface. Its source path was moved to `.cpp`; playback service calls, test/widget behavior, and Verdigris implementation were not changed. LF endings are enforced. Clang scan/compile remain unverified.
- **価値または懸念:** The implementation will now use the standard Clang `.cpp` scanning path while retaining the established service API boundary.
- **次に確認すべきこと:** Verify dependency scanning and object compilation when build execution is authorized.

## 2026-10-25: Composition audio mixer remains a view over existing service APIs
- **関連:** `Artifact/src/Widgets/ArtifactCompositionAudioMixerWidget.cppm` → `.cpp`、`Artifact/include/Widgets/ArtifactCompositionAudioMixerWidget.ixx`、`Artifact/cmake/ArtifactSources.cmake`
- **確認できた事実:** The implementation declares `module Artifact.Widgets.CompositionAudioMixer;`, matching the public interface, and imports the existing mixer, project, playback, audio service, event, and undo modules. Its path was migrated; no service calls, transport behavior, or audio feature logic were edited. LF endings are enforced. Clang scan/compile remain unverified.
- **価値または懸念:** The module boundary is preserved while Clang gets the implementation through the `.cpp` classification path.
- **次に確認すべきこと:** Verify P1689 scanning and compile after the mixer and service provider module artifacts are available.

## 2026-10-25: Procedural 3D layer keeps its existing public module boundary
- **関連:** `Artifact/src/Layer/ArtifactProcedural3DLayer.cppm` → `.cpp`、`Artifact/include/Layer/ArtifactProcedural3DLayer.ixx`、`Artifact/cmake/ArtifactSources.cmake`
- **確認できた事実:** The implementation declares `module Artifact.Layer.Procedural3D;`, matching its `.ixx`, and imports renderer, composition, shape/mask, material/mesh, image, frame/time, and ID modules. The source and manifest path were migrated; 3D layer behavior and rendering code were not changed. LF endings are enforced. Clang scan/compile remain unverified.
- **価値または懸念:** This aligns a 3D layer implementation with the standard `.cpp` compiler classification while preserving its module API.
- **次に確認すべきこと:** Verify P1689 scanning and implementation compilation when the module dependency artifacts are available.

## 2026-10-25: Noise layer implementation preserves its existing GPU and image path
- **関連:** `Artifact/src/Layer/ArtifactNoiseLayer.cppm` → `.cpp`、`Artifact/include/Layer/ArtifactNoiseLayer.ixx`、`Artifact/cmake/ArtifactSources.cmake`
- **確認できた事実:** The implementation declares `module Artifact.Layers.Noise;`, matching its public interface, and imports layer, image, procedural texture, GPU compute, property, and expression modules. The source path was migrated; procedural generation and GPU behavior were not edited. LF endings are enforced. Clang scan/compile remain unverified.
- **価値または懸念:** This brings a static-image-related layer implementation under normal `.cpp` Clang classification without changing its render path.
- **次に確認すべきこと:** Verify P1689 scanning and object compilation with the procedural texture and GPU compute providers available.

## 2026-10-25: Text gizmo implementation preserves the approved module boundary
- **関連:** `Artifact/src/Widgets/Render/ArtifactTextGizmo.cppm` → `.cpp`、`Artifact/include/Widgets/Render/ArtifactTextGizmo.ixx`、`Artifact/cmake/ArtifactSources.cmake`
- **確認できた事実:** The implementation declares `module Artifact.Widgets.TextGizmo;`, matching the public `.ixx`, and imports text/layer/composition, renderer, layout, expression, undo, and frame-rate modules. The source path was moved to `.cpp`; gizmo behavior, rendering/input logic, and public API were not edited. LF endings are enforced. Clang scan/compile remain unverified.
- **価値または懸念:** Clang can classify the existing implementation through the standard `.cpp` path without changing the approved gizmo design.
- **次に確認すべきこと:** Verify fresh P1689 scan and compile when dependency artifacts are available.

## 2026-10-25: Animation menu keeps its existing module and audio binding behavior
- **関連:** `Artifact/src/Widgets/Menu/ArtifactAnimationMenu.cppm` → `.cpp`、`Artifact/include/Widgets/Menu/ArtifactAnimationMenu.ixx`、`Artifact/cmake/ArtifactSources.cmake`
- **確認できた事実:** The implementation declares `module Menu.Animation;`, matching its interface, and imports timeline, audio analysis/segment, property, event, and undo modules. The source path was migrated; its W_OBJECT implementation, menu actions, audio-reactive binding commands, and behavior were not changed. LF endings are enforced. Clang scan/compile remain unverified.
- **価値または懸念:** Standard `.cpp` classification can cover this module implementation without changing the menu API or transport ownership.
- **次に確認すべきこと:** Verify P1689 scanning and implementation compilation with its UI/audio provider modules available.

## 2026-10-25: Composition menu remains paired with the Menu.Composition interface
- **関連:** `Artifact/src/Widgets/Menu/ArtifactCompositionMenu.cppm` → `.cpp`、`Artifact/include/Widgets/Menu/ArtifactCompositionMenu.ixx`、`Artifact/cmake/ArtifactSources.cmake`
- **確認できた事実:** The implementation declares `module Menu.Composition;`, matching its public interface, and imports project, playback, render queue, composition, dialog, and property modules. Its source path was migrated without changing imports, `W_OBJECT_IMPL`, actions, or UI behavior; LF endings are enforced. Clang scan/compile remain unverified.
- **価値または懸念:** The implementation now uses standard `.cpp` classification while preserving menu responsibilities and APIs. The large dependency set may still make standalone Clang compilation costly.
- **次に確認すべきこと:** Verify P1689 scanning and compile after dependency module artifacts are available.

## 2026-10-25: Software render test widget has a separate implementation unit
- **関連:** `Artifact/src/Widgets/Render/ArtifactSoftwareRenderTestWidget.cppm` → `.cpp`、`Artifact/include/Widgets/Render/ArtifactSoftwareRenderTestWidget.ixx`、`Artifact/cmake/ArtifactSources.cmake`
- **確認できた事実:** The implementation declares `module Artifact.Widgets.SoftwareRenderTest;`, matching its public interface, and imports SoftwareCompositor, Layer.Blend, Project service, and Application manager. The implementation path was changed to `.cpp`; the widget and software rendering code were not changed. LF endings are enforced. Clang scan/compile remain unverified.
- **価値または懸念:** This is a test-facing adapter and changes no renderer capability. It is now eligible for the normal `.cpp` Clang source classification.
- **次に確認すべきこと:** Verify its dependency scan and compile when build execution is authorized.

## 2026-10-25: Project manager implementation preserves its dedicated module interface
- **関連:** `Artifact/src/Project/ArtifactProjectManager.cppm` → `.cpp`、`Artifact/include/Project/ArtifactProjectManager.ixx`、`Artifact/cmake/ArtifactSources.cmake`
- **確認できた事実:** The implementation declares `module Artifact.Project.Manager;`, matching the existing interface, and imports project, settings, event, diagnostics, and asset modules. The source path was moved to `.cpp`; imports, `W_OBJECT_IMPL`, and project behavior were not edited. LF endings are enforced. Clang scan/compile remain unverified.
- **価値または懸念:** This keeps project management APIs behind their current module boundary while using the Clang `.cpp` dependency scan path. Its 24-module closure may still have a high compile cost.
- **次に確認すべきこと:** Verify fresh P1689 scanning and implementation compilation when the required dependency artifacts are available.

## 2026-10-25: File menu implementation remains paired with the approved File Menu interface
- **関連:** `Artifact/src/Widgets/Menu/ArtifactFileMenu.cppm` → `.cpp`、`Artifact/include/Widgets/Menu/ArtifactFileMenu.ixx`、`Artifact/cmake/ArtifactSources.cmake`
- **確認できた事実:** The implementation declares `module Artifact.Menu.File;`, matching its public `.ixx`, and imports project, export, dialog, layer, and render queue modules. Existing `filemenu_*.svg` references were left untouched. The source path was migrated with only LF normalization; module imports and menu behavior were unchanged. Clang scan/compile remain unverified.
- **価値または懸念:** This preserves the existing menu and icon approval boundary while allowing the implementation to use the standard `.cpp` classification.
- **次に確認すべきこと:** Verify fresh Clang P1689 scanning and compile/link when build execution is authorized.

## 2026-10-25: AI client implementation keeps its AI.Client module boundary
- **関連:** `Artifact/src/AI/AIClient.cppm` → `.cpp`、`Artifact/include/AI/AIClient.ixx`、`Artifact/cmake/ArtifactSources.cmake`
- **確認できた事実:** The implementation declares `module AI.Client;`, matching the existing public interface, and contains AI agent setup, provider selection, and asynchronous request state. The source and manifest path were migrated without changing module imports or implementation logic; LF endings are enforced. Clang scan/compile remain unverified.
- **価値または懸念:** This lets Clang classify the implementation through its standard `.cpp` extension while retaining the current API boundary. Its broad provider imports may require a large PCM dependency closure.
- **次に確認すべきこと:** Verify P1689 scanning and compile with the required AI provider module artifacts available.

## 2026-10-25: Tool options remain behind the existing Widgets.ToolOptionsBar interface
- **関連:** `Artifact/src/Widgets/ArtifactToolOptionsBar.cppm` → `.cpp`、`Artifact/include/Widgets/ArtifactToolOptionsBar.ixx`、`Artifact/cmake/ArtifactSources.cmake`
- **確認できた事実:** The implementation declares `module Widgets.ToolOptionsBar;`, matching its public interface, and has 12 direct module imports. Its implementation path was changed to `.cpp`; imports, `W_OBJECT_IMPL`, event routing, and option behavior were left unchanged. LF endings are enforced. Clang scan/compile remain unverified.
- **価値または懸念:** Migrating the existing source preserves the module API while bringing this UI implementation under ordinary `.cpp` compiler classification. Verdigris and dependency closure still require Clang verification.
- **次に確認すべきこと:** Verify fresh P1689 scanning and implementation compilation when the build restriction is lifted.

## 2026-10-25: Toolbar behavior stays behind the existing Widgets.ToolBar module
- **関連:** `Artifact/src/Widgets/ArtifactToolBar.cppm` → `.cpp`、`Artifact/include/Widgets/ArtifactToolBar.ixx`、`Artifact/cmake/ArtifactSources.cmake`
- **確認できた事実:** The implementation declares `module Widgets.ToolBar;`, matching its existing interface, and imports the current tool, settings, workspace, event, and toolbar modules. The implementation path was changed to `.cpp`; imports, `W_OBJECT_IMPL`, and toolbar behavior were not edited. LF endings are enforced. Clang scan/compile remain unverified.
- **価値または懸念:** This removes one UI implementation from the legacy extension while preserving its public module boundary. Verdigris compatibility with the selected Clang generator still needs compile verification.
- **次に確認すべきこと:** Verify its P1689 scan and object compilation after the provider module artifacts are available.

## 2026-10-25: Shader graph widget has a narrow named-module dependency boundary
- **関連:** `Artifact/src/Widgets/ArtifactShaderGraphWidget.cppm` → `.cpp`、`Artifact/include/Widgets/ArtifactShaderGraphWidget.ixx`、`Artifact/cmake/ArtifactSources.cmake`
- **確認できた事実:** The implementation declares `module Artifact.Widgets.ShaderGraphWidget;` and imports ShaderNode.Core, CompositionEditor, and CompositionRenderController. Its public `.ixx` remains unchanged. The implementation and manifest path were moved to `.cpp`; widget behavior and module imports were not edited. LF endings are enforced. Clang scan/compile remain unverified.
- **価値または懸念:** This reduces reliance on `.cppm` extension classification while preserving the existing named-module interface. Clang compilation may still depend on large upstream module interfaces.
- **次に確認すべきこと:** Verify a fresh Clang dependency scan and implementation compile with required provider module artifacts available.

## 2026-10-25: ArtifactWidgets Clang source scan covered only legacy `.cppm` implementations
- **関連:** `ArtifactWidgets/CMakeLists.txt`、`ARTIFACT_COMPILER_CLANG_GNU`、`ARTIFACT_COMPILER_CLANG_CL`
- **確認できた事実:** The source-specific Clang loop previously ran only for `ARTIFACT_COMPILER_CLANG_GNU` and only applied `CXX_SCAN_FOR_MODULES` plus `-x c++` to non-interface `.cppm` files. It had no explicit `.cpp` module/import scan branch and no clang-cl source-specific branch. The loop now includes both Clang compiler lanes, applies `-x c++` only to GNU-style `.cppm`, and enables module scanning on `.cpp` files with a module declaration or import. No configure/build was run.
- **価値または懸念:** This aligns dependency scanning with the `.cpp` migration path and avoids passing the GNU driver option to clang-cl. The CMake behavior on each generator still requires an authorized configure/scan verification.
- **次に確認すべきこと:** Configure clang-cl and upstream Clang separately, then inspect generated dependency scan rules for module implementations and import consumers in ArtifactWidgets.

## 2026-10-25: Motion sketch remains an implementation behind its existing tool interface
- **関連:** `Artifact/src/Tool/ArtifactMotionSketchTool.cppm` → `.cpp`、`Artifact/include/Tool/ArtifactMotionSketchTool.ixx`、`Artifact/cmake/ArtifactSources.cmake`
- **確認できた事実:** The implementation declares `module Artifact.Tool.MotionSketchTool;`, matching the public `.ixx`. It imports existing layer, composition, event, property, time, and undo modules. The file was migrated with its source content unchanged and LF endings; no event or tool behavior was changed. Clang scanning/compilation remain unverified.
- **価値または懸念:** The existing source-only module boundary can be retained while allowing Clang's `.cpp` source classification to scan the named module implementation.
- **次に確認すべきこと:** Verify fresh P1689 scanning and object compilation when build execution is authorized.

## 2026-10-25: Rasterizer Glow has an established named-module implementation boundary
- **関連:** `Artifact/src/Effects/Rasterizer/GlowEffect.cppm` → `.cpp`、`Artifact/cmake/ArtifactSources.cmake`、`Artifact.Effect.Rasterizer.Glow`
- **確認できた事実:** The source declares `module Artifact.Effect.Rasterizer.Glow;`, and the implementation manifest is its only active CMake source reference. The source was moved to `.cpp` with LF endings; shader code and CPU/GPU effect behavior were not edited. A `.cppm` string in `Artifact/CMakeLists.txt` belongs to an old history comment, not an active target path.
- **価値または懸念:** Upstream Clang can use the existing `.cpp` named-module scan classification without a special per-file source pin. Actual Clang compilation remains unverified.
- **次に確認すべきこと:** Verify fresh P1689 scanning and implementation compilation when build execution is authorized.

## 2026-10-25: Layer panel presentation is a source-only module implementation
- **関連:** `Artifact/src/Widgets/Timeline/ArtifactLayerPanelPresentation.cppm` → `.cpp`、`Artifact/include/Widgets/Timeline/ArtifactLayerPanelWidget.ixx`、`Artifact/cmake/ArtifactSources.cmake`
- **確認できた事実:** The implementation declares `module Artifact.Widgets.LayerPanelWidget;`, matching the existing public `.ixx` interface, and contains presentation descriptor functions. The extension and manifest path were updated without changing the module declaration, imports, or UI behavior. LF endings were preserved. No compiler scan or build was run.
- **価値または懸念:** Keeping this logic in the existing module implementation preserves the interface boundary and lets the Clang `.cpp` scan path handle the named module declaration.
- **次に確認すべきこと:** Verify fresh Clang P1689 scanning and implementation compilation when build execution is authorized.

## 2026-10-25: ArtifactPr implementation manifest no longer depends on the `.cppm` extension
- **関連:** `ArtifactPr/CMakeLists.txt`、`ArtifactPr/src/GpuProgramMonitor.cppm` → `.cpp`、Clang source scanning
- **確認できた事実:** After the final implementation source was moved to `.cpp`, the 28-entry `ARTIFACT_PR_IMPL_SOURCES` manifest uses `.cpp` for each entry. The CMake source-classification loop detects named `module X;` declarations in `.cpp` files and enables `CXX_SCAN_FOR_MODULES` for Clang GNU and clang-cl; the legacy `.cppm` path is retained only as a compatibility branch for other future entries. Configuration and compilation were not run.
- **価値または懸念:** This removes ArtifactPr's current reliance on compiler-specific interpretation of implementation `.cppm` files. The new source classification still needs verification on supported Clang generators.
- **次に確認すべきこと:** When build execution is authorized, configure Clang GNU and clang-cl separately, inspect generated P1689 dependency rules for the module implementation `.cpp` sources, and compile/link ArtifactPr targets.

## 2026-10-25: ArtifactPr editor engine implementation remains behind a module interface
- **関連:** `ArtifactPr/src/ArtifactPrEditorEngine.cppm` → `.cpp`、`ArtifactPr/include/ArtifactPrEditorEngine.ixx`、`ArtifactPr/CMakeLists.txt`
- **確認できた事実:** The source declares `module ArtifactPr.EditorEngine;` and imports ClipEffects, EditCommand, SequenceExporter, and Translation. Its public interface remains the `.ixx`; the implementation path was changed to `.cpp`. LF endings are preserved. No compilation or dependency scan was run.
- **価値または懸念:** The implementation contains serialization and editor operations, but this migration does not alter those behaviors. Compiler compatibility is still unverified.
- **次に確認すべきこと:** When build/scan execution is authorized, verify its P1689 dependency record and compile after required provider module artifacts are available.

## 2026-10-25: ArtifactPr main window implementation uses a complete module interface boundary
- **関連:** `ArtifactPr/src/ArtifactPrMainWindow.cppm` → `.cpp`、`ArtifactPr/include/ArtifactPrMainWindow.ixx`、`ArtifactPr/CMakeLists.txt`
- **確認できた事実:** implementation unit declares `module ArtifactPr.MainWindow;` and imports sibling modules; its public interface remains the `.ixx`. The source was moved to `.cpp` and the implementation manifest path updated. LF endings were preserved. Compilation and module dependency scanning were not run.
- **価値または懸念:** This is a large UI implementation (about 2,700 lines), so static path and module-boundary checks alone do not establish compiler compatibility.
- **次に確認すべきこと:** When builds are authorized, verify fresh Clang P1689 scanning and compilation for the migrated implementation with its provider modules available.

## 2026-10-24: Verdigris QWidget implementation may expose Clang module ownership ambiguity

- **関連:** `Artifact/src/Widgets/ArtifactSecondaryPreviewWindow.cppm`、`Artifact/include/Widgets/ArtifactSecondaryPreviewWindow.ixx`、Clang 23.1.4 / Windows MSVC ABI
- **確認できた事実:** Fresh P1689 scan succeeded, but the implementation compile failed while expanding `W_OBJECT_IMPL(ArtifactSecondaryPreviewWindow)`: Clang reported ambiguous `operator->` lookup at `QObject::d_ptr` with `QScopedPointer<QObjectData>` candidates, then the frontend terminated by signal. The `.cpp` migration was reverted and the manifest returned to `.cppm`. No source diagnostic outside this macro expansion was established.
- **価値または懸念:** A clean module scan and available PCM do not establish that Verdigris implementation macros are valid across the named-module boundary. The exact cause is unverified and should not be classified as a simple missing include or extension-only issue.
- **次に確認すべきこと:** Before retrying this widget, inspect its Verdigris macro expansion and Qt declarations as imported through its interface module; keep the implementation on `.cppm` until a source-level fix is justified and compiled.

## 2026-10-24: A module source migration may need both the manifest and a direct APP_IMPL registration updated

- **関連:** `Artifact/cmake/ArtifactSources.cmake`、`Artifact/CMakeLists.txt`、`ArtifactSwitchLayer.cppm`、`ArtifactPaintLayer.cppm`
- **確認できた事実:** SwitchLayerとPaintLayerはsource manifestのsource pathを `.cpp` に変更した後も、Artifact/CMakeLists.txt内の明示 `APP_IMPL` entryが `.cppm` を参照していた。CMake configureは `file(STRINGS)` で旧pathを読み込もうとして失敗した。2つの明示entryを `.cpp` に更新するとconfigure / generateが成功した。
- **価値または懸念:** source manifestのpath置換だけでは足りず、CMakeLists.txtの明示target pinや直接ファイル参照が残ることがある。拡張子移行ではsource pathをrepository-wide検索し、manifestと明示登録の両方を整合させる。
- **次に確認すべきこと:** 将来の `.cppm` → `.cpp` 移行前後に `rg` で旧basenameをCMakeLists / cmake directory全体から検索し、file read / target pinの参照漏れを確認する。

## 2026-10-24: Event payloads should not redeclare widget classes owned by another module

- **関連:** `Artifact/include/Event/ArtifactEventTypes.ixx`、`Artifact/include/Widgets/Render/Artifact3DModelViewer.ixx`、`Artifact/src/Widgets/Render/Artifact3DModelViewer.cpp`
- **確認できた事実:** Event.Typesが `Artifact3DModelViewer*` を持つためmodule purview内で同名classをforward-declareし、ModelViewer interfaceが同名classを定義していた。Clang implementation compileは、同じ宣言が異なるnamed moduleに所属するとして拒否した。payload sourceを `QObject*` にしてglobal module fragmentへ直接includeを加えた後、Event.Types PCMとModelViewer implementation object compileが成功した。既存consumerは `event.source != modelViewer` でsource identityを比較し、producerは `{this, mode}` で初期化している。
- **価値または懸念:** event identityにwidget派生型を露出しない設計はnamed-module ownershipの重複を避け、event busのpayloadを利用側の具象widgetから疎結合にできる。ただし変更後の全target link / runtimeは未検証。
- **次に確認すべきこと:** 他のevent payloadにwidget具象型のpointerがあり、別interface moduleでもforward declaration / 定義されていないか調べる。変更を広げる際は利用側比較とaggregate constructionを確認する。

## 2026-10-24: PCM file presence alone does not guarantee a source is safe to compile in the current Clang session

- **関連:** `Artifact/src/Widgets/Render/ArtifactLayerCompositeTestWidget.cppm`、`ArtifactCompositionRenderOverlay.cppm`、Clang 23.1.4 / Windows MSVC ABI
- **確認できた事実:** LayerCompositeTestWidget の全direct required PCMが存在し、fresh P1689 scanとpublic interface PCM compileも成功したが、implementation compileはAST source-location使用量約2.13 GB（診断上99%）でfrontend exit 70となった。別batchのCompositionRenderOverlayでも約2.15 GBでcrashしている。
- **価値または懸念:** 欠落PCMを解消しても、import closure全体のsource-location tableがClang frontendの上限に達する可能性がある。PCM存在チェックだけで対象sourceのcompile可能性を予測できない。
- **次に確認すべきこと:** 再試行前にmodule closureの大きさを評価し、frontend RAMを監視する。上限に達した候補はsource error扱いせず、根拠ある依存縮小まで `.cppm` に残す。

## 2026-10-24: A dialog's public interface can be the high-memory step even when its implementation module has few imports

- **関連:** `Artifact/include/Widgets/Dialog/QuickLayerCreationDialog.ixx`、`Artifact.Menu.Layer`、Clang 23.1.4 / Windows MSVC ABI
- **確認できた事実:** QuickLayerCreationDialog implementation fresh P1689 scanは成功した。interfaceがimportする `Artifact.Menu.Layer` のPCMを生成した後、QuickLayerCreationDialog interface PCM compile中にClang working setは約5.5 GiBとなり、空きRAMが約0.6 GiBまで下がったためcompileを中断し、source migrationを戻した。別候補のCreatePlaneLayerDialogもfresh implementation scan後のinterface PCM compileがworking set約4.9 GiB / 空きRAM約0.7 GiBまで達したため中断した。いずれもimplementation import数が少ないことはinterface closure全体のメモリコストを示さなかった。
- **価値または懸念:** implementation import数やsource byte数だけでなく、public interfaceが再export/importするmodule closureを見てClang適合候補を選ぶ必要がある。interface PCM compileもメモリ監視対象にする。
- **次に確認すべきこと:** QuickLayerCreationDialogを再試行する前に `Artifact.Menu.Layer` interfaceの公開依存closureを確認し、既存PCM再利用または依存境界を狭める根拠があるか調べる。

## 2026-10-24: Module implementation consumers need direct imports for owner-defined value types

- **関連:** `Artifact/src/Layer/ArtifactSwitchLayer.cpp`、`Utils.Id`、Clang 23.1.4 / Windows MSVC ABI
- **確認できた事実:** SwitchLayer implementationは `LayerID` を `Artifact.Layer.Abstract` 経由で使っていたが、Clangはowner module `Utils.Id` の直接importを要求した。implementationに `import Utils.Id;` を追加するとfresh P1689 scanと個別object compileが成功した。
- **価値または懸念:** あるmoduleのinterfaceから別module型が見えていても、implementationの利用箇所で所有元moduleを明示importする必要がある場合がある。拡張子を変えるだけで診断を終わらせず、型のmodule ownerに沿って依存を直す。
- **次に確認すべきこと:** Clangが未移行 `.cppm` の型可視性エラーを出した場合、型定義moduleを特定し、implementation単位に直接importを加えて再scan / compileする。

## 2026-10-24: `_2D` module componentはClangの予約名warningを発生させる

- **関連:** `Artifact/src/Layer/ArtifactAbstract2DLayer.cpp`、`Artifact.Layers.Abstract._2D`、Clang 23.1.4 / Windows MSVC ABI
- **確認できた事実:** `ArtifactAbstract2DLayer` implementation objectはcompile成功したが、既存named module component `_2D` に対してClangが `-Wreserved-module-identifier` warningを出した。
- **価値または懸念:** source filenameの拡張子移行ではこのidentifier warningは解消しない。logical module名を変更するにはpublic interfaceと全importerの協調更新が必要なため、今回の拡張子移行の範囲外である。
- **次に確認すべきこと:** module名の診断対応を別途行う場合は、全 `_2D` 宣言とimportを調査し、`.cppm` → `.cpp` とは分離したatomicなmodule名移行を計画する。

## 2026-10-24: Clang source-location limit affects some implementation module closures

- **関連:** `Artifact/src/Widgets/Render/ArtifactCompositionRenderOverlay.cppm`、`Artifact/src/Widgets/Render/ArtifactCompositionViewerFooter.cpp`、`Artifact/src/Widgets/Dialog/CreateCameraLayerDialog.cpp`、`Artifact/src/Diagnostics/AppValidationRules.cpp`、`Artifact/src/Render/Software/ArtifactSoftwareImageCompositor.cpp`、`Artifact/src/Layer/ArtifactFormParticleLayer.cpp`、`Artifact/src/Layer/ArtifactSolidImageLayer.cpp`、`Artifact/src/Layer/ArtifactGroupLayer.cpp`、`Artifact/src/Layer/ArtifactLightLayer.cpp`、Clang 23.1.4 / Windows MSVC ABI
- **確認できた事実:** CompositionRenderOverlayのimplementation compileはAST内source locations約2.15 GB（診断上99%）でfrontend exit 70となった。CompositionViewerFooter、CreateCameraLayerDialog、ArtifactSolidImageLayerはfresh P1689 scan後のcompileでそれぞれ約2.13 GB / 99%に達してexit 70。一方、同じ作業のAppValidationRules、ArtifactSoftwareImageCompositor、ArtifactFormParticleLayer、ArtifactGroupLayer、ArtifactLightLayerはfresh scanとimplementation compileに成功した。NLETransitionBridgeも単独compile成功。
- **価値または懸念:** 直接import数やsource byte数だけでClangのcompile可否は予測できない。module closureのAST負荷がtranslation unitごとに異なるため、fresh scanとimplementation compileを分けて確認する必要がある。
- **次に確認すべきこと:** source-location上限に達したUI translation unitは、import closureを具体的に縮小できる根拠が得られるまで再試行しない。AppValidationRules成功を根拠に他のclosureへ一般化しない。
## 2026-10-24: Clang module implementation では所有元 module の型を直接 import する

- **関連:** `Artifact/src/Widgets/Diagnostics/FramePipelineViewWidget.cpp`、`Graphics.RenderGraph`、Clang 22.1.3 / P1689
- **確認できた事実:** `FramePipelineViewWidget.cpp` は interface がimportした `Graphics.RenderGraph` の `ArtifactCore::RenderResourceHandle` を実装内で使っていた。`ProfilerOverlayWidget.cpp` / `ProfilerPanelWidget.cpp` でも `ArtifactCore::toStdString` の所有元 `Core.ArtifactString` が間接的にしか可視でなかった。Clang compileは直接import不足を報告し、implementationだけに所有moduleのimportを追加すると各fresh scanとcompileが成功した。
- **価値／懸念:** 既存interface越しに見える型であっても、implementationの利用moduleを直接importするとClang frontend間の可視性差を早期に検出できる。依存追加は実装側に閉じ、公開interfaceのimport集合を不必要に広げない。
- **次に確認:** 他の移行sourceで同様のtransitive visibility依存が出るか、Clang compile結果に基づき個別に確認する。

## 2026-10-22: 実 ArtifactCore module target でも sccache が Clang cache hit を返す

- **関連:** upstream clang++ 22.1.3 / MSVC ABI、`ArtifactCoreEnvironment`、sccache 0.18.0、`.cppm` → `.cpp` migration
- **確認できた事実:** vcpkg `FindStb.cmake` の directory を `CMAKE_MODULE_PATH` に明示して root configure/generate が成功。sccache launcher 下で `ArtifactCoreEnvironment` を2回 clean/build し、1回目は3 compile miss、2回目は3 compile hit、0 non-cacheable calls。2回とも `.ixx` / `.cpp` compile と static archive link が成功した。
- **価値／懸念:** isolated smoke だけでなく実ソースの CMake module target に cache が適用されることを確認できた。全 ArtifactCore / ArtifactWidgets / アプリ全体の cache hit・成果物一致は未確認。
- **次に確認:** 別 split target で Clang P1689 / module map の依存順と sccache hit を検証し、clang-cl root 側の third-party option 問題と切り分けて全体対応を進める。- **追加確認（2026-10-23）:** ImageProcessing partition implementations `ChromaSpreadGlow.cppm` / `EdgeEcho.cppm` を `.cpp` へ移し、両本文がHEADと一致すること、upstream clang++でscan・compile・ArtifactCore.lib link成功を確認。clang-clではChromaSpreadGlowの実interface/implementationとOpenCV headersを使ったfixture compile/link、sccache 1 miss→1 hitが成功した。EdgeEchoのclang-cl fixtureは元HEADにもある `FloatRGBA` 未宣言で停止し、拡張子移行と独立した既存可視性問題と判断。**未検証:** root clang-cl全体target。

## 2026-10-22: upstream clang++ の広い target 検証は OpenUSD Windows min macro で停止

- **関連:** `ArtifactCoreColorCollection` target、`libs/openusd/pxr/base/arch/stackTrace.cpp:1653`、Windows SDK `min` macro
- **確認できた事実:** root CMake configure/generate 後に ColorCollection target を開始したところ、依存 OpenUSD の `std::min(frameCount, maxdepth)` が Windows `min` macro 展開で compile error となり、target の自ソースに到達しなかった。
- **変更・確認:** `arch` target だけに親 CMake から `NOMINMAX` を追加すると `stackTrace.cpp` は compile できた。続いて `demangle.cpp` の `cxxabi.h`、`errno.cpp` の `strerror_r`、`attributes.cpp` の platform/compiler branch mismatch で止まることを確認。OpenUSD submodule の source は変更していない。`-U__clang__` を arch target 全体へ適用する案は MSVC STL の builtin math detection を壊したため撤回。
- **価値／懸念:** Core split target の P1689 / cache integration はこの試行では判定できない。GNU frontend と MSVC ABI を別軸にするには OpenUSD の compiler feature test が `__clang__` と `_MSC_VER` の両方を区別する必要がある。
- **次に確認:** OpenUSD 側に用意済みの CMake option / feature override で Clang MSVC ABI を構成できるか調べる。無ければ dependency patch 運用が必要かユーザー判断を仰ぐ。

## 2026-10-22: Verdigris 実装を含む Widget module の `.cpp` 移行と cache hit

- **関連:** `ArtifactWidgets/src/Common/ClickableLabel.cpp`、`include/Common/ClickableLabel.ixx`、Qt6、Verdigris、clang-cl / upstream clang++、sccache
- **確認できた事実:** `ClickableLabel.cppm` は Verdigris の `W_OBJECT_IMPL` と mouse event 実装を含む。`.cpp` へ拡張子のみ移し、両ファイルの Git blob hash が元 `.cppm` と一致することを確認。CMake 4.4.4 / Ninja で clang-cl と upstream clang++ + MSVC ABI の両方が module scan、compile、Qt6 DLL link に成功。Upstream clang++ / sccache launcher 下で同 target の clean/build 2回から4 compile cache hit を確認した。
- **価値／懸念:** 実装が単純な no-op ではない UI module でも、両 Clang frontend で `.cpp` source extension と module interface の組み合わせが成立する。Widget 全体の link は未確認。
- **次に確認:** UI module の次候補を依存と責務から選び、ArtifactWidgets target の実際の module dependency graph へ移行を広げる。

## 2026-10-22: DialogPlacement module を両 Clang frontend で `.cpp` として構築

- **関連:** `ArtifactWidgets/src/Common/DialogPlacement.cpp`、`include/Common/DialogPlacement.ixx`、Qt6、clang-cl / upstream clang++、sccache
- **確認できた事実:** `DialogPlacement.cppm` は Qt event filter と dialog centering を実装する。`.cpp` への移行後、CMake 4.4.4 / Ninja の clang-cl と upstream clang++ + MSVC ABI で module scan、interface / implementation compile、Qt6 DLL link が通った。upstream clang++ sccache 下で同 target の2回の clean/build に対して累計4 compile hits、0 non-cacheable calls。初回 pilot build で cache が温まっていたため miss は含まない。
- **価値／懸念:** Verdigris macro に依存しない Qt utility module でも同じ拡張子移行経路を確認できた。これは isolated target の検証で、ArtifactWidgets 全体 target の構築ではない。
- **次に確認:** source manifest と module graph を保ちつつ Widgets の移行候補を広げる。全体ビルドは OpenUSD ABI 分岐と clang-cl の third-party flags の解消後に検証する。


## 2026-10-21: CMake 4.4 で clang-cl と `.cpp` implementation unit を分離検証できる

- **関連:** ルート `CMakeLists.txt` clang-cl scanner gate、`ArtifactWidgets/src/Dock/Pane.cpp`、CMake 4.4.4、sccache v0.18.0
- **確認できた事実:** CMake 4.4.4 / Ninja / clang-cl 22.1.3 で実 Pane `.cpp` と `.ixx` を compile し、Qt6 Core / Widgets を link できた。CMake 4.2.2 では clang-cl module rule がなく、CMake 4.4 では追加される。sccache では clang-cl importer / implementation `.cpp` が cache hit、interface `.ixx` は `multiple input files` で bypass した。
- **価値／懸念:** 拡張子移行の実現可能性と root 全体の toolchain 互換を分けて評価でき、clang-cl の `.cpp` module implementation をキャッシュ対象にできる可能性が実測できた。Root target は OpenUSD の clang-cl 非互換 warning flags で停止し、全体対応は未完了。
- **次に確認:** clang-cl 用 OpenMP 自動検出と OpenUSD warning option の責務を依存側で分離し、root target の段階的 build を再開する。sccache cache hit は固定版・隔離 target で確認し続ける。

## 2026-10-21: Windows upstream clang++ は GNU frontend と MSVC ABI を別々に成立させる必要がある

- **関連:** `CMakeLists.txt` の `ARTIFACT_FRONTEND_MSVC` / MSVC SDK setup / std.ifc、`cmake/toolchain-msvc-v143.cmake`、`.cppm` 実装移行
- **確認できた事実:** 現行ビルド構成は clang-cl を `Clang` + MSVC frontend として扱い、Windows SDK / ATL 探索や `/reference`・MSVC `std.ifc` 供給の一部は `MSVC` 変数または MSVC frontend に結び付く。Windows upstream `clang++` は通常 GNU frontend として検出されるため、既存 clang-cl レーンに compiler path だけ差し替えてもこれらの設定に入らない。Visual Studio LLVM 配下には `clang++.exe` と `clang-scan-deps.exe` が存在する。
- **未検証の仮説:** clang++ `--target=x86_64-pc-windows-msvc`、MSVC STL、Windows SDK、vcpkg `x64-windows`、COFF linker を組み合わせれば、まず portable な C++ module subset を作れる可能性がある。Qt / Diligent / OpenUSD 等を含む全体ビルド成立は確認していない。
- **価値／懸念:** compiler frontend と object / library ABI を独立した軸として扱い、GNU frontend + MSVC ABI のための明示 toolchain を設計できる。既存の cl.exe 前提 v143 toolchain や MSVC IFC をそのまま流用すると、誤った configure 成功や module format の混在につながる。
- **次に確認:** upstream clang の配布元・版・driver options、CMake / Ninja scanner 対応版を固定し、最小 module provider/importer と `.cpp` implementation unit の configure・build feasibility を確認後、Windows SDK / vcpkg / Qt / Diligent を段階追加する。

## 2026-10-21: upstream clang++ の P1689 と ArtifactCore 既存依存設定

- **関連:** `tools/clang-msvc-abi-smoke/implementation.cpp`、CMake Ninja P1689 dyndep、ArtifactCore `.cppm` 実装スキャン
- **確認できた事実:** Clang 22.1.3 の `clang-scan-deps -format=p1689` が `module Smoke.Greeting;` 実装に `requires: Smoke.Greeting` を出すのは正しい P1689 attachment 情報であり、CMake 4.4.4 の最小 Ninja smoke はこの形を処理できる。ArtifactCore の分割 target では、既存 MSVC 用 `OBJECT_DEPENDS` が interface object を実装 object の前提にしながら、P1689 dyndep 側も実装 object を PCM producer の前提にしていたため cycle が発生していた。
- **変更・検証:** upstream GNU frontend では module implementation scan を維持し、MSVC 固有の `/reference` と `CMakeFiles/...ifc/.obj/.dd` 手動依存を除外。`-x c++` で `.cppm` を Clang に実装単位として認識させた。実際の `ArtifactCoreEnvironment` target を CMake 4.4.4 / Ninja で構成・ビルドし、`EnvironmentVariable.cppm` を `.cpp` に移した後も scan、compile、static archive 作成が成功した。
- **価値／懸念:** P1689 の正規依存情報を使いつつ、MSVC IFC 固有の source-level ordering から GNU Clang lane を分離できる。今回の root target 検証は ArtifactCoreEnvironment に限り、他の split target とアプリ全体の link は未確認。
- **次に確認:** 同様の分割 target を追加検証し、sccache 有効時の実 compile cache hit を計測する。Clang lane の source property 除外が他 target に波及しないか確認する。

## 2026-10-21: GNU frontend の Windows module smoke は P1689 と COFF link の設定境界を分ける

- **関連:** `cmake/toolchain-clangxx-msvc-abi.cmake`、`cmake/clangxx-msvc-abi-rules.cmake`、`tools/clang-msvc-abi-smoke/`
- **確認できた事実:** CMake 4.2.2 / Ninja 1.12.1 / clang++ 22.1.3 の Windows x64 smoke では `clang-scan-deps` P1689 と dyndep が `.cpp` module implementation unit を interface provider へ依存させ、Clang GNU frontend + `x86_64-pc-windows-msvc` で link / 実行まで成功した。stock Windows-Clang link command の MinGW 用 `-nostartfiles` / `-nostdlib` はこの ABI とは衝突した。
- **価値／懸念:** scanner / module map を Clang 標準経路に保ちながら、MSVC ABI の runtime library と linker defaults を個別調整できる。smoke の成功だけではプロジェクト全体の MSVC `/reference`・`std.ifc`・vcpkg / Qt / Diligent 整合を示さない。
- **次に確認:** 現在の rules override が CMake の対応版間で安定するか確認し、ArtifactCore / Widgets の小さな実際の target に段階適用する。sccache と CMake 3.30 / clang-cl は別レーンで実測する。

## 2026-10-21: `.cpp` module implementation unit は MSVC / Ninja でコンパイル可能

- **関連:** `ArtifactWidgets/src/Dock/Pane.cpp`、`ArtifactWidgets/include/Dock/Pane.ixx`、`ArtifactWidgets/CMakeLists.txt`
- **確認できた事実:** 既存 MSVC / Ninja ビルドで interface の module dependency scan 後に `Pane.cpp.obj` が成功した。`ArtifactWidgets.dll` は CMake が実行する manifest resource compile の `no such file or directory` でリンク停止したが、同一 `rc.exe` を手動実行すると成功した。
- **価値／懸念:** `.cpp` 拡張子は既存 MSVC module implementation として成立するが、compile 成功だけでは DLL link / runtime 完了を意味しない。
- **次に確認:** CMake `vs_link_dll` manifest step の失敗を既存 build environment で切り分けたうえで、clang++ / clang-cl 各 lane の `.cpp` pilot と sccache cache stats を独立に実測する。

## 2026-10-19: レイヤー流体の動作確認はタイムラインフレームを進めてキャプチャする

- **関連:** `Artifact/src/Layer/ArtifactAbstractLayerFractureRuntime.cppm`、`Artifact/src/Layer/ArtifactLayerTimelineSupport.cppm`、`tests/Artifact/PhysicsLodBridgeContractTest.cpp`
- **確認できた事実:** Liquid レイヤー描画は `currentTimelineFrame()` を見てフレーム間の solver update を行う。composition 未接続なら layer current frame が使われる。単一フレームの描画キャプチャは色と形状しか検証せず、時間変化を示さない。D3D12 headless で frame 0 / 12 の readback を比較するテストは成功し、泡の縮小・減光と厚み由来の方向光／局所ハイライトを加えた。
- **価値／懸念:** アニメーションを含むレイヤーシミュレーションのGPUテストは、同一条件で異なるフレーム画像を比較するとシミュレーション進行と描画の両方を確認できる。GPU/ドライバ差を考慮し、完全一致ではなく意味のある画素差を判定する必要がある。層ごとの定数色描画であり、屈折や環境反射を含む本格的な流体シェーダーではない。
- **次に確認:** 明暗幅を含む更新後テストを通し、実アプリでフレーム移動・再生・シーク時の表示を MCP capture または UI テストで確認する。

## 2026-10-19: 独立Motion Path統合テストはVS2026/Ninjaで成立した

- **関連:** `CMakeLists.txt` (`ARTIFACT_BUILD_COMPOSITION_MOTION_PATH_TEST`)、`tests/Artifact/CompositionMotionPathMain.cpp`、`out/build/x64-Debug-Agent`
- **確認できた事実:** Visual Studio 2026 / Visual Studio 18 / MSVC 19.51.36246でopt-in独立GUI targetをビルドし、MCPのstate、Transform回帰、overlay画像取得を通過させた。root CMake側の親targetにArtifactCore IFC参照補完が必要だった。ArtifactCore child repoは変更していない。MSVC 19.51の `_Fill_vbool` link名不一致にはテストexe内shim/alternatenameで対応している。
- **価値／懸念:** VS2026がこの独立GUIテストの実用コンパイラであることを実ビルド・実行で確認できた。CMake/NinjaやMSVC更新時は親側IFC補完とshimが不要になるか再検証が要る。
- **次に確認:** 同じtoolchainを更新したとき、IFC参照補完と `_Fill_vbool` workaroundの必要性を個別に再確認する。

## 2026-10-19: 状態差分の画像artifactをVP overlay認識へ適用できた

- **関連:** `tests/ArtifactTestImageArtifacts.h`、`tests/ArtifactTestWindowCapture.h`、`tests/Artifact/CompositionMotionPathMain.cpp`
- **確認できた事実:** Motion Path OFF/ONのViewport画像と差分maskを独立artifactとして生成し、実画像でBezier軌跡・接線の領域が差分に現れることを目視した。Windows Graphics Captureはテストウィンドウを撮り、CompositionViewport領域をcropする。capture cursorを無効化し、OS desktop全体の画像や他ウィンドウの重なりを避ける。実行は3 MCPステップ全てpassした。
- **価値／懸念:** 全画面画像に頼らず、対状態の差分から特定表示のpixel maskを得られる。maskは画素差であってキーフレーム等の意味データではなく、他widget/backendへの再利用は未検証。
- **次に確認:** paired-state切替が安全な別GUIテストでhelperを適用し、capture領域、DPI、backendごとの安定性を確認する。

## 2026-10-19: 過去Motion Pathキーを編集しても現在フレームを維持する

- **関連:** `Artifact/src/Widgets/Render/ArtifactCompositionRenderController.cppm` のMotion Path hit処理、`tests/Artifact/CompositionMotionPathMain.cpp`
- **確認できた事実:** 当初、Motion Path sample/key hit処理が `ArtifactPlaybackService::setCurrentFrame(hitSample.framePosition)` を呼んでいた。ユーザーから過去点を触ると現在オブジェクトが移動すると報告され、ユーザー指定の「編集時は現在フレームを維持」に合わせてseekを削除した。独立GUI回帰はframe 45でframe 0 keyをdragし、frame 45の再生位置・Transform評価を保ち、キー変更とUndo復元を確認する。VS2026ビルドと3 MCPステップがpassした。
- **価値／懸念:** 過去keyの編集対象frameとPlaybackの現在frameが分離され、現在のプレビュー位置を保ってキーを修正できる。
- **次に確認:** 通常アプリ上でも選択・ドラッグ時の編集感を確認する。回帰は独立GUI fixtureのWindows / VS2026実行であり、他backend・OSは未検証。

## 2026-10-17: ProgressiveRenderer downsample settings have no callback consumer

- **関連:** `Artifact/src/Render/ArtifactFrameCache.cppm::ProgressiveRenderer::requestUpgrade`、`ProgressiveRenderer::setDraftQuality`、`ProgressiveRenderer::setPreviewQuality`、`tests/Artifact/FrameCacheContractTest.cpp`。
- **確認できた事実:** `requestUpgrade()` intentionally skips progressive passes when `Final` is requested; `ProgressiveRendererSkipsProgressivePassesWhenFinalQualityIsRequested` asserts this behavior. `draftDownsample_` and `previewDownsample_` appear only in their declarations and setters, and are not passed to the render callback.
- **訂正:** The previous note incorrectly said the existing quality-sequence test expected Preview and Final callbacks for a Final request. That test requests `Preview`; `ProgressiveRendererIncludesCustomAsFinalRequestedStage` requests `Custom` and expects the Preview/Final/Custom sequence.
- **価値／懸念:** The progressive stage sequencing has explicit coverage. The downsample setters still expose settings with no observable effect through this callback API; this is a potential missing feature, not evidence that the current stage contract is inconsistent.
- **次に確認:** If progressive downsampling becomes an implementation target, define how a stage communicates its working resolution and add assertions at that renderer boundary. Runtime status remains unknown because tests were not run under repository instructions.

## 2026-10-17: SoftBody self-collision stays disabled after leaving Minimal LOD

- **関連:** `ArtifactCore/src/Physics/PhysicsSystem.cppm::PhysicsSystem::updateSoftBodies`、`SoftBodySolver::setSelfCollisionEnabled`、`tests/ArtifactCore/PhysicsLodSettingsContractTest.cpp`。
- **確認できた事実:** `updateSoftBodies()` disables self-collision when `disableSoftBodySelfCollision` is true, but does not re-enable it when a later LOD no longer requests the override. `SoftBodySolver` exposes the current flag, so this transition is directly observable.
- **変更:** Added a test that starts with authored self-collision enabled, enters Minimal, then returns to Full and expects the setting to be restored. The current update path appears not to satisfy the final assertion; tests were not run under repository instructions.
- **価値／懸念:** A temporary performance LOD can permanently change the user's SoftBody behavior after the quality level rises again.
- **次に確認:** Decide how PhysicsSystem should preserve and restore each solver's authored self-collision state across LOD overrides, then run the focused Physics LOD contract test.

## 2026-10-17: MeshRenderer retains stale CPU LOD levels after an empty LOD upload

- **関連:** `ArtifactCore/src/Graphics/MeshRenderer.cppm::MeshRenderer::updateMeshletGeometry`、`tests/ArtifactCore/RayTracingAcceptanceTest.cpp::MeshRendererSelectsMeshletLodAtProjectedSizeBoundaries`。
- **確認できた事実:** `updateMeshletGeometry()` releases the previous GPU LOD buffer on every upload, but updates `pImpl_->meshletLods_` only when `lodCount > 0`. A subsequent upload with `lodCount == 0` therefore leaves the prior CPU selection table active while its GPU buffer has been released.
- **変更:** Added a contract that expects an upload with no LOD levels to fall back to level zero after a previous non-empty upload. The current implementation appears inconsistent with that replacement contract; no test was run under the repository's no-test-without-request rule.
- **価値／懸念:** Renderer selection can reference stale levels after geometry replacement, diverging CPU LOD choice from the available GPU resources.
- **次に確認:** When the implementation is in scope, clear `meshletLods_` on every upload before conditionally assigning the new table, then run the targeted GPU acceptance case.

## 2026-10-17: GI working resolution accepts non-finite LOD scales

- **関連:** `ArtifactCore/include/Graphics/GIResources.ixx::GIFrameResources::scaledDimension`、`tests/ArtifactCore/RenderPipelineContractTest.cpp`。
- **確認できた事実:** `scaledDimension()` rejects zero and negative scales but does not check finiteness before converting `value * scale` to `std::uint32_t`. NaN and positive infinity therefore reach a floating-point-to-integer conversion outside its representable range.
- **変更:** Added a contract expecting NaN and ±Inf resolution scales to resolve to zero working dimensions. The tests are expected to expose a mismatch for NaN and positive infinity; they have not been executed under the repository's no-test-without-request rule.
- **価値／懸念:** Malformed GI LOD settings can otherwise produce invalid or implementation-dependent resource dimensions before texture allocation.
- **次に確認:** When ArtifactCore production changes are explicitly in scope, reject non-finite scales before conversion and run the targeted render pipeline contract suite.

## 2026-10-17: RenderContext zoom accepts non-finite values

- **関連:** `Artifact/include/Render/ArtifactRenderContext.ixx::setZoom`、`createRenderContextSnapshot`、`tests/Artifact/RenderContextLodContractTest.cpp`。
- **確認できた事実:** `setZoom()` は `std::max(0.01f, newZoom)` のみで値を保持し、snapshot はその値をそのままコピーする。NaN は 0.01 に、正の無限大は無限大のまま保持されるため、有限な zoom 値を保証しない。
- **変更:** NaN と ±Inf を Full 相当の 1.0 倍へ戻す LOD 契約テストを追加した。現状コードとの不一致が想定されるが、テスト実行は禁止されているため未検証。
- **価値／懸念:** 不正な zoom が view ROI 計算や snapshot 復元へ伝播して描画座標を壊す可能性がある。
- **次に確認:** 実装変更が許可された段階で、非有限値を 1.0 倍へ正規化し、zoom と resolutionScale の共通 clamp 方針を検討する。

## 2026-10-17: Preview quality scaling can produce zero-sized non-empty previews

- **関連:** `ArtifactCore/src/Preview/PreviewQuality.cppm::calculatePreviewSize`、`tests/ArtifactCore/PreviewSettingsLodContractTest.cpp`。
- **確認できた事実:** `calculatePreviewSize()` multiplies source dimensions by the selected scale and truncates to `int` without a positive-dimension floor. For example, a 1-pixel dimension becomes 0 at Half, Quarter, Eighth, and Auto; a 7-pixel dimension becomes 0 at Eighth.
- **変更:** Added tests that every positive source dimension remains at least one pixel while zero input axes remain zero across all quality scales.
- **価値／懸念:** This makes the smallest preview-size behavior explicit and should catch zero-sized resource requests. The positive-dimension contract is inferred from the need to preserve valid output dimensions; no test execution was performed because repository instructions prohibit it without explicit user direction.
- **次に確認:** Decide whether non-empty preview dimensions must be clamped to one pixel, then run the PreviewSettings LOD contract suite when test execution is authorized.

## 2026-10-17: Preview TextureQuality has no consumer in the current tree

- **関連:** `ArtifactCore/include/Preview/PreviewQuality.ixx`、`ArtifactCore/src/Preview/PreviewQuality.cppm`、texture resource creation and sampling paths.
- **確認できた事実:** Repository-wide search finds `TextureQuality` only in the enum, stored field, getters/setters, and High/Medium/Low preset assignments. No texture creation, residency, filtering, or mip selection path reads this property. Existing Preview settings tests can only prove its value is stored.
- **価値／懸念:** Presets advertise a texture-quality LOD dimension that currently has no verified downstream effect. An end-to-end test needs a consumer boundary first; a storage-only test would not prove texture LOD behavior.
- **次に確認:** When implementation scope includes preview texture policy, define the mapping from TextureQuality to mip/residency/filtering and add an image-level or resource-selection contract at that seam.

## 2026-10-17: Boids LOD transitions retain the previous skip counter

- **関連:** `ArtifactCore/include/Crowd/BoidsSwarmSystem.ixx::update`、`tests/ArtifactCore/BoidsDistanceLodContractTest.cpp`。
- **確認できた事実:** `update()` changes `agentLod_[i]` from camera distance and decrements `lodSkipCounter_[i]`, but does not reset the counter when the LOD changes. Existing coverage verifies initial entry into Far and periodic force refresh; a new contract also exercises Far→Near→Far after reversing the target direction and expects a fresh force on re-entry.
- **価値／懸念:** A Far agent can hold a multi-frame skip counter while Near computes a new force; returning to Far before that counter expires may skip the first force update and retain stale acceleration. This is inferred from the inspected control flow; test execution is prohibited by the repository instructions, so runtime confirmation is pending.
- **次に確認:** When implementation work is authorized, reset or refresh the skip state at distance-LOD transitions and run the targeted Boids contract suite.

## 2026-10-17: Physics fluid resolution scale is configured but not applied

- **関連:** `ArtifactCore/src/Physics/PhysicsSystem.cppm::PhysicsLODSettings`、`PhysicsSystem::update`、`FluidSolver2D::setResolution`、`tests/ArtifactCore/PhysicsLodSettingsContractTest.cpp`。
- **確認できた事実:** `fluidResolutionScale` is assigned Reduced/Minimal defaults and clamped, but a repository-wide search finds no read of the field outside its declaration, preset assignment, and normalization. Registered fluid solvers are never passed to `setResolution()` by `PhysicsSystem`.
- **変更:** Added a contract expecting a 32×24 fluid grid to move to 16×12 at Reduced, 8×6 at Minimal, and return to 32×24 at Full.
- **価値／懸念:** The test exposes a mismatch between the configured Physics LOD budget and the solver resolution. It is expected to fail against the inspected implementation; no runtime result is available because repository instructions prohibit test/build execution without explicit user direction.
- **次に確認:** When production implementation is authorized, define how live fluid fields are resampled on both downscale and restoration, then run the Physics LOD contract tests.

## 2026-10-17: Particle render LOD target ratio differs from actual fixed-stride count

- **関連:** `Artifact/src/Layer/ArtifactParticleLayer.cppm::applyParticleRenderLOD`、`tests/Artifact/Particle3DRenderContractTest.cpp`。
- **確認できた事実:** For 1000 source particles at screen scale 0.60, the implementation computes an 800-particle target, then uses `ceil(1000 / 800) = 2` as a fixed sampling stride and queues 500 particles. The previous contract asserted an 80% queued ratio, which does not match that implementation. The test now expects a 50% ratio and checks repeated same-frame medium-scale draws return 500.
- **追加確認:** At screen scale 0.10, 257 source particles produce a 64-particle target but a stride of 5, so the selected count is `ceil(257 / 5) = 52`. This falls below the apparent 64-particle minimum. Added `ParticleLodMinimumBudgetSurvivesOddPopulation` to cover this boundary; it is expected to expose the mismatch, but has not been run.
- **価値／懸念:** Fixed-stride rounding makes the target ratio differ from retained density and can reduce output below the intended minimum. The new odd-population assertion is expected to fail against the inspected calculation; runtime confirmation was not performed because repository instructions prohibit test/build execution without explicit user direction.
- **次に確認:** Decide whether fixed-stride sampling is the intended density policy. If an approximately proportional target is required, consider a bounded deterministic index-distribution scheme and add particle attribute/model-matrix preservation checks through a suitable test seam.

## 2026-10-17: Deferred command recording needs a strict first worker lane

- **関連:** `Artifact/include/Render/DiligentImmediateSubmitter.ixx`、`Artifact/src/Render/DiligentImmediateSubmitter.cppm`、`tests/Artifact/ClonerGpuRenderContractTest.cpp`。
- **確認できた事実:** Existing `submit()` recorded a Diligent deferred command list and executed it synchronously before returning, so the GUI caller still paid both recording and execution costs. The submitter mutates PSO and batching state and cannot be shared concurrently.
- **変更:** Added a narrow record/execute/finish split for buffers containing at least 16 submitter-owned `SolidRectPkt`, `SolidRectXformPkt`, `LinePkt`, `QuadPkt`, and `SolidTriPkt` packets. Recording returns the command list without touching the Immediate Context or resetting the packet buffer; a worker-thread contract test records 12 invisible rectangles, a line, a quad, a red triangle, and an overlapping green rectangle, then returns `FinishFrame()` to the recording thread.
- **価値／懸念:** The deferred record/execute/finish seam and the production `submit()` threshold-selected deferred path both pass the mixed-packet image contract on Debug / Vulkan and Debug / D3D12. The test compares 128×128 readbacks with identical alpha coverage, maximum channel difference at or below `1.0e-4`, and expected color samples; it saves immediate, worker-recorded, and production-submit frames as backend-specific PNGs beside the test binary. The generated Vulkan and D3D12 frames were visually inspected and match. CTest initially failed during global environment setup because the Cloner test lacked the Qt offscreen plugin staging used by the neighboring Particle test; its target now copies the plugin beside the binary and sets `QT_QPA_PLATFORM_PLUGIN_PATH`. This validates only these submitter-owned 2D packet types. The production submit path still records and executes synchronously on its caller. Composition snapshots, broader resource ownership, queueing, present, and GUI non-blocking remain unverified.
- **次に確認:** Extend the worker lane only after defining immutable frame inputs and worker-owned mutable renderer resources; retain this parity test as the narrow baseline. The current API does not establish end-to-end asynchronous Composition rendering.

- **追加確認 (2026-10-17):** `Model3DLightCompositionIntegrationTest` は frame serial の進行と GPU readback source の準備を同時とみなしており、point-light case で空画像を受け取ることがあった。テスト側で GPU readback を最大 5 秒待つようにし、Debug / D3D12 と Debug / Vulkan で Composition の実フレーム PNG が出力されることを確認した。両 PNG は SHA-256 が一致した。この確認は画像出力の安定化であり、worker frame ownership の証明ではない。

## 2026-10-17: Particle deferred image tests also validate readback channel order

- **関連:** `Artifact/src/Render/ArtifactIRenderer.cppm::readbackToImageF32()`、`tests/Artifact/Particle3DRenderContractTest.cpp`、`tests/Artifact/CMakeLists.txt`。
- **確認できた事実:** `readbackTextureViewToFloatBuffer()` copies RGBA8/16F/32F staging rows into a raw RGBA float array. Treating that array as BGRA made the existing Particle GPU image assertions see zero red pixels. The Windows test executable also lacked a colocated Qt offscreen platform plugin, so QGuiApplication initialization could not complete from the test runner.
- **今回の対応:** Normalize the raw RGBA readback through `setFromRGBA32F()`, copy Qt's offscreen integration plugin beside the Particle test binary, and add a 16-packet deferred-vs-immediate Particle image contract.
- **価値／懸念:** The Deferred Particle test now checks actual GPU output on D3D12 and Vulkan; the full Particle contract suite passes. Binding the DSV only when Particle depth testing or writing is enabled removed the depth-format mismatch warning. All seven Particle contract tests pass on both D3D12 and Vulkan.
- **次に確認:** Extend the same offscreen plugin deployment to other Windows headless targets only when their CTest setup demonstrates the same missing-plugin failure; continue validating depth-enabled Particle PSOs against the active DSV format on both backends.

## 2026-10-17: Deferred viewport state needs an image-level boundary test

- **関連:** `Artifact/src/Render/ArtifactIRenderer.cppm::applyRenderViewportState()`、`Artifact/src/Render/DiligentImmediateSubmitter.cppm::applyViewportState()`、`tests/Artifact/ClonerGpuRenderContractTest.cpp`。
- **確認できた事実:** Deferred context target bind resets viewport state on D3D12, so the submitter stores `RenderViewportState` and reapplies viewport/scissor after binding the target. A new headless test renders a partial viewport through immediate and 16-packet deferred paths, compares full-frame alpha coverage, and checks pixels outside the scissor.
- **今回の対応:** The partial viewport/scissor case and a two-viewport queue-drain case pass on Debug D3D12 and Vulkan in separate processes.
- **価値／懸念:** This proves viewport preservation and queue draining for solid-rect packet batches; it does not prove gizmo runs or billboard/3D render-target handoff.
- **次に確認:** Add image contracts for ordered gizmo overlays and Billboard/3D target handoff before claiming broad deferred viewport parity.

## 2026-10-17: Resident GPU shaders need paired BGRA load and store swizzles

- **関連:** `Artifact/src/Effects/InnerShadow/InnerShadowEffect.cppm` resident shader、`docs/analysis/COLOR_CHANNEL_ORDER_CONSISTENCY_AUDIT_2026-10-16.md`。
- **確認できた事実:** Inner Shadow resident shaderはアルファの参照だけならraw texture読み出しで問題ない一方、合成対象のRGBをraw `float4`から読み、logical RGBとして計算後にraw出力していた。CPU実装はBGRA backing各成分を順序不問で合成するため、このGPU経路は赤と青の異なる入力・影色で結果が入れ替わる。
- **変更:** shaderのforeground loadに`.bgra`、output storeにも`.bgra`を追加し、resident BGRA textureとの入出力規約を対称にした。
- **価値／懸念:** Inner ShadowのGPU出力とCPU fallbackを同じlogical colorに揃える。shader compileとruntime画素比較は実施していない。
- **次に確認:** 赤青が非対称な素材と影色を用い、resident GPU / CPU fallback / 後続effect chainの画素値を比較する。

## 2026-10-16（更新 2026-10-17）: Render thread separation is gated by a widely shared immediate context

- **関連:** `Artifact/src/Widgets/Render/ArtifactCompositionRenderController.cppm`（`:19486`,`:19723`,`:42329-49391`）、`ArtifactCore/src/Thread/PreciseTicker.cppm`、`Artifact/include/Render/DiligentDeviceManager.ixx`（`:48-49`,`:154`）、`ArtifactCore/src/Render/RenderWorker.cppm`、`Artifact/src/Render/DiligentImmediateSubmitter.cppm`、`Artifact/src/Render/DiligentBindlessSubmitter.cppm`、設計は `docs/planned/MILESTONE_RENDER_THREAD_SEPARATION_FEASIBILITY_2026-10-16.md`。
- **確認できた事実:** tick は `PreciseTicker` の worker thread から `QMetaObject::invokeMethod(Qt::QueuedConnection)` で GUI へ marshal され、フレーム本体 `renderOneFrameImpl`（約7,000行）は GUI スレッドで全実行される（`present()` もその中）。`DiligentDeviceManager` は single-thread immediate 契約と deferred context を持つ。さらに `DiligentImmediateSubmitter` は 16 packet 以上の一部 command buffer を deferred context で記録し、同じ呼び出し内で immediate に実行する既存経路を持つため、「deferred は未実装」という評価は誤り。shared context を取得する Effect と Engine Render Window がある一方、Timeline Render Window は独立 device/context を使う。`ArtifactCore::RenderWorker` は空スタブ。
- **変更:** `BillboardPkt` は `PrimitiveRenderer3D::drawBillboardQuadWithContext()` に submitter の `recordCtx` と packet RTV を渡し、deferred command list 内で周囲の packet と同じ順序に記録する。submitter から PrimitiveRenderer3D の RTV override を一時変更して `nullptr` に戻す操作も除き、active offscreen target を後続の即時 3D draw が失わないようにした。QImage overload は packet 作成時に texture を解決して retained SRV 付き `BillboardPkt` を作り、QImage 変換・resource 作成を deferred recorder へ持ち込まない。さらに `LinePkt` / `QuadPkt` / `SolidTriPkt` / `AtlasSpritePkt` は種類の切替時に前の保留バッチを flush し、固定末尾 flush による interleaving の順序逆転を防ぐ。Bindless submitter は fallback 時の partial draw を避けるため単一 batch 条件を先に検査し、最終 flush の失敗を検出する。ParticleRenderer の `updateBuffer()` は recording context を引数で受け、Particle upload / prepare / draw を同じ context に記録する。Diligent D3D12 / Vulkan backend は `UpdateBuffer()` の CPU 入力を呼び出し中に upload allocation へコピーしてから同じ context の command stream に buffer copy を記録する。Cloner placement は `ClonerGridDispatchPkt` として params / retained GPU resources を packet 化し、producer が output range を予約した順に dispatch packet を Cloner sprite packet の前へ置く。submitter は `recordCtx` で params を map して compute dispatch を記録し、Map が失敗すると対応する sprite packet は既存 CPU fallback を使う。Cloner も既存 deferred context で記録し、同じ submit 呼び出し内に immediate context で実行する。`PrimitiveRenderer3D` の gizmo line / triangle は順序付き vertex stream と topology run にまとめ、順番を保ったまま一度 upload して描画する。submitter は 2D packet の後に gizmo runs も同じ `recordCtx` へ記録し、packet 数と gizmo 頂点数が合計 16 以上なら deferred recording の対象にする。bindless Sprite 専用経路は pending gizmo がある時に通常 submitter へ委譲し、Sprite と gizmo の順序を保つ。初期段階では `BillboardImagePkt` が buffer-wide immediate fallback を残していたが、後続の context 対応修正で解消した。
- **確認できた事実:** `RenderCommandBuffer::tryAppend()` は容量を超えて成長せず、Cloner dispatch packet と draw packet が producer 順で追加される。dispatch packet は PSO / SRB / params buffer / output buffer を保持し、range は対応する dispatch packet index を持つ。submitter は compute と sprite draw の両方に同じ `recordCtx` を使い、resource transition を要求する。Diligent の D3D12 / Vulkan backend は dynamic buffer の `MAP_FLAG_DISCARD` で context ごとの dynamic allocation を取得し、Vulkan compute dispatch は active render scope を終えてから実行される。これらはソース確認であり、実機実行結果ではない。
- **追加確認と変更:** deferred context は immediate context の viewport / scissor state を継承せず、D3D12 の target bind は viewport を target 全体へ設定する。`RenderViewportState` を保持し、submitter の初期 target bind、Particle の target bind、PrimitiveRenderer3D の Billboard / gizmo / card / shape bind 後に viewport と scissor を適用する。renderer の viewport が変わる場合は、queued packet と gizmo を旧 state のまま先に submit してから新 state を反映する。
- **実行経路の確認:** `ArtifactIRenderer::Impl` は `DiligentImmediateSubmitter` を保持して使う。Artifact の描画経路で `DiligentBindlessSubmitter` の生成・呼び出し箇所は見つからず、その safety 修正は現行 renderer に適用されない。
- **追加未検証:** 部分 viewport / scissor 上の D3D12 / Vulkan parity は solid-rect ケースで確認済み。target サイズ変更をまたぐ state 復元、gizmo / Billboard parity は未確認。
- **追加修正:** legacy `BillboardImagePkt` handler は PrimitiveRenderer3D の保持 immediate context を使っていたため、deferred batch 全体が immediate に切り替わっていた。`drawBillboardImageWithContext()` で渡された recording context に記録し、submit 時に作った texture を packet 内で保持して command-list 実行後まで寿命を維持するようにした。これで既存 packet variant の中に buffer-wide immediate fallback を要求するものは残らない。texture の生成は引き続き呼び出し thread 上で行い、worker へは移していない。
- **fallback の理由:** 旧 Cloner producer dispatch は shared constant buffer / ComputeExecutor を更新し、renderer-wide output cursor の range を予約して immediate context へ dispatch していた。今回 range reservation は producer で直列のまま残し、GPU upload / dispatch を順序付き packet と recordCtx に移した。legacy `BillboardImagePkt` の QImage texture 生成は submitter の呼び出し thread 上に残し、描画 command のみ `recordCtx` へ記録する。別 worker から producer / submitter を同時に呼ぶ設計にはまだ進めていない。
- **確認できた事実:** `RenderCommandBuffer::append()` が SRV を retain し、submit は `ExecuteCommandLists()` 後に buffer を reset する。順序については、種類切替時に異なる保留バッチを先に flush するコード経路を確認した。並列生成の `RenderCommandBufferPool::tryMergeInto()` は worker slot 順に結合し、polyline / polygon の caller は連続した入力範囲を worker index 順で分割する。完了順のばらつきは packet 順を変えない。
- **追加修正:** `mapWriteDiscard()` の戻り値を無視して draw していた経路では、Map 失敗時に直前 packet の dynamic-buffer 内容を現在の packet 位置で描く可能性があった。`DiligentImmediateSubmitter.cppm` 内の全 callsite で必要な Map 成功を draw 前に確認し、batch は保留 CPU data を破棄、個別 packet は該当 draw をスキップする。Diligent は backend 確保前に map 状態を登録するため、null pointer 時も `UnmapBuffer()` して状態を戻す。Cloner dispatch と PrimitiveRenderer3D の context-aware / immediate Map failure 経路も同じ cleanup を行う。複数 glyph packet は途中失敗時に既に記録した glyph が残り、後続 glyph を中断する。failure injection と runtime 確認は未実施。
- **追加確認と変更:** renderer の immediate clear、skybox draw、blend、texture copy / resolve、viewport exposure、compute 用 target unbind に加え、`clearTextureRegion()` / `convertLayerToFloat()` / `applyTrackMatte()` も queued packet を先に submit する境界がなかった。呼び出し側が先行 packet を残したままこれらを呼ぶと、同じ texture の更新と compute / clear / draw / copy の順序が逆になる可能性があるため、各即時操作の前に queue drain を追加した。swapchain resize も旧 backbuffer を参照する packet を drain してから解除する。これは API 境界の順序保証としての修正で、現行 UI 呼び出しで全ケースを runtime 再現したわけではない。
- **追加修正:** 非同期 readback の staging `CopyTexture()` 前にも queued packet の submit がなく、active render target を source にした場合に copy が同 frame の draw を追い越せた。同期 readback と同じく copy 直前に `submitQueuedDraws(ctx)` を入れ、GPU copy の前に source を materialize する。fence 後の worker wait / map は変更していない。runtime readback parity は未確認。
- **追加確認と変更:** async readback worker は fence 後に captured immediate context を map/unmap していた。`DiligentDeviceManager.ixx` の single-thread immediate contract に合わせ、readback 専用 `QThreadPool` を最大1 thread・無期限維持とし、その worker 上で deferred graphics context を作成・再利用する。Diligent API は graphics context での staging map を許可し、D3D12 / Vulkan backend の staging map/unmap は command recording を使わない。device identity が変わった際は同じ worker 上で context を再作成する。pool を Impl の最終 data member に置き、他の renderer state の破棄前に join する。これらはソース確認で、backend runtime safety と device recovery は未検証。
- **追加修正:** Composition controller は pending layer packet がある状態で `renderShadowMapFrame()` を呼び、その後に `flush()` していた。shadow pass は immediate context を shadow target に切り替えて clear / draw するため、packet submit より先に実行される。shadow target bind 前に packet と gizmo を submit し、controller の呼び出し順を保つ。source 上で packet-pending の位置関係を確認したが、画像 parity への影響は未検証。
- **追加懸念:** Qt `QThreadPool` の destructor は runnable 完了まで block し、Diligent `IFence::Wait()` に timeout はない。よって renderer teardown は readback 完了まで待つ場合があり、device loss / hung GPU 時の終了待ちは未確認。**次に確認:** 実機でreadback完了時間を記録し、in-flight readback中の renderer teardown と device loss を検証する。
- **追加監査:** Composition 最終化の raw accumulator conversion / channel display は、先行する accumulator 更新が同じ immediate context 上で完了し、queued viewport background packet とは別 texture を使う。cache 色変換も build 時は packet drain 後、cache-hit 時は前フレームから保持された source を読む。ここでは同一 resource に対する順序競合を確認できず、追加 drain は入れていない。**価値／懸念:** raw context 呼び出し全件を一律 flush せず、packet と即時処理の read/write resource 依存を根拠に境界を限定できる。**次に確認:** deferred recorder を worker 所有へ移す段階では、これらの raw GPU pass を明示 API に包み、resource access と context ownership を呼び出し境界で表す。runtime resource transition は未確認。
- **追加監査:** MPM surface の queue-full retry は先行 packet を submit してから行う。並列 geometry は worker buffer の失敗分を reset してから owner queue に直列 fallback し、Cloner GPU placement も失敗した placement 単位で通常 sprite へ戻る。確認した失敗経路では部分 worker packet の後挿入や即時 draw による順序逆転は見つからなかった。queue 上限での runtime 表示は未検証。
- **追加実行確認・修正（2026-10-17）:** ArtifactClonerGpuRenderContractTest の Deferred packet 順序、同一 batch 内の GPU placement + sprite、dispatch と sprite を分けた CPU fallback の各ケースが Debug / Vulkan および Debug / D3D12 で pass。各ケースを個別プロセスで実行し、Vulkan 物理 device / D3D12 adapter のログを確認した。同一プロセスの連続 headless Vulkan initialization は Diligent global function pointer 制約エラーを出すため、個別プロセスを検証条件とする。placement range に renderer batch generation を含め、queued packet の submit または renderer / placement GPU resource の再初期化後は古い range を GPU draw API が拒否する。これにより出力 buffer の cursor 再利用後に stale range を誤読せず、caller は既存 CPU transform fallback を使える。test split path は GPU dispatch を submit した後に各 placement draw が拒否されることと、fallback image の3 component位置を確認する。runtime dispatch callsite は共通 placement observer 1 箇所で、texture 解決後に dispatch し、そのまま callback へ進む。確認した callback 内にも queue drain はない。**次に確認:** renderer device loss 中の再初期化と generation counter wraparound。
- **追加修正（2026-10-17、mutable texture packet order）:** 関連: `PrimitiveRenderer2D.cppm`、`ArtifactIRenderer.cppm`、`ArtifactCompositionRenderController.cppm`、`ArtifactDiligentTimelineRenderWindow.cppm`。確認できた事実: `textureForVideoFrame()` は layer ごとの再利用 slot を即時 `UpdateTexture()` で上書きする一方、前 frame の packet が未 submit のまま同じ SRV を参照し得る。Sprite / Cloner Sprite / Atlas Sprite / textured triangle / local sprite / masked sprite の packet 追加時、SRV が直近に解決した video slot と一致すれば queue epoch を記録し、該当 slot の上書き時だけ owner queue を drain する。Composition 側は texture 解決を Cloner dispatch の前へ移し、drain が新 dispatch と sprite の index 関係を切らないようにした。さらに GlyphAtlas の容量到達時は CPU 側で atlas 全体を clear/repack した後、GPU texture を全量更新するため、旧 atlas SRV を参照する queue が残っていれば Artifact renderer / 独立 Timeline renderer の owner submitter で先に drain する。通常の shelf 追加では部分 upload のままとする。全 queue scan と追加 heap allocation は導入していない。価値／懸念: 再利用 GPU texture と glyph atlas の内容更新が、それらを読む保留 packet を追い越す経路を境界ごとに防ぐ。複数 sample playback と atlas 飽和の runtime parity は未検証。次に確認: D3D12 / Vulkan で複数 sample 再生、RAM preview、Cloner dispatch → sprite 順序、GlyphAtlas 飽和後の Timeline text を実行確認する。
- **未検証:** build/runtime での描画順・画像 parity は未確認。Bindless fallback の実機 failure injection も未実施。Particle の deferred UpdateBuffer / MapBuffer と複数 Particle packet の GPU 結果、Cloner compute → draw、gizmo Map/Unmap と D3D12 / Vulkan 表示結果も未確認。offscreen target 上の Billboard 後に続く即時 3D draw の target 維持も未確認。
- **価値／懸念:** deferred context は command recording を段階的に移せるが、現在は同期呼び出しで GUI を外れない。submitter は PSO cache と batch 配列、PrimitiveRenderer3D は SRB / constant buffer / 定数値 / RTV override を可変共有するため、別 worker へ context pointer だけ渡す設計では競合する。順序付き gizmo run metadata は初期化時に小さく reserve するが、256 run を超える場合の steady-state allocation は計測していない。GUI block の解消には GUI snapshot、worker-owned deferred recorder と mutable resource state、immediate execution / present owner の分離が別々に必要。deferred command list 実行まで resource の寿命を保ち、順序を維持しなければならない。
- **次に確認:** 明示的なビルド／実行後に Cloner の dispatch → sprite 順序、CPU fallback、画像 parity と、gizmo の面／輪郭線順序、target、D3D12 / Vulkan parity、offscreen Billboard 後の即時 3D draw target を確認する。続けて recorder ごとの GPU resource ownership を設計し、GUI prep / command record / immediate execute / present を個別計測する。併せて shared immediate を使う Effect の frame 外利用と queue/cache 経由の context も追跡する。`MultithreadedResourceCreation` は別機能として扱う。

## 2026-10-16: Effect parameter edits bypass undo and force full surface invalidation

- **関連:** `Artifact/src/Widgets/ArtifactPropertyWidget.cppm`（effect 行 `:3555-3726`、preview `:565-597`）、`Artifact/src/Widgets/ArtifactPropertyWidgetShared.cppm:1003-1020`、`Artifact/src/Widgets/Render/ArtifactCompositionRenderController.cppm:18408`/`:18497`、`Artifact/src/Service/ArtifactEffectService.cppm:1612`/`:1691`、`Artifact/src/Undo/UndoManager.cppm:996`。計画は `docs/planned/MILESTONE_EFFECT_PARAM_UI_PERFORMANCE_2026-10-16.md`。
- **確認できた事実:** Inspector のエフェクトパラメータ UI は `ArtifactInspectorWidget` 内の `ArtifactPropertyWidget`（`effectPropertyWidget`）が担い、編集は `effect->setPropertyValue` を同期直接呼び出しする（`beginValueEdit`/`cancelValueEdit` は渡されない）。そのためこの経路は `SetPropertyCommand` を積まず undo 履歴に入らない。`LayerDirtyFlag::Effect` は lightweight 合成から除外され、1ステップごとに full/draft surface cache 破棄＋full-frame effect で全画面再合成を誘発し、描画は GUI スレッドで走る。undo 付きの正規 API（`ArtifactEffectService::setEffectProperty`）は別に存在するが、パラメータ値編集では使われていない。
- **価値／懸念:** 同じ「エフェクト param 編集」で undo 有無と dirty 処理が二重系統に分かれている。rect 編集で full 破棄を避け、undo を service 経路へ寄せれば、重さと undo ギャップを同時に縮められる可能性がある。ただし直接経路が undo 無しである理由は未確認。
- **次に確認:** 直接経路が意図的（undo 対象外）か、別ハンドラが補完しているかを確定してから Phase 1.2 を実装する。Phase 0 計測（`ArtifactEffectParamPerf.enable` 案）で setPropertyValue / invalidation / full redraw / GUI ブロック時間を実測して採否を判断する。

## 2026-10-16: GPU image upload should preserve canonical BGRA directly

- **関連:** `ArtifactCore/include/Image/GpuImageUpload.ixx`、`ArtifactCore/src/Image/GpuImageUpload.cppm`、各Effect GPU upload境界。
- **確認できた事実:** `makeGpuImageUploadBuffer()`のBGRA targetはBGRA viewをzero-copy参照するが、RGBA viewとのorder mismatch時には画像全体サイズのscratchへswizzleする分岐があった。アプリのEffect call siteはBGRA targetを指定する。
- **変更:** GPU upload APIはBGRA descriptor/targetだけを受け付け、order mismatchの全画素swizzleとscratch allocationを削除した。値順をGPUへそのまま渡す。
- **価値／懸念:** 正規BGRA CPU imageからGPU textureへのuploadがzero-copyになり、色順変換とフレーム単位scratch allocationをなくす。もしRGBA descriptorのsurfaceが残ればupload失敗し、caller fallbackとなるため、全surface producerの正規化は引き続き必要。
- **次に確認:** surfaceView生成箇所と全upload call siteでBGRA descriptorが確立していることを静的に確認し、descriptor不整合を実行時に検出する。pixel parityとallocation profileは未実施。

## 2026-10-16: RationalTimeのint64境界rescaleが符号を反転する

- **関連:** `ArtifactCore/src/Time/RationalTime.cppm` の `RationalTime::rescaledTo()`、`tests/ArtifactCore/RationalTimeContractTest.cpp`、`tests/ArtifactCore/RationalTimeStandalone/CMakeLists.txt`。
- **確認できた事実:** MSVC standalone suiteで26件中24件が通り、`INT64_MAX / 2` と `INT64_MIN / 2` をscale 1へrescaleする2ケースが失敗した。runtime結果はそれぞれ数学的な符号と逆向きの値を返した。実装は正数側で`numerator + denominator / 2`、負数側で`-numerator`を計算しており、int64境界の加算・符号反転を安全化していない。
- **価値／懸念:** フレーム時間変換の極端値で時間方向が反転する可能性を独立CPUテストで再現できた。修正はArtifactCore child repoの変更となるため、今回のテスト作業では行っていない。
- **次に確認:** ArtifactCore変更を含む明示依頼の後、境界演算をchecked arithmeticまたは符号付きsafe roundingへ置き換え、該当2ケースとRationalTime全suiteを再実行する。

## 2026-10-16: MSVC standalone module testはIFCとQt runtime pathを明示する

- **関連:** `tests/ArtifactCore/StandaloneTestHelpers.cmake`、`FrameRateStandalone/CMakeLists.txt`、`RationalTimeStandalone/CMakeLists.txt`、`PropertyPathStandalone/CMakeLists.txt`。
- **確認できた事実:** scanner-disabledのmodule implementationをinterface targetと分けるstandalone buildでは、MSVCがprimary interface / import IFCを自動参照しないケースがあり、明示`/reference`指定とinterface targetのbuild dependencyが必要だった。Qtを動的リンクするCTestもQt CoreのDLL directoryを`PATH`へ追加しないと`0xc0000135`で起動しなかった。
- **価値／懸念:** 小さなCPU contract suiteをArtifactCore全targetから隔離でき、unrelated module failuresによる実行阻害を避けられる。IFC pathはstandalone targetごとのmodule依存に合わせて指定する必要がある。
- **次に確認:** 新しいstandalone targetを作る際、interface IFC build order、implementation imports、CTestのQt runtime pathを構成時に確認する。

## 2026-10-16: CompositionNodeStore契約はArtifactAppRuntimeなしで検証できる

- **関連:** `Artifact/include/Composition/ArtifactCompositionNodes.ixx`、`Artifact/src/Composition/ArtifactCompositionNodes.cppm`、`tests/Artifact/CompositionNodeStoreContractTest.cpp`、`tests/Artifact/CompositionNodesStandalone/CMakeLists.txt`。
- **確認できた事実:** CompositionNodeStore単体suiteは上記Artifact module interfaceと実装だけをコンパイル・リンクし、MSVC standalone processで7ケースすべて通った。既存のArtifact test targetは同じCPU契約suiteにArtifactAppRuntimeをリンクしている。
- **価値／懸念:** このモデル契約をアプリruntimeや他のArtifactCore module群から分離して検証でき、広い依存targetのビルド失敗と切り分けやすい。
- **次に確認:** test targetを統合する場合も、必要moduleだけのリンク境界を維持し、AppRuntimeの追加を避けられるか検討する。

## 2026-10-16: SchemaMigrationはheader-only module単体で契約検査できる

- **関連:** `ArtifactCore/include/Serialization/SchemaMigration.ixx`、`tests/ArtifactCore/SchemaMigrationContractTest.cpp`、`tests/ArtifactCore/SchemaMigrationStandalone/CMakeLists.txt`。
- **確認できた事実:** `SchemaMigration`は実装moduleを持たず、Qt Core以外のmodule importもない。独立GTest targetから4ケースを実行し、全件passした。
- **価値／懸念:** ArtifactCore全体をビルドせずにmigration graphの純粋な契約を確認できる。migration callbackがthrowした時に入力JSONを段階反映する実装は確認できるが、atomicityは公開契約が未確定で今回のsuiteではassertしない。
- **次に確認:** migration callback例外時に入力値を保持すべきか仕様決定後、必要なら失敗時状態の契約テストを追加する。

## 2026-10-16: Effect sequence contractは製品runtimeなしでもsuite依存が広い

- **関連:** `tests/Artifact/EffectSequenceContractTest.cpp`、`tests/Artifact/CMakeLists.txt`、`Artifact/CMakeLists.txt`の`ArtifactEffectsColor` target。
- **確認できた事実:** CPU契約テストはeffect stage順・同stage安定順・画素出力・mix setterを検査し、Project RoundTripはeffectの削除再挿入後の保存再読込を扱う。テスト実行ファイルは現在の生成buildに存在せず、`ArtifactEffectsColor`はArtifactCore/Audio/ColorCollection/Render/EffectContractへ推移依存する。
- **価値／懸念:** アプリ起動を避ける既存契約はあるが、1つのsuite実行が広いmodule buildへ波及しうる。独立性を上げるにはeffect abstract/baseと2つのeffect実装、および画像buffer関連だけのmodule境界を確認する必要がある。
- **次に確認:** 次の独立化候補では、必要moduleのimport closureと既存IFCを先に列挙し、target依存が狭まることを確認してから実行する。

## 2026-10-16: Rasterizer mask integrationは専用processだがAppRuntime依存

- **関連:** `tests/Artifact/RasterizerMaskEffectIntegrationTest.cpp`、`tests/Artifact/CMakeLists.txt`。
- **確認できた事実:** 固定32×16 surfaceでmask内部・境界・外部のalpha勾配を検査するtestがある。専用CTest executableでRUN_SERIALだが、`ArtifactAppRuntime`へリンクされ、実行ファイルは現在の生成buildにない。
- **価値／懸念:** 小画像のCPU integration coverageは既に存在する一方、test単体のbuild依存は大きい。実行だけのために親targetをbuildすると多くのmodule scanへ波及し得る。
- **次に確認:** 独立化するなら、layer factoryとcomposition drawing helperの必要module closureを特定し、テストfixtureだけを構築する最小経路を評価する。

## 2026-10-16: Project/StillImage contractはfixture隔離済みだがbuild境界が広い

- **関連:** `tests/Artifact/ArtifactProjectRoundTripTest.cpp`、`tests/Artifact/StillImageLayerRoundTripTest.cpp`、`tests/Artifact/CMakeLists.txt`。
- **確認できた事実:** Matte source deletionはtemporary project rootとcleanupを使いtarget/matte参照を確認する。Still image replacementはtemporary PNGを使いsource path/寸法/画素とopacity/positionを確認する。両suiteとも専用CTest process・RUN_SERIALだが`ArtifactAppRuntime`にリンクする。
- **価値／懸念:** filesystem/process状態の隔離はあるが、target build時のmodule dependencyは狭くない。既存テストを重ねて実行する前にruntime依存を把握する必要がある。
- **次に確認:** 追加ケースを検討する前に同suiteの該当coverageを再利用し、分離が必要なら対象layer/serviceのmodule closureを個別に計測する。

## 2026-10-16: Image sequence stale-frame回帰は3条件を既存suiteで網羅

- **関連:** `tests/Artifact/StillImageLayerRoundTripTest.cpp`。
- **確認できた事実:** 欠番時のplaceholderとファイル復旧後の再接続、同一pathの破損内容でstale pixelを表示しないこと、有効な内容差替え後の再読込を別々の3ケースで検査する。各ケースが`QTemporaryDir`を使う。
- **価値／懸念:** 既存coverageが具体的で、新規テストの重複は不要。これらは1つの大きなStillImage suiteにあり、AppRuntime依存を持つ。
- **次に確認:** suiteを実行する際は対象caseだけをfilterできるか確認し、必要ならStillImageLayerの実装module closureで小さなstandalone targetを評価する。

## 2026-10-16: Undo budget rejection test must avoid the clamped entry-count field

- **関連:** `tests/Artifact/UndoManagerContractTest.cpp`、`Artifact/src/Undo/UndoManager.cppm`の`setBudget()`。
- **確認できた事実:** `setBudget()`は`maxEntryCount`を最低1へ正規化する一方、単一entry bytesと総bytesの0は拒否条件として維持する。従来のtestは`maxEntryCount=0`を設定していたため、期待するpush拒否を誘発しない。
- **価値／懸念:** rejection testを`maxSingleEntryBytes=0`へ変更し、entry count上限1の最古Undo entry evictionと残存entryのUndo/Redoを別caseで検査する。
- **次に確認:** AppRuntime targetが広いため、実行時は既存IFCを再利用する限定buildで可能か評価する。Artifact child repoは変更しない。

## 2026-10-16: Composition node removal preserves descendants below direct children

- **関連:** `Artifact/src/Composition/ArtifactCompositionNodes.cppm`、`tests/Artifact/CompositionNodeStoreContractTest.cpp`。
- **確認できた事実:** `removeNode()`は対象nodeを削除し、直接のparentが削除IDと一致するnodeだけ`parentId`を空にする。より深いdescendant linkは維持され、追加した4階層回帰テストを含むstandalone suiteは8/8 passした。
- **価値／懸念:** 直接子のroot detachと再帰削除を区別し、階層のrootが変化しても子孫を保持する契約を保護する。root同orderの並び順には依存しない。
- **次に確認:** 既存CMake構成ではMSVC `/showIncludes`の日本語一覧がNinja出力へ漏れた。接頭辞を英語に揃えるには許可済みの次回configure時に`VSLANG=1033`をconfigure/compile双方へ設定して確認する。

## 2026-10-16: Playback state assertions should include transport frame-clock behavior

- **関連:** `tests/Artifact/PlaybackRangeServiceContractTest.cpp`。
- **確認できた事実:** 既存Play/Pause/Resume/Stop integration case checked only the service state enum. PlaybackService exposes `currentFrame()`, allowing the same isolated process to observe clock progress without adding UI or graphics.
- **価値／懸念:** The test now checks initial progress, a short stable frame while paused, and renewed progress after resume. Timing uses bounded polling and does not start Artifact.exe; runtime behavior remains unverified.
- **次に確認:** Run only the PlaybackRangeService contract target after a build path that avoids the full ArtifactAppRuntime module rescan is available.

## 2026-10-16: Render cache identity tests should cover both equality and hashing

- **関連:** `tests/ArtifactCore/RenderPipelineContractTest.cpp`、`ArtifactCore/include/Graphics/RenderPipelineFoundation.ixx`。
- **確認できた事実:** Cache identity mutations already tested `RenderCacheKey` equality, while the production `RenderCacheKeyHash` delegates to `stableHash()`.
- **価値／懸念:** The shared mutation helper now asserts both key inequality and a changed stable hash for every identity field, guarding against fields omitted from the hash path.
- **次に確認:** Execute the focused target when the existing ArtifactCore build graph can be reused without a full module rescan.

## 2026-10-16: Surface cache counter changes need rendered-pixel evidence

- **関連:** `tests/Artifact/CompositionLayerSurfaceCacheMain.cpp`、Composition Editor presented-frame readback.
- **確認できた事実:** The GUI harness compared pixels only for unchanged rerender. Effect invalidation/rebuild and unrelated-layer non-interference were inferred from diagnostic counters.
- **価値／懸念:** The regression now also requires changed effect output and changed unrelated-layer output in presented-frame readbacks, while the target layer's cache build/invalidation counters remain stable for the unrelated edit.
- **次に確認:** Run the Windows GUI harness only after its ArtifactAppRuntime build is available; its current target requires the broad app module graph and GPU/window runtime.

## 2026-10-16: Convex-edge hit testing needs explicit degenerate geometry cases

- **関連:** `ArtifactCore/src/Geometry/LayerBoundsCalculator.cppm`、`tests/ArtifactCore/LayerBoundsCalculatorStandaloneTest.cpp`。
- **確認できた事実:** `contains()` checks four edge cross-product signs. For a zero-area point all cross products are zero for every query point; for a zero-width line, collinear points beyond segment endpoints also produce all zero values.
- **価値／懸念:** Added standalone assertions for rejecting a nearby off-point and line-segment endpoint extensions/off-axis points. Source analysis predicts failures, but the standalone test was not executed; no ArtifactCore product code was changed.
- **次に確認:** After an authorized independent build setup is available, run the focused standalone test and report the exposed contract failure for a separately authorized child-repo fix.

## 2026-10-16: PrecomposeViewer選択テストのbuildは全module再走査を伴う

- **関連:** `CMakeLists.txt` の `ArtifactPrecomposeViewer` target、`tests/Artifact/PrecomposeViewerMain.cpp`。
- **確認できた事実:** targetは`ArtifactAppRuntime`へリンクし、ArtifactCoreを含む全module dependency scanを開始した。最初の試行は526/7326、二度目は777/6806 scan時点で中断し、ArtifactCoreのCMakeには`EnvironmentVariable.cppm`のmodule IFC `/reference`がない一方でinterface objectの`OBJECT_DEPENDS`だけが残る。過去の同じbuild graphは当該module-not-foundで失敗した。製品アプリやGUI processは起動していない。
- **価値／懸念:** 独立実行ファイルでもAppRuntime全体を参照すると全module再走査が必要になり、さらに既知のIFC blockerがある。GUIケース自体は別processだが、ローカルで繰り返し実行するbuild負荷は高い。
- **次に確認:** ArtifactCore child repoの変更許可が得られた後にIFC参照を修正し、既存build成果物が活用される状態でPrecomposeViewerの`--check`を実行する。現作業ではサブモジュールを変更しない。

## 2026-10-16: AudioReverbのSize公開設定が信号処理へ未接続

- **関連:** `ArtifactCore/include/Audio/AudioReverb.ixx`、`ArtifactCore/src/Audio/AudioReverb.cppm`、`tests/ArtifactCore/AudioReverbStandaloneTest.cpp`。
- **確認できた事実:** `AudioReverb`は`size_`を公開parameter、setter/getter、JSON保存・読込へ含める。一方、現行`AudioReverb::process()`は`size_`を参照せず、`allpassBuffer_`も使っていない。
- **テスト期待:** 同じwet impulseへSize 0と1を設定したとき出力が異なることを独立テストへ追加した。現行sourceとは不一致で、runtime結果は未確認（ビルド・実行なし）。
- **価値／懸念:** 公開UI/APIから変更可能なSizeが実際のreverb応答に影響しない可能性がある。
- **次に確認:** ユーザーの明示的なArtifactCore変更許可後にsizeの意味と期待する信号差を決め、実装・テストを合わせて確認する。現作業では子リポジトリを変更しない。

## 2026-10-16: in-memory ImageLayer差し替えとsurface cache identity

- **関連:** `Artifact/src/Render/ArtifactCompositionViewDrawing.cppm` の `buildLayerSurfaceCacheKey()`、`Artifact/src/Layer/ArtifactImageLayer.cppm` の `setFromQImage()`、`tests/Artifact/LayerSurfaceCacheParityContractTest.cpp`。
- **確認できた事実:** cache keyはImageLayerのsource path/version、crop revision、surface dimensions等を使う。`setFromQImage()`はsource asset ID/pathを消し、source versionを0にする。同サイズのin-memory画像を差し替える場合、見たコード上はsource content revision/hashがkeyへ加わる経路を確認できなかった。
- **未検証の仮説:** 同じLayer objectのcache entryが残った状態で同寸法の画像を差し替えると、古いrasterized surfaceが再利用される可能性がある。test-only比較を試みたが、実行できず現状では挙動未確定のため、#31の完了ケースからは外した。
- **価値／懸念:** 画像編集ツールが`setFromQImage()`を使うときのsource更新とComposition surface cache無効化の整合性を検証できる。
- **次に確認:** Artifact child repoの変更許可が得られた後、差し替え時のrevision伝播とcache key更新を追い、cached/bypass出力を比較する。現行作業ではサブモジュールを変更しない。

## 2026-10-16: InOutPoints XML不正入力時の状態保持

- **関連:** `Artifact/src/Composition/ArtifactInOutPoints.cppm` の `ArtifactInOutPoints::importFromXML()`、`tests/Artifact/InOutPointsContractTest.cpp`。
- **確認できた事実:** `importFromXML()`はXMLの構文検証が終わる前に既存markersとIn/Out pointsを消去し、解析後に`reader.hasError()`を返す。途中まで有効なXMLを読んだ後に構文エラーとなる入力では、その部分状態が残る実装になっている。
- **未検証の懸念:** malformed XMLの拒否時に既存値を保つ契約が必要かは未確認。現行のJSON importは形式検証後に状態を入れ替えるため、XMLとの失敗挙動に差がある。
- **価値／懸念:** XML importerを利用する旧データ移行やユーザー操作で、壊れたファイルにより既存marker/rangeが失われる可能性がある。
- **対応:** #21のルート独立契約テストへ、既存rangeとmarkerを設定して途中まで有効なmalformed XMLを読み込ませ、失敗時の状態保持を期待するケースを追加。現行Artifact importerとは不一致がソース上明らかだが、ユーザー指示によりビルド・実行しておらずruntime結果は未確認。
- **次に確認:** この原子性契約がruntimeで失敗することを確認し、Artifact child repoの変更許可後に一時状態へparseして成功時だけcommitする実装修正を行う。

## 2026-10-16: In/Out PointsのImpl解放経路

- **関連:** `Artifact/include/Composition/ArtifactInOutPoints.ixx`、`Artifact/src/Composition/ArtifactInOutPoints.cppm`、`tests/Artifact/InOutPointsContractTest.cpp`。
- **確認できた事実:** `ArtifactInOutPoints` のコンストラクタは `impl_(new Impl())` でraw pointerを確保し、現在の実装ファイルではデストラクタが `= default` 定義になっている。
- **未検証の仮説:** `Impl` が別の所有経路で解放されていなければ、各インスタンス破棄時に漏れる可能性がある。今回追加するテストは小数のインスタンスしか生成しないため、影響を検出・評価しない。
- **価値／懸念:** In/Out Pointsを繰り返し生成・破棄する再生やComposition切替経路で、累積メモリ使用量へ影響する可能性がある。
- **次に確認:** 全デストラクタ定義と所有モデルを確認し、明示deleteが不要な意図があるか調べる。漏れが確定した場合は、テスト追加範囲から切り離して最小修正を検討する。

## 2026-10-16: Rasterizer mask/effect順序とalpha境界

- **関連:** `Artifact/src/Render/ArtifactCompositionViewDrawing.cppm` の `buildRasterizedSurfaceBuffer()`、`Artifact/src/Mask/LayerMask.cppm` の `applyToImage()`、`Artifact/src/Effects/Blur/BlurEffect.cppm`、`tests/Artifact/RasterizerMaskEffectIntegrationTest.cpp`。
- **確認できた事実:** Composition ViewのCPU surface処理はLayerMaskをCV surfaceへ適用した後、Rasterizer Effectを順に実行する。`LayerMask::applyToImage()` は4 channelをmaskで乗算し、BlurEffectは既定でpremultiplied alpha処理を有効にする。
- **未検証の仮説:** mask外周へblurのalphaが広がることが意図された合成契約か、maskをeffect後に適用する選択肢が必要かはruntime画像で未確認。追加したケースは現行順序でのalpha分布を検査するが、未実行。
- **価値／懸念:** boundary alphaを固定fixtureで監視すると、Effect/Mask順序の変更による輪郭差を小画像で検知できる。結果が意図と違う場合は、テスト期待値を変える前に合成仕様を確認する。
- **次に確認:** 許可されたCPU integration test実行で32×16 fixtureの内部・境界・外側alphaを確認し、必要ならmask/effect orderの製品仕様を決める。

## 2026-10-16: 重複時刻キーフレームの選択規則

- **関連:** `ArtifactCore/include/Geometry/Interpolate.ixx` の `KeyframeInterpolator::addKeyframe()` / `evaluate()`、`tests/ArtifactCore/KeyframeSplineTest.cpp`。
- **確認できた事実:** `addKeyframe()` は時刻で `std::sort` するが、同時刻エントリーの順序を定義していない。`evaluate()` は同じ時刻のキーを `lower_bound` で探し、完全一致時にはその位置の値を返す。
- **未検証の懸念:** 同時刻キーが複数ある場合、どの値が採用されるかは安定した契約として保証されていない可能性がある。現時点では仕様を推測してテストに固定していない。
- **価値／懸念:** キー編集・読み込みで同時刻キーが生じた場合の結果を明確にできれば、保存再読込やUndo後の再現性を高められる。
- **次に確認:** 重複時刻を拒否するか、先勝ち／後勝ちに正規化するかを仕様として決め、その契約に沿った単体テストを追加する。


## 2026-10-15: 共有子Composition解除時のsource layer所有権

- **関連:** `Artifact/src/Service/ArtifactProjectService.cppm` の `unprecomposeLayerInCurrentComposition()`、`ArtifactCore/src/Composition/PreCompose.cppm` の `unregisterPrecompLayer()`、`tests/Artifact/PrecomposeViewerMain.cpp` の `test.sharedChildReferences`。
- **確認できた事実:** `PreComposeManager` は同じchildを複数のwrapperから参照でき、1件の解除では他のwrapper登録を残す。一方、Serviceの解除処理は残存参照を確認せずchildのlayer objectを親へ移動する。
- **未検証の仮説:** 残存wrapperのsource childが空になり、その後の描画・保存・編集で共有composition参照が破損した状態になる可能性がある。追加した独立MCPケースは解除後とRedo後のsource layer ID保持を要求するが、実行していない。
- **価値／懸念:** 登録mapだけでなく、複数wrapper間で共有するCompositionデータの所有権・解除契約を回帰検知できる。期待動作が「解除拒否」「残存参照向けの内容複製」「共有を許さない」などのどれかは仕様確認が必要。
- **次に確認:** ユーザーが許可したビルド・実行で `test.sharedChildReferences` の実結果を確かめ、期待する共有Compositionの解除仕様を決める。テスト依頼の範囲を越えるService変更は別途指示を待つ。


## 2026-10-15: シェイプ描画テストで自動保存される初期形状を除去する

- **関連:** `Artifact/src/Layer/ArtifactShapeLayer.cppm` の `addShapeContent()`、`tests/shape_visual/ShapeVisualCases.inc`。
- **確認できた事実:** 空の content 配列へ最初の content を追加すると、指定 content より先に `makeContentFromLegacy()` の結果が格納される。content の Intersect は先行 content の描画を消さず、後続 content の形状だけを先行形状で切り抜く。
- **価値／懸念:** 初期矩形が穴や透明領域を覆い、意図した入力と異なる画像を検証する恐れがある。専用テストでは初期 snapshot を除去し、content Intersect の入力は塗りを無効にして演算結果を分離した。
- **次に確認:** ユーザーが許可したビルド・実行で実際の座標と alpha を確認する。製品側の仕様変更を提案するものではない。


## 2026-10-15: 既存プリコンポーズレイヤー移動時の親階層登録

- **関連:** `Artifact/src/Service/ArtifactProjectService.cppm`、`ArtifactCore/src/Composition/PreCompose.cppm`、`tests/Artifact/PrecomposeViewerMain.cpp`。
- **確認できた事実:** `precomposeLayersInCurrentComposition()` は新規ラッパーレイヤーを `PreComposeManager::registerPrecompLayer()` に登録する。一方、選択レイヤーとして移動する既存プリコンポーズレイヤーについて、移動先親への登録更新は確認できなかった。
- **未検証の仮説:** 既存プリコンポーズをさらに別のプリコンポーズへ移動すると、Managerの親参照とComposition階層が以前の親のまま残る可能性がある。独立MCPケース `test.reparentExistingPrecompose` は移動時に加え、Undoで元親、Redoでラッパー親へ登録が戻るかも検査する。実行は未確認。
- **価値／懸念:** ネスト済みcompositionの親逆引きや時間変換が、再編成後に古い親構造へ依存する回帰を検出できる。
- **次に確認:** 許可されたビルド／テスト実行で新ケースを動かし、失敗した場合はテストだけの依頼範囲を越えずに結果を報告する。


## 2026-10-15: ネストしたプリコンポーズ解除時のManager登録と時間復元

- **関連:** `Artifact/src/Service/ArtifactProjectService.cppm` の `unprecomposeLayerInCurrentComposition()`、`ArtifactCore::PreComposeManager`、`tests/Artifact/PrecomposeViewerMain.cpp`。
- **確認できた事実:** 解除経路は子レイヤーを親compositionへ戻し、非ゼロのnest start offsetを `slideTimingBy()` で戻した後、Managerの `unprecompose()` を呼ぶ。Undo/Redoでは別コマンド経由でネスト登録を復元する。
- **未検証の仮説:** ルート→子→孫の孫階層を解除・Undo・Redoした際、レイヤー時間とManagerの親参照が全段で一貫して復元されるか、既存ケースでは直接確認していない。
- **価値／懸念:** 見た目のツリーだけでなく、孫レイヤーID・In/Out/Startと実行時のネスト登録が保たれることを回帰検知する。
- **次に確認:** `test.unprecomposeNested` の実行は未確認。許可された環境でテスト実行し、結果を確認する。


## 2026-10-15: プリコンポーズのプロジェクト再読込後に実行時ネスト登録を確認する

- **関連:** `PreComposeManager`、`ArtifactProjectManager::loadFromFile()`、`tests/Artifact/PrecomposeViewerMain.cpp`。
- **確認できた事実:** `ArtifactProjectManager::loadFromFile()` はインポートしたProjectを差し替えて `ProjectCreatedEvent` をpublishする。確認したImporterとProjectCreated handlerには `PreComposeManager` の復元呼び出しがなく、プロジェクトコードを含む検索でも再読込経路の登録箇所は見つからなかった。プリコンポーズレイヤー自身には `sourceCompositionId()` があり、永続レイヤーデータ上の参照とメモリー上のネスト登録は別経路である。
- **未検証の仮説:** `PreComposeManager` 登録が再読込後に復元されず、親逆引き・階層時間変換が使えなくなる可能性が高い。ただし実行確認はまだ行っておらず、静的検索だけでは最終判定できない。
- **価値／懸念:** プロジェクトの親子参照が永続化されても、プロセス内のネスト補助状態が復元されない場合、編集・時間変換機能が再起動後に不整合になる恐れがある。
- **次に確認:** 独立GUIテストは保存後に対象登録を明示解除し、再読込後の `isPrecomposeLayer()`、`getCompositionNesting()`、子からの親逆引きを確認するよう拡張中。実行時に欠落が確認された場合の製品側復元変更は、テスト追加とは別の依頼または設計判断を待つ。


## 2026-10-15: 独立GUIテストの共通MCPヘルパー参照と実ファイルの整合

- **関連:** `tests/Artifact/FbxAnimationViewerMain.cpp`、`docs/planned/MILESTONE_FBX_ANIMATION_VIEWER_TEST_2026-10-14.md`。
- **確認できた事実:** このチェックアウトではFBXテストがincludeする `tests/ArtifactTestMcpServer.h` と `tests/ArtifactTestWindowCapture.h`、文書で参照する `tools/test_mcp_client.py` が存在しない。プリコンポーズ独立テストは製品変更を避けるため専用のWindows stdio経路を持つ。
- **価値／懸念:** 文書だけから共通テスト基盤を再利用すると、欠落依存でビルドできない可能性がある。他checkoutでの存在・履歴は未検証。
- **次に確認:** FBXテストを次回変更する際、共通ヘルパーの元コミットと追跡状態を確認し、実ファイルが揃ってから各GUIテストの通信処理を統合できるか検討する。


## 2026-10-15: Preserve straight alpha across PNG import and export

- **関連:** `ArtifactCore/src/IO/Image/ImageExporter.cppm`, `ArtifactCore/src/IO/Image/ImageImporter.cppm`, `tests/ArtifactCore/ImageIoContractTest.cpp`。
- **確認できた事実:** A half-alpha PNG round-trip changed RGB because OpenImageIO's PNG output/input plugins assume associated alpha by default, while `QImage::Format_RGBA8888` and `RawImage` use straight/raw channel values. Setting the OIIO `oiio:UnassociatedAlpha` attribute disables the implicit alpha division/multiplication in the PNG plugins.
- **変更・検証:** The image exporter marks 4-channel input as unassociated; the importer supplies that option at read time. Added tests cover half-alpha QImage and ImageBuf PNG round-trips, both of which pass.
- **次に確認:** If more image formats gain OIIO support, verify each plugin's alpha convention and metadata behavior separately; this result covers the PNG path.


## 2026-10-15: 2D particle render-queue test must distinguish particles from background

- **関連:** `tests/Artifact/RenderQueueLayerImageIntegrationTest.cpp`、`Artifact/src/Render/ArtifactOffscreenCompositionRenderer.cppm`、`ArtifactCore/src/Graphics/ParticleRenderer.cppm`。
- **確認できた事実:** 既存の2D粒子integration testは「RGBのいずれかが40より大きい画素」を数えていた。背景 `(29, 36, 50, 255)` も全画面で条件に該当し、粒子がなくても成功していた。色条件を粒子色に絞ると既定GPU backendで失敗し、Brightness出力にも粒子がなかった。`ArtifactParticleLayer::goToFrame()` はシミュレーションを進めず、描画または `ParticleSystem::goToFrame()` が状態を更新する契約だった。
- **変更:** 2D粒子のCPU Render Queue面を追加し、frame cacheを再利用、cache miss時はcomposition fpsで `renderFrame()` を呼ぶ。テストは復元後のシミュレーションを `ParticleSystem::goToFrame()` で明示し、CPU backendでベース画像とBrightness適用画像の粒子画素・背景不変を検査する。`ArtifactCore::ParticleRenderer`では深度を使わないPSOのDSV形式を`UNKNOWN`に変更した。一時診断ログは撤去済み。
- **検証:** Debug targetをビルドし、`Production2DAnd3DParticlesRenderThroughManagerQueue` は成功。2D粒子のCPU描画＋Brightnessと3D粒子GPU描画を含む。追加で既定GPU backendの2D粒子を確認したところ、粒子出力が依然なく失敗した。PSOのDSV修正でDSV mismatch警告は消えたが、GPU描画復旧には至っていない。低レベル2D契約テストはglobal test environment setup後に停止し未確認。
- **価値／懸念:** 背景色によるfalse positiveをなくし、CPU Render Queue経路で2D粒子とラスタライザーエフェクトの結合を回帰検知できる。PSO修正後もGPU 2D粒子経路は未解決で、現在の新しい2D assertionはCPU backendを対象とする。
- **次に確認:** GPU 2D粒子surfaceのreadback、後段 `drawSprite`、最終RTVへのsubmitを切り分け、CPU／GPU両backendで同じ粒子画像とBrightness結果を検証する。

## 2026-10-14: RT shading geometryへvertex colorを統合

- **関連:** `ArtifactCore::RayTracingManager`、`ArtifactIRenderer`、Raster／RTのPrincipled shading。
- **確認済み事実:** Raster `MeshRenderer` は頂点カラーRGBをsRGB→linear変換してbase colorへ乗算し、頂点alphaもmaterial alphaへ乗算する。RT shading geometryは位置・法線・UV・primitive material indexを保持していたが、vertex color入力はなく、RT closest-hitとMasked any-hitで未使用だった。
- **変更:** 頂点カラーをRT geometry bufferへ追加し、closest-hit base colorとany-hit alpha clipへ渡す。vertex colorがないgeometryは白として扱う。
- **価値／懸念:** 頂点カラー付きmeshのRaster／RT差を減らし、頂点alphaを使ったMasked材質をRTでも表現する。各importerが出す頂点RGBのtransfer encoding契約は一元化されておらず、Raster既存処理へ合わせたためglTF等のlinear vertex colorの扱いは未解決。compile／画像parity未確認。
- **次に確認:** source importerごとの色空間契約を整理し、頂点色fixtureでRaster／RTとalpha clipを比較する。

### 2026-10-13 — Render Queueのテキスト統合テストはshaping parityまで段階化する

- **関連:** `tests/Artifact/RenderQueueLayerImageIntegrationTest.cpp`、`tests/Artifact/CMakeLists.txt`、`tests/ArtifactCore/TextShapingTest.cpp`、`tests/Artifact/TextGlyphRenderContractTest.cpp`。
- **確認できた事実:** Render Manager本番queue経由の通常Text Layer画像テストは既存だった。Source Textは`setSourceTextAtFrame()`でproperty keyframe化される。Core shaping suiteにはアラビア語、デーヴァナーガリー、ZWJ family emoji、肌色modifier等の低レベルcaseがある一方、Render Queueからそれらを含む文を描画するcaseはなかった。
- **変更:** queue integrationへ2-frame Source Text、アラビア語+結合記号/デーヴァナーガリー数字の混在文、ZWJ family/肌色modifier/国旗/keycap emojiの描画caseを追加し、個別CTest entryを登録。各出力の明るいglyph pixel存在を検査する。
- **価値／懸念:** low-level shapingやGPU glyph submitterの検証と、本番queue/合成/出力の結合を区別して回帰検知できる。現在のpixel-count assertionは文字が何らかの形で描画されたことを確認するだけで、glyph順序・cluster単位・色付きemojiの完全な見た目を保証しない。フォントfallbackは環境依存。
- **実行結果 (2026-10-13):** Windows Debug / D3D12 headlessで3件の個別CTestがすべて成功した。Source Textケースでは `text.value` property生成のため `getLayerPropertyGroups()` を先に呼ぶ必要があり、Composition duration 3・Custom range `[1,3)` で2枚を出力する。最初はdurationを誤って2に上書きしていたため2枚目が範囲外だった。
- **次に確認:** 現在の画素数assertionはglyph順序・cluster単位・色付きemojiの完全な見た目を保証しない。必要なら固定font fixtureと承認baselineでshaping/emoji visual parityを追加する。

### 2026-10-12 — Partial module rebuilds can retain stale allocation templates

- **関連:** `Artifact/include/Render/GPUTextureCacheManager.ixx`、`Artifact/src/Render/ArtifactRenderQueueService.cppm`、`Artifact/src/Widgets/Render/ArtifactCompositionRenderController.cppm`、`tools/VideoPreviewBench/README.md`。
- **確認できた事実:** 通常ログに出ない first-chance UUID 解析例外と、終了時の `GPUTextureCacheManager` ヒープ破損を native Windows debugger で捕捉した。空の bulk Removed ID を nil として扱い、追加の生成所有者 `ArtifactRenderQueueService.cppm` を再コンパイルして canonical standalone bench を再リンクした後、両方の障害は再発しなくなった。D3D12VA output texture に simultaneous-access flag を指定した後、COPY queue の barrier layout ERROR も消えた。H.264 1/2 層・3 秒ずつの実行で fault exception / CRT heap assertion / D3D12 ERROR はすべて 0。性能条件は引き続き未達。
- **設計上の推論:** stale な `make_unique<GPUTextureCacheManager>` の COMDAT が別の object に残ると、生成側の古い class size と新しい実装が混在し得る。個々の constructor 命令サイズは未比較だが、生成所有者の再コンパイルでヒープ破損が解消した。
- **価値／懸念:** exported class のメンバー追加後は生成／破棄所有者も同じ module layout にそろえる必要がある。通常ログだけで例外なしと判断しない。simultaneous-access は fence の代替ではなく、texture compression / bandwidth への影響は未測定。
- **次に確認:** partial build を続ける場合の importer／allocation-owner freshness を機械的に照合する方法を検討する。性能検証再開前に canonical exe と DLL を固定し、cache enabled / ProRes と長時間の runtime fault gate を確認する。

未着手の設計判断、現在の優先方針に直結する実装候補、実機検証待ちだけを記録する。実装済みの詳細履歴と過去の調査は [Insight Archive (through 2026-09-01)](docs/analysis/INSIGHT_ARCHIVE_2026-09-01.md) を参照。

### 2026-10-12 — Animation Dynamics spring/damperの独立テスト対象
- **関連:** `ArtifactCore/include/Animation/AnimationDynamics.ixx`、`tests/ArtifactCore/AnimationDynamicsStandaloneTest.cpp`、`tests/ArtifactCore/AnimationDynamicsStandalone/CMakeLists.txt`。
- **確認できた事実:** `Animation.Dynamics`には1D/2D/3D spring-damper、Lag Follower、DynamicsChannelがあるが、既存testsには同module専用suiteがなかった。既存module interfaceは`Core.ArtifactString`だけをimportするため、standalone module interface projectで依存moduleを追加せずに登録できる形。
- **変更:** Artifact.exeやArtifactCore全体へリンクしないstandalone GTest targetを追加し、初期seed、更新方向、非有限target保持、dt上限clamp、2D軸独立、channel resetを契約化した。
- **価値／懸念:** 小さな決定的なsuiteでバネ挙動の回帰を分離できる。今回の更新方向caseは振動・収束率や解析解との誤差を主張するものではなく、resetは次回updateのtargetへ通常seedする契約を確認する。
- **次に確認:** ユーザー許可後にstandalone CMakeをconfigureし、MSVC module interface / GTest targetのbuildとCTestを実行する。数値安定性、preset/質量境界、3D、Lag/Off、overshoot limiterは追加候補。

### 2026-10-12 — Granular MPM Plane colliderを画面座標の床へ合わせる
- **関連:** `ArtifactCore/src/Physics/MpmSolver2D.cppm` (`resolveColliders()` Plane branch)、`ArtifactCore/src/Physics/PhysicsSystem.cppm` (`createGranularMaterialGrid()` gravity contract)。
- **確認できた事実:** MPM SandはCompositionの画面座標で重力`+Y`（既定`980 px/s²`）を使う。一方、Plane colliderは従来 `particle.y < plane.y` を `+Y` 法線で押し戻すため、`+Y`方向へ落下する粒子はplaneを越えても接触判定されず、既存の向きはy-up floorだった。Composition collider生成はBox/Circle/Polygonを使いPlaneを生成していない。
- **変更:** Granular Drucker–PragerのPlane branchだけをy-down座標向けhorizontal floorに変更し、`particle.y > plane.y` で位置をplaneへ戻して法線`-Y`のGranular速度応答を適用する。従来Corotated MPMのPlane挙動は維持。
- **価値／懸念:** 公開Granular solver APIでPlaneを床として使う場合、落下・法線反発・接線摩擦が重力座標と一致する。Granularの天井/任意向き片側planeを表すAPIはなく、このplaneは床として定義する。砂のplane接触はruntime未検証。
- **静的符号照合:** 落下速度`vY>0`と床法線`n=(0,-1)`では`vn=-vY<0`となり、共通応答式から反発後`vY'=-e*vY`。上向き離反時は`vn>0`なので法線速度を再反射しない。接線速度とGranular APIC `C`には既存摩擦projectionが適用される。
- **次に確認:** `y<plane`, `y=plane`, `y>plane`で静止/接近/離反を検査し、`+Y`落下速度への反発と接線friction、Corotated legacy応答をfixtureで確認する。

### 2026-10-12 — MPM moving colliderは幾何位置だけで速度を伝えない
- **関連:** `Artifact/src/Composition/ArtifactAbstractComposition.cppm` (`evaluateLayerCollisionPairs()`)、`ArtifactCore/include/Physics/MpmSolver2D.ixx` (`MpmCollider2D`)、`ArtifactCore/src/Physics/MpmSolver2D.cppm` (`resolveColliders()`、`applyGranularColliderVelocityResponse()`)。
- **確認できた事実:** Compositionは各frameのsource geometryをsolver local空間へ再配置するが、`MpmCollider2D`はposition/size/restitution/frictionだけでlinear/angular velocityやprevious poseを持たない。Granular接触応答は粒子速度の法線/接線成分だけを補正し、接触時にsource velocityを差し引いたり加えたりしない。粒子位置のpenetration correctionは実行する。
- **推測／未検証:** アニメーション床・壁は砂を幾何的に押し出せても運動量を砂へ与えず、sourceが離れた後の粒子速度が静止のままになる可能性がある。step幅・貫通量・連続するframeの接触履歴による見た目はruntime未確認。
- **価値／懸念:** 動的colliderの transform追従はposition-only kinematic contactであり、運動学的接触ではない。速度を追加する場合は前frame pose、Compositionからsolver localへの相対速度変換、polygon/circleの接触点に応じた角速度、seek replayの再構築を揃える必要があり、単純にtransform差分を一様速度として足すのは回転体で誤る。
- **次に確認:** 静止粒子へ平行移動床・回転polygonを一定速度で当てる定量fixtureを作り、現行のposition-only応答をbaseline計測した上で、動的接触速度を初版要件に含めるか決める。

### 2026-10-12 — 通常再生のmaterial colliderはComposition frame transformを評価
- **関連:** `Artifact/src/Composition/ArtifactAbstractComposition.cppm` (`setFramePosition()`、`evaluateLayerCollisionPairs()`)、`Artifact/src/Layer/ArtifactAbstractLayerTransform.cppm` (`getGlobalTransform()` / `getGlobalTransformAt()`)。
- **確認できた事実:** PlaybackEngineは通常再生でComposition `setFramePosition()` を呼ぶ。`ArtifactAbstractLayer::goToFrame()` は明示seek/replayでのみ呼ばれ、layer `currentFrame_` は通常stepでは更新されない。`getGlobalTransform()` のcache keyはlayer `currentFrame_`である一方、transform cache値にはComposition時刻で変化するkeyframeが含まれるため、通常再生中に従来collision evaluationが同APIを使うとtransform cacheが旧layer frameのままになる。seek replayは各layerを先に`goToFrame(currentFrame)`するため症状が表面化しにくい。
- **変更:** `evaluateLayerCollisionPairs()` のcollision source/target transform取得を `getGlobalTransformAt(position_.framePosition())` に変更した。親layerも同じ明示frameで再帰評価する。各simulation frameで再構築するcollider proxyとinverse transformがComposition frameに追従する。
- **価値／懸念:** 通常再生時に動くcollision source、移動するMPM/soft material target、親transform chainを現在frameで評価できる。明示transform評価のCPUコストは増える可能性があり未計測。ほかの `getGlobalTransform()` consumerが持つ同種問題は別途調査対象。
- **次に確認:** 通常playbackとseek replayでanimated box/circle/polygon colliderのlayer-local proxy座標を同frame比較し、静止sourceとanimated target、親子transformを含む受入確認を行う。

### 2026-10-12 — MPM Sand parameter previewをcommit-onlyにする
- **関連:** `Artifact/src/Widgets/ArtifactPropertyWidget.cppm` (`previewLayerValue()`)、`Artifact/src/Widgets/ArtifactPropertyWidgetShared.cppm` (`applyPreviewValue()` / `applyCommitValue()`)、`Artifact/src/Layer/ArtifactMpmSand2DLayer.cppm`。
- **確認できた事実:** Shared property rowはpreview時に先に`AbstractProperty` cacheを候補値へ更新し、その後void preview handlerを呼ぶ。MPM Sand setterはsolver再生成失敗時にauthoring値/cacheをrollbackするが、PropertyWidget preview handlerはsetterのboolを無視し、失敗後のcache状態をeditorへ再同期する契約がなかった。MPM parameter editはsolverを再生成し粒子をseedし直す。
- **変更:** `ArtifactPropertyWidget::previewLayerValue()` は `ArtifactMpmSand2DLayer` の `mpmSand.*` propertyをpreview applyせずreturnし、編集をcommit boundaryに限定した。既存commit pathはsetter後にproperty cacheと候補値の差を検出してeditorをcanonical valueへ戻す。実装TUへ `Artifact.Layer.MpmSand2D` importを追加。
- **価値／懸念:** slider scrub中の頻繁なsolver再生成・砂状態破棄を止め、solver生成拒否時もcommit callbackのcanonical resyncを使える。砂の連続live previewはなくなるが、実際の編集ではcommit時に適用される。UI runtimeは未確認。
- **次に確認:** MPM parameterのspinbox text edit、slider scrub+release、solver予算超過によるcommit拒否、Undo/Redo、multi-selectionで表示値・solver state・command履歴が同期することをruntime確認する。

### 2026-10-12 — MPM Sand multi-selection commitは失敗時に部分適用しうる
- **関連:** `Artifact/src/Widgets/ArtifactPropertyWidget.cppm` (`commitLayerValue()`)、`Artifact/src/Layer/ArtifactMpmSand2DLayer.cppm` (`setLayerPropertyValue()`)、`ArtifactCore/src/Physics/PhysicsSystem.cppm` (`createGranularMaterialGrid()`)。
- **確認できた事実:** PropertyWidgetは選択対象を順に更新し、各 `setLayerPropertyValue()` の戻り値を無視する。MPM setterはsolver生成失敗時に当該Layerだけauthoring値とproperty cacheをrollbackするが、既に成功した別Layerのsolverをまとめて戻すmacro transactionはない。各Compositionの粒子・grid/checkpoint予算により、同じ編集でもtargetごとに生成可否が異なり得る。
- **推測／未検証:** 複数Compositionまたはworld予算の限界付近で選択MPM Layerへ一括編集すると、一部だけ新設定/再seedされ、代表Layer editorの表示値は代表Layerの成否だけを示す可能性がある。具体的な予算配分再現は未実施。
- **価値／懸念:** 一括編集の原子性・表示値整合に関わる。正しくrollbackするには事前に全target solver payload/grid予算をreserveするか、旧solver state・world clock・checkpointを保持するtransaction APIが必要。後者は大きなparticle payloadを二重保持しうるため、安易な逐次rollbackは旧simulation historyを復元しない。
- **次に確認:** MPM multi-selectionを許す製品仕様と同一Composition/worldの有無を整理し、world単位preflight/commit APIまたは失敗対象別の結果表示を設計する。現行依頼の範囲では未修正。

### 2026-10-12 — MPM frame-rate変更時はworld clock remainderもreset
- **関連:** `Artifact/src/Composition/ArtifactAbstractComposition.cppm` (`setFrameRate()`)、`ArtifactCore/src/Physics/PhysicsSystem.cppm` (`invalidateGranularMaterialSnapshots()`、`setPhysicsTimelineSettings()`)。
- **確認できた事実:** Granular checkpointはframe位置とComposition world-clock remainderを対で保存する。frame rate変更時、Compositionは旧frame→秒マッピングのcheckpointを捨てるが、従来はlive world clock remainderを保持していた。再生replayはcache miss時にsolver baseline/current stateからframe 0扱いで進むため、旧時間軸のremainderが残ると新しいframe delta列と二重計上される。Composition単位のcache invalidationへ任意のclock resetを追加し、`setFrameRate()`だけ明示的にresetするよう修正した。全体Physics timeline設定の変更では他Compositionへ副作用を出さないようworld clockを維持し、granular checkpointだけを消去する。
- **価値／懸念:** Frame-rate変更後の新しいframe-time mappingと粒子clockの開始境界を一致させる。solver粒子状態はresetせず保持するため、速度・変形履歴の時間解釈を新rateへつなぐ意味論は製品上の選択として残る。変更APIはビルド/runtime未検証。
- **次に確認:** frame rate変更直前に非ゼロclock remainderを作り、rate変更後frame 0/1 replayと連続再生の粒子stateが新時間軸で一致するfixtureを実行する。

### 2026-10-12 — MPM Hencky-Drucker-Prager基準例を独立算術照合
- **関連:** `ArtifactCore/src/Physics/MpmSolver2D.cppm` (`projectMpmGranularHencky2D()`、`applyGranularPlasticity()`、`firstPiolaKirchhoff()`)、`docs/planned/MILESTONE_MPM_GRANULAR_SAND_2026-10-11.md`。
- **確認できた事実:** 文書の`μ=1, λ=1.5, φ=35°`, `ε=(-0.5,0.3)`例をPythonのdouble算術で再計算し、`α=0.4055797877`, `δγ=0.3628955311`, `H=(-0.2433941091,0.0433941091)`, `tr(H)=-0.2`を得た。実装は`||dev ε||F + (1+λ/μ)tr(ε)α`と同じ式を使う。Hencky Kirchhoff主応力から各特異値で割り、`U`/`V`基底へ戻す処理は`P=τ Fe^{-T}`と一致し、投影点のyield residualは約`-5.6e-17`。
- **価値／懸念:** 代表的な2D return mapping係数・符号・主応力変換は文書例と整合する。これは独立算術の確認で、C++ float実装・特異ベクトル再構成・shear/rotation・連成MPMの安定性を検証したものではない。
- **次に確認:** 許可された数値fixtureでhydrostatic compression、extension tip、pure shearのtrace保持、回転付き非対角Fe、`F=Fe Fp`再構成誤差、非有限入力拒否を評価する。ビルド／テストはユーザー指示待ち。

### 2026-10-12 — MPM friction hardening曲線の最大値は原典どおり

- **関連:** `ArtifactCore/src/Physics/MpmSolver2D.cppm::hardenedMpmFrictionAlpha2D()`
- **確認できた事実:** 実装式はKlár et al. 2016式(30)の`phi= h0 + (h1*q-h3) exp(-h2*q)`に対し、`h0=初期摩擦角+h3`としてq=0でLayerの設定角になるよう調整している。`h1=9°`, `h2=0.2`, `h3=10°`は同論文Table 3に記載された係数で、原典はこの曲線にmaximumとasymptoteがあると明記する。実装の89°上限は数値上の角度境界を守る追加clamp。
- **価値／懸念:** q≈6.1で増加から減少へ転じる形はこの式の特性であり、単調hardeningが必要と決めつけて変更すべきではない。論文は3D手法を説明しており、現行2D yield conversionとの物理的一致や、本UI係数解釈のruntime妥当性までは保証しない。
- **次に確認:** q=0、ピーク近傍、q=100の摩擦角・alphaを数値fixtureで固定し、hardening state上限と初期角80°近辺の89°clamp連続性を確認する。

### 2026-10-12 — MPM Sandの物性編集はsolverを再seedする
- **関連:** `Artifact/src/Layer/ArtifactMpmSand2DLayer.cppm` (`setLayerPropertyValue()`、`configureSolver()`)、`ArtifactCore/src/Physics/PhysicsSystem.cppm` (`createGranularMaterialGrid()`、`setGranularMaterialSolver()`)。
- **確認できた事実:** MPM Sandの物性・重力・seed列数/行数のproperty変更は、編集中Compositionがあれば`configureSolver()`を呼ぶ。PhysicsSystemは同じComposition world IDとLayerIDの既存solverを新solverへ置き換え、world snapshot群とworld clock accumulatorを消去/ゼロ化する。新solverは初期seed粒子から生成されるため、編集時点の粒子変形・速度・堆積状態は引き継がれない。solver生成が失敗した場合はLayer側authoring値とproperty cacheをrollbackする。
- **価値／懸念:** 物性変更後に古い物性の履歴を混ぜない明瞭な意味論だが、連続previewでは値が変わるたびに再seedされ、途中でシミュレーションした状態を破棄する。UI操作として意図的なresetなのか、物性だけを現状態へ反映するべきなのかは仕様未確定。単純に状態を引き継ぐとconstitutive history・particle mass/reference volume・checkpoint互換性の整合が必要になる。
- **次に確認:** Property Editorでのpreview/commit、複数選択、Undo/Redo時にsolver再seedが何回起きるか、またユーザーにreset意味論が明示されるかをruntime受入れで確認し、必要なら物性変更とseed解像度変更を別扱いにする仕様を決める。


### 2026-10-12 — Granular CFL floorより速い状態を緊急scaleする
- **関連:** `ArtifactCore/src/Physics/MpmSolver2D.cppm` (`updateGranularTimeStepForState`、`setTimeStep`)。
- **確認できた事実:** Granular CFL推定は`C`由来affine velocityも含む一方、共通`setTimeStep()`は`1e-6 s`未満を受け付けない。要求刻みがさらに小さい場合、従来はCFL要件を満たさない最小値へclampされていた。
- **変更:** 波速を差し引いた最小刻み互換の移流速度上限を求め、それを超える粒子だけ`vel`と`C`を同一比率で縮小して0.1%の余裕を作る。通常上限内の粒子状態には触れない。
- **価値／懸念:** timestep floor後もaffine stencil速度boundに対するCFLを維持し、過大な有限速度勾配が一歩で格子を飛ぶのを抑える。発火時は物理速度・affine stateを失う非常用の挙動で、発生頻度と材料品質への影響はruntime未検証。
- **次に確認:** 最小刻みの直前・直後でCFL bound、particle momentum、affine fieldの縮小連続性を測り、異常なパラメータ・入力だけが発火することをfixtureで確認する。


### 2026-10-12 — MPM Sand CFL boundにAPIC affine速度を含める
- **関連:** `ArtifactCore/src/Physics/MpmSolver2D.cppm` (`updateGranularTimeStepForState`、P2G)。
- **確認できた事実:** P2G node速度は`p.vel + C*dpos`を使うが、従来のGranular CFL速度上限はparticle centerの`|p.vel|`だけを集計していた。純せん断・回転ではcenter speedが小さくてもstencil内node速度は`C*dpos`分だけ大きくなる。
- **変更:** 粒子ごとに`||C||F`を計算し、4×4 cubic stencilのsupport `|dpos| < 2√2 dx`から得るaffine速度上界を中心速度へ加算してCFL characteristic speedへ渡す。
- **価値／懸念:** 大きな局所velocity gradientを含む状態でtimestepを過大に保つ危険を抑える。Frobenius boundはdirectional boundより保守的になり、step数増加のruntime影響は未計測。
- **次に確認:** pure shear、rigid rotation、併進+shearで`max_i |v + C dpos|`を実サポート上走査し、bound超過がないこと、同一seed/frame結果とCFL step予算を確認する。


### 2026-10-12 — Granular接触でAPIC affine fieldも投影する
- **関連:** `ArtifactCore/src/Physics/MpmSolver2D.cppm` (`applyGranularColliderVelocityResponse`、`stepOnce`、P2G)。
- **確認できた事実:** P2Gは粒子速度に加えて`C * (x_i - x_p)`を格子運動量へ足す。G2P後のcollider/domain補正は粒子`vel`と位置を変更するが、従来は`C`をそのまま残していた。
- **変更:** Granular colliderでは`(I - nnᵀ)C`で法線方向のaffine出力を除き、接線成分へfriction retentionを適用する。粒子domain clampでも同じ法線投影を軸ごとに行う。Corotated経路は変更しない。
- **価値／懸念:** 接触後に残る法線affine velocityが次stepのP2Gで壁向き運動量を再注入する経路を抑える。接線摩擦と角・複数接触の相互作用は数値fixture/runtime未確認。
- **次に確認:** restitution/frictionと壁接触の接近・離反・接線/回転場で1step momentumを測り、affine projection前後の有限性と固定step収束を確認する。


### 2026-10-12 — MPM砂surfaceに固定density閾値がある
- **関連:** `Artifact/src/Layer/ArtifactMpmSand2DLayer.cppm` (`densityAlpha`、`draw`)。
- **確認できた事実:** `densityAlpha()`はdensity `<=0.08`を完全透明にし、triangle生成もquad内の4 sampleがすべて`<=0.08`なら省略する。solverのdensity fieldには0..0.08の正値が保持されても表示されない。
- **推測／未検証:** 塑性流動後に粒子量が広い領域へ薄く伸び、sample densityが閾値を下回ると、物理状態が残っていても砂面が途切れるか消える可能性がある。実際の伸長分布・アンチエイリアス・重なりと閾値の関係はruntime未確認。
- **価値／懸念:** 薄い砂層の可視下限が固定値0.08で決まる。閾値を下げると遠方・疎な粒子のhaloや面積膨張を招くため、単純変更ではなくmass-preserving surface再構成との比較が必要。
- **次に確認:** 圧密 pile・長い薄層・field端を含む粒子配置でfield積分値とrender alpha footprintを測り、mass保持と輪郭の両方から閾値を調整する。


### 2026-10-12 — 弾性MPMのGPU初期化失敗時に時間を二重加算する経路
- **関連:** `ArtifactCore/src/Physics/MpmSolver2D.cppm` (`updateGPUSubsteps`、`update`)。
- **確認できた事実:** GPU substepは初期化前に`accumulatedTime_`へframe deltaを加算する。Compute初期化失敗時は`false`を返し、呼出元はCPU経路へ進んで同じ`elapsedSeconds`を再加算する。GranularモデルはGPU backendを拒否するため、この経路には入らない。
- **推測／未検証:** GPU初期化に失敗してCPU fallbackした最初のupdateで、上限に達していなければ同じ時間を2回分stepし、弾性MPMのsimulation timeが1frameだけ先行する可能性がある。実GPU failure/runtime再現は未確認。
- **価値／懸念:** GPU unavailable/初期化失敗後にCPUへfallbackする弾性MPMの時間軸とseek決定性へ影響しうる。Granular MPMの変更範囲外なので今回は未修正。
- **次に確認:** 初期化失敗時にGPU attempt前のaccumulatorを保存・復元する案を検討し、失敗前後でGPU fallbackとCPU-onlyのparticle stateが一致するfixtureを作る。実装修正はelastic GPU fallback責務として別途扱う。


### 2026-10-12 — Granular snapshot塑性硬化履歴の範囲を検証
- **関連:** `ArtifactCore/src/Physics/MpmSolver2D.cppm` (`MpmSolver2D::canRestoreSnapshot`)。
- **確認できた事実:** Granular return mappingはhardening historyを0..100へclampして使い、更新も上限100で保存する。一方snapshot validationは有限性しか見ず、negativeまたは100超の有限値をrestoreできた。
- **変更:** Granular snapshotだけplasticStrainが `[0,100]` 外ならrestoreを拒否する。既存Corotated snapshot条件は変更しない。
- **価値／懸念:** malformed checkpointが摩擦硬化係数を意図せず変え、seek結果を分岐させるのを防ぐ。保存済みcheckpointがすべて現在のsolver生成由来なら互換性影響はない見込みだが、runtime未確認。
- **次に確認:** 初期snapshot・塑性化後snapshot・値域境界のrestoreと、破損値拒否をfixture化し、restore後stepの粒子状態が連続することを確認する。


### 2026-10-12 — MPM collider geometryを登録時に検証
- **関連:** `ArtifactCore/include/Physics/MpmSolver2D.ixx` (`MpmSolver2D::addCollider`)。
- **確認できた事実:** collider geometryはsolver内部の接触計算で直接使われ、登録APIは検証なしに保存していた。Plane/Circle/Boxの必須座標や寸法がNaN/Infの場合、またPolygon配列が不完全または非有限の場合、粒子位置判定・補正へ不正値が伝播する。
- **変更:** collider typeごとに必要geometryの有限性を登録時に検査し、未知typeと不正Polygonを拒否する。friction/restitutionの方針は係数別のGranular fallbackに委ねる。
- **価値／懸念:** フレームごとの接触計算へ不正形状が入る前に一度だけ遮断し、Polygonの頂点scanもcold registrationに閉じる。既存呼出元が不正な負寸法・radiusを意図していないか、reject時に診断が必要かは未検証。
- **次に確認:** 正常な各collider typeの既存形状が従来どおり登録されることと、不正値が登録されないことをAPI fixtureで確認する。


### 2026-10-12 — MPM砂colliderの非有限摩擦係数を無反応へ退避
- **関連:** `ArtifactCore/src/Physics/MpmSolver2D.cppm` (`resolveColliders`)。
- **確認できた事実:** `std::clamp(NaN, 0, 1)`は有限値へ正規化しない。Composition側のproperty読取はfloat値を取得するが、すべての入口で有限性を保証するとは限らず、NaN係数がGranular contact responseまで届く可能性がある。
- **変更:** Granular経路に限り非有限friction/restitutionを0へ置換してからclampする。Corotated legacy経路は維持する。
- **価値／懸念:** 不正係数1つでparticle velocityへNaNが伝播しsolver状態を壊すのを防ぐ。通常のUI/API入力でNaNが到達するか、退避時の診断が必要かは未検証。
- **次に確認:** 不正係数を持つPlane/Circle/Box/Polygonのcontact fixtureで粒子状態が有限のままか確認し、API境界での一貫した係数validationの要否を判断する。


### 2026-10-12 — Granular Plane/Box contactは内向き速度だけを反射する
- **関連:** `ArtifactCore/src/Physics/MpmSolver2D.cppm` (`resolveColliders`、`applyGranularColliderVelocityResponse`)。
- **確認できた事実:** Circle/PolygonはGranular向け共通法線応答を使う一方、PlaneとBoxは旧式`abs(vNormal)*restitution`を無条件に使っていた。貫通補正で位置を接触面へ戻す際、接触面から離れる粒子速度も反転し、法線速度の符号が変わっていた。
- **変更:** Granular Plane/Boxで接触面の外向き法線を共通応答へ渡し、内向き法線速度のみ反発し、接線成分だけ摩擦する。既存Corotatedの応答は維持する。
- **価値／懸念:** collider貫通補正が接触面から離れる運動を逆流させる問題を防ぐ。Box cornerのtie breakや複数colliderの逐次応答は runtime 未検証。
- **次に確認:** Plane/Boxそれぞれで内向き・外向き法線速度、接線速度、restitution=0/1、摩擦=0/1をfixture化し、既存elastic MPM結果が不変であることを確認する。

### 2026-10-12 — 細長いレイヤーでGranular density kernelを楕円化する
- **関連:** `ArtifactCore/src/Physics/MpmSolver2D.cppm` (`writeDensityField`)、`Artifact/src/Layer/ArtifactMpmSand2DLayer.cppm` (128×96 density field)。
- **確認できた事実:** Granular density kernelは等方円形だが支持半径は`8*min(pixelWorldWidth,pixelWorldHeight)`で制限される。ピクセル寸法比が約8を超えると長辺pixel方向の半径が1pixel未満となり、粒子phaseによって局所kernel sampleが欠けたり、総weightが0となり粒子をfieldへ出せない可能性がある。
- **変更:** Granularのraster-only kernelをaxis-aligned ellipseにし、各軸ごとにpixel diagonal由来のcoverage floorと8pixel ceilingを設定する。粒子ごとの離散weight normalizationと最大18×18 stencilを維持し、既存Corotated density pathは円形のままにする。
- **価値／懸念:** 極端なlayer aspect ratioで砂が密度fieldから脱落する経路を塞ぐ。ワールド空間では異方的な可視supportとなるため、fieldは物理shapeではなく表示用surfaceの再構成だと明確に扱う。絵作り・alpha parityはruntime未確認。
- **次に確認:** aspect ratio 1:1、8:1、16:1超で複数subpixel particle phaseを走査し、field積分・surface連続性・最大18×18 stencil boundを検証する。

### 2026-10-12 — MPM grid seed APIで導出値をreserve前に検証する
- **関連:** `ArtifactCore/src/Physics/MpmSolver2D.cppm` (`MpmSolver2D::addParticlesGrid`)。
- **確認できた事実:** APIは入力引数が有限であることを確認していたが、`w*h`、粒子volume/mass、導出spacingや端点のfloat演算結果は検証せずvector reserveしてから粒子を追加していた。直接呼び出しで積がoverflowすると不正粒子や一部初期化状態を作る余地があった。
- **変更:** seed位置・spacing・volume・massを計算して有限かつ正を確認し、成功後にのみvector reserveとparticle-grid metadata更新を行う。
- **価値／懸念:** Granular factory以外からの直接利用も壊れたseedを登録しにくくする。allocation例外は既存API契約に委ね、runtimeでは未確認。
- **次に確認:** 極端値と通常値でseed追加のall-or-none性、Corotated既存呼び出しの互換性、Granular seedの有限性を確認する。

### 2026-10-12 — Granular seedはparticle volumeと一致するcell-centered配置にする
- **関連:** `ArtifactCore/src/Physics/MpmSolver2D.cppm` (`addParticlesGrid`)、`ArtifactCore/src/Physics/PhysicsSystem.cppm` (`createGranularMaterialGrid`)。
- **確認できた事実:** Granular particle volumeは`w*h/(columns*rows)`だが、初期位置は`w/(columns-1)`間隔で両端を含んでいた。点群のVoronoi代表面積は特に低い列/行数で割当volumeとずれ、PhysicsSystemのCFL用spacingも同じ端点間隔を使っていた。
- **変更:** Granularだけ`w/columns`、`h/rows`間隔のcell centerに粒子を配置し、CFL cell sizeもその小さい間隔（1px floor）から求める。既存CorotatedVonMisesの粒子配置はそのまま維持する。
- **価値／懸念:** 初期質量が表す矩形面積とseed配置の代表領域を揃え、粗い粒子設定でも端の配置に起因する系統的な密度差を抑える。離散support・pile安定性・従来presetへの無影響はruntime未確認。
- **次に確認:** 低解像度と標準解像度で粒子群の重心・総質量・初期密度fieldを比較し、shape material fixtureに変化がないことを確認する。

### 2026-10-12 — MPM SandのE/Poisson比はplane-strain係数を使う
- **関連:** `ArtifactCore/src/Physics/MpmSolver2D.cppm` (`setMaterial`、Hencky stress)、`ArtifactCore/src/Physics/PhysicsSystem.cppm` (`createGranularMaterialGrid`)、MPM Sandの材料property UI。
- **確認できた事実:** 現行Lamé係数は`lambda=E*nu/((1+nu)(1-2nu))`で、plane-stressで使う`E*nu/(1-nu^2)`ではない。一方、降伏面は一次資料の2D in-plane Drucker–Prager定義を使い、out-of-plane stressを含めない。
- **推測／未検証:** 現行構成則はplane-strain stiffnessと2D yieldを組み合わせた準2D制作モデルとして意図されているように見えるが、実砂の校正やUIに示すE/Poisson比の解釈はruntime/設計レビューで確認されていない。
- **価値／懸念:** E/nuが暗黙の次元変換として解釈される誤解を避けるため、初版の仕様に「2D in-plane deformation + plane-strain Lamé stiffness + 2D Drucker–Prager yield」と記録する。plane-stressへ変更すると圧縮剛性・安息角・体積応答も変わりうる。
- **次に確認:** material panelのtooltip/docでこの制作向け意味論を伝えるか設計レビューし、plane-stress/plane-strain解析fixtureとpile compressionを比較する。今回はUI挙動を変更していない。

### 2026-10-12 — Granular solver登録前にfloat seed計算と粒子数を確認する
- **関連:** `ArtifactCore/src/Physics/PhysicsSystem.cppm` (`createGranularMaterialGrid`)、`ArtifactCore/src/Physics/MpmSolver2D.cppm` (`addParticlesGrid`)。
- **確認できた事実:** PhysicsSystemは総体積・粒子体積・質量をdoubleで検証するが、`addParticlesGrid()`はfloatで`width*height`、粒子volume、massを再計算する。float丸めやoverflowで粒子生成がno-opでも、以前は空solverをbaseline登録できた。
- **変更:** 実際のfloat式とseed centerを登録前に検証し、solverが要求粒子数を生成し、position/mass/volumeが有限かつ正の場合だけregistryへ公開する。
- **価値／懸念:** Compositionへ有効な粒子なしsolverが登録され、Layerだけが初期化成功扱いになる不整合を防ぐ。極端値と実allocator容量での動作はruntime未確認。
- **次に確認:** 有効範囲の端・float underflow/overflow近傍・grid予算超過を生成APIで確認し、失敗時に既存solverとMaterial設定が維持されることを検証する。

### 2026-10-12 — MPMの共有fixed-step時計で非有限値を既定値へ戻す
- **関連:** `ArtifactCore/src/Physics/PhysicsSystem.cppm` (`setPhysicsTimelineSettings`、`advanceGranularMaterialSolvers`)。
- **確認できた事実:** `std::clamp(NaN, min, max)`はNaNを既定範囲へ戻さない。Granular world clockは`accumulator >= fixedDt`で進むためfixedDtがNaNだとstepが一度も実行されない。
- **変更:** 非有限fixed stepだけ1/60秒へ置き換えてから従来の範囲へclampする。
- **価値／懸念:** 不正な設定値で物理時間が静止する状態を防ぐ。呼び出し側が意図してNaNを渡す設計は見つかっておらず、fallback経路はruntime未確認。
- **次に確認:** PhysicsTimelineSettingsの保存・供給経路で非有限値が作られる可能性を確認し、CPU MPM playback/seekでは一定のfixed-step sequenceが維持されるか検証する。

### 2026-10-12 — Granular MPMのpositive SVDはスケーリングして小特異値を復元する
- **関連:** `ArtifactCore/src/Physics/MpmSolver2D.cppm` (`decomposePositiveMpm2D`)。
- **確認できた事実:** 2×2 Gram行列をfloatで直接二乗すると大きな有限変形勾配でoverflowし、`mean - radius`で小さい固有値を求めるとほぼ等方な伸長で精度を失う。さらに元スケールの`det(F)`を先に評価すると、Gram行列より前にoverflowしうる。
- **変更:** 行列をscaled Frobenius normで正規化してprincipal eigenvalueを求め、scaled determinantと最大特異値から最小特異値を復元する。正値性判定も元スケールdeterminantを使わない。
- **価値／懸念:** 有限値を保てる大伸長とほぼ等方状態でSVDが不必要に失敗する経路を減らす。float determinant自体がoverflowする状態や極端に悪条件な圧縮は依然として無効判定となりうる。
- **次に確認:** 最大・最小特異値、回転・せん断、近等方伸長の解析fixtureを作り、`Fe`再構成とHencky projectionへの誤差伝播を検証する。build/test/runtimeは未実施。

### 2026-10-12 — Granular collider摩擦は接線へ分離しpolygon法線を外向きにする
- **関連:** `ArtifactCore/src/Physics/MpmSolver2D.cppm` (`resolveColliders`、`resolvePolygonCollider`)。
- **確認できた事実:** Circle/polygon colliderは反発処理後に速度ベクトル全体を`1-friction`倍しており、摩擦が法線反発にも作用していた。またpolygon内点から最寄り境界へのベクトルは内向きなので、そのままでは粒子をsolid collider外へ出す応答法線として逆向きになる。
- **変更:** Granularモデルでは法線外向き速度をrestitutionで応答させ、接線成分だけ摩擦減衰する。polygonの応答法線はGranular時に外向きへ反転した。既存CorotatedVonMises経路は元の式を維持する。
- **価値／懸念:** Circle/Polygon colliderで砂の貫通修正と反発を法線方向に分離し、接線摩擦を独立させる。polygon角・粒子中心が境界上にある場合の法線安定性、複数collider接触順序は未検証。
- **次に確認:** circle/polygonに対するinside/outsideからの粒子移動、法線／接線速度と反発係数の解析fixtureを追加し、弾性MPM既存fixtureに差がないことを確かめる。

### 2026-10-12 — Granular MPMのwall frictionは法線速度を変えず接線だけに効かせる
- **関連:** `ArtifactCore/src/Physics/MpmSolver2D.cppm` (`applyBoundaryConditions`)。
- **確認できた事実:** boundary条件は各壁で`boundaryFriction_`を法線速度にも適用していた。granular砂では壁から内側へ動く速度が一部の壁で増幅／減衰され、wall frictionの値が法線運動まで変えていた。
- **変更:** Granular Drucker–Prager時は壁へ向かう法線速度だけ0にし、壁から離れる法線速度は保持する。`boundaryFriction_`は接線成分にだけ適用する。既存CorotatedVonMises経路は従来式を維持する。
- **価値／懸念:** 床・側壁からの反発／粘着ではなく、非貫通と接線摩擦として扱える。4壁それぞれの inward/outward velocityと摩擦の符号確認、砂pileの安息角への影響は未検証。
- **次に確認:** 4壁の正負法線速度と接線速度を固定条件で検査し、既存CorotatedVonMisesの境界fixtureに差がないことを確認する。

### 2026-10-12 — Render Queue MPM worker cloneは並列描画開始前に生成される
- **関連:** `Artifact/src/Render/ArtifactRenderQueueService.cppm` (`cloneCompositionSnapshot`、CPU MFR worker setup、GPU worker setup)、`ArtifactCore/src/Physics/PhysicsSystem.cppm`。
- **確認できた事実:** CPU MFR composition cloneとmulti-GPU/XPU fallback cloneはworker threadを起動する前のsetup loopで逐次生成される。clone時のMPM solver登録・初期checkpoint生成は、このsetup phaseで完了する。`ArtifactAbstractComposition::fromJson()`はLayer attach後に保存current frameへ`goToFrame()`し、Render Queueも各target frameで`goToFrame()`してGranular replayへ入る。worker実行時は各worker固有のComposition/runtime physics worldを使い、同一worldを複数workerへ共有しない。**追加変更:** solver registrationでworld clockとper-layer snapshot map nodeを先行作成し、worker snapshot captureは共有外側registryを`find()`するだけにした。1 Layerのdetachでworld共通cacheを消す場合も、残るsolverのlayer map nodeをdetach側で再作成する。worker advance/restoreでclock entry missing時にcontainerを構造変更せず、失敗を返す。
- **確認できた寿命:** CPU MFR clonesはRender Queue所有の`workerCompositions`に、GPU clonesはGPU worker ownerに保持され、teardownは全render workerを`join()`した後に進むため、通常dispatch中に終了workerがComposition destructorからworld registryを消す経路はない。
- **価値／懸念:** clone生成時のregistry構造変更と、worker終了時のworld teardownが並行する経路は静的には排除できた。workerは異なるworldごとのsnapshot mapとclockを同時更新するため、custom registryと異worldの並行アクセスの全スレッド安全性は静的追跡だけでは証明できない。
- **未検証:** UI側Compositionの同時solver登録／破棄やworker間のThreadSanitizer挙動は未確認。**次に確認:** 許可されたThreadSanitizer/runtime stressで複数MPM workerのcache capture/trimとUI編集並行を検証する。

### 2026-10-12 — MPM seekがbaseline復元できない場合に診断を残す
- **関連:** `Artifact/src/Composition/ArtifactAbstractComposition.cppm` (`replayGranularMaterialToFrame`)、`ArtifactCore/src/Physics/PhysicsSystem.cppm` (`resetGranularMaterialToBaseline`)。
- **確認できた事実:** checkpointがない場合のbaseline replayは正常なcache miss経路だが、baseline snapshot自体が欠落・不整合ならreplayは`false`を返す。`goToFrame()`と`setFramePosition()`はこの戻り値を無視するため、砂状態が更新されずにframeだけ移動しても記録がなかった。
- **変更:** baseline復元失敗時だけworld IDとtarget frameを`[Composition][MPM]` warningへ出す。通常のcache missではログを出さない。
- **価値／懸念:** solver未登録・baseline破損を静止した砂と区別できる。利用者向けのUI error surfaceやrecover操作は未実装。
- **次に確認:** solver登録失敗とbaseline欠落を故意に作った開発fixtureで、通常cache missが無警告、復元失敗が一度のwarningになることを確認する。

### 2026-10-12 — MPM密度meshは標本中心を保持し外周cellで全域を覆う
- **関連:** `ArtifactCore/src/Physics/MpmSolver2D.cppm` (`writeDensityField`)、`Artifact/src/Layer/ArtifactMpmSand2DLayer.cppm`。
- **確認できた事実:** `writeDensityField()` は128×96個のpixel center標本を出力する。単純な127×95 quad化ではLayer外周の半pixelと最右・最下cellを描けず、中心標本から境界への4点平均は孤立した低密度peakのalphaをしきい値未満に落とし得る。
- **変更:** 130×98頂点の格子で129×97 cellを作り、各field標本を内部頂点として保持し、その外側へ半pixel幅のcellを追加した。外周値は最端標本をclampして使う。最大mesh容量は75,078頂点。
- **価値／懸念:** GPU surfaceがLayer全域を覆い、fieldの中心標本値を失わない。外周を複製する境界条件による輪郭品質、GPU/CPU parityは未検証。
- **次に確認:** field端・単一粒子・Layer全域の密度分布を固定fixtureで可視化し、render outputでmesh外周とCPU thumbnailの境界を比較する。

### 2026-10-12 — CPU Render QueueのMPM thumbnail経路
- **関連:** `Artifact/src/Render/ArtifactRenderQueueService.cppm` (`renderLayerSurface`、`renderSingleFrameComposition`)、`Artifact/src/Widgets/Render/ArtifactCompositionRenderController.cppm` (`drawLayerForCompositionView`)、`Artifact/src/Layer/ArtifactMpmSand2DLayer.cppm`。
- **確認できた事実:** CPU Render Queueの `renderLayerSurface()` にMPM固有分岐はないが、Layerの `getThumbnail()` へfallbackする。後続実装で `ArtifactMpmSand2DLayer::getThumbnail()` がsolver密度fieldからbounded `ImageF32x4_RGBA` surfaceを作り、既存のQImage境界で返すため、CPU出力もMPM simulation像を含む。
- **残る未検証:** Render Queueの該当frame solver state、mask/effect適用、GPUとの画素・alpha parity、frameごとの変換コストはruntime未確認。
- **次に確認:** CPU/GPU Render Queueで同一scene/frameを比較し、mask/blend、透明edge、MFR cloneでの状態一致を検証する。

### 2026-10-12 — MPM密度描画は現在粒子体積を反映する
- **関連:** `ArtifactCore/src/Physics/MpmSolver2D.cppm` (`writeDensityField`)、`Artifact/src/Layer/ArtifactMpmSand2DLayer.cppm`。
- **確認できた事実:** 密度場writerは各粒子の初期体積 `volume0` と固定支持半径だけを使っていたため、MPMの変形勾配 `F` による現在体積の変化がsurface kernelへ反映されなかった。
- **変更:** `volume0 * det(F)` を描画体積に使い、支持半径を `sqrt(det(F))` に比例させた。出力fieldの最小pixel幅を基準に半径を各軸8 pixel以内へclampし、既存のbounded field契約を保つ。
- **価値／懸念:** 圧縮・膨張で粒子面積が追従し、変形後も密度fieldがmaterial stateから作られる。見た目のしきい値、体積変化時の輪郭品質、field resolutionによる性能は未検証。
- **次に確認:** 許可されたruntime検証時に、単粒子の体積変化と圧縮pileでdensity maskの面積・重なりが期待どおりに変わることをcapture比較する。

### 2026-10-12 — MPM surface migration must remove stale particle-buffer references
- **関連:** `Artifact/include/Layer/ArtifactMpmSand2DLayer.ixx`、`Artifact/src/Layer/ArtifactMpmSand2DLayer.cppm`。
- **確認できた事実:** Layer headerに `renderParticles_` は存在しない一方、property更新とJSON復元に古い `renderParticles_.reserve(...)` 呼び出しが残っていた。新しいsurface mesh経路の移行後に残った未定義メンバー参照だった。
- **変更:** 2箇所の未使用reserve呼び出しを削除し、同名参照が残っていないことを検索で確認した。
- **価値／懸念:** コンパイルを阻む古いParticle payload残骸を除去した。ビルドによる確認は未実施。
- **次に確認:** 次の許可済みbuildでMpmSand2D module pairのcompile/linkを確認する。

### 2026-10-12 — MPM surface buffer acquisition must preserve undersized recycled storage
- **関連:** `Artifact/include/Render/RenderCommandBuffer.ixx`、`Artifact/src/Render/PrimitiveRenderer2D.cppm`。
- **確認できた事実:** surface buffer poolはsubmit/reset後のpacket vertex配列を再利用する。容量不足のbufferをacquire時に単純に破棄すると、将来要求が増えたときに確保済み容量を失う。容量が揃っている現行Layerでは通常発生しないが、取得API単体の失敗経路として存在した。
- **変更:** 取得できないbufferを同じbounded poolへ戻す処理を追加した。
- **価値／懸念:** 一時的な容量不一致で再確保を誘発せず、bounded reuse契約を維持する。差分は静的に確認したが、packet resetを含む実行時の再利用やallocator計測は未検証。
- **次に確認:** build/runtime確認を許可された段階で、複数surface packetのsubmit後に容量が保持されることとpool hit率を計測する。

### 2026-10-11 — Paired module implementation must not redeclare export module
- **関連:** `Artifact/CMakeLists.txt`、`Artifact/cmake/ArtifactSources.cmake`、`Artifact/src/Layer/ArtifactMpmSand2DLayer.cppm`。
- **確認できた事実:** Artifact registers public `.ixx` providers in `APP_MODULES` and paired `.cppm` implementations in `APP_IMPL`. The MpmSand2D implementation redeclared `export module` and Ninja dyndep rejected it as an export provider outside a `CXX_MODULES` file set. Changing the implementation declaration to `module Artifact.Layer.MpmSand2D;` let dyndep proceed. The subsequent build stopped earlier in `ArtifactCore/src/Physics/MpmSolver2D.cppm:384` with MSVC C2397, before compiler verification of the MpmSand2D implementation.
- **仮説／未検証:** The implementation declaration now matches sibling paired `.cppm` files, but compile/link correctness remains unverified until the independent ArtifactCore narrowing error is resolved.
- **価値／懸念:** This separates module registration failure from the later Core compile failure; avoid changing the Core child repository without an explicit request.
- **次に確認:** After the ArtifactCore build blocker is resolved, rebuild `ArtifactUiTest` and confirm both the MpmSand2D module pair and Cloner fixture compile.

### 2026-10-11 — MPM砂は既存の弾性材質MPMとは構成則を分ける
- **関連:** `ArtifactCore/include/Physics/MpmSolver2D.ixx`、`ArtifactCore/src/Physics/MpmSolver2D.cppm`、`ArtifactCore/src/Physics/PhysicsSystem.cppm`、`Artifact/src/Layer/ArtifactSandSim2DLayer.cppm`。
- **確認できた事実:** 現行MPMはfixed-corotated応力と正規化von Mises型の塑性投影を持ち、presetはFlesh/Foam/HardRubber/Wood。既存SandSim2Dはセルオートマトンで、別Layerが描画・保存する。PhysicsSystemはLayerID単位のMPM solverとsnapshotを管理し、Shapeの材質シミュレーションから使われている。
- **仮説／未検証:** 既存MPMへ材料モデル選択を追加し、砂だけ圧力依存摩擦降伏・dilatancy・引張制限を使えば、solverの重複を抑えて粒状流動を追加できる可能性がある。Mohr–Coulomb系の2D応力条件と粒子体積更新を固定した実装は未検証。
- **価値／懸念:** 顆粒塑性を既存von Mises projectionのパラメータ調整だけで近似すると、安息角や圧密を制御できず「砂色の弾性体」になる懸念がある。GPU経路はCPU塑性tailを持つため、同モデルのGPU対応とperformanceは別途立証が必要。
- **次に確認:** `docs/planned/MILESTONE_MPM_GRANULAR_SAND_2026-10-11.md` に記録した式、単位、受入シナリオをレビューし、採用する顆粒降伏投影とLayer設定所有者を確定してから実装へ進む。

### 2026-10-11 — MPM砂モデルは弾性応力・塑性投影・GPU ABIを一体で設計
- **関連:** `ArtifactCore/src/Physics/MpmSolver2D.cppm`、`ArtifactCore/include/Physics/MpmSolver2D.ixx`、`ArtifactCore/include/Graphics/MpmCompute.ixx`、`ArtifactCore/src/Graphics/MpmCompute.cppm`、`ArtifactCore/src/Physics/PhysicsSystem.cppm`。
- **確認できた事実:** CPU `firstPiolaKirchhoff()` はfixed-corotated応力、`applyPlasticity()` はvon Mises型の対数特異値投影。GPU particle ABIはF/Fp/C/塑性履歴を転送する。`MpmSnapshot2D` はparticle stateとaccumulator等を保存するが、材料係数・モデル選択・grid設定は含まない。
- **仮説／未検証:** 砂用Drucker–Pragerへ切り替えるなら、yield projectionだけでなく弾性応力と応力測度・2D応力条件を同時に揃える必要がある。Hencky弾性が候補だが、現在の2D Fp更新との組み合わせは未検証。
- **価値／懸念:** CPU用の砂モデル追加時にGPU solverが旧構成則のまま計算する誤ルートを防ぎ、project設定とper-frame snapshotの所有境界を分けられる。モデル判別漏れはGPU/CPU state divergenceを起こす。
- **次に確認:** 材料モデルごとの弾性応力・return mapping・particle historyを定義し、Layer JSON、MpmSnapshot2D、GPU upload/readback ABIのどこが各値を所有するか設計レビューする。

### 2026-10-11 — MPM砂のComposition表示はShape格子変形と分離する
- **関連:** `ArtifactCore/src/Physics/PhysicsSystem.cppm` (`buildMpmParticleRenderData`)、`Artifact/src/Layer/ArtifactShapeLayer.cppm` (`drawMaterialGrid`)、`Artifact/src/Layer/ArtifactSandSim2DLayer.cppm`、`Artifact/src/Composition/ArtifactAbstractComposition.cppm`。
- **確認できた事実:** MPM render-data bridgeは手動呼び出し向けでComposition自動描画に接続されていない。ShapeのMPM描画はparticleGridColumns/Rowsで隣接粒子をquad化する。SandSim2DLayerはdraw呼出しのたびCPU/GPU simulationをstepし、GPU側readbackを行う。
- **仮説／未検証:** 砂は任意大変形で初期格子順のquadが折り重なり、描画の隙間・セル反転を起こし得る。独立Sand Layerと粒子sprite描画なら、既存Particle Layerのemitter意味論やShape gridを使わずMPM点配置を表現できる。描画境界を専用render packetにするコストは未計測。
- **価値／懸念:** Simulation tickをLayer drawから切り離し、Composition playback/snapshot seek契約に接続できる。点spriteだけでは粒子間の空隙が目立つ場合、bounded density surface化が追加で必要。
- **次に確認:** Composition render収集におけるlayer raster sourceと直接primitive pathを調べ、MpmSandLayerの粒子drawをCPU/GPU/Render Queueで共有する最小APIを決める。bounds expansionとopacity/mask/exportの統合も確認する。

### 2026-10-11 — MPM playbackは接続済みだがRender Queue状態隔離は未確認
- **関連:** `Artifact/src/Playback/ArtifactPlaybackEngine.cppm`、`Artifact/src/Composition/ArtifactAbstractComposition.cppm` (`setFramePosition`/`goToFrame`)、`ArtifactCore/src/Physics/PhysicsSystem.cppm`、`Artifact/src/Render/ArtifactRenderQueueService.cppm`。
- **確認できた事実:** PlaybackEngineのforward renderはComposition `setFramePosition()` を呼び、内部1〜8 frame進行の既定経路で `advancePhysicsFrame()` からMPMを更新する。従来Shape用`materialSolvers_`はglobal registryである。一方、砂solver用registryにComposition instance固有runtime ID、worldごとのfixed-step accumulator、snapshot cache/capture/restoreを追加し、CompositionからそのIDを渡す経路が入った。JSON cloneも新しいComposition Implから別runtime IDを得る。MPM cache miss後のbaseline replayは未実装。Component Simulation bakeは別にexport/importされる。
- **仮説／未検証:** Render Queue cloneのlayer IDsがglobal PhysicsSystem registryへ同じsolverを引くのか、solverを再生成するのか、各workerのMPM状態をどう隔離するかは未特定。offline renderでMPMが正しい状態になるとは現証拠から判定できない。
- **価値／懸念:** 砂solverの複数Composition間step/snapshot共有はruntime world単位で避ける基盤が入った。ただし通常Shape用registryは従来のglobal scopeであり、砂専用Layer・clone lifecycle・seek miss時の再simulationは未接続。
- **次に確認:** 専用Layerのattach/detachでruntime world付きsolverを生成・破棄し、baseline snapshotとcollider/inputを用いるcache miss replayをComposition frame evaluationへ接続する。worker cloneごとの再現性と同時Composition playbackを検証する。

### 2026-10-11 — Layer JSON復元がPhysicsSystem MPMを仮IDへ登録する疑い
- **関連:** `Artifact/src/Layer/ArtifactShapeLayer.cppm` (`fromJson`)、`Artifact/src/Layer/ArtifactAbstractLayerPersistence.cppm` (`fromJsonProperties`)、`Artifact/src/Layer/ArtifactLayerFactory.cppm` (`restoreSerializedId`)、`Artifact/src/Layer/ArtifactAbstractLayer.cppm` (`setId`)、`ArtifactCore/src/Physics/PhysicsSystem.cppm`。
- **確認できた事実:** Shape `fromJson()` はfactoryへ返す前にbase `fromJsonProperties()` を呼ぶ。base復元は `materialPhysicsEnabled` がtrueなら `enableMaterialPhysics()` を呼び、layerの現IDでPhysicsSystem material solverを登録する。Layer factoryが返却直前にserialized IDを `setId()` するが、このsetterはPhysicsSystem registryを移行しない。`getMaterialSolver()` はLayerIDでregistryを検索する。
- **仮説／未検証:** MPM enabled Shapeの通常project loadやJSON clone後、solverが復元Layerのserialized IDではなくconstructor生成IDに登録され、solverを取得できないか孤児registry entryが残る可能性がある。Artifact runtimeでのproject load結果は未確認。
- **価値／懸念:** MPM砂のLayer/Render Queue integrationより前にLayer ID restoration contractが必要。MFR cloneは同じserialized IDを再利用するため、仮IDsolverが残るとsim state lost/stale registryを招く懸念がある。
- **次に確認:** Artifactサブモジュールの変更許可後、serialized IDを確定してからphysics registrationする順序へ直す。通常load、Render Queue clone、Undo/Redoでregistry keyとsolver lifetimeを検証する。

### 2026-10-11 — Debug executable の PDB 無効化フラグ
- **関連:** `Artifact/CMakeLists.txt` の Debug executable linker option。
- **確認できた事実:** `/PDB:NONE` は PDB 出力名 `NONE` と解釈され、build tree に約6 GBのファイルを作り、再リンクで `LNK1201` になった。MSVC の `/DEBUG:NONE` が PDB を生成しない指定であることを公式 linker docs で確認し、置換した。
- **仮説／未検証:** Visual Studio 2026 と全ての supported Debug configurations での再リンク確認は未実施。今回の `ArtifactUiTest` link は通過した。
- **価値／懸念:** Debug executable link が意図どおり PDB を省略し、`NONE` という巨大な出力ファイルを作らない。
- **次に確認:** clean Debug build 後に PDB が作られず、link 成功することを確認する。

### 2026-10-11 — Cloner GPU draw parity は通常 Viewer の raster source で成立

- **関連:** `Artifact/src/Render/ArtifactCompositionViewDrawing.cppm`、`Artifact/src/Widgets/Render/ArtifactCompositionRenderController.cppm`、`tools/capture_ui_test.py`、`docs/planned/MILESTONE_CLONER_UNIFICATION_2026-08-23.md`。
- **確認できた事実:** Composition Viewer の raster fixture は、GPU有効時に3×3で9個のplacement-backed sprite drawを記録し、CPU fallback 0を報告した。D3D12上のCPU/GPU renderer readbackは `ja` / `en` / `zh` / `zh-TW` 全ロケールで完全一致し、各946×1085画像に39200個のfixture色画素があった。キャプチャスクリプトは `--cloner-backend cpu|gpu` と診断route assertionを提供する。
- **追加確認 (2026-10-11):** GPU適格streamは `drawWithGpuEligibleClonerGridIndexed` 内で候補ごとの `QMatrix4x4` を生成し、Composition View callback側の `gpuGridPlacementMatchesCpuInstance` が同じdescriptorから期待行列を再構成して全16要素を比較する。加えてsubmitter失敗用fallback行列も各clone packetに保存する。これは挙動の事実であり、比較を消す場合はcomposition fieldによる行列／weight変更とsubmit失敗時のCPU fallbackを保つ別の証明経路が必要。
- **追加確認 (2026-10-11):** `capture_ui_test.py` は `--cloner-rotation-step` を受けても fixture がその値を適用するのはRadialだけだった。誤った Linear + rotation-step 組合せが成功したように見えないよう、fixture起動前に Radial 以外の指定を拒否する検証を追加した。実行時のCPU fallback確認にはまだ非対応fixture条件の入力経路が必要。
- **追加確認 (2026-10-11):** Linear `opacityDecay=0.2` はGPU適格性から除外される既存fixture条件だったため、capture helperに `--cloner-expect-cpu-fallback` を追加して実行確認した。GPU準備済みだがplacement未処理・GPU draw 0件となり、`ja` / `en` / `zh` / `zh-TW` のCPU/GPU renderer readbackは同一frame 2で全画素一致、weight traceも一致した。arena overflow・GPU resource failureは別条件で未検証。
- **追加確認 (2026-10-11):** Radial `opacityDecay=1.0` を既存exeで試すと、CPU weight trace `1,1,0,0,0` に対してGPU stream traceが `1,1,0,-1,-2` となり、GPU 3 draw＋CPU fallback 2件へ分岐した。`drawWithGpuEligibleClonerGridIndexed` の共通candidate callbackでweightを `std::clamp(...,0,1)` し、CPU `appendCloneInstance` と意味論をそろえた。対象 `.ixx` module objectのコンパイルは通過したが、既存exeは修正前のためruntime再確認は未実施。
- **仮説／未検証:** GPU placementを読むdrawでもcloneごとのCPU順序走査とfallback用CPU transform生成を残すため、CPU準備時間が支配的な大規模Gridでは速度向上が限定的な可能性がある。GPU runtime性能、Vulkan runtime parity、容量上限／resource failure時の実機fallbackは未検証。
- **価値／懸念:** 描画段階は実画像を使ったCPU parityまで到達した。透明合成順を保持しながら複数cloneを1 drawへまとめられるかは、順序・blend semanticsを測定してから別段階で判断する。
- **次に確認:** GPU timeとCPU準備時間を分離計測し、固定容量上限近傍とGPU resource failureのfallback挙動を確認してからGPU-3の対象拡張を選ぶ。

### 2026-10-11 — Linear Cloner の平行移動配置をGPU経路へ拡張

- **関連:** `Artifact/include/Layer/ArtifactCloneEffectSupport.ixx`、`Artifact/include/Render/ArtifactIRenderer.ixx`、`Artifact/src/Render/ArtifactIRenderer.cppm`、`Artifact/src/Render/ArtifactCompositionViewDrawing.cppm`、`tools/capture_ui_test.py`。
- **確認できた事実:** Linear generatorはclone index 0から `offsetXYZ * (index + 1)` で配置する。GPU placement recordに同じ順序のtranslationを生成し、HLSL vertex shaderがslotを読み取る経路をGrid/Matrixと共用した。対応対象は単一generator、transform stackなし、回転なし、不透明度減衰なし、Random legacy mode以外。D3D12上のCPU/GPU出力は4ロケールで全画素一致し、Linear fixture色17,396画素とGPU draw 4件、fallback 0件を確認した。Gridも同じ再ビルド後、4ロケール全画素一致・fixture色39,200画素・GPU draw 9件を再確認した。
- **仮説／未検証:** singleton drawを保つため透明合成順とCPU fallbackを維持できるが、大きいcountでCPU準備時間が支配的かは未測定。回転／減衰／Random modeのruntime fallback、容量超過・resource failure fallback、Vulkan runtime parityは未確認。
- **価値／懸念:** LinearとGrid/Matrixが同じGPU output arena、record ABI、sprite vertex pathを利用する段階になった。rows/depthのゼロ組合せをLinear modeとしてHLSLへ伝えるため、そのsentinel契約を別distribution追加時に曖昧にしないこと。
- **次に確認:** rows/depth sentinelを明示的なdistribution enumへ置き換える設計を検討し、Linearの不対応設定とarena容量超過をruntime fixtureでCPU parity確認してからRadialを追加する。

### 2026-10-11 — Radial Cloner GPU placement は回転とローカル矩形原点を保持
- **関連:** `Artifact/src/Layer/ArtifactCloneLayer.cppm` のRadial評価、`Artifact/include/Layer/ArtifactCloneEffectSupport.ixx` のgenerator列挙、`Artifact/include/Render/ArtifactIRenderer.ixx` のplacement record、`Artifact/src/Render/ShaderManager.cppm` のCloner sprite vertex shader。
- **確認できた事実:** `layerGenerators()` component Radialの配置は原点周りに半径分移動し、cloneを `angle` 度回転する。GPU placement recordの予約領域をrotation degreesに使い、HLSL sprite pathへlocal rect originを回転順序に含めた。旧式Cloner内部のradialCount/radius/anglesが正規generator descriptorへ反映されていなかったため、descriptor変換時にcanonical内部設定をコピーした。80×48矩形・4 clone fixtureでD3D12の `ja` / `en` / `zh` / `zh-TW` 全てCPU/GPU完全一致（946×1085、fixture色16,599画素）、GPU draw 5件、fallback 0件。GridとLinearもjaで完全一致。旧式ClonerGenerator effectのbounds-center契約は同経路とは別である。
- **価値／懸念:** RadialをCPU fallbackからGPUへ段階的に広げ、設定値もdescriptorへ正しく渡るようになった。Radial非ゼロrotationStepもD3D12 parityを確認済み。opacityDecay／modifier、Vulkan runtime、性能は未検証。
- **次に確認:** opacity decay／modifier等はそれぞれCPU parity fixtureを追加してからGPU対象を選び、Vulkan runtime parityも確認する。

### 2026-10-11 — Grid/Matrix Cloner 減衰weightをclone indexと共有
- **関連:** `Artifact/include/Layer/ArtifactCloneEffectSupport.ixx`、`Artifact/src/Render/ArtifactCompositionViewDrawing.cppm`、Cloner UI fixture。
- **確認できた事実:** 旧Cloner Gridはflattened index `z*(rows*columns)+y*columns+x` を `1-opacityDecay*index` に適用していたが、canonical component generatorはweightを常に1としていた。Grid/Matrix descriptor CPU instance生成とGPU stream callbackに同じ0..1 clamp式を加え、GPU照合側でもweight式を確認するようにした。D3D12 Grid decay=0.2は `en` / `ja` / `zh` / `zh-TW` 全てCPU/GPU readback全画素一致（946×1085、GPU 9 draw／fallback 0）。trace列は `1,1,0.8,0.6,0.4,0,0,0,0`。enのdecay=0比で26,293画素が変化した。
- **確認できた事実:** Matrix専用descriptor fixtureはdecay=0.2で `ja` / `en` / `zh` / `zh-TW` 全て全画素一致、GPU 9 draw／fallback 0。D3D12とVulkanの両方でMatrix CPU/GPU readbackがframe 2、全画素一致した。さらにVulkan Grid decay=0.2、Radial rotationStep=15／decay=0.2も同じ4ロケールでframe 2のCPU/GPU readbackが全画素一致し、GridはGPU 9 draw、Radialは5 draw、fallback 0。Matrix weight traceは `1,1,0.8,0.6,0.4,0,0,0,0`。decay=0と比べCPU/GPU双方で26,293画素変化し、decay=0同士も全画素一致した。decay=1の英語fixtureはweight `1,1,0,0,0,0,0,0,0` でCPU/GPU全画素一致した。
- **仮説／未検証:** arena容量近傍、性能は未検証。
- **価値／懸念:** legacy Grid weight意味論を正規generator CPU/GPU描画へそろえ、opacity shader修正を使ってGridも段階拡張した。
- **次に確認:** 出力arena容量近傍と性能を確認する。

### 2026-10-11 — Cloner GPU arenaの上限はbase slot込み
- **関連:** `Artifact/src/Render/ArtifactIRenderer.cppm` (`dispatchClonerGridPlacement`)、`Artifact/src/Render/ArtifactCompositionViewDrawing.cppm`（GPU placement observer／sprite fallback）。
- **確認できた事実:** arenaは65,536 recordsで、各dispatchは候補数+1（base slot）を予約する。batch cursorが0の場合に受け入れ可能な候補数は最大65,535。残容量不足はparameter mapping／compute dispatch前にfalseで返り、range未取得時は通常のCPU行列sprite drawへ進む。cursorはsubmitter batch送信後に0へ戻る。
- **仮説／未検証:** overflow時のCPU fallbackはコード経路上boundedだが、65,536候補を実際に超えるfixtureでfallback count・画像 parity・フレーム時間を確認していない。
- **価値／懸念:** 65,536を「clone数上限」と誤読せず、base recordとbatch内の複数dispatchが共有するcursorを含めて上限を説明できる。大量clone fallbackはCPU transform loopとsprite recordingが続くため、GPU上限拒否だけでは入力 workload を安価にできない。
- **次に確認:** 安全なoverflow fixtureで1 batch超過時のCPU fallbackと画像を検証し、同一clone数でCPU準備時間／GPU時間を計測する。

### 2026-10-11 — transformed-sprite vertex shader がCPU opacityを落としていた
- **関連:** `Artifact/src/Render/ShaderManager.cppm`、`Artifact/src/Render/PrimitiveRenderer2D.cppm`、`Artifact/src/Render/DiligentImmediateSubmitter.cppm`、Cloner UI fixture。
- **確認できた事実:** fixture CPU/GPU両経路はclone weight `1.000, 1.000, 0.800, 0.600, 0.400` を作りdraw callbackまで渡していた。通常 transformed-sprite VSは入力頂点カラーattributeを宣言せず、出力を常に白・alpha=1に固定していた。GPU Cloner VSはOpacity cbufferを使うため減衰し、CPU標準sprite経路だけが減衰を失っていた。Artifact側の標準VSを頂点カラー入力へ対応させ、入力layoutへcolor attributeを追加し、Cloner専用PSOは旧2 attribute layoutを明示した。変更後、D3D12 radial `rotationStep=15` でdecay=0.2と0の両CPU/GPU readbackが全画素一致し、decay画像間では11,609画素が変化。GPUは各5 draw／fallback 0。
- **仮説／未検証:** SpriteTransformを使う他の描画操作もopacityが正常化するはずだが、専用UI / matte / Puppet deformationの全変種は未検証。
- **価値／懸念:** GPU Cloner固有のopacity差ではなく、CPU fallback共有vertex shaderの頂点color欠落を修正した。Diligent backend-neutral shader/PSO境界内で、ArtifactCore child repoは変更していない。
- **次に確認:** transformed-spriteの通常opacity、mask、textured triangleで回帰を確認し、Radial decayのlocale / Vulkan runtime coverageを広げる。

### 2026-10-11 — GPU Radial rotationStep parity
- **関連:** `Artifact/include/Render/ArtifactIRenderer.ixx`、`Artifact/src/Render/ArtifactIRenderer.cppm`、`Artifact/include/Layer/ArtifactCloneEffectSupport.ixx`、`Artifact/src/Render/ArtifactCompositionViewDrawing.cppm`、`tools/capture_ui_test.py`。
- **確認できた事実:** CPU Radialの配置角は `startAngle + angleStep * index`、描画回転は `placementAngle + rotationStep * index`。GPU placement computeへstepを追加した後、GPU向けCPU stream生成側が回転増分を変換行列に加えていなかったため、matrix照合がclone 1–3をCPU fallbackへ送っていた。GPU stream変換もCPU Component生成と一致させると、D3D12 fixtureでRadial `rotationStep=15` が5 GPU draw／0 fallbackとなり、CPU/GPU readbackが946×1085の全画素で一致した。Grid（9 draw）、Linear（4 draw）、Radial step=0（5 draw）も同buildで全画素一致。Radial step=15、Grid、Linearは `ja` / `en` / `zh` / `zh-TW` すべてで比較した。初回の15度stepキャプチャはframe 1の読み戻しで黒だったが、frame 2で再取得したCPU/GPU画像にはクローンが描画され完全一致した。
- **価値／懸念:** radial offset angle と sprite rotation を別々に運び、GPU配列生成とCPU照合を同じ意味論にそろえた。parameter blockは固定長64 bytesに拡張し、readbackなしのdispatchを保つ。Vulkan runtime、非常に大きいstep値の数値境界、性能は未検証。
- **次に確認:** Vulkan backendでもRadial step parityを確認し、opacity decayやmodifierは個別のCPU/GPU parity fixtureを作ってから段階的に追加する。

### 2026-10-11 — Cloner fallback診断はCPU描画完了後に確定する
- **関連:** `Artifact/src/Widgets/Render/ArtifactCompositionRenderController.cppm`、`tools/capture_ui_test.py`、Cloner GPU-2 fixture。
- **確認できた事実:** GPU placement eligibilityがfalseの場合、通常の `drawWithClonerEffect()` がcloneをCPU描画するが、先行して出力するroute診断の `cpuFallbackDraws` はGPU callback内の失敗だけを数えていた。そのため全体CPU fallbackを0件と記録し得た。通常CPU描画callbackで件数を加算し、描画後にroute診断を最終値で保存するよう変更した。対象controller objectはビルド済み。
- **確認できた事実:** 既存の古いexeでfallback fixtureを走らせると `cpuFallbackDraws=0` が出て、新しいassertionはこれを不合格にした。同frame 2の946×1085 renderer readbackはCPU backend画像と全画素一致し、weight traceは `1.000,1.000,0.800,0.600` だった。これはfallback描画の既存動作確認であり、新しいcounter修正自体のruntime確認ではない。
- **仮説／未検証:** 更新済みexeではweight traceと同数の4 callbackを数える見込みだが、MPMソースを変更せずリンク済みexeを更新する経路は未確認。
- **価値／懸念:** GPU placement未対応・不適格時のCPU経路をGPU部分fallbackと区別して観測できる。diagnostic有効時のファイル書込はfixture専用で、通常frameには適用されない。
- **次に確認:** 更新済み `ArtifactUiTest.exe` でRandom legacy fallback fixtureを実行し、GPU draw 0、CPU fallback draw >0、weight traceとreadback parityを確認する。現在の実行ファイルは古い。

### 2026-10-11 — Linear Cloner rotationとopacity decayは既存placement shaderで表現できる
- **関連:** `Artifact/include/Layer/ArtifactCloneEffectSupport.ixx`、`Artifact/src/Render/ArtifactCompositionViewDrawing.cppm`、`Artifact/src/Render/ArtifactIRenderer.cppm`、`Artifact/src/Render/PrimitiveRenderer2D.cppm`、Cloner UI fixture。
- **確認できた事実:** CPU Linear generatorは各cloneへ `translate(offset * (index + 1))` の後に `rotate(rotationStep * (index + 1))` を適用する。GPU placement recordは回転角を保持でき、shared vertex shaderがlocal verticesを回転してoffsetへ足す。Linearのopacity decayはGPU placementから返すclone indexに基づき、既存 per-draw opacityへ `clamp(1 - opacityDecay * index, 0, 1)` を渡す。GPU eligibility、CPU transform/weight照合、range rotation-aware判定、fixture入力とweight assertionを拡張した。既存exeのCPU fixtureではLinear count 3／decay 0.2のtrace `1.000,1.000,0.800,0.600` とdecay 1.0のtrace `1.000,1.000,0.000,0.000` がassertionを通った。静的再読でLinear branchのweight変数宣言漏れとRadial branchの重複宣言を検出し修正した。
- **2026-10-12 の確認:** 現行 `ArtifactIRenderer.cppm` から配置compute HLSLを再抽出し、Vulkan SDK `glslangValidator -D -V -S comp -e main` と `spirv-val` が両方成功した。既存 `.modmap` を用いた対象5オブジェクト（Cloner support、Composition drawing、IRenderer、AppMain、CompositionRenderController）のMSVC単独コンパイルも成功した。既存 `ArtifactUiTest.exe` の Linear CPU fixture は回転step 15／decay 0.2で `ja` / `en` / `zh` / `zh-TW` 全てcapture成功し、946×1085 readbackとweight trace `1.000,1.000,0.800,0.600` を得た。GPU fixtureはexeが `gpuReady=1` に対して `placementHandled=0, gpuDraws=0` を返してcapture assertionに失敗した。exeは2026-10-11 22:50のままなので、この失敗は最新GPU C++統合を実行した結果ではなく、最新GPU parityも検証していない。最新HLSLのDXIL compileも未実施。
- **既存GPU基礎経路の対照確認:** 同じ古いexeのGrid fixtureは D3D12 で `gpuReady=1, placementHandled=1, gpuDraws=9, cpuFallbackDraws=0`。英語CPU/GPU renderer readback（946×1085）の比較は差分画素0だった。よって前項のLinear GPU fixture失敗はGPU Grid/Matrixの既存基礎経路が全般的に動かないことを示すものではなく、exeに含まれていないLinear拡張について結論できない。
- **Radial再確認:** 同じexeで英語Radial fixture（rotationStep 15／opacityDecay 0.2）をCPU/GPU両方実行。GPU routeは5 draw／CPU fallback 0、両renderer readback 946×1085、差分画素0、両方のweight traceは `1.000,1.000,0.800,0.600,0.400`。
- **検証バイナリ更新の制約:** Ninjaの既存link graphを読むとArtifactUiTestは未生成の `ArtifactMpmSand2DLayer.cppm.obj` を入力に含み、現在は別名exeへの再linkも既存objectだけでは完了しない。Cloner関連5 objectは直接compile済み。MPMソースに触れない指定を保つ限り、Linear GPUの最新runtime parityは既存exeだけでは検証できない。
- **2026-10-12 placement compute 実機試験:** 現行HLSLを `glslangValidator` でSPIR-V化し、Vulkan descriptor bindingのみ明示した一時shaderと外部一時C++ harnessをNVIDIA RTX 4070 Tiでdispatchした。base recordを含むLinear 3候補、Radial 4候補、Grid 6候補がCPU参照式と一致し、nonzero outputOffset (2/7/15) でもrecord・rotation・slot順が一致した。readbackは検証harnessだけで行い、Artifact hot pathにはreadbackを追加していない。これは配置computeの実機検証で、Diligent統合・D3D12・sprite描画・Linear全画面 parityの証拠ではない。
- **2026-10-12 描画shader compile:** 現行 `ShaderManager.cppm` のCloner sprite vertex HLSLを抽出してVulkan SPIR-V compileと `spirv-val` に通した。一時検証用binding整理後、TransformCB / ClonerSpriteCB / placement StructuredBufferはbinding 0/1/2に分離され、32-byte placement ABIとshader interfaceの構文検証が成立した。Diligent runtime PSO/vertex layoutの実行時再生成確認ではない。
- **2026-10-12 DXIL compile:** 配置 compute と現行Cloner sprite vertex shaderの未加工HLSLを、アプリ配下の `dxcompiler.dll` に含まれるDXC APIでそれぞれ `cs_6_0` / `vs_6_0` compileした。両方DXIL object生成に成功（4,840 / 6,252 bytes）。実GPU D3D12 dispatch・PSO creation・drawの証拠ではない。
- **2026-10-12 D3D12 placement 実機試験:** DXC生成の現行compute DXILをNVIDIA RTX 4070 TiのD3D12 compute queueでdispatchし、CPU式と照合した。Linear 3、Radial 4、Grid 6に加えて、count=1 Radialとidentity Gridを検証。5ケース全てbase slot、record index、offset、rotationが一致し、nonzero outputOffset (2/7/15/22/25) も合格した。これは配置compute shader自体のD3D12実行証拠で、Diligent統合やsprite rasterizationのend-to-end parityではない。
- **2026-10-12 Linear placement-to-draw 実機試験:** 配置compute DXILをD3D12 command listでdispatchし、UAV barrierとUAV→NON_PIXEL_SHADER_RESOURCE transition後、同じbufferを現行Cloner VSのStructuredBufferとして読み、80×48 unit quadのbase+3 clonesを描画した。nonzero `outputOffset=5` のbase/clone slotsを同じ `FirstPlacement` で選択し、rotation 0/15/30/45度、offset 150/120 pixelずつ、実weight `1.0/1.0/0.8/0.6` を検証。4 quadの出力bbox・pixel数・alphaがCPU参照期待値内でPASS（alpha 255/255/204/153）。CPU reference bitmapとの差は1画素で、30度cloneの下端付近のraster boundaryに限られた。テストPSはVS Colorを返す最小PSのためtexture samplingとDiligent packet自体は未検証だが、配置compute出力からvertex shader描画まで同一GPU command listで検証した。
- **仮説／未検証:** 既存Radial rotationとGrid/Matrix opacity parityに準じる設計。修正後のC++ module compileとLinear回転・decay併用時のCPU/GPU画像一致は未確認。
- **価値／懸念:** shader/packetの新規ABIを増やさずLinear rotationとweightを拡張できる。GPU enabled時のruntime parityを確認するまでは対応完了と扱わない。
- **次に確認:** Linear `rotationStep=15` と `opacityDecay=0.2` を併用し、CPU版とD3D12/Vulkan renderer readbackを `ja` / `en` / `zh` / `zh-TW` で比較する。Random legacy modeをfallback fixtureに使う。

### 2026-10-11 — TrackerManager の project load 置換と Controller binding

- **関連:** `Artifact/src/Project/ArtifactProjectImporter.cppm`（TrackerManager::fromJson / clearTrackers）、`Artifact/src/Widgets/Render/ArtifactCompositionRenderController.cppm`（controller session binding）。
- **確認できた事実:** project importer は非同期ロード時にバックグラウンド worker で実行され、従来はその中から `TrackerManager::fromJson()` によりグローバル Manager を置換していた。トラッカー配列を `ArtifactProjectImporterResult` に保持させ、ProjectManager の同期成功後／非同期 UI-thread commit 時に Manager を復元するよう変更した。さらにフレーム収集開始前に元 tracker を job ごとの `MotionTracker` copy へ複製し、フレーム収集と solve は copy 上で進める。成功時、同じ ID／同じ登録 pointer が残る場合だけ結果を元へ反映し、失敗・キャンセル・切替時は copy を破棄する。
- **仮説／未検証:** TrackerManager の置換をまたぐ解析では古い job の結果を破棄する設計だが、実機で project load と解析完了が競合した際の UI・undo・進捗表示は未確認。MotionTracker copy の frame buffer と解析結果の独立性も runtime 未検証。
- **価値／懸念:** importer worker とグローバル TrackerManager の同時更新を避け、Manager 置換後も solve worker が削除済み tracker pointer を使わない。キャンセル時に既存結果・設定を解析準備のクリア処理で失わない。1解析jobにつき開始時に tracker 状態を複製するコールドパス確保が発生する。
- **次に確認:** 許可後に非同期 project load と長時間解析を重ね、旧 job 結果が破棄され、新プロジェクト session／UI が維持されることを確認する。

### 2026-10-11 — Tracker source/session の project 保存境界

- **関連:** `Artifact/src/Widgets/Render/ArtifactCompositionRenderController.cppm`（source selection）、`Artifact/src/Project/ArtifactProject.cppm`（extension data）、`Artifact/src/Project/ArtifactProjectExporter.cppm`／`ArtifactProjectImporter.cppm`（保存復元）。
- **確認できた事実:** Tracker ドックは追跡元と結果適用先を分離し、composition default source と tracker session 別 source override を project `extension_data` に保存する。session manifest は composition ID で索引し、active tracker ID と tracker ごとの編集 geometry も保存する。解析開始時には入力元を固定する。Controller の Composition 切替／破棄では Manager session を削除せず detach し、再選択時に bind する。
- **確認できた事実:** session create/delete 用の Undo command を追加し、tracker JSON、同じ tracker ID、geometry と source override を保持して復元する。既存 `UndoManagerChangedEvent` 経由で active manifest と controller binding を再同期する。
- **確認できた事実:** `LayerChangedEvent::Removed` は layer undo command の redo でも発行される。source ID をその場で消すと、layer undo で同じ ID の layer を戻しても source 設定が復元されず Undo/Redo の意味を損なうため、イベント時の即時 cleanup は採用しない。source 解決は現在の Composition 内で layer ID の存在を検証し、stale ID は使用せず選択 layer へ fallback する。
- **仮説／未検証:** source metadata の整理は `RemoveLayerCommand` のスナップショットと同じ undoable operation に統合する必要がある。保存後の Manager session 復元、複数 Composition 間での session ID 分離、Undo/Redo の実機動作も未検証。旧データの Manager 内 orphan tracker は安全に所有 Composition を推定できない。
- **価値／懸念:** Core／サブモジュールを変更せず既存 project extension lifecycle を使って複数 session の UI 選択と geometry/source の保存を実現できる。manager session の寿命と project serialization の接続が期待どおりかは受入れ確認が必要。
- **次に確認:** tracker source metadata の remove/restore を `RemoveLayerCommand` の Undo/Redo にどう含めるか設計し、許可後に保存／再読込、session 切替、layer Undo/Redo、tracker create/delete Undo/Redo、複数 Composition 切替を実行確認する。


### 2026-10-11 — Tracker の作業形状とセッション設定の保存境界

- **関連:** `Artifact/src/Widgets/Render/ArtifactCompositionRenderController.cppm`（TrackerBoxState 初期化とキャプチャ）、`Artifact/src/Widgets/Render/ArtifactPointTrackerGizmo.cppm`（viewport geometry）、`ArtifactCore/src/Tracking/MotionTracker.cppm`（settings / track regions）。
- **確認できた事実:** Feature/Search の幅と高さは `LayeredConfigStore` の `Viewport/TrackPoint/*` キーから読み書きされ、作業環境単位の設定になっている。Tracker の Core session には Point の位置・検索領域、Planar の track points / region と解析結果が保存される。ビューポート上の `TrackerBoxState.innerCenter` 自体はその Core session JSON に含まれず、各解析開始時に Gizmo state から Core へ点／領域を作っている。
- **仮説／未検証:** プロジェクトを切り替えた後も workspace の特徴領域サイズは共有される一方、中心座標や編集中の形状は session と一緒に復元されないため、保存済み結果と画面上の編集形状が異なる可能性がある。実際の再読込後レイアウトは未検証。
- **価値／懸念:** session chooser や再開 UI を足す前に、解析済みトラックと編集可能な source geometry のどちらを project/session が所有するか決める必要がある。
- **次に確認:** source layer／composition 単位の geometry 保存、workspace default との優先順位、プロジェクト再読込時の復元先を設計し、複数 session と source 切替で検証する。

### 2026-10-11 — 実写トラスの面追跡は背景除外と幾何誤差の確認が必要

- **関連:** `ArtifactCore/src/Tracking/MotionTracker.cppm:1002`、`ArtifactCore/include/Tracking/MotionTracker.ixx:151`、`tests/ArtifactCore/MotionTrackerVideoInputTest.cpp`。
- **確認できた事実:** planar solveのECC／特徴点検出マスクは矩形ROI全体を有効にする。公開入力は`addTrackRegion(QRectF)`であり、任意の背景除外マスクを渡すAPIは見つからない。CC0橋映像の選択領域にはトラスの穴越しの川と奥側の梁が入る。正解座標のある急角度の独立テストでは、24フレームの貼り付け四隅の最大誤差2px以下という検査を通過した（2026-10-11）。実写の信頼度0.5以上という検査だけでは幾何精度の証明にならない。
- **仮説／未検証:** 実写終盤のズレに、別奥行きの背景や梁の相対運動と、投影ROIを軸平行bounding boxへ戻す処理が寄与している可能性がある。急角度自体の一般的な失敗や、背景除外での改善は未証明。
- **価値／懸念:** 相関の信頼度が高くても、貼り付け面以外の画素に追従すれば合成の位置がずれる。背景除外マスクの保持と実写の独立した特徴点注釈が精度検証に必要。
- **次に確認:** ArtifactCore変更の明示許可後、参照フレームの除外マスクをホモグラフィで伝搬する最小設計と、実写終盤の注釈点に対する再投影誤差を検証する。

### 2026-10-11 — ArtifactHashMap iteratorの衝突chain走査疑い

- **関連:** `ArtifactCore/src/Core/ArtifactHashMap.cppm`、`tests/ArtifactCore/ArtifactHashMapContractTest.cpp`。
- **確認できた事実:** iteratorは現在Nodeとbucket位置を保持する。`operator++()`はbucket位置を先に1つ進め、現在nodeの`next`を参照していない。hash衝突した複数entryが同一bucketに入る構造は`emplaceImpl()`で確認した。また`emplaceImpl()`は既存keyを見つけたときmapped valueを代入する一方、既存の`TryEmplaceDuplicateKeepsOriginalMappedValue`は元値維持を契約している。
- **仮説／未検証:** 同一bucketの連鎖要素をrange iterationで飛ばし、重複tryEmplaceで値を置換する可能性がある。runtime testはAGENTS.mdの実行制約により未実施。
- **価値／懸念:** range traversalでmap entryを取りこぼしたり、insert-only APIが既存valueを書き換えたりすると、反復ベースのシリアライズ／集計や呼び出し側の状態前提が壊れる恐れがある。
- **次に確認:** `ArtifactHashMapContractTest.IterationVisitsEveryEntryInCollisionChain` とmodel testを実行し、再現するならiterator increment／duplicate emplacementの契約を修正・検証する。

### 2026-10-11 — SmallVector debug snapshotのsamples型不一致の疑い

- **関連:** `ArtifactCore/include/Container/SmallVector.ixx`、`ArtifactCore/include/Container/ContainerDebug.ixx`、`ArtifactCore/include/Container/NamedVector.ixx`。
- **確認できた事実:** `SmallVector::debugSnapshot()` は `debugSample(4)` の戻り値 `NamedVector<ContainerElementSample>` を、`ContainerDebugSnapshot::samples` の `std::vector<ContainerElementSample>` aggregate fieldへ直接渡している。`NamedVector` には `toStdVector()` はあるが、確認した宣言範囲では暗黙の `std::vector` 変換演算子は見つからない。
- **仮説／未検証:** debugSnapshot／watch系のtemplate memberをinstantiateすると型変換エラーになる可能性がある。ビルドは禁止中のためコンパイラでは未確認。
- **価値／懸念:** 独立suiteが当該moduleのtemplate実体化で阻害される可能性があり、debug snapshot APIの利用可能性にも影響する。
- **次に確認:** ArtifactCore変更が許可された作業で、`samples.toStdVector()` 相当の明示変換が正しい境界か確認し、module compileとdebugSnapshot利用テストを実行する。

### 2026-10-11 — CppLinq lazy views borrow the input range

- **関連:** `ArtifactCore/include/Utils/CppLinq.ixx`、`tests/ArtifactCore/CppLinqContractTest.cpp`。
- **確認できた事実:** `Where` と `Select` は `const R&` を受け取り、入力rangeへ `views::filter`／`views::transform` をpipeしたviewを返す。既存suiteは元rangeの変更反映と遅延評価を検証している。
- **仮説／未検証:** 一時rangeを渡すと関数内では名前付きconst lvalueとしてview化されるため、呼び出し式の終わりでrangeが破棄された後のview走査がdanglingになる可能性が高い。現在の呼び出し元が一時rangeを使うかは未確認。
- **価値／懸念:** borrowed view契約を明示するか、所有viewと借用viewを分ければ、一時range由来の寿命事故を防ぎやすい。
- **次に確認:** 本番呼び出し箇所を検索し、一時rangeを渡す箇所の有無と許容するownership semanticsを確認してからAPI設計と寿命テストを決める。

### 2026-10-11 — Crash report parser の認識条件が header substring のみ

- **関連:** `ArtifactCore/include/Diagnostics/CoreCrashReportParser.ixx`、`tests/ArtifactCore/CrashReportParserStandaloneTest.cpp`。
- **確認できた事実:** `parseCrashReport()` は `=== Artifact Crash Report ===` が入力内のどこかにあれば `parsed=true` を返し、`parseCrashReportFile()` はそのフラグだけで成功扱いにする。例外情報やstack sectionがないheader単独の入力も成功する。
- **仮説／未検証:** report producer が出力する正式形式で必須sectionが定義されている場合、途中で切れた／壊れたreportを成功として空に近いsnapshotへ変換する可能性がある。headerのみを有効とする仕様かは未検証。
- **価値／懸念:** 最低限の構造検証を行えば、壊れたcrash reportと正常な最小reportを区別しやすい。
- **次に確認:** crash report producerの出力形式と既存保存データを照合し、成功判定に必要なsectionを決めたうえで、header-only／欠落sectionの契約テストを確定する。

### 2026-10-11 — NoiseGenerator legacy seed state is process-global

- **関連:** `ArtifactCore/src/Math/NoiseGenerator.cppm`、`ArtifactCore/include/Math/NoiseGenerator.ixx`、`tests/ArtifactCore/NoiseGeneratorStandaloneTest.cpp`。
- **確認できた事実:** Legacy `NoiseGenerator::setSeed()` rewrites shared static permutation array `p` and `isInitialized`; `perlin()` may initialize this state lazily. `NoiseField` instead stores its permutation table per instance and is documented as concurrently evaluable.
- **仮説／未検証:** Concurrent legacy noise evaluation and reseeding may race. Actual callers and whether they reseed during rendering are not yet checked.
- **価値／懸念:** Per-instance `NoiseField` can isolate deterministic procedural fields from process-wide seed changes.
- **次に確認:** Search production call sites for concurrent `perlin()` evaluation and runtime `setSeed()` use; if needed, add an explicit concurrency contract and migrate only approved call paths.

### 2026-10-11 — LayerBoundsCalculator2D の退化矩形 hit test 境界

- **関連:** `ArtifactCore/src/Geometry/LayerBoundsCalculator.cppm`、`tests/ArtifactCore/LayerBoundsCalculatorStandaloneTest.cpp`。
- **確認できた事実:** `contains()` は四辺の外積符号を判定し、全外積が0の場合も内包として返す。幅・高さがともに0の矩形では、任意の問い合わせ点への各外積が0になる。
- **仮説／未検証:** 退化矩形が hit-test 対象になると、本来の一点以外も含む判定になり得る。UI側が0サイズ矩形を問い合わせる頻度と期待仕様は未検証。
- **価値／懸念:** 0サイズまたは極小レイヤーの選択領域が広がる可能性がある。
- **次に確認:** zero-width／zero-height／zero-area各ケースの境界意味論を決め、必要なら `contains()` の退化形状処理と独立テストを追加する。

### 2026-10-11 — FormantExtractorは末尾の不完全解析windowを出力しない

- **関連:** `ArtifactCore/src/Audio/FormantExtractor.cppm`、`tests/ArtifactCore/FormantExtractorStandaloneTest.cpp`。
- **確認できた事実:** `analyzeTrack()`は`framesPerAnalysis <= totalFrames - offset`の間だけイベントを生成するため、最後にwindow長未満のsamplesが残る場合は解析しない。
- **仮説／未検証:** frame-rateと音声長が割り切れない素材では最後の短区間がmouth-shapeイベントに反映されない可能性がある。切り捨てが意図された仕様かは未検証。
- **価値／懸念:** 音声末尾の短い発音・閉口イベントがtrack出力から欠落する可能性がある。
- **次に確認:** tail paddingまたは短windowの分析を仕様化し、末尾にイベントを生成するかどうかの期待値を決める。

### 2026-10-11 — AudioVolumeのdeserializeが入力を読まない

- **関連:** `ArtifactCore/include/Audio/AudioVolume.ixx`、`ArtifactCore/src/Audio/AudioVolume.cppm`、`tests/ArtifactCore/AudioVolumeStandaloneTest.cpp`。
- **確認できた事実:** `serialize()`はvolume/mutedをJSON文字列へ出力する一方、`deserialize(const UniString&)`の実装は引数を無視し、既定volume 1.0を返す。
- **仮説／未検証:** 保存したvolume値を読み戻す経路ではレベル設定が失われる可能性がある。利用箇所は未確認。
- **価値／懸念:** API名から期待される保存復元が実現していない。
- **次に確認:** 呼び出し元とデータ互換性を調査し、serialize/deserializeのJSON形式と不正入力処理を定義した後、独立roundtripケースを追加する。

### 2026-10-11 — AudioDecibelsのdeserializeが入力を読まない

- **関連:** `ArtifactCore/include/Audio/AudioDecibels.ixx`、`ArtifactCore/src/Audio/AudioDecibels.cppm`、`tests/ArtifactCore/AudioDecibelsStandaloneTest.cpp`。
- **確認できた事実:** `serialize()`は`{"dB":...}`形式のJSON文字列を作る一方、`deserialize(const UniString&)`の実装は引数を無視して0 dBを返す。
- **仮説／未検証:** 保存した非ゼロ値を読み戻す経路で設定が失われる可能性がある。呼び出し元の利用状況は未検証。
- **価値／懸念:** `AudioDecibels` の永続化／複製でレベル値を復元できない。
- **次に確認:** 利用箇所と既存データ互換性を確認し、修正許可範囲でserialize/deserialize roundtripおよび不正JSON入力の契約を独立suiteへ加える。

### 2026-10-11 — AudioWaveformのbinningが端数frameを捨てる可能性

- **関連:** `ArtifactCore/src/Audio/AudioWaveform.cppm`、`ArtifactCore/include/Audio/AudioWaveform.ixx`、`tests/ArtifactCore/AudioWaveformStandaloneTest.cpp`。
- **確認できた事実:** bin幅は整数除算`frames / resolution`で決まり、全binを同じstep幅で処理する。
- **仮説／未検証:** `frames`が`resolution`で割り切れない場合、末尾frameがどのbinにも入らない可能性がある。現suiteは割り切れる寸法を使っており、この境界を実行確認していない。
- **価値／懸念:** preview waveformの末尾にあるtransientが欠落する可能性がある。
- **次に確認:** 端数のあるframe数と最終sampleだけに信号を置く独立caseで、全frameがbinningされるか確認する。
- **追加確認（2026-10-23）:** AudioWaveform::process() は毎回 waveformData_.resize(resolution) を呼ぶ。ソース検索では ArtifactCore / Artifact 内の呼び出し元が見つからず、実行頻度は未確認。
- **追加懸念:** audio process callback から呼ばれる経路なら、resolution変更時の再確保が処理時間へ影響する可能性がある。今回の拡張子移行ではこの挙動を変更していない。
- **次に確認:** 呼び出し元／実行コンテキストを確認し、callback内で呼ばれるなら capacity再利用可否とallocation計測を独立して評価する。

### 2026-10-11 — AudioSpectrumのDFT binはcosine投影の絶対値合計

- **関連:** `ArtifactCore/src/Audio/AudioSpectrum.cppm`、`tests/ArtifactCore/AudioSpectrumStandaloneTest.cpp`。
- **確認できた事実:** `computeFFT()`は各binで`abs(input[i] * cos(angle))`を加算しており、複素DFTのreal/imaginary成分やmagnitude normは計算しない。
- **仮説／未検証:** spectrum binsは正弦波の周波数振幅を一般的なスペクトラム解析の期待どおりには表さない可能性がある。どの表示側がこの結果を利用するか、また現状が意図的な簡易指標かは未検証。
- **価値／懸念:** このAPIをスペクトラム表示や周波数比較へ使う場合、bin値から正しい周波数強度を判断できない可能性がある。
- **次に確認:** 利用側の正規化・表示契約を調査し、DC・単一正弦波・Nyquist近傍の期待スペクトルを定めてから独立信号ケースを追加する。

### 2026-10-11 — AudioBassTrebleの係数atomicが未使用

- **関連:** `ArtifactCore/include/Audio/AudioBassTreble.ixx`、`ArtifactCore/src/Audio/AudioBassTreble.cppm`、`tests/ArtifactCore/AudioBassTrebleStandaloneTest.cpp`。
- **確認できた事実:** `bassCoeff_`と`trebleCoeff_`はメンバー初期化とconstructorで1.0をstoreされるが、`process()`は読み出さず、都度dB値からgainを計算する。
- **仮説／未検証:** atomic係数は旧実装の残存物、または将来の制御用である可能性がある。現時点の処理結果には使われない。
- **価値／懸念:** setter更新とaudio processing間のthread-safe受け渡しを目的にした設計なら、その同期契約が実際には機能していない可能性がある。
- **次に確認:** setter/processの並行利用契約を確認し、不要な残存メンバーか、atomic値を使うべき仕様かを判断する。

### 2026-10-11 — AudioChorusの一部公開設定がprocessへ未接続

- **関連:** `ArtifactCore/include/Audio/AudioChorus.ixx`、`ArtifactCore/src/Audio/AudioChorus.cppm`、`tests/ArtifactCore/AudioChorusStandaloneTest.cpp`。
- **確認できた事実:** `process()`は`rate_`、`depth_`、`baseDelayMs_`を使うが、`feedback_`と`mode_`を参照しない。出力は固定係数でdry 0.7／delayed 0.3を混合する。
- **仮説／未検証:** Feedback parameterやFlanger modeを変更しても音声出力へ作用しない可能性が高い。現状が仮実装か意図的仕様かは未検証。
- **価値／懸念:** UIから調整できる公開parameterが期待どおりの音響変化を生まない可能性がある。
- **次に確認:** Chorus／Flangerの仕様を定め、feedback loopとmode別delay/mixの期待応答を決めてからsignal-level独立ケースを設計する。

### 2026-10-11 — AudioReverbのsize parameterが処理に未接続

- **関連:** `ArtifactCore/include/Audio/AudioReverb.ixx`、`ArtifactCore/src/Audio/AudioReverb.cppm`、`tests/ArtifactCore/AudioReverbStandaloneTest.cpp`。
- **確認できた事実:** `size_`はsetter・parameter metadata・JSON保存復元に使われるが、`process()`は参照しない。また`allpassBuffer_`は確保されるが処理で使われない。
- **仮説／未検証:** sizeを変更しても残響の時間・密度は変わらない。これらが暫定実装か意図的な簡略化かは未検証。
- **価値／懸念:** parameter UIで調整できても、sizeが音響結果へ反映されない可能性がある。
- **次に確認:** DSP仕様を確定し、sizeが残響長・遅延・密度のどれを制御するか決めたうえで、異なるsize設定間の出力差を期待値として追加する。

### 2026-10-11 — AudioStereoMixer center／delay settersがprocessに未接続

- **関連:** `ArtifactCore/include/Audio/AudioStereoMixer.ixx`、`ArtifactCore/src/Audio/AudioStereoMixer.cppm`、`tests/ArtifactCore/AudioStereoMixerStandaloneTest.cpp`。
- **確認できた事実:** `setCenter`、`setLeftDelay`、`setRightDelay`はprivate stateを更新するが、`process()`は`leftRight_`だけを参照して左右gainを適用する。
- **仮説／未検証:** 現行APIの利用者はcenter extractionや左右delayが機能すると期待し得るが、実処理へ反映されない可能性が高い。各項目が将来用placeholderかは未検証。
- **価値／懸念:** UIやautomationに設定導線がある場合、値を変えても音声結果が変化しない。
- **次に確認:** setterの利用箇所とAudioStereoMixerの正式仕様を確認し、実装範囲が許可された際にそれぞれのDSP応答を決めて独立期待値を追加する。

### 2026-10-11 — AudioHighLowPassのhigh-pass recurrenceがDCを残す可能性

- **関連:** `ArtifactCore/src/Audio/AudioHighLowPass.cppm`、`ArtifactCore/include/Audio/AudioHighLowPass.ixx`、`tests/ArtifactCore/AudioHighLowPassStandaloneTest.cpp`。
- **確認できた事実:** high-pass pathは`hpInput = input - hpState`、次に`hpState = hpCoef * hpState + hpInput`を計算して`hpInput`を出力する。low-passも有効な場合、後段で`data[i]`をlow-pass stateへ再代入する。
- **仮説／未検証:** high-pass recurrenceは一定DC入力に対して0へ収束しない可能性が高く、両filter有効時はhigh-pass成分がlow-pass出力に置換される。追加DC rejection caseは意図した期待を表現するが、実行していない。
- **価値／懸念:** UI上high-passとlow-passの双方を設定できる場合、high-pass単独やcascade filterとしての期待と異なる応答になる可能性がある。
- **次に確認:** DSP仕様（one-pole cutoff、係数、cascade順）を決め、impulse／step responseと両filter同時有効時の期待値を定義してから修正範囲と回帰確認を行う。

### 2026-10-11 — AudioDelayが書き込み位置より先のbuffer indexを読む

- **関連:** `ArtifactCore/src/Audio/AudioDelay.cppm`、`ArtifactCore/include/Audio/AudioDelay.ixx`、`tests/ArtifactCore/AudioDelayStandaloneTest.cpp`。
- **確認できた事実:** 各sampleでread indexを`writePos + delaySamples`として計算し、同じ`writePos`へ現在sampleを保存する。delay bufferは初期状態で48000要素のゼロ値を持つ。
- **仮説／未検証:** 通常の遅延範囲では過去に書いたsampleではなく未書込の先行領域を読み続け、Impulseの遅延音が返らない可能性が高い。新しい1-frame impulse regression caseがこの期待を確認するが、ビルド・実行はしていない。
- **価値／懸念:** delay timeとwet mixを設定してもechoが出ず、wet signalが無音になる可能性がある。
- **次に確認:** 許可された作業範囲で独立suiteを実行し、SPSC状態・block境界を含む読出し／書込み位置を正す修正後に、単一blockおよび複数blockのimpulse応答を検証する。

### 2026-10-11 — AudioCompressor parameter metadataとsetter範囲が異なる

- **関連:** `ArtifactCore/include/Audio/AudioCompressor.ixx`、`ArtifactCore/src/Audio/AudioCompressor.cppm`、`tests/ArtifactCore/AudioCompressorStandaloneTest.cpp`。
- **確認できた事実:** `getParameters()`のthresholdは-60〜0 dB、attackは0.1〜100 ms、releaseは1〜1000 ms。一方setterとprocess内clampはthreshold -120〜20 dB、attack 0.1〜1000 ms、release 1〜5000 msを許容する。
- **仮説／未検証:** UIやautomationがmetadata範囲を制約として使用する場合、直接setter／JSON経由で設定できる値域と表示上の値域が異なる可能性がある。
- **価値／懸念:** serialized settingの表示／再編集時に値がmetadata外となり、UIでの再現性が不明瞭になる。
- **次に確認:** `EffectParameter`利用側とcompressor UIのclamp処理を調べ、各境界の正規契約を決めてから値域を横断する独立ケースを追加する。

### 2026-10-11 — AudioTone parameter metadataとsetter／process範囲が異なる

- **関連:** `ArtifactCore/include/Audio/AudioTone.ixx`、`ArtifactCore/src/Audio/AudioTone.cppm`、`tests/ArtifactCore/AudioToneStandaloneTest.cpp`。
- **確認できた事実:** `getParameters()`はfrequencyを20–20000 Hz、amplitudeを0–1として公開する。一方、直接setterはfinite値ならこの範囲外も保持し、`process()`ではfrequencyを0–96000 Hzにclampし、amplitudeはclampしない。
- **仮説／未検証:** parameter metadataをUIやautomationが編集制約として使用し、直接setterやJSONが別経路から範囲外値を渡す場合、表示値とprocess実効値が異なる可能性がある。
- **価値／懸念:** parameter range、serialization、DSP clampの契約が未統一だと、再現性・automation表示で混乱を生む。
- **次に確認:** `EffectParameter`の全利用箇所と既存tone UIのclamp責務を調べ、どの境界が正式な制約か決めてから範囲テストを加える。

### 2026-10-11 — AudioPannerの複数modeが同一gainを返す

- **関連:** `ArtifactCore/include/Audio/AudioPanner.ixx`、`ArtifactCore/src/Audio/AudioPanner.cppm`、`tests/ArtifactCore/AudioPannerStandaloneTest.cpp`。
- **確認できた事実:** `calculateGain()`は`StereoBalance`以外の`EqualPower`、`VBAP`、`Ambisonics`、`Binaural`で、azimuth/elevationに関係なく`{1.0f, 1.0f}`を返す。定電力ゲインの独立static関数は別に存在する。
- **仮説／未検証:** 複数modeは未実装または暫定placeholderである可能性がある。呼び出し側がmode名どおりの立体音響結果を期待するか、modeがUI設定だけで未使用かは未検証。
- **価値／懸念:** mode選択が処理結果へ反映されない場合、設定上の期待と実際のpanが一致しない。
- **次に確認:** 利用箇所とmodeごとのspeaker/HRTF/encoder設計を調べ、各modeの契約と独立期待値を定めてからgain動作テストを追加する。

### 2026-10-11 — AudioRingBufferの公開12chレイアウトと保存上限に差

- **関連:** `ArtifactCore/include/Audio/AudioSegment.ixx`、`ArtifactCore/include/Audio/AudioRingBuffer.ixx`、`ArtifactCore/src/Audio/AudioRingBuffer.cppm`、`tests/ArtifactCore/AudioRingBufferStandaloneTest.cpp`。
- **確認できた事実:** `AudioChannelLayout::Surround714`は12chと定義されている一方、`AudioRingBuffer::Impl`は`kMaxChannels = 10`で、`write()`は10超の入力チャンネルを拒否する。
- **仮説／未検証:** RingBufferが12ch layoutを扱う必要があるなら、enum上の対応と実装の最大チャンネル数が一致していない可能性がある。全consumerが12chを使わない構成かは未検証。
- **価値／懸念:** 7.1.4 audioをこのring bufferへ流すとwrite失敗となり、上流が失敗を適切に扱わなければ音声欠落につながる可能性がある。
- **次に確認:** 利用箇所から実際の最大channel layoutとwrite失敗時の扱いを追い、仕様を確定してから最大ch境界のstandalone契約を追加する。

### 2026-10-11 — AudioBufferQueueのチャンネル長チェックがframeCountと整合しない

- **関連:** `ArtifactCore/include/Audio/AudioBufferQueue.ixx`、`ArtifactCore/include/Audio/AudioSegment.ixx`、`tests/ArtifactCore/AudioBufferQueueStandaloneTest.cpp`。
- **確認できた事実:** `AudioSegment::frameCount()`は全チャンネルの最小長を返す。`AudioBufferQueue::push()`はその値より短いチャンネルを探すが、同じチャンネル群から計算した最小長より短いものは存在しないため、この長さ検証条件は成立しない。
- **仮説／未検証:** channelごとにサンプル数が異なる不正segmentもqueueへ入り得る。意図した契約が可変長チャンネルを許さない場合は、不揃いデータを拒否すべき。
- **価値／懸念:** consumerがフレーム数と各channelの有効サンプル数を異なる前提で処理すると、欠落サンプルの無音化や範囲外アクセスにつながる可能性がある。
- **次に確認:** AudioBufferQueue利用側のchannel length前提を追跡し、許可された範囲で均一長要件またはframeCount基準のvalidationを定義して不揃いchannelの契約テストを追加する。

### 2026-10-11 — ArtifactOptional swap writes across inactive union member

- **関連:** `ArtifactCore/src/Core/ArtifactOptional.cppm`、`tests/ArtifactCore/ArtifactOptionalContractTest.cpp`。
- **確認できた事実:** owning `ArtifactOptional<T>::swap` のpresent／empty分岐で、empty側の `contained_` にplacement constructionではなくmove assignmentを行ってから `hasValue_` をtrueにする。
- **仮説／未検証:** 非trivialな `T` ではinactive union memberのobject lifetime開始条件を満たさない可能性がある。新しいmove-only payloadケースは通常の値状態を検査するが、lifetime違反を直接証明するものではない。
- **価値／懸念:** ASan等で検出可能な未定義動作の疑いがあり、`unique_ptr` や他の非trivial payloadに対するswap安全性へ影響する可能性がある。
- **次に確認:** 子repo編集の指示範囲を確認したうえで、標準union lifetime規則を照合し、適切なsanitizer付きstandalone実行またはplacement constructionへの修正後検証を行う。

### 2026-10-11 — Audio ring-buffer capacity unit is implicit

- **関連:** `ArtifactCore/include/Core/Defaults.ixx`、`tests/ArtifactCore/DefaultsContractTest.cpp`。
- **確認できた事実:** `kAudioRingBufferCapacity` は `kDefaultSampleRate * 8` で定義され、コメントは8秒と説明している。チャンネル数は別定数 `kDefaultAudioChannels` で2となっている。
- **仮説／未検証:** 容量がsample frames単位かinterleaved scalar samples単位かは、この定数とコメントだけでは確定できない。チャンネル数を掛けるべきかの断定は未検証。
- **価値／懸念:** 単位を明示しないまま利用側がchannel countを乗除すると、実際の保持時間が意図とずれる可能性がある。
- **次に確認:** `kAudioRingBufferCapacity` の全利用箇所とbuffer indexing conventionを追い、unitと保持時間を定義してから追加契約を設ける。

### 2026-10-11 — Tag StringLike setter implementation is empty

- **関連:** `ArtifactCore/include/Utils/Tag.ixx`、`ArtifactCore/src/Utils/Tag.cppm`、`tests/ArtifactCore/TagContractTest.cpp`。
- **確認できた事実:** `Tag::setName(const S&)` のテンプレート定義は空で、非テンプレート `setName(const UniString&)` だけが名前を更新する。既存契約テストには `std::string` と `QString` を渡して更新を期待する2ケースがある。
- **仮説／未検証:** 空のテンプレート定義ではその2ケースの期待値を満たさない可能性が高い。C++20 module の実際の呼出し解決／ビルドは未確認。
- **価値／懸念:** Tag用standalone suiteの基本API契約に不整合がある可能性があり、ケース追加だけでは回帰品質を高められない。
- **次に確認:** 子repo編集の指示範囲を確認したうえで、許可された作業時にStringLike overloadの意図と実際のsuite結果を確認する。

### 2026-10-11 — RationalTime LCM can overflow before fallback

- **関連:** `ArtifactCore/src/Time/RationalTime.cppm`、`tests/ArtifactCore/RationalTimeContractTest.cpp`。
- **確認できた事実:** 異なるscaleの加算・減算は`lcm(s1, s2)`を呼び、`lcm`は`abs(a) / gcd(a,b) * abs(b)`をoverflow検査なしで計算する。その後にあるfallbackはLCM計算の後でしか到達しない。
- **仮説／未検証:** scaleのLCMがint64範囲を超える場合は既存の`fromSeconds` fallbackでwrapせず結果化するべきと考え、ほぼint64最大の互いに異なるscaleを使う加算・減算の独立caseを追加した。未実行。
- **価値／懸念:** 適切な値のRationalTime同士でも、異なる大きなtimebaseを混ぜた際に未定義signed overflowや誤結果を検出する。
- **次に確認:** LCMをchecked multiplyで構築し、表現不可時にfallbackへ進むよう修正した後、standalone suiteを実行して確認する。

### 2026-10-11 — HashValue unknown algorithm falls through to MD5

- **関連:** `ArtifactCore/src/Utils/HashValue.cppm`、`tests/ArtifactCore/HashValueContractTest.cpp`。
- **確認できた事実:** Qt algorithm変換は`Unknown`を含むdefaultでMD5を返し、`fromData`はそのdigest bytesを`Algorithm::Unknown`として保持する。結果の`isValid()`はfalseだが、raw bytesとhex digestは存在する。
- **仮説／未検証:** 未知のアルゴリズムをMD5へ代替すると呼び出し側が誤ったdigestを参照し得るため、factoryは空のinvalid値を返すべきと考え、独立caseを追加した。テスト未実行。
- **価値／懸念:** `Unknown`を指定した際のalgorithm downgradeを可視化し、誤ったハッシュ比較を防ぐ。
- **次に確認:** `fromData`と`fromFile`双方で未知enum値をdigest計算前に拒否する契約に統一する。

### 2026-10-11 — artifactAbs preserves negative floating zero

- **関連:** `ArtifactCore/include/Core/ArtifactMath.ixx`、`tests/ArtifactCore/ArtifactMathContractTest.cpp`。
- **確認できた事実:** 浮動小数点にも使える`artifactAbs`は`value < T{}`のときだけ符号反転するため、`-0.0 < 0.0`がfalseとなり符号bitが残る。
- **仮説／未検証:** absolute valueはnegative zeroをpositive zeroへ正規化する期待が妥当と考え、`std::signbit`で検査する独立caseを追加した。テスト未実行。
- **価値／懸念:** zeroの比較では見えない符号差を、後続の符号依存演算やserializationへ持ち越さないための検出。
- **次に確認:** floating overloadを`std::abs`等へ分けるか、negative zero保持を意図した仕様とするか決める。

### 2026-10-11 — RawImage accepts unsupported pixel type as valid

- **関連:** `ArtifactCore/src/Image/RawImage.cppm`、`ArtifactCore/include/Image/RawImage.ixx`、`tests/ArtifactCore/RawImageContractTest.cpp`。
- **確認できた事実:** `getPixelTypeSizeInBytes()`は既知scalar typeのみbyte幅を返し、未知typeでは0を返す。一方`isValid()`はshapeの正値、data非empty、pixelType非emptyだけを確認し、byte幅とdata長の整合を見ない。
- **仮説／未検証:** 有効なraw imageは既知pixel typeを持ち、width × height × channels × byte widthのpayloadを最低限含むべきと考え、未知type拒否と不足buffer拒否の独立caseを追加した。実装はArtifactCore子repoのため変更していない。テスト未実行。
- **価値／懸念:** 下流処理でbyte幅0や途中で切れたpixel bufferを有効画像として扱う危険を検出する。
- **次に確認:** oversized bufferの許容、strideや平面別layoutの有無、サイズ計算overflowを確認して必要byte数の厳密な契約を定める。

### 2026-10-11 — Id parser can ignore suffix after embedded NUL

- **関連:** `ArtifactCore/src/Utils/Id.cppm`、`tests/ArtifactCore/IdContractTest.cpp`。
- **確認できた事実:** QStringからIdを構築するとUTF-8化後の`QByteArray::constData()`をBoost UUID parserへ渡す。NUL以降のbyteはC文字列として見えない。
- **仮説／未検証:** UUID入力全体を検証し、末尾の埋め込みNULとsuffixを含む入力は拒否すべきと考え、valid UUID + NUL + suffixのtestを追加した。テスト未実行。
- **価値／懸念:** 見かけ上不正な入力が有効UUID prefixへ切り詰められるのを検出する。ArtifactCore子repoの実装は変更していない。
- **次に確認:** parserへ明示byte長を渡すか、Boost parser後に消費長と元QString長が一致することを検証し、NUL境界を独立suiteで確認する。

### 2026-10-11 — Result can be successful without a value

- **関連:** `ArtifactCore/include/Utils/Result.ixx`、`tests/ArtifactCore/ResultContractTest.cpp`。
- **確認できた事実:** `Result<T>(Status)`はstatusをそのまま保持し、`Result<T>::success()`はvalue optionalでなくstatusのsuccess flagを返す。そのため`Result<int>(Status::ok())`は`success()==true`かつ`hasValue()==false`になる。
- **仮説／未検証:** valueを持たないResult<T>はfailure状態として正規化すべきと考え、食い違いを防ぐcaseを追加した。テスト未実行。
- **価値／懸念:** callerが成功を確認後にvalueを使う通常の契約で、未定義の空optional参照へ進む可能性を検出する。ArtifactCore子repoの実装は変更していない。
- **次に確認:** `Status::ok()`からの変換方針を定め、Result<T>の成功状態とvalue presenceを常に整合させる。

### 2026-10-11 — ArtifactHashMap tryEmplace eagerly constructs and replaces duplicate value

- **関連:** `ArtifactCore/src/Core/ArtifactHashMap.cppm`、`tests/ArtifactCore/ArtifactHashMapContractTest.cpp`。
- **確認できた事実:** variadic `tryEmplace(key, args...)`は`emplaceImpl(key, V(args...))`を呼ぶため、重複確認より前にmapped値を構築する。共通`emplaceImpl`は重複時に既存mapped値へ転送代入している。
- **仮説／未検証:** try-emplace系操作では重複時にmapped値を構築せず、既存値も保持するべきと考え、構築回数と値を検査するcaseを追加した。テスト未実行。
- **価値／懸念:** 重複keyに対する不要な副作用と、既存値の意図しない上書きを検出する。ArtifactCore子repoの実装は変更していない。
- **次に確認:** `tryEmplace`と`emplace`の重複時契約を分け、構築遅延と値保持を実装後に独立suiteで検証する。

### 2026-10-11 — ArtifactArray appendAll self-alias can fail to terminate

- **関連:** `ArtifactCore/include/Core/ArtifactArray.ixx`、`tests/ArtifactCore/ArtifactArrayContractTest.cpp`。
- **確認できた事実:** `appendAll(const Array& other)`は`other`のrange-for中に`append()`する。`other`が*thisと同じobjectの場合、反復中にsizeが増え続け、end iteratorも動くため処理が終了しない。
- **仮説／未検証:** self-appendは元の要素列を一度だけ複製する契約が使いやすいと考えるが、ハングを起こすテストは追加していない。
- **価値／懸念:** 明示的なaliasingで無限に配列を伸ばし続ける危険がある。ArtifactCore子repoの実装は変更していない。
- **次に確認:** alias-safeなappendAll実装方針を決め、固定された元sizeでのself-appendを独立suiteで検証する。

### 2026-10-11 — Buffer afterLast drops input byte when delimiter is absent

- **関連:** `ArtifactCore/include/Utils/Buffer.ixx`、`tests/ArtifactCore/BufferContractTest.cpp`。
- **確認できた事実:** `splitLast()`はdelimiter不在時に`{empty, value}`を返すが、`afterLast()`は右partがemptyかどうかだけを調べ、非emptyなら先頭byteを常に除去する。
- **仮説／未検証:** delimiter不在時のafter-last helperは入力全体を保つべきと考え、主要aliasを同じcaseで検査するテストを追加した。テスト未実行。
- **価値／懸念:** separatorなしの文字列から誤って先頭byteが失われる境界を検出する。ArtifactCore子repoの実装は変更していない。
- **次に確認:** delimiterが見つかったかを明示するAPIに分けるか、split結果の区別を使ってbefore/after helperの不在契約を揃える。

### 2026-10-11 — ArtifactRegex invalid recompile retains capture count

- **関連:** `ArtifactCore/include/Core/ArtifactRegex.ixx`、`tests/ArtifactCore/ArtifactRegexContractTest.cpp`。
- **確認できた事実:** `compile()`は開始時にprogramを消去するが、`groupCount_`は正常compile完了時にしか更新しない。そのためcapture付きregexの後にcompile失敗すると、invalid objectから以前のcapture数が返る。
- **仮説／未検証:** invalid状態のcapture数は0へ戻すべきと考え、2 captureから構文エラーへの再compileを検査するcaseを追加した。テスト未実行。
- **価値／懸念:** stale parser metadataを検出する。ArtifactCore子repoの実装は変更していない。
- **次に確認:** invalid patternおよびemitter失敗後に、公開metadataが常に初期状態へ戻る契約を確認する。

### 2026-10-11 — AspectRatio decimal approximation skips positive minimum

- **関連:** `ArtifactCore/src/Core/AspectRatio.cpp`、`tests/ArtifactCore/AspectRatioContractTest.cpp`。
- **確認できた事実:** decimal近似ループは分母を1〜100で探索する一方、`round(ratio * denom) <= 0`の候補をすべて飛ばす。`0.004`では最大分母でも丸めた分子が0となり、初期値の`1:1`が残る。
- **仮説／未検証:** 正の入力は許容分母内で最も近い正の分数へ近似するのが望ましいと考え、`0.004`に対する`1:100`を契約テストに追加した。テスト未実行。
- **価値／懸念:** 1/200より小さい正のratioが1:1へ化ける境界を見つける。ArtifactCore子repoの実装は変更していない。
- **次に確認:** decimal parserの近似仕様を決め、極小正数と最大許容分母周辺を独立suiteで検証する。

### 2026-10-11 — Audio profiler reset leaves last callback frame metadata stale

- **関連:** `ArtifactCore/include/Utils/PerformanceProfiler.ixx`、`tests/ArtifactCore/PerformanceProfilerContractTest.cpp`。
- **確認できた事実:** `AudioEngineProfiler::resetStats()`はcallback/fill counters、sums、maxima、underflow count、buffer levelを0にするが、`lastCallbackFrames_`と`lastRequestedFrames_`をリセットしない。
- **仮説／未検証:** reset後snapshotはframe metadataも初期値0へ戻すと考え、caseを追加した。実行していない。
- **価値／懸念:** reset後のsnapshotが古いcallback状態を示し、集計値と矛盾する境界を検出する。
- **次に確認:** reset APIの期待契約を確認し、必要なら2つのatomicをreset処理へ含めて独立suiteで検証する。

### 2026-10-11 — ArtifactRandom real range width can overflow

- **関連:** `ArtifactCore/include/Core/ArtifactRandom.ixx`、`tests/ArtifactCore/ArtifactRandomContractTest.cpp`。
- **確認できた事実:** `between(double min, double max)`は`max - min`を先に計算するため、両端が有限でも`lowest()`から`max()`までの幅は正のinfinityになる。
- **仮説／未検証:** 有限な順序付き端点からは有限で半開区間内の値を返すべきと考え、全域端点のcaseを追加した。テスト未実行。
- **価値／懸念:** 広いrangeでinfinity / NaNが返る数値境界を検出する。ArtifactCore子repoの実装修正は行っていない。
- **次に確認:** overflowしない補間式または生成器を選び、端点近傍・最大範囲の分布を独立suiteで確認する。

### 2026-10-11 — Safe relative path predicate accepts embedded NUL

- **関連:** `ArtifactCore/include/Utils/Text/Path.ixx`、`tests/ArtifactCore/PathContractTest.cpp`。
- **確認できた事実:** `normalizePathSeparators()`はembedded NULを拒否するが、`isSafeRelativePath()`は空・absolute・`..` segmentだけを確認し、`"safe\\0name"`のようなNUL含有文字列をtrueにする。
- **仮説／未検証:** `isSafeRelativePath()`の「safe」判定はNUL含有入力も拒否すべきと考え、単独caseを追加した。テスト未実行。
- **価値／懸念:** C系APIへ渡した場合に文字列が途中で切れる可能性のあるpathを安全と誤判定する境界を検出する。ArtifactCore child repoの修正は行っていない。
- **次に確認:** path predicate群でNULの方針を統一し、修正後に独立suiteで確認する。

### 2026-10-11 — NameGenerator reinterprets placeholder text inside base names

- **関連:** `ArtifactCore/src/Utils/NameGenerator.cppm`、`tests/ArtifactCore/NameGeneratorContractTest.cpp`。
- **確認できた事実:** `makeCandidateZero()`はテンプレートの`(\\name)`をbase文字列で置換した後、完成途中の文字列全体から最初の`***`を検索する。baseに`***`が含まれると、番号スロットより先にbase内の文字が置換される。
- **仮説／未検証:** baseは任意の利用者名として不透明に保持されるべきと考え、`take***`を含む入力のcaseを追加した。実行していない。
- **価値／懸念:** パターン構文と値の文字列が衝突し、生成名が破損する境界を検出する。ArtifactCore側の修正はサブモジュール編集禁止のため行っていない。
- **次に確認:** テンプレートを値挿入前に分解する方法でslot位置を保持し、独立suiteで実行確認する。

### 2026-10-11 — Numeric min/max NaN behavior depends on argument order

- **関連:** `ArtifactCore/include/Utils/Numeric.ixx`、`tests/ArtifactCore/NumericContractTest.cpp`。
- **確認できた事実:** `min_same` / `max_same` はcommon typeへ変換後、`std::min` / `std::max` に委譲する。NaNとの `<` 比較がfalseとなるため、第1引数がNaNならNaNを返し、第2引数がNaNなら有限値を返す。
- **仮説／未検証:** 既存の標準アルゴリズム委譲を契約として記録するcaseを追加した。テストは未実行。
- **価値／懸念:** NaNの順序依存性を可視化し、将来の置換・最適化で無意識に意味が変わるのを検知できる。呼び出し側がNaN伝播を期待する場合は別途明示処理が必要。
- **次に確認:** 数値ユーティリティの公開契約としてNaN伝播を保証すべきか、利用側の要件を確認する。

### 2026-10-11 — NameGenerator padding is capped by local buffer

- **関連:** `ArtifactCore/src/Utils/NameGenerator.cppm`、`tests/ArtifactCore/NameGeneratorContractTest.cpp`。
- **確認できた事実:** `formatPaddedNumber()`は32 byteのstack bufferへ`snprintf`し、幅32以上では終端nullを含む固定領域によりrequested widthを満たせない。
- **仮説／未検証:** `zeroPad`は最小桁数として機能すべきとし、width 40の回帰caseを追加。実行していない。
- **価値／懸念:** 大きいpadding設定で生成名が切れる/幅不足となる問題を検出する。
- **次に確認:** `zeroPad`の上限契約を決め、可変長ではなくbounded安全な生成形式を選んでstandalone testを実行する。

### 2026-10-11 — ArtifactFunctionRef accepts a null function pointer as valid

- **関連:** `ArtifactCore/include/Core/ArtifactFunctionRef.ixx`、`tests/ArtifactCore/ArtifactFunctionRefContractTest.cpp`。
- **確認できた事実:** function pointer overloadは入力pointerのnull検査なしでstorageに保存し、常にinvoke thunkを設定するため`isValid()`がtrueとなる。
- **仮説／未検証:** null function pointerはdefault/nullptr wrapper同様invalidであるべきと考え、trait/runtime caseを追加した。未実行。
- **価値／懸念:** wrapperがnull targetをcallableと誤認してinvoke時に落ちる境界を検出する。
- **次に確認:** constructorでnull関数ポインタを空状態として扱い、standalone caseを実行確認する。

### 2026-10-11 — UniformScale implicitly copies owning Impl pointer

- **関連:** `ArtifactCore/include/Core/UniformScale.ixx`、`ArtifactCore/src/Core/UniformScale.cppm`、`tests/ArtifactCore/UniformScaleContractTest.cpp`。
- **確認できた事実:** `UniformScale`はowning raw `Impl*`とuser-declared destructorを持つ一方、copy/move operationsを宣言していない。C++の暗黙copyはポインタ値を複製する。
- **仮説／未検証:** 公開型は少なくともshallow copy不可であるべきとしてtrait assertionをruntime testにした。現実装ではcopy traitがtrueとなり失敗見込み。未実行。
- **価値／懸念:** 二重deleteを招くcopy pathの回帰を検出する。実装修正はArtifactCore child repo変更にあたるため、今回は行わない。
- **次に確認:** copy/move禁止または安全な所有権移譲を明示し、standalone suiteでtrait契約を実行する。

### 2026-10-11 — RationalTime rounding bias can overflow independently

- **関連:** `ArtifactCore/src/Time/RationalTime.cppm`、`tests/ArtifactCore/RationalTimeContractTest.cpp`。
- **確認できた事実:** `rescaledTo()`が積のoverflowを回避した後、正負の両経路で`denominator / 2`をnumeratorへ足す。`INT64_MAX / 2`相当の端数roundingでbias加算自体が符号付き範囲を超える。
- **仮説／未検証:** 結果はhalf-away-from-zeroの正負対称値になるべきとし、新しい端値caseを追加した。実行していない。
- **価値／懸念:** 積overflow fallbackだけでは保護されないsigned UBを独立に検出する。
- **次に確認:** 端値を安全に丸める除算・剰余ベース実装を行い、両符号の結果をstandalone suiteで検証する。

### 2026-10-11 — RationalTime rescaling negates INT64_MIN

- **関連:** `ArtifactCore/src/Time/RationalTime.cppm`、`tests/ArtifactCore/RationalTimeContractTest.cpp`。
- **確認できた事実:** `rescaledTo()`の負数丸め式は`-numerator`を評価し、`numerator == INT64_MIN`では符号付き範囲を超える。
- **仮説／未検証:** `INT64_MIN`を同scaleまたはscale 2から1へ変換する数学的結果を保つべきとしてcaseを追加した。未実行。
- **価値／懸念:** 丸めbias overflowとは別に、最小値だけで起こるnegation UBを検出する。
- **次に確認:** magnitudeベースの符号付き丸め処理を用意し、INT64_MINの恒等・縮小変換を実行検証する。

### 2026-10-11 — FileTypeDetector may accept invalid UTF-8 bytes as text

- **関連:** `ArtifactCore/src/File/FileTypeDetector.cppm`、`tests/ArtifactCore/FileTypeDetectorContractTest.cpp`。
- **確認できた事実:** magic判定のtext fallbackはheader bytesのcontrol rangeだけを検査し、UTF-8 continuationやscalar validityを確認している処理が見当たらない。既存suiteにはinvalid lead/continuationがBinaryという期待がある。
- **仮説／未検証:** invalid leadとcontinuation以外のoverlong, surrogate, out-of-range, truncated sequenceもBinaryになるべきと考え、table caseを追加した。コード読解上は現実装のtext fallbackで通る可能性があるが、テストは未実行。
- **価値／懸念:** UTF-8妥当性の穴をより完全に検出する。既存期待と実装が不一致の可能性があり、ArtifactCore child repoには変更を加えていない。
- **次に確認:** standalone suiteで上記4 encoding classを実行し、必要ならUTF-8 scalar validatorを実装・別依頼で扱う。

### 2026-10-11 — AssetMetaFile setters preserve extension JSON

- **関連:** `ArtifactCore/src/Asset/AssetMetaFile.cppm`、`tests/ArtifactCore/AssetMetaFileContractTest.cpp`。
- **確認できた事実:** `setUuid()` / `setType()` は既存`asset` objectを取得して対象keyだけを更新し、`data_`内の他のroot fieldも残す。standalone caseはsave/load後の未知field保持を検査する。
- **仮説／未検証:** 将来追加されたmetadata fieldを古いreaderが更新しても壊さず保存できることが互換性上望ましい。caseは追加したが未実行。
- **価値／懸念:** forward-compatible metadata round tripを回帰契約化する。未知fieldを意図して削除するAPIが将来必要な場合は明示操作との区別が要る。
- **次に確認:** `AssetMetaFileStandalone`でroot/asset extension fieldのsave/load往復を実行する。

### 2026-10-11 — SteadyClock tick subtraction can overflow

- **関連:** `ArtifactCore/include/Core/ArtifactChrono.ixx`、`tests/ArtifactCore/ArtifactChronoContractTest.cpp`。
- **確認できた事実:** `SteadyClock::between()`は両OS経路でtick差を符号付き`int64_t`上で減算する。POSIXは`to > from`判定後だが、Windowsは差分を計算してから符号を検査する。全域の正差と逆順差の絶対値は`int64_t`で表せない。
- **仮説／未検証:** 正方向の最大Duration飽和と逆方向の0 clampが期待動作と考え、extreme tick差の両向きcaseを追加した。未実行。
- **価値／懸念:** 長時間や不正tick入力での符号付き整数overflowを検出する。Windows分岐では逆順入力も減算自体がoverflowする。
- **次に確認:** 安全な差分計算（符号付き境界判定）を実装し、WindowsとPOSIX双方で境界を実行検証する。

### 2026-10-11 — FrameRate storage scale can overflow on finite input

- **関連:** `ArtifactCore/include/Frame/FrameRate.ixx`、`tests/ArtifactCore/FrameRateContractTest.cpp`。
- **確認できた事実:** `storageScaleForFps()`はNaN/Inf/nonpositive値をfallbackへ送るが、有限正値は上限確認なしで`std::llround`後`int64_t`へ変換する。
- **仮説／未検証:** `double`の極大有限値は表現可能範囲を大幅に超えるため、整数上限への飽和が安全な契約と考え、新しい回帰caseを追加した。未実行。
- **価値／懸念:** legacy frame scale変換で範囲外の浮動小数点整数変換を検出できる。期待値は上限飽和のAPI契約を明示するため、実装修正が必要になる見込み。
- **次に確認:** `fps > int64_t max`判定を`llround`より先に行う実装へ直し、独立FrameRate suiteで極大値と境界近傍を実行確認する。

## 2026-10-24: Verdigris QWidget implementation may expose Clang module ownership ambiguity

- **関連:** `Artifact/src/Widgets/ArtifactSecondaryPreviewWindow.cppm`、`Artifact/include/Widgets/ArtifactSecondaryPreviewWindow.ixx`、Clang 23.1.4 / Windows MSVC ABI
- **確認できた事実:** Fresh P1689 scan succeeded, but the implementation compile failed while expanding `W_OBJECT_IMPL(ArtifactSecondaryPreviewWindow)`: Clang reported ambiguous `operator->` lookup at `QObject::d_ptr` with `QScopedPointer<QObjectData>` candidates, then the frontend terminated by signal. The `.cpp` migration was reverted and the manifest returned to `.cppm`. No source diagnostic outside this macro expansion was established.
- **価値または懸念:** A clean module scan and available PCM do not establish that Verdigris implementation macros are valid across the named-module boundary. The exact cause is unverified and should not be classified as a simple missing include or extension-only issue.
- **次に確認すべきこと:** Before retrying this widget, inspect its Verdigris macro expansion and Qt declarations as imported through its interface module; keep the implementation on `.cppm` until a source-level fix is justified and compiled.

### 2026-10-11 — FastSettingsStore failed reload clears existing cache

- **関連:** `ArtifactCore/src/Core/FastSettingsStore.cppm`、`tests/ArtifactCore/FastSettingsStoreContractTest.cpp`。
- **確認できた事実:** `loadFromDiskUnlocked()`はCBORの読み込み・型検査より前に`cache_`をclearし、invalid payloadではfalseを返す。`reload(true)`はこの関数を直接呼ぶ。
- **仮説／未検証:** reload失敗時は最後に読み込み済みの値を保持し、エラー復旧可能にする方が望ましい。新しいcaseはその期待だが、実行していない。
- **価値／懸念:** ディスク破損や途中書き込み後のreloadでメモリ上の有効設定が消える可能性を検出する。ArtifactCore child repoは変更していない。
- **次に確認:** invalid CBOR・I/O失敗時のreload保証を定義し、必要ならtemporary cacheへのparse後にcommitする方式へ直して独立suiteで確認する。

### 2026-10-11 — FastSettingsStore sync failure retains retry state

- **関連:** `ArtifactCore/src/Core/FastSettingsStore.cppm`、`tests/ArtifactCore/FastSettingsStoreContractTest.cpp`。
- **確認できた事実:** `syncUnlocked()`は未open、親directory作成、`QSaveFile::open/write/commit`の失敗時にfalseを返し、成功時だけdirty/pendingをclearする。
- **仮説／未検証:** 各I/O失敗経路でdirty値とpending countが保たれ、後のretryが可能なことを回帰契約にした。新caseはtargetを既存directoryにして保存open失敗を狙うが、実行していない。
- **価値／懸念:** 保存エラー後に変更が誤ってclean扱いされる回帰を検知する。ディレクトリをファイルとして開けないOS挙動に依存する。
- **次に確認:** 実行環境で`QSaveFile`のopen failureとして安定するかを確認し、必要なら明示的に書込不能な親パスを使う。

### 2026-10-11 — RationalTime rescaledTo accepts negative target scales

- **関連:** `ArtifactCore/src/Time/RationalTime.cppm`、`tests/ArtifactCore/RationalTimeContractTest.cpp`。
- **確認できた事実:** `toFrameCount()`はfpsが0以下なら0を返す一方、`rescaledTo()`はtarget scaleが0の場合だけを無効扱いし、負値では負の分子として再スケールする。RationalTimeのconstructorは非正scaleを1に正規化する。
- **仮説／未検証:** `rescaledTo()`のtarget scaleも正値に限り、負値は0または安全な既定値を返すべきと考える。追加caseは0を期待するが実行していない。
- **価値／懸念:** 不正なtarget scaleで時間方向が反転する結果を防ぐ契約候補。ArtifactCore child repoは変更していない。
- **次に確認:** 呼び出し元で負target scaleが意図的に利用されていないことと、無効入力の期待値を確認してから実装修正する。

### 2026-10-11 — ArtifactHashMap moved-from destruction may dereference null buckets

- **関連:** `ArtifactCore/src/Core/ArtifactHashMap.cppm`、`tests/ArtifactCore/ArtifactHashMapContractTest.cpp`。
- **確認できた事実:** move constructorとmove assignmentは移動元の`buckets_`を`nullptr`にし、`bucketCount_`を初期容量に戻す。destructorは常に`clear()`を呼び、`clear()`は`bucketCount_`回`buckets_[i]`を参照する。
- **仮説／未検証:** 別mapへのmove後に移動元が破棄されるとnull pointer dereferenceになる可能性が高い。実行していない。
- **価値／懸念:** move APIの基本的な破棄安全性に関わる。再現テストはstandalone suite全体をクラッシュさせる可能性があるため今回追加しなかった。ArtifactCore child repoは変更していない。
- **次に確認:** move後のsourceへ空bucketを再確保する等、valid destructible invariantを実装者と確認し、安全に失敗を報告できるテスト形態で検証する。

### 2026-10-11 — ShapeVertex mirrorHandles length mode is indistinguishable

- **関連:** `ArtifactCore/include/Core/ShapePoint.ixx`、`tests/ArtifactCore/ShapeVertexContractTest.cpp`。
- **確認できた事実:** `mirrorHandles(bool keepLength)`はtrue/false両分岐で`outHandle_`を`-inHandle_`に置き換えており、既存outgoing handleの長さを読み取らない。リポジトリ内の他の呼び出し・説明は検索で見つからなかった。
- **仮説／未検証:** `keepLength=false`は反対側ハンドルの既存長を維持しながらincoming方向と反対向きにそろえる意図かもしれない。新しい契約caseはこの解釈を期待するが、実装者の意図を示す仕様は見つけていないためテストは未実行。
- **価値／懸念:** UIから選べる長さモードが無効になっている可能性を切り分ける。ArtifactCore child repoは今回変更していない。
- **次に確認:** 呼び出し元または設計仕様で`keepLength`の意味を確定し、期待値を承認してから実装修正と独立suite実行を行う。

### 2026-10-11 — ArtifactTimer can expire before it starts for negative timeouts

- **関連:** `ArtifactCore/include/Core/ArtifactTimer.ixx`、`tests/ArtifactCore/ArtifactTimerContractTest.cpp`。
- **確認できた事実:** `hasExpired(timeoutMs)` はtimer validityを確認せず、`timer_.elapsed() >= timeoutMs` を返す。Qtの無効な`QElapsedTimer`はelapsedが負値となり、既存テストは未startの0 timeoutを未expired、開始済みの負timeoutをexpiredとしている。
- **仮説:** 未start timerではtimeout値にかかわらず未expiredを返すのがAPIとして一貫する。新しい負timeout未startケースは現実装でexpiredになる見込みだが、実行していない。
- **価値／懸念:** start前に待機完了と誤判定する呼び出しを検出できる。実装修正はArtifactCore child repoにあるため今回は行わない。
- **次に確認:** 意図する無効timerの契約を確認し、必要なら`isValid()` guardを加えて独立suiteを実行する。

### 2026-10-11 — FileTypeDetector treats malformed UTF-8 headers as text

- **関連:** `ArtifactCore/src/File/FileTypeDetector.cppm`、`tests/ArtifactCore/FileTypeDetectorContractTest.cpp`。
- **確認できた事実:** The text fallback rejects ASCII control bytes other than tab/LF/CR, but accepts every byte >= 32 without validating UTF-8. A regression case now expects malformed UTF-8 lead/continuation sequences to classify as Binary; it is not executed.
- **仮説:** Since valid UTF-8 Japanese text is already explicitly supported by a test, malformed multibyte sequences should not be classified as text. Current fallback will classify both new fixtures as Text.
- **価値／懸念:** This distinguishes valid Unicode text from arbitrary high-bit binary data. The implementation is in the ArtifactCore child repository, which is not modified here.
- **次に確認:** Confirm the intended accepted encodings (UTF-8 only or UTF-16/legacy as well), implement matching validation in the child repository if authorized, then run the standalone target.

### 2026-10-11 — AssetMetaFile tag removal does not normalize input

- **関連:** `ArtifactCore/src/Asset/AssetMetaFile.cppm`、`tests/ArtifactCore/AssetMetaFileContractTest.cpp`。
- **確認できた事実:** `addTag()` stores `tag.trimmed()`, while `removeTag()` compares each stored tag directly to the untrimmed argument. Added a standalone contract case that adds and removes the same whitespace-padded tag; based on the implementation it is expected to fail, but it was not run.
- **仮説:** Callers that pass the same user-entered tag string to add and remove can leave a tag behind because only the insertion path normalizes whitespace.
- **価値／懸念:** This test catches asymmetric tag matching without editing the child repository. If the intended contract is exact matching for removal, document that explicitly instead of normalizing.
- **次に確認:** Confirm tag matching policy, then fix/remove the regression case with the user-authorized ArtifactCore change and execute the standalone target when allowed.

### 2026-10-11 — ArtifactArray squeeze currently cannot reduce capacity

- **関連:** `ArtifactCore/include/Core/ArtifactArray.ixx`、`tests/ArtifactCore/ArtifactArrayContractTest.cpp`。
- **確認できた事実:** `squeeze()` calls `reserve(size_)`, while `reserve(n)` immediately returns whenever `n <= capacity_`. Thus a non-empty array with spare capacity retains its old allocation. Added a standalone contract case expecting capacity to shrink to size while preserving elements; it was not executed.
- **仮説:** The API name and QVector-like capacity interface imply `squeeze()` should release excess storage; current implementation does not.
- **価値／懸念:** This regression case detects an ineffective public capacity operation. Fixing it requires editing the ArtifactCore child repository and remains outside this test-only scope.
- **次に確認:** With child-repository change authorization, implement allocate/move/release or equivalent safe shrink logic, then run the standalone suite and non-trivial lifetime case.

### 2026-10-11 — ArtifactRandom inclusive maximum overflows its upper bound

- **関連:** `ArtifactCore/include/Core/ArtifactRandom.ixx`、`tests/ArtifactCore/ArtifactRandomContractTest.cpp`。
- **確認できた事実:** integer `between(min, max)` computes `max + 1` in signed `int` before passing it to Qt's bounded generator. Existing boundary tests stop at `INT_MAX - 1`.
- **仮説:** `between(INT_MAX, INT_MAX)` overflows before generation and cannot reliably satisfy the documented-by-tests inclusive range behavior. A singleton-at-maximum regression case was added; it was not executed.
- **価値／懸念:** Highest-end integer boundary is now represented in the independent suite, but the case may fail or invoke undefined behavior until the child implementation handles that endpoint.
- **次に確認:** ArtifactCore-side remediation should generate over a wider unsigned interval or special-case the upper endpoint; then execute this standalone target after explicit authorization.

### 2026-10-11 — ArtifactVariant empty-state destruction dispatch

- **関連:** `ArtifactCore/include/Core/ArtifactVariant.ixx`、`tests/ArtifactCore/ArtifactVariantContractTest.cpp`。
- **確認できた事実:** default construction sets `typeIndex_` to zero and exposes `isSet()==false`, but `destroy()` unconditionally dispatches `vtableForCurrent()` using the first alternative before resetting the index. Destructor and assignment call `destroy()` even for that unset state.
- **仮説:** an unset variant may invoke the first alternative's destructor on storage where no object lifetime began; behavior is especially hazardous when the first alternative is non-trivial. Added tests exercise default/copy/move unset state and assignment clearing an active alternative. They are unexecuted; sanitizers/runtime validation is still needed.
- **価値／懸念:** Empty-state lifecycle coverage can expose undefined destruction behavior hidden by the current first-alternative-is-int fixture. Fixing this requires editing the ArtifactCore child repository, which is outside the authorized scope.
- **次に確認:** With explicit permission for child-repository changes and test execution, guard `destroy()` on `typeIndex_ != 0`, then run the standalone suite under an available sanitizer.

### 2026-10-11 — Tag StringLike setter has an empty implementation

- **関連:** `ArtifactCore/include/Utils/Tag.ixx`、`ArtifactCore/src/Utils/Tag.cppm`、`tests/ArtifactCore/TagContractTest.cpp`。
- **確認できた事実:** `Tag::setName(const S&)` の `StringLike` template body は空である。実装側にある非-template setterは `const UniString&` overloadのみ。`StringLike` は `QString` と `std::string` を含む型を受け入れる。
- **仮説:** `QString` / `std::string` を直接渡す通常の呼び出しは空のtemplate overloadを選び、名前を変更しない可能性が高い。追加した2つのstandalone contract caseはこのAPI意図を固定し、実行時に失敗する見込みだが未実行。
- **価値／懸念:** generic setterを使う呼び出しが黙って無視される不具合を検出できる。修正が必要なら子repository編集禁止ルールに従い、ArtifactCore側の実装修正は別途明示依頼を待つ。
- **次に確認:** `Tag` のStringLike setterの意図と所有repoでの修正可否を確認し、許可後にこのstandalone suiteを実行する。

### 2026-10-11 — Id contract test references undeclared isValid

- **関連:** `ArtifactCore/include/Utils/Id.ixx`、`tests/ArtifactCore/IdContractTest.cpp`。
- **確認できた事実:** 現在の `Id` module interfaceには`isValid()`宣言がなく、テストの`ValidUuidStringRoundTripsInCanonicalForm`は`value.isValid()`を呼び出している。`Id.cppm`にもその定義は見当たらない。
- **価値／懸念:** 現行ソースのままでは独立suiteのコンパイルを妨げる可能性が高い。ビルド／テストは指示により実施していないため、実コンパイル診断は未確認。
- **次に確認:** `isValid()`を追加する設計意図があるか確認し、なければテストを`!isNil()`等の実在する契約に修正する。変更後にユーザー許可を得て独立suiteを実行する。

### 2026-10-11 — Tag StringLike setter template is empty

- **関連:** `ArtifactCore/include/Utils/Tag.ixx`、`ArtifactCore/src/Utils/Tag.cppm`、`tests/ArtifactCore/TagContractTest.cpp`。
- **確認できた事実:** `Tag::setName(const UniString&)` は `impl_->name` を更新するが、`StringLike` を受けるheader template overloadの本体は空。
- **価値／懸念:** `setName(QString/std::string/const char*)` など、`StringLike` conceptを満たす型で呼び出しても名前は変わらない。今回の独立suiteは動作する`UniString` overloadだけを検査し、この未実装挙動は通過テストとして隠さない。
- **次に確認:** template setterで`toQString`して`UniString` overloadへ委譲するか、template APIを削除するか判断した後、各対応文字列型のsetNameを回帰テストする。

### 2026-10-11 — ArtifactDict set calls absent ArtifactHashMap insert

- **関連:** `ArtifactCore/include/Core/ArtifactDict.ixx`、`ArtifactCore/src/Core/ArtifactHashMap.cppm`。
- **確認できた事実:** `ArtifactDict::set` は`map_.insert(key, value)`を呼ぶが、基底`ArtifactHashMap`が公開する挿入APIは`tryEmplace` / `emplace`で、`insert`は定義されていない。
- **価値／懸念:** `ArtifactDict::set`をインスタンス化するコードはコンパイルできない。既存の`ArtifactDict`テストはこの経路を使っており、直接module構成で検証する独立suiteもまだない。
- **次に確認:** ArtifactDictがinsert-or-assignかinsert-onlyか意味を決め、ArtifactHashMapの既存APIへ接続してから独立standaloneでset/get/removeを検証する。

### 2026-10-11 — Callback documentation references missing ifValid API

- **関連:** `ArtifactCore/include/Core/ArtifactCallback.ixx`、`tests/ArtifactCore/ArtifactCallbackContractTest.cpp`。
- **確認できた事実:** Callbackのclass commentは`isValid()`確認または`ifValid()`利用を案内するが、公開メソッドに`ifValid`はなく、`isValid` / `operator bool` / `invoke` / `clear` / `reset`だけがある。
- **価値／懸念:** 利用者が文書の例外安全な呼び出しAPIを探しても利用できず、empty時に直接`invoke`するとempty `std::function`の呼び出しになる。
- **次に確認:** null-safe invocation APIを追加するか、commentを実在APIに合わせる判断後、empty経路の仕様をテストする。

### 2026-10-11 — ArtifactFunction unsafe empty invocation and heap move

- **関連:** `ArtifactCore/include/Core/ArtifactFunction.ixx`、`tests/ArtifactCore/ArtifactFunctionContractTest.cpp`。
- **確認できた事実:** `operator()` は`invoker_`未設定時のguardなしに関数pointerを呼ぶ。heap callableのmoveでは`onHeap_`を先にtrueにした状態で、未初期化の移動先storageを`ptr()`経由で参照する。またmove後のsource callableはdestroyerを呼ばずmetadataだけnull化される。
- **価値／懸念:** 空function invocationは無効な関数pointer呼び出しとなる。heap callable moveは無効なstorage参照となり得て、SBO/heap双方のmoveはmoved-from callableのdestructorを欠落させる。
- **次に確認:** 空invokeを明示的な契約（assert/exception/no-op）にし、heap移動時のpointer transferとsource destructionを正しく実装してから、heap callable move・move assignment・destructor balance・empty invocationのテストを追加する。

### 2026-10-11 — ArtifactSpan end on null empty range

- **関連:** `ArtifactCore/include/Core/ArtifactSpan.ixx`、`tests/ArtifactCore/ArtifactSpanContractTest.cpp`。
- **確認できた事実:** default `ArtifactSpan` は`data_ == nullptr`、`size_ == 0`。`end()` は一律 `data_ + size_` を返す。
- **価値／懸念:** C++のポインタ加算対象となる配列がないnull pointerに対する`+ 0`は、標準上安全な配列内／one-past演算として保証されない。空spanをrange-forなどで走査する経路の扱いを確認したい。
- **次に確認:** empty span用にnullをそのまま返す分岐を設けるか、null pointer rangeをiteratorとして使わない契約にするか決め、default/empty spanのbegin/endとrange iterationを回帰テストする。

### 2026-10-11 — Position2D methods on moved-from objects

- **関連:** `ArtifactCore/include/Core/Position2D.ixx`、`ArtifactCore/src/Core/Position2D.cppm`、`tests/ArtifactCore/Position2DContractTest.cpp`。
- **確認できた事実:** move constructor/assignmentは`impl_`の所有権を移してsourceを`nullptr`にする。`Set` / `Get` はnull確認せず`impl_`を参照する一方、destructorはnull-safeな`delete`となる。
- **価値／懸念:** moved-from objectは破棄可能だが、再設定や読み出しはクラッシュする。通常のmoved-from型に期待される再代入・再利用可能性は満たさない。
- **次に確認:** moved-from状態を空として扱うか、有効なゼロ値へ再初期化するかを決め、move後のGet/Setと再代入の契約を追加する。

### 2026-10-11 — ArtifactPtr WeakPtr copy assignment ownership count

- **関連:** `ArtifactCore/include/Core/ArtifactPtr.ixx`、`tests/ArtifactCore/ArtifactPtrContractTest.cpp`。
- **確認できた事実:** `WeakPtr` はcopy constructorでweak countを増やすが、copy assignmentを定義していない。コンパイラ生成assignmentはcontrol-block pointerをmemberwise copyするだけなので、既存weak ownerへの代入でcountを適切に保持・解放しない。
- **価値／懸念:** 一時`WeakPtr`からの代入では一時objectのdestructorがcountを減らし、代入先が解放済みcontrol blockを参照する可能性がある。テストは未定義寿命状態を踏まないよう代入を使用しない。
- **次に確認:** copy/move assignmentの所有権契約を定め、weak countを保持する実装と同一control blockへのself-assignmentを回帰テストする。

### 2026-10-11 — ArtifactPtr UniquePtr reset with its current pointer

- **関連:** `ArtifactCore/include/Core/ArtifactPtr.ixx`、`tests/ArtifactCore/ArtifactPtrContractTest.cpp`。
- **確認できた事実:** `UniquePtr::reset(raw)` は先に `ptr_ = raw` を設定した後、元の `old` を削除する。そのため現在所有中の同じポインタを渡すと、所有値を解放した後にdangling pointerを保持する。
- **価値／懸念:** 同一アドレスへのresetはuse-after-free/double-deleteにつながり得る。
- **次に確認:** self-resetをno-opとする契約を定めて実装を修正し、同一pointerを渡した後の値アクセスと破棄回数を回帰テストする。

### 2026-10-11 — ArtifactThread condition notification synchronization

- **関連:** `ArtifactCore/include/Core/ArtifactThread.ixx`、`tests/ArtifactCore/ArtifactThreadContractTest.cpp`。
- **確認できた事実:** `Cond::wait(Lock&)` は渡された`Lock`のmutexを解放せず、別の`cv_lock_`でcondition variableを待つ。一方、`wakeOne/wakeAll`は`notified_`をmutex lockなしで書き換える。
- **価値／懸念:** wait前後のpredicate保護が呼び出し側mutexと同期せず、通知のlost wakeや`notified_`のdata raceを起こし得る。
- **次に確認:** `Lock`から待機中にmutexを解放／再取得できる設計にし、predicateを同一mutex下で確認するAPI契約を確定してrace・lost-wake回帰ケースを加える。

### 2026-10-11 — ArtifactOptional empty/present swap and empty initializer list

- **関連:** `ArtifactCore/src/Core/ArtifactOptional.cppm`、`tests/ArtifactCore/ArtifactOptionalContractTest.cpp`。
- **確認できた事実:** `ArtifactOptional<T>::swap` の片側だけpresentの分岐はempty側のunion memberへplacement constructionせずmove assignmentを行う。initializer-list constructorは空listでも`hasValue_`をtrueにし、contained `T`を構築しない。
- **価値／懸念:** empty/present swap後のdestructorは未構築`T`を破棄し、empty initializer-list optionalも同じ不正状態となる。
- **次に確認:** 片側presence swapはplacement move constructionを行う状態遷移に修正し、empty initializer-listはemptyとする契約を定めてそれぞれの回帰テストを追加する。

### 2026-10-11 — ArtifactMath signed absolute minimum overflow

- **関連:** `ArtifactCore/include/Core/ArtifactMath.ixx`、`tests/ArtifactCore/ArtifactMathContractTest.cpp`。
- **確認できた事実:** `artifactAbs` は負値を unary `-value` で正値化する。符号付き整数の最小値は対応する正値を同じ型で表せず、評価は符号付きoverflowになる。
- **価値／懸念:** `artifactAbs(INT_MIN)`等は安全な絶対値を返さない。
- **次に確認:** APIの戻り型を広げるか飽和させるか仕様を定め、整数最小値の回帰ケースを決める。

### 2026-10-11 — ArtifactRandom integer upper-bound overflow

- **関連:** `ArtifactCore/include/Core/ArtifactRandom.ixx`、`tests/ArtifactCore/ArtifactRandomContractTest.cpp`。
- **確認できた事実:** 整数版`between(min, max)`はinclusive化のため`max + 1`を`int`で計算してQt APIへ渡す。
- **価値／懸念:** `max == INT_MAX`の場合に符号付き整数overflowとなり、定義済み動作ではない。
- **次に確認:** 上限inclusive要件を保てる範囲幅APIまたは64-bit中間値による実装へ変更し、`INT_MAX`近辺を含む境界回帰ケースを追加する。

### 2026-10-11 — ArtifactVariant destroys storage while empty

- **関連:** `ArtifactCore/include/Core/ArtifactVariant.ixx`、`tests/ArtifactCore/ArtifactVariantContractTest.cpp`。
- **確認できた事実:** `destroy()` は `typeIndex_ == 0` の空状態でも index 0 の vtable を取得して destroyer を呼ぶ。index 0 は第1 alternativeであり、既定構築 variant の storageにはその型のオブジェクトが構築されていない。
- **価値／懸念:** 第1 alternativeが非自明デストラクタを持つ場合、空variantの破棄や値を持つvariantから空variantへの代入で未構築オブジェクトに対して破棄処理を行う。
- **次に確認:** `destroy()` は `isSet()` の時だけ破棄関数を呼ぶようにし、空状態の生成・破棄と値から空へのcopy assignmentを回帰テストする。

### 2026-10-11 — ArtifactAlgorithms erase API does not match ArtifactArray

- **関連:** `ArtifactCore/include/Core/ArtifactAlgorithms.ixx`、`ArtifactCore/include/Core/ArtifactArray.ixx`、`tests/ArtifactCore/ArtifactAlgorithmsContractTest.cpp`。
- **確認できた事実:** `artifactRemoveIf` は削除時に`container.eraseAt(index)`を呼ぶ。一方、`ArtifactArray`が提供する削除APIは`removeAt(index)`で、`eraseAt`は定義されていない。
- **価値／懸念:** `artifactRemoveIf(Array<T>&, ...)` は要求メンバーが一致せず直接利用できない。suiteではAPIどおり`eraseAt`を持つ小fixtureを介してアルゴリズム単体を検査する。
- **次に確認:** `artifactRemoveIf` を`removeAt`コンテナへ一般化するか、`ArtifactArray`へaliasを設けるか決めて、実際のArray統合テストを追加する。

### 2026-10-11 — ArtifactHashMap collision iteration and moved-from lifetime

- **関連:** `ArtifactCore/src/Core/ArtifactHashMap.cppm`、`tests/ArtifactCore/ArtifactHashMapContractTest.cpp`。
- **確認できた事実:** iteratorの`operator++()`はbucket indexを進め、現在Nodeの`next`を辿らないため、同一bucket内の複数Nodeをiterationで全件訪問しない。move constructor/assignmentは移動元のbucket pointerをnullにするが、destructorの`clear()`はbucket配列をdereferenceする。また、`const_iterator`はmutable `iterator` のaliasでconst mapの`find()`もmutable referenceを返す。`tryEmplace`重複時は既存mapped valueへ新valueを代入してfalseを返す。
- **価値／懸念:** collision iterationの取りこぼし、move元destruction時のnull dereference、const map経由の変更、try-emplace名称から期待される既存値非変更との不一致がある。
- **次に確認:** iteratorがchainとbucket遷移の両方を扱うよう修正し、const iteratorを読み取り専用にし、move元を空の有効状態にする。重複時のtryEmplaceを非変更契約にするか決める。回帰テストは修正後にcollision iteration、const性、重複値保持、move元スコープ終了を含める。

### 2026-10-11 — ArtifactArray squeeze currently cannot reduce capacity

- **関連:** `ArtifactCore/include/Core/ArtifactArray.ixx`、`tests/ArtifactCore/ArtifactArrayContractTest.cpp`。
- **確認できた事実:** `squeeze()` は `size_ < capacity_` の場合に `reserve(size_)` を呼ぶが、`reserve()` は要求容量が現在容量以下なら即returnする。従ってstorage容量は減らない。
- **価値／懸念:** 名前と一般的なcontainer契約から期待される容量縮小が起きず、過剰確保メモリを保持する。
- **次に確認:** 容量を縮める再配置処理が必要か判断し、必要なら要素move/destructionとallocation failure時の保証を定めた後でcapacity assertionを追加する。

### 2026-10-11 — AudioEngineProfiler reset retains last frame counts

- **関連:** `ArtifactCore/include/Utils/PerformanceProfiler.ixx`、`tests/ArtifactCore/PerformanceProfilerContractTest.cpp`。
- **確認できた事実:** `resetStats()` はcallback/fill counters、durations、underflow、buffer levelを0にするが、`lastCallbackFrames_` と `lastRequestedFrames_` はresetしない。
- **価値／懸念:** reset後snapshotが過去のframe数と新しい累積統計を混在させる。これが意図的なlast-known metadataか、reset漏れかは未検証。
- **次に確認:** reset後もlast-known値を保持する契約か決め、仕様がzero resetなら両atomicもresetして期待値を追加する。

### 2026-10-11 — PatternNameGenerator without a number slot can loop forever

- **関連:** `ArtifactCore/src/Utils/NameGenerator.cppm`、`tests/ArtifactCore/NameGeneratorContractTest.cpp`。
- **確認できた事実:** `Generate` はcandidateが `usedNames` に存在する限り番号を増やして再試行するが、`makeCandidateZero` はpatternに `***` がなければ候補文字列を変化させない。同じ固定文字列が使用済みになった後の再生成は終了しない。
- **価値／懸念:** 固定patternやname slotだけのpatternを複数回使うと呼び出しが停止する可能性がある。再試行上限なしのループであり、通常のテストではプロセスをハングさせうる。
- **次に確認:** number slotがないpatternの重複方針（エラー、同名返却、pattern妥当性検証）を決め、boundedな失敗契約を実装した後に回帰テストを追加する。

### 2026-10-11 — MultipleTag moved-from state is not reusable

- **関連:** `ArtifactCore/src/Utils/MulitpleTag.cppm`、`ArtifactCore/include/Utils/MultipleTag.ixx`。
- **確認できた事実:** move constructor と move assignment は移動元の `impl_` を `nullptr` にする。destructor は許容するが、`count()`、`clear()` などの通常メソッドは `impl_` を直接参照する。
- **価値／懸念:** 移動元を破棄以外に使うと null dereference になる。クラスの moved-from 契約が未定義であり、利用側が再利用可能と期待する場合は問題になる。
- **次に確認:** moved-from 状態を空の有効状態へ戻すか、型の契約として明示するかを決めてから、move 後再利用テストを追加する。

### 2026-10-11 — Buffer view lifetime and missing-delimiter edge

- **関連:** `ArtifactCore/include/Utils/Buffer.ixx`、`tests/ArtifactCore/BufferContractTest.cpp`。
- **確認できた事実:** `toBufferView(const QString&)` はローカル `QByteArray` のdata pointerを `BufferView` として返すため、戻り値はローカル所有物の破棄後を参照する。`afterLast` は区切りがないと `splitLast` が `{empty, value}` を返し、非empty側の先頭1byteを除いたviewを返す。
- **価値／懸念:** QString変換のviewはuse-after-lifetimeとなる。delimiter未検出時の `afterLast` は命名上期待される意味とずれる可能性がある（期待仕様は未確認）。独立suiteではこれらの不安定な期待値を固定せず、テスト対象から外した。
- **次に確認:** Buffer view APIで所有権が期待されるか、または呼び出し側所有のUTF-8作業領域を受け取る形にするかを確認する。`afterLast`／`takeAfter`／`suffixAfter` の delimiter未検出仕様を定めて境界テストを追加する。

未着手の設計判断、現在の優先方針に直結する実装候補、実機検証待ちだけを記録する。実装済みの詳細履歴と過去の調査は [Insight Archive (through 2026-09-01)](docs/analysis/INSIGHT_ARCHIVE_2026-09-01.md) を参照。

### 2026-10-10 — 2D/3D layer transform composition order

- **関連:** `ArtifactCore/src/Transform/TransformHelper.cppm`、`tests/ArtifactCore/CompositionTransformTimeContractTest.cpp`。
- **確認できた事実:** `combineLayerTransform2D(local, parent)` は `local * parent` を返す一方、3D 版は `parent * local` を返す。2D 実装コメントは親変換を子空間で適用すると記述し、3D 実装は `QMatrix4x4` の標準的な親×子順を使っている。
- **価値／懸念:** 2D と 3D の規約差が意図的か、ネストしたレイヤーの描画・座標変換で整合しているかは未検証。単体テストでは各 API の現在の契約を固定できるが、親子レイヤーの実描画結果までは保証しない。
- **次に確認:** 親子レイヤーを使う描画・bounds・入力座標の各経路で変換積の順序を追い、同じ階層構成の 2D/3D テストを追加すべきか判断する。

### 2026-10-10 — XPU CPU lanes share oneTBB through Core.Parallel

- **関連:** `Artifact/src/Render/ArtifactRenderQueueService.cppm`、`ArtifactCore/src/Core/Parallel.cppm`、`ArtifactCore/include/Common/ThreadPool.ixx`。
- **確認できた事実:** XPU mixed の CPU frame lanes はソフトウェア renderer を通り、複数の画像処理が `ArtifactCore::Parallel::For` を呼ぶ。同 API は `tbb::parallel_for` に委譲される。既存 `ThreadPool` は `tbb::global_control(max_allowed_parallelism, concurrency_)` を作り、TBB の global control は scheduler 内で同時実行する worker thread 数を制限する。TBB の implicit arena は parallel algorithm を呼ぶ user thread ごとに関連付く ([task arena](https://oneapi-spec.uxlfoundation.org/specifications/oneapi/latest/elements/onetbb/source/task_scheduler/task_arena/task_arena_cls)、[global control](https://oneapi-spec.uxlfoundation.org/specifications/oneapi/latest/elements/onetbb/source/task_scheduler/scheduling_controls/global_control_cls))。
- **今回の対応:** `Core.Parallel` に共有 TBB arena API を置き、全 `Parallel::For` と XPU CPU frame lanes を同じ arena に通す。XPU は初回 arena 初期化へ process-available thread count を渡し、job host limit を実 concurrency 以下へ上限化する。MSVC module compile で `ThreadPool.ixx` の TBB inline実装を Artifact から使う形が失敗したため撤回し、TBBの実装詳細を既存 `Core.Parallel.cppm` 内へ閉じた。XPU summary と統合テスト source に scheduler と concurrency の確認を加えた。
- **価値／懸念:** XPU CPU frame lanes と内側の TBB kernels が共有 concurrency cap を使う。Taskflow、QThreadPool、GPU submission、writer/encoderとの完全な process-wide budget は未統合。Debug構成でCore/Artifact/test targetがビルド成功し、RTX 4070 Ti Vulkan + CPUのmixed-text、2D/3D particle、10-frame all-adapters/backpressureテストがそれぞれ3回連続成功。共有TBB arena/concurrencyのdiagnosticsも確認済み。複数物理GPU/iGPUでの受入は未確認。
- **次に確認:** ArtifactCoreとArtifactの対象ターゲットをビルドし、XPU mixed runtimeでCPU frame lanesとnested `Parallel::For` が同一arenaに参加すること、summaryの実 concurrency、TBB worker数とprocess CPU utilizationを検証する。

### 2026-10-10 — XPU D3D12 requires matching Agility runtime exports

- **関連:** root `CMakeLists.txt` Windows SDK `try_compile` paths、`Artifact/src/Render/DiligentDeviceManager.cppm`、`Artifact/tests/RenderQueueLayerImageIntegrationTest.cpp`。
- **確認できた事実:** CMake `D3D12_SUPPORTED` と `VULKAN_SUPPORTED` は両方TRUE。ArtifactはAgility SDK 619 のversion/pathを無条件exportしていたが、SDK `D3D12Core.dll` が配置されない構成では `D3D12_ERROR_INVALID_REDIST` (`0x887E0003`) でdevice creationが拒否される。テスト機は Intel Core i7-13700KF と NVIDIA GeForce RTX 4070 Ti の1 GPU構成で、iGPUは列挙されない。
- **今回の対応:** Agility SDK runtimeがCMakeで設定された場合だけversion/pathをexportする。SDKなし構成ではOS D3D12 runtimeへ戻す。新しいDebug treeでRender Queue統合テストを実行し、CPU＋D3D12 primaryの混在10-frame dispatch、D3D12 backend選択、画像内容を確認。自動backend選択のCPU＋Vulkan混在画像テストも成功。
- **価値／懸念:** D3D12 primaryを利用できる構成が復旧した。Agility SDKを設定した構成は未検証。現ホストではiGPUおよび複数物理GPUを使った同時レンダリングを受け入れられない。
- **次に確認:** Agility SDK 619を明示設定したruntimeと、iGPU＋dGPUまたは複数dGPUの実機で、各GPU laneの参加・フレーム数・画像出力を検証する。

### 2026-10-10 — Particle XPU parity uses different rasterizers

- **関連:** `Artifact/src/Generator/ArtifactParticleGenerator.cppm`、`ArtifactCore/src/Graphics/ParticleRenderer.cppm`、`tests/Artifact/RenderQueueLayerImageIntegrationTest.cpp`。
- **確認できた事実:** software particle rendering paints a QPainter radial-gradient circle sprite. GPU particle rendering draws an instanced quad and computes a soft circular alpha edge with a shader `smoothstep`; the rasterizers differ.
- **今回の対応:** isolated XPU particle Render Queue test verifies deterministic CPU reference output, XPU frame outputs, plan diagnostics, and both 2D/3D layer setup. The GPU-only production particle test separately verifies visible pixels. CPU/GPU lane participation is asserted in the separate multi-frame text case.
- **価値／懸念:** exact CPU/GPU particle parity remains unverified; the XPU case records mismatch count/bounds without guessing a tolerance. Both XPU particle and GPU-only visibility cases passed three consecutive runs.
- **次に確認:** capture and compare XPU particle pixels across different GPU vendors/backends; do not call parity complete from manager output alone.

### 2026-10-10 — XPU pull dispatch adapts frame share without static weights

- **関連:** `Artifact/src/Render/ArtifactRenderQueueService.cppm`、XPU `nextFrameCounter` worker loop。
- **確認できた事実:** mixed/multi-GPU lanes receive one distinct startup frame, then claim the next frame only after `renderOneFrame` completes. There is no fixed per-device frame quota after seeding.
- **今回の対応:** documented the completion-driven pull behavior next to the shared counter and in P4. Per-device timing weights remain diagnostic rather than being fed back into assignment.
- **価値／懸念:** fast lanes naturally claim more remaining frames, so a stale startup weight is unnecessary and could bias work after scene/device conditions change. This does not prove there is no starvation under long-tail frames or output-buffer backpressure.
- **次に確認:** use runtime bench frame counts and lane render durations on dissimilar CPU/iGPU/dGPU hardware. Add a weighted policy only if the pull scheduler shows a repeatable imbalance that lowers throughput.

### 2026-10-10 — Render Queue の HW encoder slot は現行の単一job実行では飽和しない

- **関連:** `Artifact/src/Render/ArtifactRenderQueueService.cppm`、`Artifact/src/Render/ArtifactRenderQueueEncoder.cppm`。
- **確認できた事実:** `ArtifactRenderQueueService::startAllJobs()` は `isRendering_` で同一serviceの二重開始を拒否し、単一 `workerThread_` 内で `jobOrder` を順番に処理する。各動画jobのencoder backendはそのjob処理中に作成される。
- **今回の対応:** `ARTIFACT_XPU_MAX_HW_ENCODERS` の共有slotをNative HW / ffmpeg HW pipe / Vulkan pipeへ適用し、session lifetime中の上限をコード化した。
- **価値／懸念:** 将来job並列化または独立encoder利用が入った際のプロセス共通上限を先に確立できる。ただし現行Render Queue単体では複数jobが同時にencoderを開かないため、設定値の差によるthroughput/backoffのruntime効果はまだない。今回の変更だけでXPU並列性能が改善したとは扱わない。
- **次に確認:** encoder backendを呼び出す他の同一process経路とservice構築可視性を確認し、競合可能なconsumerがなければ、job並列化導入時に同時encoder上限の受入試験を追加する。

### 2026-10-10 — XPU consumer worker と RenderQueue writer 上限の統合

- **関連:** `Artifact/src/Render/ArtifactRenderQueueService.cppm`、`ArtifactCore/src/IO/Image/AsyncImageWriterManager.cppm`。
- **確認できた事実:** XPU混在時はCPU/GPU frame worker上限とは別に画像書き込みfutureが生成されていた。`AsyncImageWriterManager` は既定hardware_concurrencyサイズのthread poolを作り、enqueue件数を呼び出し側へ通知・制限するAPIがない。
- **今回の対応:** RenderQueue側のasync writer futureを1〜2件に固定し、XPU mixed jobではその同時writer数をhardware thread数から予約してframe lane上限を決める。6 logical threads未満ではasync writerを止めて同期書き込みへ戻す。job summaryへ共有枠を出し、mixed画像統合テストに枠の包含関係を追加。manager指定時はbounded future経路へfallbackし、理由を警告する。ArtifactCoreは変更していない。
- **価値／懸念:** mixed job内のCPU render laneとsequence writer threadが別々に上限を消費しない。これはjob-localな保守的予算であり、UI・TBB・他poolを含むprocess-wide schedulerではない。CPU budgetの効果、writer失敗時の挙動は実行未確認。
- **次に確認:** ユーザー許可後に混在レンダー＋async sequenceの低論理CPU数／通常CPU数条件で、summaryのthread枠、キャンセル、書込失敗伝播、長時間メモリ上限を確認し、render/consumer総CPU予算をbenchで確定する。

### 2026-10-10 — XPU mixed video encoder もframe laneのhost budgetを使う

- **関連:** `Artifact/src/Render/ArtifactRenderQueueService.cppm`、`Artifact/src/Render/ArtifactRenderQueueEncoder.cppm`、`RenderQueue/XpuEncoderThreads`。
- **確認できた事実:** native FFmpeg settingsは`threadCount=0`のままencoderへ渡しており、external FFmpeg pipeにも`-threads`指定がなかった。video consumerはframe workerと並行して動く。
- **今回の対応:** mixed動画ではencoder thread数の既定を1にし、正数指定もusable host thread数へclampする。service側は同数をframe laneの予算から控除し、nativeおよびpipe backend双方へthread countを渡す。
- **価値／懸念:** frame renderingとencodeが同じCPU資源を超過予約しにくくなる。混在jobのthread枠であり、アプリ全体のpoolを横断してはいない。FFmpegが実際にそのthread上限を守ること、画質・出力と速度への影響は未検証。
- **次に確認:** software/hardware native backendとexternal pipeで、混在動画のsummary予約数・encoder起動ログ・実プロセスthread数を確認し、thread数別benchを行う。

### 2026-10-10 — XPU mixed eligibility must include the selected render backend

- **関連:** `Artifact/src/Render/ArtifactRenderQueueService.cppm`、`xpuMixedEligibleForJob`。
- **確認できた事実:** CPU backendを選んだjobでは`cpu-render-backend-selected`をmixed unavailable reasonとして記録していた一方、eligible predicate自体にはGPU backend条件がなく、CPU/GPU lane計画と予算予約へ進む余地があった。
- **今回の対応:** `useGpuBackend`をeligibility条件に加え、実際にGPU rendererが使われるjobだけをXPU mixedとして扱う。Render Queue画像統合テストにCPU backend baselineのunavailable reason、未起動mixed lane、未予約budgetを検査するassertionを追加した。
- **価値／懸念:** unavailable reasonとplan/CPU予算の分類が一致する。統合テストは未実行で、CPU backendを明示した mixed 要求のruntime summaryは未確認。
- **次に確認:** GPU backend有効／CPU backend明示の両ケースで、active mixed plan、frame lane数、writer/encoder予約が期待通り切り替わることを確認する。

### 2026-10-10 — XPU thread budget should use process-available CPUs

- **関連:** `Artifact/src/Render/ArtifactRenderQueueService.cppm`、`Artifact/src/Render/ArtifactRenderQueueEncoder.cppm`。
- **確認できた事実:** XPU lane/encoder budgetは`std::thread::hardware_concurrency()`でmachine-wide logical thread数を参照していた。Qtの`QThread::idealThreadCount()`は、OSが対応する場合にprocessが利用可能なlogical processor数を返し、affinityやCPU hotplugで変化し得る。
- **今回の対応:** serviceとencoderの予算計算を`QThread::idealThreadCount()`へ統一し、processに適用されたCPU制限を反映する。
- **価値／懸念:** affinity/container/job objectなどの制約が公開される環境で、実際に使えないCPU数を予算へ数えない。OSごとの検出範囲とjob開始後の変化はruntime未確認。
- **次に確認:** affinityを制限した環境でsummaryのhardwareThreadsが利用可能processor数に一致すること、同一jobのencoder/service両方が同じ数を使うことを確認する。

### 2026-10-10 — 既存Schedulerはdevice-affineなXPU frame laneを表現しない

- **関連:** `Artifact/src/Render/ArtifactRenderScheduler.cppm`、`ArtifactCore/include/Render/MFR/MFRDispatcher.ixx`、`Artifact/src/Render/ArtifactRenderQueueService.cppm`。
- **確認できた事実:** `ArtifactRenderScheduler` は優先度・重複排除付きの汎用RenderTaskとTaskSystemを持ち、device/renderer affinityをタスクに渡すAPIが見当たらない。`MFRDispatcher` のFrameTaskもframe番号だけを受け取り、実行worker identityをcallbackへ返さない。現在のGPU laneは各immediate context・cache・composition snapshotを単独所有する。
- **設計仮説（未検証）:** 汎用Schedulerへ現状のframe callbackをそのまま移すと、どのGPU laneが実行中かを外部状態で割り当てる必要が生じ、context所有権やin-flight上限が壊れる可能性がある。XPU dispatcher APIはnode/worker identity付きのclaim/completeを提供する必要がある。
- **価値／懸念:** 既存Schedulerの名目上の再利用でGPU資源所有を弱めるリスクを避けられる。一方、現行のローカルatomic採番とraw thread管理はRenderQueueに残る。
- **次に確認:** P2 runtime受入後、device-affine worker descriptorを持つAPIが現在の枠内で追加できるか、シグナル／スロットを増やさず段階的に統合できるかを設計レビューする。

### 2026-10-10 — XPU assist と consumer pipeline は実際の重複実行が必要

- **関連:** `Artifact/src/Render/ArtifactRenderQueueService.cppm`、XPU P3/P4。
- **確認できた事実:** `igpu=assist` のpreview縮小と動画 `pipeline=on` は、CPU処理を `std::async` で開始して同じフレーム内ですぐ `get()` しており、iGPU処理でもencodeとの重複でもなかった。GPU assist plan node は現時点で `maxInFlight=0`。
- **今回の対応:** 誤解を招く即時待機のasync処理を撤去し、動画pipeline要求は非対応警告を出して同期経路を使うようにした。マイルストーン文書も未実装に訂正した。さらに、専用D3D12 workerに偏っていたGPU frame数の観測へprimary rendererのadapter IDとframe数を加えた。
- **価値／懸念:** 「async」指定だけでXPU利用を誤認せず、hot pathで余計なfuture/threadを生成しない。iGPU assistそのものと動画のbounded lookahead pipelineは未実装。
- **次に確認:** GPU資源で実行可能な既存convert/scale/encode経路とrenderer/device ownershipを特定し、assistの一作業を実GPUへ割り当てる。動画pipelineはbounded lookaheadで次フレーム変換と現フレームencodeを実際に重ねる。primaryとdedicated GPU laneを同時に使うfallback caseのbench counter整合は未検証。

### 2026-10-10 — XPU 開発スイッチを ArtifactStartup.json から解決する

- **関連:** `Artifact/src/Render/ArtifactRenderQueueService.cppm`、`Artifact/src/Render/ArtifactRenderQueueEncoder.cppm`、`docs/technical/STARTUP_FLAGS_CONTRACT_2026-09-29.md`。
- **確認できた事実:** Startup Flags 契約は XPU 系環境変数を `RenderQueue/Xpu*` へ段階移行する計画だが、Render Queue と encoder は環境変数だけを直接読んでいた。また GUI 起動では `ArtifactAppSettings::instance()` が `QApplication` と Startup JSON 読込より先に呼ばれていた。
- **今回の対応:** XPU spec、worker上限、iGPU参加、preview、sequence、parity、pipeline、bench、clone診断、encoder thread/preset/HW上限の値を `LayeredConfigStore` 起点へ移した。JSON があれば対応 env より優先し、各 setting accessor は解決値を起動後初回にキャッシュする。`AppMain` の GUI 起動順を変更し、Startup JSON を最初の `ArtifactAppSettings` 取得前に読み込む。テンプレートに任意keyの一覧を記載。
- **価値／懸念:** IDEやExplorer起動でもXPUを設定でき、解決層の読み込みをrender frame hot pathへ持ち込まない。StartupFlags競合時のログと全keyの型・優先順位はruntime未検証。
- **次に確認:** XPU各keyについて環境変数のみ／JSONのみ／JSONとenv競合／未設定の4条件で解決値を確認する。ビルド・テストはユーザー明示指示待ち。

### 2026-10-10 — XPU GPU worker の初期化失敗を adapter 単位で隔離

- **関連:** `Artifact/src/Render/ArtifactRenderQueueService.cppm`、`GpuFinalWorker` 初期化。
- **確認できた事実:** worker 構築ループは composition snapshot または adapter renderer の初期化に失敗したとき、既に作成済みの全 GPU worker も `workersReady` gate で無効化していた。
- **今回の対応:** 失敗した adapter の worker だけを skip し、他の GPU worker は保持する。CPU fallback snapshot が利用できなくても GPU renderer 自体は登録する。混在時の最低1 worker条件と legacy multi-GPU時の最低2 worker条件は維持する。
- **価値／懸念:** 1台の不調 adapter が他の正常な GPU lane を無効にしない。fallback snapshot がない GPU lane は GPU render 失敗時にCPUへ切り替えられない。
- **次に確認:** adapter 初期化失敗・snapshot clone失敗を個別に注入し、残存 worker のフレーム出力、fallback、summary計数を確認する。ビルド・テストはユーザー明示指示待ち。

### 2026-10-10 — XPU CPU lane の snapshot clone 部分失敗を縮退扱いにする

- **関連:** `Artifact/src/Render/ArtifactRenderQueueService.cppm`、XPU mixed CPU worker 初期化。
- **確認できた事実:** CPU composition snapshot の作成ループは一つでも clone に失敗すると `mixedCpuReady=false` にし、すでに正常作成した snapshot がある場合も mixed dispatch を有効にしなかった。
- **今回の対応:** 失敗を警告して clone ループを止め、作成済み snapshot が一つ以上あればその数だけ CPU lane を登録する。CPU lane が0ならGPU側の既存dispatchへ縮退する。
- **価値／懸念:** 一時的なclone失敗で既に確保したCPU資源を全て失わず、実際に初期化できたlane数を超えて起動しない。
- **次に確認:** clone failure を注入し、要求／実稼働 lane 数、順序復元、フレーム成功数が縮退後の構成と一致することをruntimeで確認する。ビルド・テストはユーザー明示指示待ち。

### 2026-10-10 — Kuwahara の近傍統計を一走査で集計する

- **関連:** `Artifact/src/Effects/Kuwahara/KuwaharaEffect.cppm`。
- **確認できた事実:** CPU と HLSL の両方で、各 quadrant の平均用と分散用に同じ近傍画素を別々に走査・読込していた。GPU shader は1画素につき4 quadrantを処理する。
- **今回の対応:** RGBの二乗和も平均と同時に集計し、`E[x²] - E[x]²` から分散を得る形へ変更。CPU と HLSL を揃えた。負の丸め誤差は0へクランプする。CPU 側のquadrant loopは0／負のoffsetで終端へ進まず走査が継続する向きだったため、GPU と同じ画素範囲を進むよう修正。
- **価値／懸念:** 近傍テクスチャ読込・CPU画素読込を理論上およそ半減できる。二乗和方式は従来の差分二乗方式と浮動小数点丸めが異なり、HDR値やほぼ一様な領域では分散選択に差が出る可能性がある。速度と画像差は未検証。
- **次に確認:** ビルド後、CPU/GPU pixel parity と半径別の実行時間を比較する。ビルド・テストは明示指示待ち。

## 現在の優先検証

### 2026-10-10 — ArtifactHashMap::operator[] が既存値を黙って消していた

- **関連:** `ArtifactCore/src/Core/ArtifactHashMap.cppm`、`ArtifactCore/src/Composition/CompositionRegistry.cppm`、`tests/ArtifactCore/CompositionTransformTimeContractTest.cpp`。
- **確認できた事実:** `operator[]` が `tryEmplace(key)`（空パックの可変長 overload）に転送され、キー存在時に `curr->data.second = V()` でデフォルト上書きして返していた。`NameMap::operator[]` 経由の読み取りは全て破壊読みだった。実害：`CompositionRegistry::findComposition()` は `contains()` が真でも `entries[key]` で nullptr に潰して返していた（テストで再現・確定）。script host 用の名前解決が全滅していたことになる。
- **今回の対応:** 存在確認→既存参照返却、不存在時のみ `tryEmplace(key, V())` で挿入に修正。単引数 `tryEmplace` の他利用者はなし。回帰用に NameMap 直接試験を残した。
- **価値／懸念:** 全 NameMap/HashMap 利用者の読み取りが正しくなる。逆に今まで「読むと消える」挙動に依存していたコードがあれば顕在化する。既存ビルド済み 14 件は全パス。
- **次に確認:** フルスイート（ArtifactApp 含む）のビルド・実行は未実施。`ArtifactProjectRoundTripTest`（tests/Artifact、ArtifactAppRuntime 要）は未ビルド。

### 2026-10-10 — RenderGraph に executionLevels と executeParallel を追加した

- **関連:** `ArtifactCore/include/Graphics/RenderGraph.ixx`、`tests/ArtifactCore/RenderGraphTest.cpp`。
- **確認できた事実:** `compile()` の Kahn 順序はレベル非減少なので、伝播した level 配列でグルーピングすると並列実行可能なバッチになる。既存 `execute()` の逐次語義は不変。`allocationSlot` / `lifetimes` は算出のみで backend の消費者がいない（未検証ではなく grep で確認済み）。
- **今回の対応:** `CompiledRenderGraph::executionLevels` を追加し、ダックタイピングの launcher（`async(F)->future<bool>`、`Core.TaskSystem` が適合、RenderGraph 側に新規依存なし）でレベル同期実行する `executeParallel` を実装。単一パスレベルはインライン実行で launcher に触らない。単体テスト 15 件（順序・cycle 到達不能ではなく除外・alias 共有/分離・失敗伝播・診断）を追加。cycle は宣言順 append の API では到達不能なためテスト対象外とした。
- **価値／懸念:** 現行グラフは全て線形チェーンのため、フレームパスへの配線は効果ゼロ＋リスクのみ。配線は分岐グラフが出てから。真の並列レンダリングにはパケット構築自体のスレッド分割が別途必要。
- **次に確認:** ユーザーの明示指示後、テストのビルド・実行で検証する。ビルド・テストは明示指示待ち。

### 2026-10-10 — 診断グラフへ層 raster/mask/blend チェーンを載せた

- **関連:** `Artifact/src/Widgets/Render/ArtifactCompositionRenderController.cppm`（`frameDebugSnapshot` の diagnosticGraph）。
- **確認できた事実:** 診断スナップショットのグラフは層ごとの raster が単一 `layerResource` への書き込み羅列＋単一 blend で、マスク段が存在しなかった。実行は変えず記述だけ変える余地がある（診断専用のため）。
- **今回の対応:** 層ごとに raster→mask→blend のチェーンを登録。raster は共有 scratch への書き込み（WAW 辺で直列化され実実行と一致）、mask は enabled matte 参照がある層のみ、`blend` は層ごとに accum 共有トークンへ。`FunctionalRenderPass` 実体は作らず記述子のみで、per-frame の `std::function` 確保は増やさない。空 comp 時は accum 書込維持の Empty blend を残し、下流 effect パスの read-before-write 失敗を避けた。
- **価値／懸念:** FramePipelineViewWidget で層・マスクの有無が可視化され、カリング漏れが目視できる。実行順序・性能は不変。
- **次に確認:** ユーザーの明示指示後、診断スナップショットの表示確認とテストのビルド・実行。ビルド・テストは明示指示待ち。

### 2026-10-10 — Layer2D は未定義メソッドのためテストから除外した

- **関連:** `ArtifactCore/include/Transform/StaticTransform2D.ixx`、`ArtifactCore/src/Layer/Layer2D.cppm`、`tests/ArtifactCore/CompositionTransformTimeContractTest.cpp`。
- **確認できた事実:** `StaticTransform2D::toQTransform()` と `setTransform2D()` は宣言のみで定義が codebase に存在しない（未検証ではなく grep で確認済み）。`Layer2D::transformedLayer()` は `toQTransform()` を呼ぶ。function-level linking（`/Gy`）は未設定のため、Layer2D の any symbol 参照は static lib の object 全体を引き込み LNK2019 になる。既存テストも Layer2D を参照していない。
- **今回の対応:** 新規 Composition/Transform/Time テストでは Layer2D 本体を外し、StaticTransform2D の定義済みメソッド・Registry・RationalTime・TimeRemap・MatteMode 契約のみにした。除外理由はテスト内コメントに記載。
- **価値／懸念:** Layer2D 所有権テストは定義追加後にしか書けない。本番側も Layer2D.cppm の symbol を参照した瞬間に同じリンク破損が出る。
- **次に確認:** ユーザー承認後に `toQTransform()`（回転・拡縮・アンカー・平行移動の合成順序）と `setTransform2D()`（リセット語義）の定義を追加し、Layer2D 所有権テストを復活させる。ビルド・テストは明示指示待ち。

### 2026-10-10 — Render Manager の visual fixture と queue restore 契約

- **関連:** `Artifact/src/Render/ArtifactRenderQueueService.cppm::fromJson`、`Artifact/src/AppMain.cppm`、`tools/capture_ui_test.py`。
- **確認できた事実:** `fromJson()` は保存済み queue の `Completed` / `Failed` 状態を読み込んだ後、復元した全 job を `Pending`・progress 0 に戻す。失敗の `errorMessage` は保持される。これは実行中状態を安全に復旧しないための既存動作である。
- **今回の対応:** Render Manager の visual fixture は実プロジェクトと composition を作り、待機ジョブ2件と attention 表示用 error 付きジョブ1件を使用する。`fromJson()` の復元安全性を変えずに選択・詳細・status 表示を撮影できる。
- **価値／懸念:** 画像比較が実際の composition/preflight 表示を通り、AppData queue の残存値にも依存しない。Completed / active progress の行表示はこのfixtureでカバーしない。
- **次に確認:** interaction testで完了・進行中の表示状態が必要になった時、永続化の復旧契約を弱めず、テスト専用の状態注入境界で表現できるか検討する。

### 2026-10-10 — Render Manager UI のロケール別翻訳カバレッジ

- **関連:** `Artifact/src/Widgets/Render/ArtifactRenderQueueManagerWidget.cppm`、`Artifact/src/Widgets/Render/ArtifactRenderQueuePresentation.cppm`、`tools/capture_ui_test.py`。
- **確認できた事実:** `ArtifactUiTest --lang ja/en/zh/zh-TW` で撮影した同一 fixture の PNG は4言語とも同一 SHA-256 だった。現時点では Render Manager の表示がロケール変更を反映していない。
- **価値／懸念:** 言語別 UI テストを入れることで、ロケール起動の回帰と翻訳の未接続を区別して把握できる。モックは英語のみのため、他言語は英語モックとの差分で評価できない。
- **次に確認:** ユーザーが翻訳対応を依頼した段階で、Render Manager の静的ラベル・状態・ボタン文字列の翻訳経路を調査し、翻訳済み UI に同一言語の基準画像を用意する。

### 2026-10-10 — 3D粒子contractをoffline-render optionから単独登録する

- **関連:** `tests/Artifact/CMakeLists.txt`、`ARTIFACT_ENABLE_OFFLINE_RENDER_TEST`、`ArtifactOfflineRendererContractTest`。
- **確認できた事実:** Artifactのoffline renderer contract targetは `ARTIFACT_BUILD_TESTS` または `ARTIFACT_ENABLE_OFFLINE_RENDER_TEST` で登録される。初期の3D粒子contract登録は `ARTIFACT_BUILD_TESTS` だけに結び付いていた。
- **今回の対応:** 3D particle contractとproduction-layer captureを、既存offline-render optionからも有効にし、READMEに必要なGTest/GPU条件を記載する。
- **価値／懸念:** 全unit suiteを有効化せずに3D headless rendering gateを選択できる。CMake再構成は禁止中のため、登録条件はソース確認のみ。
- **次に確認:** ユーザーの明示指示後、offline-render option単独のconfigureと対象CTest登録を確認する。

### 2026-10-10 — headless particle drawはPSOを同期準備する

- **関連:** `Artifact/src/Render/ArtifactIRenderer.cppm::drawParticles`、`ArtifactCore/src/Graphics/ParticleRenderer.cppm::ensureGraphicsPipeline/prepare`、`tests/Artifact/Particle3DRenderContractTest.cpp`。
- **確認できた事実:** offline rendererは `m_offlineWidth > 0` の間、particle draw時に `ensureGraphicsPipeline(data.options)` を呼ぶ。未キャッシュPSOはこのheadless経路で同期作成される一方、interactive経路は非同期準備を利用できる。
- **価値／懸念:** 独立headless testで最初のparticle frameが非同期PSO compile raceに負ける懸念を抑えられる。この前提はoffline draw分岐に依存する。
- **次に確認:** offline rendererの初期化・draw経路を変更した場合、同期PSO準備が保たれるか再確認し、外れるならtest fixtureで明示prewarmまたは完了待ちを行う。

### 2026-10-10 — 共有 renderer の粒子テストではカメラ状態を明示的に戻す

- **関連:** `tests/Artifact/Particle3DRenderContractTest.cpp`、`ArtifactIRenderer::reset3DCameraMatrices()`。
- **確認できた事実:** 粒子 contract tests は同じ headless renderer を共有する。3D camera/model test と depth test は `set3DCameraMatrices()` を呼ぶ一方、後続の2D粒子ケースはカメラを戻しておらず、3D状態を残したまま描画する可能性があった。
- **今回の対応:** 2Dケースの描画前に3D camera matricesをresetし、描画後に `cameraMode=2d` と `depthTest=0` / `depthWrite=0` をassertし、3D depth casesでは送信完了後に期待するdepth flagsをassertする。3D camera/model移動、透視footprint、VelocityAligned、depth-order各caseでも `cameraMode=3d` を明示assertする。depthTest/depthWrite診断はpacket送信完了後に読む。readback画像の寸法が96×96でない場合はpixel走査へ進まず失敗させる。
- **価値／懸念:** 2D粒子の no-depth regression check が、テスト実行順やrenderer状態の偶然に依存しにくくなる。今回のAGENTS.md制約により、この差分のビルド・実行は未確認。
- **次に確認:** ユーザーがビルド・テスト実行を許可した後、3D contract suiteを複数回実行し、全ケースが実行され2D診断が成立することを確認する。

### 2026-10-10 — 3D粒子のheadless runnerはQt platformもoffscreenに固定する

- **関連:** `tests/Artifact/Particle3DRenderContractTest.cpp`、`tools/Particle3DPlayground/main.cpp`。
- **確認できた事実:** 3D contract testとproduction-layer captureはいずれも `QGuiApplication` を生成する。既存のheadless renderer contract testでは同じGPU作業前に `offscreen` を設定している。
- **今回の対応:** GTest global environmentとcapture executableの両方で、`QGuiApplication` 構築前にQt offscreen platformを指定する。
- **価値／懸念:** GPU backendが利用できてもdisplay serverがない環境でQt初期化に失敗する可能性を下げる。Qt offscreen pluginがないホストでの実行は未確認。
- **次に確認:** Linux/Vulkanのdisplayなし環境とWindows/Vulkanでcapture executableを実行し、Qt初期化とDiligent device初期化の失敗を区別して確認する。

### 2026-10-10 — image-only particle runnerはQGuiApplicationに留める

- **関連:** `tests/Artifact/Particle3DRenderContractTest.cpp`、`tools/Particle3DPlayground/main.cpp`。
- **確認できた事実:** 3D contractとcapture runnerは画像・GPU描画のみを使い、QWidget APIを参照しない。既存の `ArtifactOfflineRendererContractTest` はheadless readbackに `QGuiApplication` を使っている。
- **今回の対応:** 3D test environmentとcapture mainを `QGuiApplication` に変更し、Widgets applicationの初期化を持ち込まない。
- **価値／懸念:** GUI-less GPU render testの初期化責務が実利用APIと揃う。Qt offscreen pluginがない環境については未確認。
- **次に確認:** headless test executableをdisplay serverなしで起動し、Qt GUI platformとDiligent device初期化の両方を確認する。

### 2026-10-10 — 3D particle seek回帰にもフレーム再訪を含める

- **関連:** `tools/Particle3DPlayground/main.cpp`、2D particle firework capture。
- **確認できた事実:** 2D firework captureは連続再生、直接seek、60→10→45再訪の画像を比較している。3D production captureは連続再生と直接seekだけを比較し、同一layerの非単調なframe移動は未検査だった。`ArtifactParticleLayer::draw()` は `simulationReusable` を保持し、`ParticleSystem::goToFrame()` は同一fps・未来フレーム・120Hz完全step境界でのみsimulationを再利用する。60→10はresetし、その後10→45は条件が合えば再利用経路を通る。
- **今回の対応:** production `ArtifactParticle3DLayer` の別instanceで60→10→45を描画し、frame45 PNGとpixel/channel差をレポート・許容値判定へ加える。
- **価値／懸念:** 履歴依存の状態更新が非単調seek後に再現性を壊す回帰を検出できる。GPU runtimeでの再現性は未確認。
- **次に確認:** VulkanとD3D12で再訪画像が連続再生frame45と決定論的許容差内に収まるか実行する。
- **追補:** 再訪のframe 60と10それぞれで、生存粒子数・`state=queued`・`cameraMode=3d`も確認するようにし、途中のGPU draw不成立を識別する。

### 2026-10-10 — Particle depthWrite=falseを後続geometryで検査する

- **関連:** `tests/Artifact/Particle3DRenderContractTest.cpp`、`ParticlePkt` 深度attachment、3D card PSO。
- **確認できた事実:** 既存の前後関係テストは粒子のdepthTestを確認し、production captureはdepthWrite=falseの設定で手前カードによる遮蔽を見る。ただし、粒子がdepth bufferへ書き込まず、後続のgeometryが粒子より奥でも描画できることは独立に確認していなかった。
- **今回の対応:** `depthTest=true, depthWrite=false` の前景particleを描画・readbackした後、depthWrite=trueの後景cardを描画する。最初の画像で粒子色、最終画像でcard色を別々にassertする。
- **価値／懸念:** 粒子PSOのDepthWriteEnable契約と後続3D geometryとの相互作用を画像で検査する。実GPUでのpixel結果は未確認。
- **次に確認:** Vulkan/D3D12で前景particleが先に出て、後景cardが最終pixelを覆うことを実行確認する。
- **追補:** 深度helperは各readback画像の寸法を確認してから中心pixelを読む。GPU初期化後のreadback失敗はzero pixelのassertion failureとして扱い、invalid imageへ直接アクセスしない。

### 2026-10-10 — VelocityAlignedの速度方向を画像bboxで検査する

- **関連:** `tests/Artifact/Particle3DRenderContractTest.cpp`、`ParticleRenderer` vertex shader、3D particle milestone P4-4。
- **確認できた事実:** shaderは速度をview spaceへ変換し、VelocityAligned policyのとき画面上速度方向から回転角を加える。円形particleでは回転差が見えないため、stretch付き粒子を使えば姿勢を画像bboxで区別できる。
- **今回の対応:** 正面カメラでX速度/Y速度を、傾斜カメラでワールドX速度をそれぞれGPU readbackし、cameraMode=3dを診断確認したうえで、正面Xは縦長・正面Yは横長・傾斜Xは投影された画面速度に沿う横長bboxとなることをcontract化する。深度test/writeを明示的に無効化し、全caseのGPU position/velocity xyzも明示初期化する。
- **価値／懸念:** マイルストーンに残るVelocityAlignedのruntime gateを自動画像テストへ取り込む。Vulkan/D3D12上の実bboxは未確認。
- **次に確認:** 正面と傾斜カメラの全3画像で粒子pixelが存在し、縦横bboxが期待どおりになることを各backendで実行確認する。

### 2026-10-10 — 3D粒子のモデルZ移動は透視投影サイズで検査する

- **関連:** `tests/Artifact/Particle3DRenderContractTest.cpp`、`ParticleRenderer.cppm` のvertex shader。
- **確認できた事実:** Particle vertex shaderはrow-majorのModelRowを使って粒子位置をmodel→viewへ変換し、screen-aligned billboard offsetをview-spaceへ加えてからprojectionする。テストのmodelMatrix[3]/[11]はX/Zの行translationに対応する。従って、同じ小粒子をカメラ方向へZ移動すると画面上のfootprintが増えることを、GPU readbackで契約化できる。
- **今回の対応:** 独立3D contractに、model Z=0とZ=1の画素数比較を追加。既存のX移動テストを保ち、Z移動と粒径をhelper引数で指定できるようにする。
- **価値／懸念:** 3Dテストが画面上の平行移動だけでなく奥行きと透視スケールも検査する。実GPU上での差分安定性は未確認。
- **次に確認:** VulkanとD3D12でnear/far pixel countの両方が非ゼロで、nearがfarを上回ることを実行確認する。

### 2026-10-10 — 3D particle contractもGPU位置xyzを明示初期化する

- **関連:** `tests/Artifact/Particle3DRenderContractTest.cpp`、`ParticleVertex::px/py/pz`。
- **確認できた事実:** `ParticleVertex` のposition成分はdefault member initializerを持たない。2D particle GPU画像テストの調査で、未設定の `pz` がGPU射影後の描画欠落を起こした実例がある。
- **今回の対応:** 3D移動helperとdepth-order helperで `px/py/pz` をそれぞれゼロに明示設定し、Z変化はmodel matrixだけから与える。
- **価値／懸念:** vector element constructionの値初期化規則や将来の生成方法変更に依存せず、テストの意図する座標が固定される。
- **次に確認:** GPU実行時に3D移動・透視footprint・深度ケースが引き続き粒子を描画することを確認する。

### 2026-10-09 — ParticleLayerのGPU画像出力とflipbook経路

- **関連:** `tools/Particle2DPlayground/main.cpp`、`Artifact/src/Layer/ArtifactParticleLayer.cppm`、`ArtifactCore/src/Graphics/ParticleRenderer.cppm`。
- **確認できた事実:** NVIDIA RTX 4070 Ti / Vulkan の headless readbackで矩形8836画素、明示初期化済みの粒子44324画素を確認した。先行テストの粒子は `ParticleVertex` の `pz` を設定しておらず、射影後Zが不定だった。テスト側で値初期化し `pz=0` にするとproduction shaderのまま表示されたため、ParticleRendererの射影修正は不要だった。Emitter `texturePath` は連番ディレクトリとsprite sheetの両方を既存software rendererが読めるが、通常GPU shaderには画像SRVがない。
- **今回の対応:** ArtifactParticleLayerは画像ソース付き2D emitterを既存のcached-image経路へ送る。通常ビューはQImage spriteをGPU rendererへ渡し、GPU offscreen surfaceは既存呼出側のsoftware fallbackへ戻す。連番と4×4 sheetをGPU render target経由で自動キャプチャし、各5フレーム、双方の動き、および画素差8以下／channel差2以下を検査。確認結果は5時刻中4時刻が完全一致、残りは1画素・最大channel差1。
- **価値／懸念:** 画像粒子は通常Viewportでも表示可能になった。texture sampling自体はGPU ParticleRendererに追加していないため、画像付き大量粒子ではsoftware rasterと動的sprite uploadのコストがかかる。GPU native texture samplingへの置換は未検証。
- **次に確認:** 実際のViewport上で連番／sheetの見え方と多数粒子時のframe costを確認し、必要なら既存texture cacheを使うGPU sprite pathの設計を比較する。

### 2026-10-09 — 風の動きは葉プリセットとEmitter物理値で独立確認できる

- **関連:** `tools/Particle2DPlayground/main.cpp`、`ParticlePresets::leaves()`、`ArtifactParticleLayer::setLayerPropertyValue()`。
- **確認できた事実:** 葉プリセットは連続発生・回転をすでに持ち、公開 property path から wind direction／strength、turbulence amplitude／frequency／evolution、drag を設定できる。Playground の自動キャプチャで30〜150フレームの5枚を保存し、先頭と末尾で7386画素が異なることを確認した。
- **価値／懸念:** 花びらのスプライトアニメとは異なる、Emitter物理による軌道変化を小さなオフライン例で確認できる。PNGは現状、葉テクスチャではなく既存の円形particle描画である。GPU経路での同じ動きとスプライト表示は未検証。
- **次に確認:** GPU粒子画素の欠落原因を特定した後、連番画像を載せた葉のWind/Turbulence表示をGPU／software両経路で比較する。

### 2026-10-09 — ACEScc zero encode/decode の逆変換不一致

- **関連:** `ArtifactCore/include/Color/ColorTransferFunction.ixx`、`ColorTransferFunction::linearToACEScc` / `acesccToLinear`、`tests/ArtifactCore/ColorBridgeTest.cpp`。
- **確認できた事実:** `encode(0, ACEScc)` は定数 `-0.3584474886f` を返すが、その値を `decode` へ渡すと約 `131072` になる。非ゼロの密なscene/HDR格子ではこの zero sentinel を除外すると往復を検査できる。ArtifactCore は子リポジトリなので本作業では実装を変更していない。
- **価値／懸念:** ACEScc の黒コードが公開decode式と逆変換になっておらず、黒を含む encode→decode 処理は大きな正値を生成する可能性がある。定数の丸め誤差が指数で増幅されている可能性はあるが、原因と規格上の期待値は未検証。
- **次に確認:** ACES ST 2065-4 の負値／black code 規約と定数精度を一次資料で照合し、ユーザーが ArtifactCore 修正を依頼した場合に専用の黒境界回帰テストと最小修正を行う。

### 2026-10-09 — Rec.709 OETF/EOTF 折れ点の非整合

- **関連:** `ArtifactCore/include/Color/ColorTransferFunction.ixx`、`ColorTransferFunction::linearToRec709` / `rec709ToLinear`、`tests/ArtifactCore/ColorTransferFunctionStandaloneTest.cpp`。
- **確認できた事実:** 独立した倍精度参照と全1,024個の正規化コード値を照合すると曲線本体は約4e-7以内だった。一方、OETF の線形折れ点 `0.018` を高側式へ入れた値は約 `0.08124793` だが EOTF は `0.081` を境界としており、境界ちょうどで `0.017945` 付近へ戻る。両端の折れ点が一致していない。Rec.2020 の同じ全コード比較は約4e-7以内で通る。
- **価値／懸念:** Rec.709 のブレークポイント近傍に明確な段差・往復誤差がある。原因は実装にあるしきい値 `0.081` と OETF の係数から生じる `0.0812479...` の差と見られるが、採用する規格定数と許容誤差は未検証。ArtifactCore は子リポジトリのため実装変更はしていない。
- **次に確認:** 参照する BT.709 版と規格上の breakpoint 定義を一次資料で確認し、ユーザーが ArtifactCore 修正を明示した場合に OETF/EOTF 境界の最小修正と回帰テストを行う。

### 2026-10-09 — sRGB OETF breakpoint直後の微小な下向き段差

- **関連:** `ArtifactCore/include/Color/ColorTransferFunction.ixx`、`ColorTransferFunction::linearToSRGB`、`tests/ArtifactCore/ColorTransferFunctionStandaloneTest.cpp`。
- **確認できた事実:** `0.0031308f` とその次の表現可能floatを別々の枝へ通すと、OETF出力が約 `0.040449936` から `0.040449910` へ約 `2.7e-8` 下がる。倍精度の区分式参照との比較でもこの値を再現し、隣接float専用テストに現在の段差をcharacterizationした。
- **価値／懸念:** 数値としてはごく小さいが、OETFの厳密な単調性を破る。丸められたbreakpointと係数による既知の標準近似として許容するか、分岐点／係数を調整すべきかは未検証。ArtifactCore は子リポジトリなので本作業では実装を変更していない。
- **次に確認:** IEC 61966-2-1 の採用定数と出力精度要求を一次資料で確認し、ユーザーがArtifactCore修正を明示した場合に最小の境界修正と回帰テストを行う。

### 2026-10-09 — PQ float 実装の全10bitコード精度

- **関連:** `ArtifactCore/include/Color/ColorTransferFunction.ixx`、`ColorTransferFunction::linearToPQ` / `pqToLinear`、`tests/ArtifactCore/ColorTransferFunctionStandaloneTest.cpp`。
- **確認できた事実:** SMPTE ST 2084 の独立倍精度式に対し、正規化された全1,024コード値の OETF/EOTF を比較した。現在の float 実装は最大絶対差が OETF 約 `9.94e-6`（code 274）、EOTF 約 `3.85e-5`（code 1022）。テストではこの観測差を少し上回る `1.1e-5` / `4.0e-5` を回帰上限としている。
- **価値／懸念:** PQ EOTF の高コード側で線形値の差が OETF より増幅される。これが許容可能な float 実装誤差か、要求精度に対して不足かは未検証。ArtifactCore は子リポジトリのため実装変更はしていない。
- **次に確認:** 用途別のPQ精度要求と参照規格の試験点を確認し、ユーザーが ArtifactCore 修正を明示した場合に係数・演算精度の最小変更を検討する。

### 2026-10-09 — Canon Log 3 low toe 境界の段差

- **関連:** `ArtifactCore/include/Color/ColorTransferFunction.ixx`、`linearToCanonLog3` / `canonLog3ToLinear`、`tests/ArtifactCore/ColorTransferFunctionStandaloneTest.cpp`。
- **確認できた事実:** low encoded threshold `0.04076162` から計算した線形 low toe 境界の直下では OETF が約 `0.04076`、境界ちょうどでは middle affine branch に入り約 `0.04700` を返す。EOTF も encoded threshold の直下で約 `-0.0113`、threshold ちょうどで middle branch の約 `-0.0140` に切り替わる。独立テストに全10bitコードの照合と、この low toe の枝切替を固定する characterization を追加した。
- **価値／懸念:** Canon Log 3 low toe の encode/decode に目立つ不連続がある。式の係数・境界の取り違えか、負値域の設計仕様かは未検証。ArtifactCore は子リポジトリのため実装変更はしていない。
- **次に確認:** Canon Log 3 の公開仕様にある low-end toe と負値範囲を一次資料で照合し、ユーザーが ArtifactCore 修正を明示した場合に最小修正と回帰テストを行う。

### 2026-10-09 — Canon Log 2 負側 toe 境界の OETF 段差

- **関連:** `ArtifactCore/include/Color/ColorTransferFunction.ixx`、`linearToCanonLog2`、`tests/ArtifactCore/ColorTransferFunctionStandaloneTest.cpp`。
- **確認できた事実:** 負側 toe 境界 `-(10^(toe/slope)-1)/scale` の直下は負値用式で 0 付近を返し、境界ちょうどは通常式 `slope*log10(linear*scale+1)+toe` に切り替わって約 `-0.0146` になる。回帰テストでは両枝の値と段差を characterise した。Cineon の black code は式上 `95/1023` と確認した。
- **価値／懸念:** Canon Log 2 の負値 toe に不連続がある。正値域の通常コードでは目立たない一方、負値・scene-linear入力を通すと境界に段差を生む。負値処理が規格要件かは未検証。ArtifactCore は子リポジトリのため実装変更はしていない。
- **次に確認:** Canon Log 2 の公開仕様で負の scene-linear 値と toe 境界の規約を確認し、ユーザーが ArtifactCore 修正を明示した場合に最小修正する。

### 2026-10-09 — ACESlog enum の汎用変換 dispatch 未接続

- **関連:** `ArtifactCore/include/Color/ColorTransferFunction.ixx`、`TransferFunction::ACESlog`、`ColorTransferFunction::encode/decode`、`tests/ArtifactCore/ColorTransferFunctionStandaloneTest.cpp`。
- **確認できた事実:** `TransferFunction` enum に `ACESlog` があるが、汎用 encode/decode の switch に専用 case はなく default の linear identity を返す。独立テストに現在の identity fallback を固定した。ACESlog の専用 OETF/EOTF を提供する実装はこのインターフェースで確認できなかった。
- **価値／懸念:** UI や呼び出し側が enum を選んでも ACESlog 変換は行われず、入力値がそのまま返る。これは未実装か意図的 fallback か未検証であり、テストは現状の dispatch を characterization している。
- **次に確認:** ACESlog の対象規格・必要な用途と UI 露出有無を確認し、ユーザーが ArtifactCore 機能追加を明示した場合に専用変換と独立参照テストを追加する。

### 2026-10-09 — XYZ_D60 gamut enum が Bradford dispatch に未接続

- **関連:** `ArtifactCore/include/Color/ColorGamutConversion.ixx`、`Gamut::XYZ_D60` / `getConversionMatrix()`、`tests/ArtifactCore/ColorGamutConversionTest.cpp`。
- **確認できた事実:** `XYZ_D60` と `XYZ_D65` の両方向変換は `getConversionMatrix()` で identity となる。Bradford 判定が `ACES_AP0` / `ACES_AP1` のみを D60 source/target として扱い、`XYZ_D60` は含めていない。D60白 `(.9526461, 1, 1.0088252)` は D65へ変換しても不変で、D65白から逆方向も変化しない。独立テストに現行 fallback を characterization として追加した。
- **価値／懸念:** 色域 enum の名前と実際の white-point routing が一致せず、D60↔D65 XYZ 変換ではBradford adaptationが省略される。ArtifactCore は子リポジトリのためこの作業では実装を変更していない。
- **次に確認:** `XYZ_D60` を単なるXYZ座標系タグとして扱う設計か、D60 white-point adaptation 対象として扱うべきかを仕様確認し、ユーザーが ArtifactCore 修正を明示した場合に最小 dispatch 修正を行う。

### 2026-10-09 — ArtifactUiTest の QSettings 分離境界

- **関連:** `Artifact/src/AppMain.cppm`、Artifact 内の `QSettings(organization, application)` 呼び出し。
- **確認済み:** Qt の公式 API 仕様では `setDefaultFormat()` は既定コンストラクタにのみ効き、組織名・アプリ名を明示したコンストラクタは `NativeFormat` を使う。Windows では `NativeFormat` の `setPath()` も効かない。UI Test 起動の `QStandardPaths::setTestModeEnabled(true)` は AppData／AppConfig 等の標準書込先を test 領域へ変えるが、明示的な QSettings コンストラクタの Windows Registry 保存先は分離しない。
- **価値／懸念:** 現状の UI Test exe は実画面を独立起動できる一方、これらの明示設定を読む／書く画面は通常版と同じ Registry 設定を共有する。完全な設定分離を行うなら、QSettings 呼出側の共通 factory 化などが必要で、起動処理だけの変更では実現できない。
- **次に確認:** UI テストで通常版の既存設定を読む・変更することが許容されるかを決め、完全分離が要件なら明示コンストラクタ呼出箇所を一括で扱う小さな設定アダプタの設計を検討する。

### 2026-10-09 — Filmstrip 未取得フレーム生成の実行経路

- **関連:** `Artifact/src/Widgets/ArtifactFilmstripWidget.cppm`、`Artifact/src/Render/ArtifactOffscreenCompositionRenderer.cppm`、`Artifact/src/Service/ArtifactPlaybackService.cppm`。
- **確認済み:** Filmstrip は RAM キャッシュの縮小表示とパネル内の onion skin／比較を提供する。既存の offscreen renderer の `renderFrame(position, composition)` は position をレイヤーの active 判定に使うが、各レイヤーを指定フレームへ評価していない。共有レイヤーを描画するため、そのままワーカーへ渡す根拠もない。Playback Service の build queue は単一で、Filmstrip が置き換えると他のプレビュー要求に干渉する。
- **未検証の案:** 未取得分の独立生成には、既存 GPU submission の所有経路で固定上限の要求を処理し、composition revision と表示世代で古い結果を破棄する仕組みを検討する。
- **次の確認:** 共有レイヤー評価状態と immediate context の所有者を確認し、表示中のフレームを変更せずに生成できる経路を選ぶ。

### 粒子 — GPU第一経路化・非同期PSO・ディスクPSOキャッシュの実機確認

- **関連:** `ArtifactCore/src/Graphics/ParticleRenderer.cppm`（非同期ワーカー・PSOキャッシュ・ディスクregistry）、`Artifact/src/Layer/ArtifactParticleLayer.cppm`（trail送出・GPU面）、`Artifact/src/Render/ArtifactOffscreenCompositionRenderer.cppm`。
- **状態:** 実装済み、ビルド・runtime未検証。
- **確認すること:** 層追加の固まり解消、初回CPU→GPU切替の継ぎ目、trail／stretch／エフェクト付き表示、2回目起動での待機診断消失とキャッシュファイル生成、Vulkanでの96バイト stride。

### 2026-10-09 — 粒子キャプチャはレイヤーのフレーム同期経路を通す

- **関連:** `tools/Particle2DPlayground/main.cpp`、`Artifact/src/Layer/ArtifactParticleLayer.cppm`、`Artifact/src/Generator/ArtifactParticleGenerator.cppm`。
- **確認済み:** `renderFrame()` に大きな時間差を直接渡しただけでは burst が出ず、空PNGになった。`ArtifactParticleLayer::draw()` は `goToFrame()` で決定論的な粒子状態を同期してからソフト描画へ進む。Playground の自動キャプチャもこの既存経路を通すと粒子があるPNGを出力できた。
- **価値／懸念:** 自動画像テストが空画像を成功扱いしないよう、粒子数・保存成否を検証する必要がある。現在の `--capture-firework` はソフト描画経路のみで、GPU readback と連続再生／seek間の画素一致は未検証。
- **次に確認:** 同じ burst を GPU と software で保存し、連続再生・直接seek・巻き戻し後の同一フレーム画像を比較する。

### 静止画・連番画像 — GPU cache と実素材の再生／出力確認

- **関連:** `Artifact/src/Layer/ArtifactImageLayer.cppm`、`Artifact/src/Render/GPUTextureCacheManager.cppm`。
- **状態:** 実装済み、runtime未検証。
- **確認すること:** 4K連番、欠番、Time Remap、再リンク、Preview／Render Queueでフレーム・GPUメモリ・出力が一致すること。

### Shape — Path keyframe／Merge Paths／SVG出力の実機確認

- **関連:** `Artifact/src/Layer/ArtifactShapeLayer.cppm`、`Artifact/include/Layer/ArtifactShapeLayer.ixx`、`Artifact/src/Render/ArtifactRenderQueueService.cppm`。
- **状態:** GPU／互換フォールバック／boundsの同期は実装済み。SVGのグラデーション、stroke taper／alignは未対応。
- **確認すること:** 複数subpathと各Merge Paths mode、Path keyframe、GPU／出力SVGの一致。

### Shape — 複数コンテンツ／GPUベクター描画の実機確認（2026-09-03）

- **関連:** `Artifact/src/Layer/ArtifactShapeLayer.cppm`（`ShapeContent`、`paintGpuPaintItems`、`resolveContentVisPaths`、`renderContentsToImage`）、`Artifact/include/Layer/ArtifactShapeLayer.ixx`。
- **状態:** 実装済み、ビルド・runtime未検証（ビルドは指示待ちのため未実行）。
- **内容:** 1レイヤー複数パス（形状＋塗り＋線＋表示＋結合モード、空＝従来動作）。グラデーションは三角形重心サンプリング、Inside／Outsideはハーフオフセット＋中央線、テーパー／勾配線はセグメント分割でGPU描画し、viewportのQImageスプライト分岐を撤去。結合はCPU側QPainterPath真偽値演算で解決し、スタイルは保持。物理グリッド・3Dカード高速パス・オペレータキー評価は従来のまま。
- **確認すること:** 既存単一シェイプの見た目不変（solid高速パス・operator分岐は温存）、グラデーション／align／taperのGPU描画、Subtract／Intersect／Differenceの穴・境界線、連番・サムネイル・SVG出力、Repeater大量複製時の負荷。
- **既知の近似（未検証の仮説ではない仕様）:** テーパー線の結合部は Butt 重ね、Roundキャップは矩形延長近似、dash＋taper併用はdash優先、コンテンツのパス頂点アニメは未対応（静的）。

### Shape — 沿路グラデーション線／ダッシュオフセット（2026-09-03）

- **関連:** `Artifact/include/Render/ArtifactIRenderer.ixx`（`PolylineStyle`）、`Artifact/src/Render/ArtifactIRenderer.cppm`（`drawStyledPolyline`）、`Artifact/src/Layer/ArtifactShapeLayer.cppm`。
- **状態:** 実装済み、ビルド・runtime未検証。
- **内容:** `PolylineStyle`に`gradientEnabled/gradientStart/gradientEnd/dashOffset`を追加。`drawStyledPolyline`は累積長パラメータでセグメント・ダッシュ・結合・キャップを沿路補間色で描画し、dash位相は`dashOffset`の剰余で解決。レイヤー側は`shape.dashOffset`（アニメ可）＋コンテンツ別`dashOffset`、勾配のみの線は taper 分割器ではなく`drawStyledPolyline`経由に変更（結合・キャップ・dashと合成可）。QImage互換・SVG出力（`dashOffset`のみ）・保存も配線。
- **確認すること:** 既存実線の見た目不変（新フィールド既定で旧経路と同一）、勾配＋dash＋round結合の合成、負offset・巨大offset、マーチングアンツのキーフレーム補間。
- **既知の近似:** taper＋dash併用はdash優先でtaper無効、勾配サンプリングは線形補間。

### Shape — SVG相互運用（取込・書出）（2026-09-03）

- **関連:** `Artifact/src/Layer/ArtifactShapeLayer.cppm`（`SvgImport`、`shapeContentsToSvg`、`parseShapeContentsFromSvg`、`addShapeContentsFromSvg`、`importSvgFileContents`）。
- **状態:** 実装済み、ビルド・runtime未検証。
- **内容:** 書出は結合解決済みパス＋塗り／線／dash／fill-ruleを`<path>`＋`linear/radialGradient` defsで出力（taper線→通常線、conical→単色、勾配線→中間色に縮退）。取込は`path(d全命令・Aはベジェ化)`・rect（角丸可）・circle・ellipse・polygon・polyline・line＋線形／円形グラデーション（前方参照可）＋transform bake＋継承スタイルを編集可能コンテンツ化（座標はbounds正規化、256件cap、64MB cap）。`ClipboardManager`は未変更で、受渡し自体は素のSVGテキストを呼出側に委譲。
- **確認すること:** Illustrator／Figma出力SVGの往復、userSpace勾配・奇数dash・相対命令・指数表記の数値、空・不正SVG（0件／-1）、既存JSON互換。
- **既知の近似:** 複数subpathは1要素に統合（線描画で連結線が出る）、3 stops以上は両端のみ、gradientTransform・非等方scale下の線幅・group fill-opacity継承は近似、strokeのurl()は勾配線として解決（fillのみ前方参照対応だった点をstore側で統一）。

### Shape — コンテンツ編集サポート（2026-09-03）

- **関連:** `Artifact/src/Layer/ArtifactShapeLayer.cppm`（`activeContentIndex_`、`ShapeContentProxy`、`duplicateShapeContent`、`moveShapeContent`、`insertShapeContent`、`swapShapeContents`）、`Artifact/include/Layer/ArtifactShapeLayer.ixx`。
- **状態:** 実装済み、ビルド・runtime未検証。
- **内容:** `activeContentIndex_`（-1 = レガシーモード）と`ShapeContentProxy`（`ArtifactShapeLayer*` + index）を導入。Proxyは`name`/`visible`/`opacity`/`merge`/`fill`/`stroke`/`geometry`/`duplicate`を`setShapeContentAt`経由で直接編集し、PropertyEditorは`shape.activeContentIndex`で操作対象を切り替える。複製（挿入位置にコピー）、挿入、`move`、`swap` APIを追加。`shape.content.<i>.type/width/height/cornerRadius/starPoints/starInnerRadius/polygonSides/fillRule` を `setLayerPropertyValue` で直接編集可能に拡張。JSONシリアライズに`activeContentIndex`を含む。
- **確認すること:** Proxyのスワイプ（他のインデックス参照）、move/swap後のbounds・visPaths再構築、JSON往復、PropertyEditorでのアクティブコンテンツ切り替え時の描画反映、contentジオメトリ編集時の再構築・保存。

### 2.5D — 局所DOF／motion blurの品質と負荷

- **関連:** `Artifact/src/Layer/ArtifactAbstractLayer.cppm`、`Artifact/src/Layer/Artifact{Image,Shape,Text}Layer.cppm`、`Artifact/src/Render/PrimitiveRenderer2D.cppm`。
- **状態:** 実装済み、runtime未検証。
- **確認すること:** Image／Shape／Textで深度・focus・shutterを変え、alpha順、極端なblur、再生時のGPU負荷、書き出し結果を確認する。

### Gobo runtime texture — 将来接続の安定ID境界

- **関連:** `ArtifactCore/include/Graphics/MeshRenderer.ixx`、`ArtifactCore/src/Graphics/MeshRenderer.cppm`。
- **状態:** ファイルGoboを置換できるruntime SRV入力は実装済み。Image Layerとの接続・UI・保存は未実装。
- **判断待ち:** 接続を始める時点で、packed scene-light slotではなく安定したLight IDとresource revisionを対応付ける。

## 現在の設計判断

### Semantic Debugger — 外部 `ArtifactDebugger.exe` を正規UI境界とする

- **関連:** `docs/planned/MILESTONE_EXTERNAL_SEMANTIC_DEBUGGER_2026-09-02.md`、既存のMCP／Trace／Shared Memory IPC診断基盤。
- **状態:** 未着手。設計判断をマイルストーン化。
- **判断:** ArtifactStudio本体には低コストの `ArtifactDebugRuntime`（semantic identity、mutation provenance、frame snapshot、safe-point制御）だけを置き、意味表示・原因解析・timeline・semantic breakpointのUIは外部プロセスへ分離する。既存MCPはAI専用に作り直さず、Debuggerとheadless harnessが共有するread／control protocolの基盤として再利用する。
- **価値／懸念:** 本体のQt／レンダリング状態をデバッガUIから隔離し、VS native debuggerとの併用、実行中Attach、Debugger単独更新を可能にする。一方、protocol version、履歴欠落の「未観測」表示、frame boundaryでのpause、snapshot復元とdeterministic replayの境界が必要。
- **次に確認すること:** Phase 0で既存MCP TCP／QLocalSocket／Named Pipeの接続候補、共有されるsemantic schema、diagnostic buildと通常buildのruntime有効化方針を確定する。

### Layer modulation は opacity以外へ拡張しない

- **関連:** `Artifact/src/Layer/ArtifactAbstractLayer.cppm`、`ArtifactCore/include/Audio/Modulation/Router.ixx`。
- **状態:** opacityの評価、保存、Undo基盤は実装済み。Inspector導線とruntime確認は未完。
- **判断:** Transformはvariant／physics／layoutとの評価順を定義するまで追加しない。

### 物理 — rigid joint／polygon colliderの動作確認を先行する

- **関連:** `Artifact/src/Composition/ArtifactAbstractComposition.cppm`、`ArtifactCore/src/Physics/Physics2D.cppm`。
- **状態:** jointとpolygon colliderの導線は実装済み、runtime未検証。
- **確認すること:** 重力の画面座標符号、scrub復元の制約、凹形状の凸近似、SoftBody／MPMとの接触。

### 3D rendering — AOVと実GPU契約は別スライスに保つ

- **関連:** `Artifact/src/Widgets/Render/ArtifactCompositionRenderController.cppm`、`Artifact/src/Render/DiligentImmediateSubmitter.cppm`。
- **状態:** World Position AOVのtarget基盤はあるが、書き込みPSO／runtime検証は未実装。
- **判断:** 2D／2.5Dの完成度を優先し、AOV、GPU skinning、ray tracingは個別の受入条件を定義してから進める。

### プロキシ生成 — Out-of-Process 専用ワーカー (ArtifactProxyWorker) 方式の採用

- **関連:** `Artifact/src/Layer/ArtifactVideoLayer.cppm`、`Artifact/include/Proxy/ProxyService.ixx`。
- **状態:** `ArtifactProxyWorker.exe` の native / Media Foundation / ffmpeg 経路と JSON Lines 通知を実装。native は実エンコーダー名と検出候補も返す。host は jobId/outputPath/outputBytes を照合し、Project View から queue キャンセルも可能。Eighth、音声再エンコード、hardware encoder、auto fallback、staged package の worker 実機確認済み。Media Foundation は H.264 入力で成功するが、ProRes 入力は非互換で、極小出力は 64px/axis にクランプする。成功・失敗・キャンセル時の partial cleanup と、検証ツールの worker timeout 回収も確認済み。host UI 統合の実機確認は未完。
- **判断:** 本体プロセスの安定性（クラッシュ・OOM 巻き添え防止）と FFmpeg C API 直接利用（進捗通知・GPU HW エンコード）を両立するため、専用の子プロセスワーカー（`ArtifactProxyWorker`）を設けて非同期 IPC で連携する構成を正規方針とする。

### Proxy worker — 成果物の原子性と timeout 回収を同じ受入条件にする

- **関連:** `Artifact/src/Worker/ArtifactProxyWorker.cpp`、`tools/proxy_worker_smoke_test.py`、`Artifact/src/Widgets/ArtifactProjectManagerWidget.cppm`。
- **状態:** 実装済み、host UI実機確認待ち。
- **判断:** worker の exit code だけでは成功とみなさず、completed message、final output、partial cleanup、timeout／強制終了後の状態を一組で検証する。これにより、ハングや中断を有効な proxy と誤認する経路を受入段階で検出できる。

## 保留中の設計判断

### シェイプレイヤー VP 操作の増強範囲（2026-09-02 ユーザー質問）

- **質問:** 「シェイプレイヤーのVP操作機能増強いけそうか」
- **解釈:** `VP = メインコンポジション Viewport`（`taste.md` の communication-integrity 規範に従い grep で `Viewport`/`TextViewport` へ展開、コード上に独立した `VisualProgramming` 系は無いため）。
- **現状（コード読みで確認済み、未検証含む）:**
  - `Artifact/src/Tool/ArtifactToolManager.cppm:44-46` に `ToolType::Shape / Rectangle / Ellipse` が定義され、`ToolType::Shape` は `ArtifactToolService` で `Shape modeling` 入口と紐付け済み（`docs/done/MILESTONE_2D_SHAPE_MODELING_EDITING_2026-06-29.md:258`）。
  - `Artifact/src/Layer/ArtifactShapeLayer.cppm` は `ShapeType`（Rect/Square/Ellipse/Star/Polygon/Triangle/Line）+ `customPolygonPoints_` + `CustomPathVertex { pos, inTangent, outTangent, smooth }` + `customPathClosed_` を保持し、`evaluatePathAt(frame)` でパス頂点キーフレーム評価を実装。
  - `Artifact/src/Widgets/Render/ArtifactCompositionRenderController.cppm` のシェイプ系 VP ハンドラ:
    - `mousePress` L22853-22923 で Rectangle/Ellipse/Shape ツールのドラッグ → 矩形/楕円/スター/ポリゴン/三角のシェイプレイヤー新規作成（選択レイヤー有りは mask 経路、無しなら `RectangleToolMode::Shape/EllipseShape/StarShape/PolygonShape/TriangleShape` で `ArtifactShapeLayer` を生成、L27803-27873）。
    - `mousePress` L22940-22991 で Pen ツールが Shape レイヤー選択時のみマスクではなくカスタムパスを `pendingShapePathVertices_` へ追加（開始点クリック or Enter で確定、Backspace で取消、Escape 取消）。
    - `mousePress` L23527 `beginShapePathVertexDrag`、`updateShapePathVertexDrag` L25095、`endShapePathVertexDrag` L27345 経由でカスタムパスの頂点/タンジェントドラッグ編集。`ShapePathVertexEditCommand` で Undo。
    - `mousePress` L23506-23527 で Line シェイプの端点ドラッグ (`isDraggingLineEndpoint_` / `draggingLineLayer_`) を実装。
    - `isDraggingShapePathVertex_` / `shapePathEditPending_` / `shapePathEditDirty_` / `hoveredShapePathVertex_` / `hoveredShapePathTangent_` / `draggingShapePathTangent_` (0=vertex / 1=in / 2=out) を state に保持 (L12488-12505)。
  - `ArtifactCompositionRenderOverlay.cppm` のシェイプレイヤー描画: L1028-1105 でカスタム Polygon の頂点ストローク描画、Line の 2 端点描画、customPathVertices のベジェ描画を実装。ただし **頂点ハンドル/タンジェントハンドル/選択ハイライト/セグメント挿入マーカーのオーバーレイ描画パスは未確認**（grep 上このファイルには vertex overlay / hit-area / ハンドルサイズ定数が Shape 用に出てこない）。
  - `ArtifactRenderLayerWidgetv2`（LayerEditorPanel 内、`MILESTONE_2D_SHAPE_MODELING_EDITING_2026-06-29.md:255-279`）に vertex / segment / tangent のコンテキストメニュー・Ctrl-click 選択追加・Shift toggle・numbering・hover/select 表示・path vertex duplication・polygon vertex duplication・segment insert 経路がある。`MILESTONE_LAYER_EDIT_2026-04-25.md:209-219` で `customPolygonPoints` を `CustomPathVertex` へ拡張済み。
- **増強候補（メイン VP 視点で未着手／不足）:**
  - (1) シェイプ専用 vertex/tangent/segment overlay 描画の RenderOverlay 統合（`LayerEditorPanel` の機能をメイン VP へ移植した残骸: `INSIGHT_ARCHIVE_2026-09-01.md:4549-4551` の懸念「ビルド未実施。タンジェント smooth 反射の長さ保存比、パスキーフレームの UI は次段階」）。
  - (2) Rect の `cornerRadius` ハンドル（`hitTestCornerRadiusHandle()` は `ArtifactRenderLayerWidgetv2` にあり、メイン VP 側は `setSize()` を width/height ドラッグで更新する経路のみ、`MILESTONE_LAYER_EDIT_2026-04-25.md:60` に「ShapeEditCommand 同様」とあるが RenderController 側 grep で未確認）。
  - (3) Star の `starInnerRadius_` ハンドル、Polygon の頂点ドラッグ挿入。
  - (4) `ToolType::Shape` のプリセット図形選択 UI（Rect/Ellipse/Star/Polygon/Line/Triangle のアクティブ切替。現状シェイプ作成は `Shape` 単独か `Rectangle/Ellipse` ツールの `rectangleToolMode_` 切替のみで、Panel 上にプリセット導線なし）。
  - (5) シェイプ operator stack（TrimPaths/Merge Paths/Offset/Pucker/Rounded/Wiggle/ZigZag/Twist/HandDrawnWobble/Stroke taper、9種実装済）のシェイプ VP 上インスペクタ／数値ハンドルドラッグ編集。
  - (6) パスの open/closed トグル、smooth toggle、corner ↔ bezier 切替を VP ハンドルで（`MILESTONE_2D_SHAPE_MODELING_EDITING_2026-06-29.md:243-245` の selection grammar 整備と並ぶ）。
  - (7) シェイプレイヤー選択時の頂点/セグメント/タンジェントの選択 grammar をメイン VP 上で完成させ、`MILESTONE_LONG_MODULE_SPLIT_2026-08-31.md:11-12,29` で計画中の `Artifact.Widgets.LayerEditor.ShapeOverlay` / `ShapeEditSession` / `ShapeHoverController` へ接続する。
- **価値／懸念:** AE 互換のシェイプ編集（特に頂点ドラッグ・tangent smooth・polygon segment insert・operator ハンドル）は既存の描画・データ層を破壊せずに機能を乗せられる層が既に厚く、メイン VP 側の実装ギャップはおおむね UI と routing 追加で済む。一方で (1) RenderOverlay への新規シェイプ専用 HUD 描画と `(7) ShapeEditSession` 抽出は `MILESTONE_LONG_MODULE_SPLIT_2026-08-31.md` と相互作用し、`ArtifactCompositionRenderController.cppm` 28214 行・`ArtifactShapeLayer.cppm` 3380 行・`ArtifactCompositionRenderOverlay.cppm` 1827 行という巨大ファイル状態では変更影響範囲の見積もりが難しい。`MILESTONE_FLUID_COMPONENT_VS_PYRO_DOMAIN_SPLIT_2026-07-01.md` の "incremental / stable" 方針に従い、まず最小スライスで 1 機能ずつ上げるのが安全。
- **次に必要なユーザー判断:**
  1. スコープ: 既存の `MILESTONE_SHAPE_SVG_EXPORT_AND_KEYFRAME_VERIFY_2026-08-22.md` Phase D（キャンバス頂点編集）と Phase E（複数シェイプ）をそれぞれ独立に進めるか、または一括で (1)〜(7) をフェーズ計画に起こすか。
  2. 編集ホスト: メイン VP で直接編集（既存 `ArtifactCompositionRenderController` を拡張）か、`ArtifactRenderLayerWidgetv2`（LayerEditorPanel）側に集約して「ソロビュー」相当の編集ペインにするか。
  3. データモデル: 現状の単一 primitive を維持して `ShapeType` をツールプリセットにマップするか、コア `ShapeGroup` ベース（Phase E）へ移行してから VP 操作を実装するか。

## 検証運用

- **2026-09-04 — AI セグメンテーションの Core 契約:** `ArtifactCore/include/AI/ImageSegmenter.ixx` と `ArtifactCore/include/Image/DepthMap.ixx`。**事実:** 既存の `applySegmentationMask()` は空実装で、推論結果を書き込む `DepthMap` API がなかった。**対応:** 推論を `IImageSegmenter`（正規化された1ch前景マスク出力）へ限定し、Core 側で bilinear resample、閾値・softness・反転、alpha乗算／置換を適用する共有契約を実装した。`refineSegmentationMask()`で閾値／softness、foreground expand／contract、featherの共通後処理を追加し、`segmentBatch()`で複数の静止画／フレームから非破壊マスクを一括生成できるようにした。`analyzeSegmentationMask()`は foreground coverage／平均信頼度／bounds を返し、空マスクや過大マスクを App 側で警告できる。連番では `stabilizeSegmentationMask()` が前フレームマスクを控えめに混ぜ、推論のちらつきを抑える（動き追従は行わない）。モデル未配置時は、非AI・低品質であることを明示した `LuminanceImageSegmenter` を高コントラスト素材用のフォールバックとして追加した。**価値／懸念:** ONNX／DirectML、CPU fallback、将来のGPU推論はいずれも同一結果型に接続できるが、実モデル・モデル資産契約・GPU経路／実機品質は未検証。**次に確認:** 人物セグメンテーションモデルを1つ選定し、静止画の alpha 結果と既存 GPU mask 合成の preview／export parity を確認する。

- **2026-09-04 — ONNX/DirectML セグメンテーションアダプタ:** `ArtifactCore/include/AI/OnnxImageSegmenter.ixx`、`ArtifactCore/src/AI/OnnxImageSegmenter.cppm`。**事実:** 既存のONNX DirectML実装はテキスト生成専用で、画像モデルの入力・出力を `IImageSegmenter` に正規化する実装がなかった。またONNX compile definition/link は `ArtifactCoreAI` ではなく親 target にのみ付与されていた。**対応:** NCHW float 入力、最終2次元をマスクとするfloat出力のONNXモデルを、DirectML優先で読み込み、`DepthMap`へ戻すアダプタを追加。入力RGBのscale／mean／stddevと出力のNone／Sigmoid／Softmax、複数出力モデルの `outputIndex` を設定可能にし、出力マスクは bilinear で元解像度へ戻してsoft matteの連続値を保つ。AI target 自身へONNX link/defineを付与した。**価値／懸念:** 背景除去モデルをCoreだけで動かせるが、複数入力・動的shapeなどは設定契約を拡張してから対応する。**次に確認:** 実モデル（例: U²-Net系）を配置し、人物／髪のマット品質、DirectML利用、失敗時メッセージを実機確認する。

- **2026-09-04 — ONNX image module の明示BMI参照:** `ArtifactCore/cmake/ArtifactCoreModuleReferences.cmake`。**事実:** `ArtifactCore` は実装 `.cppm` の primary interface attachment を自動dependency scanへ任せず、同ファイルで明示的なBMI参照を管理する。**対応:** `OnnxImageSegmenter.cppm` に primary interface と `Core.AI.ImageSegmenter` の参照を追加した。**価値／懸念:** Ninja/MSVCのdyndep不安定化を避けられるが、今後の新規 `import` 追加時にも同ファイルを同期する必要がある。**次に確認:** ユーザー許可後のCMake生成／ビルドで、OnnxImageSegmenterのIFC参照とONNXヘッダ解決を確認する。

- **2026-09-04 — ONNX image model diagnostics:** `ArtifactCore/include/AI/OnnxImageSegmenter.ixx`、`ArtifactCore/src/AI/OnnxImageSegmenter.cppm`。**対応:** `modelInfo()` に ready、DirectML有効状態、入力サイズ・チャンネル、入力／出力テンソル名をまとめた read-only snapshot を追加。**価値:** App/UIを変更せずに、モデル契約と実行バックエンドの診断を接続できる。**次に確認:** 実モデル読み込み時にsnapshotとONNX Runtimeのsession情報が一致すること。

- **2026-09-04 — ONNX segmentation JSON configuration:** `ArtifactCore/include/AI/OnnxImageSegmenter.ixx`、`ArtifactCore/src/AI/OnnxImageSegmenter.cppm`。**対応:** `loadOptionsFromJson()` を追加し、入力サイズ、前処理、letterbox、出力選択／activation、DirectML優先度を外部JSONから読み込む。既存sessionは設定変更時にresetする。**価値:** モデル資産を後で導入する際、コード変更なしにモデル固有契約を再現できる。**次に確認:** 実モデルの配布設定JSONを1つ作成し、モデル入力仕様と照合する。

- **2026-09-04 — ONNX segmentation letterbox pre-process:** `ArtifactCore/include/AI/OnnxImageSegmenter.ixx`、`ArtifactCore/src/AI/OnnxImageSegmenter.cppm`。**対応:** `preserveAspectRatio` と padding value を追加し、固定サイズモデルへletterboxで渡し、出力マスクを同じ座標変換で元解像度へ戻す処理を追加。既定はstretchで後方互換を維持。**価値:** 縦長素材や正方形モデルで人物形状を歪めずに推論できる。**次に確認:** 16:9／9:16／1:1の実モデル結果でpadding境界とmask座標を確認する。

- **2026-09-04 — セグメンテーション失敗診断の統一:** `ArtifactCore/include/AI/ImageSegmenter.ixx`。**対応:** `IImageSegmenter::lastError()` を共通契約へ加え、`segmentBatch()` が未ready・各item失敗・不正itemの最後の理由を返すようにした。**価値:** App側の一括処理UIが推論失敗を空マスクと誤認せず、ユーザーへ具体的に表示できる。**次に確認:** 実ONNXモデル不在・不正モデル・正常モデルでエラーが期待どおり更新されること。

- **2026-09-04 — 複数セグメンテーションマスクのCore合成:** `ArtifactCore/include/AI/ImageSegmenter.ixx`。**対応:** `combineSegmentationMasks()` に Replace／Union／Intersect／Subtract を追加。入力解像度が異なっても `DepthMap` のbilinear samplingで target座標へ合わせる。**価値:** 人物＋髪、AIマスク＋手動補正、複数推論モデルの結果をQImage経由なしに共通マットへ統合できる。**次に確認:** 異解像度マスクでの境界品質、連続アルファのSubtract意味論、GPU cutoutとのpixel parity。

- **2026-09-04 — セグメンテーション自動適用の受入れガード:** `ArtifactCore/include/AI/ImageSegmenter.ixx`。**対応:** `acceptsSegmentationMask()` と coverage／平均信頼度のしきい値設定を追加。**価値:** 空、または誤って画面全体を前景と判定したマスクを、Appが非破壊プレビューのまま停止・確認できる。**次に確認:** 実モデル別に人物／物体／背景なし素材の適正な閾値を決める。

- **2026-09-04 — OpenCV RotoBrush の IImageSegmenter adapter:** `ArtifactCore/include/AI/RotoBrushImageSegmenter.ixx`、`ArtifactCore/src/AI/RotoBrushImageSegmenter.cppm`。**事実:** 既存 `OpenCVRotoBrushEngine` はGrabCut初期マスク、前景／背景ストローク、Optical Flow伝播を実装済みだが、画像AIの共通契約へ未接続だった。**対応:** canonical BGRA float bufferを既存engineへ明示的に渡し、出力 `CV_8UC1` を `DepthMap` へ変換するadapterを追加。`propagateToNextFrame()` で、初期マスクを作成後の既存 Farneback flow 伝播をCore APIとして公開した。**価値:** モデルが無い環境でも、手動補正付きマットをONNX経路と同じbatch／refine／apply経路に渡せ、連番ではRotoBrushの追従を利用できる。**次に確認:** 現行engineのストローク座標・GrabCutマット・OpenCV例外時・大きなオクルージョンでの実機結果を確認する。

- **2026-09-04 — 軽量タスク facade の配置:** `ArtifactCore/include/Thread/LightweightTask.ixx` に、共有 `QThreadPool` を使う `executeLightweightTask` / `dispatchLightweightTasks` と、完了・キャンセル・失敗状態だけを持つ `LightweightTaskContext` を追加した。**事実:** 既存の `ThreadPool`、`Parallel`、`BackgroundTaskWorkerPool` は粒度や責務が異なる。**価値／懸念:** 短い非同期処理の入口を統一できる一方、context はタスク完了前に破棄できず、タスク内から `wait()` するとデッドロックする。**次に確認:** 実利用箇所を1つ選び、キャンセル・例外・pool飽和時の挙動をビルド／runtimeで検証する。

- **2026-09-04 — 2D Transform Gizmo の視覚ノイズ削減（Scale 中央 Y+ 軸線・Rotate 楕円重ね・軸 sweep 縮小）:** `Artifact/src/Widgets/Render/TransformGizmo.cppm` の `drawScaleCenterHandle` から Y+ 軸線 + tip ハンドルを撤去し、Rotate 描画ブロックから `drawEllipse` 2 本（X 軸赤 / Y 軸緑）と 68° sweep の X/Y 色分け弧 4 本のうち範囲を 36° に縮小。**事実:** 旧 `GIZMO_IMPLEMENTATION_STATUS_2026-04-10.md` の「Scale の中心→四隅 X 線」記述は既に解消済みで、現コードの X 線正体は中央ハンドルの Y+ 軸線だった。Aspect Lock は `isCornerScaleHandle()` 側にあり、Center ハンドルの Y 軸線とは無関係。Rotate リングは `hitThickness = ringThickness * rotateRingHitBoost` で既に hit area と visual thickness が分離済み。**価値／懸念:** X 線ノイズ・4 軸 rainbow 効果・楕円重ねがそれぞれ薄れ、平面/画像レイヤーの Scale と Rotate 操作の視認性が上がるはず。`drawEllipse` ローカル関数（816 行）は未使用になるが残置、hit test・Undo・ショートカットには触れていない。**次に確認:** ユーザー許可後に `Artifact` のモジュールビルドを実行し、`ArtifactTransformGizmo` の IFC が正常に再生成され、Scale 4 隅ハンドル・Center ハンドル・Rotate リング・Leader・Drag arc の描画が既存と一致することを確認。

- **2026-09-04 — M-VP-9 Navigation Contract 現状マップの固定:** `docs/technical/MILESTONE_VIEWPORT_NAVIGATION_CONTRACT_STATUS_2026-09-04.md` を新規作成し、既存実装の静的マップ（Alt+LMB orbit / MMB pan / Wheel zoom / `PreviewOrbitSnapshot` による orientation・pan・zoom 保存復元 / Frame Selected・All・View Undo・Redo の QAction + QShortcut 経路 / `activeViewport()` 系）と未着手項目（navigation cross 表示 / active viewport 細い枠 / preview-only と camera layer の厳密分離 / pivot・orbit source selector / surface snap）を表形式で明文化した。**事実:** `ArtifactCompositionEditor.cppm:9220-9272` の `setPreviewOrbitMode` は camera state のみを snapshot 化し、navigation session フラグ（`isAltOrbiting_` / `isPanning_` / `isAltZooming_`）は含まれない。`maskNavigationLocked` 経路は ON 時の抑制のみ。Work Cursor は配置・中央化・消去・overlay 表示まで既存、Pivot source 切替と surface snap は未着手。**価値／懸念:** AGENTS.md の「RenderScheduler / DX12 パスはシビア扱い」「既存挙動を不用意に変えない」「新規 signal/slot 接続禁止」「QPainter / QImage / QtCSS 禁止」に従うと、navigation cross 追加は Editor → Overlay への状態渡し経路が必要で pane manager (M-VP-2) 移行と密結合のため、Phase 3 では**コード改変ではなく状態マップの固定**で止めた。**次に確認:** ユーザー許可後に (1) preview-orbit snapshot に `isAltOrbiting_` / `isPanning_` / `isAltZooming_` フラグを含めた場合の復元整合、(2) navigation cross を `previewOrbitMode_` ON 時のみ theme token のみで描画する場合の最小実装可否、(3) active viewport 細い枠を pane manager 移行なしで 1 段重ね描画できるかどうか、を順に判断する。

- **2026-09-04 — FFmpeg C API のモジュール境界:** `ArtifactCore/src/Codec/FFmpegThumbnailExtractor.cppm` では、vcpkg の FFmpeg ヘッダが C リンケージを自動付与しない構成だったため、`extern "C"` でグローバルモジュールフラグメント内のヘッダ群を包む必要がある。**事実:** 未解決シンボルが `?av...` と C++ 修飾されていたが、修正後は通常リンクまで進み、`/WHOLEARCHIVE` は複数定義を起こした。**価値／懸念:** C++20 module の import／リンク問題に見えても、まず ABI のリンケージ名を確認する。**次に確認:** FFmpeg を参照する他の module 実装でも同じヘッダ配置を維持する。

- **2026-09-02 — C++ module split target の IFC 参照は分岐順に注意:** `ArtifactCore/CMakeLists.txt` では `src/Mask/` の包括分岐が個別の `RotoMask.cppm` 分岐より先に評価されるため、後置した個別参照設定だけでは実際のコンパイルコマンドに反映されない。**関連:** `ArtifactCore/CMakeLists.txt`, `RotoMask.cppm`, `ConfigSchema.cppm`, `Artifact/CMakeLists.txt`。**価値／懸念:** split target 化では「設定が存在する」だけでなく、最終的な source property の適用順と生成コマンドへの反映を確認する必要がある。**次に確認:** ユーザー許可後の再生成・ビルドで、対象コマンドに `/reference` が現れ、C1199 が解消することを確認する。

- ビルド・テスト・CMakeはユーザーの明示許可後に実行する。
- runtime検証済みになった項目は、このファイルからアーカイブへ移す。
- 実装済みの細かな履歴や重複した検証候補は、新規Insightとして追加せずアーカイブを更新する。

- **2026-10-11 — MPMのcache miss seekは旧runtime stateを残し得る:** `ArtifactCore/src/Physics/PhysicsSystem.cppm::restoreSoftBodySnapshots()` はSoftBody/Cloth/MPMの全snapshotを先行検証し、一対象でも欠落すると何もrestoreせずfalseを返す。`Artifact/src/Composition/ArtifactAbstractComposition.cppm` の `setFramePosition()` / `goToFrame()` はこの結果を使ったMPM reset/replayを行わず、逆行時の `resetSoftBodiesToRestState()` もsoft bodiesだけをresetする。**事実:** MPM baseline snapshotからの再simulationはこの経路に存在しない。**価値／懸念:** cache evictionや直接seek後に前のframeのMPM状態が残り、表示frameとsolver stateがずれる可能性がある（runtime未検証）。**次に確認:** 砂用baseline保持、nearest-prior restore、fixed-step replayをcomposition/workerごとに持つ設計とし、順再生・direct seek・逆行往復・eviction後を同一state比較する。

- **2026-10-11 — Hencky/DP principal projectionをCPU MPMへ接続:** `ArtifactCore/include/Physics/MpmSolver2D.ixx` と `ArtifactCore/src/Physics/MpmSolver2D.cppm` に `MpmConstitutiveModel::GranularDruckerPrager` を追加し、principal Hencky projection、Kirchhoff→PK1 stress、`Fp_new=Fe_new⁻¹F` plastic updateをCPU経路へ接続した。Granular modeではsolid fractureをskipしGPU backend selectionを拒否、model/friction設定は粒子生成後に変更不可とした。**価値／懸念:** existing corotated/von-Mises経路はdispatch前後で維持し、粒子寿命とCPU-only制約を局所化する。Solver modeはLayer/save/load/snapshot metadataへ未接続で、fixture/build/runtime確認も未実施。**次に確認:** 2×2 SVD再構成（特に回転・重複特異値）、圧縮・extension・せん断return map、`F=FeFp`、旧preset回帰を許可後に検証し、その後専用Layerへbindする。

- **2026-10-11 — 2×2 SVDは重複特異値で固有ベクトル角が不定:** `MpmSolver2D.cppm` の既存analytic angle式は `q≈0` または `r≈0` のときU/V角を個別に0へ置く。重複特異値ではその自由度選択で剛体回転を再構成できない場合がある。**変更:** granular constitutive pathだけ `FeᵀFe` の右固有vectorと `U=Fe V Σ⁻¹` でprincipal frameを作り、`σ1=det(Fe)/σ0` で小さい特異値の差し引きcancelも避ける。legacy solid pathは未変更。**未検証:** 回転・等方伸長・近重複特異値fixtureでSVD再構成誤差を評価していない。**次に確認:** constitutive unit testで正detのランダム2×2行列について `UΣVᵀ≈Fe` と `det(U),det(V)>0` を測定し、しきい値近傍を確認する。

### 2026-10-12 — MPM density surface packet failure remains silent at queue capacity

- **関連:** `Artifact/src/Layer/ArtifactMpmSand2DLayer.cppm`、`Artifact/src/Render/PrimitiveRenderer2D.cppm`、`Artifact/include/Render/RenderCommandBuffer.ixx`。
- **確認できた事実:** MPM Layerは粒子packetではなく、128x96 density fieldからbounded triangle meshを作って単一`MpmSurfacePkt`として送る。`PrimitiveRenderer2D::drawMpmSurface()`はcommand buffer満杯時にfalseを返しpacketを追加しない。Layerは戻り値を無視するため、そのframeの砂面は欠落する。CPU Render Queue側にはsolver densityから生成する専用`getThumbnail()`があるが、GPU Composition Viewの同一frame drawで代替描画はされない。
- **価値／懸念:** 粒子ごとのpacket overflowは避けている一方、command queue全体が満杯ならMPM面の可視性保証はない。Layer側のmesh容量は全cell三角形数を上限として確保済みで、問題はmesh vertex capacityではなくcommand packet slotの拒否。
- **未検証／次に確認:** packet capacity到達をruntimeで再現していない。優先slot確保、packet admission計画、または明示的な描画失敗診断のいずれかを設計し、他Layerの順序・bounded allocation・GPU submit失敗との責務を比較する。QPainter/QImageや粒子ごとのprimitive fallbackは追加しない。

## Weekly insight files

- **2026-10-11 — MPM砂は応力・塑性・破断・GPU tailを一体で切替える:** `ArtifactCore/src/Physics/MpmSolver2D.cppm` の静的読解では、CPU応力はfixed-corotated PK1 (`firstPiolaKirchhoff`)、塑性投影は別関数のvon Mises型、塑性投影後に固体向けfracture判定が実行される。GPU substepもGPU弾性更新のあとCPUで塑性・collider・fracture tailを実行する。一次資料 Klár et al. 2016はHencky対数主ひずみのreturn mapping、extension時のtip投影、yield gradientの偏差方向を使う体積保存型non-associated flowを説明する。**訂正:** 同entryの「2D係数`α=sin(phi)`」は標準Frobenius normの`||dev(τ)||F`には誤り。現在のproject契約の`y(τ)=||dev(τ)||F + α tr(τ)`と2D Mohr-Coulomb関係`|τ1-τ2|/2 = -(tr(τ)/2)sin(phi)`から、`α=sin(phi)/sqrt(2)`を独立に導く。Klár et al.本文の3D係数変換はこの2D係数の根拠として直接流用しない。独立dilatancy角を必須とする先の案は採用した2D体積保存型projectionと異なるため、設計書を訂正した。**価値／懸念:** 塑性モデルだけの追加は応力との不整合を生み、GPUを許すと既存shaderとfracture副作用を招く。砂対応は応力生成・return mapping・粒子寿命・backend policyを一つのconstitutive strategyとして切替え、既存solid経路を固定する必要がある。**未検証:** 現行solverの積順序・平面応力近似にこの投影を組み込む数値安定性。**次に確認:** 圧縮・せん断・extensionの解析fixtureを作り、論文式、Kirchhoff応力符号、`F=Fe Fp`更新を照合する。

### 2026

- [ISO Week 41 (2026-10-05 – 2026-10-11)](insights/2026/ISO-W41.md) — 129 entries
- [ISO Week 40 (2026-09-28 – 2026-10-04)](insights/2026/ISO-W40.md) — 94 entries
- [ISO Week 39 (2026-09-21 – 2026-09-27)](insights/2026/ISO-W39.md) — 164 entries
- [ISO Week 38 (2026-09-14 – 2026-09-20)](insights/2026/ISO-W38.md) — 31 entries
- [ISO Week 37 (2026-09-07 – 2026-09-13)](insights/2026/ISO-W37.md) — 98 entries
- [ISO Week 36 (2026-08-31 – 2026-09-06)](insights/2026/ISO-W36.md) — 79 entries
- **2026-10-11 — 粒子描画packetの容量をreset後にbounded再利用する:** `RenderCommandBuffer::reset()`はsubmit後に呼ばれ、`ParticlePkt`はGPU submit完了後に破棄されるため、その境界で最大4本のparticle vectorを回収し、次のaccepted rvalue draw時に呼出元へ返すpoolを追加した。**価値:** MPM Layerは通常フレームで確保済みvectorを使い回せる。**懸念／未検証:** 初回warm-up、4本を超える同時packet、別のworker command bufferでの予約量、vector返却後の再使用をビルド・runtime・allocator計測で確認していない。**次に確認:** 安定フレームのallocator traceと粒子数別のpool hit率を取得し、pool上限と最大粒子数を決める。
- **2026-10-11 — Granular seek snapshotには外側fixed-step clockも必要:** `MpmSnapshot2D`はsolver内部accumulatorを持つ一方、`PhysicsSystem`はComposition単位の別accumulatorでgranular solverを進めていた。solver snapshotだけを戻すと、特にfpsとfixed timestepが整数比でないCompositionで再生位相がずれる。**変更:** snapshotにGranular用world accumulatorを付け、12フレーム間隔の共通checkpointとbaseline replayを実装した。現在のrestoreコードは全Granular solverのframe snapshotとclock remainderを先に検証し、最寄りの共通checkpointを一括復元する。checkpointがない場合は全baselineを復元してworld clockを0へ戻す。Material/solver再登録はworld cacheを消去しclockをresetして新baselineを作るため、frame snapshotへ静的なmodel/config IDは含めない。公開`MpmSolver2D::restoreSnapshot()`の別instance/config復元は同一設定を呼出側前提とし、この互換性をAPI内では照合しない。**未検証:** 異なるfpsでの連続再生とseek後の状態一致、cache trim後のnearest checkpoint動作。**次に確認:** 25/30/60fpsの前進再生とrandom seek後を同じframe stateで比較し、maxCachedFrames端で再現する。

- **2026-10-12 — MPM solverの更新をLayer active範囲へ同期:** `ArtifactAbstractComposition::evaluateLayerCollisionPairs()` は `isActiveAt(position_)` によって非active Layerをcollision対象から除外する一方、`PhysicsSystem::advanceGranularMaterialSolvers()` はworld内の全Granular solverを更新していた。**変更:** Compositionの既存frame評価でMPM solverのactiveフラグを同期し、active時だけfixed-step updateする。**価値／懸念:** playhead配置やin-pointより前の再生・seekで、表示されない期間に砂だけ進む不整合を防ぐ。**補足事実:** outer world clockはComposition内で共有され、inactive solverへの`update(fixedDt)`だけがskipされる。world clockはその間もfixed stepを消費し、solver固有accumulatorへinactive時間は加算されない。snapshotには共通clockのfractional remainderが保存される。**未検証:** active境界を跨ぐ通常再生・逆再生・checkpoint replayでの数値一致。**次に確認:** active前後のsolver snapshotを比較し、非active期間で粒子状態が不変、再active後にinactive期間分をcatch-upしないこと、seek往復で同じstateへ戻ることを確認する。

- **2026-10-12 — MPM surface recycle poolは同一frameのpacket上限に揃える:** `RenderCommandBuffer::reset()` はsurface packetから頂点vectorを回収するが、pool予約は2本固定だった。frame packet capacity内に3件以上のMPM surfaceがあると後続packetのmesh storageが破棄され、Layerが次frameに容量付きbufferを得られなくなる可能性があった。**変更:** pool metadataをcommand packet capacity分だけ初期reserveし、accepted surface packetを全てreset時に再利用可能にした。**価値／懸念:** packet数でboundedのまま複数Layerを継続描画できる。最大capacity時のmetadata overhead、初回buffer warm-up、submit/reset後の実再利用は未計測。**次に確認:** 3件以上のMPM Layerを複数frame記録し、mesh容量が維持されることと同時に、packet overflow時の明示挙動を検査する。

- **2026-10-12 — CPU Render QueueのMPM thumbnailはsolver密度から生成する:** CPU Render Queueの`renderLayerSurface()`はMPM固有分岐を持たず、Layerのthumbnail APIへfallbackしていた。`ArtifactMpmSand2DLayer`に`getThumbnail()`を追加し、現在frameのMPM solverからbounded density fieldを埋め、固定サイズの`ImageF32x4_RGBA`面を作ってQImage API境界へ変換する。GPU描画と同じ支持半径、sand色、alpha曲線を共有し、その式と色・alpha閾値はLayer内の共通定数/helperへ集約した。**確認できた事実:** CPU MFRはworkerごとに`cloneCompositionSnapshot()`を作成し、GPU workerとXPU CPU laneも独立composition snapshotを持つため、このLayerの再利用surface bufferは各worker clone内に分かれる。Diligent `MainRTVFormat`はsRGBで、`ImageF32x4_RGBA::toQImage()`はlinear surfaceをsRGBへ変換する。**価値／懸念:** Software Render Queueで汎用thumbnailを出す不整合を除き、既存QImage compositorへCPU surfaceを供給する。mask/blend適用後の視覚結果、GPUとのtransfer/alpha一致、画質・frame時間は未検証。**次に確認:** GPU/CPUの同一scene/frameで透明edge、mask/blend、色差と出力寸法を比較し、MFR cloneでのruntime再現を検証する。
- **2026-10-11 — MPM粒子seed領域と計算domainは別の寸法契約が必要:** `createGranularMaterialGrid`が初期粒子配置のwidth/heightから格子Nx/Nyも作っていた。sandboxの初期粒子はLayer内の一部分に置くため、そのままでは粒子がseed領域より下へ移動する格子が足りない。**変更:** 初期seed boundsとsimulation domain boundsをAPIで分離し、計算domainをLayer全体にしてparticle boundaryを設定した。**未検証:** 格子境界上の粒子安定性と堆積見た目。**次に確認:** 全source boundsへの落下、左右壁、下端堆積、collider併用時をruntime captureで確認する。


- **2026-10-12 — MPM砂の円collider中心点に決定的な退避方向を与える:** `ArtifactCore/src/Physics/MpmSolver2D.cppm::resolveColliders()` は円の中心とparticleが一致すると距離ゼロでcollider解決をskipしていた。**変更:** GranularDruckerPragerだけ+x法線を退避方向にして円周へ位置補正し、既存CorotatedVonMisesは従来どおりskipする。**価値／懸念:** 初期配置や大きなstepで円の内部中心に入った砂粒子が永久に中心へ残る経路をなくす。中心には物理的に一意な法線がなく、+xは決定的な退避規約であり、接触の見た目・複数collider順序は未検証。**次に確認:** 中心静止／中心へ入射／小半径／ゼロ半径で有限値、位置補正、反発・接線摩擦を解析fixtureで確認する。


- **2026-10-12 — MPM砂のpolygon境界で最近接辺から法線を復元:** `MpmSolver2D.cppm::resolvePolygonCollider()` は内部particleの位置差で法線を得るため、境界上ではゼロ長になり、ray-crossingの内外判定が境界点を除外する場合は応答自体がなかった。**変更:** GranularDruckerPragerで最近接辺の向きとpolygon signed areaから外向き単位法線を復元し、距離二乗`1e-10`以下の境界点も接触応答する。既存CorotatedVonMisesの内外判定・法線処理は維持。**価値／懸念:** 辺上での入射速度に応答できるが、頂点の複数辺同距離、退化polygon、非有限頂点は別途安定性確認が必要。**次に確認:** CW/CCW polygon、辺中央・頂点・境界上の内向き／外向き速度、退化形状を解析fixtureとruntimeで確認する。


- **2026-10-12 — MPM砂Layerのdetachはworld-scoped solverを明示解除する:** `ArtifactAbstractComposition::Impl::removeLayer()` / `removeAllLayers()` はLayerをvirtual `setComposition(nullptr)`でdetachし、`ArtifactMpmSand2DLayer::setComposition()` はbase更新前に旧Composition runtime world IDとLayerIDでGranular solverを削除する。**価値／懸念:** MPM砂についてはLayer destructor頼みではなくComposition detach時に登録を閉じる静的経路がある。Undo後のsolver再生成と実world registry状態はruntime未確認。通常Shape material solverのglobal registry寿命とは別件。**次に確認:** 削除・Undo・Composition破棄後にworld-scoped granular registryが重複せず、再追加で初期baselineが一つだけ生成されることをruntime確認する。

- **2026-10-12 — Cloner Plain modifier の2D affine集約:** `Artifact/include/Layer/ArtifactCloneEffectSupport.ixx` と `Artifact/src/Render/ShaderManager.cppm`。**事実:** GPU placement recordはoffset・Z rotation・local scale XYを保持し、単一PlainのD3D12 scale＋rotation raster probeはCPU参照と画素一致した。modifier列の合成ではtranslationを前段の累積rotation／scaleで写し、累積transformがrotation＋diagonal scaleとして表せる範囲（例: uniform scale＋rotationの後へnonuniform scale）をGPU対象とし、nonuniform scaleの後にZ rotationが来るshear生成順序はfallbackする。**未検証の仮説:** placement recordに2×2 linear basis (4 floats) とtranslationを保持すれば、shearを含む複数Plain modifierもGPU上で忠実に表せる可能性がある。**価値／懸念:** より広いmodifier chainを扱える一方、record stride増加、VS/compute ABI更新、3D Z semantics、親stack合成を確認する必要がある。**次に確認:** 新しい `--cloner-plain-chain` fixtureで実descriptorとCPU/GPU matrixを照合し、shearを生む順序をfallbackさせることもruntime確認する。


### 2026-10-12 — MPM density raster work is spatially bounded per particle

- **関連:** `ArtifactCore/src/Physics/MpmSolver2D.cppm::writeDensityField()`、`Artifact/src/Layer/ArtifactMpmSand2DLayer.cppm`。
- **確認できた事実:** field dimensions/pixel count are capped, support radius is limited to 8 pixels per axis, non-finite/out-of-field particles are skipped, and each particle loops only over its clipped floor/ceil bounding box. With an exact 8-pixel radius, inclusive floor/ceil endpoints yield at most 18x18 tested samples per particle; work scales with active particle count rather than scanning the entire field per particle.
- **価値／懸念:** The density calculation has a finite per-particle stencil and fixed field storage, but the authored maximum 500x200 population can still imply high total CPU work. Floating-point edge behavior and per-frame cost at 100k particles have not been measured.
- **次に確認:** Profile `writeDensityField()` at default and maximum UI particle counts, including large deformation determinants and field-edge particles; verify the support clamp and density coverage in rendered output.

### 2026-10-12 — MPM砂の壁clampとdensity fieldの支持上限を共有

- **関連:** `ArtifactCore/src/Physics/MpmSolver2D.cppm`、`Artifact/src/Layer/ArtifactMpmSand2DLayer.cppm::mpmSupportRadius()`、`PhysicsSystem::createGranularMaterialGrid()`。
- **確認できた事実:** density writerは出力sample間隔から最大支持半径を決める一方、壁clampはsolver側で独立に粒子体積から半径を算出していた。変形膨張時、両者のclamp上限が異なり得る。
- **変更:** Layerがfield寸法から計算した基準支持半径をGranular solver生成引数で渡し、particle populationおよびworld baseline登録前に静的solver設定として確定する。CPU `stepOnce()`、GPU readback tail、density writerはすべて`granularSupportRadiusLimit_`を参照し、各自のcell/field最大値ともminを取る。snapshotには含めず、solver再生成時にLayerから再設定する。
- **価値／懸念:** solver粒子の壁距離とfield kernelが同じ基準半径上限を使い、writer入力が8px超でもbounded clampで処理できる。GPU sandboxは現状非対応のためGPU tailは将来の整合用。構成の実コンパイル、edge fieldの描画、再生成後の値維持は未検証。
- **次に確認:** helperの返却値が128x96 field上限以内であること、solver登録前に値が確定すること、四辺・異方格子・膨張変形時のcoverageを数値／runtimeで確認する。

### 2026-10-12 — MPM support radiusの粒子数積をdoubleで計算

- **関連:** `Artifact/src/Layer/ArtifactMpmSand2DLayer.cppm::mpmSupportRadius()`。
- **確認できた事実:** helperは`columns * rows`をint乗算してfloatへ変換していた。当時のLayer UI上限500×200では積は安全だが、当時のPhysicsSystem granular factoryは最大262,144 particleまで受け入れ、helperはLayer用の寸法・resolution契約を自身では検証していなかった。2026-10-12にfactoryをLayer contract同等の100,000粒子へ変更済み。
- **変更:** helperで列数2..500、行数2..200と正の有限Layer寸法を検証する。seed寸法、particle count、particle volume、nominal radius、field capまでdoubleで計算し、最終radiusが正・有限・float範囲内であることを検査してから変換する。
- **価値／懸念:** int積overflowを避け、無効resolutionを黙って1へ補正しない。実際の最大寸法・粒子数での数値比較は未実施。
- **次に確認:** UI上限とfactory上限で旧float式との値、8px field cap、density writerの有限性を数値fixtureで比較する。

### 2026-10-12 — MPM粒子格子幅を密なseed軸に合わせる

- **関連:** `ArtifactCore/src/Physics/PhysicsSystem.cppm::createGranularMaterialGrid()`。
- **確認できた事実:** grid cell sizeはX/Y粒子間隔の大きい方から決められ、縦横比が異なるseedでは間隔の小さい軸の複数列が同じ少数セルへ写り得た。Layer既定seedは384×168px、96×48配置。
- **変更:** 両軸間隔をdoubleで求め、小さい方を1px下限でcell sizeに採用した。格子ノード予算とCFL step上限は既存の制約を維持する。
- **価値／懸念:** 異方配置でも密な方向の粒子密度に見合う格子解像度を保てる。格子・粒子の数値品質とCFL棄却境界は未検証。
- **次に確認:** 既定および極端な列行比でcell mappingと格子予算を数値確認し、許容パラメータ範囲を実機で受け入れる。

### 2026-10-12 — MPM density rasterをfield sampleへ離散正規化

- **関連:** `ArtifactCore/src/Physics/MpmSolver2D.cppm::writeDensityField()`。
- **確認できた事実:** 640×480 Layerの128×96 fieldはworld sample間隔が5×5px。100,000粒子では粒子体積0.64512px²、既存nominal support radius約0.801pxでsample missし得た。半pixel対角長下限だけでは最悪位相でカーネル重みがほぼ0となり、peak clampにより離散体積寄与を保てないことも数値確認した。
- **変更:** Granularの支持半径下限をfield pixel diagonal（tiny epsilon付き）にし、各particleのfield内離散kernel weight合計をdoubleで求めて `currentVolume / (pixelArea * weightSum)` で正規化。field edgeで切られた領域は残ったsampleへ再分配される。各軸8px以内という既存上限から、18×18固定長の局所weight配列へcandidate kernelを1回計算・保存してから加算する。追加heap確保も二重の距離計算もない。個別particle peakはfloat表現上限だけを適用し、field加算後のdensityのみ1にcap。非Granular writer式は維持。
- **価値／懸念:** field sample位相とfield edge clippingに対する粒子ごとの離散体積寄与のぶれを抑え、単独kernelの輪郭まで個別peak capで縮めない。overlap時のpixel density capは局所寄与を減らし得る。固定局所配列は1,296 bytesでbounded。コード上のcandidate評価上界は18×18×100,000で約3,240万/field生成（field初期化12,288要素は別）だが、これは算術上界でありframe時間の実測ではない。
- **次に確認:** 100k粒子をsubpixel phase、layer edge、変形determinant違いでラスタ化し、pixelAreaを掛けたfield積分、overlap saturation、rendered qualityとframe timeを確認する。

### 2026-10-12 — MPM砂のG2Pで旧粒子位置の速度勾配を使う

- **関連:** `ArtifactCore/src/Physics/MpmSolver2D.cppm::gridToParticle()`、`updateDeformationGradient()`。
- **確認できた事実:** CPU stepはG2Pで`p.pos += v*dt`した後、変形勾配計算が移動後の`p.pos`から格子速度勾配を再標本化していた。G2P時の速度格子は移動前の粒子位置に対応する。したがって砂の`F`更新が1step先の位置を使う不整合だった。
- **変更:** Granular CPU経路のみG2Pの同じ旧位置・同じ格子 stencilで`gradV`を計算し、`p.pos`更新前に`F=(I+dt*gradV)F`を適用。Corotated経路の既存更新順は変更しない。
- **価値／懸念:** Hencky応力・体積・支持半径を決める`F`と、APIC速度転送の評価位置を一致させる。追加のstencil走査をなくすが、G2P内の勾配演算は増える。affine velocity fieldの一致と堆積挙動はruntime未検証。
- **次に確認:** 一様並進で`gradV=0`、affine fieldで所定勾配となること、1particle変形とshear boxをCPU solver fixtureで確認する。

### 2026-10-12 — APIC P2Gのaffine momentumへ粒子質量を掛ける

- **関連:** `ArtifactCore/src/Physics/MpmSolver2D.cppm::particleToGrid()`、`ArtifactCore/src/Graphics/MpmCompute.cppm::g_P2GBody`。
- **確認できた事実:** P2Gは格子運動量に `w * (m*v + C*diff)` を加算していた。`C`はG2Pで`velocity * diff * 4/dx²`として更新されるため速度/長さ、`C*diff`は速度単位であり、運動量項へ足すには粒子質量が不足していた。CPUとGPU shaderの両方に同じ式があった。
- **変更:** CPU/GPU両経路を `w*m*(v + C*diff)` に修正した。GPU shaderのfixed-point accumulationも同じ値式を使う。
- **価値／懸念:** APIC affine transferの次元を揃え、各粒子の格子運動量寄与が質量に比例するようにする。既存Corotated MPMの軌跡も修正前から変化する。運動量・角運動量保存とCPU/GPU parityは未検証。
- **次に確認:** constant translationとrigid rotationでP2G→G2Pの線形／角運動量を比較し、CPU/GPUの量子化誤差を測る。

### 2026-10-12 — Granular CPU格子境界の接線摩擦を一度だけ適用

- **関連:** `ArtifactCore/src/Physics/MpmSolver2D.cppm::stepOnce()`。
- **確認できた事実:** CPU stepはP2G直後とforce・grid velocity更新後の2回`applyBoundaryConditions()`を呼ぶ。Granular境界条件は壁法線速度を止め、毎回接線速度へ`1-boundaryFriction`を掛けるため、同じstep中に摩擦が二重適用されていた。GPU P2G/substep shaderの境界応答は速度更新後に一度行う。
- **変更:** Granular CPUではP2G直後の境界応答をskipし、stress forceとgrid velocityを反映した後の一度だけにした。Corotated legacyの呼出順は維持。
- **価値／懸念:** 壁際の砂の接線速度を二重に減衰させない。粒子境界clamp/collider responseも別に存在するため、衝突での全体摩擦量はruntime未検証。
- **次に確認:** 床上の水平運動量減衰を1step/固定時間で測り、CPUとGPUの境界応答回数・摩擦係数を比較する。

### 2026-10-12 — MPM初期粒子格子を一括reserve

- **関連:** `ArtifactCore/src/Physics/MpmSolver2D.cppm::addParticlesGrid()`。
- **確認できた事実:** 当時のGranular factoryは最大262,144のfactory上限内（Layer UI上限100,000）でseed粒子を作っていたが、2026-10-12にfactoryをLayer contract同等へ変更済み。汎用`addParticlesGrid()`のparticle volume denominatorは`nc*nr`をsigned intで計算していた。任意の大きなAPI入力ではoverflowし得る。またcapacityを逐次拡張していた。
- **変更:** columns/rowsを`size_t`化し、`max_size()/rows`で積overflowを先に拒否。チェック済み積をreserve数とvolume denominatorで共有し、`size()-max_size()`も検証してから必要容量を一度reserveする。
- **価値／懸念:** 初期投入中の段階的再確保を避け、particle volumeを実粒子数で除算する。1回の大容量cold-path allocationは残り、allocation時間は未計測。
- **次に確認:** 既定・最大粒子数で生成時間とピークmemoryを計測し、Composition snapshot baseline複製を含めた起動コストを確認する。

### 2026-10-12 — MPM砂の格子node予算をComposition単位へ集約

- **関連:** `ArtifactCore/src/Physics/PhysicsSystem.cppm::createGranularMaterialGrid()`。
- **確認できた事実:** factoryは1 solverあたりgrid nodeを4,194,304以下に制限していたが、同じCompositionに複数MPM Sand Layerを登録すると各Layerが個別上限を使え、solver grid memory総量に同じ上限がなかった。solverには公開`gridNx()/gridNy()` accessorがある。
- **変更:** world ID内の既存solver node数をchecked `size_t`積で加算し、新規gridを加えたComposition総数が同じ4,194,304以下か登録前に検証する。差し替え対象Layerの旧gridはsteady-state計算から除外する。
- **価値／懸念:** 多数のMPM Layerでgrid memoryがLayer数に比例して無制限に積み上がることを避け、既存単solver最大値を維持する。solver replacement中は旧gridと新gridが一時共存し、他Composition/Render Queue cloneをまたぐprocess-wide総量はこのbudget対象外。
- **次に確認:** 複数Layer・複数world・Render Queue cloneでgrid allocation bytesとピークを計測し、world-local capがmemory budgetとして十分か確認する。

### 2026-10-12 — MPM砂のparticle payload予算にlive solverを含める

- **関連:** `ArtifactCore/src/Physics/PhysicsSystem.cppm::createGranularMaterialGrid()`、`trimGranularWorldSnapshots()`。
- **確認できた事実:** 既存64 MiB budgetはbaselineとcheckpoint粒子payloadだけを数え、稼働solverの粒子配列を含めていなかった。snapshot trimmingもvector sizeを数えてcapacityの確保分を含めていなかった。
- **変更:** solver登録前のworld payload判定は既存live particle capacity + baseline capacityに加え、新solverと作成済みbaseline snapshotの実capacityを確認する。baseline snapshotは確認した同じ値を登録へ移す。checkpoint capture前にincoming particle payload分を予約し、古いcheckpointを先にevictしてからsnapshotをコピーする。枠を確保できない場合はそのcheckpointを作らない。trim計算はlive、baseline、checkpointのparticle/fragment-index vector capacityを合算する。
- **価値／懸念:** seek履歴だけでなくsimulation stateのresident particle payloadもComposition単位で制限でき、checkpointコピー中の粒子payload二重保持も抑えられる。allocator node/map overhead、vector copy capacityの実装差、solver replacement中の旧新共存は含まず、全process/worldの総量制限ではない。
- **次に確認:** capacityベースの見積もりと実allocator使用量、checkpoint capture/replacement peakを計測し、payload budgetとしての差を定量化する。

### 2026-10-12 — MPM粒子境界余白を支持半径へ制限

- **関連:** `ArtifactCore/src/Physics/MpmSolver2D.cppm::stepOnce()` とGPU readback tail。
- **確認できた事実:** 粒子境界clampは砂でもセル半幅を基礎余白としていた。格子幅をseed密度に合わせて細分化すると、この余白も増える場合がある。Granular solverには既にfield由来の共通支持半径上限があり、現在体積由来半径もclampに使っている。
- **変更:** Granularの基礎余白を `min(cellSize/2, granularSupportRadiusLimit_)` にし、CPU/GPU両経路で共有した。Corotated solidは従来のセル半幅のまま。
- **価値／懸念:** 解像度変更による粒子中心の境界距離増大を支持kernel基準で抑える。wall/node boundaryとの連成や細格子時の堆積形状は未検証。
- **次に確認:** 4面の壁衝突と床堆積で、node boundary response・particle clamp・density surfaceの間隔を数値/runtime確認する。

### 2026-10-12 — GPU Cloner fallback行列へstack scaleを反映

- **関連:** `Artifact/include/Layer/ArtifactCloneEffectSupport.ixx`、`Artifact/src/Render/ArtifactCompositionViewDrawing.cppm`、Cloner GPU placement。
- **確認できた事実:** GPU eligible用streaming列挙はCPU clone transformを生成し、GPU matrix照合失敗時のsprite fallbackとcomposition field中心計算に同じ行列を使う。Plain modifierのXY scaleがplacement dispatchへ渡る一方、このstream行列には反映されず、generator transformがidentityの場合はmodifier適用前にcloneを除外していた。transform stackもshear-freeならrotation＋diagonal XY scaleとして既存placement recordへ集約できる。
- **変更:** CPU参照行列へstackとmodifier translation／Z rotation／XY scaleを適用してからidentity判定するようにした。stack XY scaleはshearを生まない順序に限定してplacement recordへ合成する。
- **価値／懸念:** GPU fallback時の表示・field queryでmodifier scaleが欠落する経路を塞ぎ、identity配置でもmodifierが変形するcloneを保持する。10,000件のshear-free affine合成を数値比較したが、Viewer runtimeの画素一致は未確認。
- **次に確認:** GPU dispatch失敗fixtureでtranslation／rotation／scale stackとPlain chainのCPU fallback画像をGPU placement結果と比較する。

### 2026-10-12 — GPU Cloner eligibility の毎レイヤー構築コスト

- **関連:** `Artifact/src/Render/ArtifactCompositionViewDrawing.cppm::drawWithPlacementObserverIndexed`、`Artifact/include/Layer/ArtifactCloneEffectSupport.ixx::drawWithGpuEligibleClonerGridIndexed`、`Artifact/src/Layer/ArtifactAbstractLayer.cppm::layerGenerators()` / `layerCloneModifiers()`。
- **確認できた事実:** Composition Viewのindexed draw wrapperはGPU placement observerが有効な場合、描画対象レイヤーごとにGPU eligibilityを試す。helperは候補判定の前に値返しAPIの`layerGenerators()`と`layerCloneModifiers()`を呼ぶ。前者はNamedVectorを新規構築してbuiltin descriptorを同期し、後者もNamedVectorを構築して互換time-offset/sequence descriptorをpushする。
- **推測／未検証:** Clonerを持たない多数のレイヤーでもフレームごとにこのコールド寄りdescriptor生成が走り、非対象レイヤーでは後続CPU列挙時に同種の取得が重複する可能性がある。実際のheap allocation回数・frame time寄与は未計測。
- **価値／懸念:** GPU Cloner導入で描画対象全体へコピーやアロケーションを波及させると、ClonerなしCompositionのCPUコストまで増える。GPU可否判定より前にcheapな候補metadataを確認できれば、descriptor合成をCloner対象だけへ絞れる。ただし明示的generator descriptorだけを持つLayerを取りこぼさない検索方法が必要。
- **次に確認:** `ArtifactAbstractLayer`の内部generator stateを走査せずに候補有無を判定できるAPI／generation cacheの有無を調査し、allocation計測付きで非Cloner多数ケースを比較する。ReactiveEvents、MPMを変更せず、Clone descriptorだけを対象にする。

### 2026-10-12 — MPM surface packet rejection時の表示欠落

- **関連:** `Artifact/src/Layer/ArtifactMpmSand2DLayer.cppm::draw()`、`Artifact/src/Render/PrimitiveRenderer2D.cppm::drawMpmSurface()`、`Artifact/include/Render/RenderCommandBuffer.ixx::tryAppend()`。
- **確認できた事実:** MPM Layerは`drawMpmSurface()`のbool結果を無視する。surface packetはqueue満杯時に拒否され、verticesはLayer所有bufferへ戻るため次回再利用できるが、そのframeでは砂surfaceが描画されない。owner queueは最大16,384 packetを事前予約し、一般packetも同じ満杯条件でappend失敗する。
- **変更:** `ArtifactIRenderer::Impl::drawMpmSurface()`で、入力packetが有効かつowner queue満杯の時だけ既存`submitQueuedDraws()`を実行し、active render target復元後に一度再試行するようにした。
- **価値／懸念:** MPM surface packetは先行packetの後へ再投入され、queue容量を超えたフレームでもsurfaceを記録できる。満杯時は途中submitでbatchingが分かれる。GPU runtimeでtarget/order parityとoverflow頻度は未確認。
- **次に確認:** queue満杯fixtureで既存packetとMPM surfaceの画面順序・target維持をruntime確認し、途中submitの頻度とframe costを計測する。

### 2026-10-12 — MPM APIC CFL支持半径を二次カーネルに合わせる

- **関連:** `ArtifactCore/src/Physics/MpmSolver2D.cppm::updateGranularTimeStepForState()`、`weight()`。
- **確認できた事実:** 二次B-spline重みは正規化距離`|r|<1.5`でのみ非ゼロ。したがって4×4 stencilで`C*dpos`のFrobenius上限を取る距離は各軸1.5 cell、2Dユークリッド距離で`1.5*sqrt(2)*dx`。既存CFL定数`2*sqrt(2)*dx`は支持距離より33%大きかった。
- **変更:** APIC affine速度上限を`1.5*sqrt(2)*dx*||C||F`へ修正し、計画書のCFL説明も更新した。
- **価値／懸念:** kernel支持範囲を保守的に覆いながら、不要に小さな内部時間刻みを避ける。実際のparticle-grid位相での最大寄与、安定刻み、堆積挙動は未検証。
- **次に確認:** stencil内の全粒子位置位相と一般`C`行列に対し、各`C*dpos`がこの上限以下であることを数値fixtureで確認し、限界CFLケースで安定性とstep数を比較する。

### 2026-10-12 — MPM CFL波速は最も低密度の粒子で制約する

- **関連:** `ArtifactCore/src/Physics/MpmSolver2D.cppm::updateGranularTimeStepForState()`、`addParticlesGrid()`。
- **確認できた事実:** solver APIは複数回の粒子grid追加を許し、それぞれ異なる密度から粒子mass/volume0が決まる。CFL更新は粒子ごとに密度を走査していたが、ループ後に最後の粒子の密度だけで縦波速を評価していたため、追加順序で刻みが変わり得た。
- **変更:** 各active粒子の`(lambda+2*mu)/(mass/volume0)`を計算し、最大波速をCFLに使用する。
- **価値／懸念:** 複数密度の混在時にも、低密度粒子が要求する刻みを満たす。粒子密度差が大きい条件で実際の安定上限と保守性は未検証。
- **次に確認:** 同じ粒子集合を逆順追加して状態・刻みが一致すること、低密度粒子の追加で刻みが同じか小さくなることをsolver fixtureで確認する。

### 2026-10-12 — MPM CFL走査でNaNを最大値から漏らさない

- **関連:** `ArtifactCore/src/Physics/MpmSolver2D.cppm::updateGranularTimeStepForState()`。
- **確認できた事実:** `std::max(finiteValue, NaN)`は通常finite側を返すため、速度またはAPIC `C`にNaNがあると粒子速度上限へ反映されず、そのまま同stepのP2Gへ進み得た。
- **変更:** CFL上限の算出前に非有限の粒子速度をzero、非有限の`C`をzero matrixへ戻す。
- **価値／懸念:** 壊れたAPIC状態を同じstepのgrid転送へ伝播させない。位置・変形行列など他の非有限状態は既存のpost-step repairに残り、完全な異常状態回復を保証するものではない。
- **次に確認:** NaN/Infを速度・各`C`成分へ個別注入するfixtureで、P2G後の全粒子・格子状態が有限でsnapshot復元可能なことを確認する。

### 2026-10-12 — MPM粒子異常状態を格子index化より前に復旧

- **関連:** `ArtifactCore/src/Physics/MpmSolver2D.cppm::updateGranularTimeStepForState()`、`particleToGrid()`。
- **確認できた事実:** Granular状態の既存復旧はstep後に実行される。NaN positionは`particleToGrid()`の`floor`とint変換を通過して格子index計算に入るため、後段guardでは早すぎる復旧にならない。有限でもgridから極端に離れた位置はfloat-to-intの範囲外変換になり得る。`F`/`Fp`の特異・非有限値も同stepの応力計算へ入る。
- **変更:** CFL/step前処理でactive粒子のposition、velocity、`F`/`Fp` determinant、`C`、hardening stateを検査し、既存の復旧方針に沿って非有限positionをgrid domain中心へ置き、有限だがgrid外へ飛んだpositionを二次カーネルの完全支持範囲へclampし、invalid deformationはidentity、速度/`C`はzero、hardening stateは許容範囲へ正規化する。domain center自体が非有限の場合だけzeroへfallbackする。`setGridOrigin()`も非有限座標を拒否して最後の有効原点を保つ。
- **価値／懸念:** P2Gの座標変換とHencky stress前に不正数値を除く。異常状態の物理的意味を復元するものではなく、壊れた粒子をzero/identityへ落とす保護動作である。
- **次に確認:** 各粒子成分へNaN/Inf、反転determinant、過大hardeningを個別に注入し、P2G前正規化・有限な格子/粒子状態・snapshot検証を確認する。

### 2026-10-12 — MPM格子寸法とworld extentを更新前に検証

- **関連:** `ArtifactCore/include/Physics/MpmSolver2D.ixx::setGridOrigin()`、`ArtifactCore/src/Physics/MpmSolver2D.cppm::setGrid()`、grid node indexing。
- **確認できた事実:** `setGrid()`は`nx_*ny_`をsigned intで使用し、非有限cell sizeや巨大なdimensionをそのままgrid allocationへ渡せた。`setGridOrigin()`も有限座標のみ見ていたため、origin+grid extentがfloat範囲を超える設定を受け入れ得た。
- **変更:** `setGrid()`をbool APIにし、cell size、int積、vector max size、float world extentを事前検証して無効入力を拒否する。`setGridOrigin()`もbool APIとしてgrid far edgeがfloat範囲に収まる場合だけ更新し、両setterとも粒子生成後のgrid再解釈を拒否する。Material Grid/Granular Grid factoryは両設定の失敗を確認し、Material solverは粒子seedまで生成できた後にregistryへ登録する。grid幅へのdouble→int変換も範囲確認後に行う。signed積を使っていた単用途`resizeGrid()`は除去し、検査済みsize_t node countでgridをresizeする。
- **価値／懸念:** grid allocation、coordinate normalization、node index計算へoverflow/Infが混入する経路と、粒子/snapshotに対してgrid geometryだけが後変更される不整合を防ぐ。極端だが有限な大きいgridはallocator制約に達し得る。
- **次に確認:** NaN/Inf cell size、`INT_MAX`付近の各dimension、積overflow、origin+extent overflowを与え、無効入力後に直前のgrid構成が維持されることをAPI fixtureで確認する。

### 2026-10-12 — MPM material grid生成失敗をLayer authoring状態へ伝播

- **関連:** `ArtifactCore/src/Physics/PhysicsSystem.cppm::createMaterialGrid()`、`Artifact/src/Layer/ArtifactLayerPhysicsBridge.cppm`、`Artifact/src/Layer/ArtifactAbstractLayer.cppm::enableMaterialPhysics()`。
- **確認できた事実:** PhysicsSystemはinvalid grid/seed入力でnullptrを返すようになったが、Layer bridgeは戻り値を捨て、`enableMaterialPhysics()`はsolverがない場合もenabled/solverKindを先に更新していた。preset再生成が失敗したときには古いsolverだけが残り、新しいauthoring presetと実solverも食い違い得た。
- **変更:** bridgeをbool戻り値にし、PhysicsSystemのsolver生成成功後にMaterial Gridをregistryへ登録する。Layerは生成成功後にenabled/solverKind/presetをcommitし、preset変更時もenabled状態なら成功確認後にpresetを更新する。
- **価値／懸念:** 生成失敗時にUI/serialized authoring stateが存在しないsolverを有効と示さず、既存solverを保ったまま失敗を止められる。caller UIが失敗をユーザーへ表示するかは別途検討余地がある。
- **次に確認:** NaN bounds等でfactoryを失敗させ、初回enableのauthoring/registryが未変更、preset変更失敗時は旧solverと旧presetが一致することをfixtureで確認する。

### 2026-10-12 — MPM clear時にsolverの残り時間も破棄

- **関連:** `ArtifactCore/src/Physics/MpmSolver2D.cppm::clear()`、`update()`。
- **確認できた事実:** `clear()`は粒子・破断・colliderとseed grid metadataを消去する一方、solver accumulatorを残していた。clear後に粒子を追加すると、旧simulationのfractional step/backlogが新状態へ適用され得る。
- **変更:** `clear()`で`accumulatedTime_`も0へ戻す。
- **価値／懸念:** solver再利用時に旧時間が新しい粒子状態へ混入しない。PhysicsSystem側Composition clockとsnapshotの同時resetはこの直接APIの責務外。
- **次に確認:** 未完了substepを残した状態のclear→粒子再追加→updateで、clear前のaccumulatorが再生されないことをsolver fixtureで確認する。

### 2026-10-12 — MPM snapshotはgrid geometryをsolver間で持ち替えない

- **関連:** `ArtifactCore/src/Physics/MpmSolver2D.cppm::canRestoreSnapshot()`、`restoreSnapshot()`、`setGridOrigin()`。
- **確認できた事実:** Snapshotには粒子world positionとgrid originは入るが、grid dimensions/cell sizeは保存されず受け側solverの現在値を使っていた。clear後に同じorigin/seed metadataのままsolver gridだけ変更した場合、従来validatorでは旧particle positionsを異なる格子で使うsnapshotを受け入れ得た。
- **変更:** Snapshotへcell sizeとsolver node dimensionsを追加し、restore validatorでorigin・solver grid geometry・particle-grid columns/rowsが受け側の固定設定と完全一致することを要求する。Granular snapshotでは全粒子activeと各positionのquadratic-kernel-safe grid bounds内を追加条件にする。
- **価値／懸念:** restoreでparticle coordinatesを別grid geometryの下へ再解釈しない。異solverへのsnapshot移植は設計上未対応のまま明確に拒否する。
- **次に確認:** 同一solverのround tripは受理し、origin/column/rowを個別変更したsnapshotは拒否するfixtureを追加・実行する。
### 2026-10-12 — Granular MPM snapshotでseed不変の質量・体積を維持
- **関連:** `ArtifactCore/src/Physics/MpmSolver2D.cppm` (`MpmSolver2D::canRestoreSnapshot`)。
- **確認できた事実:** Granular snapshotの従来検証はmass/volume0の有限性と正値を確認するが、受け側solverの初期seed値との一致は確認していなかった。CFL波速は粒子密度 `mass/volume0` から計算される。
- **変更:** Granular snapshotの粒子数、mass、volume0、active状態を受け側solverの粒子列と照合してからrestoreする。通常のstepでこれらは更新されない固定属性。
- **価値／懸念:** snapshotの有限な改変で材料密度だけを別値にし、CFL条件・力学応答を変える経路を拒否する。異なるseed状態のsolver間snapshot移植も拒否される。
- **次に確認:** 同一solverの往復restoreが通り、mass/volume0/activeの個別改変および粒子数不一致が拒否されるfixtureを追加・実行する。

### 2026-10-12 — Granular seekは最新の共通checkpointから検証する
- **関連:** `ArtifactCore/src/Physics/PhysicsSystem.cppm` (`restoreGranularMaterialSnapshots`)。
- **確認できた事実:** checkpointはframe key順に保持される。seek復元は従来、各solverのcheckpointを古い順に走査し、候補ごとに粒子全件を`canRestoreSnapshot()`で検証してから最新frameを選んでいた。通常の有効checkpointでも最大cache候補数だけ全粒子検査が繰り返される。
- **変更:** anchor Layerのcheckpointを新しい順に見て、全solverの粒子状態・共通frame・world clock remainderが一致する最初の候補を採用する。
- **価値／懸念:** 通常のseek時に全粒子検証を最新checkpoint分へ抑える。古い候補fallbackの回数、seek時間、異常snapshot時の動作は未計測。
- **次に確認:** 1 Layer/複数Layerで最新checkpoint採用と、最新checkpointを破損させた場合の直前共通checkpointへのfallbackをfixtureで確認し、粒子数とcheckpoint数を増やして計測する。
### 2026-10-12 — MPM Sand拒否編集でProperty cacheと欄を復元
- **関連:** `Artifact/src/Layer/ArtifactMpmSand2DLayer.cppm`、`Artifact/src/Widgets/ArtifactPropertyWidgetShared.cppm`。
- **確認できた事実:** Property Editorの共通preview/commitはlayer setterを呼ぶ前に`AbstractProperty`の値を更新する。MPM Sand setterは範囲外値やsolver再生成失敗時にauthoring fieldを維持／ロールバックするが、cached property値は従来戻していなかった。commit経路もsetter成否ではなくcached propertyの前後値でUndo commandを組み立てる。
- **変更:** MPM Sandの拒否時に現在のauthoring field値を該当cached propertyへ戻す。Property Editor共通rowはpreview/commit handler後にcached propertyが入力値と異なる場合、editor表示と補助表示をcanonical property値へ同期する。
- **価値／懸念:** solverを更新できなかった値がUI/Undo上で受理されたように見える不整合を防ぐ。一般row canonical syncの他setterへの影響、multi-selection、Undo再生成失敗ケースは実機未確認。
- **次に確認:** MPM propertyの有効編集・範囲外入力・solver予算超過時にauthoring値、property cache、editor値、Undo stackが一致するか確認する。

### 2026-10-12 — MPM SandのUndo適用で拒否setterをcache fallbackしない
- **関連:** `Artifact/src/Undo/UndoManager.cppm::applyLayerPropertyValue`、`Artifact/src/Layer/ArtifactMpmSand2DLayer.cppm::setLayerPropertyValue`。
- **確認できた事実:** Undo/Redoの共通適用関数はlayer setterが`false`を返した後、一般propertyの互換性のため`AbstractProperty` cacheだけを直接更新するfallbackを持つ。MPM Sandはsolver再生成失敗時にauthoring値とcacheを旧値へ戻すため、このfallbackが走るとcacheだけが新値になる。`Artifact.Layer.Factory`はすでにMPM moduleをimportしている。
- **変更:** MPM Sand型でsetterが拒否した場合は共通fallbackへ進まず、command適用を失敗として返す。
- **価値／懸念:** Undo/Redo時の予算超過などでsolver設定とproperty cacheが分離するのを防ぐ。command stackが失敗をどう扱うか、拒否後のUndo/Redo挙動はruntime未確認。
- **次に確認:** solver再構成が成功するUndo/Redoと、意図的に再構成が拒否される場合のauthoring値・cache・solver・Undo stackの一致をruntime確認する。

### 2026-10-12 — MPM CPU Render Queueのsurfaceをbounded QImage poolへ再利用
- **関連:** `Artifact/src/Layer/ArtifactMpmSand2DLayer.cppm::getSoftwareRenderSurface`、`Artifact/src/Render/ArtifactRenderQueueService.cppm::renderLayerSurface`。
- **確認できた事実:** 汎用fallbackは`getThumbnail()`で128×96 float surfaceのQImage変換、Layer寸法scaled image、ARGB32 premultiplied変換をframeごとに作っていた。640×480 Layerで画像payload約2.4 MiB/frame。後続rasterizerはsurface dimensionsをlayer座標系として扱う。
- **変更:** MPM Layerが2枚の再利用QImage bufferを持ち、密度fieldを最終Layer寸法へresampleしてlinear sand RGBをsRGB化、premultiplied ARGBへ直接書く専用CPU surface APIを追加。当初はbilinear samplingだったが、後続変更でGPU meshと同じ固定対角線の二triangle piecewise-linear alpha補間へ置換した。最終pixelのalpha byteとRGB byteをpremultiply前に量子化する。Render Queueは型別分岐からこのAPIを使い、thumbnail変換とgeneric format conversionを避ける。detached bufferを優先し、2枚とも利用中なら従来thumbnail経路へfallbackする。通常thumbnail APIとGPU surface pathは維持。
- **価値／懸念:** 通常の1回またはmatte prepass＋final passではframeごとの画像buffer確保を再利用へ置き換え、surface dimensionsとQImage境界を保つ。linear-to-sRGB/alpha parityの8-bit量子化順序は寄せたが、Qt smooth scalingとのpixel差、別Render Queue呼出し組合せで2枚を超える生存bufferがないこと、複数worker寿命は未検証。density field生成とoutput sample走査は毎回必要。
- **次に確認:** build/link後、128×96→source dimensions基準画像比較、premultiplied alphaとopacity/matte/effect合成、matte source二重参照、frame連続処理でQImage allocationが寸法変更時に限られること、複数worker間のbuffer隔離を確認・計測する。

### 2026-10-12 — MPM Sandの同値commitでsolverを再seedしない
- **関連:** `Artifact/src/Layer/ArtifactMpmSand2DLayer.cppm::setLayerPropertyValue`。
- **確認できた事実:** Property Editorは編集previewをlayer setterへ渡し、その後commitも同じ値で呼び出す。MPM Sand setterは各呼出しでsolverを再生成し、baselineとComposition checkpointを作り直すため、preview済みの値がcommit時に再seedされていた。
- **変更:** parse/range検証後にMPM material/gravity/seed-resolution値を既存authoring値と比較し、同値ならsolver登録へ進まず成功を返す。
- **価値／懸念:** preview→commitで同じ最終値のsolver再生成・seed resetを避け、同値property通知の不要作業も避ける。異なるpreview値ごとの再生成負荷、Undo/Redo、粒子継続性はruntime未確認。
- **次に確認:** preview連続入力後に同値commitでsolver identity/baselineが維持され、異値commit・Undo/Redoでは意図した再生成が一度だけ起きるか計測する。

### 2026-10-12 — Granular world clock step予算をqueued timeへ連動
- **関連:** `ArtifactCore/src/Physics/PhysicsSystem.cppm::advanceGranularMaterialSolvers`、`Artifact/src/Composition/ArtifactAbstractComposition.cppm::replayGranularMaterialToFrame`。
- **確認できた事実:** Physics timeline fixed stepは1msまで設定できる。一方、Composition再生／replayは`max(1,fps)`のframe deltaを渡すため、1fps frameは1秒。Granular外側時計の固定64 step上限では1frameにつき64msしか消化できず、少なくとも936msずつbacklogが増える。外側backlogは102.4秒でcapされ、それを超えるstall時間は捨てる設計。
- **変更:** queued elapsed/fixed stepから必要step数を算出し、1 call上限1024で処理する。1秒/1msのケースは1callで全て消化できる。より大きいstallはbounded backlogとして残る。
- **価値／懸念:** 低fps Compositionのseek/replayでsimulation clockが恒常的に遅れる条件を解消しつつ、1 call workを1024 stepへboundedに保つ。低fps/高粒子数でのUI latencyは未計測。
- **次に確認:** fixedDt=1msで1fps/15fps/60fpsのframe replayがworld accumulatorを増やさず進み、1.024秒超stallでは最大1024 step後に正しいremainderが残ることをfixtureとruntime計測で確認する。

### 2026-10-12 — MPM checkpoint budget must include concurrent replacement peak

- **関連:** `ArtifactCore/src/Physics/PhysicsSystem.cppm::captureGranularMaterialSnapshots()`、`trimGranularWorldSnapshots()`。
- **確認できた事実:** captureは全solver分のlive particle capacityを事前見積もりとして予約し、古いcheckpointをevictしてからsnapshotを複製する。各snapshotの実particle/fracture capacity判明後にも、その1件をreservedとして予算を調整してからregistryへ公開する。複数solver captureの途中でregistry nodeが欠落するか実容量予約が失敗した場合は、そのframeのpartial entriesを全layerから除去する。最大UI設定500×200の単一solverは初期live/baselineに加え、最低1 checkpoint分がこのpayload予算へ入る。
- **未検証の懸念:** partial frameはscope guardで除去するが、allocation例外そのものはcapture callerへ伝播する。標準vector copyの実capacity上限も標準上はsource capacity以下と保証されないが、copy後にcapacityを計上してからpublishする。
- **価値／懸念:** 64 MiB payload予算内へlive+baseline+既存checkpoint+incoming snapshotを収め、実capacityが事前見積もりを上回る場合もpublish前に古いcacheを調整する。通常restoreは全solver共通frameだけを採用し、単solverだけの部分frameは受理しない。
- **次に確認:** allocation failure injectionを使い、例外境界でpartial frameが残らないこと、例外伝播がアプリのcapture callerでどう処理されるか、旧共通checkpointが必要以上に失われないかを確認する。

### 2026-10-12 — MPM Sand solver初期化失敗をLayer登録前に返す

- **関連:** `Artifact/src/Layer/ArtifactMpmSand2DLayer.cppm::setComposition()`、`Artifact/src/Composition/ArtifactAbstractComposition.cppm::appendLayerTop()`、`Artifact/src/Project/ArtifactProject.cppm::createLayerAndAddToComposition()`。
- **確認できた事実:** 当初MPM Layer attach時のsolver生成失敗はwarningのみで、Composition appendとProject作成は成功扱いだった。`setComposition()`はvoidで、Composition moduleからMPM具体型を参照すると依存循環の懸念がある。
- **変更:** 基底Layerへ既定trueの`isCompositionAttachmentReady()`確認点を追加し、MPM SandはComposition runtime worldにsolverが登録された場合だけtrueを返す。Top/Bottom/Indexの登録口はattach後・registry変更前に確認し、失敗時はLayerをdetachして登録を止める。JSON復元は失敗Layerをparent resolution対象へ加えない。Project作成もappend失敗時にdirty化しない。
- **価値／懸念:** MPM具体型のimportをCompositionへ追加せず、通常Layerの既存attach動作を維持して、MPM solverのないblank LayerがCompositionへ登録される経路を閉じる。公開virtual追加に伴う全module rebuildと実動作は未確認。
- **次に確認:** 予算超過・無効ID/geometryでのTop/Bottom/Index追加、JSON復元とclone/undo attach、通常Layer append、失敗後world registry cleanupを確認し、成功/失敗結果とComposition treeが一致することをruntimeで検証する。

### 2026-10-12 — MPM attach readinessは新規Composition接続だけで判定

- **関連:** `Artifact/src/Composition/ArtifactAbstractComposition.cppm` のTop/Bottom/Index追加経路。
- **確認できた事実:** readiness判定を毎回行うと、同じCompositionへ登録済みのLayerを並べ替えるケースでも失敗時のcleanupが `setComposition(nullptr)` を呼ぶ。registryにはLayerが残ったままComposition参照だけが切れる不整合になり得る。MPM Sandの`setComposition()`は同一Composition時にsolver再生成を行わないため、再挿入は新規attachではない。
- **変更:** 追加前の`compositionObject()`を比較し、別Composition/nullからの新規接続だけreadinessを検証する。既存Composition内での再挿入ではattach失敗cleanupを行わない。
- **価値／懸念:** 既存のLayer reorder/group経路でComposition所有状態を壊さない。Index挿入の戻り値は呼び出し側へ公開済みで、生成した新Layerは既存のmembership確認も行う。
- **次に確認:** MPM層を含むreorder/ungroup、solver登録失敗後の新規attach拒否、JSON/Undo再接続でComposition membership・QObject参照・solver registryが一貫することをruntimeで確認する。

### 2026-10-12 — Granular solver再設定時の一時メモリもworld予算へ含める

- **関連:** `ArtifactCore/src/Physics/PhysicsSystem.cppm::createGranularMaterialGrid()`、`trimGranularWorldSnapshots()`。
- **確認できた事実:** 再設定は新solverとbaselineを作ってから既存entryを置換する。従来の事前計算は同じLayerの旧solverを除外し、world checkpointもsolver構築前にevictしなかったため、置換中のlive＋baseline＋cache＋incoming payloadが64 MiBを超え得た。grid-node上限も同Layer旧gridを除外していた。
- **変更:** world byte/grid予算の事前計算へ置換対象の現行solverを含める。solver確保前に要求particle count分を予約し、solver live capacityが判明した後は実live capacity＋要求粒子数分をbaseline複製前に再予約する。baseline生成後もlive/baseline capacity合計で再度checkpoint予算を調整してからcommitする。
- **価値／懸念:** 設定変更時もComposition単位の粒子・grid上限を越えず、新solver生成に失敗した場合は旧solverを保持できる。snapshot eviction後に割当が失敗した場合、solver stateは維持されるがseek cacheは削減済みとなる。vector capacityの実測は未実施。
- **次に確認:** 上限近傍の同Layer再設定で旧solver＋新solverの実capacityを含む時に拒否され、失敗時は旧solverの状態・設定が維持されること、余裕がある時は再設定が成功することをAPI/runtime検証する。

### 2026-10-12 — Granular snapshotのfracture配列を空に固定

- **関連:** `ArtifactCore/src/Physics/MpmSolver2D.cppm::canRestoreSnapshot()`、Granular checkpoint restore。
- **確認できた事実:** Granular modelでは`checkFracture()`が即returnし、factoryもfractureを無効化するため`fracturedIndices_`は空のまま。一方validatorは各indexがparticle範囲内なら任意個数を許可し、restoreのvector assignmentでsolver側の配列容量を増やし得た。
- **変更:** Granular snapshotはseed particle数の一致に加え、`fracturedIndices`が空でなければ拒否する。Corotated modelのfracture snapshotは従来どおり許容する。
- **価値／懸念:** Granular restoreが破断状態を持ち込まず、solver payload budget外の配列増加も防ぐ。壊れた内部snapshot fixtureと通常seekは未実行。
- **次に確認:** empty fracture listの通常Granular snapshotはrestoreでき、粒子数内の重複indexを持つmalformed snapshotはrestore拒否されることをAPI fixtureで確認する。

### 2026-10-12 — MPM GPU dispatch境界でGranularモデルを拒否

- **関連:** `ArtifactCore/src/Physics/MpmSolver2D.cppm::updateGPUSubsteps()`、`setBackend()`、`setConstitutiveModel()`。
- **確認できた事実:** 公開setterはGranularモデルとGPU backendの併用を拒否するが、GPU substep入口自体は構成則を確認せずelastic shaderへ進める。現行のlayer factoryからGPU backendを設定する経路は見当たらないものの、将来callerが増えた場合や内部状態不整合では砂状態をelastic shaderで更新し得る。
- **変更:** GPU substep開始時にもGranular Drucker–Pragerを拒否し、callerの既存CPU fallbackへ戻す。
- **価値／懸念:** CPU-onlyの砂構成則契約をdispatch境界で守り、弾性GPU shaderによる誤更新を防ぐ。変更はGPU dispatch入口の防御条件のみで、Corotated経路は変わらない。
- **次に確認:** Granular+GPU backendの不正状態を作るfixtureでGPU dispatchされずCPU fallbackすること、通常Corotated GPU dispatchが維持されることを確認する。

### 2026-10-12 — MPM GPU density surfaceで頂点alphaをblend
- **関連:** `Artifact/src/Layer/ArtifactMpmSand2DLayer.cppm::draw()`、`Artifact/src/Render/ShaderManager.cppm::createPSOs()`。
- **確認できた事実:** MPMのsurface triangleは `PrimitiveRenderer2D::drawMpmSurface()` から共通solid-triangle batchへ流れる。ShaderManagerのsolid triangle PSOはsolid rectangle pipelineを複製し、blend設定も引き継ぐため、vertex alpha blendingはすでに有効。最初のPSO診断は誤りで、変更を残さなかった。
- **未検証の仮説:** 固定128×96 density fieldの見た目を支配する要素は、最大100,000粒子設定時に1粒子あたり0.64512px²となる低い粒子体積と、粒子がfield sample中心からずれた時の離散kernel weightである可能性がある。支持半径のsample coverage floorは既にfield pixel diagonal（640×480 Layerなら7.07px）で、密度寄与をその局所stencil内の離散積分で正規化しているため、単に支持半径を大きくするのは解決でなく体積も広がる。
- **変更:** solid-triangle renderer/PSOは変更せず、密度fieldと解像度の関係を追加調査する。
- **次に確認:** 1粒子をsample中心と最悪位相へ平行移動する解析fixtureで、離散field積分・threshold後の連結領域・積算体積を比較し、見た目の改善が支持半径ではなくfield resolution/thresholdから得られるか調べる。

### 2026-10-12 — MPM solver APIはauthoring粒子数上限を尊重
- **関連:** `ArtifactCore/src/Physics/PhysicsSystem.cppm::createGranularMaterialGrid()`。
- **確認できた事実:** MPM Sand Layer propertyとJSON復元はcolumnsを2..500、rowsを2..200に制限する（最大100,000粒子）。Composition solver factoryは入力値を独立に512×512へclampし、aggregate capも262,144としていたため、直接API呼出しで上限外値を黙って増やし、またLayerの既定上限より最大2.62倍の粒子を生成できた。
- **変更:** API factoryはcolumns 2..500、rows 2..200、合計100,000以下だけを受け入れ、範囲外値をrejectするよう変更。保存値が作成後に変質せず、particle payload/grid予算を不要に消費しない。
- **次に確認:** 範囲の境界・直外値、solver予算超過をAPI fixtureで確認し、既定96×48と最大500×200で生成数がrequestと一致することを検証する。


### 2026-10-12 — MPM GPU surfaceはdensity thresholdで三角形をclip（後続変更で廃止）
- **関連:** `Artifact/src/Layer/ArtifactMpmSand2DLayer.cppm::draw()`、MPM surface vertex buffer。
- **確認できた事実:** 従来はdensity field sample四隅の一つでも0.08を超えるとquad全域を描き、低密度側の格子cellまで砂面色が広がっていた。smoothstep alphaは頂点にあるが、isodensity contourそのものはcell edgeへスナップしていた。
- **変更:** 各quadを元の2三角形に分け、各三角形をdensity 0.08の半平面でSutherland–Hodgman clipする。交差位置の座標を密度から線形補間し、残った三角形／四角形をfanで出力。最大4三角形（12頂点）/quadの固定buffer容量をcold pathでreserveする。frame中heap allocationは追加しない。
- **価値／懸念:** 粒子pointを描かず連続density surfaceとして表示する契約を保ったまま、密度閾値輪郭をsample cellの内部へ置き、薄い境界を厳密にfield閾値から切り出す。密度自体のfield解像度は128×96のまま。PSO／GPU画素、最大packet量、alpha continuityはruntime未確認。
- **次に確認:** 1sample幅の孤立blob、斜め境界、場の外端、全密度の一様fieldについて面積と輪郭pixelを比較し、triangle windingとCPU thumbnail境界の差をruntime captureで確認する。
- **後続更新:** clip方式は同日後続変更で廃止。現行方式は「GPU砂輪郭のclipを廃止しCPUと同じ三角形補間へ統一」を参照。


### 2026-10-12 — MPM clip交点でCPUのfield alpha補間を保つ（後続変更で廃止）
- **関連:** `Artifact/src/Layer/ArtifactMpmSand2DLayer.cppm::draw()`、`getSoftwareRenderSurface()`。
- **確認できた事実:** CPU Render Queueはsmoothstep後のdensity alpha fieldを拡大補間して出力する。GPU surfaceをdensity thresholdでclipした交点へ `densityAlpha(threshold)=0` を直接設定すると、field node間でalphaを線形補間するCPU画像に比べて、急勾配の輪郭でGPU側が痩せる。
- **変更:** clip vertexはdensity値とは別にalpha値を持つ。交点座標とalphaの両方を同じedge interpolation factorで補間し、幾何はdensity threshold、色alphaはfield nodeのsmoothstep alphaを継続する。
- **価値／懸念:** 連続面の境界位置を密度から決めつつ、従来CPU surfaceのedge interpolationに近いalpha profileを維持する。CPUはbilinear/8-bit、GPUはtriangle-linear/floatなのでpixel parityは未保証。
- **次に確認:** （当時の案）高勾配の単粒子とsubpixel移動でCPU/GPUの輪郭幅・最大alpha・積算coverageを同じzoomで比較する。現行方式の比較条件は後続Insight項目を参照。
- **後続更新:** clip交点方式は同日後続変更で廃止。現行方式は「GPU砂輪郭のclipを廃止しCPUと同じ三角形補間へ統一」を参照。


### 2026-10-12 — MPM surfaceはopacity zero時にfield rasterを省略
- **関連:** `Artifact/src/Layer/ArtifactMpmSand2DLayer.cppm::draw()`。
- **確認できた事実:** GPU MPM drawはdensity fieldを書き出し、最大12,513 quadを走査してからopacityをvertex alphaへ設定していた。opacityが0でもsurfaceは最終出力へ寄与しない。
- **変更:** finite/clampしたsafe opacityが0ならdensity rasterとmesh traversalを始める前にreturnする。OpacityがNaN/非有限の場合は既存どおり0相当として描画しない。
- **価値／懸念:** 完全透明のMPM Layerでframe-time CPU workを避ける。opacity変化時のsolver stateは変更せず、状態復元やsimulation clockには影響しない。性能差は未計測。
- **次に確認:** Composition Viewでopacity=0および0近傍のLayer表示、GPU draw packetが生成されないことを確認する。


### 2026-10-12 — MPM fixed-step collision候補配列をComposition scratchへ保持
- **関連:** `Artifact/src/Composition/ArtifactAbstractComposition.cppm::evaluateLayerCollisionPairs()`。
- **確認できた事実:** MPM fixed-step/playback/replayが進むたびにcollision pair更新が呼ばれる。関数は各呼出しでlocal `std::vector<ArtifactAbstractLayerPtr>`を作って現在Layer数分reserveし、active solver/collider候補を再構築していた。
- **変更:** Composition Impl所有の`ArtifactArray<ArtifactAbstractLayerPtr>`をscratchにし、各呼出しで要素だけremove、current Layer countまでcapacityをreserveしてappendする。reserveは既存capacity以下ではno-opなので、過去最大Layer数を超えないstepではcandidate storageが再確保されない。Layer個別remove／remove-all時もscratch要素をreleaseし、capacityのみ保持する。collision pair setと選別ロジックは変更しない。
- **価値／懸念:** Granular fixed-stepのcollision更新で毎step起きるローカル配列のallocate/freeをなくし、独自コンテナ優先ルールへ合わせる。first useまたはCompositionの過去最大Layer数更新時はcold/hot境界でcapacity growthが残る。remove/clear時はSharedPtr要素を直ちに破棄する。CPU計測とreplay parityは未確認。
- **次に確認:** Compositionの最初のMPM playback以降でallocation profilerを取り、collision candidate bufferの容量がLayer追加時以外に変化しないこと、collider移動/active境界/seek結果が維持されることを確認する。


### 2026-10-12 — Collision evaluatorの未使用pair lifetime scanを除去
- **関連:** `Artifact/src/Composition/ArtifactAbstractComposition.cppm::evaluateLayerCollisionPairs()`、`collisionPairKey()`。
- **確認できた事実:** pair scanはactive collision/material candidate同士のboundsを毎step組み合わせ比較し、QString keyを組み立てQSetへinsertしていた。既存setとのcontains後の分岐は空で、`activeCollisionPairs_`とkey helperはこのscan外で一度も読まれない。collider solver登録は直前の独立target/source loopで実行されている。
- **変更:** `collisionPairKey()`、pair boundsの二重loop、local `nextPairs`と未使用`activeCollisionPairs_`を削除。Granular/legacy material solver向けcollider clear・変換・registerと、acoustic impactの`compositionCollisionBounds()`使用は保持。
- **価値／懸念:** collision candidate数nに対する毎step O(n²) bounds/QString/QSet仕事をなくし、衝突形状登録の実処理だけを維持する。もし将来pair lifetimeからイベントを起こす場合は、実際のconsumer/契約を追加してから再導入する。物理結果・イベント回帰はruntime未確認。
- **次に確認:** 複数collision source/material targetの組合せで各frame colliderが再登録されること、contact/audio/fracture component eventがpair scan削除で変わらないことをruntime比較する。


### 2026-10-12 — MPM polygon collider輪郭バッファを固定step間で再利用

- **関連:** `Artifact/src/Composition/ArtifactAbstractComposition.cppm::evaluateLayerCollisionPairs()`、`ArtifactCore/include/Physics/MpmSolver2D.ixx::addCollider()`。
- **確認できた事実:** 毎fixed-stepのcollision更新でPolygon source用`MpmCollider2D`をローカル生成し、輪郭をreserve/pushしていた。Solverも`addCollider()`でpolygon vectorを深コピーし、直後の`clearColliders()`はその容量を破棄していた。`colliders()`はモジュール外から使われていない。
- **変更:** Composition Implに再利用collider scratchを保持。Solverは既存`MpmCollider2D::polygonPoints`と同じ`std::vector<float>`型のpolygon point buffer poolを持ち、clear時にvector ownershipをpoolへ戻し、add時は再利用して内容だけassignする。公開`colliders()`はactive分だけを返す既存契約のまま。
- **価値／懸念:** 安定したsource/targetとoutlineサイズではfixed-stepごとのpolygon-point heap churnを避ける。新規最大polygon数／頂点数の初回増加時は容量成長し、Composition scratchとsolver poolに同じ輪郭データが保持されるため常駐メモリは増える。runtime計測・物理 parityは未確認。
- **次に確認:** 複数polygon target/sourceでoutline数の増減・target transform更新・clear/replay往復を検証し、steady-stateのallocation profilerでpolygon-point allocationが0になることを測る。


### 2026-10-12 — MPM polygon source outlineをcaller-owned scratchへappend

- **関連:** `Artifact/src/Layer/ArtifactShapeLayer.cppm::appendCollisionOutlineLocalPoints()`、`Artifact/src/Composition/ArtifactAbstractComposition.cppm::evaluateLayerCollisionPairs()`。
- **確認できた事実:** Shapeのcollision outline APIは`NamedVector<QPointF>`を値返却し、MPM fixed-stepごとにoutlineを構築してComposition側で一時オブジェクトとして受け取っていた。authoring済みpath/polygonは既存shapeデータから直接列挙できる。
- **変更:** 既存の値返却APIを維持しつつcaller-owned output APIを追加。Compositionは`NamedVector<QPointF>`をImpl scratchとして保持し、authoring済みpath/polygonは再利用capacityへ直接appendする。後続でRect、Square、Triangle、Line、Ellipse、Star、Polygon、Arrow、Heart、Diamond、Gear、Crossもcollision専用の直接サンプラーへ変更し、一時vectorを避ける。
- **価値／懸念:** 安定した輪郭サイズではMPM fixed-step中のoutline一時vector生成を避ける。primitive collision samplerは描画用`buildRenderablePoints()`とは別の式実装になったため、全ShapeTypeの点順・数値一致をruntime/fixtureで確認する必要がある。Layer interface/module integrationはbuild未検証。
- **次に確認:** 各ShapeType、corner radius、最小寸法、custom polygon/path、multi-contentについて、collision専用出力と既存`buildRenderablePoints()`を比較する。fixed-step allocation profilerでComposition scratch capacity安定後の確保数を測る。


### 2026-10-12 — MPM polygon colliderはparticle単位の辺走査前にAABB cull

- **関連:** `ArtifactCore/include/Physics/MpmSolver2D.ixx::addCollider()`、`ArtifactCore/src/Physics/MpmSolver2D.cppm::resolveColliders()`。
- **確認できた事実:** Polygon colliderは各粒子ごとに`mpmClosestPolygonPoint()`で全辺を走査し、続けて`mpmPointInPolygon()`でも全頂点を走査していた。Shape path／polygonは最大10万点まで保持でき、粒子数も最大10万であるため、bounding rectangle外の粒子にも高コストな全辺処理を行っていた。
- **変更:** `addCollider()`で有限なpolygon verticesを検証する既存loopにmin/maxを集約し、solver-owned colliderへAABBをキャッシュする。`resolveColliders()`はinclusive AABB外の粒子をpolygon traversal前にskipし、Granularの既存`normalLengthSq <= 1e-10`境界接触許容を保持するためそのAABBを1e-5拡張する。polygon座標や公開collider authored x/y/width/heightは変更しない。
- **価値／懸念:** AABB外かつ境界許容域外の粒子はinsideにもcontactにもならないため、接触意味を保った広域cullとなり、点ごとのO(vertices)を省ける。AABB内では既存の全辺走査が残り、AABBはcollider再登録時に全頂点を一度走査する。実機高速化・境界parityは未計測。
- **次に確認:** 凹凸polygon、縮退点列、AABB境界ちょうどのparticleで旧判定との結果を比較し、粒子がAABB外/内に分布するケースでstep timeと辺訪問数をprofileする。


### 2026-10-12 — MPM polygon queryの内外判定と最近傍辺探索を一周へ統合

- **関連:** `ArtifactCore/src/Physics/MpmSolver2D.cppm::mpmQueryPolygon()`、`ArtifactCore/include/Physics/MpmSolver2D.ixx::addCollider()`。
- **確認できた事実:** AABB内の各粒子に対し、旧実装はpoint-in-polygonとclosest-edgeを別々に全頂点走査していた。境界一致時のGranular法線選択ではpolygon signed areaもさらに全頂点走査していた。
- **変更:** closest-edgeループで同じedgeを使ってray-crossing toggleも更新する単一queryを導入。signed doubled areaはaddColliderの頂点validation/AABB loop中に算出・保持し、境界法線応答で再利用する。境界 epsilonと辺index順は維持。
- **価値／懸念:** AABB内粒子のpolygon vertex readsを通常2周から1周へ、境界接触時も追加周回なしへ減らす。branch/interleaveによるCPU時間差とray crossing/closest contactの数値・イベント同値は未計測。
- **次に確認:** CW/CCW、凹polygon、水平/垂直辺、頂点一致、重複点、縮退辺の結果を旧式fixtureで比較し、粒子分布別のpolygon辺訪問数とfixed-step timeをprofileする。


### 2026-10-12 — Polygon内部粒子も最近傍境界が必要

- **関連:** `ArtifactCore/src/Physics/MpmSolver2D.cppm::resolvePolygonCollider()`。
- **確認できた事実:** polygon colliderへの接触後、solverは粒子をcontainment判定だけでなく最近傍境界点へ投影する。粒子がpolygon内部にあっても境界距離と法線が必要であり、内外判定がtrueになった時点で走査を打ち切ると深くめり込んだ粒子の位置復元が変わる。
- **価値／懸念:** 直近のAABB cullとsingle-pass queryを維持したうえで、AABB内全粒子の最寄り辺探索を無条件に省く単純shortcutを避ける。大量頂点polygon内の粒子密度が高い場合は依然O(particles×vertices)となる。
- **次に確認:** 実使用sceneでpolygon頂点数とAABB内particle数、edge traversal時間を計測する。必要ならcollision semanticsを保つsegment BVHなどを評価し、単純なinside-only early-outは採用しない。


### 2026-10-12 — Collider係数sanitizeをparticle loop外へ移動

- **関連:** `ArtifactCore/src/Physics/MpmSolver2D.cppm::MpmSolver2D::resolveColliders()`。
- **確認できた事実:** friction/restitutionのfinite checkとclampは粒子ごと・colliderごとに同じcollider authored値から再計算されていた。応答関数内で他particleは参照・変更しない。
- **変更:** 走査順の変更案は取り消し、各particleのcollider orderを維持。solver-owned colliderへlegacy clamp値とGranular finite/clamp値を登録時にキャッシュして、particle×collider毎のclamp/finite検査を除去した。
- **価値／懸念:** nested particle→collider traversalと粒子アクセス局所性を保ち、係数をcollider再登録時だけ計算する。キャッシュ値がcurrent API semantics（legacy NaN保持、Granular nonfinite=0）と一致するかコード上は式を対応させたが、state parityと実CPU時間は未計測。
- **次に確認:** legacy solidとGranularのbox/circle/polygon/planeでNaN/±Inf/範囲外係数の旧loopとの粒子state parityを比較し、particle数・collider数別のstep timeをprofileする。


### 2026-10-12 — Composition collision source情報を1評価内で再利用

- **関連:** `Artifact/src/Composition/ArtifactAbstractComposition.cppm::evaluateLayerCollisionPairs()`。
- **確認できた事実:** 各material targetのsource走査で、同じcollision sourceについてenabled/shape/offset/restitution property、global transform、world collision boundsを再取得・再計算していた。Outlineもsource-target pairごとに生成していた。
- **変更:** Composition-owned `ArtifactArray<CollisionLayerScratch>`へsource/target判定、collision properties、global transform、world bounds、target inverseを一度評価して保持。collider生成をsource outer / target innerへ変更し、各targetへ入るsource順は旧処理と同じに保つ。polygon outlineは最初の有効targetでlazy生成し、そのsourceの他targetで再利用する。頂点のtarget-local変換はtargetごとに残す。
- **価値／懸念:** 重複property accessor・bounds transform・inverse transformとmulti-target時のoutline生成を削減する。scratch要素はLayer SharedPtrを保持するため、従来通りremove/remove-all時の全scratch clearを維持した。source-target順の物理結果とruntime効果は未確認。
- **次に確認:** 1 source/複数target、複数source/複数target、同一source-target除外、inactive/非invertible target、empty polygon outline fallbackを比較し、transform評価回数・outline生成回数・solver collider順が一致することを確認する。


### 2026-10-12 — Polygon頂点のQTransform mapを合成行列1回へまとめる

- **関連:** `Artifact/src/Composition/ArtifactAbstractComposition.cppm::evaluateLayerCollisionPairs()`。
- **確認できた事実:** Polygon vertexごとに`sourceOffset`加算、source global map、target inverse mapを実行していた。Qt公式QTransform文書は、積に書いた順に各transformを適用すると説明している。
- **変更:** source単位で`translate(offset) * sourceGlobal`を作り、target単位でさらに`* inverseTarget`を合成。各vertexはcombined transformのmapを1回受ける。
- **価値／懸念:** per-vertex QPointF加算と2回のmapを1回のprojective mapにまとめる。行列積先行によりfloat roundingが従来の逐次mapと異なる可能性があり、境界粒子の数値差は未検証。
- **次に確認:** translate/scale/rotation/shear/projective transformsで旧nested mapとの頂点差を許容差測定し、接触境界と大polygon頂点数でstate parityとstep timeを確認する。

# 2026-10-12 — Bound MPM polygon collider coordinate arithmetic

- **関連:** `ArtifactCore/include/Physics/MpmSolver2D.ixx`, `MpmSolver2D::addCollider()`
- **確認できた事実:** Polygon contact code calculates edge length squared in `float`, while collider registration accumulated signed doubled area in `float` around the world origin. Finite but enormous vertices could overflow intermediates, and large absolute coordinates could cause cancellation for small polygons.
- **変更:** Plane, Box, Circle, and Polygon geometry scalars are now limited to ±1e18; this bounds two-coordinate deltas, squared distances, and center±radius/half-extent operations within float range. Polygon signed doubled area accumulates in `double` relative to the first vertex and saturates to finite float range when cached.
- **仮説 / 未検証:** This bounds contact arithmetic and reduces translation-driven cancellation for accepted polygon geometry. The coordinate limit is many orders above ordinary Composition units, but extreme-coordinate handling, translated winding, and high-vertex behavior are not runtime-tested.
- **価値 / 懸念:** Invalid extreme colliders are rejected at registration, before broad-phase/contact math can propagate infinities or NaNs.
- **次に確認:** Runtime-check polygons near the coordinate limit, opposite-sign endpoint deltas, high vertex counts, both winding directions, small polygons translated far from origin, and ordinary geometry parity.

# 2026-10-12 — Reject unrepresentable MPM boundary spans

- **関連:** `ArtifactCore/src/Physics/MpmSolver2D.cppm`, `MpmSolver2D::setBoundary()`
- **確認できた事実:** Boundary endpoints were checked individually for finiteness, then subtracted in `float` to detect degenerate spans. Opposite-sign large finite endpoints can overflow that subtraction to infinity, which bypasses the minimum-width rejection and marks the boundary active.
- **変更:** Normalize endpoints locally, compute width and height in `double`, and reject either span if it exceeds finite `float` range or is below the existing minimum before assigning boundary state.
- **仮説 / 未検証:** This prevents representational overflow from entering per-particle boundary clamps without changing ordinary boundary values or order-independent endpoint handling. Runtime large-span checks remain unrun.
- **価値 / 懸念:** Invalid oversized domains no longer enable a boundary whose float extent cannot be represented.
- **次に確認:** Exercise normal, reversed, degenerate, and opposite-sign near-limit endpoints through the solver API and confirm invalid calls leave `hasBoundary()` false.

# 2026-10-12 — Require a valid domain boundary for granular solver creation

- **関連:** `ArtifactCore/src/Physics/PhysicsSystem.cppm`, `createGranularMaterialGrid()`
- **確認できた事実:** `MpmSolver2D::setBoundary()` returns `void` and can leave `hasBoundary()` false for collapsed or unrepresentable spans. Granular creation previously continued configuring, seeding, and registering that solver without checking the boundary state. Checkpoint trimming also mutates retained replay history, so invalid inputs must be rejected before that step.
- **変更:** Granular solver creation now validates the float-converted endpoints and actual spans before checkpoint trimming, uses those validated endpoints, and defensively requires `hasBoundary()` after applying them.
- **仮説 / 未検証:** This prevents an attached MPM Sand layer from succeeding with its domain clamp silently disabled and avoids evicting snapshots for a deterministic invalid-boundary request. Tiny-domain and endpoint-collapse cases have not been exercised at runtime.
- **価値 / 懸念:** Creation upholds the authored closed-domain invariant without mutating seek history for invalid geometry.
- **次に確認:** Verify normal integer-sized domains still create, while sub-epsilon spans and collapsed float endpoints fail before any solver/snapshot registry mutation.

# 2026-10-12 — Use rounded boundary geometry for the granular grid

- **関連:** `ArtifactCore/src/Physics/PhysicsSystem.cppm`, `createGranularMaterialGrid()`
- **確認できた事実:** Solver domain endpoints are stored as `float`, but granular grid dimensions and seed containment were derived from the original input widths/endpoints before float rounding. Near representational boundaries these geometries can differ.
- **変更:** The grid extent now uses the validated float-rounded boundary spans, and seed bounds are checked against those same effective endpoints before checkpoint mutation.
- **仮説 / 未検証:** This keeps grid, seed, and particle clamp geometry aligned after endpoint rounding. Ordinary and near-ULP input cases are not runtime-tested.
- **価値 / 懸念:** A successfully created world uses one consistent domain geometry across grid allocation, initial particles, and boundary enforcement.
- **次に確認:** Exercise endpoint sums around float ULP transitions and verify all seed positions remain inside the effective domain and grid support margin.

# 2026-10-12 — Validate the rounded MPM grid stencil margin

- **関連:** `ArtifactCore/src/Physics/PhysicsSystem.cppm`, `createGranularMaterialGrid()`; `MpmSolver2D::updateGranularTimeStepForState()`
- **確認できた事実:** Granular recovery clamps particles to `gridOrigin + 2*cellSize` through `gridOrigin + (gridSize-2)*cellSize` so the quadratic MPM stencil remains in bounds. Grid origin is stored as `float`, so subtracting two cells can round away at large world offsets.
- **変更:** Creation now calculates the same float origin and safe position range used by the solver, rejects a domain not fully covered by that range before checkpoint trimming, and passes the validated origin to `setGridOrigin()`.
- **仮説 / 未検証:** This prevents a solver from attaching with reduced/missing stencil margin due to float origin rounding. Large-offset/small-cell cases and ordinary-domain acceptance have not been runtime-tested.
- **価値 / 懸念:** Domain clamps, seed placement, and the solver's quadratic-kernel-safe range now share representable geometry.
- **次に確認:** Compare generated seed extrema with the computed safe interval at ordinary values and around float ULP transitions; verify invalid geometry fails before changing snapshots or solver registration.

# 2026-10-12 — Share one implementation for granular seed geometry

- **関連:** `ArtifactCore/src/Physics/PhysicsSystem.cppm`, `createGranularMaterialGrid()` / `MpmSolver2D::addParticlesGrid()`
- **確認できた事実:** `addParticlesGrid()` computes float spacing, start/last positions, volume, and mass; granular PhysicsSystem creation must know the same seed extrema before it trims frame snapshots.
- **変更:** Added `calculateMpmParticleGridSeedGeometry2D()` to the existing `Physics.Mpm2D` module and made both seed generation and preflight consume its result. The helper preserves the solver's float operation order and reports count, extrema, and mass/volume values.
- **仮説 / 未検証:** Sharing one computation removes formula drift between validation and particle creation. Module integration and numeric parity have not been built or runtime-tested.
- **価値 / 懸念:** Invalid seed geometry can be rejected before resource-budget mutation without maintaining a second independent formula in PhysicsSystem.
- **次に確認:** Build the existing module graph when authorized; compare helper extrema against seeded particles for regular and float-ULP-edge requests, and verify failures leave snapshots/registry unchanged.

# 2026-10-12 — Validate target inverse transforms before MPM bounds mapping

- **関連:** `Artifact/src/Composition/ArtifactAbstractComposition.cppm`, `evaluateLayerCollisionPairs()`
- **確認できた事実:** Material-target transforms are inverted once per collision evaluation. The previous code accepted the inverse solely from `QTransform::inverted()`'s invertible flag and then passed it to `mapRect()`.
- **変更:** The inverse is now usable only when inversion reports success and all nine matrix coefficients are finite. Target-local bounds also retain their coordinate/size finite checks.
- **仮説 / 未検証:** This blocks malformed inverse coefficients from entering MPM broad-phase bounds. No near-singular or corrupted transform has been exercised at runtime.
- **価値 / 懸念:** The broad-phase map now checks both the API inversion result and the actual matrix values before collider construction.
- **次に確認:** When runtime checks are authorized, test ordinary, near-singular, singular, and non-finite transforms against collider registration and finite solver state.

### 2026-10-12 — Reject non-finite Composition collision bounds

- **関連:** `Artifact/src/Composition/ArtifactAbstractComposition.cppm`, `evaluateLayerCollisionPairs()`
- **確認できた事実:** Collision source bounds are transformed to world coordinates and then mapped through each material target's inverse transform before solver collider construction. Non-finite transform coefficients or mapped rectangle coordinates can otherwise enter `QRectF`/solver bounds checks.
- **変更:** A non-finite global transform invalidates the source world bounds, and target-local bounds are skipped if any coordinate or size is non-finite.
- **仮説 / 未検証:** This keeps malformed transform values out of MPM AABB and contact calculations. No runtime corrupted-transform case has been exercised.
- **価値 / 懸念:** Invalid source geometry cannot poison MPM collider broad-phase values. Such a collider is omitted until its transform/bounds become finite.
- **次に確認:** With runtime validation authorized, inject NaN/Inf into source and target transforms and verify the solver state stays finite and normal transforms continue to register identical colliders.

### 2026-10-12 — Affine-only composition for MPM collision outlines

- **関連:** `Artifact/src/Composition/ArtifactAbstractComposition.cppm`, `MpmSolver2D::addCollider()`
- **確認できた事実:** Collision outline vertices previously passed through nested source-global and target-inverse `QTransform::map()` calls. The current code composes these transforms for affine source and target transforms, while retaining the nested mapping for projective transforms. It rejects the collider if any mapped coordinate or its float conversion is non-finite, consistent with the solver rejecting non-finite polygon coordinates.
- **仮説 / 未検証:** Affine composition reduces per-vertex transform work, but matrix composition can change floating-point rounding near collision boundaries. No runtime parity or performance measurement has been run.
- **価値 / 懸念:** This limits the fast path to affine transforms and avoids introducing a different projective mapping path, while preventing invalid coordinates from reaching collider registration.
- **次に確認:** When builds and runtime checks are authorized, compare affine and projective collider vertex coordinates and boundary contacts against the nested-map reference, then measure evaluation cost.
### 2026-10-12 — MPM通常再生後のGranular checkpoint capture

- **関連:** `Artifact/src/Composition/ArtifactAbstractComposition.cppm::setFramePosition()`、`PhysicsSystem::captureGranularMaterialSnapshots()`
- **確認できた事実:** Granular solverは通常再生時に`advancePhysicsFrame()`から前進する一方、checkpoint captureの呼び出しはseek replay経路にだけ存在した。capture関数自体は12 frame間隔、共通状態、Composition byte budgetを適用する。
- **変更:** 通常再生stepが完了したframeでもcapture関数を呼ぶよう接続した。
- **価値 / 懸念:** 通常再生で到達したcheckpointからseek replayを再開でき、baselineからの全再計算を避けられる。capture frameの一致とseek数値再現性は未検証。
- **次に確認:** 通常再生で12 frame境界のsnapshotが保存されること、そこからのseekがbaseline replayと同一状態になることをbuild/runtime確認する。

### 2026-10-12 — MPM外部collider摩擦は固定値

- **関連:** `Artifact/src/Composition/ArtifactAbstractComposition.cppm::evaluateLayerCollisionPairs()`
- **確認できた事実:** CompositionがMaterial/Granular solverへ登録する外部colliderはrestitutionをLayerの`physics.restitution`から読む一方、frictionは常に`0.2f`を設定する。MpmSolverはその値をGranular contact responseへ渡す。ArtifactのLayer property routingには`physics.friction`がなく、SoftBody colliderもfrictionを`0.15f`固定にしている。
- **価値 / 懸念:** 同じ砂Layerでも外部collider表面ごとの滑りやすさを調整できず、斜面堆積や壁沿いの流動が全source共通の摩擦へ依存する。既存UIから再利用できるpropertyは見つからず、摩擦設定を公開するには共有Layer/Collision契約の設計が必要。
- **次に確認:** MPM受入前に、固定摩擦係数で十分かをpile/colliderシナリオで評価する。足りなければCollision componentの摩擦設定を共通propertyとして導入する影響範囲・保存互換性・他solver統合を別途設計する。

### 2026-10-12 — MPM gravityとLayer transformの座標契約

- **関連:** `Artifact/src/Layer/ArtifactMpmSand2DLayer.cppm::configureSolver()`、`ArtifactCore/src/Physics/PhysicsSystem.cppm::createGranularMaterialGrid()`、`Artifact/src/Composition/ArtifactAbstractComposition.cppm::evaluateLayerCollisionPairs()`
- **確認できた事実:** 粒子と境界はLayer local座標で生成し、draw時にglobal transformを適用する。外部colliderはComposition座標からsolver localへ逆変換される。GravityもComposition座標からinverse linear transformでsolver localへ写す処理を`evaluateLayerCollisionPairs()`に実装した。Gravity値はLayer frame rateで時刻評価し、keyframe補間・expression・envelope・external overrideを読んでから変換する。property cacheはLayer constructorでgravity pathだけcold path初期化し、JSON復元後とsetter成功後にprivate値を同期する。
- **確認できた事実:** 普通のnon-roving keyframeは`keyFrameInterpolationSegment()`からallocation-free scalar補間を行う。範囲外は追加した`keyFrameBoundaryValue()`で端点をallocation-free取得し、既存segment APIの意味とShape Gradientキャッシュ動作を変えない。
- **価値 / 懸念:** explicit expression/envelopeとroving trackでは既存property evaluatorへfallbackするため、評価frameごとに内部vector copy等のallocationが発生する可能性がある。これは該当機能をGravityへ設定したLayerに限るが、hot-path profileは未実施。完全なallocation-free評価APIは現状見つかっていない。
- **仮説 / 未検証:** 静止した可逆affine Layer transformでは、local gravityをglobal linear transformでCompositionへ戻すと指定gravity `(0, gravityY)`になる。動的Transformのmoving-frame慣性項は実装しておらず、runtimeのgravity向き・倍率は未確認。
- **価値 / 懸念:** particle spacing、domain寸法、stress/densityのsolver-local意味論はTransformで画面変化するため、物理的な粒径や材料応答までworld-space不変ではない。特異／非affine／非finite transform、または変換gravityがfloat範囲外の場合はsolver gravityを0へfallbackする。
- **既存経路の追加確認:** Layer transformはposition/rotation/scaleを含むframe評価可能な `QTransform`。Compositionはframeごとにこのtransformとそのinverseを取得し、colliderをsolver localへ変換する。Layer physics bridgeにはGranular solver取得口があり、solver自身に`setGravity(gx, gy)`があるが、Granular gravity更新関数はbridgeにない。したがってAPI境界上、Compositionが評価済みtransformの線形部でworld gravityをlocalへ写してsolverへ設定する経路を足すことは可能。translationはベクトル変換に含めない。
- **step/snapshot経路の追加確認:** 通常再生は`setFramePosition()`の各stepでcollision pair評価後に`advancePhysicsFrame()`を呼ぶ。seek replayは`replayGranularMaterialToFrame()`でsnapshot restore/baseline reset後、各frameのlayer transform更新→collision pair評価→Granular advance→snapshot captureを行う。Granular snapshotは粒子/clock stateを持つが重力設定は持たない。world gravityを導入する場合、どのframe poseを次のintegrated intervalへ適用するかを両step経路で一致させ、restore直後にも再設定する必要がある。
- **座標型の追加確認:** `docs/planned/MILESTONE_TYPED_COORDINATE_SPACES_2026-09-27.md`はComposition/Layer vectorの型分離を進めているが、MPM gravity APIはraw floatでpx/s²の専用型は見当たらない。Composition→solver-local gravity mappingを実装する際は、明示的なnamed conversionを使うかcoordinate-space milestoneへ接続し、暗黙の`QTransform`/float混在を増やさない。
- **価値 / 懸念:** API追加とComposition frame評価の拡張で重力方向契約は合わせられる可能性がある。ただしsolverのparticle/domain寸法もLayer local pxのままなので、非一様scale時の物理的な長さ・material responseはworld-space不変ではない。gravityだけを逆変換するならLayer localをsolver単位とする局所シミュレーションであり、pixel物理の完全なworld-space実現とは区別して受入条件を記す必要がある。通常再生とseek replayの両経路でtransform/gravity同期が要る。
- **次に確認:** 逆変換の数値fixtureとruntimeでtranslation、rotation、反転、uniform/nonuniform scale後のComposition gravity、collider接触、重力keyframe、再生/seek一致を検証する。animated transformの非慣性項を含めるか、singular/non-affine fallbackをどう扱うかは受入仕様として評価する。

### 2026-10-12 — MPM density fieldは描画用の正規化surface

- **関連:** `ArtifactCore/src/Physics/MpmSolver2D.cppm::writeDensityField()`、`Artifact/src/Layer/ArtifactMpmSand2DLayer.cppm::densityAlpha()`
- **確認できた事実:** 各粒子は離散kernel weightの合計で`currentVolume`を正規化してfieldへ足すが、各pixel値は1.0で飽和し、field外へはみ出す粒子のweightもfield内sampleだけで再正規化される。
- **仮説 / 未検証:** 重なり・高密度状態ではfieldの面積積分は物理体積を保存しない。したがって現在のfield/alphaは描画surface用で、密度計測や質量保存の判定値には使えない。
- **価値 / 懸念:** 表示上のsurface再構成とsolverの質量保存を混同しない。今後field積分を体積検査へ転用する場合はsaturationと境界再正規化を除いた専用診断が必要。
- **次に確認:** 単粒子・重なり・field境界の解析配置で積分誤差を測り、描画閾値やsupport変更がsolver粒子状態へ影響しないことを確認する。
### 2026-10-12 — Granular GPU境界経路は現行契約では到達不能

- **関連:** `ArtifactCore/src/Physics/MpmSolver2D.cppm::setBackend()`、`setConstitutiveModel()`、`updateGPUSubsteps()`
- **確認できた事実:** Granularモデル選択中のGPU backend設定は拒否され、GPU backend中のGranularモデル選択も拒否される。したがってGPU substep後処理内のGranular分岐は現行APIから到達不能である。
- **訂正:** 以前GPU後処理にGranular APIC壁投影が欠落すると記録し、到達不能分岐へ変更を加えたが、現行製品経路の不具合ではなかった。無効な変更を戻した。
- **価値 / 懸念:** GPU砂対応を追加する際は、単に後処理へ分岐を足すだけでは不十分であり、GPU構成則・塑性・境界を含むCPU parityを実装・検証する必要がある。
- **次に確認:** GPU砂を明示的に対象化する設計時に、backend capability契約と対応fixtureをまとめて見直す。

### 2026-10-12 — Granular grid wall frictionを接触時のCoulomb制約へ変更

- **関連:** `ArtifactCore/src/Physics/MpmSolver2D.cppm::applyBoundaryConditions()`
- **確認できた事実:** Granular CPU solverは内部CFL timestepごとにgrid境界上の接線速度を`1-boundaryFriction`倍していた。接近していない粒子速度にも適用され、同じ実時間で内部step数が増えるほど摩擦が累積する。一方、外部collider応答は接触面の接線速度に別の一定割合を適用する。
- **変更:** Granular grid壁では外向き法線速度だけを取り除き、その速度補正量`|Δv_n|`を法線impulse相当として最大接線速度変化を`μ|Δv_n|`に制限し、接線速度を0で止めるCoulomb型の応答へ変更。離反・接触していない状態では接線速度を変えない。Corotated legacy境界式は維持。
- **価値 / 懸念:** 壁に触れていない粒子へ摩擦を適用する誤りを避け、一定割合の毎substep減衰によるCFL step数依存を減らす。grid boundaryの離散接触は投影法なので、corner接触と外部collider摩擦との数値的一致は未検証。
- **次に確認:** 解析fixtureで左右上下壁の法線非貫通、`|Δv_t| <= μ|Δv_n|`、接触なし／離反時の接線速度不変、Corotated軌跡不変、CFL timestep変更時の同一接触結果を確認する。

### 2026-10-12 — Granular外部collider摩擦を接触impulse上限へ変更

- **関連:** `ArtifactCore/src/Physics/MpmSolver2D.cppm::applyGranularColliderVelocityResponse()`、`resolveColliders()`
- **確認できた事実:** Plane/Circle/Polygon/Boxで形状接触と位置補正が成立した際、Granularの接線速度と`C`行列を摩擦係数に基づいて縮小していたが、接近法線速度が正（離反）でも同じ減衰を実行していた。
- **変更:** Granularは法線接近速度`v_n < 0`の場合のみ反発を適用し、`Δv_n=-(1+e)v_n`を上限算出に使う。接線速度は`μΔv_n`以下の変化で0まで止め、これを超える成分だけ残す。離反状態では速度・`C`を摩擦で変更しない。Corotated応答式は維持。
- **静的照合:** `v_n=-2, e=0.25, μ=0.4`なら法線補正量は2.5、接線変化上限は1.0。接線速さ0.5は0まで減り、速さ2は1残るため、接線変化量は上限を超えない。
- **追加確認:** Plane/Boxは軸方向の単位法線、Circleは半径差分を距離で除した単位法線、Polygonは内側押し戻し差分または辺方向を長さで除した単位法線を応答関数へ渡す。
- **価値 / 懸念:** 接触中の摩擦を法線運動量交換に制限し、Collider内へ位置補正された離反粒子への無条件な摩擦を除く。形状境界からの法線推定・cornerや移動collider速度を含めた挙動はruntime未確認。
- **次に確認:** 各shapeで接近・離反・接線速度の解析fixtureを行い、反発後法線速度とCoulomb上限、CPU replay経路での決定性を確認する。

### 2026-10-12 — Polygon頂点接触法線を凸凹と接近速度に応じて選択

- **関連:** `ArtifactCore/src/Physics/MpmSolver2D.cppm::mpmQueryPolygon()`、`resolvePolygonCollider()`
- **確認できた事実:** Polygon境界上で粒子が頂点と一致すると最短点差分がゼロとなり、以前はループ内で最初に見つかった辺の法線だけをfallbackに使っていた。隣接法線の単純平均だけでは、反射するturnを持つ凹頂点で和が内部を向く場合がある。
- **変更:** 同じ頂点に接する二辺の単位外向き法線を保持し、windingを掛けた辺turnで凸／凹を判定する。凸頂点では両法線へ同時進入する場合に限り和を使い、片面のみへの進入ならその面を使う。凹頂点は接近速度との内積がより負の面法線を使う。法線の和は再正規化し、面積がほぼ0のGranular polygonは接触解決対象から除外する。法線算出も最短辺候補に限定し、投影不能な長さ1e-6以下の辺は候補法線にしない。
- **価値 / 懸念:** winding反転や頂点の辺走査順に依存する法線選択を抑え、凹頂点で内向きの二等分法線を使う問題を避ける。自己交差・重複・極端に短い辺は未検証。
- **次に確認:** 同じ凸／凹頂点をCW/CCWで表して接近速度、選択法線、反発・摩擦が一致すること、重複頂点・退化形状で有限な応答となることを解析fixtureで確認する。

### 2026-10-12 — GPU砂輪郭のclipを廃止しCPUと同じ三角形補間へ統一

- **関連:** `Artifact/src/Layer/ArtifactMpmSand2DLayer.cppm::draw()`、`emitAlphaTriangle()`
- **確認できた事実:** CPU Render Queueはsmoothstep後のdensity alpha fieldを、GPU surfaceと同じ固定対角線の二triangle piecewise-linear補間で出力する。
- **変更:** 従来のdensity threshold geometry clippingを廃止し、元の三角形と各頂点alphaを保持する。全頂点alphaが0の三角形のみ省略する。これでCPU/GPUの補間関数と対角線が一致し、最大頂点数は1セル6（従来12）、二本のposition+alpha scratch bufferは約3.5 MiBから約1.75 MiBへ減る。
- **価値 / 懸念:** ソフトウェア面とGPU面のalpha field定義を揃え、境界交点の特別処理も不要にする。GPU rasterization、target format、blend経路を含む実画素parityは未検証。
- **次に確認:** 同じdensity fieldをCPU/GPUへ与え、alpha境界・細い支持領域・sRGB targetでpremultiplied pixel差を比較する。

### 2026-10-12 — MPM砂のJSON復元からsolver bindまでの順序を確認

- **関連:** `Artifact/src/Layer/ArtifactLayerFactory.cppm::createArtifactLayerFromJson()`、`Artifact/src/Composition/ArtifactAbstractComposition.cppm::appendLayerTop()` / `fromJson()`、`ArtifactMpmSand2DLayer::setComposition()`
- **確認できた事実:** FactoryはLayer JSON propertiesを復元してserialized Layer IDを戻した後、Compositionへ渡す。Composition appendが`setComposition()`でworld/Layer ID付きGranular solverを構築し、`isCompositionAttachmentReady()`で登録有無を検査する。Composition `fromJson()`はLayer attach後に保存frameへ移動してGranular replayへ入る。
- **価値 / 懸念:** 静的な保存・load順序ではsolverが未復元IDのまま登録される問題は見つからなかった。solver state自体はJSON保存せず、authoring settingsとseedから再構築してframe replayする。
- **次に確認:** project save/load、Render Queue clone、Undo/Redoのruntimeで、solver registry bind、保存frameの粒子状態、seek往復が再現することを確認する。

### 2026-10-12 — Granular dampingと壁摩擦のCFL刻み依存を静的照合

- **関連:** `ArtifactCore/src/Physics/MpmSolver2D.cppm::updateGridVelocities()`、`applyBoundaryConditions()`
- **確認できた事実:** Granular damping factorは`(1-d)^(120*dt)`であり、自由減衰の係数を複数stepで積算すると`(1-d)^(120*Σdt)`になる。壁摩擦は外向き法線速度補正量ごとに`μ|Δv_n|`以内で接線速度を減らす。
- **静的照合:** 同一総時間かつ他の力がない場合、step数を分割してもdamping係数の積は一致する。同一の総法線impulseを壁で受けた場合、接線速度の最大減少量の合計は`μΣ|Δv_n|`でstep数に依存しない。
- **価値 / 懸念:** 両項目は一定割合の毎step減衰と異なり、CFL step数だけで係数が変化しない設計。ただし力積分、接触発生時刻、粒子-G2P移送は刻み幅で変わり得るため、完全な軌跡一致は保証しない。
- **次に確認:** 同じ総物理時間でCFL substep数を変え、自由減衰解析解と壁の法線impulse・接線速度差をruntime比較する。
### 2026-10-12 — MPM Gravityのkeyframe状態はレイヤー固有JSONで永続化する

- **関連:** `Artifact/src/Layer/ArtifactMpmSand2DLayer.cppm::toJson()` / `fromJsonProperties()`、`ArtifactCore/include/Property/PropertySerializationBridge.ixx`
- **確認できた事実:** `ArtifactAbstractLayerPersistence` の一般プロパティ保存対象にはTransformなどの限定経路があり、`mpmSand.gravityY` のようなLayer固有property cacheは自動で保存されない。CompositionはMPM Gravityをframeごとにproperty evaluatorから読むため、keyframe等を保存しないと再読込後の挙動が変わり得る。
- **変更:** scalar gravityは従来キーを維持したまま、animatable flag/expression/keyframes/envelopesを専用JSONキーで追加保存・復元する。Project duplicate helperはproperty cacheへ直接deserializeしてprivate authoring scalarを更新しないため、Gravity復元後にanimatable flagをコピーしてMPM setterも呼び同期する。
- **追加修正:** `SetLayerPropertyValueCommand`のUndo/RedoではMPM setter成功後に一般property cacheへ値を書き戻すfallbackが禁止されており、Gravity以外のMPM scalar setterもcacheを同期していなかった。MPM parameter setterの同値成功・構成更新成功後に内部canonical値をcacheへ反映する。
- **価値／懸念:** 再読込・複製後もアニメーションしたGravity値とsolver authoring stateを揃える。複製時のsolver予算不足、Undo/Redo、expression評価はruntime未検証。
- **次に確認:** MPM layerのsave/load・duplicate・Undo/Redoでscalar値とanimationが揃い、Composition replay結果が一致することをruntime確認する。

\n
## 2026-10-20: Rectangular liquid surface overhang was a render artifact

- **関連:** `Artifact/src/Layer/ArtifactAbstractLayerFractureRuntime.cppm`, `tests/Artifact/PhysicsLodBridgeContractTest.cpp`, `Artifact/cmake/RunWindeployqt.cmake`。
- **確認できた事実:** コーヒーの外にはみ出して見えた薄茶色の部分は別の液体や泡ではなく、再構成した液面メッシュと縁線が容器の矩形境界を越えて描画されたものだった。液面三角形と輪郭線を容器内でクリップし、D3D12 capture の境界画素契約が成功。最終 frame12 capture でも壁・底の外への液体画素は見られない。
- **変更・価値:** 表面の深さによる色変化、広い反射、泡量に応じたクレマ線を調整し、はみ出し描画を除去した。境界クリップは固定長のスタック領域を使い、フレーム中の動的確保を増やさない。Debug の Qt DLL 配置については `windeployqt` が Debug Qt の bin を参照するよう別途修正した。
- **未確認:** 通常サイズのアプリ viewport での見え方、矩形以外の容器の境界、屈折を含む写実的な水表現。実際にユーザーが見た DLL エラー文言と deployment path 修正の完全な因果関係も未確認。
- **次に確認:** 通常 viewport でコーヒーと水の両方を確認し、必要なら容器形状に応じた境界処理を設計する。

### 2026-10-12 — MPM material reset後の通常再生は現在frameへ追いつかせる

- **関連:** `ArtifactCore/src/Physics/PhysicsSystem.cppm::setGranularMaterialSolver()` / `GranularMaterialWorldClock`、`Artifact/src/Composition/ArtifactAbstractComposition.cppm::setFramePosition()` / `replayGranularMaterialToFrame()`。
- **確認できた事実:** material再登録はworld snapshotを消去してbaselineを差し替える。従来のseek pathは新baselineから目標frameまでreplayする一方、1〜8 frameの通常再生pathは現在のlive solverをそのまま一歩進めるため、設定変更時のComposition frameが0より大きいと通常再生とseekが異なる状態を作り得た。
- **変更:** world clockに`requiresReplay`を持たせ、solver登録後最初のincremental Composition advance前に前frameまで共通replayを行う。1つのsolver解除で残solverのworld checkpointが消えた場合も同じflagを立てる。短い進行と大seek/巻き戻しの両方でrequired replayが失敗したら前frameへrollbackする。replay完了時だけflagを消す。
- **追加修正:** Composition frame rate変更はlive granular stateを保持する既存契約のため、world checkpointを破棄後にframe intervalを無視して変更時frameを強制captureする。capture成功ならそのframeをseek anchorとしてflagを消し、budget/capture失敗ならflagを残して次incremental advanceでbaseline replayへfallbackする。
- **価値／懸念:** 設定変更後の通常再生をseek時と同じbaseline/frame経路へ揃える。高いframe位置での初回再生はbaselineからのreplay時間が増える。runtime決定性・失敗回復・大frame番号の負荷は未検証。
- **次に確認:** frame Nでmaterial設定変更後の通常play N→N+1とseek N+1のparticle snapshotを比較し、world内複数MPM Layer、baseline replay失敗、最初のframe/大frame catch-upを確認する。

### 2026-10-12 — MPM frame-rate変更のseek anchor保持

- **関連:** `ArtifactCore/src/Physics/PhysicsSystem.cppm::captureGranularMaterialSnapshots()` / `trimGranularWorldSnapshots()`、`Artifact/src/Composition/ArtifactAbstractComposition.cppm::setFrameRate()`
- **確認できた事実:** frame-rate変更後のlive solver stateを保つため現在frameを強制checkpointにするが、通常checkpoint上限で古いanchorを消すと、その後のseekがframe 0 baselineから再計算され、変更直後のlive stateと分岐し得る。
- **変更:** 強制checkpoint frameをworld clockのreplay anchorとして記録し、通常frame-count trimmingとbyte-budget evictionの双方で保持する。byte budgetがanchorと新規checkpointを収容できない場合は新規captureをrollbackし、既存のbaseline replay fallbackへ戻す。evictionはworld共通の同一frameを全solverから取り除く。
- **価値／懸念:** frame-rate変更時のlive-state保持と後続seekの共通起点を両立する。pinned anchorのため利用可能なrolling checkpoint数が1つ減り、予算不足ではcapture失敗が起こりやすくなる。runtime未検証。
- **次に確認:** 複数MPM Layer、低いsnapshot budget、古いanchorと通常checkpointのeviction、frame-rate変更後の現在frame以降seekで状態一致を確認する。
- **追加修正:** seekがpinned anchorより前の共通checkpointへ戻るとき、そのpinを失効させる。またreplayの到達frameでもLayerを評価してcollision proxyとgravityを同期し、復元particle stateを次stepへ正しく接続する。
- **追加修正:** Composition frame-rate setterは同一Compositionを全Layerへ再bindする。MPM Layerの同Composition bindでは有効solverを保持し、baseの`setComposition()`でTransform3D base/variantのtime scaleだけを更新する。registry missing時だけrebind recoveryでsolverを再構築する。
- **追加修正:** CPU MPM software surfaceは頂点alphaをfield段階で8-bit化後bilinear補間していたが、GPU surfaceは同じfield gridを固定diagonalのtriangle meshでpiecewise-linear補間する。CPU resampleをGPUと同じtriangle splitへ合わせ、出力pixelでalphaを量子化する。

### 2026-10-12 — MPM seek replay失敗時にframe移動を拒否

- **関連:** `Artifact/src/Composition/ArtifactAbstractComposition.cppm::setFramePosition()`、`ArtifactCore/src/Physics/PhysicsSystem.cppm::hasGranularMaterialSolvers()`
- **確認できた事実:** 大seek/巻き戻しはGranular replayが失敗しても、solver再登録由来の`requiresReplay`が立っている場合だけComposition positionを戻していた。通常のbaseline/checkpoint restore failureでは古い粒子状態のままframeだけ移動し得る。
- **変更:** PhysicsSystemで対象worldのactive solver有無を判定し、solverがある状態でreplayが失敗したseekをprevious frameへrollbackする。
- **価値／懸念:** simulation frameとComposition frameのずれを防ぐ。追加の静的確認では、replayのfalse returnはcheckpoint/baselineの全solver事前検証で失敗する経路であり、replay進行ループにはfalse returnがない。checkpoint capture failureはcache missとして無視されるため、それ自体でsolver advanceが中断されるわけではない。bool契約どおりの実装を確認したが、実行時の破損状態fixtureは未検証。
- **次に確認:** baseline restore不可・共通checkpoint破損のfixtureでComposition frameが変わらず、solver stateも元に戻ることをruntime確認する。
- **追加修正:** UI/Timestamp seekが通る`goToFrame()`もGranular replayの戻り値を確認し、solverが存在するCompositionで失敗したら呼出し前frameへposition/Layerを戻し、Soft Bodyとrigid authored stateを復元してcollision/joint評価を更新する。

### 2026-10-12 — MPM surface meshのper-layer scratch memory

- **関連:** `Artifact/src/Layer/ArtifactMpmSand2DLayer.cppm::kMpmMaximumSurfaceVertices` / constructor、`Artifact/include/Render/RenderCommandBuffer.ixx::MpmSurfaceVertex`
- **確認できた事実:** 128×96 density fieldから作る130×98セルは、旧threshold-clip方式で最大12頂点/cellだった。旧6-float vertexで二本の全容量scratchは約7 MiB。RGBをpacket constant colorへ移してposition+alphaの3-float vertexとし、その後のCPU/GPU三角形補間統一で上限を6頂点/cellへ変更した。**現行:** 76,440頂点、二本の全容量scratchは1,834,560 bytes（約1.75 MiB）。GPU shader ABIは既存`RectVertex`変換後のまま。
- **未検証の仮説:** 多数の低粒子数MPM Layerでは粒子状態よりsurface scratchが先に総メモリを支配し得る。頂点payloadの半減後も、実sceneのLayer数・実頂点数・GPU packet lifetimeの計測が必要。
- **価値／懸念:** full-capacity reserveはframe-time再確保を避けてsurface生成をboundedにする。vertex縮小とtriangle cullingで旧設計よりscratchは小さいが、renderer packetを含む総memoryは未計測。
- **次に確認:** 1/10/50 MPM Layerでidle時と同時描画時のresident/capacity bytesを測り、default設定でのsurface vertex high-water markとpacket retentionを記録する。その結果からshared bounded poolへ進むか判断する。

### 2026-10-12 — Granular CFLをsolver内部stepごとに更新

- **関連:** `ArtifactCore/src/Physics/MpmSolver2D.cppm::update()` / `updateGranularTimeStepForState()`
- **確認できた事実:** 変更前はGranular CFL刻みを`update()`入口で一度決め、その後の最大1024 internal stepすべてへ使っていた。応力加速や移流が同じbatch内で速度を変えた後も、次stepの刻みは更新されなかった。
- **変更:** 初回CFL選択後、各step実行後の粒子状態から次のGranular刻みを再計算する。remaining elapsed timeが新刻みを満たさなければstepを行わず、余りを既存accumulatorへ保持する。Corotated経路とComposition outer clockは変更しない。
- **価値／懸念:** 1つの`update()`内で粒子速度が上がるケースにもstate-dependent CFLを適用する。粒子数が多い場合に毎step粒子scanが増えるためCPU負荷が増加し、CFL制約・再生一致ともruntime未検証。
- **次に確認:** 高剛性・大重力・衝突で1つのouter frameに複数solver substepが発生するsceneを用い、各stepで`dt * (waveSpeed + advectionBound) <= 0.05 * dx`を記録する。加えて同一elapsedを異なるouter frame分割で与え、粒子snapshotとaccumulatorを比較する。

### 2026-10-12 — MPM砂の弾性応力を全変形勾配へ連鎖

- **関連:** `ArtifactCore/src/Physics/MpmSolver2D.cppm::computeGridForces()`、`firstPiolaKirchhoff()`、`MpmParticle2D::{F,Fp}`。
- **確認できた事実:** Granularの`firstPiolaKirchhoff(Fe)`は`Fe=F Fp⁻¹`に対する弾性PK1 `P_e=∂ψ/∂Fe`を返す。粒子grid forceの形状関数勾配は全変形`F`に対応し、変形エネルギーを全Fで微分するには`P=P_e Fp⁻ᵀ`が必要。従来の`computeGridForces()`は非単位`Fp`でも`P_e`をそのまま使用していた。
- **変更:** Granularモデルだけ`P_e Fp⁻ᵀ`へ変換してからgrid forceへ渡す。Corotated modelの既存stress経路は変更しない。2×2行列値演算のみで追加allocationなし。
- **一次資料:** Klár et al., [Supplementary Technical Document](https://www.cs.ucr.edu/~craigs/papers/2016-sand/tech-doc.pdf), §5.2 defines `PE=∫Ω0 ψ(Fe)dX` and derives total PK1 `P=(∂ψ/∂Fe)Fp⁻ᵀ`. This confirms there is no extra `det(Fp)` multiplier under this energy convention.
- **価値／懸念:** 塑性履歴後の弾性応力力を、`Fe=F Fp⁻¹`の定義と全変形勾配に対して整合させる。独立数値例でanalytic gradientと中央差分の最大誤差は`5.1e-11`だった。solver/C++ float経路での力・運動量・塑性流動への影響は未検証。
- **次に確認:** `Fp=I`で旧Granular力と一致、Corotated経路で回帰差分なしを静的に確認し、テスト実行が明示許可された後にC++ fixtureとruntime比較を行う。

### 2026-10-12 — MPM有限ひずみCFL波速の保守性は未確認

- **関連:** `ArtifactCore/src/Physics/MpmSolver2D.cppm::updateGranularTimeStepForState()`、Granular Hencky弾性応力。
- **確認できた事実:** 旧CFLは各粒子の参照密度`mass/volume0`とrest-state縦波係数`lambda+2*mu`のみを使っていた。Granular応力はHencky energy `ψ=μΣ(log σi)^2 + λ/2(Σlog σi)^2`に基づくため、弾性状態のPK1接線は特異値に依存する。
- **解析:** `Fe=diag(s,1)`では`P11=(2μ+λ)log(s)/s`、主方向PK1 tangentは`(2μ+λ)(1-log(s))/s²`。PK1微分固有値に配置変換の特異値因子`s²`を掛けると、`μ=1, λ=1.5, s=0.5`で基準`λ+2μ`比は速度約1.301倍。`Fe=sI`では主特異値だけを摂動するtangentは`((2μ+λ)-(2μ+2λ)log(s))/s²`で、速度比候補は約1.411倍。以前の2.60倍値はPK1微分固有値をそのまま速度へ対応させた誤りとして撤回。
- **変更:** `granularHenckyAcousticSpeedSquaredUpperBound()`を追加。Hencky energyの2D主基底接線から方向別の2×2 acoustic tensorを組み、各Gershgorin行和を全角度に対して解析最大化する。上界とrest-state係数の大きい方をGranular CFLへ使うため、角度サンプリングによる見逃しを避ける。反復特異値は完全一致時だけ解析極限を使い、近接値では`log1p`と因数分解した平方差で評価する。CFL用singular value extractionは主方向の三角関数を計算しない。固定`Fp`で`Fe=F Fp⁻¹`を微分すると、弾性側入力方向・出力応力の双方にある`Fp`変換はcurrent-space acoustic contractionで相殺される。
- **独立照合:** 主基底の解析接線行列はPK1 rank-one中央差分（`h=1e-6`）と複数伸長・方向で最大絶対誤差約`1.5e-9`。`σ1,σ2∈exp([-3,3])`の21×21状態格子と各状態721方向を走査し、Gershgorin上界の下回りは0件。加えて`ν=-0.9,-0.5,0,0.3,0.45`ごとに`σ1,σ2∈exp([-2.5,2.5])`の13×13状態・361方向を走査し、下回り0件だった。正のE・密度は全接線と質量密度を同率scaleするため速度比走査に影響しない。対角stateでの全方向最大は一部で主軸から外れ、軸候補だけでは不足することも確認した。C++ float parity、plastic active-set/return-map後の離散安定性、runtime性能は未検証。
- **価値／懸念:** CFLは有限ひずみ弾性剛性の明示的な全方向上界を使う。Gershgorin評価は実際の最大固有値より保守的で刻みを小さくする場合がある。特異値/上界計算が失敗したstateではrest-state縦波係数へfallbackし、前stepの伸長由来の大きなdtを残さない。粒子ごとの特異値抽出と音響上界評価は毎Granular substepで走るため、実sceneのCPU費用は未計測。またこのwave boundはそのstep開始時の弾性predictorを線形化したもの。plastic projection・衝突・MPM transfer由来の追加安定制限までは包含しない。
- **出典:** Sun, Shinar & Schroeder, *Time step restrictions for MPM* (2020), [paper](https://cs.ucr.edu/~shinar/papers/2020_explicit_mpm_dt.pdf) は、rest-configurationの線形音速は有限変形で過小または過大になり得ること、一般等方超弾性で特性音速を導出することを述べる。また音速は`J`だけに依存しない例を示す。これは本件の状態依存CFL必要性の根拠であり、本Layerの2D Drucker–Prager塑性接線や積分安定性を直接証明しない。
- **次に確認:** C++ float実装値と独立差分を比較し、active Granular solverで実際に選ばれたdtが`dt*(waveUpperBound+advectionBound)<=0.05*dx`を満たすことを許可後に確認する。plastic projection・grid transfer・collisionの固有安定性は別gateとして扱う。

### 2026-10-12 — Granular外部Collider補正後の変形勾配

- **関連:** `ArtifactCore/src/Physics/MpmSolver2D.cppm::stepOnce()` / `resolveColliders()`
- **確認できた事実:** Granular stepはG2Pで速度勾配から`F`を更新し、塑性return mapを適用した後に`resolveColliders()`が粒子位置・速度・APIC `C`を補正する。外部Colliderによるpenetration correctionはそのstepの`F`/`Fp`/hardening historyを巻き戻さない。既存の最終state guardはfinite/正determinantなどの破損回避を行うが、strain量の妥当性は評価していない。
- **未検証の仮説:** 粒子が外部Colliderへ一時貫入したstepで、衝突前速度勾配由来の圧縮・塑性硬化が残り、単なる接触位置補正より強い局所圧密を作る可能性がある。一方、通常の接触応力として妥当な場合もあり、コードだけでは不具合と断定できない。
- **価値／懸念:** 粒子非貫通・速度反発だけでなく、砂の体積変化と塑性履歴が接触応答として自然か確認する必要を示す。Fを接触後に単純rollbackすると接触の応力経路まで消すため、測定なしに補正式を導入しない。
- **次に確認:** 固定seedで外部collider衝突とdomain wall衝突を比較し、接触前後の`det(F)`、`det(Fp)`、hardening、反発、積算体積を記録する。colliderなし同等stepと比べ、penetration projectionだけに起因する過剰変形が再現した場合は、grid-side contactを含む責務を設計する。

### 2026-10-12 — CFL下限到達時に以前の大きな刻みを残さない

- **関連:** `ArtifactCore/src/Physics/MpmSolver2D.cppm::updateGranularTimeStepForState()`。
- **確認できた事実:** wave speedだけで`0.05*dx/1e-6`を超えると、従来は関数がreturnし、前回の`fixedDt_`が残った。Sand layer生成APIのE/ν/密度制約と初期step budgetの範囲では、独立した101×101 log-stretch走査（ν=-0.9,-0.5,0,0.3,0.45）でrest縦波係数に対する最大上界比は約164.6、受理可能な初期speed/dx目安768に対し最小刻み限界は50000だった。この走査はfloat演算や塑性更新を含まず、C++ runtime検証ではない。
- **変更:** wave speedだけで最小刻みのCFL予算を超えた場合、前回値を残すより最小刻みへ落とす。CFL条件そのものは満たせないので、これは過大な既存刻みを避けるfail-safeであり、安定性保証ではない。
- **価値／懸念:** 異常または低レベルAPI経由の極端な状態で前step由来の大きなdtを保持しない。最小刻みでも波動CFL違反が残り得る。低レベルAPI材料設定を含む到達性とruntime結果は未確認。
- **次に確認:** C++ float実装で極端な伸長・圧縮と直接setMaterial設定を走査し、最小刻みfallback、エラー診断、回復挙動を確認する。

### 2026-10-12 — MPM Sandの表示fieldは塑性せん断形状を直接描かない

- **関連:** `ArtifactCore/src/Physics/MpmSolver2D.cppm::writeDensityField()`、`Artifact/src/Layer/ArtifactMpmSand2DLayer.cppm::draw()`。
- **確認できた事実:** density fieldの各寄与は現在体積`volume0*det(F)`を使うが、supportはscalar radiusから作るaxis-aligned kernelで、`F`の主方向・せん断成分を描画kernelへ反映しない。Layer描画はそのfieldをalpha surfaceへ変換する。
- **未検証の仮説:** 強いせん断・伸長した領域でも個々のquadrature particle supportは円形のままなので、粗いfield解像度では局所輪郭の引き延ばしが弱く見える可能性がある。ただしMPM material pointは物理的な粒子形状ではなく積分点であり、変形楕円を描く方が材料連続体の見た目に正しいとは限らない。
- **価値／懸念:** simulation stateの`F`とrender surfaceの再構成を混同しない。粒子kernelを変えるとGPU/CPU parity、field normalization、支持範囲・hot-path費用にも影響するため、表示改善は受入sceneでの比較が先。
- **次に確認:** 単純せん断・一軸伸長・圧縮の固定stateで、現行scalar kernel、変形計量を使うellipse kernel、より高解像度のfieldを同一scene上で可視比較し、輪郭の連続性とCPU費用を測る。

### 2026-10-12 — MPM Sandの境界摩擦は内部摩擦角と独立の固定値

- **関連:** `Artifact/src/Composition/ArtifactAbstractComposition.cppm::evaluateLayerCollisionPairs()`、`ArtifactCore/src/Physics/PhysicsSystem.cppm::createGranularMaterialGrid()`、`ArtifactCore/src/Physics/MpmSolver2D.cppm::applyBoundaryConditions()`。
- **確認できた事実:** 外部collision sourceからMPM colliderを組む際に`friction=0.2`を設定し、Granular Layerのsolver domain wallには`boundaryFriction=0.25`を設定する。Layerの`frictionAngleDegrees`は別物性としてDrucker–Prager内部yieldへ使われる。現在のMPM Sand property groupに外部collider／domain wall摩擦の編集項目はない。
- **未検証の仮説:** 外部接触面や砂箱壁の滑りを内部安息角とは別に調整したい制作sceneでは、固定値がチューニングを制限する可能性がある。既存Collision componentの摩擦編集契約が見当たらないため、いきなり共通UIまたはLayer専用設定を増やす根拠はまだない。
- **価値／懸念:** 材料内部摩擦と境界接線応答を混同しない。外部shapeの既定値は既存Mpm colliderの固定係数とも関係するため、変更する場合は他のmaterial solverへの影響を避ける設計が必要。
- **次に確認:** 受入sceneでdomain wallと外部colliderごとの滑りを評価し、必要なら独立設定のowner・保存先・既存Collision UIとの関係を決める。

### 2026-10-12 — MPM Sandのsource boundsは現在simulation enclosureでもある

- **関連:** `Artifact/src/Layer/ArtifactMpmSand2DLayer.cppm::configureSolver()` / `draw()` / `getSoftwareRenderSurface()`、`ArtifactCore/src/Physics/PhysicsSystem.cppm::createGranularMaterialGrid()`、`ArtifactCore/src/Physics/MpmSolver2D.cppm::stepOnce()`。
- **確認できた事実:** seedはsource sizeの60%×35%領域へ置かれる一方、granular solver domain/boundaryはsource rectangle全体`[0,width]×[0,height]`。CPU/GPU描画fieldも同じsource rectangleに限定され、solverはparticle-level wall clampでdomain外の粒子中心を止める。
- **解釈:** seed rectangleの外へ下方に広がる砂は現domain内で描画できる。source rectangleそのものを越える砂は物理境界で停止するため、現在のコードにおいてrender culling/clipによる粒子消失とは異なる。
- **未検証の仕様差:** milestoneの「初期Layer rectangle外へ広がってもclip/cullされない」がseed boundsを指すなら現構造と整合する。source boundsを越えるopen-domain sandを指すなら、有限grid/domain拡張、snapshot geometry契約、CPU/GPU/Render Queueの描画origin・boundsを一体で設計する必要があり、現実装では未達。どちらを受入契約とするかはコードだけから確定できない。
- **次に確認:** 受入sceneでLayer rectangleを砂箱の壁として使うか、描画/transform boundsの外へ伸びるopen-domainとして使うかを決める。その判断まではsolver wallやLayer geometryの意味を推測で変更しない。

### 2026-10-12 — 非アクティブMPMレイヤーは再アクティブ化まで時間を止める

- **関連:** `Artifact/src/Composition/ArtifactAbstractComposition.cppm::evaluateLayerCollisionPairs()`、`ArtifactCore/src/Physics/PhysicsSystem.cppm::advanceGranularMaterialSolvers()`
- **確認できた事実:** Composition上でLayerがinactiveのframeでは、そのMPM solverだけworld clockのstep対象から外れる。world clockは他のactive solverとともに進み、Layerが再びactiveになるとsolverは保存されていた粒子状態から進行を再開する。seek replayでは各frameのLayer activityを再評価する。
- **未検証の仮説:** visibility/activityをシミュレーションpauseとして扱う意図なら一貫している。非表示でも物理時間を進める期待なら、復帰時のcatch-up仕様が必要になる。
- **価値／懸念:** 一つのComposition world内でLayerごとに時間経過が異なるため、衝突相手がactive期間中だけ相互作用する。pauseとcatch-upのどちらを製品挙動にするかはまだ確認されていない。
- **次に確認:** Layerのin/out、enabled/visibility、通常再生とseekで「inactive中の砂」を止める仕様か、時間を進める仕様かを決め、必要なら受入fixtureへ追加する。
### 2026-10-12 — Video previewはfloat変換後のupload再量子化が重なる

- **関連:** `Artifact/src/Layer/ArtifactVideoLayer.cppm::cpuVideoFrameToImageF32x4_RGBA()`、`ArtifactCore/include/Image/ImageF32x4_RGBA.ixx::setFromCVMatOwned()`、`ArtifactCore/include/Image/SurfacePixelConversion.ixx::convertSurfacePixels()`、`Artifact/src/Render/PrimitiveRenderer2D.cppm::drawSpriteTransformed()`。
- **確認できた事実:** 独立Composition Preview BenchのH.264 1080p30では、RGB24からfloat化するCPU変換が所有権移譲前約19–23ms、移譲後約6.5msとなった。一方、そのfloat画像をRGBA8 sRGB textureへ変換するupload conversionに約10–12ms、texture作成に約2–4msを要した。最適化後もH.264 1080p30/720p30とProRes 422 HQの1・2 layer strict benchはいずれも不合格だった。H.264 decoderはFFmpeg software backendで動作した。
- **未検証の仮説:** RGB24をfloat bufferに展開してからRGBA8 textureへ戻す二段変換を避ける動画専用byte-surface経路、またはGPU側での色変換を導入すれば帯域とCPU時間を削減できる可能性がある。ただし`ImageF32x4_RGBA`は他の合成・プレビューAPIからfloat surfaceとしても利用されるため、単純な内部storage形式変更は互換性を壊す恐れがある。
- **価値／懸念:** CPU変換とtexture uploadの双方が実測でフレーム予算を使っていると切り分けられた。RAM previewの追試は準備中に約1.1 GiBへ達して停止しており、キャッシュ利用での救済は未確認。
- **次に確認:** 動画Layer専用の8-bit frame payloadを導入する場合、color transform、shared payload、frame cache、software fallback、Composition preview、export/renderの各consumerを列挙し、float APIへ変換する箇所を境界で明示する。Diligentのdynamic texture update／staging reuseとGPU decodeの現状も、renderer/backendの契約を確認してから比較する。

### 2026-10-12 — 動画preview texture slot再利用とbounded async変換候補

- **関連:** `Artifact/src/Render/PrimitiveRenderer2D.cppm::textureForVideoFrame()`、`Artifact/src/Widgets/Render/ArtifactCompositionRenderController.cppm`、`tools/VideoPreviewBench/README.md`。
- **確認できた事実:** 独立benchのH.264 1080p30で、動画Layer IDと寸法ごとにGPU textureを保持し、同一frame再描画時は転送を省き、新frameでは既存slotを`UpdateTexture`するbounded poolを追加した。既存texture cache上限は50件／512 MiB。profileは初回slot作成約1ms、更新約0–1ms、毎新frameのfloat→RGBA8 CPU conversion約11msを示す。追試2回の1/2 layer dropped/late/repeatedは1/1/10・3/7/8、1/2/13・3/6/4でいずれもstrict不合格。実行差があるため安定したend-to-end改善は未確認。
- **未検証の仮説:** CPU pixel conversionをdecode後のworkerへ移し、3個程度のfixed upload bufferとgeneration tagで結果を捨てればrender laneの11msを隠せる可能性がある。ただしdecode worker競合や追加memoryがあるため性能改善は未証明。Diligent context/texture操作をworkerへ移す設計は所有権・同期上の危険がある。
- **価値／懸念:** texture作成再利用は入ったが、ユーザーの無drop/late/repeat条件を満たしていない。1080p RGBA8は約8 MiB/frameなので、全RAM frameにsidecarを持つとpreview memoryが大きく増える。async化するなら固定数buffer、bounded backpressure、stale generation rejection、queue saturation counterを含め、GPU更新をrender ownerに留める必要がある。
- **次に確認:** bounded async conversionを加える前にdecoder frame lifetime、既存task poolの低遅延task経路、frame generation/cancel境界、buffer lease lifetimeを実コード上で確認する。導入後は同一独立benchを複数回走らせ、drop/late/repeated=0を達成するまで採用扱いにしない。

### 2026-10-12 — FFmpeg D3D11VAはComposition direct decodeで動作、preview条件は未達

- **関連:** `ArtifactCore/src/Media/MediaImageFrameDecoder.cppm`、`ArtifactCore/src/Media/MediaPlaybackController.cppm`、`tools/VideoPreviewBench/main.cpp`。
- **確認できた事実:** 同梱FFmpeg CLIでH.264 1080p30を5回ずつdecode。各run 90/90 frames。median wall timeはsoftware 0.18 s、D3D11VA 0.16 s、CUDA 0.16 s（GPU frameをNV12でCPUへdownloadする処理込み）。verbose logのhardware output formatはD3D11=`d3d11`、CUDA=`cuda`。ProRes HQはsoftwareのみ90/90 frames、median 0.14 sでD3D11VA hardware frameなし。
- **今回の試行:** `MediaImageFrameDecoder`にopt-in D3D11VA device、format選択、D3D11 frame download経路を実装し、benchに`--ffmpeg-hwaccel d3d11va`を追加した。`VideoLayer` direct decode requestからcontroller/direct decoderまでstderr traceし、D3D11VA device init=0、H.264 `AV_PIX_FMT_D3D11` frame receipt、CPU presentationへのdownloadを実証。GPU指定はdirect decoderだけへ渡し、通常video decoderやproxy controllerの不要なD3D device作成を避ける。ビルド成功。
- **確認できた事実:** 同一bench A/Bのraw decode profile medianはsoftware 22.37ms、D3D11VA 10.63ms。CPU image conversionは6.62ms／6.73ms。drop/late/repeatはsoftware 1 layer=4/1/13、2 layers=1/6/5、D3D11VA 1 layer=2/1/9、2 layers=2/13/4。D3D11VAはdecode時間をおよそ半減したが、厳格な無drop条件は未達で全counterの改善は混在。GPU framesはCPUへdownload後にfloat化し、rendererでさらにRGBA8 upload conversionする。
- **価値／懸念:** アプリのFFmpeg direct decodeでGPU accelerationは動作するがzero-copyではない。download、CPU conversion、renderer upload conversionが残る。GPU decodeの適用だけではproduction playback acceptanceを満たさない。
- **次に確認:** worker側のbounded async upload conversionを設計・実装し、複数回A/Bでdrop/late/repeat=0を狙う。別案としてD3D12VAとDiligentのresource/device共有も調査するが、異なるdeviceの受け渡し・同期を確認するまでdirect bindingしない。

### 2026-10-12 — Video upload scratch buffer再利用

- **関連:** `ArtifactCore/include/Image/SurfacePixelConversion.ixx`、`ArtifactCore/include/Image/ImageUploadConversion.ixx`、`Artifact/src/Render/PrimitiveRenderer2D.cppm`、`tools/VideoPreviewBench/README.md`。
- **確認できた事実:** 動画フレームのfloat→RGBA8同期変換は、描画スレッド上で毎回新しいvectorを生成していた。変換先bufferを書き換えるAPIを追加し、render thread-localの64 MiB上限slotを再利用するようにした。超過サイズはone-shot bufferにし、slotを拡大しない。Debug bench targetはビルド成功。H.264 1080p30 D3D11VAで2回測定し、1/2 layerのdropped/late/repeatedは2/1/6・0/13/3、および2/1/6・3/7/3。新frameの同期変換時間は約9–11msで、strict playback条件は未達。
- **未検証の仮説:** bounded allocation reuseによりheap contentionは減り得るが、今回のbenchはallocation回数を計測しておらず、end-to-endの明確な改善は確認できない。変換自体はまだ同期である。
- **価値／懸念:** 1080p/4Kで一時vectorの反復割当を避け、巨大フレームを長期保持しない。一方、ゼロドロップ改善にはデコード先読み結果を用いたworker側変換が必要で、準備完了状態、stale generation破棄、固定容量backpressureを一緒に設計する必要がある。
- **進捗／未検証:** decode workerでdecoded/prefetched frameを変換し、video layerごと3 frame / 64 MiBに制限するupload cache、別個のframe-ready判定、およびprepared uploadをrendererへ渡す経路を実装した。Diligent resource操作はrenderer owner threadに残した。動画Layer・Composition render controller・renderer・bench sourceのコンパイルは通過したが、ArtifactAppRuntime link時にlinkerが約10 GiBを使用し、システム空き物理memoryが1 GiB未満となったためbuildを中断した。新コードのruntime/A-B benchは未実施。
- **次に確認:** メモリ負荷を抑えてArtifactAppRuntimeをリンクできる構成を確認し、ビルド完了後にshared sourceの複数layer、seek/source-version切り替え、4Kのcache budget、drop/late/repeated=0を独立benchで検証する。

### 2026-10-12 — VideoLayer upload conversion buffer pool

- **関連:** `Artifact/src/Layer/ArtifactVideoLayer.cppm::Impl::cacheUploadFrame()`。
- **確認できた事実:** upload sidecarはlayerごと4 frame / 64 MiB以内で保持され、呼び出しごとに新規`ImageUploadBuffer`を確保していた。`convertImageForUploadInto()`は既存bufferへ容量を保って書き込める。`SharedPtr::useCount()`でrenderer等がまだ参照中のbufferを識別できる。
- **変更:** upload cacheから退避され、他の利用者が保持していないbufferを最大4個・64 MiBのbounded poolへ戻し、次のsidecar変換で再利用する。変換cacheもlayerごと最大64 MiB。Diligent texture操作は変更せずrender owner threadに残す。
- **価値／懸念:** sidecar入替時のheap/vector再確保を抑える狙い。これはバッファ再利用であり、CPU pixel conversion自体は依然decode worker上で同期実行されるため、decode throughputやno-drop acceptanceへの効果は未計測。
- **次に確認:** 対象moduleのコンパイル後、pool再利用回数・保持容量をprofileし、同一H.264 1/2 layerでD3D11VA/software両方を反復ベンチする。次段階の独立変換workerはbounded queue、generation cancel、layer destruction joinを揃えてから測定する。
- **追試結果:** 対象module単体コンパイルと手動差分linkは成功。H.264 1080p30のsingle-run D3D11VAは1 layer=2 drop/1 late/11 repeated/30 final、2 layers=0/7/1/13。softwareは1 layer=3/1/13/28、2 layers=1/8/2/11。各約3秒でstrict gate不合格。従ってpool再利用・GPU decodeともend-to-end改善は証明されず、preview no-dropは未達。

### 2026-10-12 — Video upload conversion bounded worker trial

- **関連:** `Artifact/src/Layer/ArtifactVideoLayer.cppm::Impl::enqueueUploadFrame()`、`cacheUploadFrame()`、`cancelPendingUploadConversions()`。
- **変更:** decode workerからsidecar conversionを外し、専用single-thread `QThreadPool`へ送る。layerごと最大4 requests、current frameをprefetchより優先、世代不一致結果を破棄。source/destruction cancelはworker join後に戻る。decoder prefetchを3から1へ減らす実験も含む。
- **確認できた事実:** 対象C++ module compile、bench incremental link、standalone process実行は成功。H.264 1080p30のD3D11VA 1/2 layerは2/1/5/12、1/4/0/6、softwareは2/2/5/13、1/1/2/5 (drop/late/repeated/final frame)。ProRes 422 HQ softwareは5/2/7/14、0/5/0/6。全て3秒測定でstrict gate不合格。D3D11VA device init/frame receiveは続けて確認した。
- **価値／懸念:** 変換とGPU texture操作のthread ownershipを分けたが、sidecar未準備frameがrenderに到達する場合があり、fallback samplesは1x1 placeholderだった。decode進行も5–14 frame程度に止まり、single-thread化・prefetch縮小・GPU decodeのどれも安定したend-to-end改善を示していない。結果のばらつきとearly-frame seek/startup costも残る。
- **次に確認:** playback tick、render CPU duration、decode request/coalescing、upload readinessを同一timestampで記録し、実際に支配するstallを特定する。必要ならGPU texture conversionまたはFFmpeg/Diligent間の共有可能なresource経路を別途評価するが、backend/device synchronizationを確認するまでDiligent resourceをdecode workerから触らない。

### 2026-10-12 — Video preview shared RGBA32F copy stall

- **2026-10-13追記（先読み）:** `decodeCurrentFrame()` の先読み分岐には値cacheへのcloneが残っていた。upload向け共有payload確定後に `putShared()` を使う最小変更を実施。frame／byte上限は同じ。通常要求側と同じ所有権を再利用するが、変更後の固有H.264 1layer10秒は提示147、late108で、性能改善は確認できず。変更前は提示145、late106。

- **関連:** `Artifact/src/Layer/ArtifactVideoLayer.cppm::Impl::prepareDecodeRequest()`、`FrameCache`、`ArtifactCompositionRenderController`、`tools/VideoPreviewBench/README.md`。
- **確認できた事実:** 1080p RGBA32F shared-frame lookupは約0.03msだが、`currentFrameBuffer_`へ複製する処理は約11.2ms、直後の`FrameCache::put()`で再度複製する処理は約10.7–11.2msだった。composition simulation eval内で周期的に合計約22–23msを占め、decode/upload準備待ちの再評価でも大きいコピーが起きていた。
- **変更・検証:** shared frameを`FrameCache::putShared()`で再利用し、upload sidecar準備済み・入力色変換なしならCPU frame bufferへのコピーを遅延する。CPU getterはshared frameをmaterializeできる。upload sidecar未準備時に同じdecoded frameを再要求する経路もskipし、upload readiness確認より前にRAM cacheをdeep copyしないよう順序を変更した。手動module compile/link後の独立benchでD3D11VA H.264の最大component simulation時間は約22–23msから6.5msに低下。
- **価値／懸念:** 大きなUI-thread stallの一つを約15ms以上削ったが、1/2 layerともstrict gate不合格。H.264の平均render frameは約31.6–46.0msで、decode新規frame 9–15msとCPU pixel conversion 6.5–7.3msも残る。uploadが間に合わず1x1 placeholderを使ったrender sampleがあり、GPU timestampも未取得。
- **FFmpeg GPU decode確認:** H.264 D3D11VAはdevice initと`AV_PIX_FMT_D3D11` frame receiptを確認したが、現presentation pathはCPU downloadを行う。ProResではD3D11VA device initは通るがhardware frame receiptなし。どちらも無drop/late/repeatの acceptance は満たさない。
- **未検証の仮説／次に確認:** GPU upload sidecarが遅れた時に現在の1x1 upload fallbackがなぜ選択されるか、GPU timestampまたはCPU present-wait計測で`Layer Surface Pass` / presentの実行時間を分離する。FFmpeg D3D11VAからDiligent D3D12へのshared-resource連携はdevice違いと同期方式の確認が必要で、未検証のまま結合しない。

### 2026-10-12 — Render-loop project diagnostics caused periodic preview stalls

- **関連:** `Artifact/src/Widgets/Render/ArtifactCompositionRenderController.cppm`、`ArtifactProjectService::currentProjectDiagnostics()`、`tools/VideoPreviewBench/README.md`。
- **確認できた事実:** Composition render loopから毎秒（composition変更時も）`currentProjectDiagnostics()`を呼び、project health / app / audio診断を集約していた。結果は advisory で、エラーでも描画は続行し、render path summaryとwarningにだけ使われていた。profileした呼び出しは12.8–100.7msで、周期的な`preSceneMs` spikeと対応した。
- **変更・検証:** 診断呼び出しと未使用キャッシュをrender loopから削除し、診断dashboard/APIは維持した。render controller module compileとbench relinkは成功。修正後のH.264 1080p30 / D3D11VA / 2 layersでは`preSceneMs`約0.00–0.01msとなり、周期的なproject diagnostic stallは消えた。
- **価値／懸念:** UI/render threadを最大約100ms止めていたadvisory処理を除去できた。一方3.259秒の独立benchはdrop=2、late=2、repeat=3、final frame=7/89でstrict gate不合格。render frame平均は約22–34ms、個別76–81ms、GPU timingは引き続き未取得。非同期化やpool増量は主問題の解決に足りず、simulation/render/presentとdecode/upload pacingの同時計測が次の切り分け対象。
- **次に確認:** component simulationの`goToFrame`内、Layer Surface Pass、present待ち、decode request/ready時刻を同一frame IDで揃えてprofileし、最大76–81ms sampleの寄与を分解する。diagnostic workerへの移動はproject snapshot/thread safetyが確認できるまで行わない。

### 2026-10-12 — FFmpeg GPU decode A/B and CPU download boundary

- **関連:** `ArtifactCore/src/Media/MediaImageFrameDecoder.cppm`、`ArtifactCore/src/Media/MediaPlaybackController.cppm`、`Artifact/src/Layer/ArtifactVideoLayer.cppm`、`tools/VideoPreviewBench/README.md`。
- **確認できた事実:** H.264 1080p30独立Composition benchで、software / D3D11VA 1 layerはdrop/late/repeat=0/1/6・1/0/6、2 layersは0/4/1・0/5/0。ProRes HQは1 layer=3/2/10・3/2/9、2 layers=1/2/3・0/1/5。すべて3秒前後のrunでstrict gate不合格。H.264 D3D11VAはdevice initと1920x1080 hardware frame受信を確認したが、`MediaImageFrameDecoder`がCPU frameへdownloadしておりzero-copyではない。ProResはdevice initのみでhardware frame受信なし。
- **FFmpeg CLI:** 90-frame H.264 warm run medianはsoftware=231.3ms、D3D11VA surface=348.4ms、CUDA surface=282.6ms。これは当該clipにおいてGPU decodeがCLI decode wall-timeも短縮しない結果。正しい`-hwaccel_output_format d3d11`でProRes 2 framesをdecodeした時はsoftware pixel formatを出力し、app pathでもhardware frameを受け取らなかったためsoftware fallbackと判断。ただしdecoderがhardware pathを選ばない具体理由は未特定。過去runと値の変動があり、恒常的な速度比は未確定。
- **価値／懸念:** GPU pathが実アプリで動作するcodecと、CPUへdownloadする境界を確認できた。GPU decode有効化だけではpreview gateに近づく安定した改善を実証できていない。共有payload cache hit時はdirect decodeを避けるため、layer count間のGPU/CPU A/Bには経路混在があり得る。
- **次に確認:** D3D11 decode surfaceのpixel formatとDiligent D3D12/Vulkan texture upload契約を調べ、CPU download＋float conversionを避ける共有可能なresource/interoperability経路があるか小さく実証する。併せて同一timestampでdecode ready、upload ready、render submit、presentを結ぶtraceを追加し、strict gateが失敗する主因をframe単位で特定する。

### 2026-10-12 — Vulkan FFmpeg decoder probe

- **関連:** `ArtifactCore/src/Media/MediaImageFrameDecoder.cppm`、`ArtifactCore/src/Media/MediaPlaybackController.cppm`、`tools/VideoPreviewBench/main.cpp`、`tools/VideoPreviewBench/README.md`。
- **変更:** standalone benchに`--ffmpeg-hwaccel vulkan`を追加し、FFmpeg Vulkan hwdeviceを作成するdecoder pathを試験可能にした。hardware AVVkFrameのdirect presentationは既存のsemaphore同期制約により有効化していない。
- **確認できた事実:** Artifact同梱FFmpegのvcpkg Debug/Release configは`CONFIG_VULKAN=0`、`CONFIG_H264_VULKAN_HWACCEL=0`、一方`CONFIG_D3D11VA=1`、`CONFIG_D3D12VA=1`。このため`av_hwdevice_ctx_alloc(VULKAN)`が利用不可となり、`av_hwdevice_ctx_create`は`AVERROR(ENOMEM)`を返していた。Vulkan runtime列挙による事前判定を追加した。1 layer=3/1/6、2 layers=0/3/1 drop/late/repeatの旧runはsoftware fallbackでありVulkan性能結果ではない。別インストールのFFmpeg CLIはH.264のVulkan output (`pix_fmt=vulkan`)で90/90 framesを5回decodeし、median 460.1ms。software CLI medianは265.5ms。同梱と外部CLIは別build/runtimeである。
- **価値／懸念:** FFmpeg Vulkan decode capabilityは別CLI build側にあるが、Artifact同梱packageにはVulkan backendが組み込まれていない。外部CLIのGPU surface出力もsoftware比で遅く、app playback改善の根拠にはならない。
- **次に確認:** package featureを変更する場合はFFmpeg Vulkan/H.264 Vulkan hwaccelを有効にした専用buildを用意し、Diligentの同一device/queueをFFmpegへ渡す経路とFFmpeg timeline semaphoreのwait/signal ownershipを解決してbenchで検証する。

### 2026-10-12 — FFmpeg D3D12VA decode is operational but does not improve preview

- **関連:** `ArtifactCore/src/Media/MediaImageFrameDecoder.cppm`、`ArtifactCore/src/Media/MediaPlaybackController.cppm`、`tools/VideoPreviewBench/main.cpp`、`libs/DiligentEngine/DiligentCore/Graphics/GraphicsEngineD3D12/interface/RenderDeviceD3D12.h`。
- **変更:** standalone benchからD3D12VAを要求できるようdecoder初期化、pixel-format選択、およびD3D12 hardware frame downloadを追加した。
- **確認できた事実:** bundled FFmpeg configで`CONFIG_D3D12VA=1`、`CONFIG_H264_D3D12VA_HWACCEL=1`。app direct pathはD3D12VA device init、decoder attach、1920x1080 `AV_PIX_FMT_D3D12` framesを受信し、CPU download後にpreview bufferを作った。同一build no-RAM-cache runのsoftware/D3D11VA/D3D12VA結果は1 layer drop/late/repeat=2/3/7・0/1/7・2/2/6、2 layers=1/3/2・0/3/2・0/4/1。render averageは1 layer=37.8/41.5/39.8ms、2 layers=24.6/37.2/42.5ms。全run strict gate不合格。External FFmpeg CLIの90-frame medianはsoftware 229.7ms、D3D12VA 432.3ms。
- **価値／懸念:** D3D12VAは実アプリで使えるが、今回のCPU-download経路はsoftware比較より遅く、安定したpreview改善を示さない。`AVD3D12VAFrame`には`ID3D12Resource`とfence/valueがありnative wrapper候補に見えるが、今回のFFmpeg decoderは独自deviceを作るためDiligent device resourceとして直接使える証拠はない。
- **次に確認:** Diligentの`CreateTextureFromD3DResource`要件、rendererから取得するdevice/queue、FFmpeg D3D12VA shared-device context、D3D12 video fence wait/signal順序を突き合わせる。同一deviceを安全に使える構成が確認できたら、CPU download無しのbounded experimentを作り、GPU resource lifetimeを保ったまま独立benchで比較する。

### 2026-10-12 — Video upload conversion poolのworker数を増やしてもstrict previewは改善しない

- **関連:** `Artifact/src/Layer/ArtifactVideoLayer.cppm::videoUploadConversionThreadPool()`、`tools/VideoPreviewBench/README.md`。
- **確認できた事実:** H.264 1080p30の独立benchで、1 worker版と2 worker版をsoftware / D3D12VA、1 / 2 layerで各1 run比較した。2 worker版のdrop/late/repeatはsoftware 1/2 layer=1/2/8・0/7/2、D3D12VA=3/2/5・0/5/1で全case strict不合格。平均描画時間も2 layerで約35.9ms / 51.3msだった。worker上限は1へ戻した。
- **未検証の仮説:** 専用poolのタスクがレイヤー単位の連続queue drainになっているため、workerを増やしてもdecode、renderer submit、presentとのCPU競合が勝る可能性がある。今回単発runだけでは原因を確定できない。
- **価値／懸念:** worker数とpool容量を増やすだけでは、遅延frameや再生range未到達を解消できないと分かった。非同期化の成否は「worker実行中」ではなく、対象frameがdeadline前にupload-readyとなった比率で測る必要がある。
- **次に確認:** decode request・decode ready・upload enqueue/start/ready・render queued・presentを同じframe IDとmonotonic timestampで記録し、queue待ち、CPU処理、present同期のどこで約数百msの遅れが生じるか切り分ける。

### 2026-10-12 — D3D12VA readback timing is measurable but not the whole stall

- **関連:** `ArtifactCore/src/Media/MediaImageFrameDecoder.cppm::downloadHwFrameToCpuVideoFrame()`、`tools/VideoPreviewBench/README.md`。
- **確認できた事実:** 1080p30 H.264 / 1 layer standalone runで、D3D12VAのwarm `av_hwframe_transfer_data()` medianは約1.41ms（最初は8.76ms）、その後のCPU RGB変換medianは約5.11ms。renderer RGBA upload conversion profileは多くが1ms未満、texture updateは0–2ms。D3D12 frame descriptorからresource pointerとfence valueも読めた。
- **価値／懸念:** GPU→CPU readbackは短く無視できないが、再生遅延を単独で説明しない。decode worker全体やsoftware decodeも高い/ばらつきがあり、短い1回ずつの比較ではbackend優位を断定できない。現アプリ経路はGPU→CPU→CPU色変換→GPU uploadで、GPU間直接転送は未計測。
- **次に確認:** requested/presented frame IDをキーに、packet/decode完了、hardware readback、CPU conversion、upload-ready、render queue、presentを同一monotonic clockで測る。別個にFFmpeg deviceとDiligent deviceのidentityを比較し、同じ場合のみD3D12 resource state・decode fence・frame lifetimeを含むdirect GPU pathの小さな実現性probeを設計する。

### 2026-10-12 — FFmpeg can decode on the Composition renderer's D3D12 device

- **関連:** `tools/VideoPreviewBench/main.cpp::runSharedD3D12DecodeProbe()`、`DiligentDeviceManager::acquireSharedRenderDeviceForCurrentBackend()`、FFmpeg `AVD3D12VADeviceContext`。
- **確認できた事実:** standalone benchでComposition rendererのnative `ID3D12Device`をFFmpegのD3D12VA contextへ設定し、H.264の1920×1080 `AV_PIX_FMT_D3D12` frameをdecodeできた。frame textureの`GetDevice()` pointerはrenderer deviceと完全一致 (`sameDevice=1`)。native resource formatはNV12。plane SRV compute shaderでRGBA8へ変換、出力をDiligentでwrapできた。固定3枠ringで各slotのoutput/descriptors/allocator/listを再利用し、source `AVFrame` cloneをcompletion fence retireまで保持する。さらにDiligent immediate queue (`LockCommandQueue`) の `WaitFence` / `Submit` / `EnqueueSignal` を使い、source wait・conversion command list・完了signalを同じDiligent queue上で順序づけた。独立runを3回行い、毎回8/8 submit・complete、PTS単調、sample pixel検証成功、`maxInFlightSlots=3`。elapsedは143–199ms。1 runのframe 3 slot waitは1.36ms、frame 4–7は0.005–0.011ms、enqueue wallは2.69–3.65ms。初回setup/sample validationは約80ms。FFmpeg decoder fenceはsubmit時にすでにcompleteだった。
- **価値／懸念:** device跨ぎ共有のためのexternal heap/handle copyは不要で、Diligent所有queueへのbounded async GPU conversion submissionが成立することを確認した。ただしprobeは固定ringの変換だけで、Diligent state trackingを伴う実draw、decodeとGPU workの同時進行、vsync-paced再生、drop/late/repeatは未検証。コード上、`GPUTextureCacheManager::acquireOrCreate(GpuVideoFrame)` は現在Vulkan image以外を拒否し、D3D12 NV12はRGBA sprite pathに入らない。
- **次に確認:** D3D12 completion fenceを非blockingにpollし、slot枯渇時はdefer可能にする bounded conversion cacheを、既存draw packetのowned queue/state境界に接続する設計を確定する。resource state ownershipとcache invalidationを明示し、その後standalone app playbackで1/2+ layer、H.264/ProRes、cache on/offのstrict frame gateを測り、ゼロdrop/late/repeatを確認する。

### 2026-10-12 — Production media controller shared D3D12 device hook

- **関連:** `ArtifactCore/include/Media/MediaPlaybackController.ixx`、`ArtifactCore/src/Media/MediaPlaybackController.cppm`、`ArtifactCore/src/Media/MediaImageFrameDecoder.cppm`、`tools/VideoPreviewBench/main.cpp`。
- **変更:** `MediaPlaybackController`にnative D3D12 device setterを追加し、COM参照を保持してdirect FFmpeg D3D12VA decoderへ渡す経路を追加した。GPU frame保持は `getVideoFrameAtFrameDirectRaw(frame, true)` の呼び出し単位で指定し、従来の1引数APIはCPU readback動作のまま維持した。
- **確認できた事実:** 変更した `MediaImageFrameDecoder.ixx`、`MediaPlaybackController.ixx`、両実装module、bench mainを手動コンパイル・再リンクし、standalone probeを実行した。H.264 1920x1080のcontroller frameは `storage=3` (D3D12Texture) でrenderer deviceと一致 (`sameDevice=1`)。続く固定3枠 ringのprobeは8/8 frameを変換・完了し、PTS monotonic、sample pixel検証成功、elapsed 132msだった。1回のcapability probe結果であり、安定した性能測定ではない。`NOMINMAX` でD3D12 Windows macroと `std::max/min` の衝突を除去した。
- **未検証:** ArtifactVideoLayerからshared deviceを供給する製品経路、D3D12 NV12を `GPUTextureCacheManager` とsprite drawへ渡す経路、実draw、vsync playback、再生性能、strict frame gateは未確認。通常の既存raw decoder APIは引き続きCPU readbackする。
- **価値／懸念:** controller境界でshared-device hardware frameのnative identityとlifetime保持を実証した。COM lifetimeはcontroller保持中とdecoder hwdeviceの保持中の両方で必要。次にlayer/render-device供給境界をつなぎ、Diligent state ownershipとNV12 conversion cacheを経て製品drawで検証する。

### 2026-10-12 — D3D12 video conversion slot needs post-draw retirement

- **関連:** `Artifact/src/Render/GPUTextureCacheManager.cppm`、`Artifact/src/Widgets/Render/ArtifactCompositionRenderController.cppm::presentAndUpdateVideoLayers()`、`tools/VideoPreviewBench/main.cpp::enqueueNv12ComputeConversionProbe()`。
- **確認できた事実:** 現在のGPUTextureCacheManagerは`GpuVideoFrame`入力でVulkan imageだけを受け付ける。standalone probeはDiligent immediate queueにNV12→RGBA computeを投入できるが、probeにはsprite drawがない。Composition rendererはdraw packetをsubmitした後にPresentし、Diligent contextの`FinishFrame()`を呼ぶ。
- **未検証の設計仮説:** conversion fence完了は出力textureが後続sprite drawから解放された証拠にならない。同じ固定出力slotを再利用する場合、当該slotを読んだ最後のdrawより後にDiligent queueへretire signalを出し、そのsignalが完了するまで再利用をdeferする必要がある。Present後signalとslot数でbounded poolを保つ案が有力だが、render graph複数経路とcache/device resetのretirement契約は未検証。
- **価値／懸念:** conversion-only fenceだけでoutput ringを実装すると、描画がまだ読んでいるtextureを次のframeで上書きする競合が起こり得る。最終draw後のretirementとpool枯渇時のdeferを先に設計し、同期waitやone-shot resource生成へ逃がさない。
- **次に確認:** 全composition present経路でcache end-frame signalを置ける位置を特定し、Diligent immediate queueのsignalがPresent後に順序化されること、frame slotの同一submission内でconversion→draw→retireの順になること、device loss時のsource AVFrame lifetimeを確認する。差分の後は1/2層でconverter slot busy率とfence retire latencyを計測する。

### 2026-10-12 — Product D3D12 video path requires slot-key invalidation and shutdown retirement

- **関連:** `Artifact/src/Render/GPUTextureCacheManager.cppm::D3D12VideoFrameConversionPool`、`Artifact/src/Layer/ArtifactVideoLayer.cppm::decodeFrameToGpuFrame()`。
- **確認できた事実:** 実装接続中、再利用可能slotを上書きする前にそのslotを指すcache entryを外さないと、古いframe keyが更新後の内容を返す。slot選択時に既存entry IDを消してから変換する経路を追加した。またCompositionの2つのPresent経路からdraw後retire signalを要求し、pool teardownではimmediate contextの`WaitForIdle()`を呼んでからslot resourceと保持AVFrameを解放するようにした。
- **未検証の懸念:** teardownでの`WaitForIdle()`は低頻度のdevice/cache破棄時だけだが、デバイスロスト時の挙動と描画stateへの影響は未確認。現在の接続差分はまだ対象moduleのコンパイル・実描画で検証できていない。
- **価値:** fixed ringのcache identityとGPU lifetimeは別々に守る必要がある。slotの上書き前にcache keyを無効化し、destructionもqueue completionを考慮する。
- **次に確認:** `setDevice` / `clearDevice` / device loss時にboundedなretirementができるか調べ、targeted compile後、H.264 / ProRes、1 / 2 video layerでslot再利用とdrop/late/repeatを計測する。strict条件はそれら3値がすべて0であること。

### 2026-10-12 — Standalone preview debugger: clean exceptions, unrelated runtime warnings

- **関連:** `tools/VideoPreviewBench`、`Artifact/src/Composition/ArtifactAbstractComposition.cppm::replayGranularMaterialToFrame()`、`Artifact/src/Project/ArtifactRevisionService.cppm::loadLedger()`。
- **確認できた事実:** 最新のcanonical `ArtifactVideoPreviewBench.exe`をネイティブデバッガ下でH.264 720p、1/2層、D3D12 renderer + D3D12VAで起動し、source replacement・playback・window/process teardownまで実行。first-chance exceptionイベント0、Debug CRT/heap/assertion出力0、D3D12 ERROR/CORRUPTION出力0、exit codeは厳格preview gateの失敗を示す1だった。一方、Compositionの空またはgranular world未登録状態でMPM baseline reset失敗warningが25回、ArtifactVCSの`ledger.jsonl`を`WriteOnly device`としてreadしたというQt warningが2回観測された。これらは今回の捕捉ではC++例外やD3D12エラーではない。
- **未検証の仮説:** MPM warningはgranular materialが存在しないCompositionでもreplay resetを試みている可能性がある。QFile warningはVCS ledgerのread/write lifecycle上の別経路の可能性があり、現ログだけでは呼び出し元を特定できない。
- **価値／懸念:** 例外再現の前提はこのstandaloneシナリオでは解消したと確認できるが、診断ログ上の2種類のruntime warningは検証ログをノイズ化する。Product app全体・ProRes・RAM cache-on・長時間実行の例外不在までは証明していない。
- **次に確認:** 必要に応じてQt message handler / call stackを使い`ledger.jsonl` read warningの呼び出し元を特定し、granular worldの登録有無を調べてreset警告が無操作のCompositionにも妥当か確認する。動画のstrict frame gateは例外とは独立の未解決課題として続ける。

- **追記 2026-10-12:** `ArtifactRevisionService::appendRevisions()`がledgerを`WriteOnly | Append`で開いた後、末尾確認に`read(1)`を呼んでいたことを特定。`ReadWrite | Append`へ変更し、ArtifactRevisionServiceとComposition実装をtargeted compile、standalone benchを再リンクして再実行。ネイティブデバッガの同一1/2層シナリオで`WriteOnly device`警告は2件から0件、granular solver reset失敗warningは25件から0件になった。first-chance exception、Debug CRT/heap/assertion、D3D12 ERROR/CORRUPTIONも引き続き0件。厳格preview gateは依然失敗し、例外解消とは別に扱う。

### 2026-10-12 — 動画準備完了通知のキャッシュ無効化によるGUI停滞
- 関連: `Artifact/src/Layer/ArtifactVideoLayer.cppm`, `Artifact/src/Service/ArtifactPlaybackService.cppm`, `Artifact/src/Playback/ArtifactPlaybackEngine.cppm`, `tools/VideoPreviewBench/main.cpp`。
- 確認済み: 独立H.264/D3D12VA/720pテストで、動画準備完了のLayerChanged通知が約280〜340ms。既存の複数レンダリング所有者から1,800フレーム範囲のディスク削除が繰り返され、各範囲約70〜100ms。GUIへのblocking dispatchが300〜530ms待ち、定常audio供給は約0.02〜0.03ms、診断サンプリングは約1msだった。
- 修正: 保存画像のない再生tickはディスク名前空間/シリアライズを計算しない。範囲無効化は名前空間を一度解決し、空のディレクトリでフレームごとの削除を省略する。存在する未hydrationディスクキャッシュの削除は維持。計測では通知が多くの場合約4〜16msへ低下。診断は既存PROFILE_SETUPフラグ、固定回数、追加flushなし。
- 懸念/未解決: 実行時フレーム準備完了と作者によるレイヤー編集が同じLayerChanged::Modifiedとして扱われるため、RAM構築キャンセルと全キャッシュ無効化は残る。既存イベント責務を整理し、停止中の非同期完成の再描画を保ったまま、編集・ソース変更のみが永続プレビューを無効化することを確認する必要がある。現段階では新たなイベント型/公開signalやslotは追加していない。
- 検証: 修正後のネイティブデバッガはfirst-chance実行時例外0、終了コード1（厳格性能条件の不合格）。D3D12 live-object警告は別途残る。全アプリ/ProRes/長時間安定性は未確認。
- 最終の非デバッガ/非profile測定（5秒、720p H.264、1/2レイヤー）: 1レイヤーは74フレーム進行、dropped=2/late=39/repeated=0。2レイヤーは58フレーム進行、dropped=0/late=46/repeated=0。3秒profileの進行改善だけを性能合格の証拠とはしない。ゼロコマ落ち・リアルタイム条件は未達。

### 2026-10-12 — 動画FrameReadyを既存通知経路で編集変更から区別
- 関連: `Artifact/include/Event/ArtifactEventTypes.ixx`, `Artifact/src/Layer/ArtifactVideoLayer.cppm`, `Artifact/src/Service/ArtifactPlaybackService.cppm`, `Artifact/src/Widgets/Render/ArtifactCompositionRenderController.cppm` と既存LayerChanged購読者。
- 実装: 既存LayerChangedのChangeTypeへFrameReadyを追加（フィールド・レイアウト変更なし）。デコード/アップロード完了だけが送信する。レンダラーは該当レイヤーの一時サーフェスを無効化し、既存render tickを起こす。RAM構築・永続キャッシュの無効化は行わない。他の編集購読者はFrameReadyを無視し、自動保存/編集hook/階層再構築/編集UI更新を抑制する。ソース読み込みと遅れて判明したソース寸法変更はModifiedのまま。
- 境界: 新規公開signal/slot、バス購読、Transport状態所有者は追加していない。新規コンテナやフレームごとの確保は追加していない。既存の一時サーフェス無効化・再描画経路を再利用する。
- 要確認: 停止中の非同期完成、RAM構築完了、キャッシュなし/あり再生、例外/終了時安全性を独立benchで確認する。全アプリの編集UIおよびソフト/別backendの実行確認は別範囲。
- 検証結果: event interfaceと既存全購読者・動画producer・render controllerを対象に23ファイルを直接コンパイルし、独立benchをリンク/配置。非debugger・非profileの720p H.264/D3D12VA/5秒: 1レイヤーfinal=80/dropped=3/late=45/repeated=0、2レイヤーfinal=56/dropped=0/late=51/repeated=0。性能条件は不合格。前回比較で速度向上は断定しない。Native debuggerの1/2レイヤー再生・source replacement・終了チェックはexceptions=0、D3D12 ERROR/CORRUPTION=0、終了code=1（性能判定）。
- RAM構築結果: 40秒待ってready=0/requested=89。確認済みのコードではrequestRamPreviewBuildがキューを作り、レンダラーが現在フレームとbuildQueueNextFrameが一致するときだけcaptureする。nextRamPreviewBuildFrameの呼び出しはservice内部・診断だけで、停止中に構築対象へ進める実行経路を検索で確認できなかった。未検証: 全アプリの別経路が存在する可能性と、frame0のcaptureが発生しない他の条件。これはFrameReady種別修正とは別の構築ドライバー課題として調査を続ける。

### 2026-10-12 — RAM構築を止めていた通知の順序と初回描画条件の確定
- 関連: `Artifact/src/Service/ArtifactPlaybackService.cppm`, `tools/VideoPreviewBench/main.cpp`, `Artifact/src/Layer/ArtifactVideoLayer.cppm`。
- 確認済み: FrameReady区別後もready=0/90。PROFILE_SETUPでqueueReason=layer-changed、active=0を確認。GUI上の編集イベントでも無効化をさらにqueued hopしていたため、イベント後に開始したbuildが消され得た。GUI同一threadはinline、他threadはqueuedかつComposition再確認へ修正。
- 確認済み: この修正後ready=1/90、queueReason=preview-render-contract-changed。初回readbackでrender contractを確定すると残りbuildも消されていた。古いcache/世代を無効化しつつ、activeだったrange/reasonは新contractで再要求する。次の検証で52フレームまで進行。
- 確認済み: 構築用seekのnear-playhead prewarmが前方へ進むほど既構築frameをevictし、90枚が揃わなかった。独立benchは構築range以上の既存prewarm radiusを設定し、既存PlaybackService API経由でqueue先頭を表示・render tickへ再試行を依頼する。画像合成・Transportの別実装は追加しない。headをvideo/readback待ちで回す既存deferは、非同期完成とcapture対象がずれるため、この2理由のみheadを保持する。
- 検証済みの到達点: 720p H.264/1 layerでRAM=90/90、3秒でfinalFrame=91。しかしstrict drop/late条件は不合格。2 layersはまだ0/90。測定範囲と準備phaseの分離前の値であり、性能合格として扱わない。
- 測定修正: FrameRange終端はexclusive。benchはframeCountをendへ渡し、Composition durationも測定秒数に合わせる。case間はstop/wait、seek0/pump後にlayerを置換。準備phaseのlateを測定へ混ぜないため、既存VideoLayer診断にresetFrameTimingDiagnosticsを追加し、測定前のみ呼ぶ。新規signal/slot・コンテナ・Transport所有者は追加していない。cold contract再構築は既存queue再確保を使う（毎frameではなく描画条件の変更時のみ）。

### 2026-10-12 — キャッシュ構築のケース再利用・終了所有権・表示確認の待ち合わせ
- 関連: tools/VideoPreviewBench/main.cpp、Artifact/src/Layer/ArtifactVideoLayer.cppm、Artifact/src/Playback/ArtifactPlaybackEngine.cppm、Artifact/src/Service/ArtifactPlaybackService.cppm。
- 確認済み: case間のviewport.stopがrenderer tickも停止していた。停止中構築前に既存controller.startを呼び、1/2レイヤーとも30/30および90/90を構築できた。Transportは引き続きPlaybackServiceが所有する。
- 診断訂正: shared-cache経路ではCPU bufferが1x1でもcurrentSharedFrameは正しい画像。debugStateを実効shared寸法に修正。画像cache破損は確認していない。一方setSourceSizeはgeometry revisionを進めず、同一frameのboundsが1x1に残るため動画metadata/source寸法更新時に既存Source dirtyを設定した。
- 終了ハング: native attachでmainがshared background pool destructorを待ち、decode workerがvideo upload poolのstart mutexを待つstackを確認。独立benchはpoolが生存しているうちにremoveAllLayersでdecode/uploadをjoinして終了する。修正後H.264/ProResの複数caseプロセスは正常終了。製品全体の終了順序は未検証。
- 表示抜け: composition同期だけではrender tick前に次frameが適用され得る。既存recordPresentedFrameをworkerへ返し、既存mutex/conditionで最大100msのbounded waitを追加。GUI/GPUを待たせず、pause/stopで解除し、observer不在なら既存dispatchへ戻す。signal/slot・購読・コンテナの追加なし。
- 非debugger結果: H.264 720p30/cacheあり3秒、1 layer final89/drop0/late0/repeat0、2 layers final54/drop0/late25/repeat0。ProRes422HQ1080p30/cacheあり1秒、1 layer final29/drop0/late0/repeat0、2 layers final19/drop0/late4/repeat0。短時間の1 layer合格であり、持続realtime/多layer条件は未達。
- 未検証の仮説: tryGetRamPreviewFrameImageの毎frame DeepCopyとCPU uploadが残る。cache hit=GPU zero-copyとは限らず、このコストの計測と所有権を保った再利用を次に検討する。新規標準コンテナは追加していない。

### 2026-10-12 — RAM合成画像の同期描画に限定したborrow
- 関連: Artifact/include/Service/ArtifactPlaybackService.ixx、Artifact/src/Service/ArtifactPlaybackService.cppm、Artifact/src/Widgets/Render/ArtifactCompositionRenderController.cppm。
- 確認済み: tryGetRamPreviewFrameImageはDeepCopyを返し、viewportは描画ごとに大容量CPU cloneを行っていた。新しいborrowRamPreviewFrameImageはowner thread限定でcache-owned const imageを返す。既存copy APIは他の呼び出し用に維持。rendererは取得からdrawまでcache変更/API呼び出し/event pumpを挟まず、PrimitiveRenderer2Dはその呼び出し中にCPU uploadを完了し、コマンドにはGPU textureだけを保持する。
- 境界: 返却pointerはcache変更まで、同期描画のみ。保存・background渡しは禁止。新規signal/slot、コンテナ、allocationは追加せず、既存一時Imageの1x1 allocationとDeepCopyを減らす。CPU upload自体は既存経路のまま。速度への寄与とnative例外は独立benchで検証中。
- 直前native結果: cacheあり3秒/H264のデバッガ実行はfault exceptions0、D3D12 ERROR/CORRUPTION0、正常exit1。ただしready71/90で35秒timeoutとなり1caseだけ。debugger検証は速度や2case切り替えの証拠には使わない。

- borrow変更後の非debugger結果: H.264720p30/cacheあり3秒、1 layer final89/drop0/late0/repeat0、2 layers final59/drop0/late18/repeat0。両case ready90/90、プロセス正常終了。前回final54/late25との一度の比較だけではspeedupを断定しない。全体条件は未達。temp/video-borrowed-preview-3s.json。

- borrow後native検証: 1/2 layers両caseのRAM30/30、再生/source replacement/終了までfault exceptions0、D3D12 ERROR/CORRUPTION0、CRT heap/assertion0、terminal exit1（性能gate）。temp/video-native-borrowed-debug.log。非fatal live-object/shader warningは残る。全アプリは起動・検証していない。
- bench判定修正: 250ms周期sample後の終盤で発生したlate/repeatを取りこぼさないよう、測定終了時も診断を取得してから合否を決める。

### 2026-10-12 — RAM再生のディスクprobeとFrameReadyのviewport状態初期化
- 関連: Artifact/src/Service/ArtifactPlaybackService.cppm、Artifact/src/Widgets/Render/ArtifactCompositionRenderController.cppm。
- 確認済み: valid RAM imageがあるtickでもhasPreviewFrameOnDiskを呼び、QImageReader.canReadとmanifest/file検証を同期実行していた。PROFILE_SETUPのpublication後半は10〜15ms、sync自体は約0.2ms。valid RAM時はwriter/eviction/invalidationが維持するdisk stateを保ち、取得が必要なdisk-load時の検証は残す。
- 結果: H264720p30/cacheあり2layers/3秒でfinal74/drop0/late28。以前のfinal59より進んだが単独runでspeedupを断定しない。
- 確認済み: FrameReadyはinvalidateBaseComposite(true)を呼び、lastRenderKeyState全体がゼロ化される。viewport変化判定がこのzoom/panを使うため、コンテンツ完成をviewport-transform-changingと判定しRAMfallbackを外し得る。FrameReadyの呼び出しだけ3個のfloatを保存復元する。base invalidation serial/dirty/render keyの無効化は維持。他の編集操作は変更しない。
- 結果: cached2layers/3秒final81/drop0/late0だが、90frameのrealtime進行条件は未達。後述の計測修正前の値であり、latenessの合格証拠にしない。

### 2026-10-12 — キャッシュ再生の表示時間診断を描画前から計測
- 関連: Artifact/src/Widgets/Render/ArtifactCompositionRenderController.cppm、Artifact/src/Layer/ArtifactVideoLayer.cppm。
- 確認済み: RAM表示のmarkFrameCompositionCacheReady/markFrameRenderQueuedがrenderer.present後に呼ばれ、request timestampまでresetしたためpresentedMsほぼ0になり、キャッシュ描画の遅延を隠していた。
- 修正: RAM選択確定後の描画前にcache ticket/queuedを設定し、同一frameのcomposition request timestampを維持。既存present後のmarkPresentedが実時間を記録する。RAM ticketを非表示のdecode completionで書き換えない。キャッシュがdiskにも保存されているという理由だけで実際のRAM表示をcomposition-diskと記録しない。
- 検証: H264720p30/cacheあり1秒、1layer final29/drop0/late0/repeat0、2layers final29/drop0/late6/repeat0。両case30/30、正常exit1。1layerの平均presentedMs20.19（最終19.98）、2layers24.23。以前のcached PASSは新計測で再確認が必要。gateは緩和していない。

### 2026-10-12 — デコード済みpayloadとupload表現の寿命を分けて再利用
- 関連: Artifact/src/Layer/ArtifactVideoLayer.cppm。
- 確認済み: upload cacheは4frames/64MiBのbounded保持。prepareDecodeRequestのshared-payload再利用がhasCachedUploadFrameを必須にしていたため、CPU payloadが有効でもupload evictionだけでdecode workerへ進んでいた。
- 修正: source version/dimensionsが合うshared CPU payloadは再利用し、不足uploadだけを既存priority upload conversion poolへenqueueする。cache容量は増やさない。新規signal/slot・container・pool・起動flagなし。既存workerのbounded4 requestを再利用し、再デコードとCPU image cloneを避ける。既存のbackground upload allocationは残る。live cache-miss/共有payload/source変更/終了と複数layerを再検証する。

- upload再利用後の検証: H264/cacheあり1秒の1,2,4 layersはfinal29/drop0/repeat0。1/4はlate0、2はlate1。続く3秒の2,4,8 layersは全case RAM90/90、final89/drop0/late0/repeat0、terminal exit0（temp/video-shared-upload-scale.json）。修正後timingを使うが短いsingle runで、持続安定性は未達。
- 測定範囲: bench Compositionは固定1280x720/30fps。1080p ProResは入力寸法でありnative1080p Compositionの性能とは異なる。現在同一入力の複数layerであり、独立した異なる動画streamの同時性能は未検証。
- 未検証の次の確認: PrimitiveRenderer2Dのsprite texture cacheは50 entries/512MiB。RAM frame cache128とentry数が違い、90-frameのGPU事前保持はentry限界で全体が残らない可能性がある。GPU warmupを実装するならentry/byte両budget・冷/温phaseの分離・正確なowner/version identityを先に確認する。現在のscopeを勝手に拡げてはいない。

- ProRes再検証: 1080p ProRes422HQ入力/720p Composition/cacheあり3秒/2,4,8 layers。全case90/90、final89/drop0/repeat0、lateは2/1/10。正常exit1。strict条件は未達。temp/video-prores-shared-upload-scale.json。
- 最新native再検証: 1/2 H264 layers両case30/30、再生・source replacement・終了。fault exceptions0、D3D12 ERROR/CORRUPTION0、CRT heap/assertion0、正常exit1（性能gate）。temp/video-native-cache-reuse-debug.log。ProRes/native1080p/長時間/異なるsource同時/他backendはまだ検証範囲外。

### 2026-10-12 — 再生時計と実際に表示した最終frameを分けるgate
- 関連: Artifact/include/Service/ArtifactPlaybackService.ixx、Artifact/src/Service/ArtifactPlaybackService.cppm、tools/VideoPreviewBench/main.cpp。
- 確認済み: benchのfinalFrameはengine currentFrameで、recordPresentedFrameの最後の実表示を読んでいなかった。特に測定終了直前は未表示targetが残り得る。
- 修正: serviceの実表示通知だけで更新するlastObservedPresentedFrameを追加。playで-1にresetし、仮のlastPresentedFrame seedとは区別する。benchはfinalPresentedFrameもframeCount-2以上を必須とする。新規signal/slot/container/フレームallocationなし。setterは既存renderer routeのみ。表示完了はCPU-side present成功通知で、GPU scanout時刻ではない。
- 未検証: 新gateで2/8 layersの3秒cached H264を再測定中。以前のrealtime PASSは新gateで再確認が必要。
### 2026-10-12 — 独立benchのCompositionとviewport解像度を明示
- 関連: tools/VideoPreviewBench/main.cpp。
- 修正予定の現状: composition-width/height CLIを追加し1..8192整数をvalidate、既定1280x720。Composition/viewport/windowを同じ要求サイズで作り、reportにComposition寸法/FPSと実viewport寸法を記録。入力1080pだけをnative1080pの証拠にしない。source編集済み、実行中bench終了後にbuild/runする。巨大frameのメモリ実行は別のbounded budget確認が必要で、上限入力validateだけで性能を保証しない。

- 実表示gate再検証: H264720p/cacheあり3秒/2,8 layers。両方transport89/actual89/RAM90/90/drop0/repeat0。2layers late1でfail、8layers late0でpass（temp/video-actual-presented-gate.json）。複数runで安定合格とはまだ言えない。
- native1080p実行: ProRes入力/Composition1920x1080/viewport実寸1920x1080/cacheあり3秒。1layer transport79/actual79/late65、2layers transport65/actual64/late58。両方90/90/drop0/repeat0、正常exit1（temp/video-native1080-prores.json）。720p結果を1080pの証拠に使わない。約4.5GiB working setを観測、長時間の保持/budget調査が必要。
- bench取得負荷: frameDebugSnapshot(false)でも診断resource/RenderGraph/stringをGUI threadで構築していた。通常は既存frameDebugCountersにし、PROFILE_SETUP時のみ詳細snapshot。合否用actual/video timingは維持。変更後1080p1layer/cacheあり1秒はtransport29/actual28/late21/RAM30/30/drop0/repeat0でfail。診断負荷だけでnative1080pの遅延は説明できない（temp/video-native1080-counters.json）。
- 次の計測: 既存PROFILE_UPLOADの384sample上限で準備後の再生まで捕捉し、bench境界measure markerで区切る。GPU/CPU間copy自体ではなくRGBA conversion/texture creationも既存timerで測る。profiling実行をrealtime acceptanceに使わない。明示flushを1つ削除。新規flag/pool/containerなし。

### 2026-10-12 — 1080pキャッシュ表示のCPU変換と新規texture生成の実測
- 関連: Artifact/src/Render/PrimitiveRenderer2D.cppm、tools/VideoPreviewBench/main.cpp。
- 計測事実: PROFILE_UPLOAD/1920x1080 Composition/ProRes/cacheあり1秒のmeasure marker後、27 upload samples。convertMs中央値18/max20、createTextureMs中央値7/max16、keyMs中央値0。cacheHit中央値0。再生区間にvideo decode samplesはなく、cached composite表示の変換・texture作成が約25msを占めている。temp/video-native1080-upload-profile.log。profiling runの遅延19はacceptanceには使わない。
- 実寸訂正: CPU upload imageは2400x1195。CompositionとQt widgetの論理寸法1920x1080だけをphysical cached image寸法と見なしてはいけない。benchへcachedImageWidth/Height、viewportDevicePixelRatioを追加。画像がcanvas-onlyかviewport背景も含むか、読出しcontractを次に確認する。
- 次の実装候補（未実装・未検証）: RAM entryへ準備済みRGBA8 upload representationを持たせ、cold build時に既存convertImageForUploadIntoで作る。表示時は既存textureForVideoFrameのowner/content-key付き再利用texture slotへ渡し、毎frame変換/texture生成を避ける。CPU imageと同じeviction/generation/source変更で無効化し、追加upload byte budgetを明示する必要がある。新しいstd mapは作らず既存entry value/独自containerを検討。単に全frameGPU常駐を無制限に増やさない。変換をGUI cold boundaryで行うか既存bounded workerへ送るか、終了/ownership/未完成表示の経路を先に読む。
- 修正しない推測: ImageF32x4RGBAWithCacheはDiligent GPU APIを持つがimage() constが可変参照を返してcpu-dirtyを設定し、float textureのcolor contractもsprite RGBA8経路と違う。これをそのままGPU借用して速度を断定しない。

### 2026-10-12 — RAM entryのbounded表示用RGBA8準備と既存texture slot再利用
- 関連: Artifact/include/Service/ArtifactPlaybackService.ixx、Artifact/src/Service/ArtifactPlaybackService.cppm、Artifact/src/Widgets/Render/ArtifactCompositionRenderController.cppm。
- 実装: 既存RAM mapのentry valueにCPU storage/既存ImageUploadBuffer/revisionをまとめる。停止中cache storeのみ既存convertImageForUploadIntoを実行し、毎frame live captureには追加変換しない。uploadは1frame64MiB、全体1GiB上限。容量超過ではuploadなしで既存画像表示へfallback。置換/evict/range無効化/clearでuploadも破棄しbyte計数を更新する。
- 表示: 同一owner thread/同期描画のborrowUpload API。controllerごとにcold生成UUID、entryごとの単調revisionで既存textureForVideoFrameへprepared uploadを渡し、既存reusable texture slotを使用する。色変換target/texture formatは従来のRGBA8 sRGB straightと同じ。準備済みGPU texture不在/失敗では従来sprite経路を使う。Transport、新規signal/slot/subscription、独自GPUqueueは追加しない。
- allocation例外: 停止中cache preparationでImageUploadBufferの既存std::vector byte storageを確保する。新規std containerは作らず既存RAM mapのvalueのみ変更。既存独自ImageUploadBuffer API/ABIを再利用するため、その既存std::vectorを置換しない。追加メモリ最大1GiB。ライブでは既存GPU texture update allocation/バックエンド内部確保の実測が必要。
- 現状: source編集/直接build中、runtime/color/eviction/source replacement/終了はまだ未検証。新方式の速度は未証明。目標を達成したとは扱わない。
- 直前native検証: actual-present gate/DPI reportまでの版本は1/2layers30/30でexceptions0、正常exit1。prepared upload変更は含まない。

- prepared upload初回検証: ProRes/cacheあり/1920x1080 Composition/viewport DPR1.25/実cache2400x1195/1秒。1layer transport29/actual29/drop0/late0/repeat0でpass。2layers transport29/actual29/drop0/late1/repeat0でfail。RAM各30/30、プロセス正常exit1。temp/video-prepared-upload-native1080.json。単発1秒で安定性は断定しないが、以前1layer late21だった条件で進展した。新GPU slotのnative例外検証中。

- prepared upload native検証: 1/2 H264/cache各30/30、replacement/playback/teardownをnative debuggerで確認。fault exceptions0、D3D12 ERROR/CORRUPTION0、CRT heap/assertion0、正常exit1。temp/video-native-prepared-upload-debug.log。GPU image visual parity、ProRes native debugging、追加byte budget exhaustion、他backendはまだ未確認。

- prepared upload 3秒検証: ProRes/1920x1080論理viewport/DPR1.25/cache実寸2400x1195/2,4layers各90/90。2layers actual89/drop0/late4/repeat0でfail、4layers actual89/drop0/late0/repeat0でpass、正常exit1。temp/video-prepared-upload-prores-scale.json。cold-caseと後続caseの差、disk writer負荷、dynamic GPU page warmupの寄与は未検証。workingset約8GiBを観測（PC約32GiB、空き約4GiB）。単発4layer passで完全達成とはしない。
- bench比較モード: --no-disk-cacheは既存setDiskPreviewCacheEnabledを起動設定時に使い、RAM policyを維持したままdisk persistenceの寄与を分ける。defaultはdisk有効のまま。reportにdiskCacheEnabledを記録。製品のdefault変更ではない。build/run確認中。

### 2026-10-12 — 表示済みcached frame再描画の診断二重計上を抑制
- 関連: Artifact/src/Layer/ArtifactVideoLayer.cppm。
- 確認済み: 同じtimeline frameのcache redrawもmarkFrameCompositionCacheReadyでticketを作り直し、同一request timestampを保ってmarkPresentedするため、すでに正常表示済みのフレームが後の再描画でlateとして追加され得た。新しいtransport frameへの表示と無関係なredrawを区別する必要がある。
- 修正: FrameTicketにpresentationRecorded boolを追加。最初のpresent成功/lateの結果を記録した後は、同一ticketのqueue/present/cacheReadyで結果を作り直さない。beginFrameTicket/resetが新timeline frameを評価するため、実際の初回遅延・repeatedLastGood・actual presented-range gateは維持する。フレームごとの確保・公開signal/slotは追加しない。現在のmatrix実行には未反映、終了後にbuild/retestする。
- 確認すること: 並行decodeのbeginFrameTicketで同一timeline ticketが書き換わる経路、source/seek/loop変更時のreset、およびrecorded outcome count。短時間PASSを目標達成へ読み替えない。

- 二重計上の追加証拠: prepared upload/RAM-only/ProRes1080p論理3秒/1..8matrixは全actual89/drop0/repeat0。1,7layersのみlate0でpass、他late10/6/2/3/2/3でfail。1layer ticket window89だが複数layer全てwindow120（上限）となり90-frame範囲より多いoutcomeを確認。temp/video-prepared-upload-prores-ram-matrix.json。二重計上が全lateの原因だとはまだ断定せず、修正後first-present遅延とcountを再測定する。

- ticket bool単独の再検証: ProRes/1080p論理/RAM-only3秒/2,8layersはactual89/late0/drop0/repeat0でpassしたがwindow120/117と二重件数が残った（temp/video-unique-presentation-prores.json）。prepareDecodeRequestのbeginFrameTicketが同一frameのticketを作り直す経路を踏まえ、ImplでlastRecordedOutcomeFrame/Generationを保持し、queue/presentでも連続する同一frame+generationを拒否する。reset診断でこのledgerもclearする。全lateを隠す変更ではなくfirst present結果を維持する。現在build済み・再測定中。
- ticket bool版本ProRes native検証: 1920x1080論理/1,2layers各30/30/RAM-only/終了までexceptions0、D3D12 ERROR/CORRUPTION0、CRT heap/assertion0、正常exit0。temp/video-native-unique-prores-debug.log。ledger追加はそのrunに含まない。

- ledger修正後ProRes1080p論理/DPR1.25/RAM-only3秒/2,8layers: actual89/drop0/late0/repeat0、両first-video window89へ整合。avgPresentedMs16.60/18.85。RAM各90/90、正常exit0。temp/video-outcome-dedup-prores.json。短期/同一素材の複数layer条件であり、長時間・異なる素材混在・disk有効・visual parityは未達。ledger版本native検証中。

- 最新ledger版本ProRes native検証: 1080p論理/RAM-only/1,2layers各30/30、actual29/late0/drop0/repeat0。fault exceptions0、D3D12 ERROR/CORRUPTION0、CRT heap/assertion0、正常exit0。temp/video-native-outcome-dedup-debug.log、temp/video-native-outcome-dedup-repro.json。visual parity/他backend/長時間はこの証拠に含まない。

### 2026-10-12 — 独立プレビューの混在素材検証と長時間診断の範囲
- 関連: tools/VideoPreviewBench/main.cpp、Artifact/src/Layer/ArtifactVideoLayer.cppm。
- 確認済み: canonical exe、H.264 720p + ProRes HQ 1080p、論理1920×1080/DPR1.25、RAM-only、3秒の2/8-layer検証は共にactual89/drop0/repeat0。2-layer late0/pass、8-layer late3/fail。temp/video-mixed-outcome-dedup.json。混在条件の安定合格は未達。
- 確認済み: canonical exeを明示指定したnative debuggerの混在2/8-layer・1秒検証は例外0、重大D3D12/CRTエラー0、terminal exit1。両case actual29、late1/0。temp/video-native-mixed-dedup-debug.log。旧helper既定exeの初回起動はcase report未生成で検証証拠から除外。
- 懸念（未検証）: 現在のlate/repeated検出は各レイヤーの直近outcome窓を周期的に読むため、長時間の累積件数とは異なる。長時間検証では窓を超える計測、ループのframe/generation ledgerリセット、128-frame RAM上限を含め、合格条件を取りこぼさない測定方式が必要。
- 次に確認: 通常実行で8-layer混在の遅延発生箇所を計測し、表示用転送・GUI scheduling・メモリ圧迫を切り分ける。長時間試験はboundedなキャッシュ運用と累積present測定を設計してから行う。

### 2026-10-12 — 混在動画プレビューの転送とPresent計測
- 関連: Artifact/src/Render/PrimitiveRenderer2D.cppm、ArtifactCompositionRenderController.cppm、tools/VideoPreviewBench。
- 確認済み: 既存opt-in uploader診断を未準備24回／準備済み2048回に分離し、prepared表示と強制flush除去を追加。preparedはdecode workerも利用するのでRAM Compositeの識別子ではない。診断なしの描画処理は同じ。既存object/static library/benchを局所ビルドし正常リンク。
- 確認済み: 混在8-layer/3秒測定区間の128 uploadは全て2400×1195、pool hit、texture新規生成0。CPU側conversion/key median0/max2ms、UpdateTexture median1/max8ms。actual89/drop0/repeat0/late8、terminal exit1。temp/video-mixed-upload-bounded-summary.json。診断付きなので性能合格の証拠ではない。
- 確認済み: 既存Composition ViewフェーズログはQT_FORCE_STDERR_LOGGING=1を併用して採取。混在8-layer/1秒はactual29/drop0/late0/repeat0、terminal exit0。16ms以上のslow26回に限りframe median19/max24ms、setup1/1ms、layer2/13ms、submit2/11ms、present14/20ms。temp/video-mixed-phase-stderr-profile.log。CPU wall phaseの一部であり全frame平均やGPU時刻ではない。
- 仮説（未検証）: 残る散発lateはCPU変換やtexture新規生成だけでなく、Present待ちとGUI再描画タイミングの競合が関係する可能性。vsyncやpresent同期を根拠なしに解除すると表示順序・画面品質を悪化させる恐れがある。
- 次に確認: 同一frameの再描画要求、transport ACK、Present待ちの時系列とswapchain既存契約を調べ、boundedな再描画抑制で改善可能か確認する。

### 2026-10-12 — 同一frame再描画のtemporal不連続判定
- 関連: Artifact/src/Widgets/Render/ArtifactCompositionRenderController.cppm のrender tick。
- 確認済み: temporalFrame != last+1という判定は同一frame再描画でもtrueとなり、履歴無効化とN-1のpre-roll描画が走る。条件にtemporalFrame != lastを追加。同一frameを履歴の不連続として扱わず、seek/reverseの従来経路を維持。描画抑制・同期解除・追加allocationはない。
- 確認済み: controller objectのみ再コンパイルしてbenchをリンク。通常mixed2/8-layer・RAM-only・論理1920×1080・3秒はactual89/drop0/repeat0、late0/1、exit1。temp/video-same-frame-history-mixed.json。旧8-layer late3から単発で減ったが、ばらつきがあり性能改善は未確定。
- 確認済み: 最新canonical exeのnative mixed2/8-layer・1秒検証は両actual29/drop0/repeat0、late1/0、fault exception0、重大D3D12/CRTエラー0、正常terminal exit1。temp/video-native-same-frame-history-debug.log。実行中benchは残っていない。
- 価値／懸念: 同一frame再描画の不要な履歴破棄とpre-rollを回避できる。temporal effectの見た目、seek/reverse、scene変更時の履歴更新はこの動画のみの試験では未検証。
- 次に確認: 同一条件で再描画／Present回数を比較し、8-layerの残る遅延と、seek/reverseの既存履歴契約を確認する。

### 2026-10-12 — RAM表示済みフレームへのFrameReady追加描画
- 関連: Artifact/src/Widgets/Render/ArtifactCompositionRenderController.cppm。
- 確認済み: 同一frame temporal修正後でも8-layer混在3秒の測定は138 pooled texture参照。全2400×1195/pool hit、UpdateTexture CPU median0/max3ms、actual89/drop0/late0/repeat0。temp/video-history-upload-summary.json。Present回数減少の証拠ではない。
- 実装: RAM表示成功時のframe/CompositionIDを値保持。既存FrameReady subscriberはlive surface/damage無効化を維持し、再生中・同一Composition/frameのRAM表示成功済み・interactionなし・RAM画像残存時だけmarkRenderDirty追加を省く。既存dirtyは消さず、FrameChangedやauthoring要求も抑制しない。追加allocation/queue/signal/subscriptionなし。
- 確認済み: 局所controller build/bench link後、通常混在2/8-layer・RAM-only・論理1920×1080・3秒は両actual89/drop0/late0/repeat0でterminal exit0。temp/video-ram-ready-coalesce-mixed.json。
- 確認済み: native混在2/8-layer・1秒はactual29/drop0/repeat0、late1/0、fault exception0、重大D3D12/CRTエラー0、正常terminal exit1。temp/video-native-ram-ready-coalesce-debug.log。bench processは残っていない。
- 未検証: 単発短時間の通常合格であり長時間・反復・Composition切替・viewport操作・cacheなし・diskありを証明しない。追加描画回数の再比較も必要。
- 次に確認: 通常反復と測定中Present回数を比較し、長時間試験のbounded cache/累積診断を整備する。

### 2026-10-12 — Disk有効試験と長時間向け累積診断
- 関連: Artifact/src/Layer/ArtifactVideoLayer.cppm、tools/VideoPreviewBench/main.cpp。
- 確認済み: coalesce修正のdisk有効・混在2/8-layer・論理1920×1080・3秒は両RAM90/disk90/ready90、actual89/drop0/late0/repeat0、terminal exit0。temp/video-ram-ready-coalesce-mixed-disk.json。保存処理を伴う単発短時間の合格。
- 実装: 直近120-outcome窓とは別にlate/repeatedの固定quint64累積値を保持。first presentationのみ加算し既存診断resetでゼロにする。benchの合否は累積値のレイヤー間最大を採用（INT32_MAXで飽和）、policyをJSONに記載。累積diagnostic欠落は有効sampleに数えない。新規hot allocation/public ABI変更なし。
- 確認済み: 変更2objectを局所compile/link。H.264 1-layer・論理1280×720・RAM-only・4.2秒は126/126、actual125、window120、累積late/repeated0、drop0、terminal exit0。temp/video-cumulative-window-boundary.json。
- 確認済み: 最新exeのnative mixed2/8-layer・1秒は両actual29/drop0/repeat0、late1/0。fault exception0、重大D3D12/CRTエラー0、正常terminal exit1。temp/video-native-cumulative-outcomes-debug.log。benchは残っていない。
- 未検証: 早期の非ゼロerrorを窓からevictした際の累積保持は注入試験未実施。長時間ループ、bounded rolling cache、disk保存の長時間安定、全layer matrixを今後確認する。
- 次に確認: 128-frame cache上限を維持した長時間ループ試験と、wrapをまたぐactual present/drop診断の契約を整備する。

### 2026-10-12 — bounded windowの30秒ループ検証
- 関連: ArtifactPlaybackService.ixx/.cppm、tools/VideoPreviewBench/main.cpp。
- 実装: サービス既存present通知で同じframeのredrawを除く表示成功数を値保持しplay時reset。benchにloop-window-seconds追加、windowを128frames以内に制限、source/Composition/cache期間をwindowへ分離。測定時間のfps相当actual countと累積late/repeated/dropで判定。新規hot allocation/queue/signalなし。
- 実装: Loop中skip Noneの境界gapをplayable inclusive端点から数える。明示seekは既存baseline reset、ping-pongは除外。wrap欠落注入、reverse/ping-pong/seek回帰は未検証。
- 確認済み: service interface/implementation/bench objectを局所compile/link。混在2/8-layer・論理1920×1080・RAM-only・3秒窓を30秒ループは両900 actual presentation notifications、RAM90、final0、drop0/累積late0/repeat0、terminal exit0。temp/video-loop-window-mixed-30s.json。
- 確認済み: native mixed2/8-layer・1秒窓を3秒ループは両90 notifications/final0/drop0/repeat0/late1、fault exception0、重大D3D12/CRTエラー0、正常terminal exit1。temp/video-native-loop-window-debug.log。残存bench processなし。
- 制限: 成功通知はCPU present()の観測で物理scanoutではない。cached短区間の持続反復の証拠であり、長い固有区間やrolling decode、disk有効長時間、全layer matrix、反復試験、visual parityを証明しない。
- 次に確認: 同じ30秒条件の反復とdisk有効、全1..8 matrix、および長い固有区間をboundedに準備・再生する方式。

### 2026-10-13 — 全1..8-layer持続matrixと比較成果物
- 関連: tools/VideoPreviewBench/export_report.py、ArtifactVideoLayer::goToFrame、ArtifactPlaybackService::syncCurrentCompositionFrame。
- 確認済み: 最新ordinary混在1..8-layer（1はH.264のみ）・論理1920×1080・RAM-only・3秒窓/各30秒は全900 observed presentations/drop0/repeat0、late0/0/0/1/2/0/0/1。4/5/8はfail、正常exit1。temp/video-loop-window-mixed-matrix-30s.json。先行2/8単発passの安定再現は未達。
- 確認済み: 8-layer最後のwindow late0でも累積late1でfail。5-layerもwindow late1/cumulative2。過去の実遅延を窓のeviction後も保持した実行証拠。
- 実装／確認: offline exporterでCSVとmanifestを出力。全8case status/present/late件数を元JSONと照合一致。post-run静的CPU/GPU/driver/OS inventory、report/exe SHA256を記録。exe snapshotはlaunch identityではなく、利用率/peak memory/physical scanout等の不足を明記。測定中にOS inventory/hashを実行せず終了後に収集。残存benchなし。
- 確認済みのcode事実: PlaybackServiceは各frameでComposition goToFrameを呼び、VideoLayer goToFrameはsource frameが変わるとdecodeCurrentFrameを先行開始する。RAM final composite表示中もこの入口が残る。
- 仮説（未検証）: RAM表示中の不要なlive decode/prefetchやframe-ready処理が散発latenessに関与する可能性。LayerからPlaybackServiceへ新規importすると責務やmodule循環を広げるため、cache-ready時のdecode抑制はfallback/viewport契約を確認してから最小境界で検討する。
- 次に確認: 失敗caseのフレーム単位時系列とdecode/GUI待ちを計測し、反復条件で比較。長い固有区間とdisk有効matrix、残るmetricsを継続する。

### 2026-10-13 — First-late保持とComposition反映前の描画
- 関連: ArtifactVideoLayer.ixx/.cppm、ArtifactCompositionRenderController.cppm、VideoPreviewBench main/exporter。
- 確認済み: 旧版4/5/8-layer・各30秒repeatは全900 presentations/drop0/repeat0、late0/1/0、exit1。temp/video-loop-failed-cases-repeat.json。散発性と5-layer再発を確認。
- 実装: 最初のlate frame/source/elapsed/decode/queued/present/renderToPresentをPODで保持し既存resetで消去。詳細文字列は明示decodeState(true)でcold取得、通常decodeState()は従来summary。benchは計測終了後に全layer ID/source/diagnosticsをJSONへ、exporterはmanifestに保存。新規hot allocationなし。
- 確認済み: 初回詳細format実験5-layer60秒は約41.475秒のframe/source74でlate1、各layer decode0、queued16.09–17.00ms、renderToPresent17.20–17.48ms。temp/video-first-late-mixed-5layer-60s.json。この版は毎frame詳細整形がありperformance比較の限界。cold取得へ修正した5-layer60秒は1800/drop0/late0/repeat0/exit0。
- code事実／実装: rendererはPlaybackService clockを描画frameに採用する一方、Composition更新はGUI dispatchされる。render tickでmatching Composition frame != active service frameの場合、描画せず既存同期FrameChanged要求を待つguard追加。pause/different Composition/render queueは除外。新規transport/queue/signal/waitなし。不一致発生率とlateへの因果寄与は未測定。
- 確認済み: 最新guardのmixed5/8-layer・論理1920×1080・RAM-only・3秒窓/各60秒は両1800/drop0/累積late0/repeat0/exit0。temp/video-composition-sync-gate-60s.jsonとCSV。
- 確認済み: native mixed2/8-layer・1秒窓/3秒は両90/drop0/repeat0、late1/0。fault exception0、重大D3D12/CRTエラー0、正常exit1。temp/video-native-composition-sync-gate-debug.log。bench残存なし。
- 次に確認: 最新全1..8 matrixと反復、guard deferral時系列、viewport/no-cache/disk有効経路。cached短区間であり長い固有sourceとvisual parityは未達。

## 2026-10-13 — 完成描画数と表示通知数の比較

- 関連: tools/VideoPreviewBench/main.cpp、export_report.py、ArtifactCompositionRenderController。
- 確認事実: 既存currentFrameSerialの測定区間差分を追加。全1..8-layer各60秒試験では4-layerにlate3、他はlate0。4-layer追試は完成描画1800／表示通知1800、drop/late/repeat0。余分な完成描画はこの追試では確認されない。
- 価値・懸念: 単発合格だけでは散発遅延を解決したと判断できない。queued19.96msとrender-to-present16.46msの先行first-late証拠はCPU側時間であり、背景decodeやGPU負荷を除外できない。
- 次の確認: 同一条件反復と遅延発生時の描画数比較。physical scanoutは未検証。Microsoft GetFrameStatistics仕様ではflip/fullscreenの条件と複数monitor等の信頼性制約があるため、新規hot-path DwmFlushを安易に入れない。参照: https://learn.microsoft.com/en-us/windows/win32/api/dxgi/nf-dxgi-idxgiswapchain-getframestatistics

### 2026-10-13 — 同数描画でも散発遅延が再現

- 関連: temp/video-render-cycles-repeat-1.json、repeat-2.json、Composition render ticker。
- 確認事実: 4-layer各60秒の独立2試行は両完成描画1800／表示通知1800、drop/repeat0、late0/1。失敗試行はframe68、queued18.49ms、render-to-present15.19ms、total33.68ms。再生開始の既存PlaybackStateChangedでticker intervalが1msへ更新されるコードも確認。
- 価値: この失敗を余分な完成描画だけで説明する仮説は支持されない。
- 未検証: OS scheduling、背景decode/prefetch、swapchain待ち、ベンチの250ms診断sampleの影響。次にこれらの時間を低負荷で分離し、合否閾値を緩めず調査する。

### 2026-10-13 — 定期診断sampleの観測負荷を数値化

- 関連: tools/VideoPreviewBench/main.cpp、export_report.py。
- 確認事実: 既存QElapsedTimerを使いvideo＋renderer診断の最大時間と開始時刻を固定scalarで保持。4-layer60秒試験は表示/描画1800、drop/late/repeat0。最大sample2.3833ms、video部分2.2934ms。新規ログ・container・product変更なし。
- 価値・懸念: 診断自体のGUI占有を無視しない。現試行はlateなしのため因果は未検証。既存QString正規表現/QJson sample確保は残り、今回の追加scalar計測自体に確保はない。
- 次の確認: 同一診断版の失敗試行と最大sample時刻の比較、msleep/GUI dispatchの時間分離。renderer平均時間には準備区間が混ざるため、区間内平均として使う前に集計範囲も確認する。

### 2026-10-13 — 動画upload cache上限と共有cache fallbackの区別

- 関連: Artifact/src/Layer/ArtifactVideoLayer.cppm goToFrame、prepareDecodeRequest、hasCachedUploadFrame、buildUploadFrame。
- 確認事実: goToFrameはComposition RAM再生でもsource更新とdecodeCurrentFrame準備へ進む。ローカルuploadFramesは4-frame/64MiB上限だが、hasCachedUploadFrameは共有Asset decodedPayloadも確認し、共有uploadがあればenqueueを避ける。worker側でも共有uploadを再利用して変換を省略する。
- 価値・懸念: ローカル4-frame上限だけではsteady-state変換の原因と断定できない。上限拡張やdecode停止の根拠はまだない。
- 次の確認（未検証）: Composition RAM再生中の共有lookup hit/missと準備時間。全layer frame評価を止めると動的property/audio/simulationへの影響があり、単純なgoToFrame省略は採用しない。

### 2026-10-13 — disk併用時の失敗と準備完了条件

- 関連: VideoPreviewBench waitForRamPreview、PlaybackService disk writer、export_report.py。
- 確認事実: disk有効4/8-layer各60秒でdrop1/0、late6/3、repeat0。最終disk90であるが測定開始時のdisk完了数はレポートに残っていない。既存準備はRAM-readyのみでreturnする。
- 実施: 任意wait-disk-cacheでRAM＋disk全range完了を待つ比較条件を追加。製品コード変更なし、ビルド成功、無効option組合せexit2。実再生比較は未検証。
- 価値・懸念: 保存ワーカーの負荷を原因と断定しない。RAM併用なのでdisk-only read性能の証拠でもない。外部OS peak観測はwhole-process Working Set約10.57GiBで、case別集計や最後の未sample peakではない。
- 次の確認: 準備時disk数を別fieldに保持し、waitあり/なしを同条件で反復。

### 2026-10-13 — ベンチmeasurement event pumpの切り分け候補

- 関連: tools/VideoPreviewBench/main.cpp measured playback loop。
- 確認事実: measurementはprocessEvents(AllEvents,10)とQThread::msleep(2)を繰り返す。既存250ms診断は最大約2–3msだが、sleep自身の実待機時間・GUI dispatch遅延は未取得。
- 仮説（未検証）: polling/sleepが自然なQt event loopと異なるGUI応答を作り、散発lateに寄与する可能性。製品rendererだけの問題と断定しない。
- 次の確認: QBasicTimer＋既存QEventLoopで測定deadline/sampleを処理する独立harness比較。新signal/slotやtimerごとのheap allocationを追加せず、合否を緩めない。既存条件の結果も保持する。

### 2026-10-13 — disk保存完了後にもlateが残る

- 関連: VideoPreviewBench prepared cache counts、wait-disk-cache。
- 確認事実: mixed4/8-layer各60秒の開始時はRAM90/disk90。両表示/描画1800、drop/repeat0、late1/0。4-layer first-lateはframe17、queued19.74msとrender-to-present14.50msでtotal34.24ms。
- 価値: disk未完了のみを原因とする説明は支持されない。待機なしcaseとの件数差は試行変動があり因果未確定。
- 次の確認: 同じprepared countを記録した反復と、GUI measurement event loop比較。基準や合否を緩和しない。

### 2026-10-13 — measurement dispatchの比較実装

- 関連: VideoPreviewBench PreviewMeasurementLoop、measurement-loop option。
- 確認事実: polling既定値を維持してqt modeを追加。stack QEventLoop/QBasicTimer2個と参照callbackで、250ms診断と残り測定時間deadlineを処理する。製品transport/render変更なし、新signal接続・標準container・callback heap所有なし。object/link成功。
- 価値・懸念: ベンチ自身の2ms polling sleepを分離する比較条件。timer登録は測定開始のcold boundaryで行うが、Qt内部のallocation全体を測定したとは言えない。
- 次の確認: 同一保存完了条件でqt/polling反復、native例外検証。単発合格を因果proofにしない。

### 2026-10-13 — Presentation ACKの成功判定を厳格化

- 関連: ArtifactCompositionRenderController::presentAndUpdateVideoLayers、ArtifactIRenderer::present、VideoLayer::markPresented。
- 確認事実: controllerはstatusにfailが含まれなければPlaybackServiceに表示成功を通知していた。rendererにはskipped-no-swapchain、device-recovery-deferred-shared、device-recovered、およびfailを含まない例外messageが存在する。VideoLayer側はok以外をbottleneck=presentとするが、stage/outcomeは時間と重複だけで判定する。
- 修正: controller ACKもokのみへ1行で統一。新API/待ち/確保なし。現在のdispatch比較は旧binary固定のまま完了させ、その後にobject/linkとnative再検証する。
- 懸念・次の確認: skip/recoveryを実表示と数えない正しさの修正であり、現在の通常lateの原因を説明したとは言えない。故障注入による各statusのruntime確認は未検証。

### 2026-10-13 — 同binary dispatch反復比較の結果

- 関連: compare_dispatch_reports.py、temp/video-dispatch-paired-comparison.json。
- 確認事実: polling/qt/qt/polling各60秒の報告条件と起動直前SHA256は全一致。polling合格1/2（drop1/0、late1/0）、Qt合格2/2（drop/late/repeat0、表示1800）。全試行準備RAM90/disk90。
- 価値・懸念: dispatch寄与の可能性はあるが、少数逐次試行から因果や安定合格は証明できない。比較binaryに後続ACK修正は含まない。
- 次の確認: 最新ACK修正版でQt mode全layer matrixと反復、長い固有source区間、physical表示。ACK修正後nativeは例外・重大GPU/CRTエラー0、2/8-layer各90表示、late各1、正常exit1。故障注入は未検証。

### 2026-10-13 — Qt/ACK全matrix未達とWindows短待機候補

- 関連: video-qt-ack-full-matrix-60s.json、probe_windows_waits.py、ArtifactCore/src/Thread/PreciseTicker.cppm。
- 確認事実: 全1..8-layer各60秒は全表示/描画1800、drop/repeat0、late0/0/0/0/1/1/9/27。開始cache RAM90/disk90。実行直前/終了後exe hash一致、外部memory観測終了、Working Set peak約10.48GiB。Qt dispatch単独では全条件未達。
- 独立probe: 1ms要求各256回、Sleep/通常waitable/high-resolution waitableのp50は15.4294/15.5977/1.5371ms。設定・priorityは変更せず自timer handleのみ作成/close。非GUI Pythonであり、実Qt/C++ CV/previewの証拠ではない。
- 仮説（未検証）: PreciseTickerのCV wait_untilが要求1msより粗い待機になり、queued15–22ms＋Present12–19msのlateに寄与する可能性。
- 次の確認: 実tickerと同じC++/GUI環境の待機測定を先に行い、global timer resolutionやpriority変更ではなく再利用するhigh-resolution waitable timerを検討。stop/reschedule、fallback、handle lifetime、他ticker利用者への影響を読んでから最小修正する。
- 参照: https://learn.microsoft.com/en-us/windows/win32/api/synchapi/nf-synchapi-createwaitabletimerexw 、https://learn.microsoft.com/en-us/windows/win32/api/timeapi/nf-timeapi-timebeginperiod

### 2026-10-13 — 実レンダーtickerの短待機計測

- 関連: ArtifactCompositionRenderController.cppm、temp/video-real-render-dispatch-profile.json。
- 確認事実: 8動画レイヤー、Qt測定ループ、disk-readyの診断実行で、要求1msの実ticker間隔63件は中央値1.555ms、最小1.028ms、最大28.272ms。独立Python待機probeの約15msを実tickerへ当てはめる説明は支持されない。診断有効の試行はlate1であり性能合格の根拠ではない。
- 次の確認: 既存workerからGUIへの投稿とcallback開始の時間差をboundedで記録する。共通PreciseTickerの待機方式はこの結果だけでは変更しない。

- 続くGUI受け渡し診断: temp/video-gui-dispatch-profile.json/.log。64件の投稿からGUI callback開始まで中央値3.910ms、最大32.710ms、10ms超20件。ticker63件は中央値1.556ms、最大28.623ms。8レイヤー3秒試行はdrop0/repeat0/late2で未達。診断ログ自体の影響があるため受け渡しの遅延存在を示すに留め、原因や改善量は断定しない。次はGUI側の処理占有時間を特定する。

### 2026-10-13 — 提示後処理と診断文字列の切り分け

- 関連: ArtifactCompositionRenderController.cppmのpresentAndUpdateVideoLayers、ArtifactStatusBar.cppm。
- 確認事実: Present成功通知の後に各動画のmarkFramePresented、最初の可視動画のdecodeState文字列生成があり、描画側は診断eventを発行する。StatusBarは文字列の大文字化とelide、tooltip更新を行う。
- 仮説（未検証）: この経路がGUI占有に寄与する可能性があるが、存在だけでは遅延原因の証拠にならない。
- 次の確認: 既存profileフラグでPresent呼び出し／提示後bookkeepingを64件まで別々に測る。通常実行ではclock読み取りと整形を行わない。診断eventの削除やUI挙動変更は未実施。

- 提示区間の診断結果（2026-10-13）: `temp/video-present-stage-profile.json/.log`。8混合レイヤー、Qt、disk-ready、cache window1秒／測定3秒。64件のPresent中央値3.3755ms／最大7.414ms、提示後bookkeeping中央値0.240ms／最大1.714ms、GUI待ち中央値4.3595ms／最大23.064ms。drop／late／repeatは0でexit0。ただし3秒windowの前回とは条件が異なる短診断であり安定性合格や改善量を示さない。提示後更新だけを主因とする説明はこの結果では支持されず、GUIの他の処理占有／描画開始前を引き続き調べる。

### 2026-10-13 — 描画tickerのidle停止とworker再生成候補

- 関連: ArtifactCompositionRenderController.cppmのrenderDirty空判定とmarkRenderDirty、ArtifactCore/src/Thread/PreciseTicker.cppm。
- 確認事実: 描画要求が空のcallbackはviewport操作中でなければtickerをstopする。markRenderDirtyは停止中ならstartする。stopはworkerをjoinし、startはstd::threadを生成する。
- 仮説（未検証）: 再生フレーム間でもこのidle判定が走るとworker生成とjoinが毎フレーム発生し、GUI待機の揺れに寄与する可能性。停止時playing状態とframeSerialを64件まで診断して確認する。共通tickerの実装変更は未実施。

- 実測追記: temp/video-idle-stop-profile.json/.logの8混合レイヤー、cache window1秒／測定3秒で停止診断64件中61件がplaying=1、frameSerial58–118の連続フレームだった。drop／late／repeat0の短試行でも毎フレーム停止がある。再生中のidle判定でworkerを維持する最小修正を実施し比較準備中。通常の停止中idle／明示停止経路は維持。idle時も既存のcoalesced Qt投稿が続くため、その小さなイベント確保・CPU負荷と遅延を次の実行で確認する。

- 2026-10-13 実行結果: `temp/video-playback-worker-retain-60s.json`。修正後、診断無効、8混合レイヤー、Qt、disk-ready、3秒window／測定60秒でpreparedRAM90／disk90、描画1800／提示1800、drop0／late0／repeat0、exit0。最大diagnostic sample7.5894ms。旧全matrixの8レイヤーlate27との条件は主要項目が一致するが、別試行かつmatrix内順序・熱状態差があるため因果や安定性はまだ断定しない。全matrix反復、長い固有フレーム、物理scanoutは未完了。

- 2026-10-13 native確認: `temp/video-native-playback-worker-retain-debug.log` はexceptions0、D3D12 ERROR／CORRUPTION等の重大エラー0、正常な合否終了exit1。2／8レイヤー各3秒、window1秒、disk-ready、Qtで提示／描画90ずつ、drop／repeat0、late1／0。debugger下の2レイヤーはタイミング未達。Live Object警告は残る。通常実行の性能合格として扱わない。1–8レイヤー各60秒、window3秒の通常全matrixを開始し、`temp/video-worker-retain-full-matrix-60s-launch.json` に実行前hashとargumentsを保存した。終了時hash／exitは終了後に保存する。

### 2026-10-13 — 長い固有区間のcacheゲートと復元境界

- 関連: tools/VideoPreviewBench/main.cppのprepareRamPreview、ArtifactPlaybackService.cppmのeviction／hydrateFrameFromDisk／storeRamPreviewImageData。
- 確認事実: 全RAM-readyゲートと128-frame evictionは128超区間で両立しない。同期disk復元と停止中限定RGBA8 sidecar生成も長いrolling経路の制約。
- 価値: 3秒cache反復の成功を長いsource安定性へ拡張して解釈しない。
- 次の確認: 全disk準備と先読みRAMを別ゲートにし、既存所有権境界を保つbounded非同期復元の設計・実装・検証。詳細を既存milestoneの次段階へ記録した。未実装。

### 2026-10-13 — 非同期disk復元の共有状態境界

- 関連: ArtifactPlaybackService.cppmのdisk writer、loadPreviewDiskManifestSnapshot、isPreviewDiskManifestFrameValid。
- 確認事実: writerはpreviewDiskWriteMutexを保持してPNG保存、容量eviction、manifest書込を行う。GUIからのwrite enqueueとcache削除も同じmutexを使う。manifest読込memoはサービスのmemberへ書き込むため、既存hydrate／validation関数をそのままworkerで呼ぶ方式は共有状態の競合を招く。
- 懸念（未検証）: writerとの共用キューは保存の重い処理の後ろへ先読みを滞留させる可能性。現在のwarm-cache試行に影響する原因だとは断定しない。
- 次の確認: worker専用manifest snapshotとimmutable request identityを使い、GUI所有LRU／frame statesへ触るのは結果反映のみとする。frame、namespace、composition、generation、期待サイズ／形式を結果へ保持し、世代不一致を捨てる。保存・復元のbounded schedulingと終了処理を設計してから実装する。

### 2026-10-13 — rolling復元の画像APIと色契約

- 関連: ArtifactCore/src/Image/ImageF32x4_RGBA.cppmのload、ImageUploadConversion.ixx、ImageF32x4_With_Cache。
- 確認事実: ImageF32x4_RGBA::loadはOIIO FLOAT／reorient／RGBA channels経路でunknownRgba32Floatを設定し、失敗時はQImageのpremultiplied BGRA互換経路へ進む。既存disk hydrateはQImageからRGBA8へ明示変換する経路なので、loadへ単純置換すると色・alpha契約が一致するとは証明できない。upload変換には既存convertImageForUploadIntoがある。cache wrapperはswap APIを持ち、固定slotからGUIcacheへの所有移譲の候補となる。
- 次の確認: PNG cacheのsRGB／straight alphaを明示し、半透明・黒・白・色パッチの比較でdisk/RAM一致を確認してから採用する。固定slot／byte budget／stale世代廃棄とともに設計し、GUI側の深いcopy・大容量確保を持ち込まない。未実装・未検証。

### 2026-10-13 — CPU RAM画像とuploadの合算budget

- 関連: ArtifactPlaybackService.cppmのevictRamPreviewImageCacheIfNeeded、storeRamPreviewImageData。
- 確認事実: CPU image evictionは128-frame上限、upload sidecarは別1GiB byte上限。全CPU画像のbyte上限は当該経路では未確認。全matrix中PID16700の単一working set観測は10857377792bytesで、peak／cache単独値ではない。temp/video-worker-retain-mid-matrix-memory-note.jsonに観測とnominal payload計算を保存。2400x1195のRGBA32F＋RGBA8は90枚で5162400000bytes、128枚で7342080000bytes（複製・GPU・queue等を除く）。
- 懸念: rolling poolを128枚だけで制限してもbyte量は大きい。新たな容量制御はRAM ready契約と先読み数を同時に設計し、既存試験を単純なeviction変更で壊さない。
- 次の確認: CPU＋upload＋in-flight＋retired slotを合算した固定byte budgetと、サイズ検証／allocation前reservation／高解像度でのbounded backpressureを実装する。現在のrenderer性能原因や単一観測をpeakと解釈しない。

### 2026-10-13 — 全matrixのメモリ継続観測

- 関連: temp/video-worker-retain-memory-observer.py、temp/video-worker-retain-full-matrix-memory.json。
- 確認事実: 実在するbench PID16700へquery/read/synchronize handleを1回だけ取得し、1秒間隔最大3600件でworking set、OS lifetime peak working set、private bytes、peak pagefileを記録するread-only観測を開始。case4途中からで、先行caseの時系列はない。初回保存を確認し、working set10935255040／peak11099897856／private10746658816bytes。process exit待機はhandleを用い、PID再利用された別プロセスへ再接続しない。終了または上限でhandleを閉じる。停止／kill／priority変更は行わない。
- 価値: 単発観測だけでpeak・リーク・増加傾向を判断しない。記録中はprocessExited=falseが未終了を示し、末尾区間が未観測の可能性も残る。CPU画像・GPU資源・allocator保持を区別できる値ではない。
- 次の確認: matrixとobserverの終了を別々に確認し、完成reportとlaunch hash、whole-process memoryをoffline exportへ添付する。1–3レイヤー各60秒の途中case結果はdrop／late／repeat0、最終JSONはまだ未完成。

### 2026-10-13 — 試行実行記録の再利用

- 関連: tools/VideoPreviewBench/run_recorded.py。
- 実装: 新規prefixに限って独立benchを実行し、exe／startup fileの前後hash、arguments、selected environment、exitを記録するrunner。既存report上書きを事前拒否し、runner例外時も終了記録を残す。
- 確認: help正常、既存prefix拒否exit2で新たなbenchを起動しなかった。実際の新規試行は未検証。
- 次の確認: 現行matrix終了後の反復試行で採用し、exportのlaunch-recordと一致することを確認する。loaded DLLと実効startup flagsの証明ではない。

- worker維持の全matrix途中結果追記（2026-10-13）: 1–6レイヤー各60秒はdrop／late／repeat0、7レイヤーはdrop0／late5／repeat0で条件未達。temp/video-worker-retain-full-matrix-interim.jsonへログ集計を保存。8レイヤーは進行中で、最終per-layer firstLate診断は終了reportで確認する。単独8レイヤー成功を全matrix安定性の証明へ拡張しない。worker再生成の解消だけで十分とは言えない。

### worker維持・全matrix終了（2026-10-13）

`temp/video-worker-retain-full-matrix-60s.json` は1–8レイヤー各60秒、Qt、disk-ready、window3秒。全case提示1800／描画1800、preparedRAM90／disk90、drop／repeat0。lateは0／0／0／0／0／0／5／1でexit1、未達。launch前後exe hash一致、observerは終了確認済み642件、OS peak working set11413118976bytes（全process／途中開始、cache単独ではない）。CSVとmanifestへlaunchとmemoryのfingerprintを添付した。

7レイヤー最初のlateはframe54・開始1806.25ms、decode0、queued6.45ms、render-to-present29.37ms、全体35.83ms。8レイヤーの最初のlateはProRes layer2・frame59・開始55969.30ms、decode0、queued1.60ms、render-to-present31.79ms、全体33.39ms。単独8レイヤーの成功で安定性は証明できず、次はrender区間全体とPresent／uploadをbounded診断する。

### 2026-10-13 — 再生区間の描画profile追加

- 実測: temp/video-worker-retain-render-duration-7.json/.log。measure marker後の7レイヤー診断で描画64件中央値16.2385ms／最大24.040ms、Present中央値3.3445ms／最大9.623ms、bookkeeping中央値0.2485ms／最大1.671ms。prepared upload90件update中央値0ms／最大3ms（整数ms量子化）、conversion／creation最大各1ms。GUI待ち64件中央値0.059ms／最大22.005ms。drop0／late1／repeat0で未達。診断有効の短試行であり改善の証明ではない。
- 確認事実: 既存SetupProfileはrenderFrameCounter<32のみなのでcache準備でサンプルを使い切り、measure区間を記録しない。再生中用の独立64件bounded counterを追加し、既存phase marksを再利用して残りの描画区間を計測する。通常実行のclock／整形を増やさない。
- 次の確認: setup phaseを再計測し、render全体とupload／Present以外の時間を確認する。新recorded runnerで実試行が終了し、exit1と実行前後exe hash一致を確認済み。

- 再生SetupProfile結果（2026-10-13）: temp/video-worker-retain-playback-setup-7-summary.json、7レイヤーのmeasure marker後64件。preScene中央値0.01ms、sceneLights0.235ms、envShadow0.01ms、cameraRange0.02ms、playbackState0.01ms、damage0.04ms、pipeline0.41ms、graphSetup0.02ms。renderPresent区間中央値15.645ms／最大26.24ms。短診断はdrop／late／repeat0でexit0だが通常全matrix未達を覆さない。初期scene／damage設定よりrenderPresent内部が支配的。次にRAM base／composite／overlay／submit／presentの既存pass境界を切り分ける。

### 2026-10-13 — RAM描画pass内の区間計測

- 関連: ArtifactCompositionRenderController.cppmのRAM Base／Composite、Overlay、SetupProfile。
- 実装: 既存setup marksを固定14要素へ拡張し、RAM pass前後・Overlay前後を記録。profile有効かつ既存bounded枠内だけRAM pass／between／overlay／tailPresentを出力する。描画順序／内容／event経路は未変更。
- 次の確認: 7レイヤーの再生中64件でrenderPresent中央値約15.65msの内訳を調べる。固定stack領域の増分32bytes、heap collectionや毎フレームログの無制限追加はない。通常実行では計測時計や整形を行わない。ビルド・実測は確認中。

- RAM pass実測追記（2026-10-13）: temp/video-worker-retain-ram-pass-7-summary.jsonのmeasure後64件。RAM pass中央値1.857ms／最大13.411ms、between中央値8.3795ms／最大15.313ms、overlay中央値0.0755ms／最大1.071ms、tailPresent中央値5.9855ms／最大17.883ms。短診断drop／late／repeat0。between内のshadow finalize／flush／selection overlayを追加分割する。ArtifactIRenderer::flushはsubmitQueuedDrawsの後にctx->Flushし、既存flushContextTimeUsはctx->Flushだけを測るため、submit全体時間と同一視しない。barrierの削除は未実施。

- finalize分割結果（2026-10-13）: temp/video-worker-retain-ram-finalize-7-summary.jsonのmeasure後64件。beforeFinalize中央値0.002ms／最大0.004ms、finalize中央値2.316ms／最大16.578ms、selection中央値0.0235ms／最大0.050ms、afterSelection0ms。先のbetween中央値8.38msとの差は別試行変動で、改善量とは解釈しない。中間区間はshadow／submitQueuedDraws／context Flush／RTV復帰に集中する。DiligentImmediateSubmitterはdeferred contextがある場合Begin／FinishCommandList／ExecuteCommandLists／FinishFrameを使い、DeviceManagerはdeferred contextを作成している。これが主因かは未検証。次はsubmitとcontext Flushを分ける。barrierやdeferred経路の変更は未実施。

### 2026-10-13 — context Flushとsubmitを既存counterで分離

- 関連: controllerのRAM finalize、ArtifactIRenderer::flushContextTimeUs。
- 実装: 既存64件setup profile枠内でrenderer.flush全体の高精度時間と既存context Flush累積counter差分を同時に出力する。共通renderer／Diligent依存は未変更。全体からcontext時間を引いた残差にはsubmit以外の少量処理も含み、純粋なsubmit時間と断定しない。
- 確認: controllerのみcompile／standalone relink成功、diff check成功、7混合レイヤー診断実行中（temp/video-worker-retain-ram-flush-7）。直前のfinalize診断は提示90、drop0／late1／repeat0、正常合否終了exit1、exe前後hash一致。
- 次の確認: Flush自身が長いか、その前のcommand recording／executionが長いかを実測する。通常実行では追加clock／counter取得／整形を行わない。

- Flush内訳の実測（2026-10-13）: temp/video-worker-retain-ram-flush-7-summary.jsonのmeasure後64件。flush全体中央値8.2035ms／最大17.222ms、context Flush中央値0.004ms／最大0.021ms、残差中央値8.1985ms／最大17.219ms。短診断drop／late／repeat0でexit0。残差はsubmitQueuedDraws（2D submitと3D gizmo flush）と微小処理を含む。context Flush自身を主因とする説明は支持されない。checkerboardは既存CheckerboardPkt1件でGPU描画するため、多数のCPU矩形記録を原因として扱わない。次に2D submitのrecord／FinishCommandList／ExecuteCommandLists／FinishFrameと3D gizmo flushを計測する。汎用Profilerの有効化はframe／scopeログに動的確保を伴うため、bounded診断の代替として無条件に有効化しない。

### 2026-10-13 — 2D submitと3D gizmo送信の分離

- 関連: Artifact/src/Render/ArtifactIRenderer.cppmのsubmitQueuedDraws。
- 実装: 既存PROFILE_SETUPで2D submit／3D gizmo flush時間とpacket数を測る。診断時のみ非空submit最大4096件／threadの固定counter、通常時clock／packet size取得／整形は行わない。準備区間のsubmitが多いためcontrollerの64再生枠とは別枠にし、measure marker後のみ集計する。公開API・signal・新規container・Diligent依存変更はなし。固定counter以外の保持容量は増やさず、stdioは明示flushしない。
- 次の確認: renderer object／static library／standalone exeの順に更新し、7レイヤーの再生中サンプルを比較する。profileによる追加ログの影響があるので性能合格に使わない。

- 2D／3D送信実測（2026-10-13）: temp/video-worker-retain-submit-7-summary.json、measure後180非空submit。2D中央値2.1645ms／最大14.129ms、3D gizmo中央値0.001ms／最大0.027ms。短診断drop／late／repeat0でexit0。2D側の揺れに絞り、DiligentImmediateSubmitter.cppmへrecord、FinishCommandList、ExecuteCommandLists、FinishFrameのbounded stage診断を追加。4096件/thread、既存PROFILE_SETUP、公開APIと描画動作未変更。headerはglobal module fragmentへ直接include。compile／library／standalone link確認中。

### 2026-10-13 — 小さなserial submitのcommand-list handoff

- 実測: temp/video-worker-retain-submit-stages-7-summary.json、measure後180件すべてdeferred、packet数3と9が各90件。record中央値0.465ms／最大8.306ms、FinishCommandList中央値0.0305ms／最大0.051ms、ExecuteCommandLists中央値1.712ms／最大11.808ms、FinishFrame中央値0.011ms／最大0.029ms。短診断drop0／late1／repeat0、exit1。
- 実装: 16 packet未満は既存immediate submit経路を使い、より大きなbatchはdeferredを維持。既存compute consumer例外は維持。変更はsubmitter cppmの選択条件のみで、描画順序／PSO／resource transition／GPU buffer仕様は変えない。16という境界は初期実装判断であり最適値は未検証。
- 次の確認: 同じ7レイヤー診断でrecord／executeと全体時間、通常7／8長時間、native例外とD3D12エラー、表示一致を確認。性能改善・正しさはまだ未証明。新規hot allocation／containerなし、依存Diligent変更なし。

- 小submit immediate診断（2026-10-13）: temp/video-small-submit-immediate-profile-7-summary.json、measure後180件、deferred0件。record中央値0.3815ms／最大1.897ms、FinishCommandList／ExecuteCommandLists／FinishFrameは使わず0。短7レイヤー診断はdrop／late／repeat0でexit0。単一診断・ログ有効なので通常長時間安定性と表示一致はまだ未証明。次はcanonical exeのnative2／8レイヤーと通常7／8長時間、cold明示readbackを使う表示確認を進める。

### 小submitのimmediate経路・検証中（2026-10-13）

再生中の2D送信は3／9packetで、旧deferred経路のExecuteCommandListsは最大11.808msだった。DiligentImmediateSubmitter.cppmの選択条件を16packet未満は既存immediate経路へ変更した。大きなbatchとcompute consumer例外、描画順序／PSO／resource transitionを維持。境界16の最適性、他描画内容の表示一致は未確認。共通submitterの変更なので動画以外の小batchにも作用し、表示確認が必要。Diligent依存や公開APIは未変更。

修正後7レイヤー診断は180非空submitすべてimmediate、record中央値0.3815ms／最大1.897ms、drop／late／repeat0。native2／8レイヤー各3秒はexceptions0、重大D3D12エラー0、late1／0で正常合否終了exit1。Live Object警告は残る。7／8レイヤー各120秒、window3秒、Qt、disk-ready、診断無効の通常試行を開始（temp/video-small-submit-immediate-7-8-120s）。長い固有source／他レイヤー表示／physical scanoutの証明ではない。

### 2026-10-13 — 性能区間外のGPU画像保存

- 関連: tools/VideoPreviewBench/main.cpp。
- 実装準備: 任意capture-final-frameを追加。report必須、測定とvideo診断の完了・停止後に既存renderer.readbackToImageを明示呼出ししPNGへ保存、caseのpostMeasurementCaptureへpath／size／saved／取得元を記録する。QImageは既存Qt入出力境界のcold診断のみで、描画や合成に採用しない。既存captureCurrentFrameImageのscreen fallbackへは委譲しない。既存PNGは上書きせず保存失敗時overallExit1。性能カウンターの合否と画像保存結果は別記録。
- 制限: GPU render targetの終了時読み戻しであり、physical scanout／全再生フレームの表示／frame identity一致の証明ではない。capture未指定時はreadbackしない。
- 次の確認: 現在の120秒試行終了後、bench mainだけcompile／relinkし、保存画像の目視とcolor／alpha／black検査を進める。現時点では未ビルド・未実行。

- 小submit immediate通常試行の途中結果（2026-10-13）: temp/video-small-submit-immediate-7-8-120s.logで7レイヤー／window3秒／診断無効／120秒のcase終了を確認、drop／late／repeat0。8レイヤーは準備中。最終JSONが出るまで提示数などの全項目は未確認。画像保存オプションは未ビルドのままなので、この試行条件には影響しない。

### 2026-10-13: 停止後swapchain readbackのframe対応

- 関連: Artifact/src/Render/ArtifactIRenderer.cppm、tools/VideoPreviewBench/main.cpp。
- 確認済み: activeColorViewはcurrent backbufferを優先する。
- 未検証: Present後readback画像と直前の提示frameとの対応。buffer rotationにより一致しない可能性がある。
- 価値／懸念: 単一GPU画像を最終提示frameや物理scanoutの証拠と誤認しない。
- 次の確認: 明示的な対象texture／frame identityを保持できる既存経路を調べる。現在の画像保存は診断用途に限定する。
- **追加確認 (2026-10-19):** direct 2D controller pathのPresent後に`readbackToImage()`すると、直前に描画したbufferではなく次のcurrent backbufferを読むため黒画像になるfixtureを確認した。production image testではpresented SRVを保持するGPU composition pathへfixtureを寄せると、D3D12 / Vulkanで同一の400×300 PNGと赤pixelを得られた。post-presentのswapchain readbackはframe identityがなければ描画結果の証拠として扱わない。

### 2026-10-13: disk hydration上限は走査件数を制限しない

- 関連: Artifact/src/Service/ArtifactPlaybackService.cppm、hydrateFramesFromDiskNear。
- 確認済み: touchedFramesはhydrateFrameFromDiskがtrueのときだけ増加する。cache欠損時にはmaxFramesToTouch=9でもrange全体を探索し、各候補で同期file／manifest確認を行い得る。
- 懸念: 画像読込件数の上限と、GUIでの探索／I/O件数の上限は別。長いrangeの欠損や壊れたcacheで入力応答へ影響する可能性（時間は未計測）。
- 次の確認: 既存range readiness契約を保ち、候補走査の固定上限と継続cursor／非同期readerを設計する。現在の短い全RAM試験ではこの経路の改善効果を判断できない。

- 上記候補探索上限の進捗（2026-10-13）: 成功ではなく試行件数を数える最小修正を実装。未ビルド／runtime未確認。同期デコードとrange全体disk readinessの課題は残る。

### 2026-10-13: 非同期disk復元後のGUI DeepCopy境界

- 関連: ArtifactPlaybackService.cppm::storeRamPreviewImageData、ArtifactCore/src/Image/ImageF32x4_With_Cache.cppm::SetCpuImage。
- 確認済み: 保存先のSetCpuImageはDeepCopyする。既存swapはimpl pointerを交換する。Image／wrapperはQObject。
- 懸念: decodeだけのworker化では、結果反映時の大容量コピー／確保がGUIへ残る。worker生成QObjectの移譲をswapだけで安全と断定できない。
- 次に確認: GUIコールド生成slotのraw作業域をworkerへ貸す設計、GPU resource lifetime、GUI cacheへの交換、全slot合計bytesの上限。未実装／未検証。

### 2026-10-13: disk書込queueの画像所有と容量上限

- 関連: ArtifactPlaybackService.cppm::PreviewDiskWriteTask、3箇所のpreviewDiskWriteQueue_.push_back。
- 確認済み: taskはImageF32x4_RGBAを値で所有し、enqueue前にqueue件数／合計bytesの上限を確認していない。通常captureと2つのstore overloadでenqueueする。
- 懸念: writerが遅いと画像snapshotが累積し得る。今回whole-process memory peakへの寄与量は未計測で、主因とは断定しない。
- 次に確認: snapshot bytes／queue high-waterをbounded診断し、固定slotとdefer契約を設計する。満杯時のframe破棄、RAM画像の無所有borrow、GUI同期待機への置換は避ける。既存persistToDisk／generation／invalidation契約を維持する。未実装。

### 2026-10-13: 中断で消えるcase診断の保存境界

- 関連: tools/VideoPreviewBench/main.cpp、run_recorded.py。
- 確認済み: 従来reportは全case終了後にだけ保存され、途中終了時にはfirst-late等のcold snapshotを失う。hydration-bound全matrixはprocess／session消失、reportなし、launch exit nullで終了原因未確認。
- 実装: case境界でQSaveFileに専用schema／complete=falseのcheckpointを保存。runnerは既存partialを起動前拒否。測定中の新規I/Oはなし。未ビルド／runtime未検証。
- 次の確認: case結果とcheckpoint一致、exporterの未完了schema拒否、保存失敗時exit、同exeの周期／終了時のみ診断比較。

### 2026-10-13: RAM合成textureの単一更新slot

- 関連: Artifact/src/Render/PrimitiveRenderer2D.cppm::textureForVideoFrame、CompositionRenderControllerのRAM Preview Composite。
- 確認済み: texture cache keyはowner IDと画像寸法。frame内容は同一textureへUpdateTextureし、前frameと同じtextureを再利用する。RAM合成でもこの経路を使う。
- 仮説（未検証）: GPUが前frameを参照中の更新がsubmission／resource transitionの待ちや揺れに関係する可能性。CPU upload profileだけではGPU依存待ちを断定できない。
- 次の確認: Diligentの所有laneでUpdateTextureとdrawの順序、GPU completionとslot寿命を測定する。固定複数slotは明示completion／総bytes上限／cache invalidationを含めて検討し、単に輪番上書きして安全と見なさない。現在のABBA中はrendererを変更しない。

### 2026-10-13: firstLateQueuedMsはGUI queue単独ではない

- 関連: ArtifactVideoLayer.cppm::markRenderQueued／markFrameCompositionCacheReady、CompositionRenderController.cppmのRAM経路。
- 確認済み: requestedAtからmarkRenderQueuedまでを計測し、cacheReadyはtimestampを保持する。GUI callback開始より前後の処理を含み得る。
- 懸念: 155msをGUI queue単独の停滞と断定すると誤った最適化へ進む。
- 次の確認: frame identity付きdispatch待ちとpre-draw各stageを照合する。現時点では要求→描画開始の合計のみ確認。

### 2026-10-13: 再生tickerのidle GUI投稿

- 関連: CompositionRenderController.cppm::compositionRenderDispatchIntervalMs／PreciseTicker callback／markRenderDirty。
- 確認済み: 再生中interval1ms、workerはpendingをcoalesceするがdirtyを確認せずGUIへ投稿し、GUI側でdirty=falseならreturnする。worker維持修正により毎frameの再生成は避けたが、idle GUI投稿は継続する。
- 懸念（未計測）: callback処理とQt queued event確保がフレーム間でも発生する。今回の遅延原因とは断定しない。
- 次の確認: resize ghostの継続描画、interaction timeout、停止／hidden／同期gateの責務。workerからGUI専用状態を読まず、idle投稿だけ減らせるatomic demand契約を検討する。単純dirty gateはこれらの機能を壊し得るため未実装。

### 2026-10-13 — 再生中のRAM captureに生成キュー先頭制約

- **関連:** `ArtifactCompositionRenderController::PreviewFrameRequest`／`publishPreviewFrameRequestResult()`、部分cache bench。
- **事実:** 描画frameと生成キュー先頭の一致を投入時と完了時に要求していた。部分準備30frame／10秒試行で提示141、合成cacheは30frameのまま。
- **修正:** 再生中だけ描画frameをtargetとし、先頭との一致を要求しない。世代・composition・target identity・pending membershipは保持。paused warmupは同じ制約を保持。
- **価値／懸念:** 先頭の待ちが後続captureを止める条件を除去する。生成増加と性能への影響は未検証。新たに進むreadback／保存量は既存の容量制限との関係を確認する必要がある。
- **次に確認:** 同条件prefix試行でcache増加を確認し、例外・GPU debug layer・lateを別々に判定する。
- **追加確認:** 修正後もRAM30のまま。nativeでframe30／120のpendingBuild=falseを確認。bench prefix設定がradius30へ縮め、play()のprewarmで生成範囲まで30に戻ることが実試行の直接原因。prefix準備数とradiusを分離。先頭制約の修正だけではこの試行の停止を解決しなかった。

### 2026-10-13 — live captureのQt境界変換重複

- **関連:** `ArtifactPlaybackService::Impl::storeRamPreviewFrameImage(QImage)`、`storeFrameImageInRam()`。
- **事実:** RAM取り込みでimageToCpuPreviewFrameを呼び、disk task構築でも同じQImageを再変換していた。diskなしprefix試行も提示94／late64で性能不合格。
- **変更:** Qt境界の変換を1回にしCPU版store overloadでRAMとwriterの所有を扱う。既存copy契約は維持。修正後prefix試行は提示48／late19／drop1で、提示47から速度改善は確認できず。
- **懸念／次に確認:** GUI上のRAM画像cloneとwriter queue mutexのPNG処理待ちが残る。単独効果を実測し、容量と世代を守った非同期引き渡しを検討する。

### 2026-10-13 — native readback poolのleaseはGUI受け取りまで必要

- **関連:** ArtifactIRenderer async readback、Composition controllerのcache publication。
- **事実:** staging fence完了だけでslotを再利用すると、GUI待ちのnative byte payloadが上書きされ得る。旧QImage経路は独立pixel storageの所有でこれを回避していた。
- **実装:** 既存ImageUploadBufferを3slotに冷間準備し、SharedPtr useCountでGUI受け取りまで再利用を拒否。worker／GUI／renderer破棄をまたぐbyte storageの寿命を共有所有で維持する。1slot64MiB上限。
- **価値／懸念:** readback→RAM cache間のQt画像境界を除去するが、GPU→CPUとCPU cacheのコピーは残る。通常prefixは提示47／drop17で不合格。最新版native試行はexception0だが提示47／drop1で不合格。色alpha parityは未検証。
- **次に確認:** 通常prefix試行、native exception、payload色／alphaの旧経路との一致、GPU resident cacheのidentityとbyte budget。


## 2026-10-13 — RAM replay の GPU residency と cache budget

- 確認済み事実: PrimitiveRenderer2D の video upload は owner と寸法ごとの単一 texture を上書きする。既存 sprite/mask texture LRU は 50 entry / 合計512 MiB。controller の RAM replay は CPU cache があってもこの upload を使っていた。
- 関連: Artifact/src/Render/PrimitiveRenderer2D.cppm、Artifact/src/Widgets/Render/ArtifactCompositionRenderController.cppm。
- 今回の範囲: cold RAM publication 後の32 owner warmupと、内容一致のみを返す resident lookup。cache eviction 時は既存経路。総 budget は増やさない。
- 未検証の仮説: 大きい preview と90 frame以上のループは budget不足により residency改善が限定的になる。GPU-only capture と memory policy は別途寿命・編集無効化を確認する必要がある。
- 次に確認: 実際の resident hit / upload 回避、例外、同条件の late/drop、RGBA parity。短いloopの成功を長尺生成の成功と扱わない。

### 2026-10-14: Principled material controls and maps (continuation record)

- **関連:** `ArtifactCore::Material`、`Artifact3DModelLayer`、Raster `MeshRenderer`、RT `RayTracingManager`。
- **確認済み事実:** 既存作業でAO／base color／metallic-roughness／normal／emission map、ray-cone mip選択、Raster multi-material slot map、Masked shadow alpha、異方性、Burley diffuse、coat／sheen層を順次接続している。
- **変更:** ここまでの作業記録が失われたため、セッション要約から確認できる直近機能を集約して復旧した。独立Coat Normal、Diffuse Roughness、float Base Color transfer encoding、alpha-aware odd-size mip filterを含む。
- **価値／懸念:** 実装経緯の要点を残す。詳細な過去の検証条件は元記録から完全復元できず、GPU compile／runtime parityも未確認。
- **次に確認:** 主要map・material controlsをRaster／RTでcompileし、texture LOD・alpha mode・normal variance・energy consistencyを代表fixtureで評価する。

### 2026-10-14: ufbxのPrincipled拡張scalar import

- **関連:** `ArtifactCore::MeshImporter::detectTexturesFromUfbx()` と `Artifact3DModelLayer::setModelSource()`。
- **確認済み事実:** ufbxの `ufbx_material_pbr_maps` は coat factor／roughness、transmission factor、specular IOR、sheen factor／roughnessを保持する。既存importはmetallic／roughness scalarと基本texture mapのみLayerへ渡していた。
- **変更:** ufbxの最初に明示されたscalar値をImporter getterへ保持し、single-material 3D layer importで対応値が既定値のままならMaterialへ反映する。
- **価値／懸念:** extension scalarを手作業で再設定せず、既存Principled shadingへ取り込める。multi-material meshへのper-slot配布、sheen color、extension texture mapは未対応で、glTF fixture/runtimeは未確認。
- **次に確認:** `KHR_materials_clearcoat`／`transmission`／`ior`／`sheen` fixtureで値の取得、Layer Materialへの適用、再保存後の値を確認する。必要ならper-slot UFBX material mappingを別段階で設計する。

### 2026-10-14: UFBX / glTF multi-material partsをmesh slotへ接続

- **関連:** `ArtifactCore/src/Geometry/MeshImporter.cppm`、`Artifact/src/Layer/Artifact3DModelLayer.cppm`、既存Raster／RT multi-material paths。
- **確認済み事実:** ufbxは`material_part_usage_order`と各partの`face_indices`、`num_triangles`を提供し、face単位のmaterial割当てを構築している。`Mesh::generateRenderData()`はpolygonを挿入順にfan triangulationする。
- **変更:** 全UFBX materialのPrincipled factors、tints、texture pathsを取得し、material別face順とindex rangesを生成。Layer slot Materialsを既存Raster range／RT triangle slotへ渡す。texture-only base color mapでは無指定color factorを誤適用しない。
- **価値／懸念:** glTF/FBXで複数素材の色と質感が一つのflatten mesh内でも保持される。実build／fixtureでufbx material table index整合、UV、alpha blending、Raster/RT描画は未確認。node instance overrideやalpha mask semanticsも未対応。
- **次に確認:** multi-part glTFとFBX fixtureでslot range、base color／extension factor／texture、RT primitive mappingを照合し、build後にRaster／RT画像を確認する。

### 2026-10-14: Multi-material Layerの再読込でimport設定を保持

- **関連:** `Artifact/src/Layer/Artifact3DModelLayer.cppm` のJSON serializationとmulti-material draw。
- **確認済み事実:** 複数source materialはLayer内の`meshMaterials_`に保持され、draw時にRaster／RTへ渡る一方、既存JSONは単一の`material_`だけを保存していた。
- **変更:** `material.slots`配列へ各slotのsource name、主要Principled係数、tint、alpha state、emission、texture pathを保存。source再import後に未使用slotを名前優先で照合し、名前がない／重複する場合は保存順へfallbackする。旧JSONはimport元の値を使う。
- **価値／懸念:** material tableの並びが変化しても一意名のslotなら設定を追跡できる。duplicate names、slot独立UV、UI編集は未対応。round-trip未実行。
- **次に確認:** 一意名と重複名を含むmulti-part fixtureでtoJson/fromJson後の全scalar・path一致を確認し、重複名の安定識別子を追加する。

### 2026-10-14: UFBX material slotのopacity・coat tint・emissionをimport

- **関連:** `ufbx_material_pbr_maps`、`MeshImporter::ImportedPbrMaterial`、`Artifact3DLayer` multi-material slot creation。
- **確認済み事実:** ufbx PBR mapにはopacity factor、coat color、emission color／factor、thin-film thickness／IORがある。slot pathはopacity textureを取り込んでいたが、これらのscalar／color値をMaterialへ転送していなかった。
- **変更:** 明示されたopacity factorをslot Materialへ設定し、1未満ならBlended alpha modeを指定する。coat colorとemission colorはlinear-to-sRGB変換し、emission factorとthin-film thickness／IORも各Material設定へ渡す。
- **価値／懸念:** 対応可能な透明度・coat色・発光・iridescence設定をmulti-material pathで保つ。ufbx経路からalpha mask cutoff／mask-vs-blendの別は復元しない。実データと描画はruntime未確認。
- **次に確認:** 不透明・半透明・opacity texture、着色coat、emission、thin-filmを含むfixtureをimportしてslot値とRaster／RT描画を比較する。

### 2026-10-14: UFBX multi-materialのPrincipled係数を追加import

- **関連:** `ufbx_material_pbr_maps`、`MeshImporter::ImportedPbrMaterial`、`Artifact3DLayer` slot construction。
- **確認済み事実:** ufbx PBR material mapにdiffuse roughness、coat IOR、specular anisotropy、subsurface factor/color/radius/anisotropyがあり、現在のMaterial APIおよびslot Inspectorには対応setter／propertyがあるが、Importer tableから転送されていなかった。
- **変更:** 値が明示されている場合だけ上記係数をImportedPbrMaterialへ取り込み、multi-material slot Materialへ適用する。subsurface RGB colorは既存のlinear-to-sRGB変換を使い、radiusとscalarはMaterialの許容範囲へ収める。
- **価値／懸念:** 対応するUFBX／glTF材質情報がslot作成時に失われず、すでに実装済みのPrincipled編集・描画経路へ届く。fixture import、JSON保存／復元、描画はruntime未確認。
- **次に確認:** 各係数が明示されたmulti-material FBX／glTF fixtureを読み込み、slot値と再読込後の値、Raster／RT応答を比較する。

### 2026-10-14: Material slot textureをファイルダイアログから選択

- **関連:** `ArtifactPropertyEditorShared.cppm` のPath property判定とfile dialog filter、3D material slot texture properties。
- **確認済み事実:** material slot texture path propertiesはString editorになり、既存のPath editor（browse button）へ振り分けられていなかった。
- **変更:** `material.slots.*.*Texture`をPath propertyとして認識し、既存Path editorを使う。file dialogは画像形式のfilterを表示し、All Filesも選択可能にする。
- **価値／懸念:** material slotへtextureを割り当てるために絶対パスを手入力する必要をなくす。ダイアログ操作と選択後のtexture reloadはruntime未確認。
- **次に確認:** 7 mapそれぞれで既存texture差替え／新規割当／取消を行い、Property表示とRaster／RT反映を確かめる。

### 2026-10-14: UFBX material slotのtexture／UVを編集可能にする

- **関連:** `Artifact3DLayer::getLayerPropertyGroups()`、slot property dispatch、`material.slots` restore。
- **確認済み事実:** 各slotは7種のPBR texture pathとmap別scale／rotation／offset／UV0-UV1をすでに保持し、JSONへ保存していたが、slot Inspectorから編集できなかった。また復元時に空pathを無視するため、textureを外した状態を保存しても再読込時に消去されない。
- **変更:** Base Color／Metallic-Roughness／Normal／Coat Normal／Emission／Occlusion／Opacity mapごとにpathとmap別UV設定をPropertyへ公開。該当Material setterへ反映し、JSON keyが存在する場合は空pathも明示的に復元する。
- **価値／懸念:** imported multi-materialのmap割当とUV調整をslot単位で行い、textureを外す操作も保存可能にする。UI操作・texture再bind・再読込・Raster／RT描画は実行確認していない。
- **次に確認:** map pathの差替え／クリア、mapごとのUV0/1・transform編集後に保存し、再読込後のProperty値とRaster／RT sampleを確認する。

### 2026-10-14: UFBX material slotのPrincipled項目を拡張

- **関連:** `Artifact3DLayer::meshMaterials_` のProperty、JSON保存／復元、Raster／RT multi-material経路。
- **確認済み事実:** material slot編集面は基本Principled値だけを表示し、Materialとsingle-layer propertyにあるthin film、clearcoat tint／IOR、subsurface、anisotropy、emission、alpha、normal／AO、environment、shared UV設定がslot単位で調整できなかった。slot JSONも同じ高度項目を保存していなかった。
- **変更:** slotごとに上記のscalar／color controlsを公開し、編集値を各slot Materialへ反映する。`material.slots`へ高度項目とshared UV transformを追加し、範囲検証付きで復元する。
- **価値／懸念:** imported multi-material assetsのPrincipled設定を編集・保存できる範囲が単一Material property面に近づく。UI操作、save/reload、Raster／RT描画は実行確認していない。
- **次に確認:** user-facing editorでalpha modeや色を含むslot値を変更し、保存／再読込後に各mesh partの描画が保たれることを確認する。

### 2026-10-14: UFBX textureごとのUV transformをPBR slotへ渡す

- **関連:** `ArtifactCore::MeshImporter::ImportedPbrMaterial` と `Artifact3DLayer::meshMaterials_`。
- **確認済み事実:** UFBX `ufbx_texture` はUV set nameとUV transform（translation、quaternion rotation、scale）を提供する。Materialにはmap別のrotation／offset／UV setとpositive uniform scale APIがあり、Raster／RT samplingで使用する。
- **変更:** 7つのPBR mapについてUV metadataをImporterからLayerへ渡し、名前一致でUV0／UV1を選び、既存map別Material APIへ適用。`material.slots` JSONにもmap別の解決済み設定を保存・復元する。
- **価値／懸念:** glTF `KHR_texture_transform`相当の回転／offset／uniform scaleとUV1指定をslot別textureに保ちやすい。nonuniform／negative scale、3番目以降のUV set、複数meshでUV set名が異なる素材、座標変換向きの実画像検証は未対応／未確認。
- **次に確認:** UV0／UV1とrotation／offset／uniform scaleが異なる複数material fixtureをRaster／RTで描画し、UFBXのtransform方向がglTF表示と一致するか比較する。

### 2026-10-14: UFBX material slotにPrincipled編集面を追加

- **関連:** `Artifact3DLayer::getLayerPropertyGroups()`、`setLayerPropertyValue()`、Raster／RT multi-material draw。
- **確認済み事実:** importerとdrawはslotごとの`ArtifactCore::Material`を保持・描画し、layer JSONにもslot値を保存しているが、通常のProperty面は単一`material_`だけを編集していた。
- **変更:** multi-material UFBX/glTF layerごとにBase Color、Metallic、Roughness、Specular/Tint、IOR、Transmission、Clearcoat/Roughness、Sheen/Roughness/Tintのslot property groupを生成。編集時はslot Materialを更新し既存layer changed通知を使う。
- **価値／懸念:** DCCのmaterial-slot単位で主要Principled値を調整でき、描画・保存経路に直結する。runtime widget behavior未確認。map/UV/alpha/emissionと高度パラメータはslot UI未対応。
- **次に確認:** Material Slot property editingが正しいindexのRaster／RT drawだけを更新し、プロジェクト再読込後も同じ値を保持することを実画面で確認する。

### 2026-10-14: PBR base-color mipの透明縁と奇数寸法を保持

- **関連:** Raster `buildRgba8MipChain` とRT `loadRgbaPbrTexture` のtexture mip構築。
- **確認済み事実:** 固定2x2または整数binの縮小では奇数幅／高さの端texelが落ちるか重みが偏る。透明base-color texelのRGBをalphaに関係なく平均すると、縮小時に色bleedし得る。
- **変更:** source／destination texel矩形の重なり面積で平均し、base-colorはalpha重み付きlinear RGB、alphaは面積平均にする。normal vectorの平均長alphaも維持する。
- **価値／懸念:** 透明縁と不均等縮小の問題を抑える。alphaをopacityに使わない素材では色平均へ影響する可能性があり、実画素未確認。
- **次に確認:** 奇数寸法と透明色edgeのmip readbackをRaster／RTで比較する。

### 2026-10-14: Principled sheen IBLへAOを適用

- **関連:** Raster `MeshRenderer.cppm` とRT `RayTracingManager.cppm` のsheen environment。
- **確認済み事実:** AO mapはdiffuse IBLとbase specularの遮蔽に使う一方、sheen environmentには使われていなかった。
- **変更:** sheen roughnessとview cosineに既存roughness-aware specular-occlusion近似を適用する。AO=1なら従来応答を保つ。
- **価値／懸念:** 凹部でsheenだけが残る不整合を抑える。Charlie sheenにGGX由来scalar近似を使うため、物理積分ではなく画素検証も未実施。
- **次に確認:** AO／roughness sweepとRaster／RT parityを確認する。

### 2026-10-14: Normal Mapへ独立した追加UV回転を追加

- **関連:** `ArtifactCore::Material`、`Artifact3DModelLayer`、Raster `MeshRenderer`、RT `RayTracingManager`。
- **確認済み事実:** Materialの既存offset／scale／rotationは全texture共通だった。normalとcoat-normalは別texture pathを持つが、どちらも同じ共通UV座標でsampleされ、Rasterのderivative tangent basisとRT triangle tangent basisもそのUVに一致していた。
- **変更:** 共通UV transform後にNormal Map専用の追加rotationをMaterial／JSON／Property／material signature／single・multi-material Raster／RTへ追加した。normal sample座標を回し、RTはtangent／bitangent basisにも同角度を適用する。Coat Normalは同じ追加回転を使う。
- **価値／懸念:** albedo／roughness textureの位置・方向を変えずnormal detailだけを向け直せる。後続変更でcoat-normal専用rotation／scaleとmap別UV set選択も追加した。shader compileと画像parityは未確認。
- **次に確認:** 0度で旧応答を保つこと、90度で+U normal detailが+Vへ回ること、mirrored UV／non-uniform scale、normal／coat-normal併用をRaster／RTで比較する。

### 2026-10-14: Normal Map専用UV scaleとray-cone mip連動

- **関連:** `ArtifactCore::Material`、`Artifact3DModelLayer`、Raster `MeshRenderer`、RT `RayTracingManager`。
- **確認済み事実:** Normal Mapには共通UV変換後の独立rotationを追加済みだが、texture密度はbase color等の共通scaleを共有していた。Raster LODはnormal UVのimplicit derivative、RT LODはtriangle UV edgeとray coneから計算する。
- **変更:** 0.01〜10のuniform Normal UV ScaleをMaterial／Property／JSON／signature／single・multi Raster／RTへ接続した。normalとcoat-normalのUVを中心基準で拡縮し、RT ray-cone LOD計算にもscaleを反映する。
- **価値／懸念:** base colorやmetallic-roughnessのtile密度を保ちながらnormal detailの大きさを調整できる。後続変更でcoat-normal専用scale／rotationとUV set選択を追加した。shader compileとmip image parityは未確認。
- **次に確認:** scale 0.5／1／2で模様密度が対応して変化し、RT LODもtexture footprintに応じて上がること、rotationとの合成とRaster／RT方向一致を確認する。

### 2026-10-14: Coat Normal専用UV rotation／scale

- **関連:** `ArtifactCore::Material`、`Artifact3DModelLayer`、Raster `MeshRenderer`、RT `RayTracingManager`。
- **確認済み事実:** 独立Coat Normal textureは別pathでbindされていたが、sampling UV、tangent basis、mip scaleはbase Normal Mapと同じmappingを共有していた。
- **変更:** Coat Normal専用のrotationとuniform scaleをMaterial／JSON／Property／material signature／single・multi Raster／RTへ追加。Rasterは専用UV derivativeからnormalを再構成し、RTはcoat tangent frameを回転しray-cone LODへscaleを適用する。default 0度／1倍では従来と同じmapping。
- **価値／懸念:** 塗膜のnormal detailを基材のnormal textureと独立に向き・密度・UV set調整できる。shader compilationとRaster／RT画像parityは未確認。
- **次に確認:** base／coat normalの角度・密度を別々に変更し、coat highlightとray-cone mip応答がRaster／RTで一致することを確認する。

### 2026-10-14: Coat Normal専用UV rotation／scale

- **関連:** `ArtifactCore::Material`、`Artifact3DModelLayer`、Raster `MeshRenderer`、RT `RayTracingManager`。
- **確認済み事実:** Coat Normalは別texture pathでbindされていたが、sample UV、tangent basis、ray-cone LODはNormal Mapの共通mappingを共有していた。
- **変更:** Coat Normal専用rotationとuniform scaleをMaterial／JSON／Property／signature／single・multi Raster／RTへ接続。Rasterはcoat UV derivativeからbasisを再構成し、RTはcoat tangent basisとray-cone LODへ専用変換を適用する。
- **価値／懸念:** 塗膜のnormal detailを基材と独立して向き・密度調整できる。offsetとUV setの個別選択は未対応。shader compileとRaster／RT parityは未確認。
- **次に確認:** base／coat mapへ異なる方向・密度のfixtureを使い、coat highlightの変化とRaster／RT一致を比較する。

### 2026-10-14: Raster異方性接線へNormal Map UV rotationを反映

- **関連:** `ArtifactCore/src/Graphics/MeshRenderer.cppm` のRaster mesh PBR shader、RT `RayTracingManager.cppm` のtangent frame。
- **確認済み事実:** RTはNormal Map専用rotationをnormal tangent／bitangentへ反映するが、Raster異方性軸の微分は通常 `meshUv` から求めていた。Raster normal map自体は専用 `normalMapUv` を使っていた。
- **変更:** Raster異方性接線のUV微分を `normalMapUv` から求め、Normal Map専用rotation／scaleを接線方向にも反映した。
- **価値／懸念:** normal map rotationと異方性の組合せでRaster／RTのハイライト方向が食い違う可能性を減らす。画像によるparityは未確認。
- **次に確認:** mirrored UVを含むメッシュでNormal Map rotationとAnisotropy Rotationを個別・複合変更し、Raster／RT方向を比較する。

### 2026-10-14: Raster normal mappingの接線基底をRTと直交化

- **関連:** Raster `computeNormalMapped()` とRT `RTWarmup_ClosestHit` の微分／triangle接線基底。
- **確認済み事実:** Rasterは画面微分からTとBを独立に正規化し、補間法線への投影や直交化を行っていなかった。RTはTを補間法線へ投影し、UV handednessを保ってBをcross積から再構成していた。
- **変更:** RasterのNormal／Coat Normal用基底をRTと同じ投影・符号付き再構成にそろえ、退化Tでは元の法線へfallbackする。
- **価値／懸念:** 滑らかな法線でnormal detailが法線方向へ混ざる誤差と、Raster／RT間の異なる基底を抑える。画像parityは未確認。
- **次に確認:** 球面のsmooth normal、mirrored UV、normal strength 0／1、coat normal併用で各経路の法線／ハイライトを比較する。

### 2026-10-14: Clearcoatをsheen・基材反射・透過の上層として適用

- **関連:** Raster `MeshRenderer.cppm` とRT `RayTracingManager.cppm` のdirect／IBL material layering。
- **確認済み事実:** マイルストーン本文はcoatがsheenの上層と記録している一方、direct／IBL sheenと透過環境光はcoat energyで減衰していなかった。base diffuse／specularとEmissionは既にcoatによる減衰を受けていた。またRaster／RTのback-light transmissionは、`NdotL <= 0`でループを抜ける前にしか評価されず到達不能だった。
- **変更:** direct／IBL sheen、IBL transmissionへclearcoat energyを適用し、Raster／RTのback-light transmissionを裏面光分岐へ移してview-side coat energyを適用した。RTはPrincipled IOR／thin-film Fresnel、view-ray TIR、および裏面向きshadow-ray originを使う。status記述も本文と一致させた。
- **価値／懸念:** clearcoat有効時にsheenや環境透過が最上層coatを突き抜ける見え方を抑える。Raster／RTの背面直接光transmissionが意図した条件で寄与する。RTのsoft／area shadowとsurface parityは未確認で、視覚検証とshader compilationも未実施。
- **次に確認:** coat weight／IOR／tintを変え、sheen・透過の減衰とRaster／RT一致を確認する。

### 2026-10-14: ufbx UV1をMesh RenderDataまで保持

- **関連:** `MeshImporter.cppm`、`Mesh::RenderData`、`Mesh::generateRenderData()`、`generateMeshletLODData()`。
- **確認済み事実:** ufbx meshは`uv_sets`配列で複数の`vertex_uv`属性を持つが、importerは`vertex_uv`（UV0）だけを読み、RenderDataとmeshlet remapにもUV1の格納先がなかった。
- **変更:** 2番目のufbx UV setを`uv1` vertex attributeへ読み、RenderDataの`uvs1`にtriangle flattenする。mesh内の一部だけUV1がない場合はUV0を複製する。meshlet remapにもUV1を含めた。Base Color／Metallic-Roughness／Normal／Coat Normal／Occlusion／Emission／OpacityごとにUV Set（0／1）をMaterial／Layer JSON／Propertyへ追加し、Raster／RT samplingへ伝搬した。Masked shadow alpha clipとRT Any HitにもBase Color／Opacityの選択UVを通す。RTは各選択UVの微分でray-cone mip密度を計算し、Normal／Coat Normalは別々の接線基底を使う。
- **価値／懸念:** すべての現在のPBR texture mapでUV1をRaster／RT共通に選択できる。shader compilation／画像 parityは未確認。
- **次に確認:** 複数UVを含むFBX／glTF fixtureをRaster／RTで描画し、各map選択、masked shadow境界、normal detail、mip応答、material serialization、parityを確認する。

### 2026-10-14: OBJ/MTL material slotをPrincipled tableへ接続

- **関連:** `ArtifactCore::MeshImporter::detectTexturesFromTinyObj()`／`loadWithTinyObj()`、`Artifact3DLayer::loadFromFile()`、既存Raster／RT material slot範囲描画。
- **確認済み事実:** OBJ importerはMTL texture pathだけを拾い、geometryにmaterial slot rangeを作っていなかった。tinyobjloaderのMTL parserはmaterial IDごとのdiffuse／specular／emission、PBR metallic／roughness／clearcoat／sheen／anisotropy、dissolve／IOR、texture optionsを保持し、`Mesh::MaterialSlot`とUFBX向けPrincipled tableの適用経路は既存である。
- **変更:** MTL tableをslot用Principled materialへ変換し、面material IDから隣接triangle rangeを作る。Phong shininessからroughnessへ近似変換し、MTL anisotropy rotationのturn値を度に変換する。後続変更で単一material tableもlayer本体Materialへ適用し、single／multiで変換処理を共有した。
- **価値／懸念:** 複数材質を持つOBJが単一の全体materialではなく、MTLで定義された面単位の外観とRaster／RT slot割当を保持できる。tinyobjloaderが`Kd`未指定と明示的な黒を区別しないため、全0 diffuseはwhite fallbackにする。roughnessとmetallicの別々のMTL textureは既存packed map契約に合わせて片方だけを選ぶ制限があり、rotation単位・色空間・画像応答をruntimeで未検証。
- **次に確認:** build禁止解除後に単一／複数材質OBJ/MTL fixtureでmaterial ID範囲、色／roughness／metallic／透明、texture transform、保存再読込、Raster／RT一致を検証する。独立roughness／metallic textureは後続課題。

### 2026-10-14: MTL roughness／metallic mapはpacked mapと分離する

- **関連:** `tinyobj::material_t::roughness_texname`／`metallic_texname`、`ArtifactCore::Material`、Raster `MeshRenderer`、RT `RayTracingManager`。
- **確認済み事実:** 現行shaderのMetallic-Roughness入力は1枚のpacked textureを前提にし、G channelをroughness、B channelをmetallicとして読む。tinyobjloaderはMTLの`map_Pr`と`map_Pm`を別々のpathで保持する。片方をpacked入力に代入すると他方のchannelも同時に誤って乗算される。
- **変更:** Core Material／OBJ importerでpathとUV transformを分離し、Raster single／slotとRT material tableへmetallic・roughness別map／独立UV transformを接続した。両経路で各mapのR channelを対応factorへ乗算し、独立mapがある場合はpacked map samplingを止める。RTでは変換後UV edgeからray-cone mipを選ぶ。3D layerのsingle／slot JSON、Inspector、render signatureにも接続した。
- **価値／懸念:** OBJ/MTLの独立mapとmap固有 scale／offset／rotation／UV setがRaster／RTの描画データ経路に届く。shader compile、build、実機描画およびRaster／RT visual parityは未確認。
- **次に確認:** ビルド・実機確認が許可された段階でRT shader ABI／samplingとRaster／RT表示を検証し、次のPrincipled差分を進める。

### 2026-10-14: 異方性IBLに対称な長軸方向tapを追加

- **関連:** Raster `MeshRenderer.cppm` とRT `RayTracingManager.cppm` のPrincipled base-specular IBL。
- **確認済み事実:** 両経路は異方性で反射方向を曲げていたが、等方GGX prefilterを単一方向から読むため、伸長したGGX反射ローブを直接表現していなかった。
- **変更:** 異方性の符号から長軸接線を選び、環境回転後の反射方向に沿って投影した軸の正負へ2 tapを追加する。roughness mipとBRDF LUT roughnessを長軸／短軸ごとに分け、中央と対称tapを混合する。異方性0は単一sampleの既存経路を保つ。
- **価値／懸念:** 専用異方性prefilter textureや新しいGPU resourceを増やさず、Raster／RTで方向性のある環境ハイライトを近似できる。等方prefilter由来の近似であり、画像品質・energy bias・GPU costは未検証。
- **次に確認:** 方向性HDR、異方性 -1／0／+1、rotation 0／90度、roughness 0.1／0.5／0.9でハイライト幅とRaster／RT pixel responseを比較し、tap weightingを調整する。

### 2026-10-14: Clearcoat factor textureをPrincipled経路へ接続

- **関連:** `ArtifactCore::MeshImporter`／`Material`／Raster `MeshRenderer`／RT `RayTracingManager`、`Artifact3DModelLayer`。
- **確認済み事実:** UFBX material APIに`pbr.coat_factor.texture`があり、Material・Importer・single／slot JSON／Inspector・Raster・RTへpathとUV transformを接続した。RasterとRTのclearcoat weightはtexture R channelをscalarへ乗算する。
- **価値／懸念:** coat maskで塗膜量を面内制御できる。textureはlinear扱い、UV setは0／1のみ。shader compile／GPU runtime／Raster・RT parityは未確認。
- **次に確認:** shaderをビルドし、UV0／UV1とslot再bindを含むclearcoat mask fixtureでRaster／RTの画素結果を比較する。

### 2026-10-14: Clearcoat roughness mapのRaster／RT ABIを拡張

- **関連:** `MeshRenderer::MaterialState`、`RTMaterial`、clearcoat roughness import／slot binding。
- **確認済み事実:** Raster material cbufferとRT structured bufferはいずれもCPU側の構造体配列順・float4数をHLSL側と一致させる必要がある。新mapはMaterial・Importer・slot/render経路へstateを追加する。
- **価値／懸念:** shader ABIを静的assertと並び確認で保ちつつ、clearcoat roughness mapをRaster／RT両方で利用できる。実際のshader compilation／runtime ABIは未検証。
- **次に確認:** ビルド許可後にcompileし、単一材質／複数slotのtexture切替とRT material packetを検証する。

### 2026-10-14: Sheen色mapは材質別のsRGB pathとUV変換を持つ必要がある

- **関連:** `ArtifactCore::Material`、`MeshImporter`、Raster `MeshRenderer`、RT `RayTracingManager`、`Artifact3DLayer`。
- **確認済み事実:** 既存Sheen Tintは定数RGBのみで、UFBXの`pbr.sheen_color.texture`を保持するMaterial値もRaster／RT texture入力もなかった。clearcoat、emissionなど各mapの実装に、single／material slot共通のpath・UV・cache・shader経路がある。
- **変更:** Sheen Color mapのMaterial API、UFBX path／UV取得、single／slot JSONとInspector、Raster／RT GPUデータおよびshader sampleを追加した。RT texture mipは変換後UVからray-coneで選択する。
- **価値／懸念:** 色の異なる繊維・布のSheenをテクスチャで制御できる。HLSL ABI、sRGB mip、slot別resource切替、UFBX texture transformは実ビルド・runtimeでは未確認。
- **次に確認:** build許可後にshader compileを行い、UFBX sheen color fixtureでsingle／slot、UV0／UV1、save/reload、Raster／RT pixel responseを確認する。

### 2026-10-14: Sheen Roughness mapもsingle／slotとRaster／RTで同じUV契約を使う

- **関連:** `ArtifactCore::Material`、UFBX material import、Raster `MeshRenderer::MaterialState`、RT `RTMaterial`、3D layer JSON／Inspector。
- **確認済み事実:** Sheen Roughnessはscalarだけで、map入力がなく、Sheen Color mapの後でもroughness scalarだけがCharlie lobeとIBLへ渡っていた。
- **変更:** map pathとUV transformをsingle／slotへ追加し、linear R channelをscalar roughnessに乗算するRaster／RT経路を追加した。RTは変換後UVのray-cone mipを使用する。
- **価値／懸念:** 布などのSheen lobe幅を面内で制御できる。Raster cbufferとRT structured materialのfloat4 ABIを拡張したが、shader compile／GPU runtimeは未検証。
- **次に確認:** ビルド許可後に両backend shader compile、UFBX texture fixture、single／slot rebinding、UV0／UV1、save/reload、Raster／RT pixel parityを確認する。

### 2026-10-14: Anisotropy mapはglTFのlinear RGB packingを使う

- **関連:** `ArtifactCore/src/Graphics/MeshRenderer.cppm`、`ArtifactCore/src/Graphics/RayTracingManager.cppm`、`Artifact/src/Layer/Artifact3DModelLayer.cppm`。
- **確認済み事実:** Khronos `KHR_materials_anisotropy` はlinear RGに接線方向、Bに強度を格納し、Scalar strengthへBを乗算する。ufbx現行material mappingに同拡張の項目がないため、`.gltf` JSONから補完する必要がある。
- **変更:** Raster／RTともmapのB値をscalar量へ乗算し、RGを[-1,1]へ展開・正規化して接線基底へ適用。Material回転角をmap方向へ加算する。`.gltf`／`.glb`のJSONからstrength／rotation、外部URI image、`KHR_texture_transform`をImportedPbrMaterialへ抽出する。方向map UVとshading tangent frameを分離した。
- **価値／懸念:** glTF標準異方性mapをRaster／RT directとIBLで利用できる入口ができる。data URI／bufferView埋め込み画像／KTX2とmaterial orderは未確認。Mesh経路にauthored vertex TANGENTがなく、normal UVからの再構築はglTF tangent basisと差が残る。shader compileと画像結果は未検証。
- **次に確認:** KHR glTF AnisotropyRotationTest fixtureでmaterial照合、外部URI、texture transform、single／slot、Raster／RTの方向と強度を確認する。公式fixtureはauthored TANGENT方向と、textureに格納した逆rotationの相殺ケースを含む。将来、mesh tangent attributeをsource／RenderData／GPU／RTへ一貫して保持し、subdivision／deformation後のtangent再構築を定義する。

### 2026-10-14: GLB埋め込み画像はstable asset identityが先

- **関連:** `.glb` embedded imageのImporter path、Material保存、`AssetManager` source registry。
- **確認済み事実:** `ImportedPbrMaterial`とMaterial texture参照はpath文字列で表現され、現在のimport pathにはembedded bytesの論理asset ID／origin identity／save-restore blob契約がない。Asset relocation契約はpathだけでsource identityを決めない方針である。
- **変更:** embedded bytesを一時ファイルや仮想URIへ変換する実装は追加しない。
- **価値／懸念:** project再読込・移動後に壊れる一時参照を避ける。GLB内蔵画像は現状未対応。
- **次に確認:** embedded sourceのstable ID、origin GLBとの関係、project portability、relink後のidentity、content versionを先に定義し、その後texture decoderへ渡す。

### 2026-10-14: Authored tangent導入はUVセット単位で定義する

- **関連:** `third_party/ufbx/ufbx.h` の `ufbx_mesh::vertex_tangent`／`vertex_bitangent`、`ufbx_uv_set`、`ArtifactCore/src/Geometry/MeshImporter.cppm`、`ArtifactCore/src/Mesh/Mesh.cppm`、Raster／RT shading geometry。
- **確認済み事実:** ufbxはmesh primary tangent/bitangentに加えて各UV setの tangent/bitangent を保持できる。glTF `TANGENT` はhandedness付きvec4である一方、現行Mesh RenderData、meshletのPackedVertex、MeshRenderer input、RT shading poolsはtangentを保持しない。Mesh subdivisionは既知属性だけを再生成する。
- **未検証の設計仮説:** 異方性textureがUV1を参照する場合はUV1 basisが必要だが、glTF authored `TANGENT` はprimitiveの標準tangent属性であり、anisotropy `texCoord` overrideだけからそのbasisを選ぶのは正しくない可能性がある。normal map UV、authored tangent、anisotropy direction mapの関係をfixtureで確かめてからMaterial APIを決める。
- **価値／懸念:** vertex tangentを単純に追加するだけでは既存UV1異方性と非一様transform、normal map、deformation／subdivision、Raster/RT parityの契約が曖昧になる。handednessを含む一貫したasset-to-shader経路が必要。
- **次に確認:** Khronos AnisotropyRotationTestとUV1 fixtureを基準に、ufbxで各UV setのbasisが取れる条件、glTF tangentとbitangent再構成、Mesh modifier後の再計算方針を定め、RenderData／GPU／RTを同時に更新する。

### 2026-10-14: RT Anisotropy mapをdirectとIBLで共有

- **関連:** `ArtifactCore/src/Graphics/RayTracingManager.cppm` のclosest-hit shader。
- **確認済み事実:** RTはAnisotropy mapをdirect light loop内でサンプルしていたが、bent reflectionとanisotropic environment lookupはmaterial scalarだけを参照していた。
- **変更:** barycentric UVとray-cone mipで異方性量をヒットごとに一度だけサンプルし、direct・bent normal・IBL reflectionで共有する。
- **価値／懸念:** Rasterと同様にmapがdirect／IBL双方を制御し、ライト数に比例する重複sampleをなくす。shader compile／実機のIBL画像は未確認。
- **次に確認:** ビルド許可後にshaderをcompileし、異方性mapの黒／白部分でRT IBLとRasterの方向／強度を比較する。

### 2026-10-14: FBXアニメーションの受け入れ確認を独立GUIに切り出す

- **関連:** `tests/Artifact/FbxAnimationViewerMain.cpp`、`Artifact3DModelViewer`、`MeshImporter`。
- **確認済み事実:** ImporterはFBX animation clipを取り込み、既存viewerはclip時刻を再評価し、Diligent render windowへmeshを渡すAPIを持つ。プロダクトの大きな起動経路なしにこれらを手で操作するGUIターゲットがなかった。
- **変更:** ArtifactAppRuntimeへリンクする任意ビルドの独立viewerを追加し、FBXを開く方法、再生／停止、clip切り替え、view reset、clip指定、複数フレームcaptureを提供する。再生tickごとのFBX全mesh再importを避け、Viewer内でMeshImporterを保持して`updateSkinPose()`を使う。pose配列を再利用し、CPU fallbackで変形を二重実行せず、UI queueへ再生tickが蓄積しないようcoalesceする。clip時間をstatusに表示する。
- **確認済み事実:** 起動引数で`Standard_Walk.fbx`のclip #1を再生し、1.63秒のclipで再生時刻と歩行姿勢の違う複数のD3D12画面フレームを確認した。修正前は全mesh再import／UI queue蓄積によってViewerが応答なしになったが、修正後は30秒の連続表示中もUIが応答し、再生時刻が進行した。以前使ったY Bot FBXのclip #1は0.03秒だったため検証素材として不適切だった。
- **追加確認:** `Standard_Walk.fbx`にはbase colorとnormalのembedded PNGがあるが、FBX内の文字列pathは配布元の一時Linuxディレクトリを指し、実行環境には存在しない。ufbxのtexture content blobから一時ファイルへ展開すれば既存ImageImporter／Diligent texture upload経路を再利用できる。今回の実行でtemporary cache pathとモデル表示を確認した。
- **追加変更:** ViewerがFBXの先頭PBR材質のbase color、metallic／roughness等と主要texture pathを描画へ渡し、Importerが見つからない外部pathよりembedded imageを優先するようにした。
- **価値／懸念:** プロダクトのプロジェクト設定なしでImporterからviewportまで実FBXアニメーションを確認できる。検証した`Standard_Walk.fbx`は8+ influencesのCPU LBSであり、フレーム時間／負荷とblend shapeを含むFBXのfallback評価は未確認。Qt `QScreen::grabWindow()`がD3D12画面を含められない点は継続し、画面取得ツールでフレームを保存した。
- **次に確認:** 別のFBX構成（GPU 4 influences、blend shape、複数clip）で表示・切替を確認し、CPU fallbackのフレーム時間をprofileする。

### 2026-10-14: FBX歩行姿勢確認は3/4視点を既定にする

- **関連:** `Artifact3DModelViewer::resetView()`、独立FBX animation viewer。
- **確認済み事実:** Reset Viewはyaw／pitchを0°に設定しており、キャラクターを真正面から見る。ユーザーは足の向きが不自然に見えると感じた一方、正面視点とサンプルモデルの分かりにくさも原因候補として挙げた。
- **変更:** Reset Viewの初期角度をyaw 28°／pitch 10°、距離4.0にし、3/4視点を既定にした。
- **確認済み事実:** OpenGameArtはQuaternius `Animated Human Low Poly`をCC0として掲載し、配布ZIP内の`License.txt`もCC0 1.0を記載する。FBXは4,734 vertices、41 bones、9 clipsとして読み込まれ、`Human Armature|Walk` (#7、1.03秒)の複数時刻で左右脚の姿勢変化が見えた。clip #6はRun (0.63秒)。`Y_Bot` #0は歩行ではなく腕を広げた直立姿勢だった。
- **価値／懸念:** 対称なシンプル人型により、装備差のある`Standard_Walk`より足の動きを比べやすくなった。3/4視点で奥行きも確認できる。距離3.2では歩行中に下腿がビューポート端で切れる場合があったため4.0へ戻して最終確認する。歩行品質そのものの正誤判定はground-contact基準なしには断定できない。
- **次に確認:** 距離4.0で歩行中の全身と両足が常時フレーム内に入ることを再取得画像で確かめる。CC0 assetはビルド出力下に保ち、製品へ同梱しない。

### 2026-10-14: 独立FBX viewerの再現手順にruntime DLLとmodule IFC準備が必要

- **関連:** `CMakeLists.txt` の `ArtifactFbxAnimationViewer`、`ArtifactAppRuntime`、`ArtifactCore` module targets、`ArtifactCoreMeshImporterFbxTest`。
- **確認済み事実:** 新しいビルドディレクトリでviewer targetはリンクできたが、実行ファイル単体ではvcpkg runtime DLL不足の終了コード`-1073741515`になった。vcpkgの通常／debug `bin`を子プロセスの`PATH`へ足すとプロセスは20秒間稼働した。Ninjaでは利用側の`ArtifactCoreText`／`ArtifactAppRuntime`が`Shape.Types`／`Memory.SharedPtr` IFCより先に走る失敗を観測し、producer targetを先行ビルドして回避した。
- **価値／懸念:** 別の開発環境でFBXテストループを再現する際、起動時DLL配置とC++ module target順が隠れた前提になる。現状のGUI実行ではモデル読込状態を観測できず、ロード成功を確認した事実ではない。
- **追加対応:** `ArtifactCoreMeshImporterFbxTest` のWindows targetにもPOST_BUILD runtime DLL copyを設定した。module producerの順序依存と他exe targetへの影響は未調査。
- **次に確認:** computer-use native RPCの復旧後に同じviewerを実画面で検証する。その後、CMakeのmodule dependency graphとruntime deployment規約を調べ、最小の再現性改善を別途判断する。

### 2026-10-14: FBX importer回帰テストはfixture供給契約が必要

- **関連:** `tests/ArtifactCore/MeshImporterFbxTest.cpp`、`ARTIFACT_ENABLE_FBX_IMPORT_TEST`。
- **確認済み事実:** FBXのanimation stackとembedded texture bytesは実fixtureなしではImporter API経由の回帰検査ができない。追加したGTestはパスを環境変数で受け取り、CTest登録時はCMake cacheで設定したfixture pathを渡す。テスト有効時はfixture未指定や不存在をconfigure errorにする。
- **未検証の改善仮説:** 小さいCC0のFBXをテスト専用fixtureとして同梱できれば、CIと開発者が追加準備なしで常時テストできる可能性がある。asset size、ライセンス表記、CMake配布先は未調査。
- **価値／懸念:** 外部アセットへの暗黙依存や製品同梱を避けつつ、指定済みFBXのclip評価とembedded PNG展開を自動検査できる。CIではfixture供給とCMake cache設定が必要。
- **次に確認:** ライセンスと容量を確認した最小のanimation＋base-color＋normal FBX fixtureを用意できるか判断し、可能ならfixture checksumと取得・保存方法をテスト契約に固定する。

### 2026-10-14: FBX視覚QA素材は輪郭と色柄を両立させる

- **関連:** `ArtifactFbxAnimationViewer`、FBX animation/texture validation fixture。
- **確認済み事実:** Quaternius Animated Humanは歩行姿勢を読み取りやすい一方で色テクスチャがなく、Mixamo `Standard_Walk.fbx`は色付きtextureを持つが防具と片足装備がシルエット比較を難しくする。OpenGameArtのCC0 `Basic char textured animated`は説明上 diffuse/normal mapとRun/Idle等を含むものの、配布ZIPの内容は`.blend`とPNGのみでFBXを含まない。環境内にBlender実行ファイルも見つからなかった。
- **価値／懸念:** 形状変形とtexture破損を画像認識で同じフレームから判定するには、明瞭な人型輪郭、分かれた色領域、実際のanimationを持つFBXが有効。別形式をQA素材として流用すると、形式変換工程の影響とImporter挙動を混同する。
- **追加変更:** `tools/fbx/analyze_viewer_frames.py`でviewport背景を除いたモデルbbox・edge contact・silhouette IoU・彩度ピクセル率・遠隔foreground component面積を集計し、bbox/maskを重ねた画像とJSONを保存できるようにした。同一pose/timeのknown-good `--reference-image`または入力filenameごとの基準`--reference-dir`と比較するsilhouette IoU・前景BGR MAE・HSV histogram similarityに加え、16x32局所gridのshape mismatchとcolor MAEを追加した。さらに立位人型の下端20%を足先付近として別報告し、annotated preview上に境界線を描画する。`--min-reference-bottom-silhouette-iou`は明示指定時のみ有効にし、解剖学的な向き判定とは区別する。
- **検証:** reference画像を自身と比較すると合格。Standard_Walkのモデル領域をグレースケール化した合成画像は彩度0.0／HSV similarity 0.09797で不合格、モデルの左半分を消した合成欠損画像はreference silhouette IoU 0.60095で不合格となった。別途Standard_Walkの同一Solid viewport capture 3枚で画面端接触0/3、silhouette変化2区間を検出し、画像認識で外形・四肢接続を確認した（textureの証拠には使っていない）。
- **追加検証:** 16x32 gridではモデル上の8 px単色patchはBGR MAE 23.58で通過し、12 px以上は52.73〜121.05で不合格となった。60 px patchでも全体BGR MAE 3.89／HSV similarity 0.86にとどまり、局所指標の必要性を確認した。
- **足領域の対照検証:** 同時刻Standard_Walk基準3枚はbottom-20% silhouette IoU 1.0で3/3合格。JPEG quality 92再圧縮は0.99714／差分13pxで0.80閾値を通過し、足先帯を左右反転した合成差分は0.36048／差分4,272pxで不合格になった。これはROI指標の制御試験で、実モデルの解剖学的な正しさやRangerの描画確認を示すものではない。
- **誤検出確認:** Standard_Walkの同一clip内でも単一referenceを別poseへ適用すると、正常なframe 1枚がIoU 0.77913で不合格になった。同名対応の3枚基準folderに切り替えると3/3枚が合格し、時刻別参照がpose変化による誤検出を抑えることを確認した。
- **確認上の訂正:** 以前のQuaternius walk検証で「足が切れていない」とした仮判断は、保存フレームの画像解析と目視で誤りと分かった。使用したフレームではモデルがviewport下端で切れていた。解析はこの再評価に使った既存画像に限られ、現行実装の再ビルド後確認ではない。
- **価値／懸念:** 固定camera/viewportでのframe crop、無描画、急なsilhouette/area変化を人が全画像を探す前に抽出できる。reference比較はclip時刻・camera・viewportが一致する場合に限り有効。色histogramはtexture配置が変わっても分布が同じなら見逃し得る。彩度はUVの意味的正しさを保証せず、輪郭変化だけではskin崩れと正しい歩行動作を完全に区別しない。
- **確認済み事実:** Quaternius RPG Character Packの`Ranger.fbx`をignoredな`out/build/x64-Debug/fbx-animation-test/quaternius-rpg-character-pack/`へ取得した。配布元`License.txt`はCC0 1.0。FBX binaryに`Foot.L`／`Foot.R`、`CharacterArmature|Walk`／`CharacterArmature|Run`を確認し、SHA-256は`CECC642A3CEDCD319FE0CB07F71985E07C788FE6759EF1CDA2240A52EC1D14A9`。同ファイルからPNG/JPEG signatureや画像拡張子参照は見つからないため、足・歩行のfixture候補に限定し、embedded texture確認には既存`Standard_Walk.fbx`を使う。
- **追加確認:** 既存の`ArtifactFbxAnimationViewer.exe`でRangerを起動すると`Model loaded successfully. Vertices: 7704`を出し、画面statusは14 clips、#13 `CharacterArmature|Walk`、1.25秒、32 bones、ufbx backendを報告した。capture中にstatus時刻は0.26秒から0.86秒へ進んだ。一方、保存された`QScreen::grabWindow()` imageは白いD3D12 viewportで、モデル描画を画像認識できなかった。既存exeの読込・時刻進行確認であり、足向きの視覚QAではない。
- **追加確認:** 2026-10-14にWindowsデスクトップをFFmpeg `gdigrab`で撮影すると、Ranger ViewerのD3D12モデル表示が取得できた。3時刻の画面にwalk姿勢の変化があり、単色silhouetteでも脚と靴状の足形状が見えるフレームがあった。CaptureはCodexウィンドウに覆われ、画面領域の遮蔽と不均一cropがあるため、連続定量比較・接地姿勢の正しさは未確認。
- **CC0テクスチャ付き候補:** Quaternius公式 `Animated Man Pack` ページはCC0、FBX、animated、texturedと説明し、atlas textured maleとmany animationsを明記する。公式Driveフォルダ内のFBXファイルIDを特定したが、Driveのファイルdownload endpointはHTTP 500。Animated Men Packも公式CC0／FBX／Texturedだがfolder endpointがHTTP 400。候補特定はできたがassetの取得・内容・Viewer表示は未確認。
- **別取得候補:** Quaternius公式itch.io `LowPoly Animated Knight`はCC0とWalk／Runを含むanimated FBX ZIP（8 MB）を公開する。itch.ioの無料purchase endpointは通常のdownload CTAを返すが、取得POSTは支払方法エラーとなりZIPを得られなかった。この公式説明はtexture付きとは明記していない。第三者GFX-HUBは700.8 MBの複数packコレクションにMan Animated ZIPを掲載するが、公式配布ではなく、必要な単体assetより大きく出所の検証が弱いため取得に使わない。
- **訂正／次に確認:** ユーザーが意図したCC0モデルは足向き用Rangerだけでなく、当初からテクスチャ付きAnimated Man Packを使うことだった。RangerはCC0だがFBX内にtexture画像が見つからずtexture fixtureにはならない。Animated Man Pack FBXを取得できる公式またはミラー配布経路を確保し、Viewer上でWalkの複数時刻、両足、base color textureを一連で確認する。Importer GTestでは`ARTIFACT_FBX_ANIMATION_TEST_FILE`／`ARTIFACT_FBX_TEXTURE_TEST_FILE`もこの候補で実行可能か確認する。GTestはビルド／実行未確認。

### 2026-10-14: Audio Layerの音声レンダリングをComposition経由でも検証する

- **関連:** `ArtifactAudioLayer::buildWaveformData()`、`ArtifactAudioLayer::getAudio()`、`ArtifactAbstractComposition::getAudio()`、`tests/Artifact/AudioLayerRenderQueueIntegrationTest.cpp`。
- **確認済み事実:** Audio Layerの`draw()`は画像を描かない。Render QueueのWAV分岐はComposition::getAudioを呼ぶ。今回のテスト実行環境では`QT_QPA_PLATFORM=offscreen`に対応するQt pluginがなく、`windows` pluginを指定するとComposition／Mixer評価は短時間で完了した。
- **変更:** トーンWAVのwaveform peak/RMS、layer PCM、Composition出力のvolume半減、Composition上で片方をmuteした場合との重ね合わせを確認する統合テストを追加した。さらにRender ManagerのWAVキューから異なる周波数の2音を含む音声を出力し、SimpleWavで読み戻すケースを追加した。両レイヤーを半音量にした2つ目の出力と比較し、対象をビルドしてCTestの3ケースが成功。
- **価値／懸念:** Audio Layerのsource読込・波形・音量・重ね合わせをCompositionの実際の音声mixとRender ManagerのWAV出力まで含めて検証できる。video付きジョブに統合する音声経路は別の経路であり、このテスト範囲外。
- **次に確認:** 動画レンダージョブに音声を統合する経路を変更する際は、video+audio出力の同期と最終containerを別途検証する。

### 2026-10-14: Quaternius Knightは歩行表示には使えるが足向きQAには限定的

- **関連:** `ArtifactFbxAnimationViewer`、`KnightCharacter.fbx`、Quaternius CC0 FBX素材選定。
- **確認済み事実:** Quaternius公式itch.io `LowPoly Animated Knight`配布からZIPを取得した。`KnightCharacter.fbx`は2,092,236 bytesのFBX 7400、SHA-256 `41f2616fb4a42f9d35e242bbed1450ededc7537def4bf38cbb11b4e54ff2bf21`。公式配布はCC0とWalk／Runを含むアニメーションモデルとして説明される。同梱にPNG/JPEG画像もlicenseファイルもなく、FBX binaryにも画像pathなし。ZIPの109-frame Preview.gifは色付き騎士歩行を示す。
- **追加確認:** 既存Viewerで2,058 vertices、31 bones、12 clips、ufbx backendとして読み込めた。`HumanArmature|Idle` #1で全身が表示され、gdigrabで取得した0.7秒差の同一viewport画像から脚・足の画素差が出て、再生時刻も進行した。#0 `HumanArmature|Death`は倒れる動きであり、歩行fixtureとして誤って使わないこと。Viewer描画は単色、足先は小さく装備も重なり、足の向きや接地を正しく評価できる画面証拠ではない。
- **調査上の訂正:** 同梱素材として残っていたQuaternius `Animated Human.fbx`はFBX binaryとして正常だが、この既存Viewerでは`unsupported format`となり、過去記録の同Viewer成功を再現できなかった。`Character_HumanoidRig.fbx`もunsupported format、`Character.fbx`はウサギ。どちらも今turnの人体足向きQAには採用しない。
- **価値／懸念:** Knightは既存Viewerで読み込めるQuaternius人型かつ歩行clipを持つため、読込・再生確認の候補を提供する。一方、テクスチャがなく騎士装備が脚を隠すため、足の向きとtextureを同時に判定する素材として不十分。
- **次に確認:** ViewerがAnimated Humanをunsupported formatとする具体的なimporter errorと旧Viewer build差を調べる。ユーザーが色テクスチャ付き人体assetを希望する場合は、Quaternius公式Animated Man Packの取得失敗を踏まえ別の公式CC0 FBX候補が必要。Importer GTestやビルドはユーザー指示まで実行しない。

### 2026-10-14: Modular Character Outfitsは衣装FBXでアニメ確認に使えない

- **関連:** `ArtifactFbxAnimationViewer`、Quaternius `Modular Character Outfits - Fantasy[Standard]`、`Male_Ranger.fbx`。
- **確認済み事実:** ユーザー取得ZIPはCC0 1.0。Standard版のMale Ranger FBXは既存Viewerで読み込めるがanimation clipが0件。FBXのtexture参照はQuaternius配布元の絶対Dropbox作業pathを指し、ZIP内のtexture画像は参照解決されずviewerがbase color読込失敗を記録した。左側サブディスプレイでSolid／Wireframeを確認し、静止A-poseのブーツ輪郭に明白な分離・欠損は見えなかった。
- **価値／懸念:** CC0の色柄画像と人型衣装meshを保持するが、animation acceptance fixtureにはならず、絶対path参照のため現在のViewerでtexture描画も成立しない。足先の違和感がshading単独か、低ポリ形状・歩行pose・装備の見え方かは未検証。
- **次に確認:** Quaterniusのアニメ付き人型FBXと相対pathまたはembedded textureを持つ配布物で同一時刻のSolid／Wireframe比較を行う。texture pathの移植可否やFBX編集は別途明示的な実装範囲として扱う。

### 2026-10-14: Composition ControllerのGPU統合テストは光源ごとに別プロセスで実行する

- **関連:** `tests/Artifact/Model3DLightCompositionIntegrationTest.cpp`、`tests/Artifact/CMakeLists.txt`、`CompositionRenderController`、`ArtifactCore/src/Graphics/MeshRenderer.cppm`、Diligent Vulkan／D3D12。
- **確認済み事実:** 実 `CompositionRenderController` のtickerを起動し、OBJ fixtureを読み込んだ3Dモデルとライトレイヤーを連続2フレーム描画すると、ポイント／スポット／平行／環境／面のすべてで照射モデルの画素が赤優位になった。同じプロセス内で複数controller/deviceを順次初期化する試験は、2回目にDiligent Vulkanのglobal function pointer制約とGPU初期化失敗を起こしたため、CTestは光源ごとに別プロセスで実行する。unit cube fixtureはComposition cameraに対して小さすぎるため、テストだけXY scale 100で画面占有率をdefault modelに合わせる。
- **不具合と修正:** 2フレーム目でDiligentのdebug assertionが `Mesh Position Buffer` のdynamic allocationをframe境界後に再利用していると検出した。頂点バッファを`USAGE_DEFAULT`へ変更し、`updateMeshGeometry()`で内容更新時だけGPU更新する設計にした。静的メッシュは初回、アニメーションメッシュは形状更新時のみ転送し、通常描画／shadow passごとの再uploadとCPU snapshotを除去した。位置バッファをメッシュシェーダー／レイトレーシングと共用し、頂点色なしは初期化時確保の白色fallbackを使う。また `Present()` 内でbackend自身が`FinishFrame()`を呼ぶため、renderer側の重複した`FinishFrame()`を除いた。
- **テストの検出:** Composition統合exeはDiligent debug message callbackでERROR/FATALをatomic flagへ記録し、該当テストを失敗させる。Windows error dialogを無効化し、unhandled process exceptionには固定長の`WriteFile`診断を設けた。故意のSEH fault injectionをCTestで実行し、GoogleTestがtest-body例外`0xc0000005`を記録して失敗扱いにすることを確認した（test-bodyではGoogleTestがSEHを捕捉するため、unhandled filterではなくGoogleTestの診断が出る）。今回、offscreen画像テストexeにも起動時から同じERROR/FATAL callbackを登録し、DiligentのデバッグブレークとWindowsのクラッシュダイアログを抑止、renderer/device破棄後にエラーが記録されていれば環境teardownを失敗させるようにした。また、Diligent `LOG_ERROR_MESSAGE`で合成診断を通知する自己テストを追加し、callbackがERRORを捕捉しつつテスト実行を続けられることを確認した。対象exeのビルドとCTest 1件が成功し、通常のモデル／ライト画像ケースでDiligent ERROR/FATALがないことを確認した。
- **readbackの訂正:** `captureCurrentFrameImage()`はfinal GPU readback surfaceがない場合、画面全体のscreen grabへfallbackする。取得PNGに別desktop windowが写るケースを確認し、そのfallbackによる従来の色判定を実シーンの証拠として扱わない。`CompositionRenderController::readbackPresentedFrameImage()`を追加し、completed composition textureだけをreadbackしてscreen captureにfallbackしないようにした。初回controller frameの直後はfinal surface未準備の場合があり、dirty renderを完了してからbaselineを採る。
- **確認結果:** 低ポリicosphere OBJをComposition integration専用fixtureとし、モデルの実geometryと多数のnormalを実render pipelineに通す。Vulkan／D3D12を別processで実行する16ケース（5 light types、IncludeOnly linking、two lights、visibility/model opt-out）がすべてpassし、Diligent ERROR/FATALなし、red-lightの画素条件を満たす。2灯目追加によるred平均輝度増加は実readbackで約12.5%だったため、測定結果より厳しかった15%閾値を8%に修正し、両backendでpass。保存画像は各400x300で、backend間の平均絶対channel差は全caseで0.012以下。D3D12のMesh Shader PSO失敗は任意経路としてindexed pathへfallbackする。
- **追加runtime確認:** 同じ低ポリモデルを同一位置・異なるZ深度・異なるbase colorで2個置き、near redの後にfar greenを描くケースを追加した。controller最終GPU surface readbackに対する「near redが優位」「geometryの画素量が範囲内」のassertionがVulkanとD3D12のCTest両方でpassした。これは単一の色判定であり、far model単体の表示コントロールやdepth順を反転した比較は未実施。captureではfallback camera時のgeometryが画像左上で切れて見えるため、viewport framingの視覚受入証拠としては不十分。
- **ビルド環境の観察:** 追加のdepth反転controlをコンパイルしようとした別ビルドで、CMake再構成後のNinja module dyndepが`ArtifactEffectsColor.dir/CXX.dd`欠落、続けて`ArtifactAsset.dir/CXX.dd`のobject未記載を報告した。control実装は検証前に取り除き、既にビルド済みのtwo-model testへ戻してVulkan／D3D12 CTestを再実行し成功。失敗原因がこのbuild treeだけかCMake module scan一般かは未特定。
- **価値／懸念:** 本suiteはproduction `CompositionRenderController`のlight collection、layer conversion、real GPU render、completed final composition readbackを検査する。single-light passはbackend全体やeditor UIの完全一致までは保証しない。簡単なopaque二重モデルケースは通ったが、far modelの可視性control、depth反転control、透明境界、shadow寄与は未検証。
- **2026-10-14 の修正:** テストfixtureのモデルは原点配置だったが、実レンダラーのfallback cameraはコンポジション中央を注視するため、captureではgeometryが左上で切れていた。XY中央配置後もnear modelの大きさで上端が切れていたため、two-model testだけscaleを60から24へ下げた。画像の色判定を左上の固定領域から検出済みgeometry boundsへ移し、silhouetteが四辺の画像端へ接していないこともassertする。これはsource変更の記録であり、更新版のbuild／実行結果は未確認。
- **次に確認:** build treeのCMake/Ninja module dyndep再生成手順を調べ、再ビルドが安定してからfar model単体の可視性とZ順反転を同じcontrollerで確認する。中央配置したモデル画像で深度assertionを両backend実行する。実アプリで3D／light layerを編集し、同じlighting結果とMesh Shader fallback表示を確認する。大規模・頻繁変形meshでは`UpdateBuffer`帯域とframe costを計測する。shadow、透明モデル、mask／DOF接続を別契約で受入れる。同じprocess内の複数Diligent device初期化可否は引き続き事前確認する。

### 2026-10-15: 3D／ライトのデバッグをFBXテストMCPへ揃える

- **dump解析:** `tools/inspect_light_minidump.py` でAMD64 exception contextとcaptured memoryをJSONへ抽出。実dumpのGetData stack上のsize `0x73e0e` と出力blobのbegin/end差が一致（474,638 bytes）。memcpy中のr8約2.7GBは出力blob容量と一致しない。native内部のサイズ／sourceの破損仮説は未検証で、原因断定はしない。関連artifact: `out/light-mcp/occlusion-dump-context.json`。symbol解決や完全なheap検査はこの小さなツールの範囲外。

- **first-fault dump追加:** 独立test exeにアクセス違反時の一度だけのminidump保存とCPU register／access addressを追加。通常GTestのnative exception catchを無効にし、二次Qt終了例外の混入を防ぐ。合成SEH gateだけcatchを維持。実D3D12遮蔽faultで1,374,114-byte dumpを保存、合成SEH gate pass、D3D12 MCP15 capture pass。コピー中のr8=0xa3371b31は約2.7GBだが、元のSerialize引数や破損原因を示す証拠はまだない。dumpはWindows TempのArtifactLightTest-2408.dmp、runnerは固有dump pathと保存成否をreportへ記録する。
- **D3D12 validation結果:** InfoQueueで MeshRendererのopaque／transparent／shadow cache name重複によるLoad desc mismatchとStore duplicate nameを観測。さらにPSOがtexture resource format UNORMで作成され、bound RTVがUNORM_SRGBのdraw mismatchを確認。Artifact側をviewのformat使用へ修正し、ArtifactCoreのopaque／transparent／meshlet／shadow PSOにprocess-wide atomic serial付き一意名を追加した。変更後のvalidation付きocclusion runもpassし、descriptor mismatch／duplicate Store／format mismatchは消えた。未登録名への初回Load warningは残る。Serialize access violationは間欠的に残り、PSO命名がその修正だとは証明していない。関連ログ: `out/light-mcp/occlusion-core-pso-cache-validation-current.log`。

- **遮蔽例外のstack特定:** DbgHelpでstartup後のmodule listをrefreshし、exception CONTEXTからstackを取得した。最初のfaultはocclusion-destroy段階でShaderManager::savePSOCache→Diligent PipelineStateCacheD3D12Impl::GetData→D3D12Core::Serialize→memcpy。PSO生存中へsave順序を動かす仮説は同じfaultで否定され、順序変更を撤去した。Diligent constructorはCreatePipelineLibrary(nullptr,0)なので入力cache blobのローカル寿命説も今回のコードには一致しない。次に2モデル間のPSO cache load/store再利用とnative Serializeデータ寿命を確認する。logはout/light-mcp/occlusion-pso-serialize-failure.log。
- **追加切り分け:** cache modeをSTOREだけにしても同じSerialize access violationが起き、load blob原因説を否定。destroy直前にrendererの`flushAndWait()`をテストから呼ぶ対照も実施したが、3反復中1回pass後に2回目が同じ`D3D12Core.dll` Serialize faultとなり、待機追加を撤去した。出力`out/light-mcp/occlusion-store-only-failure.log`。クラッシュは間欠的であり、原因や製品修正は未確定。次は複数MeshRenderer間のPSO cache name/store再利用と native library serializationを特定する。
- **MCP診断情報の拡張:** `light.state` と各capture metadataに既存rendererのGPU validation文字列を追加。D3D12のdevice removed reasonと取得可能なInfoQueue、Vulkanのvalidation状態をJSONへ含める。更新exeを両backendで再実行し各15 capture成功、D3D12 deviceRemovedReason=0、InfoQueueはunavailable、Vulkanはunsupportedと記録した。D3D12遮蔽CTestの最終repeatは1回目で同じserialize fault（`out/light-mcp/occlusion-final-3-repeat.log`）。この診断追加はクラッシュ原因を修正していない。

- **guide分離の検証:** lightの `draw()` は照明ではなくeditor controlsを描くため、production controllerのbeauty描画から既存overlayへ移した。光源収集は維持し、overlayではvisibility／active frame／selected-only／category／gizmo設定を守ってcamera指定中にflushする。testのthin-guide除外を撤去しraw GPU boundsへ戻した。両backend15画像、gizmo off/onのbeauty同一、HWND側のpoint guide表示変化が確認できた。追加overlay walkは既存layer list参照で新規collection allocationなし、大規模light時の性能は未計測。
- **遮蔽の訂正:** CTest全18件では17成功だがD3D12 two-model例外が再発した。単発passは安定性を保証しなかった。fixed-buffer moduleログとvectored first-chance observerを追加し、最初はucrtbase.dll、終了時はQt6Cored.dllと確認。Qt自体のroot bugを断定する情報ではない。原因未特定のままテスト失敗を維持する。

- **readback根因と修正の確認:** Diligent `FenceD3D12Impl.cpp`はmanual-reset eventを作成し、`Wait()`はresetを行わない。reuse slotではWaitが完了値到達前に戻れるコードだった。アプリ側 `ArtifactIRenderer.cppm` にWait後のGetCompletedValue確認とthread yieldを追加し、slot再利用前／CPU map前の両readback経路へ適用。同期readbackのcold境界で追加allocationなし。D3D12通常runnerが2回連続13画像成功し、device infoからactual backendを照合する改訂runnerでもD3D12／Vulkan各13画像が成功した。two-model occlusion CTestはXYZ同一scale／thin guide除外boundsで両backendpass。依存Diligent自体は編集していない。広いGPU同期契約全体の証明ではなく、今回のreadbackとlighting更新の再現ケースの検証。

- **HWND比較の確認:** 同じ更新exeでD3D12の通常13画像runnerはframe 13のmodel opt-outにgreen残留を検出して失敗した。`--window-proof`を指定した対照は13画像で成功し、window／GPUとも無彩色opt-outを表示した。window captureがreadback前へ入ることで症状が変わるタイミング依存を確認したが、production修正の証拠とはしない。runnerの通常検査はwindow proofなしのまま維持する。

- **D3D12切り分け追加:** model opt-outの失敗PNGには緑／白の横帯が混在していた。mesh light constantsはdraw前にdynamic map-discardで更新し、readbackはcopy fenceをwaitするコードを確認したが、原因の断定はできない。`light.capture(includeWindow=true)`へFBX共通HWND captureを追加し、GPU readback前の表示画像と同じframeのGPU画像を別々に保存する。表示自体かreadback境界かを比較するための診断で、window画像による合否の代用はしない。

- **visibility対照の追加:** `light.controls` でvisible／affectedByLightsをboolean検査後に反映し、hidden／model opt-out／restoreの3画像を追加した。Vulkanは13画像で成功。D3D12はrestore色差不足、再実行はmodel opt-out中のgreen残留で失敗した。native exceptionはなくstateは変更を報告しDiligent errorもfalseだった。画像の状態不一致を検出できたが原因は未特定。readbackは既にcopy fenceをCPU waitするため、単に待ち時間を足す修正は行わない。次にresidual draw queueとframe間のlight/material GPU定数の更新寿命を確認する。

- **runtime確認:** 変更版exeをbuildし、Vulkan／D3D12指定のMCP runnerが両方成功した。XYZ同一scaleのmodelに対し各processで5 light typeそれぞれのred／greenを切替し、10 fresh GPU画像、frame進行、foreground、geometry margin、channel優位と色変更の対照差、Diligent errorなし、不正RGB拒否後の状態維持を確認した。MCP待機中はtickerを停止しcapture境界だけ起動する。出力は`out/light-mcp/{vulkan,d3d12}/report.json`とPNG。`backendRequested`は起動指定値であり実device typeの独立readbackではない。
- **既存遮蔽テストの再確認:** 新exeでtwo-model CTestを実行したが、Vulkanは原点guide混入によるmargin assertion失敗、D3D12はSEH `0xc0000005`で失敗した。MCP single-model light成功をtwo-model depth安定性の証拠へ拡張しない。
- **確認した制約:** point等のlight guideがgizmo非表示でもfinal GPU画像へ混ざる。現状のbounds診断は実測3pxまでの細線を近傍判定で除外するが、元PNGは保持する。productionのguide／final composition出力の責務は別途確認が必要。native crash dump収集は未実装。

- **実装更新:** 既存composition integration exeへ `--mcp-stdio` モードを追加した。共通serverに `light.state`／`light.set`／`light.capture` を登録し、production controllerのframe進行とfinal GPU surfaceを画像・状態で観測する。外側の `tools/test_light_mcp.py` は要求timeout、stderr、exit code、red／green切替画像と不正入力の原子性を記録する。crash dump、model／cameraの任意編集、遮蔽control操作は未実装。

- **関連:** `tests/ArtifactTestMcpServer.h`、`tests/Artifact/FbxAnimationViewerMain.cpp`、`tests/Artifact/Model3DLightCompositionIntegrationTest.cpp`、`tools/test_light_mcp.py`。
- **確認済み事実:** FBX Viewerと同じstdio MCP serverを独立composition test exeから使い、production controllerを操作する。state／light property／visibility／model opt-out／GPU capture、backend・frame serial・geometry bounds・viewport/DPR、stderr・exit code・minidumpを一連のrunnerで記録できる。
- **今回の改善:** MCP serverを開く条件をframe serial単独から、2 frame以上・`Layer RT`／`Accum RT` ready・final GPU readback成功へ強化した。起動時点で中間pipelineが未準備なら最大15秒待ち、timeout時に最後のframe snapshotをstderrへ出す。`light.state`とcapture metadataも中間RT準備状態を返す。
- **価値／懸念:** 起動直後の未準備画像を、正常なcapture失敗や照明不具合と混同しにくくなる。readbackは起動時にのみ準備確認として行い、state pollingごとには同期GPU readbackを発生させない。runnerの15 captureはD3D12／Vulkan双方で成功し、全画像でactual backend、中間RT、Diligent errorを確認した。D3D12 PSO Serialize終了時アクセス違反は別途未解決。
- **次に確認:** D3D12遮蔽CTestを反復し、PSO cache命名の再利用が終了時Serialize faultの発生率に影響するか切り分ける。現段階ではクラッシュ修正を主張しない。

# 2026-10-14: テストGUIを非アクティブのまま検査する共通経路

- 関連: `tests/ArtifactTestMcpServer.h`, `tests/ArtifactTestWindowCapture.h`, `tests/Artifact/FbxAnimationViewerMain.cpp`
- 確認できたこと: `ArtifactCoreMeshImporterFbxTest`は3件すべてpass。Quaternius Rangerのskin pose変化、Tempに展開した配布不可Nathanのembedded base-color JPEG（8192×8192）の完全decodeとUV、配布可能な`Standard_Walk.fbx`のembedded base-color／normal texture・UV・material assignmentをそれぞれ検査した。Nathan ZIPと展開モデルはTemp外へ移動せず、プロジェクトに含めていない。
- 不具合と修正: Windowsでは`QDir::separator()`と`QDir::cleanPath()`の区切り表現が異なる場合があり、texture testのembedded-cache判定がfalse negativeになった。判定前に両pathを`/`へ正規化した。Diligentの反復errorはPSO static shader variablesの未割り当てを示しており、PSO作成後に`TransformCB`、`SkinningCB`、`ColorCB`、`BaseColorSampler`を設定するよう変更した。修正後のD3D12ログで該当errorは0件。HLSL `pow(f,e)`のnegative input warningは残るが、今回の未bind errorとは別件。
- 注意: NathanのFBXにはbase-color画像はあるがnormal textureは見つからず、normal map試験には向かない。テストではNathanをbase-color用、`Standard_Walk.fbx`をbase-color/normal用fixtureとして分ける。
- 仮説（未検証）: shader resource未bindが以前の金属的・まだらな陰影の一因である可能性がある。修正後のcaptureを旧画像と目視比較できていないため、質感が解消したとは断定しない。
- 次に確認すること: MCPの`viewer.capture`で修正後のNathan表示を再取得し、sRGB base-color処理、metallic/roughness係数、UV mappingを画像とImporter値の両方で確認する。

### 2026-10-15: FBX Viewerの画面証拠から見つかったUV・外面・色空間の不整合

- **関連:** `ArtifactCore/src/Geometry/MeshImporter.cppm`、`ArtifactCore/src/Graphics/MeshRenderer.cppm`、`Artifact/src/Widgets/Render/ArtifactDiligentEngineRenderWindow.cppm`、`Artifact/src/Render/DiligentDeviceManager.cppm`、`tests/Artifact/FbxAnimationViewerMain.cpp`、`tools/test_mcp_client.py`。
- **確認済み事実:** 前日の「error 0」は初回render完了を待てていない証拠だった。8K atlasのCPU準備でGUIが約41秒止まり、その間はMCP discoveryだけ応答していた。待ち続けた同一processからcaptureを得るとtextureの上下逆転、二重gamma、逆cullingによる眼球／歯の露出が分かった。モデル同梱の見本JPEGと実画像を比較して区別した。
- **変更:** FBXのUV0／UV1をtop-leftへ正規化し、UV transformを同じ原点に変換。ViewerはsRGB RTVへlinear値を渡し、CCW外面を描く。8-bit decode lookupと型判定hoistをViewer／Coreへ反映。Diligent VB／IB replacement pointer、swapchain resize、Vulkan WireframeFill requestを修正。GUI版Qtのログがdebuggerへ送られてstderrに残らないため、独立Viewerのmessage handlerをstderrへ明示した。
- **確認結果:** Core importer 4ケースとCTestがpass。D3D12／Vulkanの15-step MCP scenarioが双方passし、5 PNG／backendで顔・衣服・靴・歩行pose・wireframe・別モデルへの切替を確認。両方でError/Fatal 0、foreground／cursor不変、左端display作業領域内。正面Solidの同時刻viewport画素はbackend間一致、背面／別モデルSolidのRGB平均差は0.000002/255未満。初回待ち41.43→9.62秒はDebug Viewerの単一比較であり、Core全体やsteady-state性能の保証ではない。配布不可素材とPNGはすべてユーザーTempに保持。
- **再利用できる知見:** MCP tool実行の成功、初期化ログ、実render済みcapture、終了処理の診断は別々に確認する。APIの既存object上書きやwireframe feature未要求は、空scene起動後にモデルを差し替えるscenarioで発見できる。共通Python clientは他のstdio test serverでも同じJSON step形式を使える。
- **追加検証と修正:** 自作Phong fixtureで`pbr.base_factor.texture`のMR誤分類、自作3ds Max PBR fixtureで独立metalness／roughness画像の欠落を2つのfailing testとして再現し、CoreとViewerを修正した。R=51／204の画像版と定数0.2／0.8版は、D3D12／Vulkanのviewport画素が一致した（材質差し替え後も一致）。画像のG／Bは異なる値として誤ったpacked解釈を検出する。Core全6ケースpass、両backendの32-operation連続scenarioでError/Fatal 0、既存Nathan表示は基準と一致。今回の検証中のcursor座標変化はユーザー操作と区別できないため、不変の証拠には使わない。
- **今回追加した境界:** Viewerに独立scalar画像のset/upload/bindを追加し、linear Rとして読む。初期化／map path変更時にのみtextureを作り、既存mip生成を使う。追加descriptor作業領域は32要素のstack配列で固定。独立mapはCore既存の材質slotと対応し、packed MRとは別に保持する。自作fixture generatorは標準PythonのみでFBX／PNGを再生成でき、第三者モデルのbytesを含まない。
- **残る別契約:** Viewerのmap固有UV transform／UV1対応、DiffuseFactor画像をbase-color weightとして適用する機能、製品Composition／RTの最終画像とHDR表示は未確認／未対応。今回のR-channel読込の確認を、これらまでの保証として扱わない。

### 2026-10-15: Render Queueの動画・音声mux結合テストを追加

- **関連:** `tests/Artifact/RenderQueueLayerImageIntegrationTest.cpp`、`tests/Artifact/AudioLayerRenderQueueIntegrationTest.cpp`。
- **確認済み事実:** 音声レイヤーには既存のWAV Render Queue出力テストがあったが、動画＋音声レイヤーを同一compositionからmuxed fileへ出す結合テストはなかった。新テストはFFmpeg内蔵MJPEG encoderで赤い動画fixtureを作り、既存Render Queueのintegrated render、AAC mux、出力後のdecodeを検査する。source中心画素は赤だが初版出力は黒だった。Render Queue snapshotの動画open完了を待つbarrierを `Artifact/src/Render/ArtifactRenderQueueService.cppm` に追加後、出力中心画素の赤成分・チャンネル比較、AAC stream、音声抽出PCMのRMSとdurationを含む対象テストが成功した。
- **価値／懸念:** 厳密な映像内容比較と抽出PCMのRMSにより、mux成功statusやstream metadataだけでなく映像・音声の内容を検査できる。barrierはrender worker threadのqueued open completionを処理し、最大15秒で打ち切る。異なるGPU／FFmpeg構成での再現性と、ロード失敗時の待ち時間は未確認。
- **次に確認:** Debugの追加ケース1件は成功。suite全体、CIなど異なるFFmpeg codec構成・GPU adapterでの再現性は未確認。

### 2026-10-15: 動画muxのAAC音声が途中で切れる問題を検出・修正

- **関連:** `ArtifactCore/src/Image/FFmpegAudioEncoder.cppm` の `muxAudioWithVideo()` / `probeAudioFile()`、`tests/Artifact/AudioLayerRenderQueueIntegrationTest.cpp`。
- **確認済み事実:** 3秒のmuxed MP4を外部FFmpegで調べると、container durationは3秒だがAAC streamは約0.768秒・37 packetだった。AACをPCM WAVへdecodeすると0.45–0.70秒のマーカーは聞こえる一方、1.20–1.80秒と2.25–2.50秒は無音／EOFになった。原因箇所はresampler出力をAAC encoderの固定frame単位で保持せず、そのままencoderへ渡していたmux経路だった。mux側に`AVAudioFifo`を導入し、resampler出力を蓄積してencoder frame単位で投入、最後の部分frameとdecoder/resampler/encoderのflushを処理するよう修正した。`probeAudioFile()`はformat全体ではなくaudio streamのdurationを優先する。修正後は同じ単体テストをビルド・実行し、MP4内AACからPCMへ抽出したdurationが動画尺と一致し、0.45–0.70秒と2.25–2.50秒が有音、1.20–1.80秒が無音であることを検証して成功した。
- **追加確認・修正:** 回帰テストから`FFmpegAudioDecoder::openFile()`を直接呼ぶと、`avformat_open_input()`成功後の`avformat_find_stream_info()`内でSEH access violationを再現した。原因となったstream-info呼び出しの`threads` / `thread_type` AVDictionary渡しを削除し、codec open側の単一スレッド設定は維持した。テストはmuxed AACをopenし、最初のdecoded segmentを取得する確認を追加。修正後、同じDebugテスト1件が成功した。
- **価値／懸念:** テストはencoded MP4の映像色切替、AAC codec／sample rate／channel数、muxed AACの抽出後durationと時刻窓ごとのRMS、`FFmpegAudioDecoder`によるopen/decodeを一つの同期契約として確認する。`ArtifactVideoLayer::getAudio()`のfalse応答は別経路の観測として残り、今回の修正対象外。
- **次に確認:** 対象Debugテスト1件は成功。全suite、他のFFmpeg codec構成やGPU adapterでの再現性は未確認。


## 2026-10-15: DPX writerはCineon transferとビット深度を個別メタデータで保持する

- **関連:** `ArtifactCore/src/IO/Image/ImageExporter.cppm`、`ArtifactCore/src/IO/Image/ImageImporter.cppm`、`ImageExportOptions::bitsPerSample`。
- **確認済み事実:** OIIO DPX writerは汎用 `TransferFunction` 文字列を標準transferとして保存せず、`oiio:ColorSpace=sRGB` があると `dpx:Transfer` より優先して709として読む場合があった。Cineon出力に `oiio:ColorSpace=KodakLog` と `dpx:Transfer=Logarithmic` を設定し、UINT16コンテナに `oiio:BitsPerSample` を指定すると、10 / 12 / 16 bitの実DPXファイルで指定深度とtransfer headerを読み戻せる。
- **変更:** Cineon DPX書き出しへ標準transfer metadataを付け、ImporterはDPXのKodakLog/Logarithmic組み合わせをCineonと判断する。`oiio:BitsPerSample`をRawImageへ反映する。独立基準値によるCineon code valueとRGB/alphaの往復をテストする。
- **価値／懸念:** DPXの規格フィールドとOIIO固有のtransfer名を混同せず、素材metadataをコア転送関数へつなげられる。packing詳細はOIIO writer依存であり、別のOIIO build/pluginでは再確認が必要。
- **次に確認:** 別のOIIO build/pluginでも標準DPX headerとpackingを読み戻せるか確認する。


## 2026-10-15: 非連続プリコンポーズ解除でのレイヤー順復元

- **関連:** `Artifact/src/Service/ArtifactProjectService.cppm` の `unprecomposeLayerInCurrentComposition()`、`tests/Artifact/PrecomposeViewerMain.cpp`。
- **確認できた事実:** 解除処理はプリコンポレイヤーの挿入位置を記録し、復元する各レイヤーを `insertIndex + i` へ順番に配置する。移動対象が元Composition内で非連続だった場合の順序契約は、既存MCPテストで検査されていなかった。
- **未検証の仮説:** AとUntouchedを選びBを残す非連続選択では、逐次挿入位置がシフトし、解除後の順序が元のA/B/Untouchedと異なる可能性がある。`test.nonContiguousSelectionOrder` を追加したが、実行は未確認。
- **価値／懸念:** レイヤー順は描画重なりや編集結果に影響するため、連続選択だけでなく隙間を含む選択解除の回帰検知が必要。
- **次に確認:** 許可されたGUIテスト実行で解除・Undo・Redo後のレイヤー順が一致するか確認する。


## 2026-10-15: Native Dockの一時ターゲット表示と局所分割

- **関連:** `Artifact/include/Widgets/Dock/NativeDockSurface.ixx`、`Artifact/include/Widgets/Dock/DockManager.ixx`、QADS互換のドック配置。
- **確認できた事実:** Native Dockは当初`Left / Right / Top / Bottom / Center`の固定5領域と各領域の`QTabWidget`で配置を表現していた。QADS風の中央＋四方向オーバーレイだけを追加しても、方向候補を既存の固定領域へ割り当てるだけでは、ホバー中のドック領域の横へsplitを作る配置意味を表現できない。
- **追加で確認できた事実:** QADS 4.5.0はmain windowとfloating containerの両方に4辺のAuto-Hide sidebar、展開／折りたたみ、サイドバー内の挿入順、通常ドックへの復帰を持つ。Native Dockにはsidebar／auto-hide stateがなく、現在の`pinned_`はタブを閉じる操作の制御に使われるため、これをAuto-Hide状態へ流用できない。
- **追加で確認できた事実:** 浮動タブ面は`createFloatingTabSurface()`で`acceptDrops`を有効にしておらず、挿入プレビューもNativeDockSurface直下へ座標変換して描く経路だった。浮動ウィンドウ上のdrop targetと予告表示が別々のwidget／座標系に分かれていた。
- **今回の対応:** 浮動ダイアログに専用のdrop preview childを置き、ダイアログ自身でもdropを受けて浮動タブ面へルーティングする。挿入線は対象タブバー自身で描く。ドック面の一時オーバーレイは対象領域上に表示し、中央はタブ統合、四方向は対象タブ面を分割する。QADS 4.5.0のドロップ競合処理を確認し、オーバーレイ選択したsplitが失敗した場合に別の固定領域へフォールバックしないようにする。QADSの`dropIntoCenterOfSection()`はinvalid tab indexを0へ丸めるため、中央オーバーレイの合流位置も先頭へ合わせる。`DockLayoutEntry`に任意のsplit pathと比率を追加し、埋め込み面・浮動面の分割木を保存・復元する。旧JSONではこれらのキーがなくても従来の領域タブとして扱う。
- **価値／懸念:** オーバーレイの予告とdrop結果が局所splitとして一致し、split配置をレイアウトJSONと既存Undo用スナップショットに保持できる。空になったsplit葉を移動・フロート化後に畳む経路も追加した。実装後のビルド・実機動作は未確認。
- **次に確認:** ビルド・実機確認が許可されたら、分割方向、入れ子分割、splitterリサイズ後の保存・復元、Undo/Redo、空split葉のcollapseを確認する。浮動グループへのタブ挿入位置と、キャンセル時の予告解除も確認する。QADSとの次の大きな機能差であるAuto-Hideは、`pinned_`と分離した状態・保存形式・4辺sidebar・popup lifecycleを設計してから実装する。


## 2026-10-15: FBX importerはDiffuseFactorをbase-color weightとして適用する

- **関連:** `ArtifactCore/src/Geometry/MeshImporter.cppm` の `detectTexturesFromUfbx()`、`ArtifactCore/include/Geometry/MeshImporter.ixx` の `ImportedPbrMaterial`、`tests/fixtures/fbx-materials/diffuse-factor-weight.fbx`、`tools/fbx/create_material_fixtures.py`、`tests/ArtifactCore/MeshImporterFbxTest.cpp`。
- **確認済み事実:** vendored ufbx は `pbr.base_color` を FBX `Diffuse`/`DiffuseColor` に、`pbr.base_factor` を `DiffuseFactor` に対応させている（`third_party/ufbx/ufbx.c:19398-19414`）。3ds Max physical の `base_weight`、OSL の `base`、OpenPBR の `base_weight` も同じ map に入る。glTF マッピングには BASE_FACTOR が無く、`baseColorFactor` は `main|baseColor` に畳まれている（`ufbx.c:19590-19610`）。したがって `pbr.base_factor` は全対応シェーダで scalar weight として扱える。Importer は base_color だけを読み、factor を捨てていた。
- **変更:** `pbr.base_factor` を linear 空間で base_color に乗算し、その後 sRGB エンコードする。`ImportedPbrMaterial` に `hasBaseColorFactor` / `baseColorFactor` を追加。DiffuseFactor 0.5 の fixture を追加し、factor 1 が恒等であることと factor 0.5 がエンコード後の base color を半分にすることを検査する回帰テストを2件追加した。
- **残る穴:** `pbr.base_factor.texture`（DiffuseFactor に画像を繋いだ FBX。既存の `diffuse-factor.fbx` がこの形）は base-color weight map として描画へ届かない。新テクスチャ slot が `ArtifactCore::Material` → `MeshRenderer` → シェーダ → JSON 保存 → Inspector まで通る必要があり、Importer 内では閉じない。
- **別便:** カメラ／ライトノードの取り込みは未配線。`scene->cameras` / `scene->lights` から取り出す公開 API と、受け皿である `ArtifactCameraLayer` / `ArtifactLightLayer` の生成・配置・保存配線が必要。
- **実装不可と判断した項目:** alpha mask cutoff / mask-vs-blend は FBX 経路から復元できない。FBX に cutoff の概念が無く、ufbx も `ufbx_material_feature_info`（`enabled` と `is_explicit` のみ、`ufbx.h:2317-2326`）以上を露出しない。glTF の alphaMode も ufbx の material map に出ない（`ufbx.c:19590-19610`）。復元するなら glTF JSON を直接読む別経路になる。
- **未検証:** ビルドとテスト実行は未実施。回帰テストの期待値 0.39886 / 0.24228 は手計算であり、実測で確認していない。


## 2026-10-15: FBX importerの単位正規化は現状維持（有効化すると全FBXが1/100になる）

- **関連:** `ArtifactCore/src/Geometry/MeshImporter.cppm` の `loadWithUfbx()` が組む `ufbx_load_opts`、`third_party/ufbx/ufbx.c` の `ufbxi_update_scene_settings()` / `ufbxi_scale_units()`。
- **確認済み事実:** ufbx は `UnitScaleFactor` 未宣言の FBX を cm 既定として扱う（`unit_scale_factor = 1.0` → `unit_meters = 1.0 * 0.01`、`ufbx.c:23796-23802`。`ufbx.h:3903` にも "FBX files usually default to centimeters" とある）。`target_unit_meters` は既定 0 で無変換（`ufbx.h:4832-4834`）。Importer は unit 変換を一切設定しておらず、`Artifact3DModelLayer` 側にも補正は無い。
- **価値／懸念:** `opts.target_unit_meters = 1.0` を設定すると scene が `0.01 / 1.0 = 0.01` 倍され、既存の全 FBX が 100 分の 1 の大きさになる。ファイル間のスケールは揃うが、基準画像・既存プロジェクトの見た目が変わるため1行の修正として扱えない。
- **決定:** 2026-10-15 のユーザー判断により現状維持。単位正規化は必要になった時点で、既存モデルと基準画像の更新を含む別の作業として扱う。


## 2026-10-16: ArtifactScriptとLayerの統合fixtureを独立ファイルで差し替える

- **関連:** `tests/Artifact/ArtifactScriptLayerIntegrationTest.cpp`、`tests/fixtures/artifact-script-layer/write-target.layer-script`、CMake option `ARTIFACT_ENABLE_SCRIPT_LAYER_INTEGRATION_TEST`。
- **確認済み事実:** Compositionの構築時にComposition向けスクリプトAPIが登録され、Layerのscript bindingからfixtureファイルを読み込んでLifecycle hookを実行できる。FBXテストと同じくCMake cache pathと環境変数でfixtureを差し替える構造を作れる。
- **変更:** 実Layerを持つCompositionで `getLayer` / `getProperty` / `setProperty` を通し、別Layerのopacity変更を検証する選択可能な統合suiteを追加した。初回実行は `QGuiApplication` のGTest環境setupでSEH access violationとなったが、GUIを使わないsuiteのapplicationを `QCoreApplication` に替え、MSVC Debugでbuild後CTest 1/1 passed。
- **価値／懸念:** fixtureスクリプトをテストコードから分離すると、ユーザーが実際に編集するscriptファイル単位でケースを再現しやすい。ビルド後の `windeployqt` はvcpkgの翻訳カタログ／release-bin探索警告を出したが、runtime DLL・Qt platform pluginコピーは成功しテスト実行に影響しなかった。一方、Compositionが登録するグローバルAPI callbackの寿命は各テストプロセス内で扱う必要があるため、このsuiteを同一プロセスで複数Compositionの生存・破棄テストへ広げる場合はcallback所有権を別途確認する。
- **次に確認:** 負のケース（存在しないLayer／property）とキーフレーム書込みをfixture suiteへ追加するか検討する。CMake再configure後の独立build/runは成功済み。


## 2026-10-15: FBX取り込み時にカメラ／ライト層を生成する

- **関連:** `Artifact/src/Widgets/Menu/ArtifactLayerMenu.cppm` の `handleCreateModel3D()` / `createImportedSceneLayers()` / `applyImportedSceneTransform()`、`ArtifactCore` の `MeshImporter::importedCameras()` / `importedLights()`、`third_party/ufbx/ufbx.h` の `target_camera_axes`、`Artifact/src/Layer/ArtifactCameraLayer.cppm`、`ArtifactCore/src/Animation/AnimatableTransform3D.cppm`。
- **確認済み事実:** FBXカメラはローカル +X が前方（`ufbx.h:4836-4838`「By default FBX cameras point towards the positive X axis」）、Artifactのカメラはローカル -Z が前方（`ArtifactCameraLayer.cppm:484`「Camera convention looks down local -Z」）、レイヤーの回転は `Rz * Ry * Rx` で合成される（`AnimatableTransform3D.cppm:179`）。3つを合わせると、取り込んだカメラのワールド行列 `M` に対する層の姿勢は `G = M * Ry(-90°)` になる。
- **変更:** 3Dモデル層を追加した直後に、そのFBXが宣言しているカメラ／ライトを1つずつ層として追加する。名前はFBXのノード名（空ならシーン名 + `Camera`/`Light`）。カメラは projection / fov / near / far（orthographic は ortho width / height）、ライトは type / color / intensity / cone angle / feather / castsShadows を転送し、位置と回転は `G = M * Ry(-90°)` を分解して設定する。ライトの `Intensity` は ufbx が100分の1で返すため100倍して `Units::Percent` に入れる（層の既定が `Percent{100}` のため）。余分な層を作っている間は選択が移動するので、最後にモデル層へ選択を戻す。
- **検証:** 変換式はスクラッチパッドの `verify_scene_transform.py` で7ケース（identity／平行移動／pitch／yaw／合成回転／真上・真下の gimbal）を実計算し、層の前後・上下・左右がFBXの +X / +Y / +Z 軸と一致することを確認した。ビルドと実行は未実施。
- **未確認／懸念:** (1) ライト色を `FloatColor` へ入れるときの色空間変換をしていない（`toLinear()` / `fromLinear()` の向きを確認していない）。(2) FBX座標をcomp単位へ変換していない（単位正規化を現状維持にしたためで、3Dモデル層もFBX座標をそのまま層ローカルに使うことに合わせた）。(3) 取り込んだカメラが active camera になるかは未確認。なれば合成の描画が変わる。(4) Undo が既存の層追加経路と同一になるか未確認。(5) `LightType` に volume が無いため volume ライトは point に落としている。
- **別便にできる改善:** `MeshImporter` は3Dモデル層が保持しているのに、メニュー側でファイルを読み直している（2回パース）。層からカメラ／ライトを公開すれば1回で済む。`Artifact3DModelLayer.ixx` に `import MeshImporter;` を足すことになるため、公開APIを広げる判断が要る。

## 2026-10-15: Contents Viewerの描画経路に初期化・開始時競合の穴

- **関連:** Artifact/src/Widgets/Viewer/ArtifactContentsViewer.cppm、Artifact/src/Widgets/Render/ArtifactDiligentEngineRenderWindow.cppm、ArtifactCore/src/Media/MediaReader.cppm、ArtifactCore/src/Media/MediaPlaybackController.cppm。
- **確認できた事実:** OS desktop captureでもSVG画像面は空、WAV波形は No audio data、未再生MP4は黒だった。画像rendererの初期化結果はContents Viewer側で確認されず、hidden CPU QLabelは表示fallbackに使われない。3D viewer software fallbackはrender/presentで即returnするため実描画を持たない。Audio play() は非同期readerを開始後に同期でPCMをpollし、viewer側の最初の空pollが直ちにcontroller stopとsink suspendを起こす構造である。
- **追加実行事実:** PNG/SVG fixtureの起動標準出力ではDiligentがNVIDIA GeForce RTX 4070 Tiを選び、Vulkan swap chainのsurface transformとpresent modeを初期化し、Diligent ERRORは出ていない。プロセス内のGPU初期化は確認できたが、Contents Viewer固有のsprite submit／present成功は未確認。計測hookの再ビルドは既存の未コミット NativeDockSurface.ixx のコンパイルエラーで止まった。
- **未検証の仮説:** WAVの空表示は最初のpacketが未到着のpollによって再生が停止した可能性が高い。GPU画像previewではNVIDIA Vulkan deviceとswapchain初期化ログが得られたが、sprite submit／present成功は未計測。3Dでもdeviceとviewport相当サイズのswapchain resizeログがあるためdevice/swapchain failure fallbackではなさそうだが、draw／present成功は不明。Qt widget captureの白viewportはネイティブQWindowの実画面表示を確定しない（desktop capture時にテスト窓が前面でなかった）。paused動画は再生開始前のため黒画面のままで、先頭frameを表示する経路がない可能性がある。
- **価値／懸念:** メタデータ読込の成功だけではsurfaceの初期化・frame delivery・audio queue readinessを検査できない。独立ウィジェットテストに経路別runtime assertionが必要になる。
- **次に確認:** Renderer isInitialized / swapchain / present結果、video frameChanged受信、audio sink deviceと初回packetまでの状態をテスト結果へ出し、各失敗を形式別に再実行する。製品修正は独立した明示依頼を待つ。


## 2026-10-16: Contents ViewerのSVGはimageformats plugin配置を要確認

- **関連:** `Artifact/src/Widgets/Viewer/ArtifactContentsViewer.cppm`、`Artifact/src/Render/ArtifactIRenderer.cppm`、`tools/capture_ui_test.py`、`Artifact.exe` / `ArtifactUiTest.exe` のQt runtime plugin配置。
- **確認できた事実:** fixtureの通常環境ではSVGのQPixmap decode結果が0×0だった。`Qt6Svgd.dll` とSVG icon engineは出力先に存在するが、Qt imageformats用 `qsvgd.dll` はArtifactとfixture双方の実行フォルダーに無かった。vcpkgのQt plugin directoryを `QT_PLUGIN_PATH` に指定するとSVGが640×360へdecodeされ、画像GPU load後にpresent成功2回／失敗0となった。PNGもプラグイン指定なしで128×128へdecodeされpresent成功した。既存 `ArtifactIRenderer` は初期化、swap chain、present結果のカウンターを公開しており、環境変数gate付き一時ログで個別コンパイル・fixture実行に使えた。
- **追加確認:** `ArtifactUiTest` build出力はSVG imageformats pluginに加えて `plugins/multimedia/ffmpegmediaplugin[d].dll` も欠き、capture helperはこのexeを直接起動する。一方Artifact本体のPOST_BUILDではwindeployqtが走り、別配布先にSVG pluginと通常名のFFmpeg multimedia pluginが配置されていた。よってfixtureは通常Artifactと異なるQt plugin runtimeで起動している。音声fixtureはmono/24kHzだが、`MediaAudioDecoder::initialize()` がcodec初期化時にdefault resamplerを設定し、出力PCMはstereo/44.1kHz/S16となるためviewer sink/waveform形式と合う。前に疑ったformat mismatchは誤り。
- **Profiler境界の追加確認:** `audio-after-play-os.png` のAudio Playback Profilerは3 scopeを表示。prefix-filtered latest sample数はscope名の種類数で、音声経路のscope一覧と照合すると記録済みなのは `pumpAudioPlayback` / `fillPendingBuffer` / `getNextAudioFrame`。`audioSinkWrite` と `waveformAppend` は未記録。よってこの実行ではreader/controller pollの後、PCMが空としてsink write前に早期returnし、波形処理にも入っていない。sinkがStopped/Suspendedならpump先頭returnとなりfill/getNextのscopeが残らないので観測結果と合わない。WAVは有効なPCM S16でffmpeg decodeも成功しており、非同期readerの初回packet到着前pollが最有力原因。queue長とdecoder出力bytesを直接取得していないため、queue emptyとdecoder no-outputの完全な区別は未了。
- **修正・検証状態:** MediaReaderは`isStreamFinished(stream)`でproducer終了かつqueue空を判定し、MediaPlaybackControllerへ公開。Contents Viewerは一時empty時に停止せず次の16ms pumpへ持ち越し、本当の終了後のみstop/timer停止する。出力PCMを44.1kHz stereo/Int16へ明示固定し、waveform/FFTのsampleRateも44.1kHzへ合わせた。ArtifactUiTestのWindows POST_BUILDへwindeployqtとvcpkg/Qt runtime deploymentを追加。Windows SDK `x64` binをPATHに加えると、CMake再構成後のArtifactUiTestビルドは終了コード0。WAV captureはja/en/zh/zh-TWすべてで波形と解析値を表示し、Profiler Scopes 5まで進んだ。短い音源のためcapture時は再生終了後で、再生中位置の更新は別途確認が必要。
- **価値／懸念:** SVG空表示はfixtureにQt decoder pluginがなかったことで説明できる。動画独立テストもplugin不在という差分があり、通常アプリ側配布での結論へ一般化できない。動画fixtureはmultimedia plugin pathなしでPlay後も黒く、vcpkg Debug Qt plugin pathを指定すると同じPlay操作でH.264 frameが実画面表示された。これはテストruntime配置が動画の黒画面原因であることを示す。
- **今回の追加確認:** qsvgdとffmpegmediaplugindをArtifactUiTestのpluginsへコピーした後も静止画Previewは空。PNG/SVG fixtureは後続の実験ビルドで`0x80000003`終了を起こし、原因確定に至らなかった。ネイティブサーフェス属性の一時変更は撤回し、現ソースに残していない。画像surfaceはrenderer/device初期化ログだけでは正常表示を証明できない。capture入口はMP4をPlayしないので、Plugin配置後の動画frame表示は今回未検証。過去のA/B実行ではQt multimedia plugin pathを明示しPlayした場合にH.264 frameを確認している。
- **次に確認:** Contents ViewerのGPU previewとComposition Viewのsurface作成・owner階層を比較し、可視captureでPNGのdraw/presentを再現する。Play後のMP4 frameもcapture helperまたはGUI操作で確認する。Audioは再生中に短間隔でplayhead更新を確認する。


## 2026-10-16: Contents Viewer previewのswapchain resizeでDebug Assertion

- **関連:** `Artifact/src/Widgets/Viewer/ArtifactContentsViewer.cppm`、`Artifact/src/Render/ArtifactIRenderer.cppm`、Contents Viewer独立fixture。
- **確認できた事実:** PNG decode後、preview paintはclear/draw/flush/presentまで進むが、160×160から163×163へのresizeでMSVC Debug Runtimeの `vector subscript out of range` assertionが出た。`present()`後にcommand bufferがbackbuffer RTVを次フレーム用に保持し、immediate contextも同targetをbindしたままswapchain resizeしていた。
- **修正・確認:** resize前にcontextのrender target bindingを解除し、command bufferが保持するtarget RTVもnullにしてからswapchainをresizeする。ArtifactUiTestを再ビルド。共有renderer経路でPNGのcheckerとSVG画像をOS画面上で確認し、captureまでassertionなし。別観測の45秒間に複数回resize/recreate/presentが完了した。
- **残る確認:** 共有rendererで長時間の連続異サイズresize stress、MP4 fixture上のPlay/seek、WAV再生中positionの独立操作確認。Widget captureはネイティブGPU surfaceを欠くため画面画像の証拠にはOS desktop captureを使用。
- **関連する未検証の設計案:** GPU viewport widgetのswapchain lifecycleを、present後のRTV保持とresize前のunbindを含めて共通契約として明文化できる可能性がある。現時点ではこの一箇所で得た事実であり、他widgetへ一般化できるかは未検証。
- **次に確認:** Contents ViewerでMP4とWAVの操作確認を行い、異なるwidgetのresize前にも同じRTV所有条件があるかコードから調べる。


## 2026-10-16: Contents Viewer transport表示がnull state labelで早期return

- **関連:** `Artifact/src/Widgets/Viewer/ArtifactContentsViewer.cppm` の `ArtifactContentsViewer::Impl::updatePlaybackState()`。
- **確認できた事実:** `stateLabel` はメンバー初期値nullで、代入箇所が存在しない。一方、`transportTimeLabel`等は生成済み。関数先頭で`stateLabel`がnullならreturnするため、時刻・フレーム表示と再生ボタン有効状態の更新へ到達しない。MP4 fixtureでは再生中フレームが変化し、WAVではwaveform/analysis appendとaudio profiler scopesが観測されたのに、共通transport時刻は0表示に留まった。
- **変更:** 任意`stateLabel`のnullを全transport更新の終了条件にしない。下流は各widgetごとにnull guard済み。
- **価値:** transport time/frame/range表示とplay/pause/stopの有効状態をmedia再生状態に追従させられる。
- **追加修正:** シークスライダーの入力接続を`sliderMoved`から`valueChanged`へ変更した。`QSignalBlocker`で保護したposition同期更新はユーザーseekとして再処理されない。
- **追加確認:** 修正後のArtifactUiTestで20秒WAVのpositionが `00:05 / 00:20` から `00:11 / 00:20` へ進むこと、4秒H.264 MP4で `00:00 / 00:04` と `Frame 0 / 120` が表示され、Play後に映像とframe番号が進むことを確認した。seek sliderにフォーカスしてRightを押すとpositionが1秒、frame 30/120へ移った。マウスドラッグシーク、長時間resize stressは未確認。
- **次に確認:** WAVのpause/seek後の保持とMP4のマウスドラッグseekを確認する。長時間の連続異サイズresize stressも実行する。

## 2026-10-16: Composition Surface Cache の検証では render-key suppression と区別する

- **関連:** `Artifact/src/Widgets/Render/ArtifactCompositionRenderController.cppm`、`tests/Artifact/CompositionLayerSurfaceCacheMain.cpp`。
- **確認できた事実:** Layer Surface Cache はレイヤー描画内の `drawLayerForCompositionView()` 経路で lookup される一方、同一状態 render key の再描画は上位で full composite を省略できる。従って、単に `markRenderDirty()` を呼んで再描画要求しただけでは surface cache hit の実証にならない。
- **対応:** 独立 fixture は1px panを往復して view composite を再実行し、panを戻した状態で同じレイヤー surface cache を再利用する受入経路を用意した。
- **価値または懸念:** 同一画面で描画完了だけを測るテストは cache を通らない偽陽性になり得る。view差分と layer-content invalidation を別の操作として記録する必要がある。
- **未検証:** render-key 経路を通過して surface lookup/hit が実際に観測されるかは、新 target のビルドおよび実機実行が必要。既存 build target は ArtifactCore の module IFC 重複エラーで新コードの compile 前に停止した。
- **次に確認:** IFC 重複の出所を確認してから target をビルドし、`cache.runRegression` の hit / invalidation / unrelated-edit assertion と capture を GPU runtime で実行する。

## 2026-10-16: プリコンポーズ解除のkeepCompositionは空の子を残す

- **関連:** `Artifact/src/Service/ArtifactProjectService.cppm` の `unprecomposeLayerInCurrentComposition()`、`tests/Artifact/PrecomposeViewerMain.cpp`。
- **確認できた事実:** `keepComposition=true` は子Composition自体の削除を止めるが、子レイヤーは親へ移動する。結果として子Compositionは空になり、Undoでレイヤーが子へ戻る。
- **価値／懸念:** 「子を保持」という表現を元の内容も残す意味と誤読すると、テストが誤った内容保持を期待し得る。レイヤーeffect保持テストの解除後条件を空childに合わせて修正した。
- **次に確認:** UI文言・API引数名とこの意味が一致しているかは未確認。必要なら別途ユーザー向け文言を確認する。

## 2026-10-16: ShapeExtrude の穴付きキャップは穴を横切る可能性

- **関連:** `ArtifactCore/src/Geometry/ShapeExtrude.cppm` のキャップ三角形生成、`tests/ArtifactCore/ShapeExtrudeTest.cpp`。
- **確認できた事実:** 現実装は外周を ear clipping した後、三角形の重心が穴内にある面だけを削除する。三角形全体と穴輪郭の交差を判定してはいない。
- **価値／懸念:** 重心が穴外でも面が穴の一部を横切る場合、キャップが穴を塞ぐ可能性がある。独立テストでは穴中心を含むキャップ三角形がないことを受け入れ条件にした。
- **未検証:** この穴形状で現在の実装が実際に失敗するか、コンパイル・実行では確認していない。
- **次に確認:** ビルドが許可された際にShapeExtrude独立テストを実行し、失敗する場合はポリゴンと穴の交差を考慮する三角形化方法を別途検討する。

## 2026-10-16: ShapeExtrude は不正輪郭を個別に読み飛ばす

- **関連:** `ArtifactCore/src/Geometry/ShapeExtrude.cppm` の輪郭正規化、`tests/ArtifactCore/ShapeExtrudeTest.cpp`。
- **確認できた事実:** 正規化ループは重複頂点を含む輪郭を`continue`で除外し、別に有効な輪郭が残れば生成処理を継続する。一方公開APIコメントは不正・退化輪郭でfalseを返すと記載している。
- **価値／懸念:** 不正な穴輪郭などが黙って失われると、返却成功でも意図しない塗りつぶし形状になり得る。テストは有効な外周と重複点輪郭を同時に渡し、失敗時に既存出力が維持される契約を確認する。
- **未検証:** 独立テスト実行前の静的確認であり、現在の失敗挙動は実行で未確認。
- **次に確認:** ビルドが許可された際に失敗を再現し、輪郭群全体の入力検証方針を別途判断する。

## 2026-10-16: AudioRingBuffer の12ch対応分岐は書き込み上限と不一致

- **関連:** `ArtifactCore/include/Audio/AudioSegment.ixx`、`ArtifactCore/src/Audio/AudioRingBuffer.cppm`、`tests/ArtifactCore/AudioRingBufferStandaloneTest.cpp`。
- **確認できた事実:** `AudioChannelLayout::Surround714`とリングバッファ読取側の12ch→Surround714分岐があるが、書き込み側は`kMaxChannels = 10`で12ch入力を拒否する。
- **価値／懸念:** Surround714のセグメントをリングバッファ経由で再生できない可能性がある。テストで12chの書き込み・読出し・サンプル保持を要求する。
- **未検証:** 入力拒否の挙動とテスト結果は実行未確認。下流コードは変更していない。
- **次に確認:** ビルドが許可された際に失敗を確認し、リングバッファ容量・チャンネル固定ストレージ・音声出力経路全体で12ch対応の意図を別途判断する。

## 2026-10-16: AssetManager の未知ID無効化はregistry entryを生成する

- **関連:** `ArtifactCore/src/Asset/AssetManager.cppm` の`invalidateSource()`、`tests/ArtifactCore/AssetManagerContractTest.cpp`。
- **確認できた事実:** nullでないIDについて`impl_->sources[assetId]`で状態を取得するため、未知IDにもversion 1のSourceStateが作成される。`sourceVersion()`は本来registryにないIDへ0を返し、`acquireExistingSource()`は未知IDを拒否する。
- **価値／懸念:** 誤った・古いIDのinvalidate呼び出しがsource registryへ参照数0の幽霊状態を残す可能性がある。回帰テストでは未知IDを無効化してもversion 0のまま、既存sourceとして取得不能であることを確認する。
- **未検証:** 現在の結果と実行時影響はテスト未実行。下流コードは変更していない。
- **次に確認:** ビルドが許可された際に失敗を確認し、無効化呼び出し側が必ず登録済みIDを渡すか追跡する。

## 2026-10-16: AudioVolume deserialize は保存値を読み戻さない

- **関連:** `ArtifactCore/src/Audio/AudioVolume.cppm` の`serialize()` / `deserialize()`、`tests/ArtifactCore/AudioVolumeStandaloneTest.cpp`。
- **確認できた事実:** `serialize()`はvolumeとmutedを含むJSON文字列を生成するが、`deserialize()`の実装は引数を使わず常に`AudioVolume(1.0f)`を返す。
- **価値／懸念:** 音量設定を保存して再読込すると非既定値が失われる。回帰テストでは0.42の値が往復後に保持されることを要求する。
- **未検証:** テストは実行していない。現実装では失敗することを静的なコード読解から予想している。
- **次に確認:** ビルドが許可された際に失敗を確認し、JSONの不正入力・旧形式に対する既定値方針を別途定める。

## 2026-10-16: AudioDecibels deserialize も保存値を読み戻さない

- **関連:** `ArtifactCore/src/Audio/AudioDecibels.cppm` の`serialize()` / `deserialize()`、`tests/ArtifactCore/AudioDecibelsStandaloneTest.cpp`。
- **確認できた事実:** `serialize()`はdB値をJSON文字列に書くが、`deserialize()`は引数を使わず常に`AudioDecibels(0.0f)`を返す。AudioVolumeと同じ形式の欠落。
- **価値／懸念:** dB音量値も保存・復元で失われる。回帰テストでは-12.75 dBの往復保持を要求する。
- **未検証:** テスト未実行。現実装では失敗するとコード読解から予想している。
- **次に確認:** ビルドが許可された際に両音量型のserialize/deserialize挙動と不正JSON時の既定値を確認する。

## 2026-10-16: RawImage の validity は記述サイズとpixel buffer長を照合しない

- **関連:** `ArtifactCore/src/Image/RawImage.cppm` の`RawImage::isValid()`、`tests/ArtifactCore/RawImageContractTest.cpp`。
- **確認できた事実:** 既存の`RejectsPixelBufferShorterThanDescribedImage`は1×1 RGBA float画像に15 byteを設定し無効を期待する。一方、現実装は正の寸法・channels・非空data・pixelTypeだけを確認し、画素サイズとdata長を照合していない。
- **価値／懸念:** descriptorに対して切り詰められたpixel bufferがvalid扱いになる可能性がある。既存テストが回帰要求を既に表現しているため、追加の重複ケースは不要。
- **未検証:** テスト結果は未実行。コード読解では現テストの期待と実装が不一致。ArtifactCore childは変更していない。
- **次に確認:** 対象suiteをビルド・実行できる環境で失敗を確認し、必要byte数の計算で整数overflowを安全に扱う契約も確認する。

## 2026-10-16: GI frame resource descriptor はresource種別ごとにchannel layoutが異なる

- **関連:** `ArtifactCore/include/Graphics/GIResources.ixx` の`GIFrameResources::descriptor()`、`tests/ArtifactCore/RenderPipelineContractTest.cpp`。
- **確認できた事実:** `GIResourceKind`はDepth/Normal/Motion/DirectLighting/IndirectLighting/Historyの6種で、descriptorはDepthを1 channel、残りを4 channelとして返す。既存テストはHistory descriptorだけを検査していた。
- **価値／懸念:** 他の種別での誤ったchannel数や寸法伝播を捕捉するため、全種別を一括で検査するtable-driven testを追加した。
- **未検証:** テストは未実行。期待値は現行headerの分岐から確認した。
- **次に確認:** 次回対象buildが利用可能なときに`RenderPipelineContractTest`を実行する。

## 2026-10-16: FrameCache の容量上限は設定時と明示trimの契約を分けて確認する

- **関連:** `Artifact/src/Render/ArtifactFrameCache.cppm` の`setMaxFrameCount()` / `trimToSize()` / `put()`、`tests/Artifact/FrameCacheContractTest.cpp`。
- **確認できた事実:** 既存契約テストは通常の上限超過時evictionとoversized entry拒否を確認していたが、zero frame capacity時の拒否と明示trim時のbyte目標到達は確認していなかった。
- **価値／懸念:** hot cacheは通常store pathだけでなく、設定変更・memory pressure対応で呼ばれるtrimの境界も回帰防止できる。zero-capacity拒否時の状態不変とFIFO trim後の残存entryを追加した。
- **未検証:** テストは未実行。期待値は現行FrameCache実装とentryの16-byte画素領域から導いた。
- **次に確認:** `ArtifactFrameCacheContractTest`を実行し、trim target以下への到達とcount/bytesの整合を確認する。

## 2026-10-16: RenderPipeline contract単体targetもCore module dependency再構築で広域compileを要求する

- **関連:** `out/build/x64-Debug-Agent` の`ArtifactCoreRenderPipelineContractTest` target、`ArtifactCore/src/Codec/MFEncoder.cppm`、`ArtifactCore/src/Animation/AnimatableTransform3D.cppm`。
- **確認できた事実:** 対象target buildはCMake再構成なしでNinjaから開始できたが、Core依存moduleの再構築が発生し、対象test compile前の185/603段階で上記2 translation unitのmodule compile errorsにより停止した。
- **価値／懸念:** 個別契約targetを選んでも既存生成物の古さにより広いCore rebuildが必要になり、今回追加したテストのコンパイル結果を得られなかった。
- **未検証:** compile diagnosticsは出力上で確認したが、根本原因の切り分け・修正は行っていない。関連submodule/製品コードは変更していない。
- **次に確認:** 依存module側の既存修正または有効なbuild stateが利用可能になった後、同じtargetを再実行する。

## 2026-10-16: Undo history byte budget eviction と entry-count eviction は別契約

- **関連:** `Artifact/src/Undo/UndoManager.cppm` の`enforceBudget()`、`tests/Artifact/UndoManagerContractTest.cpp`。
- **確認できた事実:** UndoManagerはentry数超過と総estimated byte数超過の両方で古い履歴を先頭から退避するが、既存追加テストはentry count制約だけを対象としていた。
- **価値／懸念:** byte budget側だけでredo stackとの合算や古いUndoの退避順が壊れる回帰を見逃し得る。単一command相当のbyte上限で2回pushし、最新commandのUndo/Redoとpressure上限を検査するケースを追加した。
- **未検証:** AppRuntime依存targetのため未ビルド・未実行。期待値は現実装の`estimatedMemoryBytes()`と`enforceBudget()`から確認した。
- **次に確認:** 依存module build blockerが解消した後、UndoManager contract suiteを実行する。

## 2026-10-16: Schema migration callback exception can expose a partially migrated object

- **関連:** `ArtifactCore/include/Serialization/SchemaMigration.ixx` の`SchemaMigrationRegistry::findPath()`、`tests/ArtifactCore/SchemaMigrationContractTest.cpp`。
- **確認できた事実:** migration path実行はcallbackごとに返却JSONを参照引数`data`へ代入する。後続callbackがthrowすると、先行stepの更新がすでに呼び出し元objectへ反映されている。
- **価値／懸念:** load/import側が失敗後のobjectを再利用すると、中間versionデータを誤って有効データとして扱う可能性がある。後段callback例外で入力が開始時状態のままという原子性testを追加した。
- **未検証:** テスト未実行。現実装と期待の不一致は静的に確認した。ArtifactCore childは変更していない。
- **次に確認:** 独立SchemaMigration suiteを実行して再現し、必要なら移行全体を一時objectで行う方針を設計判断する。

## 2026-10-16: Existing standalone Ninja build may lack the MSVC include environment

- **関連:** `out/build/standalone-schema-migration-vs`、`ArtifactCoreSchemaMigrationContractTest`。
- **確認できた事実:** 既存Ninja targetはconfigure不要で起動したが、compile commandのcl.exeが`gtest/gtest.h`を開いた後、MSVC標準header `cstddef`を見つけられずC1083で停止した。`Launch-VsDevShell.ps1`もDeveloper Shell assemblyの解決に失敗した。
- **価値／懸念:** standalone targetのテストコード変更を検証するには、対象hostで有効なMSVC compiler include environmentが必要。実行ファイルは再生成されず、新規caseは未検証のまま。
- **未検証:** Visual Studio installationの修復・環境変数の手動構成は行っていない。
- **次に確認:** 動作するDeveloper PowerShell/VS environmentから同じbuildを実行する。

## 2026-10-16: Failed project replacement should preserve both Undo and Redo branches

- **関連:** `tests/Artifact/ArtifactProjectRoundTripTest.cpp` の`SaveLoadGuardsRejectBadInput`、`Artifact::ArtifactProjectManager::loadFromFile()`。
- **確認できた事実:** 既存の不正replacement-load検査はactive project pointer、Composition、Layerを維持することを確認するが、global UndoManagerの既存history stackは比較していなかった。
- **価値／懸念:** failed importが現Projectを維持しつつUndo historyだけ消すと、ユーザーが直前操作を戻せなくなる。2件push後に1件undoし両stackを非空にしてから不正loadし、stack countsと残るcommand順序を確認するcaseを追加した。
- **未検証:** AppRuntime依存のProject RoundTrip suiteは未ビルド・未実行。実際のimporterがUndo historyに触るかはruntime未確認。
- **次に確認:** ArtifactAppRuntime buildが通る環境で`ArtifactProjectRoundTripTest.SaveLoadGuardsRejectBadInput`を実行する。

## 2026-10-16: Effect stage stability should be checked through actual CPU pixel output

- **関連:** `Artifact/src/Effect/AbstractEffect.cppm`、`tests/Artifact/EffectSequenceContractTest.cpp`。
- **確認できた事実:** 既存suiteはstable stage sortの同stage pointer順と、別ケースでEffectの手動適用順によるpixel差を確認していたが、sort結果をそのまま適用する経路での同stage pixel契約は結合していなかった。
- **価値／懸念:** stable sortが正しくても後続stack実行が別順序を使う回帰を取り逃す可能性がある。同stageの非可換Brightness/Invertをsort経由でrenderし、順序ごとのRGBとalphaを検査するケースを追加した。
- **未検証:** CPU contract target未実行。Effect stage APIと既存pixel期待値から静的に構成した。
- **次に確認:** ArtifactEffectsColor依存targetがbuild可能な環境でsuiteを実行する。

## 2026-10-16: Mask/effect order is clearer with a same-pixel mask-only baseline

- **関連:** `tests/Artifact/RasterizerMaskEffectIntegrationTest.cpp`、`Artifact/src/Render/ArtifactCompositionViewDrawing.cppm` の`buildRasterizedSurfaceBuffer()`。
- **確認できた事実:** 実装はmaskをsurfaceへ適用した後にRasterizer effectを順次適用する。既存testはBlur後の内部/境界/遠方外部alpha勾配を確認するが、blur無しのmask edge baselineとは比較していなかった。
- **価値／懸念:** edge antialiasだけでboundary assertionを満たす可能性があり、mask後にblurが広げる寄与を分離できていなかった。同じfixtureをmask-onlyでも描画し、mask外の同一pixel alphaが0からblur時に増加する比較を追加した。
- **未検証:** `ArtifactAppRuntime`依存integration suiteは未実行。現実装のmask→effect順序とradius=4 fixtureから期待を構成した。
- **次に確認:** integration targetを起動可能なGPU/Qt test環境で実行する。

## 2026-10-16: Track Matte deletion should preserve unrelated source references

- **関連:** `tests/Artifact/ArtifactProjectRoundTripTest.cpp` のTrack Matte source deletion case。
- **確認できた事実:** 既存fixtureはtargetに1つのexternal source referenceだけを作り、そのsource削除後にreference count 0を確認していた。無関係なmatte referenceを保持することや、surviving referenceの保存往復は対象外だった。
- **価値／懸念:** source削除時にtargetの全matte referencesを消す回帰、または残ったreferenceのmetadataを失う回帰を検出できない。第2sourceを追加し、削除後に参照ID・opacity・invertを確認し、save/reload後も保持するケースへ拡張した。
- **未検証:** ArtifactAppRuntime依存のProject RoundTrip suiteは未実行。Artifact childは変更していない。
- **次に確認:** `ArtifactProjectRoundTripTest.RemovingMatteSourceClearsOnlyDanglingReferenceAndPreservesSurvivorRoundTrip`を実行する。

## 2026-10-16: Still-image path replacement should also be checked when dimensions stay constant

- **関連:** `tests/Artifact/StillImageLayerRoundTripTest.cpp` の`LoadingAnotherStillImageReplacesPixelsAndKeepsLayerEdits`。
- **確認できた事実:** 既存replacement testは2×1画像から3×2画像への切替でpixelと寸法更新、opacity/position保持を確認していた。別caseでは同じpathの内容変更をsource versionで再読込する。
- **価値／懸念:** pathを変えて同じ寸法の画像へ切り替えたときに古いpixel bufferを再利用する回帰は、寸法差で更新できる既存caseでは捕捉できない。3×2の別path・異色画像へ再差し替え、編集値保持も確認するcaseを追加した。
- **未検証:** ArtifactAppRuntime依存のStillImageLayer suite未実行。
- **次に確認:** `StillImageLayerRoundTripTest.LoadingAnotherStillImageReplacesPixelsAndKeepsLayerEdits`を実行する。

## 2026-10-16: Consecutive missing sequence frames must not retain a just-reconnected frame

- **関連:** `tests/Artifact/StillImageLayerRoundTripTest.cpp` の`MissingImageSequenceFrameDoesNotKeepStalePixels`。
- **確認できた事実:** 既存testは欠番1 frameをplaceholder化し、そのpathを復旧して再読込するが、復旧直後の次frameが連続欠番の場合は検査していなかった。
- **価値／懸念:** sequence frame stateが前frame bufferを再利用すると、直前に復旧したpixelが次のmissing frameへ漏れる可能性がある。第2の欠番を追加し、repair pixelをcacheした後に移動するとplaceholderへ戻ることを検査する。
- **未検証:** ArtifactAppRuntime依存suite未実行。
- **次に確認:** `StillImageLayerRoundTripTest.MissingImageSequenceFrameDoesNotKeepStalePixels`を実行する。

## 2026-10-16: Relinking a missing still-image source should preserve layer edits

- **関連:** `tests/Artifact/StillImageLayerRoundTripTest.cpp` の`MissingSourceCanBeRelinkedAfterProjectReload`。
- **確認できた事実:** 既存caseはmissing source pathとplaceholderのProject往復、同pathへのsource復旧後の画素読込を確認していたが、opacityとpositionの保持を比較していなかった。
- **価値／懸念:** source切断/再接続やProject reload処理がlayer編集値を初期化する回帰を取り逃す可能性がある。missing化前にopacity/positionを設定し、reload後とrelink後の両方で保持を確認するassertを追加した。
- **未検証:** AppRuntime依存のStillImageLayer suiteは未実行。Artifact childは変更していない。
- **次に確認:** `StillImageLayerRoundTripTest.MissingSourceCanBeRelinkedAfterProjectReload`を実行する。

## 2026-10-16: Duplicated layer properties should be independent after copying

- **関連:** `tests/Artifact/ArtifactProjectRoundTripTest.cpp` の`DuplicateLayerCreatesIndependentIdAndCopiesProperties`。
- **確認できた事実:** 既存Project integration caseは新しいIDと初期opacityの複写を確認していたが、複製側opacityを編集しても元レイヤーが変わらないことは確認していなかった。
- **価値／懸念:** 複製後にproperty storageを共有する回帰を取り逃す可能性がある。複製opacityを変更し、複製値と元値が独立しているassertを追加した。
- **未検証:** ArtifactAppRuntime依存のProject RoundTrip suiteは未実行。Artifact childは変更していない。
- **次に確認:** `ArtifactProjectRoundTripTest.DuplicateLayerCreatesIndependentIdAndCopiesProperties`を実行する。

## 2026-10-16: Stop should freeze the playback frame clock

- **関連:** `tests/Artifact/PlaybackRangeServiceContractTest.cpp` の`TransportTransitionsPlayPauseResumeAndStop`。
- **確認できた事実:** 既存caseはStopと`waitForStop()`後のStopped stateを確認するが、Stop後にframe位置が安定するかは検査していなかった。
- **価値／懸念:** stateだけStoppedになりframe timerが進み続ける回帰を取り逃す可能性がある。Stop完了後に100msのevent processing窓を設けてframe位置が不変であることを確認する。
- **未検証:** Playback Service targetはAppRuntime依存で未実行。テストは実時間待機を既に使うsuiteへboundedな100ms観測を追加した。
- **次に確認:** `PlaybackRangeServiceContractTest.TransportTransitionsPlayPauseResumeAndStop`を実行する。

## 2026-10-16: Resized viewport should remain within the requested window bounds

- **関連:** `tests/Artifact/CompositionPlaybackStressMain.cpp` の`resizeWhilePlaying`。
- **確認できた事実:** 既存resize harnessは再生継続・frame進行・Viewport寸法が正であることを確認していたが、Viewportが要求window寸法より大きくなっていないことは確認していなかった。
- **価値／懸念:** resize後のlayout overflowを正寸法だけでは検出できない可能性がある。実viewportはcentral areaでwindow chromeより小さい前提のため、幅・高さがそれぞれ要求値以下であるassertを追加した。
- **未検証:** Windows GPU/GUI stress harness未実行。実行時にplatform frame geometryがviewportのsizeへ影響しないか確認が必要。
- **次に確認:** `stress.resizeWhilePlaying`を起動可能なGUI環境で要求寸法を変えて実行する。

## 2026-10-16: Composition cycling should explicitly assert its return point

- **関連:** `tests/Artifact/CompositionPlaybackStressMain.cpp` の`cycleCompositions`。
- **確認できた事実:** 既存巡回ループは算術上開始Compositionへ戻るが、応答の`passed`と出力には最終positionの明示確認がなかった。
- **価値／懸念:** loop順序変更時に全transitionが通っても開始点へ戻らない回帰を見逃す可能性がある。最後のactive indexとPlayback Service current Compositionを開始点と比較し、出力・合否へ加えた。
- **未検証:** Windows GPU/GUI stress harness未実行。既存test exe起動も`0xC0000135`でloaderが必要DLL不足を報告し、suiteは未起動。
- **次に確認:** `stress.cycleCompositions`を2つ以上のCompositionで実行し、`returnedToStartComposition`を確認する。

## 2026-10-16: Standalone tests can run with the repository's bundled DLL paths

- **関連:** standalone `CompositionNodeStore`, `FrameRate`, `PropertyPath`, `RationalTime` test targets。
- **確認できた事実:** standalone executables initially exited with Windows loader status `0xC0000135`. The runtime DLLs were available under `out/build/raytracing-acceptance/vcpkg_installed/x64-windows/{debug/bin,bin}` and the VS 18 CRT directory. Adding those locations to the process-local PATH allowed suites to start; no repo configuration or files needed changing.
- **価値／懸念:** this path can make further independent CPU suites testable without reconfiguring CMake. FrameRate passed 34/34, PropertyPath 7/7, CompositionNodeStore 8/8; RationalTime reproduced two signed overflow failures (24/26).
- **未検証:** other executables may depend on additional DLLs or Qt platform plugins. These outcomes apply to the existing build outputs; only CompositionNodeStore was confirmed up to date by Ninja reporting no work.
- **次に確認:** reuse the process-local DLL search path for other already-built, low-dependency standalone targets, and rebuild narrow targets when source updates require it.

## 2026-10-16: Schema migration callback exceptions currently leave partial input mutations

- **関連:** `tests/ArtifactCore/SchemaMigrationContractTest.cpp` の`FailedCallbackLeavesInputUnchanged`と`SchemaMigration::migrate()`。
- **確認できた事実:** isolated targetをrebuildし、新test込みで実行した結果4/5 pass。後段callback例外時に、migration前JSONへ戻らず1段目のfield変更が残ることをfailure outputで確認した。
- **価値／懸念:** callerが例外を捕捉しても部分更新済みデータを誤って継続利用する可能性がある。入力atomicityをtestとして固定したが、実装修正はArtifactCore child repo変更禁止のため未実施。
- **未検証:** 子repoの修正後に再実行してpassすること。最終linkは一時`LIB`設定で成功した。
- **次に確認:** 明示的にArtifactCore変更が依頼された段階でcopy-and-commit方式などmigration原子性を設計し、単体testを再実行する。

## 2026-10-16: The RawImage test target in the app build still pulls the full Core module graph

- **関連:** `tests/ArtifactCore/RawImageContractTest.cpp` と`out/build/x64-Debug-Agent`の`ArtifactCoreRawImageContractTest` target。
- **確認できた事実:** targetは既存buildに登録されているが、build時にArtifactCore 425-step dependencyを引き、対象testより前に`AnimationDynamics.cppm`と`AnimatableTransform3D.cppm`のmodule compile failuresで停止した。
- **価値／懸念:** standalone形式のtest名でも実際のtargetが小さい依存境界を意味するとは限らない。広域build失敗の証拠を得たため、同じbuild treeからさらにAppRuntime/Core依存targetを積み増して試さない。
- **未検証:** `RawImage::isValid()`短buffer拒否の追加期待はruntime未実行。現実装はdata非空だけを確認する。
- **次に確認:** ArtifactCoreが健全なbuild stateになった後にのみ、既存targetでRawImage contractを実行する。

## 2026-10-16: New keyframe cases were missing from the minimal standalone test project

- **関連:** `tests/ArtifactCore/KeyframeSplineTest.cpp`、`tests/ArtifactCore/KeyframeStandalone/CMakeLists.txt`。
- **確認できた事実:** `KeyframeStandalone`は`Math.Interpolate` moduleだけを登録するが、gtest sourcesには既存`KeyframeContractTest.cpp`のみが含まれ、追加済み`KeyframeSplineTest.cpp`はtop-level full-Core targetからしか実行できなかった。
- **価値／懸念:** 高優先の追加補間ケースをアプリ/Core全体のbuild blockerから分離できない。既存standalone targetに`KeyframeSplineTest.cpp`を加えた。
- **未検証:** standalone projectのconfigure/build/testはユーザーの明示的CMake許可待ち。
- **次に確認:** target再構成後にkeyframe contract suite全体を実行する。

## 2026-10-16: レイヤーマスクの独立テストは演算・状態・描画を分ける

- **関連:** `tests/mask_visual/MaskVisualTest.cpp`、`Artifact/src/Mask/LayerMask.cppm`、`Artifact/src/Layer/ArtifactLayerSerialization.cppm`、Composition mask render path。
- **確認できた事実:** 既存 `ArtifactMaskVisualTest` は `LayerMask::compositeAlphaMask()` のCPU演算を画像出力と数値assertで検査する。FBXの独立GUI Viewerが扱うimport／animation表示とは異なり、CPU mask契約だけではlayerへの保持、保存復元、Composition描画での適用を確認できない。
- **価値／懸念:** 一つのGUIテストへ全責務を集めず、演算回帰・layer state/persistence・実Composition captureを独立実行可能にすれば、失敗箇所を切り分けやすい。既存CPUケースとの重複やComposition Surface Cacheテストとの責務重複を避ける必要がある。
- **次に確認:** `docs/planned/MILESTONE_LAYER_MASK_STANDALONE_TEST_2026-10-16.md` を基に、現行APIとfixture経路を実装前に照合する。テストは計画段階であり、ビルド／実行未確認。

## 2026-10-16: Unscanned module attachments need explicit IFC references

- **関連:** `ArtifactCore/CMakeLists.txt` の `ARTIFACTCORE_IMPLEMENTATION_MODULE_REFERENCES` 処理。
- **確認できた事実:** implementation `.cppm` は `CXX_SCAN_FOR_MODULES OFF` にされている。コンパイルコマンドに `/reference` がなければ、IFCファイルがディスク上にあっても `module Name;` や `import Name;` は解決できない。`src/Color/` と `src/Core/` 全体を対象とする早期分岐が各ファイル専用の参照設定を遮っていた。
- **価値／懸念:** ディレクトリ単位の省略分岐は、個別IFC参照とBMI producer順序の設定まで無効化し、コンパイル時に多数の連鎖エラーを生む。
- **確認結果:** Visual Studio同梱CMake 4.3.1で再生成し、`AnimatableTransform2D.cppm`、`AnimationDynamics.cppm`、`FloatRGBA.cppm` の3オブジェクトをMSVCでコンパイルできた。`AnimatableTransform2D` ではprimary interfaceと推移importのIFC参照、およびproducer objectの順序を確認した。
- **次に確認:** ArtifactCoreテストターゲットを再ビルドし、同様の未参照IFCが別のディレクトリ／分割ターゲットに残っていないか確認する。
- **再確認（2026-10-17）:** 現在の `out/build/x64-Debug-Agent` Ninja command は `AnimatableTransform2D.cppm` を `CXX_SCAN_FOR_MODULES=OFF` でコンパイルする一方、実際の `cl.exe` command に `Animation.Transform2D` / `Animation.Value` の `/reference` がなく、両 IFC が生成済みでも C2230 で失敗した。ArtifactCore の未コミット CMake 変更では「CMake が provider refs を付ける」とする分岐になっているが、この Ninja command には付いていない。試験的に Ninja 専用の2参照を加えるとこの2件を越え、次に `Container.NamedVector` の不足と、同様に参照がない `AnimationDynamics.cppm` / `Channel.cppm` が顕在化した。試験パッチは取り消し済みで、Core の既存変更は保持している。
- **次に確認:** ArtifactCore CMake の現在の未コミット変更者と意図を確認し、unscanned unit ごとの provider IFC と producer dependency を整合させたうえで Ninja target を再ビルドする。今回の成果物検証を続けるにはこの依存 build blocker の解消が必要。
- **追確認（2026-10-17）:** app target の実ビルドでは SceneNode の aggregate/split provider 重複、scanner-enabled source への手動 import 重複、VectorExport の split target ownership と export-import 閉包不足が別々に C7684/C2230 を起こした。consumer と provider の出力 directory を分離し、scanner-enabled translation unit は手動 import table から除外、分割先を module/interface owner から解決する必要がある。VectorExport の再公開 import は Shape.Layer → Shape.Group / Property.Types → Shape.Operator まで必要になった。修正後は Artifact と現在の worker image contract test target のビルドが完了し、worker image case は D3D12/Vulkan 両方で pass。別の非コンパイル blocker として、ビルド環境 PATH に Windows SDK rc.exe がなく test DLL の link で停止したため SDK x64 bin を加えて通した。
- **未検証の仮説:** CMake configure 後も test target build が2,131 action の広い module graph を再構築した。target dependency の実体が広いのか、再configure後の dyndep invalidation が主要因なのかは未分離。次は warm Ninja build を同一 preset で比較し、aggregate Core に残る source 数と renderer source の依存閉包を計測してから追加分割候補を決める。

## 2026-10-16: Dialog confirm throttling belongs at the application event boundary

- **関連:** `Artifact/src/AppMain.cppm` の `DialogLatencyEventFilter` とアプリ共通ダイアログ。
- **確認できた事実:** アプリには `QApplication` に登録されたイベントフィルターがあり、`QDialogButtonBox` の確定 role と既定ボタンを共通に検出できる。各ダイアログの OK 処理へ個別ガードを追加すると実装漏れが起きやすい。
- **価値／懸念:** ダイアログ単位で確定入力を短時間抑止すれば、同じボタンだけでなく別の確定ボタン経由の連続実行も防げる。入力イベントをフックする方式は `QAbstractButton::click()` の直接呼び出しには適用されない。
- **次に確認:** UI 上でマウス・キーボードの連続確定と入力エラー後の再試行が正常に動くことを確認する。

## 2026-10-16: Layer creation emitted the same Created event twice

- **関連:** `Artifact/src/Composition/ArtifactAbstractComposition.cppm::appendLayerTop()`、`Artifact/src/Project/ArtifactProject.cppm::createLayerAndAddToComposition()`、平面レイヤー Undo 登録。
- **確認できた事実:** `appendLayerTop()` が `LayerChangedEvent::Created` を publish し、成功した `ArtifactProject::createLayerAndAddToComposition()` も同じ composition ID / layer ID で同イベントを再 publish していた。平面レイヤーの作成後に使う Project Item Undo 情報の組み立てでは、同一 layer の `toJson()` を2回呼んでいた。
- **変更:** Project の wrapper から重複 publish を除き、平面レイヤーの Undo 情報を一度の `toJson()` 結果から作るようにした。
- **価値／懸念:** 作成イベントの同期購読者による重複更新を避ける。`appendLayerTop()` が通常どおり通知する既存契約に依存するため、別の通知抑止経路を使う呼び出しではイベントが必要か確認する。
- **次に確認:** UI で平面レイヤー追加後の Timeline / Inspector / Project View 更新が一度で完了し、Undo / Redo と選択が維持されること、作成時間の実測が改善することを確認する。

## 2026-10-16: Successful creation diagnostics synchronously flushed every phase

- **関連:** `Artifact/include/Diagnostics/WidgetCreationDiagnostics.hpp::record()`、composition/project creation phase reporting。
- **確認できた事実:** `recordPhase()` は outcome を `measured` として渡し、従来の flush 条件 `outcome != created || totalMs >= 50` は正常な全 phase record の書き込み直後に `QFile::flush()` を実行していた。
- **未検証の仮説:** composition 作成中の複数回の同期 flush が UI の体感待ちへ寄与していた可能性がある。ディスクキャッシュや保存先による実時間差は未計測。
- **変更:** `created` と `measured` はバッファリングし、失敗など例外 outcome のみ即時 flush する。
- **価値／懸念:** 正常な診断記録を残したまま同期ディスク待ちを減らす。異常終了時は、最後の buffered record がプロセス終了前に未flushなら失われる可能性がある。
- **次に確認:** 実機で composition 作成時間を複数回測定し、プロセス終了後に正常・失敗記録の両方がファイルに残ることを確認する。

## 2026-10-16: BGRA backing requires auditing every raw channel-index boundary

- **関連:** `ArtifactCore/src/Image/ImageF32x4_RGBA.cppm`、`Artifact/src/Effects`、`Artifact/src/Render/ArtifactRenderLayerPipeline.cppm`。
- **確認できた事実:** BGRA backingへ揃えたGPU upload/readbackだけでは色の一貫性は保証されない。CPU fallbackの輝度係数、OpenCV conversion code、画素indexでの赤青指定、GPU resident shaderの入力/出力値順、QImage export write orderが同時にその契約を使う必要がある。今回の追加静的監査ではDeflicker、Radial Shadow、Rim Light、Kaleidoscope、NCC tracker、Liquid Glow、Luminescence Caustics、Offscreen exportに漏れが見つかった。
- **価値／懸念:** 次回の全画像形式変更では型/APIの統一だけで完了とせず、CPU、OpenCV、GPU shader、描画境界ごとに非対称なRGB値で静的分類と画素比較を行う必要がある。現在はbuild/runtime/pixel parity未確認。
- **次に確認:** 全rendererの`drawSprite(ITextureView*)`に渡すtextureのchannel orderをproducerまで追跡し、BGRA image textureとRGBA external textureの境界が明示されているか調べる。

## 2026-10-16: External color APIs need named channel-order adapters

- **関連:** `ArtifactCore/src/Image/FFmpegEncoder.cppm`、`ArtifactCore/src/Image/ImageYUV420.cppm`、`Artifact/src/Effects/Ofx/ArtifactOfxEffectImpl.cppm`、`Artifact/src/Color/ArtifactOCIOManager.cppm`。
- **確認できた事実:** BGRA内部契約を整えても、RGBAを明示するOFX ABI、OCIO packed image、FFmpeg RGBA staging frame、OIIOのR/G/B/A channel namesへ生ポインタを渡すと外部境界で色が崩れる。FFmpeg staging frameには行strideがあり、幅だけで平坦走査するとpaddingも無視される。
- **価値／懸念:** raw pointer boundaryにdescriptorから独立した名前付きadapterを用意すると、新しい外部APIを追加したときの見落としを減らせる。現状この変更では各境界個別に実装しており、共通adapter化は未実施・未検証。
- **次に確認:** 同一の非対称色fixtureで各外部APIへの出力を確認し、繰り返し使うRGBA/BGRA境界処理を共通APIへ寄せる価値を測る。

## 2026-10-16: Shared distortion sampling must return logical colors

- **関連:** `ArtifactCore/src/ImageProcessing/Distortion.cppm` のbilinear/nearest sampling、morph、displacement。
- **確認できた事実:** 共通sample helperはraw channel 0/1/2をそのままFloatRGBAのr/g/bとして返しており、BGRA backingでは赤青が入れ替わる。morph/displacement出力も論理RGBA順を書いていた。
- **価値／懸念:** sample helperの一箇所の順序誤りが、複数distortion effectに共有される。descriptor-aware read/writeでRGBAとBGRA双方を扱い、出力descriptorをsourceから引き継ぐ。
- **次に確認:** 非対称画素fixtureでnearest/bilinear、morph、各displacementをCPU実行し、RGBA/BGRAの論理色一致を比較する。未実施。

## 2026-10-16: OIIO channel names should drive image ingress and export mapping

- **関連:** `ArtifactCore/src/Image/ImageF32x4_RGBA.cppm::load()`、`ArtifactCore/src/IO/Image/ImageExporter.cppm::encodeImageBufToPath()`。
- **確認できた事実:** OIIO読み込みは先頭4 channelを無条件にR/G/B/Aとしていた。汎用書き出しも出力にR/G/B/Aの名前を付けながら入力順をそのまま書き、内部BGRA値とラベルがずれる可能性があった。EXR等ではchannel namesが別順序やlayer prefix付きの場合がある。`OpenEXR.cppm`にはchannel nameを探す別実装が存在する。
- **変更:** ingress/egress双方で名前から論理R/G/B/A channelを選び、名前が欠ける場合は位置fallbackを使う。書き出し時は出力channel数へ整形したImageBufを経由する。
- **価値／懸念:** file reader/writerのchannel nameを境界契約に使うことで、source/internal channel layoutと保存形式のラベルを一致させる。名前がないファイルでは位置fallbackとなる。OIIO版とのAPI整合および画素結果は未検証。
- **次に確認:** R/G/B/AとB/G/R/Aの異なるchannel layoutを持つOIIO fixtureでload/export結果を比較し、実際の対象formatでchannelnamesがどう報告されるか確認する。未実施。

## 2026-10-16: float4 component names do not define image channel order

- **関連:** `ArtifactCore/src/ImageProcessing` の`float4*` processors、`ArtifactCore/include/ImageProcessing/Halation.ixx`、Glow / SurfaceFXなどのCPU effect色処理。
- **確認できた事実:** `float4.x/y/z`は画像配列のcomponent 0/1/2を指す。BGRA画素ではそれぞれB/G/Rだが、Anamorphic Flare、Broadcast Colors、Edge Echo、Leave Color、Threshold、Luma Key、Emboss、Halftone、Duotone、Strobe Light、Physical Halationが一部でRGB係数や色設定をx=R/z=Bとして扱っていた。追加でRadio Waves、Pixel Sort、Inner Shadow、Surface FX、Simple Rain、Glow CPU出力setterにも同じ種類のCPU RGB/BGRA対応漏れがあった。
- **価値／懸念:** raw `float4*` processorの引数型から論理色順を推定するとCPU effect間の不整合が再発する。BGRA順を公開コメントへ明記し、設定色は論理RGBAとして個別にmapする。
- **次に確認:** 同系列の全`float4*` processorとCPU effectについて、RGB依存計算とユーザー色設定の両方を継続監査する。今回の列挙以外は未確認。

## 2026-10-16: Shared matte, viewport exposure, and render-farm transport also consume BGRA

- **関連:** `ArtifactCore/src/Layer/LayerMatte.cppm`、`Artifact/include/Render/ViewerHelperShaders.ixx`、`ArtifactCore/src/IPC/RenderFarmSharedBuffer.cppm`。
- **確認できた事実:** matte fast pathは`rgba32fData()`のcomponent順をそのまま論理`FloatRGBA`へ渡し、luminance matteの赤青寄与が逆になっていた。viewport exposure shaderはBGRA accumulator値を`.rgb`として扱っていた。Render Farm packetはBGRA backingをそのまま送信した後、受信側でRGBA入力setterへ渡していた。
- **変更:** matte raw valuesを論理RGBAへ展開し、viewport shaderはBGRA read→論理RGB処理→BGRA writeを明示し、packet受信側はBGRA setterを使用する。
- **価値／懸念:** 画像effect以外の表示・matte・共有transport境界にも同一の値順を適用する。変更はコード読解から確認したがbuild、実行、非対称色比較は未実施。
- **次に確認:** 残るCPU raw-index processors、GPU shader texture read/store、preview/export境界を継続監査し、最終的にD3D12/Vulkan双方で赤・青が異なる入力を用いて確認する。

## 2026-10-16: Single-pass shader generator needs an explicit pixel-function color contract

- **関連:** `ArtifactCore/src/ImageProcessing/DirectCompute/SinglePassShader.cppm`、`docs/technical/IMAGE_CHANNEL_ORDER_CONTRACT_2026-10-16.md`。
- **確認できた事実:** 共通生成器はGPU textureのraw `float4`をpixel functionの`src`へ渡し、その戻り値を変換せずoutput textureへ保存する。一方、標準stageは`.rgb`を論理色として演算する。Artifact image-pipeline texture値はBGRAなので、標準stageを含むpixel functionの色契約が一致していなかった。repository内に現時点のcallerは見つからない。
- **変更:** 生成器の読み出しでBGRA→論理RGBAを行い、pixel functionの戻り値をBGRAへ戻して保存する。
- **価値／懸念:** 個々のsingle-pass effectへswizzle漏れを分散させず、共通生成境界で契約を固定する。関数戻りswizzleのHLSLコンパイルおよびruntimeは未確認。
- **次に確認:** caller追加時に、非対称RGB値を用いて生成HLSLのCPU期待値との一致を確認する。shader compilerがfunction return swizzleを受け付けるかビルド時に確認する。

## 2026-10-16: Core GPU scopes and analysis shaders need logical RGB from BGRA textures

- **関連:** `ArtifactCore/include/Graphics/Shader/Compute/HLSL/Histogram.ixx`、`ScopeParade.ixx`、`ScopeVectorscope.ixx`、`ScopeWaveform.ixx`、`LUT3D.ixx`、`EdgeEcho.ixx`、`MatteTrack.ixx`。
- **確認できた事実:** GPU analysis shader群がArtifact image-pipeline textureをraw float4として読み、RGB channel identityやluminanceを使っていた。BGRA texture上ではraw `.rgb`がB,G,Rを指す。Scope Parade pane order、vectorscope chroma、histogram RGB bins、3D LUT index、edge/matte luminanceに影響する。
- **変更:** 色解析箇所で`.bgra.rgb`によりlogical RGBへ展開し、LUT/Edge Echoの色付き出力をBGRAへ保存する。matte alphaとscalar mask operationは順序不変なので維持した。
- **価値／懸念:** image effectsだけでなくGPU scopes、analysis、keying/matteとdisplay LUTも同じresident texture契約へ揃える。これらのGPU実行結果は未検証。
- **次に確認:** scopes、3D LUT、Edge Echo、Track Matteを非対称RGB入力で実行し、GPU readbackおよびD3D12/Vulkanでlogical resultを比較する。

## 2026-10-16: Serialized and shared-memory image payloads need order-specific versions

- **関連:** `ArtifactCore/src/Composition/ParametricComposition.cppm`、`ArtifactCore/src/IPC/RenderFarmSharedBuffer.cppm`。
- **確認できた事実:** Parametric Compositionはraw image backingを`rgba32f` keyへ保存し、読込はRGBA入力setterを使っていた。Render Farm packetはchannel orderを識別せず、fixed magicでpayloadを受け渡していた。
- **変更:** 新しい保存データは`bgra32f` keyを使い、legacy `rgba32f`はRGBAとして読む。共有packet magicをBGRA payload v2へ更新し、旧peerからのpacketを拒否する。
- **価値／懸念:** 新旧形式を無言で取り違えることを防ぐ。旧JSONは維持できるが、旧Render Farm peerとの通信は成立しない。protocol混在時の拒否動作は未実行。
- **次に確認:** 保存／再読込の非対称色比較と、新旧Render Farm peerの互換挙動を確認する。

## 2026-10-16: Internal image byte snapshots need channel metadata or versioning

- **関連:** `Artifact/src/Project/ArtifactPresetManager.cppm`、`Artifact/src/Layer/ArtifactPaintLayer.cppm`、`Artifact/src/Layer/ArtifactImageLayer.cppm`。
- **確認できた事実:** Effect Preset / Paint frame JSONは`rgba32fData()`のraw BGRA bytesをorder metadataなしで保存し、RGBA setterで復元していた。still image derived cacheもraw bytesを保存しながらRGBA setterで読む。全てが以前のRGBA backing時代から残った互換名とは限らず、新規保存payloadでは赤青がずれる。
- **変更:** 新JSONには`channel_order: BGRA`を付けてBGRA setterで復元し、metadataのない旧JSONはRGBAとして読む。derived cacheはversionを上げて旧キャッシュを再生成し、BGRA setterでロードする。
- **価値／懸念:** 新旧データの意味を分け、画像cache／preset／paint frameの往復で順序を固定する。既存JSON互換の非対称色読み戻しは未実行。
- **次に確認:** legacy RGBA JSONと新BGRA JSONの両方を非対称色画像で読み込み、derived cache再生成後の見た目とdescriptorを確認する。

## 2026-10-16: Remaining raw CPU effects still assumed component zero was red

- **関連:** `Artifact/src/Effect/ArtifactCreativeEffects.cppm`、`ArtifactCore/src/ImageProcessing/ColorTransform/TriChromaticShift.cppm`。
- **確認できた事実:** Deband/DeblockはBGRA raw channel 0/2へRec.709 R/B係数を逆に掛けていた。Beauty StudioはBGRAから抽出したOpenCV channel 0/2をR/Bとみなし、skin hue判定とwarmth tintを入れ替えていた。TriChromaticShiftのfloat4 processorもx/zをR/Bとして扱っていた。
- **変更:** luma係数をBGRA indexへ合わせ、Beauty Studioでは論理RGB順の作業画像を作ってBGRA出力へ戻し、TriChromaticShiftはraw input/outputをBGRAと明記してR/B componentをmapする。
- **価値／懸念:** これらのCPU-only / fallback effectでも赤青依存の見た目と選択をCPU/GPU間で揃える。pixel comparisonは未実行。
- **次に確認:** remaining raw CPU effect passでluminance、hue/saturation、skin-color、user tintの全てをchannel-order viewで継続走査する。

## 2026-10-16: Wire removal and dust repair compute paths consume BGRA image textures

- **関連:** `Artifact/src/Effect/ArtifactCreativeEffects.cppm` のWire Removal / Dust Repair HLSL。
- **確認できた事実:** GPU shaderがBGRA `float4` textureをraw `.rgb`で読み、foreground RGB、clean plate RGB、temporal luma、Dust Repair median channel selectionを論理RGBと誤認していた。両shaderのinput textureはArtifact image-pipeline textureから直接作られる。
- **変更:** shader inputをlogical RGBAへswizzleして色演算し、出力をBGRA値順へ戻す。bilinear foreground sampling、clean plate、前後frameも同じ扱いにした。
- **価値／懸念:** CPU/GPUで同じeffectの赤青寄与を一致させる。共有shader compilation/runtimeは未確認。
- **次に確認:** GPU Wire Removal/Dust Repairを非対称RGB素材でCPU fallbackと比較し、D3D12/Vulkan双方で出力を確認する。

## 2026-10-16: Creative Effects RGB identities need to survive BGRA CPU processing

- **関連:** `Artifact/src/Effect/ArtifactCreativeEffects.cppm`、`ArtifactCore/src/ImageProcessing/FilmRepair.cppm`。
- **確認できた事実:** BGRA backingへ移行した後も、いくつかのCreative EffectsがOpenCV分割channel 0を赤とみなす輝度計算、logical RGB配列とphysical BGRA出力の混在、色付き加算のchannel直書きを残していた。Spherizeの赤青分散符号も物理indexに結び付いていた。FilmRepairはchannel 0へ赤係数、channel 2へ青係数を掛けていた。
- **変更:** 論理RGBで処理してBGRA indexへ戻す。色付き加算、luma、深度/法線抽出、暖色補正、赤青分散をBGRA backingに合わせた。
- **価値／懸念:** エフェクト固有の色設定・チャンネル選択で赤青が入れ替わる残存要因を減らす。GPU/CPU runtimeや非対称色画素での結果は未確認。Energy Zapの正のwarmthを赤寄りと解釈した点は、UIラベルからの意味解釈で実行時見本は未検証。
- **次に確認:** 各CPU/GPU fallbackで非対称な赤青入力を比較し、Creative Effectsの色設定と暖色方向の期待値を確認する。

## 2026-10-16: GPU output swizzles and readback descriptors must agree

- **関連:** `Artifact/src/Effects/AddNoise/AddNoiseEffect.cppm`、`Artifact/src/Effects/Blur/BlurEffect.cppm`、`Artifact/src/Effects/Blur/GauusianBlur.cppm`。
- **確認できた事実:** これらのcompute shaderはBGRA textureを論理RGBAへ読み、`.bgra`でoutputへ保存していた。一部readback descriptorはRGBAのままなので、BGRA payloadをRGBAとして再解釈して追加swapする。Gaussian Blur CPU fallbackもchannelwise gaussian前後で不要なR/B swapを行っていた。
- **変更:** GPU readback descriptorをBGRAへ合わせ、Gaussian Blur CPU fallbackは元のBGRA planesを直接処理する。
- **価値／懸念:** GPU shader内のBGRA-native規約とCPU readback契約を揃え、二重変換とfallbackだけの色差を減らす。変更後の実行結果・D3D12/Vulkan差は未検証。
- **次に確認:** Add Noise、Blur、Gaussian Blurについて、CPU/GPU双方で赤と青が異なる入力を比較する。

## 2026-10-16: OpenCV effect helpers must name their returned channel order

- **関連:** `ArtifactCore/src/ImageProcessing/OpenCV/Glow.cppm`。
- **確認できた事実:** Vertical Glowの内部helperは後段でCV_32FC4をBGRAとしてsplitしlumaを作る一方、3-channel BGR入力を`COLOR_BGR2RGBA`で変換していた。R/Bが逆転する入力経路が残っていた。
- **変更:** helper名をBGRAにし、BGR/gray入力をBGRAへ変換するよう修正した。4-channel inputは順序を変えずに扱う。
- **価値／懸念:** OpenCV matrixのchannel countだけでRGBA/BGRAを推測する誤りを避ける。入力producerの4-channel順序は呼び出し契約に依存し、実行時確認は未実施。
- **次に確認:** 8-bit/float BGRおよびBGRA入力でVertical Glowの出力色を比較する。

## 2026-10-16: Color-specific effect parameters must use logical RGB indices

- **関連:** `Artifact/src/Effect/ArtifactCreativeEffects.cppm`、`Artifact/src/Effects/Rasterizer/FilmDamageEffect.cppm`、`Artifact/src/Effects/Rasterizer/GlowEffect.cppm`、`Artifact/src/Effects/Glow/GlowEffect.cppm`。
- **確認できた事実:** 輝度式が正しくても、Film burn tint、grain bias、chromatic separation、warmth tint、reflection tintがraw channel index 0/2を論理R/Bとしていた。Rasterizer GlowのGPU shaderはresident BGRA textureを`.rgb`として処理・保存し、CPU fallbackと意味が異なっていた。
- **変更:** 色パラメータをlogical RGBへ対応させてBGRA indexへ戻し、Glow shaderはBGRA load/storeを明示した。
- **価値／懸念:** CPU fallbackとresident GPU経路の赤青差、および色付きeffectの赤青反転を減らす。runtime parityは未検証。
- **次に確認:** Film Damage、Film Grunge、Heatwave、Texturize Motion、Reflection Composer、Glowを赤青非対称画素でCPU/GPU比較する。

## 2026-10-16: Raw float processor overloads need an explicit BGRA contract

- **関連:** `ArtifactCore/src/ImageProcessing/ColorTransform/Fill.cppm`、`ArtifactCore/src/ImageProcessing/ColorTransform/GradientRamp.cppm`。
- **確認できた事実:** Fill / Gradient Rampのpixel APIは論理RGBA参照で正しいが、raw `float*` overloadがBGRA backingのindex 0を論理Rとして渡していた。Effect wrapperはtyped logical viewを使うため、この差は直接raw overloadを呼ぶ利用者にだけ残っていた。
- **変更:** raw BGRA pixelのindex 2/1/0をlogical R/G/B参照へ接続した。
- **価値／懸念:** 同じprocessorのtyped pixel APIとraw image APIが色付き入力で一貫する。現在のrepository内にこのraw overloadを呼び出す箇所は見つからず、挙動は未実行。
- **次に確認:** 今後 `float*` image processor APIを追加・変更するとき、物理BGRAとlogical RGBAを関数境界で明記し、typed viewとの対応を確認する。

## 2026-10-16: Image producers and CPU fallbacks must map logical colors to BGRA storage

- **関連:** `Artifact/src/Layer/ArtifactNoiseLayer.cppm`、`Artifact/src/Layer/ArtifactPaintLayer.cppm`、`Artifact/src/Layer/ArtifactSandSim2DLayer.cppm`、`Artifact/src/Effects/LensDistortion/LensDistortionEffect.cppm`、`Artifact/src/Effects/Rasterizer/ScreenShakeEffect.cppm`。
- **確認できた事実:** Noise Layerの色マッピング、Paint Layerの筆色ブレンド、Sand Simulation LayerのGPU readback後とCPU fallbackの描画、Lens Distortionのbilinear logical pixel出力、Screen Shakeのclamp/mirror custom samplerが、論理Rをraw index 0へ、論理Bをindex 2へ書いていた。Lens DistortionのGPU shaderは既にBGRA backingを保つ。
- **変更:** raw index 0にlogical B、index 2にlogical Rを書き、CPU/GPU Lens Distortionの出力順を一致させた。
- **価値／懸念:** 静止画effectだけでなくレイヤー生成・ペイントでも色設定の赤青を維持し、CPU/GPU fallback差を減らす。runtime画素比較は未実施。
- **次に確認:** 色設定を使ってraw image backingへ書くproducerを継続走査し、CPU生成画像がpreview、cache、exportまでBGRA descriptorを保持することを確認する。

## 2026-10-16: Explicit QImage order is required at software image boundaries

- **関連:** `Artifact/src/Render/Software/ArtifactSoftwareImageCompositor.cppm`、`Artifact/src/Render/ArtifactCompositionViewDrawing.cppm`。
- **確認できた事実:** software compositorは`Format_RGBA8888`をRGBA順のcv::Matへ変換していたが、`setFromCVMat()`のdescriptorなしoverloadはBGRAと解釈する。scene-light liftはcanonical BGRA imageをlogical RGBA Matへ変換後、同じBGRA前提overloadで戻していた。
- **変更:** software compositorでRGBA input descriptorを明示した。scene-light liftは順序に依存しないためraw BGRA RGB成分をin-placeで更新し、全画面swizzleと一時Matを除いた。
- **価値／懸念:** QImage↔canonical CPU image境界の色を保ち、software compositionでの赤青反転とscene-light段の不要なmemory passを解消する。実行時比較は未実施。
- **次に確認:** software compositor出力とscene-light on/offを異なるR/B値の素材で比較し、GPU経路とlogical colorが一致することを確認する。

## 2026-10-16: Color mattes in CPU effects must be written in BGRA order

- **関連:** `Artifact/src/Effects/DropShadow/DropShadowEffect.cppm`、`Artifact/src/Effects/Satin/SatinEffect.cppm`、`Artifact/src/Effects/Stroke/StrokeEffect.cppm`。
- **確認できた事実:** 3つのeffectが`QColor`のlogical R/G/B値を`cv::Vec4f`のcomponent 0/1/2へ直接書いていた。これらのmatteは後続でBGRA image backingと同じ順序として合成される。`InnerShadowEffect.cppm`は既に`(B,G,R,A)`として書いていた。
- **変更:** CPU matteのcomponent 0/2を入れ替え、backingのBGRA契約を誤っていたコメントと見出しを修正した。
- **価値／懸念:** 影・サテン・ストロークの色指定で発生する赤青入替を防ぎ、Inner ShadowおよびGPU BGRA経路と揃える。runtime画素比較は未実施。
- **次に確認:** 非対称色（赤と青の異なる値）を使い、3 effectのCPU/GPU出力とeffect chain後のlogical colorを比較する。

## 2026-10-17: Float image classes should enforce their BGRA backing invariant

- **関連:** `ArtifactCore/src/Image/ImageF32x4_RGBA.cppm::setColorDescriptor()`、`ArtifactCore/include/Image/ImageF16x4.ixx`、`Artifact/src/Widgets/ArtifactFilmstripWidget.cppm`。
- **確認できた事実:** Float32 pixel APIは常に物理BGRAを書き、Float16 conversionもBGRAへ正規化する一方、descriptor setterはRGBA labelへ変更できた。Filmstripはpixel APIで画像を構築した後descriptorだけRGBAへ変えており、`toQImage()`が順序を誤解釈する。
- **変更:** Float32 descriptor setterはstorage/orderをFloat32/BGRAに固定し、Float16 constructor/setterもFloat16/BGRAを強制する。Filmstripはcolor-space metadataのみ継承する。
- **価値／懸念:** 「BGRA backing」とdescriptorが食い違う状態を内部画像クラスで表現できなくし、FilmstripのR/B swapも防ぐ。既存descriptor互換テストや全callerは実行未確認。明示RGBA export helperは境界用途として残る。
- **次に確認:** 全 `setColorDescriptor()` callerと既存画像契約テストを見直し、RGBA boundary bufferが内部Image objectとして流通していないことを確認する。

## 2026-10-17: RGBA video frame display needs an explicit BGRA adapter

- **関連:** `Artifact/src/Layer/ArtifactVideoLayer.cppm::cpuVideoFrameToQImage()`、`ArtifactCore/include/ImageProcessing/OpenCV/CvUtils.ixx::cvMatToQImage()`。
- **確認できた事実:** `RGBA32F` video frameはproducer順がRGBAだが、`cvMatToQImage()`の4-channel変換はOpenCV標準BGRAとして扱い、little-endian `Format_ARGB32`へコピーする。
- **変更:** QImage表示境界で`COLOR_RGBA2BGRA`を明示してから既存helperへ渡す。
- **価値／懸念:** RGBA32F video frameの表示に残る赤青入替を防ぐ。境界で全フレームのR/B変換は必要だが、内部Float image/GPU processingでは発生させない。runtime比較は未実施。
- **次に確認:** RGBA32F video producerからQImage表示とFloat32 image ingressまで同一非対称色で照合する。

## 2026-10-17: GPU readback must carry the source channel order

- **関連:** `Artifact/src/Render/ArtifactIRenderer.cppm`、`Artifact/src/Widgets/Render/ArtifactCompositionRenderController.cppm`。
- **確認できた事実:** CompositionはRGBA化shader成功時にはstraight RGBA textureを、失敗時にはBGRA accumulatorをreadback sourceとして選ぶ。readback APIが常にRGBA解釈するとfallback captureとaccumulator診断previewの赤青が入れ替わる。非同期RAM previewもbuffer descriptorはRGBAなので、BGRA sourceは境界で並べ替えが必要。
- **変更:** rendererの同期・非同期readbackへsource channel order overloadを追加し、QImage/native buffer出力をRGBAへ正規化した。Composition capture、RAM preview、layer/accumulator診断previewでsource orderを明示。行stride不正時にnative buffer callbackへ失敗payloadを返すようにした。
- **価値／懸念:** RGBA化fallbackが発生してもcapture/previewへ正しい色を渡せる。変更は静的確認のみで、module compile、D3D12/Vulkanのreadback挙動、RGBA/BGRAの実画素比較は未検証。
- **次に確認:** 非対称赤青pixelをRGBA化成功/fallback双方、sync/async QImage、RAM preview buffer、RGBA8/16F/32F stagingで比較する。全ての未指定legacy overload callerが真にRGBA sourceであることも再確認する。

## 2026-10-17: Float render readback must use the canonical BGRA image setter

- **関連:** `Artifact/src/Render/ArtifactIRenderer.cppm::readbackToImageF32()`、`readbackToMultiChannelImage()`。
- **確認できた事実:** app image-pipeline GPU float値をBGRAとする一方、高精度readbackはraw component順を変えずに`ImageF32x4_RGBA::setFromRGBA32F()`へ渡していた。このsetterは外部RGBA入力をcanonical BGRA backingへ変換するため、BGRA sourceを誤ってRGBAとして扱うとR/Bが交換される。multi-channel exportもbeauty/albedoをraw component 0/2からlogical Red/Blueとして抽出していた。
- **変更:** F32 image readbackをBGRA setterへ接続し、multi-channel exportではbeauty/albedoだけR/Bをlogical RGBAへ正規化してから各channelを抽出する。QImage readbackにもsource order overloadを追加し、OffscreenCompositionRenderer、GPU Render Queue、Export PreRenderはBGRAを指定する。normal/vector/scalar AOVは成分の意味順を保持する。
- **価値／懸念:** render queueとoffscreen F32経路で発生し得るreadback時の赤青反転を抑える。source GPU targetがBGRA image-pipeline値を保持するという契約に基づく変更であり、実際の各AOV producerおよび外部render targetとの一致は未検証。
- **次に確認:** RGBA/BGRA各producerから高精度readback、MultiChannel export、最終画像出力まで非対称色fixtureで比較する。

## 2026-10-17: Pixel order needs raw-byte and logical-color assertions together

- **関連:** `tests/ArtifactCore/PixelChannelOrderStandaloneTest.cpp`、Float32/Float16 image backing、GPU upload descriptor。
- **確認できた事実:** surface conversion tests already cover RGBA/BGRA conversion and alpha cases, but a direct invariant test should independently inspect raw canonical backing and logical pixel access across Float32, Float16, and GPU upload.
- **変更:** Added a separately launchable `ArtifactCorePixelChannelOrderStandaloneTest` executable, linked only to ArtifactCore and the standard test harness. Its test uses exact binary fractions and distinct R/G/B/A to check raw BGRA float values, logical RGBA getters, exact IEEE-754 binary16 channel bits, Float16 round-trip, descriptor setter invariance, and BGRA zero-copy GPU upload/rejection of an RGBA target.
- **価値／懸念:** Raw order regressions and API semantic regressions fail at separate assertions with the failing channel index. The standalone executable has not been configured, built, or run under the repository rule.
- **次に確認:** Execute ArtifactCorePixelChannelOrderStandaloneTest when build/test execution is explicitly allowed; then add renderer GPU readback coverage using the same non-symmetric fixture and compare D3D12/Vulkan where available.

## 2026-10-17: Boids formation initialization appears to omit LOD state

- **関連:** `ArtifactCore/include/Crowd/BoidsSwarmSystem.ixx::initializeFormation()` and `update()`.
- **確認できた事実:** `update()` indexes `agentLod_` and `lodSkipCounter_` once per agent. `addAgent()`, `initializeRandom()`, and `initializeWithTypes()` populate those arrays, while `initializeFormation()` clears agents and trails but does not clear or populate either LOD array.
- **気づき:** Calling `update()` after formation initialization may index empty or stale LOD vectors. This was found while mapping distance-LOD test coverage; it is outside the current test-only change and remains unverified at runtime.
- **価値／懸念:** A future targeted regression should cover `initializeFormation()` followed by `update()` and assert all agents remain finite. First decide whether formation should initialize LOD state alongside agent creation, then run under an address/undefined-behavior sanitizer if available.
- **次に確認:** Reconcile the formation agent lifecycle with LOD arrays and add the regression in the ArtifactCore-owning change.

## 2026-10-17: Imported 3D model drawing currently ignores viewport detail level

- **関連:** `Artifact/src/Layer/Artifact3DModelLayer.cppm::drawLOD()` and `Artifact/src/Layer/ArtifactProcedural3DLayer.cppm::drawLOD()`.
- **確認できた事実:** Procedural 3D maps Low/Medium/High to Draft/Preview/Full before resolving geometry. The imported 3D model override currently delegates to `draw(renderer)` for every detail level.
- **気づき:** Composition zoom LOD therefore changes procedural geometry quality but does not change the imported model layer's own draw path. Meshlet selection may still independently reduce GPU mesh work, but that is a separate renderer-level mechanism and does not establish layer-level parity.
- **価値／懸念:** A test asserting model LOD behavior would need a concrete observable contract (for example selected meshlet level or reduced submitted geometry) and GPU-backed evidence. No such behavior is currently specified by the layer override.
- **次に確認:** Define whether imported models should consume `DetailLevel` directly or rely solely on meshlet LOD, then add a production-path test at the chosen boundary.

## 2026-10-17: Physics target-rate LOD is bypassed by fixed timeline advancement

- **関連:** `ArtifactCore/src/Physics/PhysicsSystem.cppm::advancePhysicsFrame()` and `update()`.
- **確認できた事実:** Reduced and Minimal presets assign positive `targetHz` values. `advancePhysicsFrame()` sets `fixedTimelineMode_` before calling `update()`, while the `targetHz` accumulator branch in `update()` runs only when `!fixedTimelineMode_`.
- **気づき:** Calls through the fixed-timeline entry point appear to ignore the preset target rate, even though solver-detail overrides such as soft-body grid scale still apply. This is a source-level finding; runtime behavior was not tested in this task.
- **価値／懸念:** Tests should distinguish frame-clock fixed stepping from an intended lower simulation update frequency. Do not assert target-rate throttling until that contract is clarified; the current test addition verifies only Reduced/Full soft-body resolution transitions.
- **次に確認:** Decide whether `targetHz` should subdivide or skip fixed timeline updates, then add a deterministic registered-solver test for the selected timing contract.

## 2026-10-17: Layer drawLOD overrides have no caller in the current render sources

- **関連:** `Artifact/src/Layer/ArtifactProcedural3DLayer.cppm::drawLOD()`、`Artifact/src/Layer/Artifact3DModelLayer.cppm::drawLOD()`、composition render paths.
- **確認できた事実:** A repository search finds `drawLOD()` definitions for the abstract layer, procedural 3D layer, and imported 3D model layer, but no call site under `Artifact/src`. Composition rendering sets renderer detail level and then calls `layer->draw(renderer)` in its layer loops.
- **気づき:** The layer-specific Draft/Preview/Full dispatch currently appears unreachable through the production composition paths. Meshlet selection and other renderer detail-level consumers are separate LOD routes.
- **変更:** Added `ArtifactProcedural3DLayerLodContractTest` to call the layer override directly and verify Terrain, PathTube, and TextExtrude quality reporting, plus vertex-count growth for Terrain and PathTube and Low→High→Medium regeneration on the same frame. This is an override contract only; it does not establish viewport integration.
- **価値／懸念:** A direct unit test of `drawLOD()` would prove only the override in isolation, not viewport integration. A meaningful regression needs a composition/render-path test that observes the selected quality after the layer loop.
- **次に確認:** Select the intended production dispatch point and add an integration assertion for procedural 3D quality; keep imported model behavior separately specified because its current override delegates to ordinary drawing.

## 2026-10-17: Physics fluid resolution scale has no observed application site

- **関連:** `ArtifactCore/src/Physics/PhysicsSystem.cppm::PhysicsLODSettings` and its registered `FluidSolver2D` update loop.
- **確認できた事実:** Full/Reduced/Minimal settings initialize and clamp `fluidResolutionScale`. A source search finds no read of that field after normalization; the registered fluid loop only applies `fluidSolverIterations` before calling `update()`.
- **気づき:** The preset's advertised fluid resolution reduction is currently stored but does not appear to change `FluidSolver2D::width()` or `height()`. This is a source-level gap, not runtime-tested in this task.
- **価値／懸念:** A future application test should register a solver, step under Reduced/Minimal, and assert a defined scaled grid plus restoration behavior. No such expectation is added here because resolution scaling semantics (rounding, minimum dimensions, and state resampling) are not established by current code.
- **次に確認:** Define resolution scaling and state preservation, then implement and test its lifecycle from Full through reduced levels and back.

## 2026-10-17: Full rigid-body LOD may skip collision restoration

- **関連:** `ArtifactCore/src/Physics/PhysicsSystem.cppm` rigid-world update loop and `Physics2D::RigidBody2D::restoreCollisionMesh()`.
- **確認できた事実:** `restoreCollisionMesh()` is called only inside a loop guarded by `applySleepPolicy || disableContinuousCollision`. Minimal enables both flags and can simplify polygon shapes. Full defaults both flags to false, so its update skips that loop entirely.
- **気づき:** A shape simplified under Minimal may remain simplified after switching back to Full. Extended the physics LOD integration contract to assert the mass returns to its authored value after the Full step; based on the current source guard this is expected to expose the restoration gap. The test was not run.
- **価値／懸念:** This identifies a likely transition bug in the parent/child-owned physics implementation. Avoid adding a passing expectation for restoration until the implementation is corrected or the intended lifecycle is clarified.
- **次に確認:** Ensure every registered rigid body receives collision restoration on transition to a policy with `simplifyCollisionMesh == false`, then add a Minimal→Full mass/geometry round-trip contract.

## 2026-10-17: Physics LOD solver budgets may remain reduced after returning to Full

- **関連:** `ArtifactCore/src/Physics/PhysicsSystem.cppm::update()` and `tests/ArtifactCore/PhysicsLodSettingsContractTest.cpp`.
- **確認できた事実:** Registered fluid, soft-body, cloth, and material solvers receive a setter only when the corresponding LOD budget is greater than zero. Full settings normalize these budgets to zero, so after Reduced/Minimal has set the solver object, a later Full update does not restore its constructor default. Material fracture enablement is set each update, but its max-substep budget has the same conditional setter gap.
- **気づき:** Added Minimal/Reduced→Full assertions for fluid iterations, soft-body/cloth substeps, and material substeps to make the transition contract observable. Same-solver Reduced→Full soft-body and Cloth3D solve comparisons against fresh Full solvers now cover constraint-iteration restoration behavior too. Based on the current source path these assertions are expected to expose restoration gaps; no runtime execution was performed.
- **価値／懸念:** A passing transition suite needs an explicit policy for restoring per-solver defaults (including custom pre-LOD settings), not merely clearing the global override to zero. The implementation belongs to the ArtifactCore child repository and was not changed in this parent test task.
- **次に確認:** Define whether Full restores constructor defaults or each solver's pre-LOD value, implement that policy in the owning repository, then run the new round-trip tests.

## 2026-10-17: Text extrusion quality presets do not currently change mesh density

- **関連:** `Artifact/src/Layer/ArtifactProcedural3DLayer.cppm::Impl::regenerate()`, `ArtifactCore/src/Geometry/Procedural3DGenerators.cppm::makeTextExtrudePreset()` and `generateTextExtrude()`; `tests/ArtifactCore/Procedural3DQualityLodTest.cpp`; `tests/Artifact/Procedural3DLayerLodContractTest.cpp`.
- **確認できた事実:** The Draft/Preview/Full preset stores the requested `quality`, but sets the same `bevelSegments` value for all three. `generateTextExtrude()` does not read `settings.quality`; it passes the unchanged bevel segment count into extrusion. The layer's regeneration path also passes `textExtrude` unchanged, unlike Terrain and PathTube which copy settings and apply `qualityOverride`. Generator and layer contracts require strictly growing geometry sizes and are expected to expose both gaps.
- **価値／懸念:** Text extrusion can report three quality labels while generating identical tessellation, so a quality preset may not provide the intended preview cost reduction. This is a source-level finding; the ArtifactCore child implementation was not changed or runtime-tested.
- **次に確認:** Define Draft/Preview/Full bevel or contour tessellation budgets while preserving text bounds and silhouette as much as possible, then make the new geometry-count test pass and validate representative glyphs including counters and narrow strokes.

## 2026-10-17: LOD threshold validation accepts NaN values

- **関連:** `Artifact/src/LOD/ArtifactLODManager.cppm::setThresholds()` and `tests/Artifact/LODManagerContractTest.cpp`.
- **確認できた事実:** `setThresholds()` validates thresholds only with ordered comparisons. Comparisons with NaN are false, so either NaN argument passes all current guards and is stored; subsequent zoom classification or factor calculation can then behave inconsistently.
- **気づき:** Added contract assertions that NaN low and medium thresholds leave the prior valid configuration unchanged. They are expected to fail against the current source; tests were not run.
- **価値／懸念:** A NaN threshold can poison LOD classification for the lifetime of the manager. Fixing validation belongs to the Artifact child repository and is outside this parent test-only change.
- **次に確認:** Reject non-finite thresholds before storing either value, then run the LOD manager contract test and verify selection/factor continuity remains intact.

## 2026-10-17: Preview resolution scale can retain NaN

- **関連:** `ArtifactCore/src/Preview/PreviewSettings.cppm::setResolutionScale()` and `tests/ArtifactCore/PreviewSettingsLodContractTest.cpp`.
- **確認できた事実:** `setResolutionScale()` stores `std::clamp(scale, 0.125f, 1.0f)` directly. With NaN, both ordered comparisons used by clamp are false, so the non-finite value can be retained. Positive and negative infinity clamp to the upper and lower bounds respectively.
- **気づき:** Added a contract for finite fallback on NaN and bound clamping for infinities. The NaN assertion is expected to fail against the current source; no tests were run.
- **価値／懸念:** A non-finite preview scale can propagate into render-size calculations and invalidate previews. The implementation belongs to the ArtifactCore child repository and was not changed.
- **次に確認:** Normalize non-finite scale input to the default full scale before clamping, then exercise preview render dimensions at the accepted scale bounds.

## 2026-10-17: Render context cache keys round LOD scale to three decimals

- **関連:** `Artifact/include/Render/ArtifactRenderContext.ixx::RenderContextRegistry::makeKey()` and `tests/Artifact/RenderContextLodContractTest.cpp`.
- **確認できた事実:** Cache keys serialize `resolutionScale` with fixed three-decimal precision. Distinct scales such as 0.5000 and 0.5004 therefore map to the same key, while at a 4096-pixel base width they produce different truncated working dimensions. Scales 0.5000 and 0.5001 also collide at 1920 pixels (both truncate to 960), while Blur reads the scale itself for parameter scaling.
- **気づき:** Added contracts requiring different keys both when scales imply different pixel dimensions and when dimensions match but the scale value differs, plus snapshot round-trip coverage. The collision assertions are expected to fail with the current precision; no runtime test was performed.
- **価値／懸念:** A cache hit can return an image rendered at another LOD scale when callers use fine-grained custom resolution values. This RenderContext implementation belongs to the Artifact child repository and was not modified.
- **次に確認:** Encode enough scale precision, or key by resolved pixel dimensions when available, then verify no collisions across supported widths and scale inputs.

## 2026-10-17: Render context resolution scale accepts positive infinity

- **関連:** `Artifact/include/Render/ArtifactRenderContext.ixx::RenderContext::setResolutionScale()` and `tests/Artifact/RenderContextLodContractTest.cpp`.
- **確認できた事実:** The setter applies `std::max(0.01f, newResolutionScale)` without a finite-value check. Positive infinity remains infinite; NaN becomes the minimum value with the current argument order. The preview settings API has a separate finite-scale contract.
- **変更:** Added a contract requiring NaN and either infinity to fall back to full resolution (1.0), while leaving finite custom scales available for export upscaling.
- **価値／懸念:** A non-finite scale can propagate into cache keys and downstream size calculations. This is a source-level finding; the Artifact child implementation was not changed or tested at runtime.
- **次に確認:** Normalize non-finite inputs to 1.0 before applying the positive floor, then verify snapshot restoration and cache-key generation remain finite.

## 2026-10-17: GI working dimensions cast non-finite scales to unsigned integers

- **関連:** `ArtifactCore/include/Graphics/GIResources.ixx::GIFrameResources::scaledDimension()` and the GI working-resolution tests in `tests/ArtifactCore/RenderPipelineContractTest.cpp`.
- **確認できた事実:** The function rejects zero/negative scales but does not check finiteness before multiplying and converting to `std::uint32_t`. NaN and positive infinity pass the guard; converting an out-of-range or NaN floating value to an integer has undefined behavior.
- **気づき:** Existing tests cover zero, negative, and tiny positive scales. No test invokes the unsafe non-finite case because that would exercise undefined behavior before the implementation is hardened.
- **価値／懸念:** Invalid user or propagated preview settings could corrupt GI resource dimensions or trigger undefined behavior. The implementation belongs to ArtifactCore and was not modified in this test task.
- **次に確認:** Reject non-finite scale values before the cast with a defined fallback, then add NaN and infinity dimension tests.

## 2026-10-17: Viewport LOD zoom cutoffs differ from LODManager defaults

- **関連:** `Artifact/src/Widgets/Render/ArtifactCompositionRenderController.cppm::detailLevelFromZoom()` and `Artifact/src/LOD/ArtifactLODManager.cppm::getDetailLevel()`.
- **確認できた事実:** The composition render controller classifies zoom below 0.50 as Low, below 0.90 as Medium, and otherwise High. `LODManager` defaults classify below 0.25 as Low, below 0.75 as Medium, and otherwise High.
- **気づき:** There are two different zoom-to-LOD policies in the same application. Whether this is intentional (viewport rendering vs manager contract) or causes UI/render detail drift is unverified; this task did not run the viewport.
- **価値／懸念:** A controller-level regression test should cover its 0.50 and 0.90 boundaries and confirm the selected policy is applied consistently to effect surfaces, layer culling, and renderer material LOD. Avoid assuming the manager defaults are the intended controller thresholds until the design is clarified.
- **次に確認:** Define whether the controller cutoffs are a separate policy or should be sourced from `LODManager`, then expose a stable controller-level contract test without broadening the public API unnecessarily.

## 2026-10-17: Boids far-LOD skip counter can survive a near-detail interval

- **関連:** `ArtifactCore/include/Crowd/BoidsSwarmSystem.ixx::BoidsSwarmSystem::update()` and `tests/ArtifactCore/BoidsDistanceLodContractTest.cpp`.
- **確認できた事実:** `lodSkipCounter_` is decremented on every update, including while an agent is in LOD0, but is only reset when a reduced-LOD force computation runs. A `skipFrames` value is calculated from the current LOD but is not used in the counter update.
- **気づき:** Added a transition contract that moves an agent LOD2 → LOD0 → LOD2 and expects its first force in the new LOD2 interval to be refreshed. The assertion is expected to expose stale counter carry-over based on source inspection; it has not been executed.
- **価値／懸念:** A distant agent may skip its first force update after returning from near detail and temporarily keep a force derived from an earlier target or neighbor arrangement. Whether transition refresh is the intended policy remains unverified.
- **次に確認:** Run the focused Boids LOD tests when testing is authorized, confirm the transition behavior, then decide whether the counter should reset on LOD changes or whether carrying the remaining interval is intentional.

## 2026-10-17: Composition controller LOD enable API has no implementation

- **関連:** `Artifact/include/Widgets/Render/ArtifactCompositionRenderController.ixx::setLODEnabled()` / `isLODEnabled()` and `Artifact/src/Widgets/Render/ArtifactCompositionRenderController.cppm::Impl::lodEnabled_`.
- **確認できた事実:** The public header declares both methods and the implementation class has a `lodEnabled_` member, but repository-wide search finds no method definitions and no reads or writes of the member.
- **気づき:** A test for disabling zoom-based LOD was considered but not added because it would reference an unimplemented API and could fail to link. Whether the API was intended to control layer culling, renderer detail selection, effect resolution, or all three is unverified.
- **価値／懸念:** The declarations imply a control that currently has no observable behavior, while defining a test expectation requires clarifying its intended scope.
- **次に確認:** Define the toggle's scope, implement it in the controller, then add a viewport regression test that checks the affected LOD decisions.

## 2026-10-17: Interactive render region does not restore the prior preview LOD

- **関連:** `Artifact/src/Widgets/Render/ArtifactCompositionRenderController.cppm::setInteractiveRenderRegion()` / `clearInteractiveRenderRegion()`、`setPreviewQualityPreset()`、`tests/Artifact/Model3DLightCompositionIntegrationTest.cpp`。
- **確認できた事実:** `setInteractiveRenderRegion()` changes a Final preset to Preview and sets `irrForcedPreview_`. `setPreviewQualityPreset()` overwrites `previewQualityPreset_` with Preview; `clearInteractiveRenderRegion()` then passes that same Preview value back to the setter. The Final preset is therefore not retained by this path.
- **追加確認:** `setInteractiveRenderRegionResolutionScale()` uses `std::clamp` without a finite-value check, so NaN remains NaN even though the API documents a `[0.25, 1.0]` range. Added low/high, representative, infinity, and NaN checks to the controller integration test; the NaN case is expected to expose this gap and has not been run.
- **変更:** Added GPU blur LOD contracts for region-forced Preview, quality restoration after clear, and resolution-scale bounds. The restoration and NaN assertions are expected to expose the inspected state overwrites; tests were not run.
- **価値／懸念:** An interactive render region can leave the viewport at reduced preview quality after it is cleared, while a NaN scale can reach HUD formatting despite the documented range. The intended restoration behavior is documented in the controller comments; runtime confirmation remains pending.
- **次に確認:** Store the pre-region preset separately from the active forced preset, normalize non-finite resolution scales before clamping, then run the focused controller integration test when test execution is authorized.

## 2026-10-17: Broad renderer module interface changes amplify rebuild and IFC risk

- **関連:** `Artifact/include/Render/ArtifactIRenderer.ixx`, `Artifact/src/Render/ArtifactIRenderer.cppm`, `Artifact/src/Widgets/Render/ArtifactCompositionRenderController.cppm`, `ArtifactCore/CMakeLists.txt` split-target module tables.
- **確認できた事実:** A small new renderer coordination API in the widely imported `Artifact.Render.IRenderer` interface caused broad module dependency reevaluation. In the current Debug build, MSBuild compiled the renderer and Composition controller and linked `Artifact.exe`; the Ninja/MSVC lane compiled `ArtifactIRenderer.cppm` but stopped in the separate `ArtifactCoreAnalyze` target with unresolved `ImageByteSurfaceView` / `SurfaceChannelOrder` diagnostics. MSBuild compiled that same Core target successfully.
- **仮説（未検証）:** Keeping high-churn render coordination declarations in the broad renderer interface may unnecessarily expand IFC invalidation and make manual split-target provider tables more fragile. A narrow render-submission control interface or implementation-only adapter might reduce the rebuild closure, but introducing another module could also add dependency edges and worsen it.
- **価値／懸念:** Compile diagnostics can become very large when an IFC/provider mismatch appears, and wide public interfaces increase the number of consumers rebuilt after a small change. A split that only adds module boundaries without reducing importer closure would add maintenance without improving iteration time.
- **次に確認:** Capture warm incremental action counts for an implementation-only renderer change and a public interface change, then inspect the generated MSVC `/reference` map for `ArtifactCoreAnalyze` in Ninja versus MSBuild before choosing any interface split.

## 2026-10-17: Composition async recording is workload-thresholded

- **関連:** `Artifact/src/Widgets/Render/ArtifactCompositionRenderController.cppm` base-to-overlay flush and `Artifact/src/Render/ArtifactIRenderer.cppm::flushDeferredPacketsAsync()`.
- **確認できた事実:** A point-light Composition integration scene initially produced zero deferred-worker submissions. Adding sixteen 1×1 black Solid layers behind the model raised the supported packet workload, after which the production controller selected the async recorder on both D3D12 and Vulkan; captured frame hashes matched across backends.
- **気づき:** Production worker reachability depends on the actual supported packet count and packet mix. The no-load frame correctly remains synchronous under the current 16-packet minimum, so exercising the worker in an integration test requires a deliberately eligible composition.
- **価値／懸念:** A passing direct renderer contract alone could miss a controller integration that never submits eligible batches. The new test checks both worker selection and the produced Composition frame. It does not show that ordinary scenes gain latency benefit or that the whole frame is non-blocking.
- **次に確認:** Measure representative Composition frames' eligible packet count and worker submit frequency before changing the 16-packet threshold or broadening accepted packet types.

## 2026-10-17: Deferred frame cleanup can outlive immediate execution

- **関連:** `Artifact/src/Render/ArtifactIRenderer.cppm::DeferredPacketRecordWorker` and Composition overlay submission.
- **確認できた事実:** After the immediate owner executes a recorded command list, the worker still calls `FinishFrame()` on its own deferred context. The renderer can release the packet buffer and submit later immediate work while cleanup continues, then gate recorder reuse and teardown on completion. D3D12 and Vulkan overlay-order image contracts and the Composition worker-selection frame passed with this split.
- **価値／懸念:** Waiting for deferred-context cleanup at the first immediate barrier shortens the available overlap window. Resource lifetime and deferred context reuse still require explicit completion tracking; visual parity alone does not measure a latency improvement.
- **次に確認:** Measure cleanup overlap duration on representative frames and verify any other supported graphics backends before widening this completion policy.

## 2026-10-17: Deferred packet eligibility needs an allocation and ownership audit

- **関連:** `Artifact/src/Render/DiligentImmediateSubmitter.cppm::canRecordDeferred()` and `RenderCommandBuffer` packet visitors.
- **確認できた事実:** Gradient rect, dot line, checkerboard, grid, and rect outline recording use packet values plus recorder-owned buffers and pipelines; their immediate and worker images matched on D3D12 and Vulkan. Solid circle currently creates a `std::vector` for every draw, so it remains on the owner path under the render hot-path allocation rule.
- **気づき:** Packet type support should depend on both resource lifetime and the recording implementation's allocation behavior. A list of apparently value-only packets can still hide frame allocations or renderer-owned mutable state.
- **次に確認:** Audit the bounded vertex capacity / growth behavior of `MpmSurfacePkt` and measure how often these newly supported packet types occur in production Composition batches.

## 2026-10-17: Module build error counts can be dominated by IFC graph faults

- **関連:** `ArtifactCore/CMakeLists.txt` scanner-disabled module reference tables, `ArtifactCoreAnalyze`, and split-target Ninja/MSVC builds.
- **確認できた事実:** Earlier split-target diagnostics included duplicate IFC candidates (`C7684`) and missing provider references; these triggered cascades of unresolved type/member errors. The latest `ImageAnalyzer.cppm` failure was fixed by registering `Image.ImageSurfaceView`, `Graphics.SurfaceColorContract`, and the re-exported `Color.TransferFunction` IFCs plus provider object dependencies. Ninja/MSVC `ArtifactCoreAnalyze` then compiled and linked after about 1,520 actions.
- **気づき:** A high diagnostic count does not directly measure the number of independent source defects. For modules, the first IFC/provider/target mapping error often explains a long cascade. Split targets also do not guarantee narrow rebuilds when aggregate dependencies and IFC closures remain broad.
- **価値／懸念:** Sorting build failures by earliest module-resolution diagnostics can avoid repairing downstream symptoms. Conversely, lowering source line counts alone may increase module graph complexity and rebuild work.
- **次に確認:** Capture warm incremental action counts for implementation-only and public-interface edits; compare Ninja and MSBuild provider-reference maps before selecting the next split.

## 2026-10-17: Borrowed PSO pointers must not enter owning PSOAndSRB temporaries

- **関連:** `Artifact/src/Render/DiligentImmediateSubmitter.cppm::cloneSubmitterPipeline()` and worker submitter teardown.
- **確認できた事実:** The glyph pipeline adapter returns borrowed raw PSO/SRB pointers. Wrapping those pointers in a local owning `PSOAndSRB` caused its destructor to release the `ShaderManager` references. Visual Studio debugger triage observed the resulting access violation later in `DiligentImmediateSubmitter::destroy()` while releasing a glyph PSO whose vtable had already been cleared.
- **価値／懸念:** Separate worker SRBs can safely share immutable PSOs only when each owner holds a balanced reference. Borrowed adapters must clone directly from raw pointers or explicitly acquire their own references before entering an owning wrapper.
- **次に確認:** Keep the D3D12/Vulkan worker image contracts covering renderer teardown, and audit future adapter-to-owner conversions for accidental ownership transfer.

## 2026-10-17: LOD contract test families now span generation, selection, and rendering

- **関連:** `tests/ArtifactCore/MeshLodContractTest.cpp`、`PhysicsLodSettingsContractTest.cpp`、`PreviewSettingsLodContractTest.cpp`、`Procedural3DQualityLodTest.cpp`、Artifact 側の `LODManagerContractTest.cpp`、`Model3DLightCompositionIntegrationTest.cpp`、`LayerSurfaceCacheParityContractTest.cpp`。
- **確認できた事実:** 追加されたテスト群は、CPU mesh / meshlet LOD生成とGPU選択、物理負荷段階、描画品質プリセット、ズーム・投影サイズ選択、レイヤー可視性・画像効果サンプリングまでを別レベルで扱っている。Ray Tracing の chooser も疎な段階・境界・バッファ更新／消去を受け入れテストで扱う。Composition Controllerでは、Gaussian Blurの3σオーバースキャンあり／なしの1pxレイヤーを対照し、Low LODのスキップ判定差を確認する統合ケースを追加した（未実行）。
- **気づき:** LOD は単一の値ではなく、生成品質、CPU/GPU選択、実描画、描画解像度、シミュレーション頻度という別々の契約を持つ。テストをこれらの境界ごとに分けると、未実装の領域と重複を見つけやすい。メッシュシェーダー経路は chooser の一致だけでは描画オフセット・dispatch 数を保証できないため、2対1のmeshlet dispatchを3つの領域に分けたreadbackで確認する受け入れテストを追加した。専用のVulkan fixtureに分けてRay Tracing機能との不要な依存も外した（未実行）。
- **価値／懸念:** 今回はテスト起動が禁止されているため全ケースの実行結果は未確認。特に既存大規模GPU統合テストの追加ケースは実行環境依存性を含み、Focused run が必要。
- **次に確認:** テスト実行が許可された後、登録済みLOD専用ターゲットを優先し、その後 GPU / Composition 統合テストを段階的に実行する。

## 2026-10-17: Glyph Text needs a prepared packet before worker recording

- **関連:** `Artifact/src/Render/DiligentImmediateSubmitter.cppm::submitGlyphText()` / `submitGlyphTextTransformed()`、`GlyphAtlas`、`ArtifactIRenderer::DeferredPacketRecordWorker`。
- **確認できた事実:** Glyph submit 中に Qt/HarfBuzz shaping、font fallback、glyph atlas の QImage と map 更新を行う。worker submitter は独自 atlas を持ち、初回 upload では `CreateTexture()` が呼ばれる。既存の `canRecordDeferred()` は両 glyph packet を受け入れない。
- **未検証の設計案:** producer thread で bounded glyph instance data を作り、atlas texture/SRV を保持した immutable packet を worker に渡せば、text preparation を caller 側に残しつつ GPU command recording を移せる可能性がある。atlas eviction / clear、texture upload の順序、複数 submitter 間での lifetime pinning はまだ設計・実機検証していない。
- **価値／懸念:** allow-list だけの拡張は worker 内 allocation と lazy GPU resource creation を持ち込む。まず owner fallback の実画像 contract を固定し、その後に prepared packet の bounded capacity と frame lifetime を決める必要がある。
- **次に確認:** GlyphAtlas が glyph rect を移動・無効化する条件を調べ、frame packet が使う atlas generation を保持または安定化できる契約を設計する。D3D12 / Vulkan の即時・worker 描画画像を比較してから eligibility を広げる。

## 2026-10-17: Deferred eligibility should account for packet work, not only count

- **関連:** `Artifact/src/Render/DiligentImmediateSubmitter.cppm::canRecordDeferred()`、`MpmSurfacePkt`。
- **確認できた事実:** 16個の単位 packet 最小数では、最大75,078頂点の MPM surface 1個でも worker 選択を拒否した。submitter は最大4,096頂点ずつ bounded staging に積んで flush する。
- **変更:** worker eligibility の work estimate に MPM staging chunk 数を加え、最大 surface packet 単独なら worker recording を選ぶようにした。15個の不可視 filler packet を使わない production `ArtifactIRenderer` test が worker selection と描画 pixel を検証し、3頂点の小 packet は owner path に残ることも確認した。Debug / D3D12 と Debug / Vulkan で両ケース pass。opt-in profile で最大 packet の worker record は D3D12 3.966ms、Vulkan 3.697ms。worker PNG hash は両 backend で同一。
- **価値／懸念:** packet 数 threshold は軽い packet 多数を表すには機能するが、単体の大きな geometry packet を過小評価する。worker recording 時間は実測したが、同じ workload の owner recording との比較や GUI latency の改善量は未検証。
- **次に確認:** 実 Composition で MPM surface の packet サイズ分布を計測し、16 chunk threshold が通常 workload の余計な command-list overhead を生まないか確認する。

## 2026-10-17: Particle worker preflight needs its renderer dependency first

- **関連:** `Artifact/src/Render/ArtifactIRenderer.cppm::flushDeferredPacketsAsync()`、`DiligentImmediateSubmitter::canRecordDeferred()`、production worker image contract。
- **確認できた事実:** `canRecordDeferred()` は `ParticlePkt` に対してsubmitter内の `ParticleRenderer*` を必要とする。非同期handoffはそのpreflight後にpointerを渡していたため、Particleを含むbatchは非同期経路へ進まなかった。pointer設定をpreflight前に移動すると、D3D12 / Vulkan のproduction renderer testでworker handoffと後続overlayの画像順がpassし、両PNGのhashが一致した。
- **価値／懸念:** eligibility判定も実行時依存を要する場合、preflight準備の順序が実際のサポート可否を決める。ParticleRendererはmutableなので、handoff中の診断読み取りを避け、再利用前のjob完了待ちを保つ。
- **次に確認:** CompositionのParticle workloadで通常blend以外のrender options頻度を計測し、非対応設定がowner fallbackに残る条件を確認する。GUI latencyへの影響量は未測定。

## 2026-10-17: Large Particle uploads should count toward deferred work

- **関連:** `Artifact/src/Render/DiligentImmediateSubmitter.cppm::canRecordDeferred()`、`ParticlePkt`、production Particle frame image contract。
- **確認できた事実:** `ParticleRenderer::updateBuffer()` issues one `UpdateBuffer()` whose payload grows with particle count. A 65,536 vertex packet (6 MiB) can be worker-selected by counting 4,096-vertex work units; one-particle packets remain on the owner path. Debug D3D12 and Vulkan rendered the worker frame and owner fallback frame with matching per-image hashes across backends.
- **価値／懸念:** packet count alone hides large uploads, and payload-based eligibility produced a 6 MiB upload. Two worker frames on the same renderer retained the same heap high-water marks (D3D12 6/8 MiB, Vulkan 6/7 MiB); D3D12 logged one 8 MiB dynamic page creation for both frames. This suggests capacity reuse, but the logs do not count per-frame suballocations or prove that every Composition context follows the same pattern.
- **次に確認:** Measure allocator calls directly if an existing profiler hook is available, then collect Composition-level particle packet distribution and GUI-thread wait. Keep the 100,000-particle cap and recheck high-water behavior if its capacity changes.

## 2026-10-17: Deferred 3D packets need recorder-owned renderer state

- **関連:** `Artifact/src/Render/ArtifactIRenderer.cppm::DeferredPacketRecordWorker`、`PrimitiveRenderer3D`、`BillboardPkt`。
- **確認できた事実:** `BillboardPkt` の描画は viewport / camera constants buffer と SRB を更新するため、owner とworkerが同じ `PrimitiveRenderer3D` を並行使用すると状態を共有する。worker専用の `PrimitiveRenderer3D` を初期化し、camera / viewport stateをsnapshotとして渡すと、元texture参照解放後もD3D12 / Vulkan両方でdeferred frameが即時画像と一致した。Billboard PSOはdepthを無効化しているため DSV formatを `UNKNOWN` にでき、color-only targetとのbind警告も消えた。
- **価値／懸念:** 3D packetをallow-listへ足すだけではなく、packetを記録するrenderer固有の可変GPU stateもrecorder単位で所有する必要がある。worker rendererはshader / bufferを別作成するため初期化コストがあり、今回の起動時間への影響は未計測。
- **次に確認:** Gizmo / card系のstate snapshot要件を整理し、packetごとに必要な最小stateとtarget / DSVの契約を確認する。Compositionの実 workloadでBillboard頻度とworker初期化時間も計測する。

## 2026-10-17: Gizmo worker recording needs queue and SRB ownership transfer

- **関連:** `Artifact/src/Render/PrimitiveRenderer3D.cppm::pendingGizmoVerts_` / `pendingGizmoRuns_`、`ShaderManager` の Gizmo PSO、`ArtifactIRenderer::DeferredPacketRecordWorker`。
- **確認できた事実:** Gizmoはowner rendererに順序付きvertex/run配列を蓄積し、ShaderManagerから得たSRBへ各描画でTransformCBを設定する。worker記録でowner SRBや同じqueueを触るとoverlay追加との競合になる。owner queueをworker queueへswapし、workerだけのSRBを作成すると、15 filler packet + base + Gizmo handoff後にowner overlayを描くframeが、同期基準とD3D12 / Vulkanの両方で全画素一致した。depth無効pipelineはDSVをbindしないようにした。
- **価値／懸念:** 値のcopyではなくcapacityを含めたqueue ownership swapでframe中のgeometry copyを避けられる。workerのGPU vertex buffer容量拡張はdispatch前のcaller threadで行うため、初回の高watermark拡張自体はcallerを止める。Gizmo頂点生成もcallerに残る。
- **次に確認:** CompositionでGizmo頂点数とworker buffer high-watermarkの分布を計測し、capacity増加が発生する頻度を調べる。Gizmo packetを独立packet化する場合は、現在の2D後 overlay orderingを保持できるframe command contractを確認する。

## 2026-10-17: Deferred Billboard barriers must preserve offscreen color and depth targets

- **関連:** `ArtifactIRenderer::flushDeferredPacketsAsync()` / `draw3DCard()`、`PrimitiveRenderer3D::drawBillboard()`、offscreen render-target stack。
- **確認できた事実:** RGBA8 sRGB offscreen RTV と D32 DSV に対して、worker Billboard packet の後に深度付き immediate 3D card を描く production path が同期 owner frame と一致し、D3D12 / Vulkan 両方で全画素比較を通過した。worker submit counter は async recording の開始時ではなく、command list のexecute/release後に進む。
- **価値／懸念:** 3D draw barrierは次のpacketだけでなく、active offscreen targetとdepth viewを再bindする責務を持つ。画像 parityはこの具体的RTV/DSV formatを確認するが、別formatやComposition内での出現頻度、GUI latencyは測っていない。
- **次に確認:** target override の異なるcolor/depth formatと連続したtarget push/popでworker PSO format、resource state、順序が維持されるかを個別に検証する。

## 2026-10-17: Vulkan headless tests need an actual-device assertion

- **関連:** `DiligentDeviceManager::Impl::initializeHeadless()`、`tests/Artifact/DiligentHeadlessRenderer.hpp`、Deferred renderer image tests。
- **確認できた事実:** Vulkan Deferred 10-case一括実行では2件目以降にDiligentの「global pointers to Vulkan functions」errorが出る。`initializeHeadless()` はVulkan作成に失敗するとD3D12へfallbackするため、GoogleTest全体がpassしても全ケースをVulkanで描いた保証にならない。今回のrender-owner回帰でも、Point-light Compositionとdedicated-ownerを同じVulkanプロセスで続けると後者がVulkan device assertionに失敗し、Diligent global-function-pointer errorが出た。一方、dedicated-owner単独の新しいVulkanプロセスはpassした。各caseを個別プロセスで実行すると各起動でVulkan physical deviceの選択ログを確認でき、全10ケースがpassした。
- **価値／懸念:** GPU render testは環境変数だけでbackendを証明せず、実際に選ばれたdevice typeをassertする必要がある。新しいoffscreen barrier caseにrequested backendとdevice typeの一致assertを加えた。Vulkan backend比較は引き続き個別プロセスを正とし、一括テストのVulkan結果を証拠にしない。同一プロセス失敗の具体的な共有状態原因はまだ未検証。
- **次に確認:** Deferred test helperのbackend identityを全testで共通にassertできる既存の公開面を確認し、共有test harness変更の影響範囲を評価する。

## 2026-10-18: Headless glyph parity needs a registered font fixture

- **関連:** `tests/Artifact/ClonerGpuRenderContractTest.cpp`、`GlyphTextPkt` / `GlyphTextXformPkt`、repository-bundled Roboto fixture。
- **確認できた事実:** headless Qt で未登録の Arial を使うと `QRawFont` が文字列中の複数 codepoint を同じ glyph ID / silhouette に解決し、owner と worker の画像 parity が通っても capture は replacement-box glyphs になった。test が同梱 Roboto を `QFontDatabase::addApplicationFont()` で登録してからは、raw font の distinct glyph ID / mask と実 frame の複数 silhouette を検査する。cold owner fallback で `Seed` のみを atlas に入れ、その後の worker batch で初見 glyph を owner 側で追加・upload してから記録し、warm 後の同期 owner frame と全画素一致することを Debug / D3D12 / Vulkan で確認した。別の bounded ASCII fixture では raw glyph bitmap と2px paddingの総面積が2048×2048 atlasを超えることを確認し、先に同じ大サイズ font の atlas texture を作成・upload して clean にした後、eviction を含むバッチが worker submit count を増やさず owner 経路で可視 glyph frame を出力することを両 backend の実デバイスで確認した。eviction frame PNG の SHA-256 は両 backend で `EAB84E5FF0CA9D0A6B7104BB44B8984EFE0AD59DC9DD55E1FF2FFDE4F16151E5` で一致した。
- **価値／懸念:** headless parity test が同期経路と同じ誤った fallback image を正解扱いする問題を避け、保存した frame で字形が実際に描かれたことを確認できる。atlas容量超過もworkerへ渡さず owner fallback に残る境界を画像付きで固定した。production font fallback 全般や実アプリでのフォント登録状態は未検証。
- **次に確認:** 実アプリの `FontManager` 初期化後に選択される system / user font を使う production-frame contractでも glyph ID と silhouette を確認し、実運用フォントの登録境界を検証する。

## 2026-10-18: Deferred glyph snapshot capacity stays on owner

- **関連:** `Artifact/include/Render/RenderCommandBuffer.ixx::kPreparedGlyphCapacity`、`DiligentImmediateSubmitter::prepareDeferredGlyphPackets()`、`tests/Artifact/ClonerGpuRenderContractTest.cpp`。
- **確認できた事実:** warm atlas 上で2つの2,048-glyph text packetと1 glyph packetを準備すると、4,097番目の prepared glyph で worker recording が拒否され、owner fallback が可視文字を含む frame を描いた。Debug / D3D12 と Debug / Vulkan の device type をテスト内で明示し、capture PNG の SHA-256 は両 backend で `28B2220508B23A29D4551B4B1D73E7D503BF719B66929576414E1CD0EE7F791E` で一致した。
- **価値／懸念:** fixed sidecar capacity が超過した batch を worker に部分移譲せず、描画を owner に戻す境界を画像付きで記録できた。owner fallback では 4,097 glyph の shaping / draw work が依然として caller 側に残るため、大きな text payload の latency は未計測。
- **次に確認:** 通常 Composition の glyph 数分布と 4,096 slot 超過頻度を測り、sidecar の固定容量を変える前に bounded owner preparation cost と worker recording cost を分けて評価する。

## 2026-10-18: Deferred glyph recording preserves later owner overlays

- **関連:** `ArtifactIRenderer::flushDeferredPacketsAsync()` / `flushAndWait()`、`GlyphTextPkt`、deferred double-buffer、`tests/Artifact/ClonerGpuRenderContractTest.cpp`。
- **確認できた事実:** warm atlas で未登録 glyph を含む `Deferred` text batch を worker に渡し、record 中に owner 側の2つ目の command buffer へ blue overlay を追加するケースは、同期 owner frame と全画素一致した。overlay は text の一部を覆い、残りの glyph は表示された。Debug / D3D12 と Debug / Vulkan の実 device で pass し、PNG hash は両 backend で `CC001DBF4E2B7684FCD0E8FD166D283A1FF9997A59642C8B4BCA2384B0B0B6FB` で一致した。
- **価値／懸念:** glyph-specific atlas upload と、worker base batch 後の owner overlay ordering を同じ image contract で確認できた。これは一つの bounded packet mix の順序であり、Composition 全体の非ブロッキングや queue backpressure の改善量は示さない。
- **次に確認:** Composition 側の複数 pass / immediate barriers で base worker batch と owner-produced packet の境界が実 workload でも保たれるか、controller frame sequence の source/runtime evidence を集める。

## 2026-10-18: Deferred Cloner ranges expire at the submit boundary

- **関連:** `ClonerGridPlacementRange::batchGeneration`、`ArtifactIRenderer::flushAndWait()`、Composition の GPU placement draw fallback、`tests/Artifact/ClonerGpuRenderContractTest.cpp`。
- **確認できた事実:** submit は Cloner batch generation を進めるため、以前に dispatch した range を後続 batch の GPU sprite draw に使えない。Production caller はその draw が失敗した場合、通常の transformed sprite draw へ移る。通常の split-submit test に加え、15 filler + dispatch を async handoff した後に immediate clear を呼び、clear が worker batch を drain してから後続 range を stale として拒否する経路も D3D12 / Vulkan で実行した。CPU fallback の3 cloneを含む PNG は両 backend で byte-identical、SHA-256 `5CFCE0311CB2DB4A2DC9B42080F7676BE635AB9A328611817D80853544E0CCB5`。
- **価値／懸念:** deferred queue drain 後に stale GPU range を誤用せず、画面上の clone 描画を継続できる境界を、明示的な immediate clear barrier を含む実画像で固定した。Composition の通常 observer-to-draw path に同じ drain が入る呼び出しは確認されていない。
- **次に確認:** device loss / placement resource 再初期化中の generation invalidation と、Composition 実 frame で observer-to-draw 間の barrier 有無を測定する。

## 2026-10-18: Measure the owner wait at the deferred recording barrier

- **関連:** `DeferredPacketRecordWorker::executeAndRelease()`、`ARTIFACT_VIDEO_PREVIEW_PROFILE_SETUP`、Composition point-light integration。
- **確認できた事実:** owner-side wait on `recordReady_` had no measurable duration at 0.001 ms display precision for the tested 16-packet Composition batch on either D3D12 or Vulkan. D3D12 worker record took 0.798 ms / 0.206 ms across two frames; Vulkan took 0.224 ms. The captured point-light frame hashes matched across backends. First-frame setup and render-to-present totals were much larger and backend-dependent.
- **価値／懸念:** The existing profile separated worker recording and execute, but did not expose whether the owner actually waited at the completion barrier. The new opt-in field closes that observability gap for this boundary. Zero at the current resolution does not imply zero latency in other scenes, and this single workload does not quantify the improvement over synchronous recording.
- **次に確認:** Repeat with representative Composition packet mixes and a matched synchronous owner-record baseline before claiming GUI latency improvement or changing the bounded worker threshold.

## 2026-10-18: Async readback teardown joins the in-flight renderer worker

- **関連:** `ArtifactIRenderer::readbackTextureViewToImageAsync()`、`m_asyncReadbackPool`、Deferred frame contract test。
- **確認できた事実:** A 16-packet Deferred frame was submitted by `readbackToImageAsync()` before an explicit flush. The renderer was destroyed before the test waited for the callback; afterward, the callback image was available and pixel-identical to a synchronous owner-rendered frame. Debug / D3D12 and Debug / Vulkan both passed, with matching PNG hash `9F90B34416CF9A860F0CAEA56B54FCD361D363340593AF3BF6EDB5BA8286FACB`.
- **価値／懸念:** This establishes the normal-completion lifetime and packet-before-copy ordering at the renderer teardown boundary. It does not establish recovery from device loss or a GPU fence that never signals.
- **次に確認:** Exercise texture-size reinitialization while one readback slot is busy, then separately characterize the shutdown behavior when the device is lost or stalled.

## 2026-10-18: Async Deferred handoff must not retire video texture readers early

- **関連:** `flushDeferredPacketsAsync()`、`completeDeferredPacketRecording()`、`PrimitiveRenderer2D::textureForVideoFrame()`。
- **確認できた事実:** The owner advanced the video texture queue epoch when it handed packets to the recording worker, before the resulting command list executed. A red frame queued on a reusable video texture and followed by a blue upload before owner execution rendered blue into the earlier frame on D3D12. Moving epoch advancement to deferred completion and allowing the overwrite callback to drain an in-flight worker batch made the first Deferred frame pixel-identical to its synchronous red baseline; the next frame rendered the blue update. D3D12 and Vulkan passed with matching first-frame hash `2C79384A22C014CC16AA2BE55C417108D63A879FE23DD4940FC370CB88B5ABE8` and second-frame hash `0C5D42EEBCA087A4D52A6318E551534CF4648170F1C07935A819B4C253FE1F94`.
- **価値／懸念:** Queue handoff and GPU execution are different resource lifetime boundaries. This repair keeps the reusable texture immutable until all earlier deferred readers have been executed, without scanning all queued packets or allocating per frame.
- **次に確認:** Verify how often Composition inserts the overwrite boundary in real frame sequencing and characterize normal queue occupancy.

## 2026-10-18: Deferred video texture reuse holds through Cloner and masked draws

- **関連:** `PrimitiveRenderer2D::textureForVideoFrame()`、`drawSpriteTransformedClonerGrid()`、`drawMaskedTextureLocal()`、`tests/Artifact/ClonerGpuRenderContractTest.cpp`。
- **確認できた事実:** Red frame を同期描画した baseline に対し、同じ reusable slot を参照する Cloner / masked Deferred batch を async handoff してから blue frame を同 slot に upload する。両 batch の first frame は baseline と全画素一致（最大 RGBA float error `1.0e-4` 以下）し、次の frame は blue に更新された。Debug / D3D12 と Debug / Vulkan の個別 test process で実 device を確認して pass。Cloner と masked の各 first / second frame PNG は backend 間で byte-identical だった（hashes: Cloner `74BA55BB60526FA817E87DE8FC837B3B93BD93D1FA158DA5BEB9A0F9F9F03982` / `BBE7EE1C5B71549A2366493074D35025A20142529577B0169F8789F223077398`; masked `FF0FEF7C917DBA84D1554283B8E2285978F2C1E71D3F27B7293195567C796572` / `1B6EAEF93115F273DB138C6E18FA52583FBD62BFCE392FCA6F9AAA56C343B12A`).
- **価値／懸念:** Shared video texture reader lifetime is now covered for ordinary sprite packets, Cloner placement/sprites, and masked sprites. These are bounded renderer-level image contracts; they do not establish how often Composition inserts the overwrite boundary or the normal queue occupancy distribution.
- **次に確認:** Exercise the same boundary through a representative Composition frame and characterize normal queue occupancy before adjusting thresholds.

## 2026-10-18: Queue saturation exposed missing video tracking in local sprites

- **関連:** `PrimitiveRenderer2D::drawTextureLocal()`、`drawSprite(float, float, float, float, ITextureView*)`、`ArtifactIRenderer::draw3DShape()` queue-full fallback。
- **確認できた事実:** A full 16,384-packet Deferred batch with a reusable red video texture was handed to the worker, then the same slot was updated blue. The first test run rendered blue in the earlier frame because the ordinary texture-view sprite path appended its packet without calling `markQueuedVideoTextureUse()`. Adding the tracking call after append restored red-first / blue-second output. A second full queue caused `draw3DShape()` to drain and record its eligible packets, then draw the shape immediately on the owner. Debug / D3D12 and Debug / Vulkan passed with visible red and blue probes plus a green shape center; first / fallback PNG hashes matched across backends: `EF2F20156DCC758374D60DB13C7231322289100D258274AFF202037770C719F9` / `4B1E3308A4B3E680F81E2BABF8E9012F102C788B37CA22A77C498D0E2B4BA44E`.
- **価値／懸念:** The texture lifetime contract now also holds for the non-transformed `ITextureView` sprite API, and a bounded queue overflow path is verified with actual GPU output. Existing Composition video drawing uses transformed sprites; the ordinary API fix protects other callers that share the same reusable slot.
- **追加確認:** The production `CompositionRenderController` point-light integration case now inserts 16 small base layers, asserts that `deferredWorkerSubmitCount()` advances, and reads back the rendered frame. Debug / D3D12 and Debug / Vulkan passed; the captured 400×300 PNGs were byte-identical (SHA-256 `AC7C475B62E440744DFF4D9F97D69ED63C38BC71C0020E1A67BC11254AA18754`). This exercises Composition's async base-to-overlay recording window, but the frame does not contain a video layer. Follow-up video-fixture test attempts stayed at FFmpeg hardware-decoder fallback output. Source inspection confirms `preloadFrames()` decodes synchronously; the tested `seekToFrame()` route dispatches decode asynchronously, but the process still did not return within two minutes after the viewport render was included. Exact stopping point remains unknown: the Visual Studio debugger was not running and DebugMCP had no test-runner adapter for this executable. These observations do not establish a production renderer defect.
- **次に確認:** Use an attachable debugger or a test-local timeout boundary to locate the video fixture stall before adding a Composition video assertion. Then exercise cached video-frame replacement while Cloner or a masked draw remains queued, and collect queue occupancy during normal playback before revisiting worker thresholds.

## 2026-10-18: Deferred 3D card recording needs an explicit target and projector snapshot

- **関連:** `Artifact/src/Render/ArtifactIRenderer.cppm::draw3DCard()` / `draw3DTexturedCard()`、`Artifact/src/Render/PrimitiveRenderer3D.cppm::drawCardPass()`、`DeferredPacketRecordWorker`。
- **確認できた事実:** Public card draws previously drained queued packets, then `PrimitiveRenderer3D::drawCardPass()` recorded through its stored `ctx_`, current RTV/DSV, mutable projector texture/matrices, and renderer-owned SRB/constant buffer. The worker renderer already owns separate card PSO/SRB/constant-buffer state but does not receive projector state. `CardPkt` and `TexturedCardPkt` retain active DSV and call-time view/projection matrices; textured packets also retain their SRV. The Composition camera scope resets matrices before the later packet flush, so card packets carry their camera snapshot. `draw3DShape()` now moves its prebuilt triangle-list into `ShapeTrianglesPkt`, which has the same target/camera snapshot; worker upload writes directly into the preallocated shape VB and only accepts up to its 256-vertex startup capacity. Larger geometry stays on the owner path. Polygon triangulation and triangle-list expansion still allocate producer-side vectors. In `drawMesh()`, persistent geometry buffers update only on mesh revision changes, but the per-draw `MeshRenderer` is then mutated with camera, material, environment, scene lights, instance data, transparency, and shadow state before its immediate draw; shadow caster registration and ray-tracing instance / TLAS updates also happen in this caller path. For ordinary `GlyphTextPkt` / `GlyphTextXformPkt`, `PrimitiveRenderer2D` queues `QString` / `QFont`; `submitGlyphText*()` performs HarfBuzz shaping, fallback-font resolution, glyph-atlas acquisition, and GPU atlas upload while recording. Warm ordinary `GlyphTextPkt` now has an owner-preparation path: shaped glyphs and rects go into a pre-reserved 4,096-entry sidecar in the deferred double-buffer slots, and the buffer retains the atlas SRV for the worker. The generic parallel-geometry pool does not reserve this sidecar. Cold atlas creation, full atlas upload / eviction, capacity overflow, and `GlyphTextXformPkt` remain owner fallback. D3D12 and Vulkan offscreen image contracts pass for the warm path. The controller's raw immediate-context references are mostly passed into GPU effect / postprocess helpers; the direct `CopyTexture()` calls found are rollback copies after a failed spatial effect pass. A raw `immediateContext()` access alone does not prove a draw-order barrier is required; source and resource dependency must be traced.
- **気づき:** Solid / Textured Card and bounded shape triangles now join ordered worker recording without sharing owner SRB / constant-buffer state. Active projector, queue exhaustion, and shapes beyond the worker VB's preallocated capacity stay on the owner path. Shape's producer-side triangulation allocations remain a likely follow-up for GUI latency.
- **価値／懸念:** Offscreen image contracts cover worker Card parity after the Billboard barrier and camera reset, projector-active owner fallback, textured two-pass parity after releasing the producer SRV, shape triangle parity, and warm glyph text. Tests verify worker submit counts and actual D3D12/Vulkan device identity. Glyph preparation still runs on the owner; this moves only draw recording, and transformed text remains owner-only. Projector-enabled Textured Card still uses owner immediate recording; Composition effect and postprocess dispatches also remain owner-context operations. Mesh GPU draws are a larger boundary because the cached `MeshRenderer` combines reusable geometry with mutable per-draw state and pre-draw side effects.
- **次に確認:** Measure ordinary Composition frequencies and work cost for Card / Shape / Glyph packet types before widening the 16-unit deferred threshold. Extend prepared glyphs to transformed text only after validating transform / device-pixel-ratio parity and atlas update ordering. For mesh work, separate immutable geometry ownership from per-draw material / instance snapshots and identify which shadow / ray-tracing mutations must stay on the owner before proposing a worker packet. Continue following Composition effect / postprocess helpers through their inputs, outputs, and flush boundary.

## 2026-10-18: Composition channel views need distinct RGB and AOV coverage sources

- **関連:** `Artifact/src/Widgets/Render/ArtifactCompositionRenderController.cppm::composeViewportChannelOverlayImage`、`drawViewportChannelOverlayImage`、`finalizeGpuRenderToViewport`、`tests/Artifact/CompositionViewportChannelTestMain.cpp`。
- **確認できた事実:** Standalone の31モード試験で、完全透明として確認したチャート位置はRGBとR/G/BおよびAOVで黒を保った。以前比較したRGBの値45は半透明チャート位置であり、背景比較点として誤選択していた。テストfixtureはAOVモードでチャートを隠すため、同じ画像内容の比較ではない。`readbackChannelToImage(Alpha)` はColorAlpha fallbackで全面白になり、`lastPresentedReadbackSRV_` から読むとAlpha値に変化が出る。テスト用3D fixtureのDepth readbackは全画素0で、深度値そのものの検証には使えない。`drawViewportChannelOverlayImage()` は元々 `renderer_->present()` 後に呼ばれており、CPU fallbackの表示を同一フレームに反映できない順序だったため、Present前に移動した。
- **変更:** ColorAlpha fallbackのAlpha sourceを最終合成面へ変更し、CPU fallbackのchannel image描画をPresent前へ移動した。Present順序修正後も、AOVの画像内容と背景の完全不変は未確認。
- **価値／懸念:** Final presentation と独立AOV readbackではAlpha/coverageの意味が異なる。チャンネル結果の正しさと、透明領域の背景保持を同じ画像sourceで保証できるとは限らない。
- **次に確認:** Present前への移動後にAOVの実表示値と同一フレームの背景をキャプチャ比較する。Depth fixtureに有効な深度値を出し、全モードの表示内容を検証できるようにする。

## 2026-10-18: Deferred recording and render-thread ownership are separate milestones

- **関連:** `ArtifactIRenderer::initializeOnRenderThread()`、`DiligentDeviceManager::initializeOnRenderThread()`、`tests/Artifact/Model3DLightCompositionIntegrationTest.cpp`。
- **確認できた事実:** The first smoke-test attempt omitted `QApplication` initialization; the next created the render child HWND from the worker. Creating the HWND on the GUI thread and passing it to the render owner removed those test setup faults. The final Debug D3D12 and Vulkan isolated-process runs each passed twice using the existing shared device / immediate context: the worker initialized the swapchain, cleared and read back a 320×240 frame, and presented once. The saved PNGs were visually the expected solid orange-red and byte-identical across both backends and reruns (SHA-256 `3F9A730CF7BA3CF6540471CF68AC24DFCA804C55BC6B83265E37C1DE25A82C62`). Existing Deferred Context tests still exercise worker command recording while the Composition frame owner and `present()` remain on the caller thread.
- **価値／懸念:** The test proves the shared immediate context can be initialized and used by a dedicated worker in isolation, which preserves compatibility with existing device-owned resources. It does not prove that the Composition frame graph, layer traversal, caches, or final present have moved off the GUI thread; other shared-context clients must be transferred or serialized before production ownership changes.
- **追加確認:** After the manager change, the existing D3D12 Point-light Composition integration test failed once (`baselineImage != deferredRestoredImage`, then null final image), passed on an immediate isolated-process rerun, and passed once on Vulkan. The failure's cause is unverified; do not attribute it to either the change or known backend flakiness without another controlled run.
- **次に確認:** Define immutable frame inputs and GPU resource lifetime handoff for `CompositionRenderController::renderOneFrameImpl()`. Move production frame work in bounded slices, retaining backend PNG parity and thread-identity assertions at each integration gate.

## 2026-10-19: Offscreen Composition preview is not a production 3D parity oracle

- **関連:** `Artifact/src/Render/ArtifactOffscreenCompositionRenderer.cppm::renderFrame` / `renderLayerFrame`、`tests/Artifact/Model3DLightCompositionIntegrationTest.cpp`。
- **確認できた事実:** Composition `renderFrame()` binds/clears the offscreen color RTV but not the renderer depth target; `renderLayerFrame()` binds and clears both. The full Composition route also omits controller scene-light preparation and does not apply its `FramePosition` to the Composition before layer traversal. A test-only attempt added DSV bind/clear, `goToFrame()` with restoration, and gizmo flush, but the source Composition image remained black even though model active/visible/final/evaluate gates were all true. The temporary library change was reverted; the exact missing setup remains unverified. Source/worker 3D-layer `renderLayerToQImage()` with matching camera, gizmo camera, and empty-light state instead yields non-empty, pixel-identical images on isolated D3D12 and Vulkan runs. This validates only the cloned model layer draw path; the production Composition thread gate still fails because its frame body thread ID equals the GUI thread ID.
- **価値／懸念:** Offscreen helper naming can imply full Composition equivalence even though its setup diverges from the controller. Do not use its black output as worker parity evidence, and do not treat DSV binding alone as a fix. The first worker model draw is cold-start heavy; persistent-worker repeated draws were about 7.7 ms D3D12 and 2.3 ms Vulkan in the latest runs, but this is a single layer fixture, not full Composition timing.
- **追加確認:** Reused the same worker-owned renderer for three draws. First-frame and third-frame images matched, as did source-model and worker-clone images on D3D12 and Vulkan. D3D12 renderer initialization / first draw / average warm draw measured 862 / 1172 / 7.30 ms; Vulkan measured 1225 / 2411 / 1.70 ms. These are single-fixture Debug cold-start measurements. The production frame identity assertion still fails on both backends because `renderOneFrameImpl()` runs on the GUI thread.
- **追加確認:** Asserted worker thread affinity for every layer QObject in the rehydrated two-layer Composition (3D model and light); all passed on both D3D12 and Vulkan. Updated single-run Debug measurements were D3D12 renderer init / first draw / warm average 1239.8 / 1397.4 / 7.69 ms and Vulkan 1858.9 / 3914.7 / 2.28 ms. Variation from the previous sample reinforces that cold-start numbers are diagnostic, not performance guarantees. The end-to-end controller gate continues to fail only at the expected GUI thread identity assertion after image checks.
- **追加確認:** Tried binding/clearing the DSV and applying/restoring the requested frame in the test-only full-Composition offscreen path. Its source image remained black although the model was active, visible, final-render eligible, and selected for evaluation. Reverted the temporary Artifact library and full-Composition test changes because the test did not establish valid output. After rebuilding the reverted library and focused test, worker-layer image, full layer QObject affinity, repeated-frame stability, and source/clone equality passed again on both backends. Latest Debug cold init / first draw / warm average was 1055 / 1329 / 6.63 ms D3D12 and 1200 / 3154 / 2.29 ms Vulkan. The production thread-gate still fails at GUI thread identity after image checks.
- **追加確認:** The active `PreciseTicker` callback explicitly posts its frame body to the controller QObject with `Qt::QueuedConnection`, and the public manual `renderOneFrame()` calls `renderOneFrameImpl()` synchronously. The latter mutates the controller's `Impl` state (renderer, preview pipeline, caches, temporal history, frame counters) and presents. Direct owner callbacks inside it include two `videoDebugMessage()` calls and one `markRenderDirty()`. Changing only the invocation thread would race shared controller state and violate QObject affinity; it needs a single-owner handoff for the render state plus a GUI-captured frame request and GUI-posted results.
- **次に確認:** Identify a faithful production frame slice whose immutable inputs and GPU resource lifetime can be transferred without JSON round-trip per frame. Implement an explicit single-owner render-state boundary and capture/completion protocol, then keep the end-to-end completion gate on production Composition output and worker thread identity. Do not use the current Composition offscreen helper as whole-scene parity evidence.

## 2026-10-18: GUI-created resources need the app's shared device before worker handoff

- **関連:** `ArtifactIRenderer::initializeOnRenderThread()`、`DiligentDeviceManager::initializeHeadless()`、`tests/Artifact/Model3DLightCompositionIntegrationTest.cpp`。
- **確認できた事実:** A green texture created by a renderer initialized with `initializeHeadless()` was not a valid Vulkan handoff to a worker renderer using the shared-device lease: Diligent rejected concurrent Vulkan instance/device creation and the worker fell back to D3D12. Initializing both renderers through the QWidget/shared-device path removed that setup mismatch. The worker sampled the GUI-created texture and presented a real frame; D3D12 and Vulkan each passed twice in separate processes, with all four PNGs byte-identical.
- **価値／懸念:** A resource pointer crossing threads is not sufficient evidence of GPU-resource compatibility; its device identity and lifetime must match the receiving renderer. The isolated test serializes GUI texture creation before worker use and does not establish safety while unrelated clients concurrently use the same immediate context.
- **次に確認:** For Composition, identify a frame-owned immutable resource snapshot that retains the GUI-created GPU resources through worker completion, and define how all shared immediate-context submissions are ordered around the frame owner.

## 2026-10-18: Composition's host snapshot is not yet a render-frame snapshot

- **関連:** `CompositionHostFrameSnapshot`、`CompositionRenderController::renderOneFrame()`、`CompositionRenderController::Impl::renderOneFrameImpl()`。
- **確認できた事実:** `PreciseTicker` は `std::thread` 上で callback を呼ぶが、Composition controller はその callback から `QMetaObject::invokeMethod(Qt::QueuedConnection)` で GUI affinity に戻してから `renderOneFrameImpl()` を呼ぶ。公開 `renderOneFrame()` も同関数を同期実行する。現在の `CompositionHostFrameSnapshot` は host visibility/size/DPR、viewport transform、Composition size/frame metadata、debug flags の値だけで、scene/layer state や retained GPU resources はない。frame body は約7,000行規模で、前回調査では`renderer_` 294箇所、`immediateContext()` 12箇所を参照した。2026-10-19の現行関数範囲は7,212行。body内に`QWidget` / `QApplication`直接参照はないが、Playback Service参照が5箇所、controller owner callbackが3箇所あり、Implのrenderer・Composition・render cache/contextへ依存する。GUI tickは`renderInProgress_`を保持し、`prepareHostFrame()`はGUI上でviewport sizeとswapchainを更新してからframe bodyを呼ぶ。`ArtifactAbstractComposition` は `QObject` を継承する。既存 Render Queue の worker snapshot は `toJson()` / `fromJson()` で Composition を複製し、video layer の load completion のため clone 後に Qt events を最大15秒 pump する。`ArtifactAbstractComposition::revision()` は Composition API の `changed()` が進める一方、layer property 変更は独立した `ArtifactAbstractLayer::changed()` から `LayerChangedEvent` を発行しており、composition revision 単独では layer snapshot cache の完全な invalidation key にならない。Diligent の backend / synchronization guidance は同じ context の複数 thread 同時使用を避ける契約を示し、deferred context を並列 command recording、immediate context を command-list execution に使う構成を説明している。Artifact の複数 render service が immediate context API を使うが、各 renderer が共有 device lease を使うか独立 device を所有するかは caller ごとの確認が必要。PlaybackRangeServiceContractTestは現行packet handoffを含め6/6 passしたが、本番Composition thread gateはGUI thread一致でfailした。
- **価値／懸念:** 現状の ticker が専用 thread であることは描画が専用 thread である証拠にならない。Composition clone を毎 frame worker 上で作る方法は、QObject affinity、動画ロード待ち、JSON 全量処理のため適切な frame packet にならない。revision だけで clone を cache すると layer property edits の stale state を使う恐れがある。同じ renderer/context を使う利用者との ownership は明示的に確認が必要であり、worker移行後の並行呼出しを許す根拠にはできない。描画 thread を成立させるには、frame input/lifetime handoff と immediate-context single-owner submission 経路の両方を確立する必要がある。
- **価値／懸念:** frame body自体にQt widget APIが直書きされていることより、controllerの可変render state、Composition QObject、共有immediate contextが同じImplに束ねられていることが移行障壁である。関数呼び出しだけをworkerへ移すとGUI editとのstate raceとGPU contextの同時使用を招き、GUI応答性も保証しない。
- **追加変更と検証:** setup profile判定とProfilerの再生中flagは、controllerがGUI側でcaptureした`HostFrameSnapshot.playbackRunning`から読むようにし、render body内のPlayback Service `isPlaying()`を2箇所減らした。VS18 Debug rebuild後、packet/camera/mailbox契約4/4、worker clone→present画像testはD3D12 / Vulkan双方pass。production thread gateは双方とも画像capture後にGUI thread ID一致でfailし、変更が描画threadを移していないことを確認。RAM Preview状態・画像・uploadへのPlayback Service参照は引き続き残る。
- **追加変更と検証:** host snapshotにComposition IDと一致確認済みのplayback frame位置、RAM Preview upload pinを含め、render bodyのComposition frame lookupとupload lookupをlive Playback Serviceからrequest値へ移した。worker packetはcaptured playback frameをJSON保存frameより優先してcloneへ適用する。CPU image borrowとRAM Preview cache/status、defer/readback side effectsはまだPlayback Serviceに依存する。新しいcaptured-playback-frame contractを含むsnapshot/packet/mailbox testsは5/5、PlaybackRange service testsは6/6。worker clone→PresentはD3D12 / Vulkan両方passし、production thread gateは両方GUI thread ID一致でfailした。Production画像は両backendとも SHA-256 `0EEFCA2B91D08008E879513A0A4AB75EEA523E0A2F874F54B595398E7F49ED19`で一致。
- **追加変更と検証:** `captureCompositionPlaybackFrameSnapshot()`がenabled、frame cache state、pending-build、fallback policy、summary、pinned uploadをPlayback Service所有thread上で捕捉し、render bodyのcache state / pending / summary照会をsnapshot経由にした。worker threadからのcaptureを拒否してsnapshotを未変更にするtestも追加し、PlaybackRangeServiceContractTest 6/6 pass。Production gateはD3D12 / Vulkanで実GPU画像を取得した後にGUI thread ID一致でfail。CPU image borrow、readback完了・defer更新のservice副作用、renderer/context/cache/state所有権は未完了。
- **次に確認:** JSON cloneをper-frame transportにせず、値化できるscene/layer inputsとretained GPU resourcesを持つbounded immutable frame packetを作る。single-owner render-state boundary、GUI側snapshot capture、worker側frame execution、completion/present orderingを結び、GUI edit・resize・shutdown順序を保つ実Composition image gateを用意する。

## 2026-10-18: Composition frame requests need scene mutation identity

- **関連:** `Artifact.Widgets.CompositionHostFrameSnapshot`、`CompositionRenderController::Impl::captureHostFrameSnapshot()`、active Composition の `LayerChangedEvent` subscription。
- **確認できた事実:** `CompositionHostFrameSnapshot` now carries `ArtifactAbstractComposition::revision()` and an atomic `sceneMutationGeneration`. The controller advances the generation for accepted layer events on its active Composition, including `FrameReady`. Both values are captured on the GUI thread with the viewport/frame metadata. The generation is suitable for identifying a stale queued request, but no worker consumes it yet and it contains no layer state or GPU resource leases.
- **価値／懸念:** The composition revision alone misses layer property changes; combining it with the event generation gives a lightweight invalidation identity for future bounded frame requests. It does not make the QObject scene immutable or permit worker dereferences. A later worker queue must re-check this token without reading Composition from the worker and still receive an independently owned scene/resource snapshot.
- **検証:** VS18 Debug `ArtifactAppRuntime.vcxproj` and `ArtifactModel3DLightCompositionIntegrationTest.vcxproj` builds passed without CMake regeneration. The production Point-light frame test passed on D3D12 and Vulkan on controlled reruns; both saved 400x300 PNGs matched baseline SHA-256 `AC7C475B62E440744DFF4D9F7D69ED63C38BC71C0020E1A67BC11254AA18754`. One earlier run per backend failed the existing baseline/restored image equality check and passed on isolated rerun. The dedicated-Composition-thread gate generated the same verified PNG on both backends and failed only its expected thread assertion; the native frame thread ID equalled the GUI thread ID.
- **次に確認:** Build the bounded immutable scene/resource packet and use `sceneMutationGeneration` to reject stale work before rendering; then make the production thread-identity image gate pass on both backends.

## 2026-10-18: Existing CompositionRenderWidget worker is not the editor render path

- **関連:** `ArtifactCompositionRenderWidget::Impl::startRenderLoop()`、`ArtifactCompositionRenderWidget::Impl::renderOneFrame()`、`ArtifactCompositionEditor`。
- **確認できた事実:** `ArtifactCompositionRenderWidget.cppm` contains a `std::thread` render loop and calls `ArtifactIRenderer::present()` on that worker. Repository-wide source search finds no construction/use of `ArtifactCompositionRenderWidget` outside its own declaration and implementation. `ArtifactCompositionEditor.cppm` imports `Artifact.Widgets.CompositionRenderController` and constructs `CompositionRenderController` for the main and secondary panes. Its production frame identity gate still reports GUI-thread execution. The legacy widget worker also reads the Composition and playback service from its worker and accesses widget dimensions during rendering, so it is not a safe template for moving the editor controller's frame body.
- **価値／懸念:** A render thread already present in an unused/legacy widget can make the app look thread-separated in a source search while the active editor path remains synchronous on GUI. Migrating by reusing that loop would carry scene ownership and QWidget-affinity hazards into the active path.
- **次に確認:** Keep the production `CompositionRenderController` as the acceptance target. Separate immutable scene inputs and renderer/context ownership before moving frame execution; retain the thread-identity plus rendered-image gate on D3D12 and Vulkan.

## 2026-10-18: Shared render-device mutex does not serialize GPU context use

- **関連:** `SharedRenderDeviceState`、`acquireSharedRenderDeviceForCurrentBackend()`、`DiligentDeviceManager::initializeOnRenderThread()`。
- **確認できた事実:** `SharedRenderDeviceState::mutex` guards shared device creation/acquisition, reference counting, and release/invalidation. `DiligentDeviceManager::initializeOnRenderThread()` acquires the same shared device and immediate context; no operation-wide context lock is held around draw, submit, or present calls. Other managers can acquire the same context lease while the mutex is released.
- **価値／懸念:** The shared device lease is a lifetime mechanism, not an execution-serialization contract. Moving Composition calls to a worker while leaving other renderer clients active can concurrently use the same immediate context; serializing only initialization/refcount does not prevent that. A global owner/executor or an explicit frame-level exclusive-use contract is needed in addition to the scene snapshot.
- **次に確認:** Map every production shared-device user and its frame boundary, then choose one app-wide immediate-context owner model and test interleaved viewport/render-queue requests on D3D12 and Vulkan before Composition migration.

## 2026-10-18: RenderCommandBuffer is a GPU packet buffer, not a Composition frame snapshot

- **関連:** `RenderCommandBuffer::swapContents()`、`DrawPacket`、`ArtifactIRenderer::flushDeferredPacketsAsync()`。
- **確認できた事実:** The existing command buffer can exchange preallocated packet storage and retains several referenced GPU resources (`ITextureView` / target views). Its packet variants also include renderer-specific GPU dispatches, prepared-glyph ranges, and Qt value types. `ArtifactIRenderer` records eligible packet batches through a Diligent Deferred Context, then executes their command list on the immediate context. Composition frame work also performs direct offscreen target changes, resolves, compute passes, layer traversal, and caches outside this packet stream.
- **価値／懸念:** The command buffer is a useful bounded handoff for eligible draw packets, but moving its worker recorder or swapping the buffer does not transfer immediate-context ownership and does not represent the full Composition frame. A dedicated drawing owner would still need ordered control of direct GPU passes, renderer resources, and present.
- **次に確認:** Trace which Composition passes can be represented by packets without changing target / resource ordering, and identify the first complete ordered pass interval that can be executed by one worker-owned immediate context while preserving its frame image.

## 2026-10-18: Composition frame graph is diagnostic, pass execution remains synchronous

- **関連:** `CompositionRenderController::Impl::renderOneFrameImpl()`、`RenderPassExecutor::runWithRenderGraph()`、`buildFrameRenderPassPlan()`。
- **確認できた事実:** `renderOneFrameImpl()` constructs a frame-level `RenderGraph` when the pass plan changes or diagnostics are enabled, but only compiles it for validation. The actual frame still invokes legacy rendering inline. `RenderPassExecutor::runAllWithRenderGraph()` serially loops through pass objects, and each executor pass captures the live renderer/context and resources by reference. The layer sequence additionally changes render targets, runs resolve/effects/AOV work, draws overlays, and calls present in that same caller frame.
- **価値／懸念:** The current graph is not a scheduled worker graph, and its current pass closures cannot safely be copied to a worker because they retain references to mutable frame locals and renderer state. Turning on graph execution in parallel without explicit resource dependencies and owned packet inputs would change ordering and lifetime guarantees.
- **次に確認:** Refactor one real frame stage into an owned pass input/output contract, not just a diagnostic graph node; test its output against the existing production Composition PNG before migrating additional stages.

## 2026-10-18: Worker renderer can own a D3D12 device, Vulkan must transfer its shared device

- **関連:** `ArtifactIRenderer::initializeIndependentOnRenderThread()`、`DiligentDeviceManager::initializeIndependentOnRenderThread()`、`createIndependentRenderDevice()`。
- **確認できた事実:** Added a render-thread renderer initialization path that creates an independent device/context and swapchain, distinct from the existing shared-device path. Debug D3D12 test verified distinct GUI/worker device and context pointers, worker-thread draw/readback/present, and a captured frame. The worker image matched the Vulkan shared-context worker image byte-for-byte (SHA-256 `EA350DF86A123D7B80504A8939AB18553429D7C6DF2E8366CFEE797384594719`). Attempting a second Vulkan instance/device while the app shared Vulkan device was live produced Diligent's documented process-global function-pointer error; the new independent path now rejects that state before creation. Vulkan uses the existing shared device/context worker path, and both the shared-resource handoff smoke and dedicated-owner image test pass on D3D12 and Vulkan.
- **価値／懸念:** D3D12 can isolate Composition worker context usage from shared-device clients. Vulkan can move context use to a worker only when the app coordinates exclusive ownership of the one shared Vulkan device; this test proves sequential worker use, not concurrent safety or Composition scene lifetime. The production `CompositionRenderController` still runs its frame body on GUI.
- **次に確認:** Wire the active controller to the appropriate backend owner path only after its scene/resource packet and app-wide shared Vulkan context schedule are explicit; preserve the production frame thread-ID and image gates.

## 2026-10-18: Composition image tests must wait for present before readback

- **関連:** `CompositionControllerRecordsDeferredFxaaWhileRenderingSpotShadowMap`、`CompositionRenderController::readbackPresentedFrameImage()`、`lastPresentedReadbackSRV_`。
- **確認できた事実:** The production Spot overlap test's helper initially called `readbackPresentedFrameImage()` before waiting for a new successful present, and Vulkan sometimes returned null. Reordering the helper to wait for `presentSuccessCount` first removed the failures in three profile-off Vulkan runs and two D3D12 runs. The capture fallback counter remained zero on the latest run. Both backend frame PNGs were visually inspected and byte-identical (`AC7C475B62E440744DFF4D9F7D69ED63C38BC71C0020E1A67BC11254AA18754`).
- **価値／懸念:** The intermittent null was a test-side ordering issue; it was not evidence that the deferred overlap failed or that the renderer thread probe caused a defect. `currentFrameSerial` may advance before `present` completes, so a frame test must gate readback on successful presentation.
- **次に確認:** Keep frame capture APIs tied to the presented serial where available. Continue testing renderer ownership independently; this correction does not move Composition frame execution or `present()` to a dedicated drawing thread.

## 2026-10-18: Keep the Composition thread gate independent from deferred-packet mutation coverage

- **関連:** `CompositionFrameBodyRunsOutsideTheGuiThread`、`CompositionControllerAppliesPointLightLayer`、`exerciseCompositionLight()`。
- **確認できた事実:** The production thread gate had enabled the separate async-packet test path, which adds sixteen layers and mutates visibility/color before comparing restored images. The point-light integration test already independently asserts deferred worker submission. After disabling that option only for the thread-identity gate, D3D12 and Vulkan each produced an actual 320×240 presented-frame PNG with identical SHA-256 (`ABCBCE0B0225AA7BF5BFE985BF3157165416BBB342B9AAFC4F1A8B8FE383C3DB`); both passed image assertions and failed only because the frame thread ID remained the GUI thread. The separate point-light async-worker test passed on both backends.
- **価値／懸念:** Separating these assertions makes a thread-ownership failure easier to diagnose without dropping async-recording coverage. It does not move any production frame work or establish GUI responsiveness.
- **次に確認:** Build and consume an immutable, bounded Composition frame packet on a dedicated worker while preserving the image hash and the separate deferred-worker test.

## 2026-10-18: Existing GPU effects can resolve a render-owner device through a thread-local binding

- **関連:** `ScopedRenderDeviceBinding`、`acquireSharedRenderDeviceForCurrentBackend()`、`releaseSharedRenderDevice()`、Composition worker renderer tests。
- **確認できた事実:** Many GPU effects obtain their device/context through the shared acquisition API instead of receiving the active renderer. A scoped, thread-local binding now lets those existing call sites resolve the currently bound worker renderer device/context, while acquisitions outside the scope continue to use the app shared device. The D3D12 independent-device and Vulkan shared-device worker image tests passed and asserted that the binding is removed after its scope. Both backend PNGs matched byte-for-byte.
- **価値／懸念:** This provides a device-identity handoff for existing effect code without editing dozens of effect modules. The scope must outlive all acquisitions made through it. Binding the app's shared Vulkan context still does not serialize concurrent GUI/render-queue use, and the active Composition controller has not been moved to a worker.
- **次に確認:** Build an immutable frame input and worker-owned Composition clone/resource lifetime around this binding; add explicit Vulkan shared-context scheduling before enabling asynchronous production frames.

## 2026-10-18: Composition JSON can be packetized on GUI and rehydrated with worker QObject affinity

- **関連:** `CompositionRenderFramePacket`、`CompositionHostFrameSnapshot`、`ArtifactAbstractComposition::toJson()` / `fromJson()`、`CompositionFrameBodyRunsOutsideTheGuiThread`。
- **確認できた事実:** The new frame packet owns a serialized Composition document beside copied host facts and source identity. In the production Point-light fixture, the GUI built the packet before activating the Composition, then a worker reconstructed the scene. The clone had worker-thread QObject affinity and matched the source ID, effective size, and layer count. D3D12 and Vulkan both passed those packet assertions; the regular presented Composition images remained byte-identical across backends. The production frame-body thread assertion still fails because rendering continues on the GUI thread. A fresh VS18 Debug rebuild passed; the isolated D3D12 and Vulkan runs again passed the image checks and stopped only at that expected thread assertion. Captured 2761-byte PNGs had the same SHA-256 (`ABCBCE0B0225AA7BF5BFE985BF3157165416BBB342B9AAFC4F1A8B8FE383C3DB`).
- **価値／懸念:** Existing JSON persistence gives an ownership boundary that can be tested without letting the worker dereference GUI-owned Composition objects. The two-layer Point-light fixture measured JSON capture at 12.062 ms (D3D12) / 8.606 ms (Vulkan) and worker rehydration at 17.879 ms / 15.509 ms. This is already too costly to perform synchronously for each edit or playback frame. The active controller advances `sceneMutationGeneration` on each accepted non-`FrameReady` layer event and captures host metadata on the GUI render path, so serializing the cached packet there would put full-scene JSON work on frequent drag edits. Avoid eager controller-side serialization until a coalesced, bounded handoff exists or measurements justify it. Clone reconstruction does not yet preserve all runtime decoder/effect GPU resource lifetimes, and this probe does not render the clone.
- **次に確認:** Add a representative multi-layer payload/cost measurement, then design a persistent worker-owned scene/resource update path that does not block the GUI on full-scene JSON serialization. Keep the worker frame image gate and shared Vulkan context scheduling explicit before connecting present/completion to the live viewport.

## 2026-10-19: Composition worker-clone probe should verify reopened model geometry

- **関連:** `CompositionRenderFramePacket`、`ArtifactAbstractComposition::fromJson()`、`Artifact3DLayer::sourcePath()` / `mesh()`、`CompositionFrameBodyRunsOutsideTheGuiThread`。
- **確認できた事実:** The production Point-light fixture serializes a real OBJ-backed 3D layer. After reconstructing the Composition on a worker, the test now verifies that the cloned model has the same source path and mesh vertex count as the GUI-owned source. Those checks passed on D3D12 and Vulkan. The frame-thread gate still records the GUI thread ID; image readback was transiently null on the first paired run, while separate reruns produced byte-identical GPU-readback PNGs across both backends.
- **価値／懸念:** Layer-count equality alone could pass when a worker clone had lost its externally loaded mesh. The stronger assertion confirms model-file reopening for this fixture, but it does not verify texture/material dependencies, effect runtime state, GPU uploads, or rendering the clone.
- **次に確認:** Render an owned worker clone through the production Composition frame path and compare the presented pixels with the GUI-owned source. Preserve explicit assertions for all external assets used by the fixture and investigate null readback separately from frame thread identity.

## 2026-10-19: A synchronous render pass worker proves ownership, not GUI responsiveness

- **関連:** `ArtifactIRenderer::ImmediateContextPassWorker`、`renderShadowMapFrame()`、production Composition Spot-light rendering。
- **確認できた事実:** The production shadow-map pass now executes on a persistent worker thread and its native thread ID differs from the GUI thread. The D3D12 and Vulkan Spot-light image/Deferred-FXAA overlap tests pass with matching PNG hashes. The GUI caller waits synchronously for the pass, and Composition frame traversal plus `present()` remain on the GUI/caller thread. The full-frame thread assertion still fails after the image checks.
- **価値／懸念:** This isolates a real immediate-context pass on a render worker and keeps worker creation out of the per-frame path after first use. It is not an asynchronous render-thread architecture and does not improve UI responsiveness; first-use worker startup remains a cold-path cost. Inline fallback also means thread identity must remain an explicit test contract.
- **次に確認:** Define ownership and lifetime for immutable frame input, mutable evaluation/cache state, immediate-context access, and swapchain present together before moving the complete Composition frame. Measure UI wait time and validate shutdown/resize ordering when the queue becomes asynchronous.

## 2026-10-19: The Composition frame body has GUI callbacks beyond its renderer

- **関連:** `CompositionRenderController::Impl::renderOneFrameImpl()`、overlay helpers、onion-skin capture、RAM preview readback、damage continuation。
- **確認できた事実:** The frame body passes its controller owner to selection/interaction/viewport overlay helpers, publishes video debug messages, queues onion-skin capture, captures a `QPointer` for asynchronous preview readback completions, and calls `markRenderDirty()` when bounded damage remains. It also mutates the live preview pipeline, temporal state, damage slots, and renderer resources. The frame-body worker gate continues to fail specifically because execution remains on the GUI thread after image checks.
- **追加確認（2026-10-19）:** RAM Previewのframe body描画分岐は、GUI snapshotでpin済みの`ImageUploadBuffer`からrenderer textureを解決するよう変更され、CPU cache image borrowとCPU画像fallbackは取り除かれた。低レベルupload-only texture経路のD3D12/Vulkan画像比較はpassしたが、Production controller branchを通すintegration fixtureは黒画像になり、fallback選択・texture作成・sprite描画・fixture条件のどこが原因か未特定。よって本番fallback branchの描画は未検証であり、worker安全性の証拠にも数えない。
- **追試（2026-10-19）:** opt-in profileを拡張して判定状態を出したところ、frame 1/2で`fallback=1 reason=ready`とRAM passの実行を確認した。画面/window captureはそのframeのGPU出力と一致しなかった。RAM passはcontrollerの通常composite readback SRVを生成しないため、現行readback APIだけではbranch出力を確証できない。赤pixel fixtureは外し、viewport最終出力を取得する明示的なtest/readback契約を次の確認点とする。
- **修正確認（2026-10-19）:** 黒画像はRAM passの描画不良ではなく、`Present()`後の`readbackToImage()`が次のswapchain backbufferを読むためと特定した。profile有効時のみpresent前にqueued drawsをdrainしてbackbufferをPNG captureし、production controller fixtureをD3D12/Vulkanで実行。双方で赤中心pixel・viewport coverageを含むassertionがpassし、400×300 PNG hashも一致（`FB58E291346EF0810FC2D37356F61A650347A47625793DA04F273F477217B6A3`）。通常非profile時のreadback契約と本番frame bodyのthread移行は未完了。
- **価値／懸念:** The helper argument is an active UI/async ownership edge; moving only the large function body to a worker could cause off-thread QWidget/controller use or lifetime races even if GUI drawing is blocked during a synchronous wait. `showSelectionRectLines`, Tracker activity, and the TrackPoint/RigSelect/RigWeight/Clone/Pen/Rectangle tool kind plus overlay label are now captured into `CompositionHostFrameSnapshot`; these values flow through Selection Editing and frame overlays, and overlay, selection-editing, and ghost helpers no longer receive an unused controller pointer. D3D12/Vulkan Point-light images remain byte-identical and the Spot/deferred-overlap test passes, but the full-frame worker gate still fails on GUI thread identity. PuppetTool, MotionSketchTool, BrushTool, and selection-manager references remain in overlay rendering; the Tracker-active visual branch does not yet have a dedicated runtime fixture. Shift/Alt/Ctrl modifier flags are now GUI-captured values for three frame overlay decisions, while event handlers retain live modifier reads. A worker-owned serialized clone can traverse and draw its full 3D+light Composition into an offscreen target; D3D12/Vulkan output with a fixture Point Light matched pixel-for-pixel (320×240, SHA-256 `08EB28A632F987DC0C8E052BAE691F1F0189D182776551881FDE9D951F9B9108`). The no-light/lit center-pixel comparison prevents a non-black silhouette from satisfying the light gate. A separate worker-owned swapchain test now presents that clone's SRV from the same worker, but active-controller cache/effect/AOV/present ownership, live shared-context scheduling, and worker frame evaluation's process-wide PhysicsSystem interaction remain unverified.
- **次に確認:** Replace active owner calls with a value-based frame input and a bounded frame result returned to the GUI thread; pin scene/resource lifetime and serialize resize/present before enabling worker execution for the full Composition frame.

## 2026-10-19: A worker-owned Composition clone can now render and present one frame

- **関連:** `OffscreenCompositionRenderer::renderTargetShaderResourceView()`、`CompositionRenderFramePacket`、`WorkerCompositionCloneRendersAndPresentsOnDedicatedThread`。
- **確認できた事実:** A GUI-created child HWND is passed to one worker that reconstructs the JSON Composition clone, verifies worker affinity for all layer QObjects and the reopened model mesh, renders the full layer traversal to an offscreen target, samples that target into a worker-owned swapchain, and presents from the same worker thread. The dedicated contract passes on D3D12 and Vulkan, checks requested device type, output dimensions, centered model coverage, and exactly one successful present. The worker-presented 400×300 PNGs are byte-identical (SHA-256 `96EFBCBF789AD799929D89F22523536EA70DC16C70C3D2599AB57C9C8596FB7D`). The separate production controller gate still passes image/readback assertions and fails only because its frame body remains on the GUI thread.
- **価値／懸念:** This joins previously separate worker-clone drawing and worker swapchain ownership into one image-producing path and gives the future viewport worker a borrowed GPU output-SRV boundary. It is still a test-only synchronous worker whose device context is used exclusively during the run. Debug cold initialization of the offscreen renderer plus presenter costs about 1.74 s on D3D12 and 2.53 s on Vulkan; production must retain one worker, renderer, and offscreen target. JSON serialization still costs about 6.5 ms on the GUI fixture, and this probe does not cover worker queue coalescing, live edits, playback PhysicsSystem ownership, effects/overlays, concurrent shared-context users, or resize/shutdown lifecycle.
- **次に確認:** Move this passing path behind a persistent bounded worker interface, measure GUI submit/serialization time under rapid edits, and validate stale-frame rejection, shared-context single ownership, resize, teardown, and representative effect/overlay parity before connecting it to the active controller.

## 2026-10-19: Composition frame packet is not connected to the production controller yet
- **関連:** `Artifact/src/Widgets/Render/ArtifactCompositionHostFrameSnapshot.cppm`, `Artifact/src/Widgets/Render/ArtifactCompositionRenderController.cppm`, `tests/Artifact/PlaybackRangeServiceContractTest.cpp`
- **確認できた事実:** Source search finds `CompositionRenderFramePacketCache` only in its module and contract tests; `renderOneFrameImpl()` still reads the live controller Composition and emits `present()` synchronously. Packet construction previously consulted the singleton Playback Service to fill missing frame/upload data; this was removed so only GUI-captured snapshot fields cross that boundary. The focused PlaybackRangeServiceContractTest passes 6/6 after the change.
- **価値 / 懸念:** Worker-clone, mailbox, and packet contract tests do not yet validate a production controller handoff. Connecting the packet needs a clear owner for the live renderer/immediate context, mutable render caches, playback completion side effects, resize, and shutdown; passing metadata tests cannot stand in for that integration.
- **次に確認:** Define the production request/result owner boundary and identify which helpers can consume an owned scene packet without reading `previewPipeline_`, controller members, or QObject services before attempting to dispatch the frame body.

## 2026-10-19: UI test MCP code in AppMain crosses Qt module import boundaries
- **関連:** `Artifact/src/AppMain.cppm`, `Artifact/src/Test.cppm`, `Artifact/include/Widgets/ArtifactUndoHistoryWidget.ixx`
- **確認できた事実:** AppMain currently carries a 373-line test-only TCP/MCP server and additional Qt widget/network headers. After correcting a malformed `QJsonObject` initializer, the existing VS18 build reaches an IFC import failure at `Artifact.Widgets.UndoHistoryWidget` (`QMetaType::fromType<Qt::CheckState>`). A trial move into the already-large `Artifact.TestRunner` unit moved the failure to its `Artifact.Test.PreCompose` import (`QStringView::arg` specialization); that trial was reverted. `Artifact.TestRunner` is clean again, and the UI server remains in AppMain.
- **仮説・未検証:** Qt declarations added to a large module purview may be colliding with Qt specializations exposed through imported IFCs. A small dedicated module with a forward-declaration-only interface and implementation-local Qt includes may isolate the dependency set, but this has not been tested.
- **価値／懸念:** AppMain gained hundreds of lines for test-only control and the build error now blocks fresh render integration verification. Moving the code into another already-large module does not solve that boundary and can create a second IFC failure.
- **次に確認:** Trace the exact `QMetaType::fromType<Qt::CheckState>` trigger, then place the endpoint in a narrowly scoped test-control module if the generated-project path can be verified without CMake regeneration.
## 2026-10-19: ArtifactPr workbench polish should track module boundaries

- **関連:** `ArtifactPr/src/ArtifactPrMainWindow.cppm`、`ArtifactPr/src/ArtifactPrEditorEngine.cppm`、`ArtifactPr/src/TimelinePanel.cppm`、`tests/ArtifactPr/TimelinePanelStandaloneTest.cpp`。
- **確認できた事実:** ArtifactPr は独立した NLE モデルと共有 ArtifactCore を持つ。MainWindow と EditorEngine には依然広い責務が残る。今回 Timeline の clip / ruler / transition / panel UI を `ArtifactPr.TimelinePanel` に切り出し、MainWindow から独立して起動する widget contract test を追加した。
- **価値／懸念:** NLE 所有境界を保ちつつ、UI 変更単位と検証単位を一つ分離できた。独立テストはまだビルド・実行されておらず、残る MainWindow / EditorEngine の結合も未解消。構造改善による開発速度への効果は未検証。
- **次に確認:** 明示的に許可されたビルド・runtime 検証の後、Monitor / Inspector 群と EditorEngine の責務を依存関係から分けて段階的に切り出す。
- **追記 (2026-10-19):** この項目が参照する `tests/ArtifactPr/TimelinePanelStandaloneTest.cpp` は削除し、内容は `tests/ArtifactPr/ArtifactPrTimelinePanelTest.cpp` (gtest) へ移植した。下の「ArtifactPr module set could not have compiled」も参照。

## 2026-10-19: Composition Viewport grid alpha blending corrected

- **関連:** `Artifact/src/Render/ShaderManager.cppm` の Grid PSO、`tools/CompositionViewportGridTest/main.cpp`、`temp/composition-viewport-all-fixed.png`
- **変更・確認:** Grid PSO に既存 SolidRect と同じ straight-alpha blend state を設定した。D3D12 の独立テスト target を再ビルドし、修正後キャプチャでグリッドの透明度が反映され、合成画像でセーフエリア／ガイドがグリッドより前面に読めることを目視した。ビューポート全体に表示する範囲は維持した。
- **価値／懸念:** 透明度を使う Composition Viewport overlay がグリッドに隠されにくくなる。`AppMain.cppm` には実際の利用がない `Artifact.Widgets.UndoHistoryWidget` import があり、削除後に C1116 が解消して Artifact.exe の再リンクが完了した。`ArtifactUndoHistoryWidget.ixx` に QMetaType を追加する試行は C1116 を解消しなかったため戻した。アプリ本体の実ウィンドウ確認と Vulkan backend は未確認。
- **次に確認:** アプリ起動後に Composition Viewport 上で同じ表示を確認する。必要なら Vulkan の同じ独立テスト capture も行う。

## 2026-10-19: Liquid frame changes pass, but coffee appearance remains weak

- **関連:** `ArtifactCore/src/Physics/FluidSolver2D.cppm` の `buildSurfaceSnapshot()`、`Artifact/src/Layer/ArtifactAbstractLayerFractureRuntime.cppm`、`drawGradientTriangleLocal()`、`PhysicsLodBridgeContractTest.LiquidLayerRendersAndWritesCoffeeColoredPngCapture`。
- **確認できた事実:** D3D12 headless GPU capture test passes and writes frame 0/12 PNGs; visible pixels change between frames. Per-vertex color interpolation removes the obvious cell-shading seams. `RenderConfig::MainRTVFormat` is `TEX_FORMAT_RGBA8_UNORM_SRGB`, while layer QColor values cross `ColorBridge::toFloatColor()` as normalized sRGB values. Passing those values directly to liquid draw packets produced a tan capture (dominant surface RGB around 161,127,93 for QColor 102,61,31).
- **変更・確認:** Liquid fill, contour, spill and foam draw colors are converted with `FloatColor::toLinear()` at the render boundary. With contour enabled, the same D3D12 capture test passes and dominant coffee pixels are around 96,58,32. Test now checks mean dark coffee pixels remain close to the configured QColor, as well as frame variation and luminance span.
- **Core テスト:** 17件の Liquid テストを実行。初回に表面間引き比較テストが失敗した。LOD 側は `sqrt(stride)` で粒子半径を補正する設計だが、explicit reference 側は未補正だったため、参照の有効粒子径を揃え、幾何（triangles/contours）だけを比較するテストへ修正。foam は独立テストで確認し、再実行では17/17 pass。
- **追加の画面確認:** fixture を 320×320 内に中央配置し、frame0/12 の双方で Transform3D を設定。最初に流入/foam の setter を誤って隣接の property contract test に置いていたため、capture 画像は設定を反映していなかった。設定を capture fixture に移した後は中央の注ぎ流れが見え、液面上方の coffee pixel が783個。foamAmount 1.0 は大きなリング状の泡で画面が汚れ、既定相当の0.35では流れが読みやすく泡の縁取りが控えめ。foamAmount=0 と同じ frame12 を比較すると1494画素が変化し、差分は stream 周辺 (x=138..181, y=55..97) に限定された。GPU test は中央上部 stream pixel >100 と泡差分 pixel >50 を要求し、pass。
- **泡位置の分離（2026-10-20）:** foamBias を nominal fill surface より上でゼロにする試作は泡A/B差分0となり失敗。Core sample に `foamOffsetY` を追加し、密度場/triangle は元位置、foam exposure も元位置、泡描画位置のみ offset 後を使う。矩形 container の falling stream から surface 内へ泡を投影するとA/B差分1788画素、範囲 x=136..183, y=110..156 となり、前回の上方リングから液面接触域へ移った。Core test は泡位置だけ動き triangles が同一であることを検証、Liquid 18件と D3D12 GPU capture pass。
- **液面上縁の反射:** contour segment のうち上側かつ横向きの区間だけに細い液色/Crema混色の sheen を重ねた。コンテナの全周線はそのままに、frame12 GPU capture で液面の浅い波形が暖色の細い縁として読み取りやすくなった。D3D12 GPU capture test pass。通常アプリ表示とVulkanは未確認。
- **GPU backend parity:** 最新の泡・階調調整を含む headless Composition-view capture を D3D12 / Vulkan で再実行。両方で NVIDIA GeForce RTX 4070 Ti を使い、frame12 PNG SHA-256 は `5D352ED5E9BD4F53E4273EF65BD6174C7D0E967C4D12F173012AE7BCDC639FC6` で一致。GUI viewportの通常操作は未確認。
- **仮説・未検証:** The image still shows a broad, angular, nearly flat pool, with weak foam detail. Surface reconstruction/edge geometry and fixture framing limit appearance in addition to shading and foam intensity. App viewport behavior and color appearance at normal UI scale are unverified.
- **価値／懸念:** A passing color-pixel assertion and luminance spread do not guarantee a convincing liquid surface. Keep image review as a separate acceptance check from simulation determinism and pixel-change tests.
- **次に確認:** Refine the stream-side foam from projected points into a softer crema patch, improve the surface boundary/meniscus, and inspect the app viewport at normal display scale. Keep the current result labeled as stylized fluid shading.
- **2026-10-20 の追加確認:** 同じ frame12 で foam 有効/無効を見比べると、大きな明るいハローは陰影ではなく foam particle の画面占有が原因だった。泡の描画サイズを 0.68→0.42、不透明度係数を 0.72→0.48 に下げ、液体本体は白い specular を外して上下方向の色勾配に切り替えた。D3D12 capture test は通過し、ハローは縮小したが、通常アプリの画面サイズでの見え方と水らしい屈折表現は未検証。
- **速度方向に沿った泡表現（2026-10-20）:** `LiquidFoamPoint2D` にCore sampleの速度を保持し、アプリ側のfoam billboardをVelocityAlignedにして横ストレッチを加えた。GPU captureの泡有効/無効差分bboxで「落下流に対して横長」を確認する契約を追加。Core physics suite 30/30成功、D3D12/Vulkan capture両方成功、frame12 PNG SHA-256 は `4D5E80264E4DE6B57EBA2B8649248F24D5C8C07DAA57F29E3C5464B891621A10` で一致。見た目は接触点のクレマが改善したが、プールの平坦さは泡ではなく水面形状・照明表現にも依存する。
- **現状の仮説・未検証:** 水らしさを上げる次の主因は泡量だけではなく、液面境界の曲率/メニスカスと視点依存のハイライトまたは屈折近似。GPU capture上では未実装で、通常アプリviewportと実時間性能も未確認。次ループではまず描画パスを確認し、 bounded なGPU表現と画像契約から水面シェーディングを追加できるか調べる。
- **2026-10-20 密度シェーディング試作:** Core surface triangleへ閾値クリップ後の頂点密度を保持し、描画色に厚み係数を適用。縁の過度な明るさを減らすため輪郭線も細く・暗くした。試した法線方向照明は三角形ごとの法線変化が斑点状に出たため、D3D12 captureを目視して撤回。現行画像は以前より縁が控えめだが、依然として平坦で、屈折・反射・水面曲率を表現できていない。Core physics 31/31、Fluid/Liquid app contract 3/3 pass。D3D12/Vulkan frame12 SHA-256 `D0D34E9A8F73FDFFBCA2512FEEE7BBF47351C430C2A5BE796AE1900FAD8483BA` 一致。次の確認は、粒子由来の三角形パターンを露出させずに上面の広い反射を作る手段と通常viewportの見え方。
- **接触反射帯の試作（2026-10-20）:** 法線ハイライトを使わず、mesh vertex座標から y=.45・surface center を中心とした滑らかな low-amplitude cream bandを頂点色へ重ねた。foamAmount=0 の同一フレームでも接触位置の中心画素が横方向の液面画素より明るい契約を追加し、D3D12/Vulkan captureで合格。両frame12 PNG SHA-256 `4200A72CF5B4430B540992D9DCCA77FD7D3E9A7D731B87B35FB5E2BD784CE050` で一致。画像は接触部のクレマ風ハイライトを識別できるようになったが、座標依存のstylized shadingであり、実時間viewport、視点/背景に応じた反射・屈折、液面曲率は未検証。
- **充填率追従の描画契約（2026-10-20）:** `PhysicsLodBridgeContractTest` で fill amount を0.62から0.35に変更し、フレーム12の泡なしGPU readbackから pool edge の位置を比較する assertion を追加。`LiquidLayerRendersAndWritesCoffeeColoredPngCapture` はD3D12/Vulkan双方で成功し、低い充填率のエッジが高い充填率より20px超下に現れた。通常充填frame12は両backend SHA-256 `D7C3453DC39C002EAB6…`、低充填frame12は `A34E4F950D99F103138F372BD8373B1B0BB6E004A9858FEDF2B5BD2B3598E21D` で一致。画像目視でもsurface上端と流入の接触位置が低くなる。反射中心そのものの画素位置と通常アプリviewport、実時間性能は未計測。
- **充填面アンカーによる反射追従（2026-10-20）:** 画像解析でfill .35時のpool edgeが全surface bounds基準の相対高さ .25.. .58 から外れ、反射帯が設定面からずれる懸念を特定。矩形容器では `1-liquidFillAmount` を `baseTransform` で描画座標へ写し、上下方向にboundedな許容幅を設けてその近傍の水平輪郭から反射中心を計算するよう変更した。泡なし低充填キャプチャで液面直下の中央輝度が側方より24階調高いことを確認し、契約テストは+8以上を要求。D3D12/Vulkanの通常・no-foam・lower-fill各画像はSHA-256一致。コンター候補が許容幅に無いケースは従来のbounds中心へfallbackする。回転レイヤー、非矩形容器、通常アプリviewport、実時間性能は未検証。
- **回転レイヤーの輪郭シェーディング（2026-10-20）:** 25度回転したLiquid captureを追加し、画面のx/yではなくbaseTransform由来のlayer X/Y単位軸へ点・線分を投影して、水平輪郭の判定、reflection center、幅・高さ、surface color gradientを求めるよう変更。回転lower-fill captureは液体色画素と明暗差の条件を満たし、D3D12/Vulkan SHA-256 `9AA5018F58128BD2F318439241640947763FEA5A81950661C972021E9CCF7CA1` で一致。通常・no-foam・lower-fill画像も両backendでそれぞれ一致。テストは回転時の描画と色調差を確認するが、回転境界の角度・物理的な液面傾斜は定量化していない。通常アプリviewportと実時間性能は未確認。
- **Liquid surface clipの固定作業領域化（2026-10-20）:** `LiquidSolver2D::buildSurfaceSnapshot()` が1セル4三角形をdensity thresholdでclipするたび、input/output/intersectionsの一時`std::vector`を3つ生成しreserveしていた。triangle clipの最大出力4頂点・交点2個を固定 `std::array` に置換し、この内側ループから最大3回/三角形のheap allocationを除去。Core PhysicsDeterminism 31/31 pass。Artifact GPU captureもD3D12/Vulkanでpassし、通常・foam off・lower fill・25度rotatedの4画像は各backend一致、さらにCore変更前の保存済み画像とも一致。実時間profileは未取得。surface construction全体のdensity `std::map`、cell `std::set`、spill neighbor map/vectorは依然動的確保を行うため、snapshot全体をallocation-freeとは扱わない。
- **関連する契約修正:** 登録 FluidSolver2D の LOD 反復数が明示値100をそのまま通し、期待上限12を超えていたため PhysicsSystem の LOD適用箇所で1..12に clamp。`ReducedAndMinimalLODApplyFluidIterationBudgetsToRegisteredSolver` と liquid GPU capture は再ビルド後に pass。
- **2026-10-20 のアプリ反映:** `cmake --build build --config Debug --target Artifact` が成功し、`build/bin/Debug/Artifact.exe` のリンク・配布ステップを完了。Node kernel asset のパスエラーが続き、GUI viewportのcaptureはまだ得られていない。
- **Core hot-path 修正（2026-10-20）:** `LiquidSolver2D::update()` のフレーム内ローカル位置 `std::vector` を Solver 所有の `NamedVector<LiquidContainerPoint2D>` 作業領域へ移した。粒子数が同じフレームでは `resize()` を呼ばず、粒子数の増加時だけ既存NamedVector容量に応じて拡張する。最終サブステップの位置差分から速度が再構成される契約テストを追加し、`ArtifactCorePhysicsDeterminismTest` 全29件（Liquid19件含む）pass。Artifact.exe と GPU capture/LOD contract target も再ビルド。D3D12 capture contract pass。
- **残る確認候補（未計測）:** `forEachLiquidNeighborPair()` は呼び出しごとに `std::map<Cell, std::vector<size_t>>`、粒子cell/valid配列、候補配列を作り、距離constraintの各反復と粘性適用で再構築する。ここは今回変更しておらず、profile/allocation count未取得。なお粒子数の増加時にNamedVectorが容量拡張する経路も、通常再生で発生頻度を計測していない。
- **次に確認:** HOT_PATH_RULESに沿って上限粒子数・代表粒子数で `LiquidSolver2D::update()` のCPU時間とallocation回数を計測し、spatial-neighbor workspaceの再利用を設計する。std::mapを単純に置き換えず、決定論的なpair visitation順、複数solver同時所有、粒子数増加時のbounded capacityを先に契約化する。
- **液面クレマの追加（2026-10-20）:** 静止poolでは速度・衝突起点のfoamPointsがほぼなくなるため、矩形容器の上側・横向きcontour segmentからクレマ粒子を追加。最初の大粒子版は表面に広いハローを作り、foamが画面を汚すと判明した。泡を間引き、サイズを再調整して細かい粒列にし、GPU capture testに静止液面帯の差分assertを追加した。D3D12の個別capture testは直接実行でpassし、`build/fluid-capture-d3d12-crema-refined2/liquid-coffee-frame-12.png` で液面上縁に小さな点状の泡を確認。CTest全体は DLL 検索パス不足で起動しなかったが、原因はテストexeの隣にない `build/bin/Debug` 内Artifact DLLだった。対象CTestに同ディレクトリをPATH先頭へ加える `ENVIRONMENT_MODIFICATION` を設定し、再実行でDLL起動問題は解消した。12件中8件pass、残る4件は既存のPhysics LOD simulation / SoftBodyGrid LOD契約失敗で、液体描画capture自体はpass。通常viewport・Vulkan parityは未確認。
- **クレマ粒の均一さ（2026-10-20）:** 前回のcaptureでは液面に等間隔の小点が並び、泡というより破線に見えた。区間座標から整数ハッシュを作り、採用密度・粒径・不透明度に決定論的なばらつきを入れた。D3D12 GPU capture testはpassし、captureで規則的な点列は崩れたが、一部が塊状に集まって見える。これは見た目調整であり、粒子状態やCore solverには影響しない。クレマの説得力はまだ不十分で、液面形状も概ね平坦。通常viewportでの表示スケール、Vulkan parity、粒配置の追加調整を続ける。
- **接触反射とアプリリンク（2026-10-20）:** 液面中心に限った反射寄与を0.16から0.21へ上げ、GPU capture testの明暗差条件を3から6へ強めた。D3D12 capture testはpassし、上部の反射帯が少し広がることを画像で確認。`ArtifactCorePhysicsDeterminismTest --gtest_filter='Liquid*.*'` は21/21件pass。`Artifact` targetもリンク成功し、runtime deploymentでQt/vcpkg等のDLLが `build/bin/Debug` に配置された。`windeployqt` は vcpkg の `bin/Qt6Widgetsd.dll` とtranslations catalogs.jsonがないという警告を出したが、最終配置先 `build/bin/Debug/Qt6Widgetsd.dll` は存在する。実アプリプロセスも8秒後にResponding=Trueで稼働を確認したが、画面取得はUI automation helperの初期化エラーで未確認。
- **液面反射筋（2026-10-20）:** 頂点色補間に消される細い筋は前後captureで23画素しか変わらなかったため、周期波のピーク幅を0.07→0.30、上部マスクを0.30→0.45、強度を0.12→0.20へ広げた。D3D12 capture testはpassし、frame12の比較では1569画素が変化 (最大チャンネル差40)。三角形頂点ごとのfloor/arithmetic演算のみで追加確保はないが、ホットパスCPU費用は未計測。最新 `ArtifactPhysicsLodBridgeContractTest` のtarget buildは成功した一方、`Artifact.exe` は前のturnに起動したインスタンスがファイルをロックしてLNK1168。MainWindowTitleは `Artifact 0.9.0 | dev | local` でResponding=True、CloseMainWindowは終了要求を受け付けなかった。したがって最新反射筋を含むexeのリンク／画面確認は未完了。ユーザーが画面を使用中の可能性があるため強制終了しない。
- **水色液体の描画契約（2026-10-20）:** コーヒーGPU capture契約に水色のframe12 captureを追加し、water pixel hue、mean channel tolerance、液面高さが液色変更で変化しないことを検証する。初回のwater captureは前ケースの低いfill 0.35を残したため形状比較に不適切と判明。fillを0.62に戻してから撮影するよう修正後、D3D12 GPU capture testはpass。mean blueは入力色より暗くなる (shading込み、許容差45)、画面上では水色の液面と反射筋を確認した。アプリexeは引き続き起動中プロセスにより最新コードへの再リンクが止まっている。
- **液面サンプル workspace 再利用（2026-10-20）:** `ArtifactAbstractLayerFractureRuntime.cppm` は各描画呼び出しで `NamedVector<LiquidSurfaceSample2D>` を構築・reserveしていた。これを `LayerFluidRuntimeState` の所有workspaceへ移し、フレームごとにclear、容量が不足した時だけreserveするよう変更。最大render particle countに応じたcold growth後はframeごとのサンプル配列確保を避ける。Artifact GPU capture contract targetは再ビルドされ、水色/コーヒー描画テストpass。具体的なallocation回数とGPU/CPU frame timeは未計測。Artifact.exeは稼働中のためこの更新を含む最終リンクは依然未確認。
- **Core surface snapshotの出力再利用（2026-10-20）:** `LiquidSolver2D::buildSurfaceSnapshot()` は三つの出力NamedVectorを新規作成していた。既存value-return APIは維持し、呼び出し側が `LiquidSurfaceSnapshot2D&` を渡す `buildSurfaceSnapshotInto()` を追加。Appの `LayerFluidRuntimeState` が保持するsnapshotへ直接出力し、triangle/contour/foam配列をclearして容量を再利用する。Core testはvalue-return版との全geometry比較と2回目呼び出し時のcapacity不変を確認し、Liquid 22/22件pass。D3D12コーヒー/水capture GPU testもpass。Core内のvalidSamples、density map、surface-cell setはまだ関数内一時領域で再確保されるため、allocation profilingとそのworkspace化が残る。
- **実行確認（2026-10-20）:** 起動中の検証用Artifact.exeがDLLをロックしていたため、こちらが起動した同プロセス (PID 35100) を終了し、ArtifactPhysicsLodBridgeContractTest と `Artifact` target を再ビルドした。最新 `build/bin/Debug/Artifact.exe` のリンク・runtime deploymentが成功し、新プロセスPID 36196は8秒後にResponding=True、window title `Artifact 0.9.0 | dev | local`。`windeployqt` はtranslations catalogとその探索先のQt DLL不足警告を出すが、アプリプロセスは起動・維持できた。通常viewport内のfluid scene表示はまだ確認していない。
- **Debug Qt deployment path（2026-10-20）:** `RunWindeployqt.cmake` は Debug 実行でも標準 `qtpaths.exe` を使っていた。この exe は `QT_INSTALL_BINS` を `vcpkg_installed/x64-windows/bin` と返す一方、Debug Qt6 DLL は `debug/bin` にあるため、windeployqt が `Qt6Widgetsd.dll` を誤った場所で探して warning / failure code を出していた。Debug 用 `qtpaths.debug.bat`（`qt.debug.conf` を読む）を `--qtpaths` に渡すよう修正。スクリプトを Artifact.exe に対して直接実行すると warning なく終了し、アプリプロセスも維持された。実際の不足 DLL ダイアログの文言は取得できていないため、この原因との完全な同一性は未確認。

## 2026-10-19: ArtifactPr module set could not have compiled

- **関連:** `ArtifactPr/src/{AppTheme,AudioPreviewMixer,GpuProgramMonitor,MediaFrameDecoder,MediaThumbnailer,SequenceCompositor,SequenceExporter}.cppm`、`ArtifactPr/src/VideoSurface.cpp`、`ArtifactPr/src/{MarkerEditDialog,PrColorPickerDialog,PrShortcut,PrStatusNotifier,TimecodeOverlayWidget}.cppm`、`ArtifactPr/CMakeLists.txt`、`tests/ArtifactPr/`、ルート `CMakeLists.txt:269`。
- **確認できた事実:** 8 個の `.cppm` が `export module ArtifactPr.X;` を宣言し、同名の `include/X.ixx` も同じモジュールを `export module` していた。二重の module interface unit になり、どのビルドでも成立しない。加えて同じ 8 個と他 5 個の `.cppm` が `import ArtifactPr.X;` で自分のモジュールを自己 import していた (AGENTS.md が dyndep 破壊要因として禁止している形)。`ArtifactCore/src` の実装ユニットは `module X;` + 自己 import なしが正準であることを grep で確認した。この状態は、`ARTIFACT_BUILD_PR` が既定 OFF であること、ルート `CMakeLists.txt:265` の「ArtifactPr disabled in build」、`insights/2026/ISO-W39.md:103` の「configure が通るか未確認」と整合する。つまり ArtifactPr のモジュール集合は一度も configure 検証されていない。
- **変更:** 8 個を `module ArtifactPr.X;` へ変え、13 個の自己 import を削除した。`ArtifactPr` は実行ファイル 2 本から `ArtifactPrApp` (STATIC) + 薄い `ArtifactPr` / `ArtifactPrCLI` へ分割し、module interface を `PUBLIC FILE_SET CXX_MODULES` で公開した。`tests/ArtifactPr/` に gtest 一式 (`ArtifactPrContractTest` / `ArtifactPrEditorEngineTest` / `ArtifactPrTimelinePanelTest`) を新設し、`ARTIFACT_BUILD_PR_STANDALONE_TESTS` を `ARTIFACT_BUILD_PR_TESTS` に置き換えた。旧 standalone test の実行ファイルは削除し、内容は gtest へ移植した。
- **価値／懸念:** テスト 1 本あたりの費用が「モジュール集合の再コンパイル」から「.cpp 1 枚 + CMake 2 行」になった。ただし今回の suite は **未ビルド・未実行** であり、静的確認だけである。特に、`ArtifactCore` は一部コンシューマに `/reference;NLE.Core=...ifc` を明示している (`ArtifactCore/CMakeLists.txt:2914`) ため、`FILE_SET CXX_MODULES` の IFC 伝播だけで ArtifactPr のテスト target が import を解決できるかは未確認。
- **確認・修正（2026-10-25）:** `MediaThumbnailer` は `QCoreApplication::processEvents` と `QEventLoop::AllEvents` を使う一方、対応する直接 include がなかったため、`.cppm` → `.cpp` 移行時に global module fragment へ `<QCoreApplication>` / `<QEventLoop>` を追加した。Clang compile による検証は未実施。
- **仮説・未検証:** `VideoSurface.ixx` は `Q_OBJECT` / `Q_SIGNALS` を宣言する一方、リポジトリ内の CMake 検索では `AUTOMOC` 設定や有効な `qt6_wrap_cpp` が見つからず、Qt meta-object 実装が生成されない可能性がある。実際のリンク状態は configure/build 未実施のため未確認。
- **次に確認:** build を許可された後、`ArtifactPrApp` で `VideoSurface.ixx` に対する Qt MOC 出力が生成・リンクされるか確認する。あわせて IFC 伝播、EditorEngine の編集操作 (`moveClip` / `splitClipAtPlayhead` / `rippleDeleteClipAt` と undo/redo)、`ArtifactPrCLI` の別プロセス契約を確認する。

\n

## 2026-10-21: CMake 4.2 clang-cl module scan は GNU frontend rule を流用できない

- **関連:** `CMakeLists.txt` clang-cl scanner setup、CMake 4.2.2 `Compiler/Clang-CXX.cmake`、`ArtifactWidgets/CMakeLists.txt` OpenMP discovery
- **確認できた事実:** clang-cl root configure で OpenMP compiler flags を `-Xclang -fopenmp` として与え、MSVC `libomp.lib` および spec date を指定すると Qt / OpenMP の configure-time checks は越えた。generate は clang-cl 用 CXX scan rule 不在で停止した。CMake 4.2.2 Clang P1689 rule は GNU frontend 分岐内でのみ定義される。
- **価値／懸念:** GNU frontend の P1689 scanner command を clang-cl にそのまま渡すのは不適合で、dependency scanning、module map、BMI 出力の MSVC driver syntax を分けて実装する必要がある。より新しい CMake が clang-cl 対応を追加していても、最小 CMake 3.30 を維持する場合の backport / version gate を決める必要がある。
- **次に確認:** CMake clang-cl module scan 対応版の最小要件と、P1689 scanner invocation が `clang-cl` の MSVC style arguments を扱うか一次資料・隔離 smoke で照合する。その後で root scan override と OpenMP / Qt try_compile の適用境界を最小化する。


## 2026-10-22: clang-cl で移行済み .cpp の実 target module scan を確認

- **関連:** ArtifactCore/CMakeLists.txt、ArtifactCore/src/EnvironmentVariable/EnvironmentVariable.cpp、CMake 4.4.4 / Ninja / clang-cl
- **確認できた事実:** MSVC frontend 用 /reference と OBJECT_DEPENDS を Clang module map に適用すると競合するため、Clang lane ではこれらの手動依存を除き、移行済み .cpp にのみ P1689 scan を有効化した。GNU frontend の既存 .cppm scan は維持。ArtifactCoreEnvironment の scan、dyndep、compile、static archive が成功した。ArtifactCore/CMakeLists.txt は tracked child repository 側の未コミット変更であり、親から submodule commit/push はしていない。

- **価値:** Qt 隔離 smoke のみでなく、実際の split target で clang-cl の CMake module map 経路を確認できた。
- **次に確認すること:** 他 split target と Clang interface compilation の依存解決、OpenUSD を含む root 全体 build、OpenMP 検出を段階的に検証する。


## 2026-10-22: ToggleGlyphButton module implementation を `.cpp` に移行

- **関連:** `ArtifactWidgets/src/Common/ToggleGlyphButton.cpp`、`ArtifactWidgets/include/Common/ToggleGlyphButton.ixx`、CMake 4.4.4 / Ninja、Qt6、clang-cl / upstream clang++ / sccache
- **確認できた事実:** source body を LF 化して `.cpp` に移し、Widget implementation source list に明示登録した。両 Clang frontend で P1689 scan、module interface / implementation compile、Qt6 DLL link が成功。upstream clang++ + sccache 0.18 では初回2 compile miss、clean build 再実行時に2 hit、bypass なし。
- **価値:** QWidget 系 implementation を `.cpp` に移す経路が両 Clang frontend で継続して使える。sccache も module interface を含むこの pilot で再現可能な hit を確認した。
- **次に確認すること:** Widget 全体 target の依存関係と MSVC の最終 DLL link を確認し、同じ manifest 手順で次の候補を段階移行する。


## 2026-10-22: ContentViewer implementation を両 Clang frontend で `.cpp` 検証

- **関連:** `ArtifactWidgets/src/Viewer/ContentViewer.cpp`、`ArtifactWidgets/include/Viewer/ContentViewer.ixx`、CMake 4.4.4 / Ninja、Qt6、clang-cl / upstream clang++
- **確認できた事実:** `ContentViewer.cppm` の本文を変更せず、LF 化して `.cpp` へ移し、CMake の明示 implementation source list に追加した。clang-cl と upstream clang++ + Windows MSVC ABI の両方で P1689 scan、interface / implementation compile、Qt6 DLL link が成功した。
- **価値:** Common widget と異なる Viewer 領域でも、現在の `.cpp` source 登録・module scan 経路が成立することを確認できた。
- **次に確認すること:** ArtifactWidgets 全体 DLL の MSVC / Clang link と、より広い依存を含む implementation の段階移行を確認する。


## 2026-10-22: MSVC module implementation の sccache 判定

- **関連:** root `CMakeLists.txt` の sccache launcher、MSVC 19.51、sccache 0.18、`ContentViewer.cpp` / `.ixx`
- **確認できた事実:** MSVC module interface compile は `-interface`、implementation compile は `/reference` を含み、sccache 0.18 は両方を非 cacheable として bypass した。同 toolchain の通常非 module `.cpp` は単独 compile で miss→hit。root `ArtifactCoreEnvironment` は launcher 下で成功したが、`ArtifactWidgets` 依存の OpenUSD `tf` target は parallel compile 時に PDB C1041/C1090 で停止。同じ source は sccache 単独実行と直接 cl.exe で成功し、MSVC opt-in launcher の並列相互作用が疑われる。
- **価値／懸念:** 普通の `.cpp` は cacheable だが、module source は bypass される。PDB を使う並列 root build の失敗を解決できるまで、MSVC-style frontend の CMake launcher は無効に保つ。
- **追加確認:** MSVC option ON の root configure と `ArtifactCoreEnvironment` build を実行。6 module compile command (interface 4 / IFC-reference implementation 2) は sccache 0.18 が全て bypass し、static archive は成功した。通常 `.cpp` の miss→hit は同環境の直接 compile で確認済み。
- **次に確認すること:** ArtifactWidgets 全体 link と、実アプリ build で通常 `.cpp` の cache hit が出るか確認する。


## 2026-10-22: MSVC Widgets 全体 link と TimeCodeLabel pilot

- **関連:** `ArtifactWidgets/src/Time/TimeCodeLabel.cpp`、`ArtifactWidgets/CMakeLists.txt`、MSVC 19.51 / CMake 4.4.4 / Ninja
- **確認できた事実:** launcher 無効の dedicated MSVC build tree で `ArtifactWidgets` target が576 actionを完了し、`ArtifactWidgets.dll` を link した。その後 `TimeCodeLabel.cppm` を本文を保って `.cpp` に移し、明示 implementation source list に登録した。MSVC 本体 target はCMake再生成後に436 actionを実行し、新 source をscan・compileしてDLLを再linkした。独立 Qt6 pilot では upstream clang++ + MSVC ABI と clang-cl の双方で同じ source / interface をscan・compile・linkした。
- **境界:** 独立 Clang pilot は未使用 import `Frame.Position` をテスト用BMI stubで補った。MSVC 本体 target は実依存 graph を通る。Clang pilot の個別 sccache hit/miss 数は未採取。既存 interface が宣言する `setFrame` / `setDisplayMode` は現行 implementation に定義がなく、`impl_` も初期化されていない。拡張子移行だけの今回の変更ではruntime動作を確認していない。
- **価値:** MSVC では compiler cache launcher を外すと実際の Widgets DLL link が通り、次の小さい implementation unit でも両 Clang frontend の `.cpp` module scan とlinkが成立した。
- **次に確認すること:** TimeCodeLabel の既存runtime/API欠落を別途扱うか判断し、より機能が揃った小さな module pair を選びながら移行と compiler/sccache 測定を続ける。


## 2026-10-22: ColorViewLabel migration と Clang sccache 実測

- **関連:** `ArtifactWidgets/src/Color/ColorViewLabel.cpp` / `include/Color/ColorViewLabel.ixx`、CMake 4.4.4、upstream clang++ / clang-cl、sccache 0.18.0
- **確認できた事実:** source本文を保持して `.cppm` から `.cpp` へ移し、Widget implementation list に明示登録した。MSVC本体 `ArtifactWidgets` target は436 action後に当該sourceをscan・compileし、DLLをlinkした。両Clang frontendの隔離Qt6 targetでもP1689 scan・compile・linkが成功した。`Color.Float` は最小テストBMI stubで補った。
- **sccache 実測:** upstream clang++ は2 clean buildで6 requests / 3 misses / 3 hits / 0 bypass / 0 failure。clang-cl は6 requestsのうちcacheable compileが1 miss→1 hit、`multiple input files` bypassが4、0 failure。
- **価値:** widget implementation の拡張子変更を3 compiler laneで成立させ、本家 clang++ と clang-cl の実 source cache behaviorを別々に計測できた。
- **次に確認すること:** Clang root targetでCore moduleとQtの実依存を結合した際のscan / link、MSVC parallel compiler cacheのPDB競合、さらに低fan-inな implementation候補を継続確認する。


## 2026-10-22: CollapsibleSection implementation unit を3 compiler laneで検証

- **関連:** `ArtifactWidgets/src/Common/CollapsibleSection.cpp` / `include/Common/CollapsibleSection.ixx`、MSVC 19.51、upstream clang++ + MSVC ABI、clang-cl、sccache 0.18.0
- **確認できた事実:** `CollapsibleSection.cppm` を本文保持で `.cpp` に移し、Widgets implementation list に登録した。MSVC本体 `ArtifactWidgets` target、両Clang frontendの独立Qt6 targetでscan・compile・DLL linkに成功した。Clang pilotはCore module stubなしで成立した。
- **sccache 実測:** upstream clang++ は2 clean buildで4 requests / 2 misses / 2 hits / 0 bypass / 0 failure。clang-cl は4 requests中1 miss→1 hit、module interface `multiple input files` bypass 2件、0 failure。
- **価値:** Widgets内の独立module implementationを3 compiler laneで確認し、source module graphを増やさず拡張子移行とcache再利用を再現した。
- **次に確認すること:** ArtifactCoreの低fan-in implementation候補をmanifest/reference-map依存まで確認して選び、Clang root target側の実依存とsccacheを拡大検証する。
## 2026-10-22: OpenUSD arch の upstream clang++ / MSVC ABI 判定

- **関連:** root `CMakeLists.txt`、`libs/openusd/pxr/base/arch/defines.h` / `attributes.h` / `demangle.cpp` / `errno.cpp`
- **確認できた事実:** Windows clang++ が MSVC ABI target を使っても OpenUSD `defines.h` は `__clang__` により Clang/GNU compiler kind を選ぶ。`arch` target で `cxxabi.h`、`strerror_r`、Windows constructor-entry declarations の不足がコンパイルエラーとして現れる。親 CMake の source property は child directory の OpenUSD target に伝播せず、compiler-kind macro 上書きも `defines.h` が再定義する。試験 workaround は取り除き、OpenUSD submodule は変更していない。
- **推測・未検証:** 上流 OpenUSD が clang-cl/MSVC ABI lane を正式に扱う場合、compiler detection または arch implementation の条件修正が必要になる可能性が高い。upstream の現在版に修正があるかは未検証。
- **価値または懸念:** isolated Qt module pilots の成功と root OpenUSD 統合の失敗を区別できる。広域 macro spoofing は STL ヘッダや別の OpenUSD source を壊すため、親から全ターゲットへ適用する workaround は避けるべき。
- **次に確認すること:** OpenUSD の更新候補を精査する際、Windows Clang MSVC ABI の `arch` 対応を upstream で確認する。submodule を変える場合はユーザーの明示指示を得る。
## 2026-10-22: Analyze.OpticalFlow の末端 implementation 移行

- **関連:** `ArtifactCore/src/Analyze/OpenCV/OptifalFlowResult.cppm`、`ArtifactCore/include/Analyze/OpenCV/OptifalFlowResult.ixx`、Analyze source manifest / BMI reference map
- **確認できた事実:** implementation unit は module 宣言と空の実装コメントだけを持ち、実機能は interface の inline 定義にある。本文を保持した `.cpp` 化後、MSVC の専用 target compile、upstream clang++ MSVC ABI、clang-cl の各 compile が成功し、Clang 両 lane は OpenCV 付き isolated DLL link まで成功した。
- **推測・未検証:** interface に inline 実装を多く置く他 module も移行候補になり得るが、各 interface の依存と split target ownership は個別確認が必要。
- **価値または懸念:** source manifest、module reference map、専用 split target を一緒に更新する移行例となった。OpenUSD root blocker は独立して残るため、pilot 成功を full Clang build 成功とは扱わない。
- **次に確認すること:** 次候補では manifest 所有 target、MSVC reference map、importer object dependency、sccache cacheability を先に照合する。
## 2026-10-22: Utils.Tag module implementation を `.cpp` に移行

- **関連:** `ArtifactCore/src/Utils/Tag.cppm` / `.cpp`、`include/Utils/Tag.ixx`、ArtifactCore source manifest と module reference map
- **確認できた事実:** Tag implementation は `Utils.String.UniString` を import し、interface は `UniString` と `StringLike` を import する。MSVC root `ArtifactCore` target で Tag `.cpp` object と archive が生成された。upstream clang++ MSVC ABI と clang-cl の isolated pilots でも P1689 scan / compile / DLL link が成功した。
- **推測・未検証:** 依存 BMI を stub に置き換えた pilot は Tag の Clang 実 dependency graph を証明しない。sccache の cacheability も未計測。
- **価値または懸念:** Core module implementation と reference map の migration をもう一件通したが、Clang root target は OpenUSD blocker により未完了のまま。
- **次に確認すること:** 次の移行は module reference 数だけでなく MSVC object dependency、importing consumers、split target ownership を確認してから選定する。


## 2026-10-22: ArtifactCore 単一 module object の検証範囲が target order dependency に広がる

- **関連:** `ArtifactCore/CMakeLists.txt`、`cmake/ArtifactCoreSources.cmake`、`cmake/ArtifactCoreModuleReferences.cmake`、`src/Utils/NameGenerator.cpp`
- **確認できた事実:** `NameGenerator.cpp` の単一 object を Ninja に直接指定しても、`cmake_object_order_depends_target_ArtifactCore` を通じて OpenUSD / DiligentEngine の多数の依存 build が order-only dependency として選択された。最初の `ArtifactCore` target は5,054 actionを提示し、object 直接指定でも依存更新後に752 actionを実行した。SDK `fxc.exe` を build shell の PATH に追加した後、必要 module interface の scan / compile と `NameGenerator.cpp.obj` の compile は成功した。
- **推測・未検証:** 低 fan-in module の検証を頻繁に行う場合、実装 object ごとの短い検証 lane または provider subset を持つ split target が build feedback を短縮できる可能性がある。target ownership や link / ABI 条件への影響は未調査。
- **価値または懸念:** module 1件の変更でも第三者依存の再構築が検証時間と環境依存ツール（例: `fxc.exe` PATH）を持ち込む。今回の object compile 成功を全 ArtifactCore archive の link 成功とは扱わない。
- **次に確認すること:** Core split target の既存構造を保ったまま、selected implementation と importer / provider BMI だけを検証できる正式 target が作れるかを調べる。必要なら target-order dependency の発生元を生成 Ninja graph と CMake target property で追跡する。


## 2026-10-22: Qt `QStringLiteral` と Clang module interface の TU-local warning

- **関連:** `ArtifactCore/include/Utils/Path.ixx:43`、Qt `qstring.h` の `QStringLiteral` macro、Windows upstream clang++ / clang-cl module build
- **確認できた事実:** `Path.ixx` を実 `Utils.String.Like` dependency と Qt6 Core 付きで isolated module build した際、upstream clang++ と clang-cl の両方が `QStringLiteral(":/icons")` から `qMakeStringPrivate` の TU-local entity exposure warning を出した。`Path.cppm` → `.cpp` の拡張子移行前後で interface 内容は変えていない。compile と DLL link は成功した。
- **推測・未検証:** C++ modules で Qt inline API が内部 TU-local helper を露出する警告は、実行時不具合ではなく module linkage / compiler diagnostic 上の注意である可能性がある。実 consumer での ABI・ODR 影響や Qt 側の既知対応は未確認。
- **価値または懸念:** Clang の Qt module pilot で移行以前から存在する warning を検出した。現時点で警告抑制や Qt interface 改変は行わず、root build の warning 整理と混同しない。
- **次に確認すること:** Qt module support の公式説明・該当 helper の linkage 属性を調べ、実際の consumer module compile が可能になった段階で再現性と影響を確認する。


## 2026-10-22: Module partition implementation は primary-module migration と別扱いが必要

- **関連:** `ArtifactCore/src/File/FileInfo.cpp`、`ArtifactCore/include/File/FileInfo.ixx`、`ArtifactCore/CMakeLists.txt`、CMake 4.4.4 / upstream clang++
- **確認できた事実:** 現在のimplementationは `module File:InfoImpl; import :Info;`、interfaceは `export module File:Info;` を宣言する。root CMake はこのimplementationのmodule scanを無効化し、File targetのpartition IFC / object orderingを明示している。過去のisolated `.cpp` pilotでは、partition sourceをP1689 scan対象に含めるとCMake Ninja dyndepがimplementation objectをfile set外providerと判定した。
- **関連する別経路:** `ArtifactCore/src/Image/OpenEXR.cpp` は `module Image.OpenEXR;` のprimary implementationで、同じ初期scanner-off設定を使うが、Clang用の後段分類ではP1689 scanへ戻る。FileInfoと同一扱いにしない。
- **推測・未検証:** この二重ユニットを standard-conforming に扱うには、同じ partition 名を再提供する形を解消し、partition interface を primary module implementation unit から importする構成などへ整理する必要がある可能性がある。正しいMSVC / Clang共通形は未検証。
- **価値または懸念:** `.cppm` から `.cpp` への移行後も、module partition sourceは通常のimplementation scanに載せられない場合があり、Clang固有の明示PCM参照と依存順序が必要になる。
- **更新（2026-10-25）:** ArtifactCoreの最終Clang分類に残っていた除外パスは `FileInfo.cppm` で、現行 `FileInfo.cpp` に一致していなかった。CMake例外を `.cpp` に更新し、GNU/clang-cl共通でP1689 scanを無効のまま明示 `/reference` を `-fmodule-file` PCM参照へ変換する処理を追加した。CMake configure / compile は未実行。
- **次に確認すること:** Clang configure後にFileInfoのcompile commandがpartition interface PCMを参照し、重複dyndep ruleを生成しないことを確認する。今回は実行禁止のため未検証。


## 2026-10-22: `export namespace` 内の二重 `export` が Clang module compile を阻止

- **関連:** `ArtifactCore/include/Animation/AnimationDynamics.ixx`、`AnimationDynamics` module implementation migration
- **確認できた事実:** `export namespace ArtifactCore { ... }` 内の関数宣言2件に内側の `export` があり、Clang 22.1.3 は `export declaration appears within another export declaration` で interface module compile を拒否した。内側の指定だけを削除した後、upstream clang++ MSVC ABI / clang-cl の双方で実 interface と実 dependency modules の compile、implementation の compile、DLL link が成功した。
- **推測・未検証:** 同じ記述が他の interfaces にも残っている場合、Clang root build の次の blockers となり得る。全 repository の同型記述の頻度と MSVC compile への影響は未調査。
- **価値または懸念:** namespace export block の中で宣言を維持すれば内側の `export` は不要で、API export visibility を保ったまま Clang parser error を除ける実例を得た。
- **追加確認（2026-10-24）:** 同じnested exportが `Artifact/include/Widgets/Render/TransformGizmo.ixx` と `Artifact/include/Core/Guides/ArtifactSmartGuidesManager.ixx` にもあった。両export namespace内のclassから内側の `export` だけを除き、各interface compileに成功。SmartGuides implementation compileも成功した。TransformGizmo implementationのfresh scanは成功したが、object compileはAST約2.14GB（診断上99%）でfrontend crashし、実装側は未確認。`TransformGizmo` API exportが広がった・狭まったとの兆候はinterfaceとSmartGuides compileでは見られないが、全consumer linkは未確認。
- **次に確認すること:** ArtifactCore / Artifact / ArtifactWidgets interfaces を狭い検索で監査し、同じ構文があれば個別に宣言の所属と export visibility を確認する。

## 2026-10-22: Clang named-module BMI graph source-location 上限

- **関連:** `Artifact/include/Layer/ArtifactAbstractLayer.ixx`、`Artifact/include/Composition/ArtifactAbstractComposition.ixx`、Clang 22.1.3 / Windows upstream clang++。
- **確認できた事実:** Artifact の実 target で Preview / Project / LayerFactory implementation compile が `ran out of source locations` で失敗。Clang 22.1.3 では Preview 診断が約2.15 GB・99%、modmap は154 BMIだった。LLVM 23.1.4 でも Preview は1,260 files / 約964.86 MB、LayerFactory は1,160 files / 約976.44 MB、Project は1,490 files / 約966.30 MBを読み込んだ後に同じ内部エラーで停止した。Preview の未使用 GMF header 整理および video frame access の独立 module interface 化だけでは解消しなかった。診断内では `qcompilerdetection.h` が1,433回・約75.13 MB分の source-location entries を占める記録があり、同一の Qt compiler-detection header が複数 BMI 経由で重複する仮説を補強する。Clang の公式 named-modules 文書は BMI が AST を保存し、module imports を推移的に扱う構造を記述している。
- **上流状況:** LLVM PR [#209795](https://github.com/llvm/llvm-project/pull/209795) は、複数 module が同じ textual header を取り込むと各 BMI に重複して source location が保存され、ロード時に source-location address space を消費する問題と、その重複を再利用する修正を明記している。2026-10-22 時点で PR は open であり、[LLVM 23.1.4 のリリース変更一覧](https://discourse.llvm.org/t/llvm-23-1-4-released/92564) にも含まれていない。今回の現象と原因仮説が強く一致するが、このパッチを Artifact で適用・検証したわけではない。
- **仮説・未検証:** `ArtifactAbstractLayer` と `ArtifactAbstractComposition` の公開 interface が大量の依存 module をimportしているため、多数のQt/MSVC標準ヘッダー source locations が複数 BMIに保存され、consumerで合算されている可能性がある。公開型の所有境界を維持した interface closure の縮小、または新しい Clang での再現が解決候補。
- **価値または懸念:** この上限を回避できない限り、Artifact 全体の Windows upstream clang++ ビルドは成立しない。大規模interface変更はMSVC module graphとABIにも影響するため、まずimport閉包と最小consumerで測定する。
- **次に確認すること:** `ArtifactAbstractLayer` / `ArtifactAbstractComposition` それぞれの宣言で値型・継承・関数引数に必要な依存と実装専用依存を分類し、分割可能な境界を探る。PR #209795 の merge / LLVM 23.x への backport と、修正を含む Clang での再検証を追う。clang-cl でも同じ大規模 module closure による制限が出るか、Artifact target で確認する。

## 2026-10-22: Artifact object-only Ninja build の依存範囲

- **関連:** `Artifact/cmake/ArtifactSources.cmake`、`ArtifactGuiManager.cpp`、ArtifactAppRuntime / CMake 4.2.2 / Ninja。
- **確認できた事実:** Artifact source manifest を更新した後、既存 upstream clang++ build tree で対象 `ArtifactGuiManager.cpp.obj` だけを指定した。Ninja は CMake 再構成後に6,622 actionsを提示し、約180 actionsで停止した。再構成出力と action 数は確認したが、対象 object の compile には到達していない。
- **追加確認（2026-10-24）:** Render Queue の `.ddi` object target 2件を指定したところ、Ninja は4,449 actionsへ展開し、Diligent/SPIR-Vのcompileを開始したため245 actions時点で停止した。別途 `ArtifactBatchRenderer.ixx` を Clang/sccache commandでcompileすると、AST address-space約2.13GB（clang診断上99%）でfrontendがcrashしたため、その未検証の実装拡張子変更は元に戻した。一方、同じ Render 領域の `ArtifactAccumulatorPass.cppm` と `ArtifactRenderScheduler.cppm` はfresh P1689 scanが通り、全PCM providerを確認した既存module mapsを使うNinja生成sccache commandで個別object compileに成功した。warm repeatでsccache hitが2件増えた。
- **推測・未検証:** C++ modules の dyndep/provider closure と stale build tree の再生成が重なり、object 指定でも広い target graph が要求された可能性がある。専用 isolated target は拡張子・attachment の検証には有効だが、root target の統合確認を代替しない。
- **価値または懸念:** 拡張子移行の局所検証と app target 統合検証のコストを区別する必要がある。対象 object のみを狙ったつもりの build が third-party を含む大規模再構築へ広がることがある。
- **次に確認すること:** build tree の Ninja query / dyndep を調べ、対象 implementation の最小必要 provider closure と、root target の広域依存を引き起こす条件を特定する。`ArtifactBatchRenderer.ixx` のClang memory使用は依存BMI数・source-location共有・Clang build差を分けて再現する。
## 2026-10-22: `.gitattributes` と実ファイルの改行状態

- **関連:** `Artifact/.gitattributes`、`Artifact/src/Core/SystemStats.cppm`、`.cppm` → `.cpp` の rename。
- **確認できた事実:** `git check-attr text eol` は対象 `.cppm` に `text=set` / `eol=lf` を返したが、`git ls-files --eol` は `i/lf w/crlf attr/text eol=lf` を示した。rename 時に worktree を LF に揃えた後、index へ追加し直すと Git は内容100%同一の rename として判定した。
- **価値または懸念:** attribute 設定だけでは現在の worktree の改行コードが LF である証拠にならない。拡張子移行時は `git ls-files --eol` と rename diff を併用すると、不要な全行変更と既存の改行不整合を区別できる。
- **次に確認すること:** 今後移行する `.cppm` でも source と manifest の worktree 改行状態を確認し、必要な場合だけ正規化後に rename 判定と diff を確認する。
- **追加確認（2026-10-24）:** `Artifact/src/Mask/LayerMask.cppm` も作業ツリーbytesはCRLF、Git HEAD blobはLFだったが、Git statusではclean扱いだった。移行時にLFへ正規化すると `.cpp` はGit HEAD本文と一致した。ユーザー設定の `core.autocrlf` は `false` であり、`.gitattributes eol=lf` とGitのclean判定だけでは作業ファイルの実bytesを保証しないことを再確認した。
## 2026-10-22: Live scope widgets は paintEvent 内で画像分析を実行

- **関連:** `ArtifactWidgets/src/Color/VectorScopeWidget.cppm`、`ArtifactWidgets/src/Color/HistgramWidget.cppm`、`Artifact/src/Widgets/Color/ArtifactColorSciencePanel.cppm`、`Artifact/src/Widgets/Render/ArtifactCompositionEditor.cppm`。
- **確認できた事実:** Color Science panel は accepted scope frame ごとに visible な vectorscope と histogram へ `updateFrame(frame)` を呼び、composition editor の scope readback completion も両 widget を更新する。VectorScope は dirty な `paintEvent` で同期 `rebuildScopeImage` を呼び、新しい `QImage` の確保、`convertToFormat` と pixel loop を行う。Histogram も dirty な `paintEvent` で同期 `computeHistogram` を呼び、`convertToFormat` 後に pixel を走査する。
- **推測・未検証:** live playback で更新頻度が高い場合、GUI paint 内の CPU 処理と画像確保が UI 応答や frame presentation に影響する可能性がある。実際の呼び出し頻度、allocation 量、遅延は未計測。
- **価値または懸念:** module 移行とは別に、live scope の CPU work が UI paint boundary に置かれている点を性能計測対象として記録する。今回の拡張子移行では描画経路を変更しない。
- **次に確認すること:** 両 scope が表示された状態で代表的な frame size / playback を計測し、paint duration と allocation frequency を確認する。必要なら bounded reusable buffer と stale-frame rejection を含めて再設計を検討する。
## 2026-10-22: WaveformScope / ParadeScope の再計算ごとに completion handler が追加される

- **関連:** `ArtifactWidgets/src/Color/WaveformScopeWidget.cppm`、`ArtifactWidgets/src/Color/ParadeScopeWidget.cppm`、`Artifact/src/Widgets/Color/ArtifactColorSciencePanel.cppm`、`Artifact/src/Widgets/Render/ArtifactCompositionEditor.cppm`。
- **確認できた事実:** visible WaveformScope は accepted frame を受けると `updateFrame` が dirty 化し、`paintEvent` から `rebuildScopeImageAsync` が呼ばれる。同関数は各計算開始時に `QFutureWatcher<QImage>::finished` へ lambda を接続しているが、その接続を開始ごとに解除する処理は見当たらない。ParadeScope の `rebuildScopeImagesAsync` も `computing_` が false の各計算開始時に `QFutureWatcher<std::vector<QImage>>::finished` へ lambda を接続し、再利用前に解除する処理は見当たらない。
- **推測・未検証:** watcher の再利用後、finished signal ごとに過去の completion lambda も実行され、同じ result の反映や redraw が重複する可能性がある。実際の handler 数、画面負荷、終了時の lifetime 影響は未計測。ParadeScope でも finished 時に過去の callback が再実行される可能性があるが未検証。
- **価値または懸念:** frame 更新のたびに watcher callback が累積しないかを、extension migration と分けて確認する必要がある。今回の `.cpp` 移行では async / signal 経路を変更しない。
- **次に確認すること:** WaveformScope と ParadeScope の連続 frame 更新・dialog close を含めて completion callback 数・paint duration・allocation を計測し、単一 completion registration と stale-frame rejection の設計を検討する。
## 2026-10-22: CommandAction command state is not reflected by hasCommand()

- **関連:** `ArtifactWidgets/src/Common/CommandAction.cppm`、`ArtifactWidgets/include/Common/CommandAction.ixx`。
- **確認できた事実:** `CommandActionPrivate::hasCommand()` は `hasCommand_` を返さず常に `true` を返す。`CommandAction(const QString&, QObject*)` は受け取った command を private state に設定していないように見える。
- **推測・未検証:** これは既存の挙動不整合の可能性があるが、利用時の期待値や歴史的理由は未確認。現在の移行では処理を変更しない。
- **価値または懸念:** コマンド設定状態の問い合わせが正確でない可能性があり、呼び出し側が `hasCommand()` を分岐に使う場合に誤動作し得る。
- **次に確認すること:** `CommandAction` の全利用箇所とコンストラクタ呼び出しを調べ、`setCommand()` 前後に期待される状態を確認してから別途修正範囲を決める。
## 2026-10-22: Deprecated KnobSlider impl_ may be read before initialization

- **関連:** `ArtifactWidgets/src/Knob/KnobSlider.cppm`、`ArtifactWidgets/include/Knob/KnobSlider.ixx`。
- **確認できた事実:** public class は `Impl* impl_` を保持する。constructor initializer は `QWidget(parent)` のみで、implementation body 内に `impl_` への代入が見当たらない。一方 `mousePressEvent` は `impl_->dragStartPos` に書き込む。
- **推測・未検証:** `impl_` が初期化されない状態で mouse press を受けると不正アクセスになる可能性がある。deprecated widget の実生成有無、ゼロ初期化や別の定義経路の影響は未確認。
- **価値または懸念:** 廃止予定 UI でも外部利用可能な public class のイベント経路に危険が残る可能性がある。今回の拡張子移行では挙動を変更しない。
- **次に確認すること:** repository 全体で module import と `KnobSlider` の生成を追跡し、実利用・互換要件を確認してから削除／修正方針を別途決める。
## 2026-10-22: GraphicsFloatImageItem interface and implementation do not match

- **関連:** `ArtifactWidgets/include/Graphics/GraphicsItemFloatImageItem.ixx`、`ArtifactWidgets/src/Graphics/GraphicsItemFloatImageItem.cppm`。
- **確認できた事実:** interface は `ArtifactCore::GraphicsFloatImageItem` の constructor と destructor を宣言する。実装単位は同じ module 名だが、定義しているのは interface に宣言されていない `GraphicsFloatImageItemPrivate` だけで、public class の constructor / destructor definitions は見当たらない。repository source 内での利用元も見つからなかった。
- **推測・未検証:** public class を実際に生成する経路があれば未解決 symbol になる可能性がある。CMake / compiler で現行状態を再現したわけではない。
- **価値または懸念:** 拡張子を `.cpp` へ移すだけでは API / implementation mismatch は解決しない。今回の migration は宣言・実装の責務を変更しない。
- **次に確認すること:** interface の利用履歴、生成・除去された graphics item 経路、`GraphicsItemFloatImageItem.Temp.axt` の役割を調べ、API の完成・削除・再接続を別途判断する。

## 2026-10-22: TimeCode implementation の標準ヘッダー依存

- **関連:** `ArtifactCore/src/Time/TimeCode.cppm` / `TimeCodeRange.cppm` から `.cpp` への拡張子移行。
- **確認できた事実:** `TimeCode` implementation は `std::max` と `int64_t` を使っていたが、global module fragment に `<algorithm>` と `<cstdint>` がなかった。`TimeCodeRange` も `int64_t` に `<cstdint>` を直接含めず依存していた。各移行で必要な標準 header を source に直接追加した。
- **推測・未検証:** 以前は Qt または imported module の実装詳細から推移的に見えていた可能性があるが、実際の Clang diagnostic は未取得。
- **価値または懸念:** implementation source ごとの direct header dependency を明示すると、compiler / module boundary の違いに左右されにくくなる。
- **次に確認すること:** ビルド・依存スキャンを許可された後、MSVC・clang-cl・upstream clang++ の module scan と TimeCode / TimeCodeRange compile を確認する。

## 2026-10-22: 拡張子移行時の補助スクリプト参照

- **関連:** `ArtifactCore/check_remaining.py` と `TimeCodeRange.cppm` から `TimeCodeRange.cpp` への移行。
- **確認できた事実:** tracked checker script の固定対象リストに移行対象の絶対 Windows path が含まれていた。source / CMake manifest 以外の参照も検索し、対象 path を新拡張子へ更新した。
- **推測・未検証:** script は `X:\dev\ArtifactStudio` を固定しているため、他環境での再利用性は低い可能性がある。script の用途と実行結果は未確認。
- **価値または懸念:** 拡張子移行では補助 script、IDE 設定、生成 source list も旧 path を保持することがある。
- **次に確認すること:** 今後の移行でも tracked references を検索し、script の一般化は別作業として扱う。

## 2026-10-22: DepthMask implementation の Qt 直接依存

- **関連:** `ArtifactCore/src/Mask/DepthMask.cppm` から `DepthMask.cpp` への移行、および `Artifact/src/Widgets/LayerEditorMaskPressInteractionController.cpp`。
- **確認できた事実:** implementation は `Q_UNUSED` と `QVector4D` を使うが、global module fragment では `QVector3D` / `QMatrix4x4` だけを include していた。移行時に `QtGlobal` と `QVector4D` を直接追加した。
- **2026-10-24 追加確認:** `LayerEditorMaskPressInteractionController.cpp` が `QPointF` を使う一方、global module fragment にそのheaderを含めていなかった。Clangのmissing `qpoint.h` diagnosticに従い `#include <QPointF>` をmodule declaration前へ追加し、fresh scan / object compileが成功した。
- **推測・未検証:** 以前の compile は別 Qt header の推移的 include に依存していた可能性がある。compiler diagnostic は未取得。
- **価値または懸念:** named module import だけでは Qt header 内の全宣言が通常 header include のように伝播するとは限らない。使用する Qt 型・macro の直接依存を source 側に明示する。
- **次に確認すること:** ビルド・依存スキャン許可後、MSVC・clang-cl・upstream clang++ で DepthMask implementation compile と module scan を確認する。

## 2026-10-22: Saturation の interface / implementation 宣言差

- **関連:** `ArtifactCore/include/Image/Sarturation.ixx` と `ArtifactCore/src/Color/Sarturation.cppm`。
- **確認できた事実:** public interface は `Saturation(float)` を宣言するが、対応 implementation source にその constructor definition は見当たらない。default constructor、destructor、saturation getter/setter、comparison definitions はある。
- **推測・未検証:** parameterized constructor の利用があれば unresolved symbol になる可能性があるが、link で再現した事実はない。
- **価値または懸念:** 拡張子だけ移すと宣言／定義の不一致は残るため、移行前に public API と method definitions を照合する必要がある。
- **次に確認すること:** usage を調査し、constructor の補完・削除を別途判断する。今回の移行では変更しない。

## 2026-10-22: ColorSwatch interface uses unique_ptr PImpl

- **関連:** `ArtifactCore/include/Color/ColorSwatch.ixx`、`ArtifactCore/src/Color/ColorSwatch.cpp`
- **確認できた事実:** `ColorSwatch` public module interface stores `std::unique_ptr<Impl>` and declares copy / move operations; implementation defines them out-of-line.
- **推測・未検証:** DLL / module boundaryでのallocatorおよびdestructor ABIに影響するか、現行 toolchain の export configuration 下では評価していない。
- **価値または懸念:** 現行のPImpl所有ルールに照らして、将来この型を変更する場合に所有権境界とABIを先に照合する手掛かりとなる。
- **次に確認すること:** 実際のDLL export境界・consumer側の生成コード・copy/move利用を調査し、独立した変更として扱うか判断する。

## 2026-10-22: RendererQueueSetting template definition is implementation-only

- **関連:** `ArtifactCore/include/Render/RendererQueueSetting.ixx`、`ArtifactCore/src/Render/RendererQueueSetting.cppm`
- **確認できた事実:** public interface declares `template<StringLike T> setRendererQueueName(const T&)`; the matching template definition is in the module implementation unit.
- **推測・未検証:** consumers may need the definition reachable from the imported interface to instantiate this member template. Current MSVC and Clang compile/link behavior has not been checked.
- **価値または懸念:** this can be a compiler-specific module boundary issue and should be checked before migrating the implementation file.
- **次に確認すること:** search call sites and inspect BMI/IFC visibility; then reproduce with MSVC, clang-cl and upstream clang++ in an isolated consumer before deciding on a code change.

## 2026-10-22: CoordinateResolver expression evaluator precedence

- **関連:** `ArtifactCore/src/Coordinate/CoordinateProfile.cpp`、`CoordinateResolver::resolveExpression`。
- **確認できた事実:** expression parser は演算子に遭遇するたびに現在の term を直前の演算子で累積値へ適用し、括弧を特別扱いしない。
- **推測・未検証:** `2+3*4` のような入力は通常の算術優先順位ではなく左から逐次評価される可能性がある。式の正式な文法や既存利用者がこの挙動を前提としているかは未確認。
- **価値または懸念:** 座標式を拡張する前に、後方互換を含めて演算子優先順位と括弧の契約を定義する必要がある。
- **次に確認すること:** expression の実際の利用箇所・設定例を調べ、現在の式文法を仕様化して runtime 例を確認する。


## 2026-10-22: AspectRatio PImpl ownership boundary

- **関連:** `ArtifactCore/include/Core/AspectRatio.ixx`、`ArtifactCore/src/Core/AspectRatio.cpp`。
- **確認できた事実:** exported `AspectRatio` interface owns `std::unique_ptr<Impl>` and declares copy / move operations; the implementation defines those operations out of line.
- **推測・未検証:** DLL / module consumer boundaries may make allocator and incomplete-type ownership behavior worth checking. No ABI or consumer build was run for this note.
- **価値または懸念:** current repository guidance prefers raw-pointer PImpl ownership for new types, while existing smart-pointer PImpl must not be mechanically replaced without compatibility review. This migration intentionally preserved the existing ownership model.
- **次に確認すること:** when next changing this type's ownership, inspect DLL export boundaries, consumers, and copy / move generation before proposing a separate change.

## 2026-10-22: LayerState copy/move and solo toggle definitions

- **関連:** `ArtifactCore/include/Layer/LayerState.ixx`、`ArtifactCore/src/Layer/LayerState.cppm`。
- **確認できた事実:** `LayerState` の copy / move constructor は新しい `Impl` を作るだけで、source の state をコピー・移譲しない。`toggleSolo()` の実装は空で、`Impl::solo_` を変更しない。
- **推測・未検証:** call site がこれらの操作を利用する場合、コピー後・move 後に visible / locked / solo / guide / adjustment state が意図せず失われる可能性がある。実際の利用箇所で問題が再現するかは未検証。
- **価値または懸念:** 拡張子移行ではこの既存挙動を変更せず、LayerState 自体の移行前に API 実装と呼び出し側の契約を別途精査する必要がある。
- **次に確認すること:** `LayerState` の生成・copy/move・`toggleSolo()` call site を調査し、期待する state transition を確認して独立した修正として判断する。


## 2026-10-22: ValidationRules provider が source manifest に未登録の可能性

- **関連:** `ArtifactCore/cmake/ArtifactCoreModuleReferences.cmake`、`ArtifactCore/cmake/ArtifactCoreSources.cmake`、`ArtifactCore/src/Diagnostics/ValidationRules.cppm`。
- **確認できた事実:** provider mapping には `src/Diagnostics/ValidationRules.cppm|Core.Diagnostics.ValidationRules|...` がある一方、CMake が再帰 discovery を行わないと宣言する source manifest と `ArtifactCore/CMakeLists.txt` の source registration には、この実装パスが見つからなかった。
- **推測・未検証:** `ValidationRules.cppm` は現行 ArtifactCore target で compile されていない可能性がある。意図的に無効化された旧 implementation かは未確認。
- **価値または懸念:** source manifest を前提に `.cppm` migration を進めると、provider table だけに残った未使用実装を誤って build target に追加する恐れがある。
- **次に確認すること:** CMake target source evaluation と呼び出し／link 利用を調べ、意図的な除外か manifest 漏れかを判断してから、別変更として修正する。


## 2026-10-22: Artifact.Composition._2D module name warning in Clang

- **関連:** `Artifact/include/Composition/ArtifactComposition2D.ixx`、`Artifact/src/Composition/ArtifactComposition2D.cpp`。
- **確認できた事実:** upstream clang++ 22.1.3 の module scan / compile smoke で、既存の public module 名 `Artifact.Composition._2D` の `_2D` segment に reserved module name warning が出た。拡張子移行では module 名を変更していない。
- **推測・未検証:** module 名変更が既存 import / BMI / MSVC 利用者に与える影響と、将来の標準・compiler interoperability 上の実害は未評価。
- **価値または懸念:** public module rename は依存全体へ波及するため、警告だけを理由に移行範囲へ含めない。別の互換性調査として扱う。
- **次に確認すること:** 全 consumer と cross-compiler module import を洗い出し、変更が必要と判断した場合は独立した移行計画を作る。

## 2026-10-22: AudioLimiter の条件付き delay buffer 再確保

- **関連:** `Artifact/src/Audio/ArtifactAudioLimiter.cpp`。
- **確認できた事実:** `AudioLimiter::process()` は sample rate の変化時に delay buffer を clear し、delay buffer が有効で channel count が異なる場合は `delayBuf_.assign(...)` で channel ごとの領域を再確保する。
- **推測・未検証:** この関数がリアルタイム audio callback から呼ばれる経路では、format change 時の allocator 呼び出しが deadline を乱す可能性がある。実際の呼び出し経路、変更頻度、buffer reuse の可否は未確認。
- **価値または懸念:** `.cppm` から `.cpp` への拡張子移行では動作を変えず、この既存 allocation behavior を別途調べる価値がある。
- **次に確認すること:** Audio processing の呼び出し元と format change の境界を追い、再確保が callback 内で起きるか、準備済み capacity を保持できるかを確認する。

## 2026-10-23: FluidVisualizer のフレームごとの source copy

- **関連:** `ArtifactCore/src/ImageProcessing/FluidVisualizer.cpp`、`ArtifactCore/include/ImageProcessing/FluidVisualizer.ixx`。
- **確認できた事実:** `FluidVisualizer::render()` は出力画素数分の `std::vector<float4>` を生成し、呼び出しごとに入力 buffer 全体をコピーしてから `ForPixels` 処理を行う。ArtifactCore / Artifact のソース検索では呼び出し元が見つからなかった。
- **推測・未検証:** クラス名と API から描画経路で使う可能性はあるが、外部利用を含む実呼び出し頻度、vector allocation の実コスト、copy を除去できる安全な設計は未確認。
- **価値または懸念:** render 呼び出しが反復される経路なら、大きな一時確保と全画素 copy が frame cost に影響しうる。今回の拡張子移行では挙動を変えず、調査項目として記録する。
- **次に確認すること:** module consumer と call site を追跡し、buffer aliasing の必要性、容量再利用の可否、呼び出し頻度を特定してから別変更として評価する。
\n## 2026-10-23: Viewport color key generation in the render path\n\n- **Related:** Artifact/src/Widgets/Render/ViewportColorPipeline.cpp; Artifact/src/Widgets/Render/ArtifactCompositionRenderController.cppm.\n- **Confirmed:** ViewportColorPipeline::prepare() constructs a QString key through multiple QString::arg() calls before checking whether the key changed. The composition render controller calls prepare() from applyDisplayColorTransform().\n- **Inference / unverified:** This call may run once per displayed composition frame; exact call frequency, heap allocation count, and user-visible cost have not been profiled.\n- **Value or concern:** If repeated in steady-state rendering, rebuilding the unchanged key may add avoidable string work and allocations. The current extension migration preserves behavior and does not address this.\n- **Next check:** Measure prepare() call frequency and allocations, then consider a revision-based or cached key in a separate performance change if confirmed.\n
## 2026-10-23: RendererQueueSetting の既存未実装 setter / copy constructor

- **関連:** `ArtifactCore/src/Render/RendererQueueSetting.cpp`、`ArtifactCore/include/Render/RendererQueueSetting.ixx`
- **確認できた事実:** `RendererQueueSetting::Impl::setRendererQueueName()` の本体は空で、copy constructor は引数 `settings` を使わず新しい `Impl` を確保する。ArtifactCore / Artifact のソース検索では consumer は `RendererQueueManager.ixx` の import 以外に見つからなかった。
- **推測・未検証:** 現在の呼び出し元で名前設定や copy が必要な場面があるか、外部 consumer が存在するかは未確認。
- **価値または懸念:** API 利用時に設定値が失われる可能性がある。拡張子移行では挙動を変更しておらず、独立した修正候補として扱う。
- **次に確認すること:** manager の construction / copy / name setter 経路と公開 API consumer を確認し、必要なら挙動修正を別変更として評価する。

## 2026-10-23: clang-cl preset の vcpkg manifest mode

- **関連:** `CMakePresets.json` の `x64-Debug-ClangCL`、root configure。
- **確認できた事実:** preset が `VCPKG_INSTALLED_DIR` と `VCPKG_MANIFEST_MODE=OFF` を指定していない状態で configure すると、CMake / vcpkg が build dir 内へ manifest dependencies を新規展開しようとした。C: の空きがない環境では configure が失敗した。既存 `C:/vcpkg/installed` の利用を明示した preset では configure / generate が成功した。
- **推測・未検証:** 新規 clone / clean machine でこの固定 installed tree を使う運用が常に適切かは未評価。
- **価値または懸念:** Windows の clang-cl と upstream clang++ で dependency provisioning を揃えると、意図しない vcpkg restore の大規模なディスク消費を避けられる。preset に既存 vcpkg root、installed tree、triplet、manifest mode を記録した。
- **次に確認すること:** 別の clean vcpkg installation / manifest mode を使う contributor workflow が必要か、既存 preset 利用者の環境を確認して判断する。


## 2026-10-23: SimpleChoker の per-call full-frame scratch allocation

- **関連:** `ArtifactCore/src/ImageProcessing/SimpleChoker.cpp`、`ArtifactCore/include/ImageProcessing/SimpleChoker.ixx`。
- **確認できた事実:** `SimpleChoker::process(float4*, int, int, ...)` は `width * height` 個の `float4` を `new[]` し、入力 buffer 全体をコピーしてから pixel loop を実行し、最後に `delete[]` する。
- **推測・未検証:** effect evaluation から高頻度に呼ばれる場合、大きな allocation と全 pixel copy が処理時間・allocator contention に影響する可能性がある。実 caller、call frequency、allocation profile は確認していない。
- **価値または懸念:** 画素処理の意味を保ったまま再利用 workspace 化できるかを別途評価する価値がある。今回の `.cppm` → `.cpp` 移行では挙動を変更していない。
- **次に確認すること:** `SimpleChoker` の consumer と process callsite を特定し、呼び出し頻度と必要な buffer lifetime を確認してから hot-path allocation の別変更として検討する。


## 2026-10-23: ArtifactCore Ninja object target inherits wide ordering dependencies

- **関連:** `ArtifactCore/CMakeLists.txt` の `ArtifactCore` target、clang-cl / Ninja root build graph。
- **確認できた事実:** `ninja -t query` で単体 `BitmapImage.cpp.obj` の command を確認できたが、その object target を build すると `cmake_object_order_depends_target_ArtifactCore` と target dyndep を経由して OpenUSD Sdf prerequisites が起動した。全 ArtifactCore target build も OpenUSD を含む広い initial dependency build に展開した。
- **推測・未検証:** target-level ordering dependency がどの `add_dependencies` / object library relationship で広がるか、module ordering に必要な最小境界は未特定。
- **価値または懸念:** root clang-cl の個別 source 検証でも依存群全体を再コンパイルする可能性があり、試験所要時間が source 本体の複雑さを反映しない。
- **次に確認すること:** ArtifactCore target dependencies と generated Ninja order-only edges を追い、必要性を損なわず clang-cl の対象限定 build が可能な fixture / object boundary を設計する。


## 2026-10-23: Emboss CPU effect allocates a full-frame temporary buffer

- **関連:** `ArtifactCore/src/ImageProcessing/Emboss.cpp`、`ArtifactCore/include/ImageProcessing/Emboss.ixx`。
- **確認できた事実:** `Emboss::process(float4*, int, int, ...)` は全画素数分の `float4` scratch buffer を `new[]` し、入力全体を copy して parallel tile evaluation 後に `delete[]` する。
- **推測・未検証:** effect evaluation から繰り返し呼ばれる場合、全画素 allocation / copy が frame cost に影響する可能性がある。consumer、呼び出し頻度、allocation profile は未確認。
- **価値または懸念:** workspace 再利用などの余地を実 call path と hot-path contract に沿って評価する価値がある。今回の `.cppm` → `.cpp` 移行では挙動を変更していない。
- **次に確認すること:** Emboss の consumer と process callsite を調べ、頻度と workspace lifetime を特定してから allocation を別変更として検討する。


## 2026-10-23: SpinTransition implementation is absent from explicit source registration

- **関連:** `ArtifactCore/src/Video/Transitions/SpinTransition.cppm`、`ArtifactCore/cmake/ArtifactCoreSources.cmake`、`ArtifactCore/CMakeLists.txt`、`ArtifactCore/cmake/ArtifactCoreModuleReferences.cmake`。
- **確認できた事実:** module-reference manifest は `Video.Transitions.SpinTransition` とその `.cppm` implementation path を列挙する一方、ArtifactCore の explicit implementation source list と Video implementation list に `SpinTransition.cppm` がなく、root `ArtifactCoreVideo` buildでもimplementation objectは生成されなかった。
- **推測・未検証:** omission が意図的か、遷移 registrar の runtime reachability に影響するか、別経路でリンクされる利用者があるかは未確認。
- **価値または懸念:** 移行対象として登録済み implementation と誤認すると、拡張子移動で未ビルドコードを新たに有効化する変更になる。今回は scope を保つため `SpinTransition` を `.cppm` のまま残し、移行には含めなかった。
- **次に確認すること:** Spin transition の product behavior / registrar consumer と、explicit manifest から外れている設計意図を確認し、別作業で source registration が必要か判断する。


## 2026-10-23: Creative effects copy full color channels per render

- **関連:** `ArtifactCore/src/Graphics/Effect/FisheyeEffect.cpp`、`MirrorEffect.cpp`、`KaleidoscopeEffect.cpp`、`EdgeEchoEffect.cpp`、`LightPressureEffect.cpp`、`TemporalFossilEffect.cpp`、`DepthMeltEffect.cpp`、`GlitchCreativeEffect.cpp`、`OldTVEffect.cpp`、`PigmentSeparationEffect.cpp`、`SurfaceMemoryEffect.cpp`。
- **確認できた事実:** 各 `process()` は Red / Green / Blue channel の全要素を、それぞれの入力 `VideoChannel` からローカル `std::vector<float>` 3個へコピーしてから画素処理する。追加した EdgeEcho / LightPressure / TemporalFossil / DepthMelt / GlitchCreative / OldTV / PigmentSeparation / SurfaceMemory でも同じ3チャンネル全コピーを確認した。
- **推測・未検証:** 実 caller、呼び出し頻度、対象解像度、frame cost / allocation 数は未計測。
- **価値または懸念:** 繰返し評価時に複数の全画素 allocation と copy が発生し得る。今回の拡張子移行では処理を変更しない。
- **次に確認すること:** effect 呼び出し経路と頻度を確認し、入力と出力の aliasing / 読み取り一貫性を保てる bounded workspace または再利用 buffer の可否を評価する。


## 2026-10-23: Chromatic aberration effect allocates full-frame channel temporaries

- **関連:** `ArtifactCore/src/Graphics/Effect/ChromaticAberrationEffect.cpp`。
- **確認できた事実:** `process()` は Red / Blue channel の全要素数ぶんの `std::vector<float>` を2個確保し、各チャンネルを一時配列へコピーしてから色ずらし処理を行う。
- **推測・未検証:** effect の呼び出し頻度、解像度別の確保量、frame cost は未計測。
- **価値または懸念:** 高解像度・反復適用時に大きな一時確保と copy が生じ得る。今回の拡張子移行では処理を変更していない。
- **次に確認すること:** caller と buffer aliasing 条件を確認し、再利用領域で処理できるか別途評価する。

## 2026-10-23 — SpinTransition implementation registration gap

- **関連:** `ArtifactCore/src/Video/Transitions/SpinTransition.cppm`, `ArtifactCore/cmake/ArtifactCoreModuleReferences.cmake`, `ArtifactCore/cmake/ArtifactCoreSources.cmake`, `ArtifactCore/CMakeLists.txt`
- **確認できた事実:** `Video.Transitions.SpinTransition` は module-reference manifest に記載されている一方、実装ソースは `ArtifactCoreSources.cmake` と `ARTIFACTCORE_VIDEO_IMPL` に登録されていない。今回の CMake target scan にも同実装は現れなかった。
- **価値／懸念:** 現状では Clang / MSVC のいずれでもこの実装単位の compile 検証・リンクはされない。ソース登録を補うと遷移実装が有効化される可能性があり、単なる拡張子移行の範囲を越える。
- **次に確認すること:** transition 実装の登録漏れが意図的か確認し、挙動を有効化してよい判断が得られた後に、ソース登録・ビルド順・拡張子移行を一緒に扱う。


## 2026-10-23: Acoustic split target requires direct Clang verification

- **関連:** `ArtifactCore/CMakeLists.txt` の `ARTIFACTCORE_ACOUSTIC_MODULES` / `ArtifactCoreAcoustic` target、7つの `src/Acoustic` 実装。
- **確認できた事実:** 7実装は `ArtifactCore` の通常targetから除外され、`ArtifactCoreAcoustic` に登録されている。今回 `ArtifactCore` だけをbuildした時点では各実装のcompileを確認できず、専用targetのbuildで初めて全7実装のP1689 scan・compile・archive linkを確認できた。
- **価値または懸念:** split target を持つ領域では親の主要target成功だけでは移行対象のClang対応を証明できない。
- **次に確認すること:** 今後も変更した実装が所属する実際のtargetを source registration から特定し、そのtargetをbuildして確認する。


## 2026-10-23: Explicit CXX_MODULES file set accepts `.cpp` module interfaces under upstream Clang

- **関連:** `ArtifactCore/cmake/ArtifactCoreSources.cmake` の `ARTIFACTCORE_MODULE_SOURCES`、`ArtifactCore/CMakeLists.txt` の `ARTIFACTCORE_AI_MODULES`、16個の `Core.AI.*Descriptions` module interfaces。
- **確認できた事実:** 16個の exported module source を `.cppm` から `.cpp` に変更し、両明示 module source lists を更新した。upstream clang++ 22.1.3 / CMake 4.4.4 / Ninja の `ArtifactCoreAI` target が module scan、全16 compile、static archive link に成功し、warm rebuild は16 sccache hits、0 missesだった。
- **価値または懸念:** 拡張子ヒューリスティックとは別に、明示 `CXX_MODULES` file set が `.cpp` の exported interface を module source として扱えることを、この実 target で検証できた。これは明示 file set に含まれない一般の `.cpp` が module interface になることを意味しない。
- **次に確認すること:** 同じ登録方式を使う他の split target で小さな module group を移行し、upstream clang++ の scan / link と cache behavior を検証する。


## 2026-10-23: Core utility module rename fans out but remains fully cacheable

- **関連:** `ArtifactAtomic` / `ArtifactOptional` / `ArtifactHashMap` module interfaces、ArtifactArray / ArtifactDict / NamedVector consumers、upstream clang++ 22.1.3、sccache 0.18.0。
- **確認できた事実:** 3 module interface implementation paths を `.cppm` から `.cpp` に移し、手動 object-order references を更新した後、`ArtifactCore` target の初回ビルドと archive link に成功。module provider 変更により初回 build は271 Ninja steps、warm rebuild は268 stepsとなったが compile requests は262 hits / 0 misses、cache errors 0 だった。
- **価値または懸念:** ArtifactCore の汎用型 module interface は多くの consumer を再ビルドへ巻き込む。sccache はこの波及範囲を compile cache hit で吸収できる。
- **次に確認すること:** 今後 provider interface を移行する場合、直接ファイルだけでなく依存 consumer の object ordering と全 target build の範囲を確認する。
- **追加確認（2026-10-23）:** `Memory.SharedPtr` / `TrackedPtr` / `UniquePtr` の3 provider を `.cpp` に移し、source manifest、29 module-reference entries、CMake object dependencies を更新。`ArtifactCore` compile / link と NamedVector を含むconsumer graphが成功し、warm rebuildは203 Ninja steps中197 compile hits / 0 misses。


## 2026-10-23: ArtifactCore に未登録の `.cppm` 実装が複数残る

- **関連:** `ArtifactCore/src/**/*.cppm`、`ArtifactCore/cmake/ArtifactCoreSources.cmake`、`ArtifactCore/cmake/ArtifactCoreModuleReferences.cmake`、`ArtifactCore/CMakeLists.txt`。
- **確認できた事実:** 現在 `ArtifactCore/src` に物理 `.cppm` が18件ある。source manifest / CMake source registration に現れるのは `Reactive/ReactiveEvents.cppm` と `Render/RendererQueue.cppm` の2件で、残り10件は module-reference manifest のみに記載、6件は source registration と module-reference のどちらにも記載がない。`ArtifactCore` は再帰 source discovery を行わないと CMakeLists に明記している。
- **推測・未検証:** reference-only / unregistered の16実装が意図的に無効化された旧コードか、登録漏れかは未確認。
- **価値または懸念:** これらを `.cpp` にrenameしても現行 target の Clang compile 検証にはならない。source registration を足すと未使用機能を build graph に再導入し、link / dependency graph を変える可能性がある。
- **次に確認すること:** 各 public `.ixx` の consumer と git history、target 構成を照合し、意図的な除外か登録漏れかを分類してから個別に移行判断する。

## 2026-10-23: Ofx source extensions hide export-module interfaces

- **関連:** `Artifact/src/Effects/Ofx/ArtifactOfxEffectImpl.cppm`, `ArtifactOfxHost.cppm`。
- **確認できた事実:** 両 `.cppm` は `module;` のglobal module fragmentに続いて `export module Artifact.Effect.Ofx.*;` を宣言し、exportされた型の宣言を含む。拡張子だけを見て実装単位と分類できない。
- **推測・未検証:** source manifest上で `.cppm` としてどのCMake target / file setに配置するのが意図された構成か、再整理が必要かは今回の移行調査では未検証。
- **価値または懸念:** `.cpp` への変更で既存PCM依存が欠落し、sccache compileがHost PCM不足で失敗した。またOfxHost単独compileではexport redeclarationが発生した。interface形式のソースを拡張子移行対象として扱わず、CMakeのmodule classification / provider registrationを別途監査する必要がある。
- **次に確認すること:** CMake生成file setとP1689 provider scanを照合し、二つのexport module providerの正しいinterface登録状態を調べる。


## 2026-10-23: upstream Clang module PCM generation can approach source-location capacity

- **関連:** `Artifact/src/Render/ArtifactCompositionViewDrawing.cppm`、`Artifact/src/Export/ArtifactExportSession.cpp` / `ArtifactExportPreRenderPipeline.cpp`、upstream clang++ 23.1.4、Ninja module map。
- **確認できた事実:** Export implementation の個別compileで不足していた `Artifact.Render.CompositionViewDrawing.pcm` を得るため、依存interface sourceを単独compileしたところ clang frontend が exit 70 でクラッシュした。診断は source manager location usage 2.14 GB、available space の99%を示した。同じ試行で `Artifact.Render.OffscreenComposition.pcm` の生成は成功した。
- **推測・未検証:** クラッシュの直接原因がlocation address-space上限、反復読み込みによる module graph膨張、または別の compiler defect かは未特定。Ninja全target build時にも再現するか未確認。
- **価値または懸念:** 大規模 module mapを持つ1 translation unitの単独検証でもclang frontendのメモリ／source-location上限に達し得る。依存PCM不足を無視したobject compile結果は source compatibility の証拠にできない。
- **次に確認すること:** 可能なら最小の再現コマンドと clang crash reproducerを保持し、同moduleのimport集合・PCM再利用状況・clangの修正版で比較する。広域ビルドに拡張する前にメモリ余裕と出力先容量を確認する。


## 2026-10-23: Clang split-target build may fan out through stale dependency state

- **関連:** `ArtifactColorNode` target、CMake 4.4.4 / Ninja、upstream clang++ build tree `V:\ArtifactStudioBuilds\x64-Debug-ClangXX23-MsvcAbi`。
- **確認できた事実:** targetをNinja経由でビルドした際、再configure後も既存 depfile / command-log の再構築が発生し、OpenUSDを含む3,179 stepsが列挙された。550 stepsで止め、対象2 `.cpp` implementationのP1689 scan / compileと、既存2 interface objectを含む `ArtifactColorNode.lib` archive commandを直接実行して成功させた。
- **推測・未検証:** 広域fan-outの直接原因がbuild treeの古いdepfile、missing command log、verify-globsの再走査、またはそれらの組合せかは未確定。クリーンな専用build treeでも再現するか未検証。
- **価値または懸念:** 特定split targetの短い検証を意図しても、既存build treeでは大量のthird-party actionに波及し得る。target全体のcompile/linkを成功したと誤報せず、個別objectとarchiveの検証範囲を分けて記録する。
- **次に確認すること:** Clang laneの専用build treeを一度クリーンに構成し、module scan / dyndep生成後のtarget action数と依存順を比較する。大規模依存再構築が不要な状態を確認してからtarget全体の検証を再開する。

## 2026-10-23: ArtifactCore 残存 `.cppm` の登録監査と Distort 移行 manifest

- **関連:** `ArtifactCore/src/**/*.cppm`、`ArtifactCore/cmake/ArtifactCoreSources.cmake`、`ArtifactCore/cmake/ArtifactCoreModuleReferences.cmake`、`Artifact/CMakeLists.txt`、`ArtifactEffectsDistort`。
- **確認できた事実:** 現在の ArtifactCore 作業ツリーには12個の `.cppm` が残る。うち2件 (`ReactiveEvents`, `RendererQueue`) はsource登録済み、10件は未登録。未登録10件のうち4件はmodule-reference manifestだけに記載され、6件は両manifestに記載されない。`AnamorphicFlare` と `MonochromeHalide` はpartition interface / 実装 / reference mappingが揃う登録候補。`EasingCurveUtil` の公開interfaceは同等機能をinline実装し、`ValidationRules` は現行ソースでconsumerを確認できない。残り6件はtest専用2件、module宣言のない旧stub/旧実装4件で、通常target登録の根拠がない。ArtifactCoreの登録は変更していない。
- **10件の個別判定:** `AnamorphicFlare` はpublic partitionと集約interfaceからの再export、implementation/reference mappingが揃い登録候補。ただし `process()` は画像全体の複製と2つの大容量 `std::vector` 確保を行うため、runtime接続前に hot-path contract のレビューが必要。`MonochromeHalide` は `ImageProcessing:Halide` interfaceと実装が揃う登録候補だが、Halide/OpenCV依存と有効化条件をtarget側で先に確認する。`EasingCurveUtil` はTimeline consumerがある一方、public `.ixx` が主要APIをinline定義しており、sourceにも重複実装があるため現状のままsource登録しない。`ValidationRules` はconsumerを見つけられず、実装がArtifact層moduleをimportしてCore→Artifact依存を逆流させるので登録しない。`FFmpegEncoder.Test` / `SoftwareRayTracer.Test` はtest専用で通常library targetには追加しない。`HalideImageF32x4RGBA` は空stub、`Affin` はコメント化されたincludeと空namespace、`Glow` はmodule宣言なしの旧Halide helper、`FilmGrain` はmodule宣言なしのOpenCV G-API kernel定義で、いずれも現在のmodule implementation sourceとして登録する根拠がない。
- **変更・確認:** Distort source pinning が移行済み `TimeDisplacementEffect.cpp` ではなく旧 `.cppm` をAPP_IMPLへ追加していた。`.cpp` 優先・未移行時 `.cppm` fallbackへ修正し、CMake 4.2 / Ninja / upstream clang++ 23.1.4 + MSVC ABI でroot configure/generate成功。ArtifactEffectsDistort targetのbuildは未実施。
- **価値／懸念:** 拡張子移行ではsource manifestだけでなく、追加の明示target pinningも追跡する必要がある。画像処理2候補も現状ではtarget未登録であり、登録するとbuild graphを変えるため別判断を要する。
- **追加確認（2026-10-23）:** upstream clang++で3件のP1689 scan / object compileが成功。cold 3 miss、warm 3 hit。通常のdyndep targetはthird-partyを含む2,430 actionsへ広がり、486で停止した後、対象の既存DDI群とCMake dyndepを直接生成して3 objectをcompile、既存objectと合わせて`ArtifactEffectsDistort.lib`を再アーカイブした。ArtifactCore候補2件は引き続き登録意図が確認できるまで保留する。


## 2026-10-23: CMake Ninja dyndep から実装targetを限定検証できる

- **関連:** `ArtifactEffectsDistort`、`ArtifactEffectsWave`、`ArtifactEffectsSurfaceFX`、upstream clang++ 23.1.4 / Ninja / CMake 4.2。
- **確認できた事実:** 通常のDistort dyndep target buildはOpenUSD等を含む2,430 actionsへ展開した。一方、targetの既存P1689 `.ddi` 群、`CXXDependInfo.json`、`cmake_ninja_dyndep` を使って対象の `.obj.modmap` を生成し、変更対象だけをsccache付きcompile、既存objectを合わせてtarget archiveへ再格納できた。3つのtargetで実装source scan / compile、合計3 target archivesが成功した。
- **価値／懸念:** 既存build treeの広いorder dependencyを実行せず、CMakeが生成したmodule mappingでextension migrationのcompileとarchiveを確かめられる。ただし生成済み依存PCM / interface objectsの存在を前提とし、clean buildや通常Ninja targetの成功を証明しない。
- **次に確認すること:** clean build treeまたは更新済み依存が揃った環境で通常target buildを再評価し、P1689 scanからarchiveまでNinja管理下で完了する依存範囲を比較する。


## 2026-10-24: Spatial specialty effects also compile from `.cpp`

- **関連:** `Artifact/src/Effects/DirectionalGlowEffect.cpp`、`AutoMosaicEffect.cpp`、`ArtifactEffectsGlow`、`ArtifactEffectsAutoMosaic`。
- **確認できた事実:** 両 implementation source はそれぞれ専用 module interface / CMake target と結び付き、`.cpp` 化後も upstream clang++ 23.1.4 / Windows MSVC ABI で P1689 scan、object compile、static archive生成に成功した。sccache cold 2 miss / warm 2 hitで、累積のcache error / compilation failureは増えなかった。実装本文はGit HEADと一致しLF。
- **価値または懸念:** specialized effect targetでも同じ明示manifest / module map経路が成立する。Ninja管理下のtarget全体 build とArtifactアプリ統合linkは未検証。
- **次に確認すること:** Wave / SurfaceFX、DirectionalGlow / AutoMosaicの小targetで得た結果を踏まえ、残る implementation-only `.cppm` を登録済みtargetごとにまとめて移行し、target archiveと必要なconsumer linkを段階確認する。


## 2026-10-24: 同一primary moduleの複数implementationを `.cpp` に移行できる

- **関連:** `Artifact/src/Generator/ArtifactParticleGenerator.cpp`、`ParticleEmitterDescription.cpp`、`Artifact.Generator.Particle`、`ArtifactAppRuntime`。
- **確認できた事実:** 両sourceは同じ `module Artifact.Generator.Particle;` を宣言する別implementation unitである。両方を `.cpp` へ移し、explicit source manifestを更新後、upstream clang++ 23.1.4 / Windows MSVC ABI のP1689 scanとobject compileが成功した。sccache cold 2 miss / warm 2 hit。source bodyはGit HEADと一致しLF。
- **価値または懸念:** 同一primary moduleに属する複数implementationをひとまとまりで移行しても、生成済み interface PCMを使う限定compileではClangが扱える。`ArtifactAppRuntime` の全dyndep / link成功を意味しない。
- **次に確認すること:** 同targetに残るimplementation-only sourceをCMake scan / PCMの状態で分類し、小グループごとのcompileと必要なtarget linkを進める。

## 2026-10-24: Generator implementation pair has different dependency verification outcomes

- **関連:** `Artifact/src/Generator/CloneGenerator.cpp`, `AbstractGeneratorEffector.cpp`, `ArtifactAppRuntime`。
- **確認できた事実:** 両 implementation source を `.cppm` から `.cpp` に移し、Artifact explicit source manifestを更新。CloneGenerator は通常CMake生成 module mapで upstream clang++ 23.1.4 / Windows MSVC ABI object compileに成功した。AbstractGeneratorEffector はP1689 scan成功したが、局所module map compileは `Artifact.Layer.Abstract.pcm` の間接依存PCM不足（`Utils.Result` 等）とstale PCMで停止。public interfaceには必要な `Memory.SharedPtr` と `Artifact.Layer.Abstract` のimportが存在する。
- **推測・未検証:** AbstractGeneratorEffector の停止がclean/更新済み ArtifactAppRuntime dyndep buildで解消するかは未検証。現在のエラーはsource互換性不良の証拠ではない。
- **価値または懸念:** 対象ソースの限定compileでも、直接DDIだけで生成したmodule mapはCMakeの既存PCM依存閉包を完全に再現しない場合がある。通常のtarget dyndep compileとの結果を分けて報告する必要がある。
- **次に確認すること:** cleanまたは依存PCMを更新したClang build treeで対象interface dependency closureを含む通常dyndep buildを再試行し、AbstractGeneratorEffector のcompile結果を確定する。

## 2026-10-24: Color management and grading compile after `.cppm` migration

- **関連:** `Artifact/src/Color/ArtifactColorManagement.cpp`, `ArtifactColorGradingEngine.cpp`, `ArtifactColorManagement`, `ArtifactColorGrading`。
- **確認できた事実:** 対応する専用targetへ登録済みの2 implementation unitを `.cpp` に移し、common manifestとtarget source pathsを更新。upstream clang++ 23.1.4 / Windows MSVC ABIで両方のP1689 scan、object compile、static archive再生成が成功した。最終warm sccacheは2 hits / 0 missesで、既存cache errors / compilation failures集計は増加しなかった。実装本文はGit HEADと一致しLF-only。
- **価値または懸念:** 小さいColor専用targetでも既存Ninja build treeがOpenUSDを含む3,181 actionsへ膨らむ。対象だけの正規CMake module map compileとarchive確認はできるが、target全体の依存build / app linkの証拠にはならない。
- **次に確認すること:** CMake/Ninjaが全依存を管理するclean build treeでtarget全体の最小依存buildを実行し、直接object/archive検証との差を確かめる。
## 2026-10-24: Playback service pair compiles with generated module maps

- **関連:** `Artifact/src/Service/ArtifactPlaybackService.cpp`、`ArtifactPlaybackShortcuts.cpp`、upstream clang++ 23.1.4 / Windows MSVC ABI。
- **確認できた事実:** 2つの implementation unitを `.cppm` から `.cpp` に移し、source manifestを更新した。両public interfaceと両implementationを個別にコンパイルできた。CMake configure / generateも成功。
- **価値または懸念:** 直接DDIだけで作った限定dyndep mapはimport依存閉包が足りず失敗したが、実装の問題ではなく検証mapの不備だった。通常生成されたCMake module mapでは4件成功。sccache warm hit、clean build、アプリ統合linkは未確認。
- **次に確認すること:** 次のsccache記録時にwarm compile結果を取得し、対象2 sourceの本文一致・LFを監査する。

## 2026-10-24: Composition PlaybackController migration is blocked by a module cycle

- **関連:** `Artifact/src/Composition/ArtifactCompositionPlaybackController.cppm`、`Artifact/include/Playback/ArtifactPlaybackEngine.ixx`、`ArtifactPlaybackAudioDiagnostics`。
- **確認できた事実:** 現行ツリーには `ArtifactCompositionPlaybackController.cppm` が残り、対応する `.cpp` は存在しない。`ArtifactPlaybackAudioDiagnostics` は `Artifact.Playback.Engine` interfaceで定義され、同interfaceは `Artifact.Composition.PlaybackController` をimportする。ImplementationからEngineをimportする案は循環依存になる。
- **推測・未検証:** Diagnostics型の所有moduleを分ければ依存を解ける可能性があるが、分離先・利用範囲・API影響は未検証。
- **価値または懸念:** Clang移行時に出た直接import不足を単純に追加すると module cycleを作るため、このunitを `.cpp` 化済みとして数えない。現行の変更差分は先頭BOM除去だけで、module importは追加されていない。
- **次に確認すること:** Transport APIの責務と型の利用箇所を調べ、共有診断型のmodule分離が適切か別途判断してから再移行する。

## 2026-10-24: ColorScience and OCIO implementation pair compile as `.cpp`

- **関連:** `Artifact/src/Color/ArtifactColorScienceManager.cpp`, `ArtifactOCIOManager.cpp`, `Artifact.Event.Types`, upstream clang++ 23.1.4。
- **確認できた事実:** 2つの登録済み implementation unitを `.cppm` から `.cpp` に移し、Artifact source manifestとCMake cycle-pinning pathsを更新。public interfaceのcompileでPCMを作成後、両sourceのP1689 scanとobject compileが成功した。限定CMake dyndep mapには `Artifact.Event.Types` providerを含め、OCIOがColorScience interfaceに依存する順序も確認した。sccache warm compileは2 hits / 0 missesで、累積errors/failuresは増えなかった。source bodiesはHEADと一致しLF-only。
- **価値または懸念:** cycle ownershipを保ったまま、実装拡張子だけを移せることをClangで確認できた。これはArtifactAppRuntime全体のdyndep build / app link成功を示さない。
- **次に確認すること:** clean Clang build treeでArtifactAppRuntimeの通常Ninja buildを再開し、この pair を含むmodule graphからリンクまで通す。

## 2026-10-24: Artifact source manifest membership does not prove final target membership

- **関連:** `Artifact/cmake/ArtifactSources.cmake`、`Artifact/CMakeLists.txt`、`Artifact/src/Effects/Rasterizer/{GlowEffect,KaleidoscopeEffect}.cppm`。
- **確認できた事実:** 2つのlegacy Rasterizer sourceは共通manifestに記載されているが、CMakeLists側でcanonical Glow / Kaleidoscope unitsとmodule nameが重複すると明記され、最終target listsから除外されている。
- **推測・未検証:** 同じ形式の除外・再配分が他source familyにも存在する範囲は未監査。
- **価値または懸念:** 拡張子移行の候補はmanifest登録だけで選ばず、変数の再配分と `list(REMOVE_ITEM)` 適用後の最終target所属まで追う必要がある。除外されたlegacy fileを移してもClangの実際のbuild path改善にならない。
- **次に確認すること:** 次の候補選定で各implementationの最終target所属と専用target pinningを確認し、buildされるtranslation unitだけを移行する。


## 2026-10-24: Project module implementation uses an indirectly exposed item type

- **関連:** `Artifact/src/Project/ArtifactProjectStatistics.cpp`、`Artifact.Project.Items`、Clang module map。
- **確認できた事実:** Statistics implementation traverses `ProjectItem` and invokes its members, while its imports omitted the owning module `Artifact.Project.Items`. A preexisting module map happened to contain that provider; Clang direct-import checking still rejected the source. Adding the direct implementation import compiled successfully and the Items interface did not import Statistics.
- **価値または懸念:** Existing module maps can conceal missing direct imports, so compile success against a stale or broad map is weaker evidence than fresh P1689 scanning plus a real compile. In addition, a PCM may retain a deleted `.cppm` path after a source rename; regenerating that provider's build-tree DDI/map can validate the current source without changing child repository files.
- **次に確認すべきこと:** Continue migration verification with fresh source scans and check direct ownership imports; when stale PCM blocks a compile, establish whether its provider moved and rebuild only that provider before judging the candidate.


## 2026-10-24: Project importer module-map closure can exceed Clang memory

- **関連:** `Artifact/src/Project/ArtifactProjectImporter.cppm`、Project source modules、Clang 22.1.3 GNU frontend / Windows MSVC ABI。
- **確認できた事実:** Project Importer の実装sourceを一時的に `.cpp` としてスキャンした後、既存のCMake module mapには149件のPCM providerがあり、その不足PCMを揃えてからコンパイルを試した。ClangはAST読込約2.13GB（報告上99%）の後にfrontend crashし、objectを生成できなかった。この候補は `.cppm` と manifest登録へ戻した。
- **価値または懸念:** import countだけでなく、module mapの依存閉包とClang frontendの実メモリ負荷を候補選択に含める必要がある。クラッシュは拡張子変更のsource errorと断定できず、コンパイル成功とも扱えない。
- **次に確認すべきこと:** Project importer / managerの分割・依存閉包を別途切り分け、メモリを抑えた再現条件またはより小さい翻訳単位で再評価する。


## 2026-10-24: Missing module-map providers can be validated through interface compiles

- **関連:** Render Queue Widget implementation units、CMake/Ninja Clang module maps、P1689。
- **確認できた事実:** Render Queue Widget `.cppm` implementations had fresh maps that referenced their paired public interfaces plus Queue Job / Service providers. The existing PCM set was incomplete, but compiling those registered `.ixx` interfaces with their own generated maps produced the missing PCMs; the implementations then passed fresh P1689 scans and object compiles.
- **価値または懸念:** A missing PCM in a copied/stale implementation map does not by itself show a source incompatibility. Compile the owning public interface with its own provider map when the missing module is an interface, then verify the implementation against the complete closure. This avoids invoking the much larger application target for a narrow migration check.
- **次に確認すべきこと:** Prefer regenerating a complete CMake dyndep map from fresh DDIs when practical; use individual interface compiles only when provider ownership is clear and all its own imports are present.


## 2026-10-24: Frame cache entry retains two image representations

- **関連:** `Artifact/include/Render/ArtifactFrameCache.ixx`、`FrameCacheEntry`。
- **確認できた事実:** `FrameCacheEntry` declares both a `QImage image` and `std::vector<float> pixels` with width/height metadata. The implementation unit was moved to `.cpp` without changing the declaration or cache behavior.
- **推測・未検証:** If producers populate both representations for the same frame, this could duplicate memory use; whether both are simultaneously populated, needed by callers, or touched on the hot path has not been traced.
- **価値または懸念:** The hot-path rules discourage repeated image allocation/conversion, so any future cache cleanup needs caller/producer evidence and must preserve explicit CPU/GPU transfer boundaries.
- **次に確認すべきこと:** Trace FrameCache producers and consumers, determine whether `QImage` and float storage coexist per entry and at what frequency, then profile memory and allocation before proposing a representation change.


## 2026-10-24: Migrated module providers leave stale source paths in downstream PCMs

- **関連:** ArtifactCore Physics modules and Artifact Layer PhysicsBridge compilation under Clang 22.1.3.
- **確認できた事実:** Current ArtifactCore source manifest lists Physics providers such as `SoftBodySolver.cpp`, `ClothSolver3D.cpp`, `FluidSolver2D.cpp`, `Physics2D.cpp`, `MpmSolver2D.cpp`, and `PhysicsSystem.cpp`, while preexisting PCMs still attempted to open former `.cppm` paths. Fresh scans of current `.cpp` sources succeeded. Rebuilding the affected provider objects/PCMs in dependency order allowed Layer PhysicsBridge and FluidRuntimeState objects to compile; no ArtifactCore source or manifest was changed in this work.
- **価値または懸念:** A complete file-based PCM map does not prove its serialized dependency state matches current source paths. When a module provider was renamed, refresh the provider and its migrated prerequisites before attributing downstream failures to the consumer source.
- **次に確認すべきこと:** For broader validation, regenerate dyndep maps from fresh DDIs in a clean build tree, then compare with this targeted provider refresh workflow.

## 2026-10-24: Large module implementations reach Clang frontend memory ceiling
- **関連:** `Artifact/src/Project/{ArtifactProject,ArtifactProjectImporter,ArtifactProjectManager}.cppm`、ArtifactAppRuntime、Clang 22.1.3 GNU frontend / Windows MSVC ABI、`Artifact/src/Widgets/LayerEditorContextMenu.cppm`。
- **確認できた事実:** 3実装を一時的に `.cpp` としてfresh scanした結果、CMake configure / generateとP1689 scanは成功したが、各個別compileがAST読込約2.1GB（診断上99%）でfrontend crashしobjectを生成できなかった。3ファイルとmanifestは `.cppm` に復元した。 LayerEditorContextMenuも一時 `.cpp` としてfresh P1689 scan後にobject compileを試したが、同じくAST読込約2.13GB（99%）でfrontend crashしたためmanifestとsource pathを `.cppm` に戻した。実装には `Memory.SharedPtr` の直接importを追加したが、crash前にこの修正を含むcompile成立は確認できていない。
- **価値または懸念:** source scanの成立だけでは実用的なcompile互換性を示せない。Projectの大きいmodule dependency closureがfrontend memoryを消費している可能性があるが、正確な原因は未確定。
- **次に確認すべきこと:** Project importer / managerのimport閉包とPCM再利用を調べ、小さな依存単位または修正版Clangで再評価する。

## 2026-10-24: VST module implementations expose a missing direct String import under Clang
- **関連:** `Artifact/src/VST/VSTHost.cpp`、`Core.ArtifactString`、Clang 22.1.3 / Windows MSVC ABI。
- **確認できた事実:** `VSTHost.cpp` が `ArtifactCore::String` を使う一方で直接importしていなかったため、implementation object compileでClangが不足を報告した。実装 `.cpp` に `import Core.ArtifactString;` を追加すると fresh P1689 scanとobject compileが成功した。
- **価値または懸念:** VST module pairでも、既存module mapにPCMが含まれているだけでは直接import要件を満たさない。所有module importを実装側に置くことでpublic interfaceを広げずに済む。
- **次に確認すべきこと:** 他の移行sourceも実compileで不足が示された場合に限り、実装側の直接importを追加して検証する。
## 2026-10-24: Asset Browser undo helper needs its owning Project.Items module
- **関連:** `Artifact/src/Widgets/Asset/ArtifactAssetBrowserUndoCommands.cpp`、`Artifact.Project.Items`、Clang 22.1.3 / Windows MSVC ABI。
- **確認できた事実:** Undo helper implementation uses `ProjectItem*` while importing `Artifact.Project`; Clang rejected the indirect visibility and required `Artifact.Project.Items`. Adding that direct import to the implementation source allowed fresh P1689 scan and object compilation to succeed.
- **価値または懸念:** Project type ownership remains clear when implementation imports the module that declares the type; this avoids widening the widget interface.
- **次に確認すべきこと:** Add direct imports only when the compiler identifies a missing owning module, then verify no cycle and recompile.
## 2026-10-24: Color Science Panel compile is gated by a separate interface error
- **関連:** `Artifact/src/Widgets/Color/ArtifactColorSciencePanel.cppm`、`Artifact/include/Widgets/Render/Artifact3DGizmo.ixx`、Clang 22.1.3 / Windows MSVC ABI。
- **確認できた事実:** Science Panelを一時的に `.cpp` としてfresh P1689 scanしたが、implementation objectに必要な `Artifact.Widgets.CompositionEditor.pcm` が欠けていた。依存PCM生成のため既存interfaceを個別compileしたところ、`Artifact3DGizmo.ixx` のline 70でnested `export` declaration errorが発生し、その上流PCM chainを揃えられなかった。Panelは `.cppm` とsource manifest entryへ戻した。
- **価値または懸念:** この検証結果はScience Panel implementationのClang非互換を示すものではなく、source scanだけでmigration成功とも判断できない。依存interface側の独立したClang errorがobject-level検証を止めている。
- **次に確認すべきこと:** `Artifact3DGizmo.ixx` のexport nestingを独立して精査し、修正後にScience Panelのmodule closureを再compileする。
## 2026-10-24: OFX module required exported forward declarations and a direct EffectContext import
- **関連:** `Artifact/src/Effects/Ofx/ArtifactOfxHost.cpp`、`ArtifactOfxEffectImpl.cpp`、Clang 22.1.3 / Windows MSVC ABI。
- **確認できた事実:** OFX Host compile rejected `OfxPluginDescriptor` and `ArtifactOfxHost` because non-exported forward declarations preceded exported definitions in the same module. Marking those forward declarations `export` made the host implementation compile. EffectImpl then required a direct import of the owning module `Artifact.Effect.Context` for `EffectContext`; adding it to the implementation allowed fresh scan and compile.
- **価値または懸念:** Module implementation migration can expose declaration linkage inconsistencies and transitive type visibility hidden by existing build state. Both fixes are confined to the OFX module sources.
- **次に確認すべきこと:** Continue verifying exported declaration consistency and direct owning-module imports through fresh scan and individual object compilation.
## 2026-10-24: Clang implementations need direct owners and current provider PCMs
- **関連:** `Artifact/src/Tool/{ArtifactCameraTrackerTool,ArtifactPointTrackerTool}.cpp`、`Artifact/src/Widgets/LayerEditorShapeInputController.cpp`、`LayerEditorShapeHoverController.cpp`、`LayerEditorShapePressInteractionController.cpp`、`LayerEditorShapeCommands.cpp`、`LayerEditorShapeEditSession.cpp`、`LayerEditorShapeParameterController.cpp`、`Tracking.CameraTracker`、`Artifact.Layer.InitParams`、`Memory.SharedPtr`、`Artifact/src/Widgets/LayerEditorShapeOverlay.cpp`。
- **確認できた事実:** CameraTracker implementationが `ArtifactLayerInitParams` を間接可視のまま使っていたためClangが `Artifact.Layer.InitParams` を要求した。PointTrackerでは `makeShared` の所有元 `Memory.SharedPtr` が不足していた。両implementationへの直接import追加でfresh scan / object compileが成功した。また古いArtifactCore CameraTracker PCMは削除済み `.cppm` sourceを参照していたため、ArtifactCoreの現在の `.cpp` sourceをbuild tree内で個別scan / compileしPCMを更新してから利用した。ArtifactCoreソースは変更していない。LayerEditorShapeInputControllerでも `ArtifactCore::dynamicPointerCast` の所有元 `Memory.SharedPtr` を直接importしておらず、implementationへのimport追加後にfresh scan / compileが成功した。 ShapeCommands、ShapeEditSession、ShapeParameterControllerでも `ArtifactCore::SharedPtr` / `dynamicPointerCast` の所有元 `Memory.SharedPtr` を実装からimportしておらず、直接importを足した後にそれぞれfresh scan / compileが通った。 続くLayer Editor hover / press interaction群では、ShapeHoverControllerに `Memory.SharedPtr` の直接import、ShapePressInteractionControllerにglobal module fragment側の `#include <QPointF>` が必要とClangが指摘し、追加後にfresh scan / compileが成功した。 ShapeOverlayでは `ArtifactCore::Collider2DShape` のowner `Physics.Collider2DEdit` をimportしておらず、implementationに直接importを追加してfresh scan / compileが成功した。
- **価値または懸念:** module-mapの古さとClangのsource import不足は別の問題であり、どちらも個別に解消してからcompile結果を判断する必要がある。
- **次に確認すべきこと:** 同様のmissing source pathでは所有sourceの現在pathとtargetを照合してPCMをbuild tree内で更新し、missing declarationは直接importで明示する。

## 2026-10-24: Playback ControlWidget verification reaches Application Manager memory ceiling
- **関連:** `Artifact/src/Widgets/Control/ArtifactPlaybackControlWidget.cppm`、`Artifact/include/Application/ArtifactApplicationManager.ixx`、Clang 22.1.3 / Windows MSVC ABI。
- **確認できた事実:** Playback ControlWidgetのsourceを一時 `.cpp` としてfresh scanできたが、個別compileに必要な `Artifact.Application.Manager.pcm` を作るためApplicationManager interfaceを個別compileした際、Clang frontendはAST約2.15GB（診断上99%）でcrashした。ControlWidgetは `.cppm` とmanifest entryへ戻した。Playback Engine implementation単体は別途 `.cpp` 化後にfresh scan / object compileに成功している。
- **価値または懸念:** UI implementation sourceのcompile可否は、依存interface自身のfrontend memory上限に制約される場合がある。今回ControlWidget本文はcompile到達前で、Clang互換性を判定できていない。
- **次に確認すべきこと:** Application Managerのmodule closure / memory負荷を独立に分割評価し、必要PCMが揃う条件を確認後に再試行する。
## 2026-10-24: Artifact primary module interfaces must stay `.cppm` under the current source classifier
- **関連:** `Artifact/src/Widgets/ArtifactProjectResponsiveLayout.cppm`、`ArtifactProjectItemPresentation.cppm`、`Artifact/cmake/ArtifactSources.cmake`。
- **確認できた事実:** 2 sourceは `export module ...;` を宣言し、対応する別 `.ixx` interfaceを持たないprimary module interfaceだった。Artifactの現在のCMake source分類は `.cppm` を内容走査してinterface / implementationを判別する一方、`.cpp` はimplementationとして扱う。拡張子変更とmanifest更新だけではinterfaceを維持できないため、両sourceをコンパイル前に元の `.cppm` pathへ戻し、CMakeを再生成した。
- **価値または懸念:** `.cppm` → `.cpp` の候補はファイル名やtarget登録だけでなく、module declarationとinterface ownershipで判定する。`export module` を持つsourceを現在の規則下で単純移動するとimporter向けinterfaceを失わせる可能性がある。
- **次に確認すべきこと:** 各候補のsource declarationが `module X;` のimplementation unitで、public interfaceが別に登録されていることを確認してから移行する。

## 2026-10-24: Ninja object target can expand into a large stale module dependency graph
- **関連:** `Artifact/src/Widgets/LayerEditorInteractionStateController.cpp`、`out/build/x64-Debug-ClangXX-MsvcAbi`、Ninja / Clang modules。
- **確認できた事実:** CMake再生成後に当該object pathをNinjaへ指定すると5267件の依存buildが計画され、40件進んだ時点で停止した。既存の `LayerEditorInteractionStateController.cppm.obj.modmap` をbuild tree内で新しい拡張子pathへコピーして行った直接compileは、import対象の `Artifact.Widgets.LayerEditor.MaskMoveController.pcm` が欠落していたためsccacheが拒否した。fresh P1689 scan自体は成功した。 また、`ArtifactTextGlyphShaderSources.cpp` のfocused runtime targetでdyndepだけを要求した場合も985件を計画したため116件で停止したが、対象source/interfaceのP1689 scanと、import closureに合わせてbuild tree内で生成したmodule mapによるinterface PCM / implementation objectの直接compileは成功した。warm compileでsccache hitが1件増えた。その後sourceをLFへ正規化して再compileした際は入力hash変更によるmissが1件増え、直後のrepeatではhitが1件増加、errorsは不変だった。 さらに `GridRenderer.cpp` と `ViewportOverlay.cpp` でも、既存module mapの全PCMが存在することを確認後、GridRenderer interface PCMと両implementation objectを直接Clang compileできた。fresh P1689 scanも両sourceで成功し、warm repeatはsccache hit +2 / misses不変だった。
- **価値または懸念:** object path指定だけではmodule provider群や外部targetの再buildを避けられない場合がある。依存PCMが揃っているかを確認せずにNinja target buildを行うと、意図した単体検証を超える大きな再buildが始まる。
- **次に確認すべきこと:** 既存build treeで対象module providersのPCMとmodmapを確認し、必要依存を限定してからobject compileする。全target buildを単体compileの代わりに使わない。

## 2026-10-24: Qt pointer forward declarations belong to the global module fragment
- **関連:** `Artifact/include/Widgets/ArtifactObjectPickerDialog.ixx`、`Artifact/src/Widgets/ArtifactObjectPickerDialog.cpp`、Clang 22.1.3。
- **確認できた事実:** Clangでinterface compileした際、`QDialog` headerがglobal moduleに宣言済みの `QPushButton` をmodule purviewでも再宣言しておりエラーになった。またmodule purviewの `QTreeWidget` / `QLineEdit` forward declarationsは、implementation global module fragmentのQt headersが定義する型と同一視されず、implementationのメンバーアクセスがincomplete typeで失敗した。重複 `QPushButton` 宣言を除き、pointer-only forward declarationsをglobal module fragmentへ移した後にinterface PCMとimplementation objectがcompile成功した。
- **価値または懸念:** public module interfaceでのみポインタ保持するQt型は、module purviewで独自宣言せずglobal module fragmentの宣言を共有すると、implementation側の直接Qt includeと同じ型宣言に揃う。
- **次に確認すべきこと:** 他のpublic `.ixx` にmodule purview内のQt forward declarationsがあれば、該当型がmodule境界を越えてimplementationで使われる場合にClangで確認する。

## 2026-10-24: AnimationTimeline implementation Clang crash identifies an unresolved PCM/source-location failure
- **関連:** `Artifact/src/Widgets/ArtifactAnimationTimelineWidget.cppm`、Clang 22.1.3、`out/build/x64-Debug-ClangXX-MsvcAbi`。
- **確認できた事実:** sourceのfresh P1689 scanとpublic interface PCM compileは成功した一方、implementation compileは2回とも `malformed or corrupted precompiled file: 'ran out of source locations'` を報告してClang frontend crashした。対象sourceの `.cpp` 移行は取り消した。build mapが参照するPCMファイルの存在は事前確認済みだが、どのPCMの内容 / source-location tableが原因かは特定できていない。
- **価値または懸念:** PCMファイルが存在することだけではClang frontendが正常に読み込める保証にならない。sourceのcompile診断や拡張子起因と断定せず、imported PCMの個別整合性を分けて調査する必要がある。
- **次に確認すべきこと:** import closureを限定し、AnimationTimelineが読むPCMを個別再生成 / 読み込み検証してから、Clang compileを再試行する。

## 2026-10-24: Clang module interface PCM size varies substantially by UI surface
- **関連:** `Artifact/include/Widgets/ArtifactUndoHistoryWidget.ixx`、`Artifact.Widgets.UndoHistoryWidget.pcm`、Clang 22.1.3。
- **確認できた事実:** interfaceの個別compileは成功し、Clang build treeに生成されたPCMは86,293,516 bytesだった。対応するimplementation objectもcompile成功した。
- **価値または懸念:** source行数からmodule PCM容量を推測できない。Clang build容量を整理するときは一部の小sourceだけで全モジュールのPCM規模を代表させない。
- **次に確認すべきこと:** build tree容量を整理する場合、対象構成を特定した上でPCM / object / dependency cacheの実サイズを区別して集計する。

## 2026-10-24: Project Manager interface PCM compile has a high Clang memory peak
- **関連:** `Artifact/include/Project/ArtifactProjectManager.ixx`、`Artifact.Project.Manager.pcm`、Clang 23.1.4 / Windows MSVC ABI。
- **確認できた事実:** PCM欠落を補うためinterfaceを個別fresh scan / compileしたところ、Clang processのworking setは約7.2 GiB、空き物理RAMは約3.7 GiBまで下がったためcompileを中断した。`Artifact.Project.Manager.pcm` は出力されず、候補のHierarchy implementation sourceは未変更である。
- **価値または懸念:** 依存interfaceが存在しない場合の単体検証は、少数の対象sourceだけでも大きなRAMを要することがある。作業機全体の空きメモリを見ずに続けると、周辺プロセスまでメモリ圧迫を受ける。
- **次に確認すべきこと:** 大きなUI interfaceのPCMが欠落した場合は、先にメモリ上限と空きRAMを確認し、既存PCMを再利用できる候補を優先する。

## 2026-10-24: ProjectImporter implementation hits Clang's AST source-location limit
- **関連:** `Artifact/src/Project/ArtifactProjectImporter.cppm`、Clang 23.1.4 / Windows MSVC ABI。
- **確認できた事実:** sourceを一時 `.cpp` に移した後、fresh P1689 scanは成功したが、implementation compileがAST file読み込み時に約2.13 GiB（診断上99%）でfrontend crash / exit 70した。extensionとmanifest entryは `.cppm` へ戻した。失敗はimplementationのsource diagnosticではなく、個別compile・リンク・runtimeには到達していない。
- **価値または懸念:** ProjectImporterの現状module closureはClang 23のAST source-location容量上限に達し、拡張子移行の検証が成立しない。問題PCMや依存を絞らず再試行しても同じ失敗を繰り返す可能性が高い。
- **次に確認すべきこと:** ASTを膨らませているPCM依存を個別に調べ、import closureまたはmodule partitioningを見直せる根拠が得られるまで `.cppm` のままにする。

## 2026-10-24: Inspector effect catalog leaves a stage enumerator unhandled
- **関連:** `Artifact/src/Widgets/InspectorEffectCatalog.cpp`、`ArtifactEffectStage`。
- **確認できた事実:** Clang 23.1.4でInspectorEffectCatalog implementationを個別compileした際、`switch (stage)` に `ArtifactEffectStage::PreProcess` caseがないという `-Wswitch` warningが出た。拡張子移行ではこのコードを変更しておらず、warningが実行時の欠落動作に結び付くかは未検証。
- **価値または懸念:** effect stage一覧とcatalogの分類がずれている可能性を確認できる。安易にcaseを追加するとUI分類を変えるため、現状の責務・表示方針を調べず修正しない。
- **次に確認すべきこと:** `ArtifactEffectStage::PreProcess` の利用元とInspectorのstage表示要件を確認し、意図的な除外か欠落かを判定する。

## 2026-10-24: UI source migrations need per-source validation and provider ordering

- **関連:** `Artifact/src/Widgets/Dialog/ArtifactScreenshotExportDialog.cpp`、`ArtifactResolutionRemapDialog.cpp`、`CreateNoiseLayerDialog.cpp`、`FloatColorPickerHooks.cpp`、`CreateLightLayerDialog.cpp`、`ArtifactImportAssetsDialog.cpp`、`Artifact/src/Widgets/AssetThumbnailPipeline.cpp`、`CreateCameraLayerDialog.cppm`、各 `.ixx` interface、Clang 23.1.4 / P1689 / sccache。
- **確認できた事実:** ScreenshotExport / ResolutionRemap / CreateNoise / FloatColorPickerHooks / CreateLight / AssetThumbnailPipeline / ImportAssetsの7 implementationは、public interfaceとimplementationのfresh P1689 scan、interface PCM compile、implementation object compileが成功した。Hooksを先にcompileした後、Hooks PCMをimportするCreateLightが成功し、AssetThumbnailPipelineを先にcompileした後、同PCMをimportするImportAssetsも成功した。CreateCameraはscanとinterface PCM compile後のimplementation compileでClang frontendがexit 70のcrashとなり、sourceとmanifestを `.cppm` に戻した。
- **価値または懸念:** UI近接機能を一群として監査しても、成立性はsource単位で異なる。consumerのrequiresにproviderが現れる場合、provider PCMを個別に先行生成すると不足依存を特定して段階検証できる。crashした候補は成功群に含めず `.cppm` に戻す。
- **次に確認すべきこと:** 各public interfaceと直接module importを確認し、依存PCMが欠ける候補はproviderを含む個別評価に回す。Camera crashの原因は未特定のため、原因調査なしに再試行を繰り返さない。

## 2026-10-24: Qt event types used by module implementations need direct global-fragment includes

- **関連:** `Artifact/src/Widgets/Render/ArtifactLayerEditorPanel.cpp`、`Artifact/include/Widgets/Render/ArtifactLayerEditorPanel.ixx`、`Artifact/src/Widgets/StyleSurface.cpp`、Clang 23.1.4 / Windows MSVC ABI。
- **確認できた事実:** ArtifactLayerEditorPanel implementationでは`QCloseEvent*`、StyleSurface implementationでは`QPaintEvent*`を使っていたが、各sourceのglobal module fragmentに該当headerがなく、Clang compileがunknown typeで失敗した。それぞれglobal module fragmentへ`#include <QCloseEvent>` / `#include <QPaintEvent>`を追加した後、fresh P1689 scanとimplementation compileが成功した。
- **価値または懸念:** Qtイベント型は他Qt headers経由で見えていると推測せず、実装が使う型をsource側で直接includeするとClangのmodule境界で依存漏れを検出できる。追加includeはglobal module fragmentに置き、module purviewには置かない。
- **次に確認すべきこと:** 移行候補のmodule implementationでイベント型のメンバー参照や完全型利用があれば、Clang診断に基づいて直接includeを確認する。
## 2026-10-24: Camera layer implementation has a self-contained public module boundary

- **関連:** `Artifact/src/Layer/ArtifactCameraLayer.cppm` → `.cpp`、`Artifact/include/Layer/ArtifactCameraLayer.ixx`、`Artifact/cmake/ArtifactSources.cmake`
- **確認できた事実:** CameraLayer interfaceは `Artifact.Layer.Abstract` をimportして公開APIを定義している。実装単位は同じmoduleのimplementation unitで、追加の直接CMake APP_IMPL entryはなく、source manifestにだけ登録されていた。`.cppm` を `.cpp` に変更しmanifestも同期した。ビルドは禁止されていたためコンパイルと生成後のtarget分類は未確認。
- **価値または懸念:** 追加のpublic importやCMake pinを増やさず一実装単位ずつ拡張子移行できる候補だった。実ビルドまでは安全性が確定していない。
- **次に確認すべきこと:** CMake configure後のsource分類とClang MSVC-ABIでのimplementation compileを確認する。
## 2026-10-24: A narrow Ninja object target can still trigger a large regenerated dependency graph

- **関連:** `Artifact/src/Layer/ArtifactNLETransitionBridge.cpp`、`Artifact/cmake/ArtifactSources.cmake`、`V:\ArtifactStudioBuilds\x64-Debug-ClangXX23-MsvcAbi`
- **確認できた事実:** CMake configure/generate succeeded after migrating the NLE bridge source path. A direct Clang 23.1.4 GNU frontend compile using the existing module map completed successfully. Running Ninja for only the bridge object then reported 6632 actions because of regenerated transitive dependencies and began OpenUSD generation/compilation; it was interrupted after roughly 112 actions to limit additional disk/build work.
- **価値または懸念:** A one-object Ninja target is not necessarily a low-cost verification after CMake regeneration. Inspect its planned action count before executing; isolated compile with an existing module map provides narrower evidence but does not validate a fresh dependency scan or complete build-graph closure.
- **次に確認すべきこと:** For future migrations, use fresh scan and compile-only checks where module maps exist, and inspect Ninja's dry-run/plan before starting a target that may fan out. Confirm whether regenerated dependency artifacts are expected before another broad build.
## 2026-10-24: Fresh P1689 scans can validate migrated sources without running their build closure

- **関連:** `Artifact/src/Layer/ArtifactNLETransitionBridge.cpp`、`Artifact/src/Effects/ImageMorph/ImageMorphEffect.cpp`、`Artifact/src/Widgets/Coordinate/ArtifactPerformanceHUD.cpp`、`Artifact/src/Widgets/WebUI/ArtifactReportDashboard.cpp`
- **確認できた事実:** After CMake regeneration, invoking each generated `clang-scan-deps -format=p1689` rule directly succeeded and emitted the `.cpp.obj.ddi` dependency record. The isolated implementation compile succeeded for NLETransitionBridge. The other three compile attempts stopped before Clang compilation because their module maps referenced missing interface PCM files. Running the Ninja object target expanded to 6632 actions and was interrupted after about 112 actions.
- **価値または懸念:** Fresh dependency scanning is a useful narrow check of module source classification, while it does not prove implementation compilation. A direct object target can fan out into a much larger generated dependency graph.
- **次に確認すべきこと:** Build only the required interface PCM/module dependencies in a bounded way before retrying the three isolated implementation compiles; inspect Ninja's planned action count first.
## 2026-10-24: Interface PCM preparation can isolate source migration, but interface cost is not predicted by import count

- **関連:** `Artifact/include/Widgets/CollabPresenceWidget.ixx`、`Artifact/include/Widgets/ArtifactLooksPresetBrowser.ixx`、`Artifact/include/Widgets/ArtifactCurveEditorWidget.ixx`、`Artifact/include/Widgets/ArtifactExpressionCopilotWidget.ixx`、`Artifact/include/Widgets/Render/ArtifactPointTrackerGizmo.ixx`、`Artifact/include/Service/ArtifactAudioService.ixx`、Clang 23.1.4 / Windows MSVC ABI
- **確認できた事実:** CollabPresenceWidget、ArtifactLooksPresetBrowser、AIChatWidget、ArtifactColorPaletteWidget、CurveEditorWidgetではinterface PCMを単独compile後、implementation fresh scan / compileまで進められた（ExpressionCopilotWidgetはinterfaceは成功したがimplementation frontendはsignal終了）。ArtifactAudioServiceはinterface importが3件でもfrontend signal終了。PointTrackerGizmoはinterface compile中にClang working setが約4.2 GiBとなり、空きメモリ約1.48 GiBで中断した。
- **価値または懸念:** interface import数だけではPCM生成コストやimplementation compile成功を予測できない。各段階を別々に測定し、メモリ圧迫時は依存を広げず停止する必要がある。
- **次に確認すべきこと:** interfaceのimport closureと実測memoryを候補ごとに確認し、frontend signal/crashしたsourceは原因を理解するまで拡張子を戻す。
## 2026-10-25: Point tracker gizmo migration remains unvalidated at interface boundary

- **関連:** `Artifact/src/Widgets/Render/ArtifactPointTrackerGizmo.cppm` → `.cpp`、`Artifact/include/Widgets/Render/ArtifactPointTrackerGizmo.ixx`
- **確認できた事実:** 実装単位のmodule declaration/import群を保ったまま拡張子とsource manifest entryだけを変更した。以前のClang試行はimplementationではなくinterface PCM生成中、working set約4.2 GiB・空き約1.48 GiBで中断されている。
- **価値または懸念:** 描画用scratch bufferの挙動は変更していないが、現在の変更だけではClang source分類・compile成立性は検証されていない。
- **次に確認すべきこと:** 利用可能なメモリに余裕がある環境でinterface PCMを作成した後、fresh scanとimplementation compileを個別に行う。ビルド実行は禁止中のため今回は実施しない。
## 2026-10-25: Render module-reference inventory has its own implementation paths

- **関連:** `Artifact/cmake/ArtifactRenderModuleReferences.cmake`、`Artifact/cmake/ArtifactSources.cmake`、`Artifact/src/Render/DiligentBindlessSubmitter.cpp`
- **確認できた事実:** Render module reference inventory records implementation paths separately from the main Artifact source manifest. The BindlessSubmitter extension migration required updating both records; its named module declaration and imports remained unchanged.
- **価値または懸念:** Future renderer implementation migrations can leave the dependency inventory stale even when the main source list is correct. This inventory is an additional static consistency surface.
- **次に確認すべきこと:** For each renderer migration, check both source manifest and render reference map, then use the project's reference checker when an authorized build/verification is available.
## 2026-10-25: Some registered .cppm files are ordinary C++ translation units

- **関連:** `Artifact/src/Render/DiligentUploadCoordinator.cppm` → `.cpp`、`Artifact/src/Render/DiligentUploadCoordinator.hpp`、`Artifact/cmake/ArtifactSources.cmake`
- **確認できた事実:** このmanifest登録sourceにはmodule declaration/importがなく、implementationは通常のC++翻訳単位だった。拡張子とsource manifestだけを変更し、upload queue / synchronization本文を保持した。
- **価値または懸念:** `.cppm` inventoryをmodule declarationの有無で分類すると、通常sourceにmodule compiler modeを課しているケースも識別できる。GPU upload pathなので動作・性能検証は行っていない。
- **次に確認すべきこと:** 他のmanifest登録`.cppm`も、exported interface / named module implementation / module-free translation unitに分け、分類に沿ったscan設定を確認する。
## 2026-10-25: Artifact CMakeLists was replaced by migration notes in the working tree

- **関連:** root `CMakeLists.txt` (`add_subdirectory(Artifact)`), `Artifact/CMakeLists.txt`, `Artifact/cmake/ArtifactSources.cmake`, `docs/planned/MILESTONE_CPPM_IMPLEMENTATION_TO_CPP_CLANG_SCCACHE_2026-10-21.md`
- **確認できた事実:** Artifact working-tree `CMakeLists.txt` began with Markdown and held a migration log, while the root CMake adds that directory as a subdirectory. The repository version is executable CMake and contains compiler-specific module source classification. The migration log was already preserved in the planned milestone; its one distinct manifest-comment note was semantically covered there. Restored the repository CMake script and updated 148 distinct active CMake path references to current migrated sources.
- **価値または懸念:** With Markdown at this path, root CMake could not evaluate the Artifact subdirectory, so the Clang source scan rule was not reachable. The restored script statically identifies implementation `.cpp` module sources and enables Clang P1689 scanning; this is not proof that configure or compile succeeds.
- **次に確認すべきこと:** Once CMake execution is explicitly authorized, configure/generate and inspect the generated scan rules for both upstream clang++ and clang-cl; no such commands were run in this change.
## 2026-10-25: Artifact `.cpp` Clang classification must include import-only consumers

- **関連:** `Artifact/CMakeLists.txt` source-classification loop、`Artifact/cmake/ArtifactSources.cmake`、six import-only sources in `Artifact/src`
- **確認できた事実:** The restored classifier selected migrated `.cpp` sources only when they had a named `module` declaration. Static inspection found six `ARTIFACT_APP_IMPL_SOURCES` `.cpp` files with `import` statements and no named module declaration.
- **価値または懸念:** These translation units need dependency scanning despite not defining a named module. The classifier now includes either named module declarations or import declarations; CMake configure and generated scan rules are unverified.
- **次に確認すべきこと:** With explicit authorization, configure under GNU-style Clang and clang-cl and inspect generated scan rules for all six import-only sources.

## 2026-10-25: Extension migrations must normalize inherited CRLF
- **Related:** `Artifact/src/Layer/ArtifactAbstractLayerLayoutRouting.cppm` → `.cpp` and `Artifact/cmake/ArtifactSources.cmake`
- **Confirmed fact:** The migrated source retained CRLF when its text was copied to the new `.cpp`; a follow-up check detected and normalized it to LF. The module declaration and source manifest path were otherwise preserved.
- **Value or concern:** A text-preserving rename alone does not guarantee the repository's LF rule when a source has pre-existing CRLF. Each migration should explicitly normalize and verify line endings.
- **Next check:** Keep the CRLF scan in the static migration checklist; Clang scanning and compilation remain unverified.
## 2026-10-25: Clang scan classification must include direct-target module implementations
- **Related:** `Artifact/src/Render/ArtifactTextGlyphSubmitter.cppm`, its private runtime targets in `Artifact/CMakeLists.txt`, and the Clang source-classification loop
- **Confirmed fact:** `ArtifactTextGlyphSubmitter.cppm` was added directly with `target_sources()` rather than through `ArtifactSources.cmake` or an `_IMPL` variable, so the generic Clang scan loop did not include it.
- **Value or concern:** Migrating this implementation to `.cpp` requires an explicit classifier entry; otherwise Clang may compile the import without the intended CMake P1689 scan/dependency ordering.
- **Next check:** When CMake execution is authorized, verify both private targets receive module dependency scans under GNU-style Clang and clang-cl.
## 2026-10-25: Source-tree `.cppm` inventory includes unregistered implementation files
- **Related:** `Artifact/src/Color/ArtifactColorWheels.cppm`, `Effect/ArtifactStabilizer.cppm`, `Preview/ArtifactTimelineClock.cppm`, `Render/ArtifactTextRenderTarget.cppm`
- **Confirmed fact:** Static source-tree inventory found these four named implementation units without entries in `Artifact/cmake/ArtifactSources.cmake` or references in `Artifact/CMakeLists.txt` / `Artifact/cmake/*.cmake`. `ArtifactTextRenderTarget.ixx` has inline method definitions, and its dedicated excluded runtime target intentionally registers only the interface; adding its `.cppm` could create duplicate definitions.
- **Value or concern:** Three files have no import sites or target registration; the TextRenderTarget source duplicates inline interface definitions. Renaming alone does not improve a compiled target, and registering them may change build scope or violate ODR.
- **Follow-up evidence:** Recent history shows `ArtifactStabilizer.cppm` was changed on 2026-10-10. The Command Palette was separately found behind `ARTIFACT_FEATURE_COMMAND_PALETTE`; its interface and implementation are now conditionally registered, and the option macro reaches `ArtifactAppRuntime`.
- **Next check:** Inspect module import sites and feature-specific target ownership for the three remaining unreferenced files to determine whether they are intentionally dormant, superseded, or missing an intended target registration.
## 2026-10-25: ArtifactWidgets still compiles unregistered `.cppm` files through its source glob
- **Related:** `ArtifactWidgets/CMakeLists.txt`, `src/Render/BackendSettingWidget.cppm`, `src/Common/AbstractWidget.cppm`, `src/Knob/KnobEditorWidget/KnobEditorWidget.cppm`, `src/Graphics/NodeWireGraphicItem.cppm`
- **Confirmed fact:** Unlike ArtifactCore's explicit source manifest, ArtifactWidgets uses `file(GLOB_RECURSE)` over `.cppm`; these four files had no literal CMake path references but were added to `WIDGETS_IMPL` through the glob. Their contents have no module declarations or imports.
- **Value or concern:** A filename-only CMake search would incorrectly classify these as inactive. Moving them to `.cpp` removes unnecessary module-extension handling while retaining them in the library source list through the explicit migrated-source list.
- **Next check:** When CMake execution is authorized, confirm the refreshed glob and explicit list produce each `.cpp` exactly once under both GNU-style Clang and clang-cl.## 2026-10-25: Remaining Artifact source-tree `.cppm` files are interfaces or unregistered
- **関連:** `Artifact/src` の `.cppm` inventory、`Artifact/cmake/ArtifactSources.cmake`、Clang source classifier
- **確認できた事実:** 現在の静的分類では Artifact の `.cppm` 47件のうち45件が `export module` の interface、残る2件は凍結対象。登録済み実装 `ArtifactAbstractLayerUtilities` と `ArtifactCompositionHostFrameSnapshot` は `.cpp` へ移行し、manifestを更新した。別途残る `ArtifactColorWheels` / `ArtifactTimelineClock` / `ArtifactStabilizer` / `ArtifactTextRenderTarget` はmanifest外であり、前3件は参照・登録なし、TextRenderTargetはinterfaceにinline定義が重複する。
- **価値または懸念:** source-tree上の `.cppm` 件数だけを移行残件数として扱うと、interfaceや未使用sourceを誤って実装対象に含める。登録状態と宣言種別を合わせて分類する必要がある。
- **次に確認すべきこと:** ArtifactCoreの残る凍結外 `.cppm` は同様にsource manifest、module map、CMake source propertyを照合し、Clang configure / P1689 scan / compileは実行許可後に確認する。
## 2026-10-25: Direct executable entry points with imports need explicit Clang scans
- **関連:** `Artifact/src/AppMainEntry.cpp`, `Artifact/CMakeLists.txt`, `ArtifactPr/src/main.cpp`, `ArtifactPr/src/main_cli.cpp`, `ArtifactPr/CMakeLists.txt`
- **確認できた事実:** Artifact main entry and ArtifactPr GUI/CLI entries import named modules but are attached directly to executable targets, outside their implementation source lists. The source classifiers therefore did not include them in their P1689 scan sets. Added these direct entries to the Clang scanning configuration without changing source behavior.
- **価値または懸念:** A migrated implementation can have correct scan configuration while its directly attached executable consumer still lacks dependency ordering. Direct target sources need separate static inventory from source manifests.
- **次に確認すべきこと:** When CMake execution is authorized, inspect generated P1689 rules for all three entry points and verify Ninja schedules their providers first.
## 2026-10-25: Module-aware sources may be owned outside explicit source manifests
- **関連:** `Artifact/CMakeLists.txt`, `ArtifactCore/CMakeLists.txt`, `Artifact/cmake/ArtifactSources.cmake`, `ArtifactCore/cmake/ArtifactCoreSources.cmake`
- **確認できた事実:** A static comparison found several module/import-aware `.cpp` files outside the primary manifests. Active examples are held by domain lists ending in `_IMPL` or `_MODULES`, while directly attached executable or smoke-target sources require explicit Clang scan classification. Other unregistered candidates have no active target reference.
- **価値または懸念:** Manifest-only audits can misclassify active split-target sources as dormant, while direct target sources may evade generic scanner collection. Clang scan audits must compare filesystem sources, manifest lists, split target lists, and direct `target_sources()` registrations.
- **次に確認すべきこと:** After authorized configure, inspect generated P1689 rules against the static source ownership map; do not infer scanner coverage only from manifest membership.
## 2026-10-25: Historical Clang logs must be reconciled with current source and dependency closure
- **関連:** `ArtifactCore/src/Time/RationalTime.cpp`, `include/Utils/ExplorerUtils.ixx`, `include/Frame/FrameSkipTracker.ixx`, existing `temp/*clang*.log`
- **確認できた事実:** Old diagnostics reported RationalTime being parsed as a module interface, nested exports in two interfaces, and a missing GPUCapabilities IFC. The current RationalTime source is `.cpp` with `module Time.Rational;`, the two interfaces no longer contain nested export declarations, and the IFC error came from an isolated compile log whose provider was not part of that command.
- **価値または懸念:** Historical log errors can identify useful failure modes but do not establish current Clang status after migration or after dependency-isolated experiments. Tie each diagnostic to the current source revision and actual provider set before changing code.
- **次に確認すべきこと:** Use authorized GNU Clang and clang-cl configure/scan/compile outputs for current status; keep isolated missing-provider diagnostics separate from source failures.
## 2026-10-25: Clang scan classification order is part of CMake behavior
- **関連:** `Artifact/CMakeLists.txt`、`ArtifactCore/CMakeLists.txt`、`ArtifactWidgets/CMakeLists.txt`、`ArtifactPr/CMakeLists.txt`
- **確認できた事実:** Artifact と ArtifactCore は複数の MSVC 向け `CXX_SCAN_FOR_MODULES OFF` / manual IFC property blocksを先に設定し、ファイル末尾のClang分類で通常のimplementation scanを再設定している。FileInfoはpartition例外としてscan disabledと明示PCM参照を維持する。ArtifactWidgetsとArtifactPrはtarget source登録前にClang scan propertyを設定する。
- **価値または懸念:** 新しいsource-local `OFF` をClang分類より後へ追加すると、ClangのP1689依存順序を静かに無効化する可能性がある。反対にFileInfoのような例外は最終分類から明示して除外する必要がある。
- **次に確認すべきこと:** CMake configure後のsource-specific scan rulesとcompile commandsをGNU Clang / clang-cl双方で比較する。現状は静的確認のみで生成結果は未検証。
## 2026-10-25: Artifact render module reference map had a stale submitter name
- **関連:** `Artifact/cmake/ArtifactRenderModuleReferences.cmake`, `Artifact/src/Render/Artifact3DPrimitiveSubmitter.cpp`, `Artifact/include/Render/Artifact3DPrimitiveSubmitter.ixx`
- **確認できた事実:** The implementation and exported interface both declare `Artifact.Render.ThreeDPrimitiveSubmitter.Contract`, while the explicit reference table had omitted `.Contract`. The table is included from `Artifact/CMakeLists.txt`; a static search found no consumer of the table variable.
- **価値または懸念:** Keeping the reference metadata aligned avoids future source-map tooling or CMake consumers resolving the implementation against the wrong module name. Current Clang build behavior is not known to depend on this table.
- **次に確認すべきこと:** If the render module-reference table becomes active in source classification or BMI wiring, validate its rows against actual declarations during an authorized CMake configure/scan.
## 2026-10-25: Active implementation migration inventory is exhausted
- **関連:** `Artifact`, `ArtifactCore`, `ArtifactWidgets`, `ArtifactPr` の `.cppm` inventory、Artifact Clang source classifier、各 repository の CMake source manifests
- **確認できた事実:** Static inventory found no active in-scope implementation unit remaining to migrate from `.cppm` to `.cpp`. Module-aware `.cpp` files outside the central Artifact source manifest were reconciled with split-target `_IMPL` lists or the explicit Clang scan list. The Artifact render module-reference table has 17 rows with existing files and matching source/interface module declarations.
- **価値または懸念:** Source extension migration and scanner registration have reached the point where the remaining confidence gap is compiler-generated P1689 rules and actual Clang compilation, which source inspection cannot prove.
- **次に確認すべきこと:** After CMake/build execution is authorized, validate both upstream Clang and clang-cl from fresh generated trees, inspect module scan outputs and compile commands, then address only current reproducible diagnostics.
## 2026-10-25: Existing Clang Ninja tree predates current Artifact CMake changes
- **関連:** `out/build/x64-Debug-ClangXX-MsvcAbi/build.ninja`, `Artifact/CMakeLists.txt`, `Artifact/cmake/ArtifactSources.cmake`
- **確認できた事実:** The old generated Ninja file was timestamped 2026-10-24 04:28 and lacked the newly registered preview helper rules. Current GNU-style Clang and clang-cl trees have since been reconfigured with CMake 4.4.4; their fresh rules include the six preview implementation sources, and P1689 scans pass for all six in both frontends. Direct compilation still requires current module BMIs; old PCMs report stale/out-of-date or missing transitive modules.
- **価値または懸念:** Generated source classification and scan coverage are now confirmed, while compile/link coverage remains open until the module dependency graph can rebuild without triggering the entire 7,737-action target graph.
- **次に確認すべきこと:** Identify a bounded way to regenerate the preview module BMI dependency closure and compile/link these sources on both Clang frontends.
## 2026-10-25: Workspace-root `.cppm` search includes a separate review checkout and scratch files
- **関連:** `Artifact_dev_review` worktree (`codex/review-artifact-fixes`)、root `temp*.cppm` files、active project CMake manifests
- **確認できた事実:** A root-wide `.cppm` search found additional files under the separate `Artifact_dev_review` git checkout and three root-level `temp*.cppm` files. The active project CMake files do not reference these paths.
- **価値または懸念:** Counting every filesystem hit as an active migration candidate would mix a separate review branch and scratch artifacts into the current project scope.
- **次に確認すべきこと:** Keep future migration inventories rooted in active project source manifests and distinguish other worktrees or scratch files from configured build sources.
## 2026-10-25: Unregistered Artifact `.cppm` files are not implementation migration candidates
- **関連:** `Artifact/src/Test/ArtifactTestRenderQueue.cppm`, `Artifact/composition_codex_original.cppm`, Artifact CMake source manifests
- **確認できた事実:** These are the two Artifact `.cppm` files absent from active CMake references. `ArtifactTestRenderQueue.cppm` declares `export module Artifact.Test.RenderQueue`; `composition_codex_original.cppm` is an unregistered source snapshot.
- **価値または懸念:** A missing CMake reference alone is insufficient to classify a `.cppm` as a module implementation; exported interfaces and source snapshots must remain distinct from active implementation units.
- **次に確認すべきこと:** Preserve this distinction when refreshing `.cppm` inventories; migrate only files confirmed as implementation units in the active source scope.
## 2026-10-25: Rename audit caught line-ending drift and mojibake
- **関連:** 682 `.cppm` → `.cpp` sibling pairs across `Artifact`, `ArtifactCore`, `ArtifactWidgets`, and `ArtifactPr`; `ArtifactCore/src/ImageProcessing/Halide/Glow.cpp`
- **確認できた事実:** Byte comparison against HEAD found 645 pairs identical after line-ending/final-LF normalization. The other 37 pairs contain 26 direct include/import or narrow API-name corrections and 11 BOM/blank-line changes; all 16 added imports have an in-scope exported provider. Three Japanese comment lines in `Glow.cpp` had been re-encoded with replacement characters and were restored byte-for-byte. Migration had also introduced CRLF in 53 files and omitted final LF in 29; all migrated `.cpp` files now use LF and end with LF. No migration-introduced U+FFFD remains.
- **価値または懸念:** Extension migration needs a byte-level pass because PowerShell/encoding transformations can silently alter comments and line endings even when module declarations and behavior are intended to stay unchanged.
- **次に確認すべきこと:** Configure and P1689 scanning are now confirmed for both Clang frontends. Continue with current BMI regeneration, object compilation, and link validation.

## 2026-10-25: clang-cl module flags must stay separate from frontend compatibility flags
- **関連:** `CMakeLists.txt`, `ArtifactCore/CMakeLists.txt`, `ArtifactWidgets/CMakeLists.txt`, `Artifact/CMakeLists.txt`
- **確認できた事実:** clang-cl identifies as Clang with the MSVC frontend, so `ARTIFACT_FRONTEND_MSVC` is true while `ARTIFACT_COMPILER_MSVC_CL` is false. A fresh CMake 4.4.4 configure generated clang-cl commands with `-std=c++23`; after gating `/std:c++latest` and `/experimental:module` on actual MSVC, these switches are absent from the generated CoreAudio compile command.
- **価値または懸念:** Frontend compatibility does not imply MSVC module flags are valid for clang-cl; keeping these controls separate avoids unused-option diagnostics without changing C++ standard selection or Clang module scanning.
- **次に確認すべきこと:** Compile the representative CoreAudio/Core implementation targets and inspect diagnostics, then continue a full clang-cl build to expose source-level compatibility issues.
## 2026-10-25: Preview pipeline module import size exceeded Clang source-location capacity
- **関連:** `Artifact/src/Preview/ArtifactPreviewCompositionPipeline.cpp`, its layer draw branches, and Artifact source manifest
- **確認できた事実:** A GNU-style Clang 22.1.3 build failed repeatedly while importing the pipeline's large set of layer PCMs with `malformed or corrupted precompiled file: ran out of source locations`. Incremental direct probes showed that the module primary interface and five imports fit, while adding another large layer PCM crossed the limit. Each of the layer modules also compiled alone with the module interface.
- **提案（未検証）:** Keep the public pipeline module and move layer-specific draw implementations into separate implementation translation units that share the module declaration; this should reduce the cumulative imported PCM set per translation unit while keeping branch order and rendering behavior intact.
- **価値または懸念:** Splitting can lower front-end memory pressure without reducing module coverage, but cross-translation-unit linkage and final module build integration must be verified with fresh generated scan outputs.
- **確認できた事実:** The six current implementation units passed GNU-style Clang and clang-cl 22.1.3 P1689 scans after fresh CMake generation. An anonymous-namespace mismatch in pipeline forward declarations was caught by symbol inspection and corrected. A partial `lld-link /force:unresolved` probe showed no unresolved `tryDraw*Preview` helper symbols, but did not prove full module or application linkage. A direct compile against old PCMs failed because the primary module's transitive imports were missing and several referenced module PCMs were out of date. Invoking Ninja for one object expanded to 7,737 actions, so the run was interrupted after 113 actions.
- **次に確認すべきこと:** Find a bounded path to regenerate the required module BMI closure and compile/link the six translation units, then repeat the validation for the full ArtifactAppRuntime target if feasible.
## 2026-10-25: Clang production compile is exposing module ownership and import-boundary errors
- **関連:** `Artifact/include/Widgets/Dialog/ApplicationSettingDialog.ixx`, `Artifact/src/Tool/AIDSL/`, `Artifact/src/Layer/ArtifactAbstractLayerRuntimeSnapshot.cpp`, and directly imported module/header dependencies
- **確認できた事実:** Current Clang compilation reported a module-purview redeclaration of Qt's `QEvent`, nested `export` declarations inside exported namespaces, AIDSL member definitions attached to `AIToolDSL.Parser` although declared in `AIToolDSL.Types`, and member definitions of `ArtifactAbstractLayer` emitted from the global module. The corresponding narrow fixes have been applied; the current incremental target compiled `ArtifactAbstractLayerRuntimeSnapshot.cpp` after making it an implementation unit of `Artifact.Layer.Abstract`.
- **提案（未検証）:** These errors suggest MSVC has accepted some module ownership and transitive-import patterns that Clang diagnoses more strictly. Further repairs should follow explicit owning-module and direct-import boundaries rather than broadening exports.
- **価値または懸念:** Correcting ownership at the source boundary improves portability while keeping interface dependencies narrow. The complete GNU Clang target and clang-cl production compilation remain unverified.
- **次に確認すべきこと:** Continue the current GNU Clang application build and address subsequent concrete diagnostics; then perform a full clang-cl production build and inspect final link results.

## 2026-10-25: Clang source-location limits can be reduced by splitting independent service access
- **関連:** `ArtifactRenderMenu.cpp`, `ArtifactRenderMenuQueueSupport.cpp`, `ArtifactRenderMenuProjectAccess.cpp`, `ArtifactCompositionViewerFooter.cpp`, `ArtifactRenderQueueService.ixx`, `ArtifactApplicationManager.cpp`
- **確認できた事実:** GNU Clang 22.1.3 exceeded its source-location capacity when Render Menu combined Project/Composition and Render Queue imports. Separating Project access from queue access allowed the three Render Menu objects to compile. Separating Project and Playback reads likewise allowed the Composition Footer objects to compile. Removing declaration-irrelevant implementation headers from `ArtifactRenderQueueService.ixx` also reduced its interface surface. `ArtifactApplicationManager.cpp` still exceeded the limit after removing the project-service import because the remaining manager imports cumulatively consumed too many source locations. A first same-module accessor split also exposed a Clang ownership ambiguity between the AppManager purview forward declaration and the imported service type. The verified solution is an import-light `Artifact.Application.Manager.Access` interface plus separate implementation TUs, each importing one concrete manager module; the AppManager PImpl stores opaque raw access handles with explicit destroy functions. GNU Clang 22.1.3 compiled all 12 access TUs and `ArtifactApplicationManager.cpp`. The full application target and final link remain unverified.
- **価値または懸念:** Splitting an implementation along independent dependency sets can keep Clang module imports below the per-translation-unit source-location ceiling without changing behavior. The opaque access boundary keeps module-owned public types in their existing interface while limiting each helper implementation TU to a narrow manager import. Full application linkage remains a required follow-up.
- **次に確認すべきこと:** Build and link the complete `ArtifactAppRuntime` target, then repeat the focused support-source validation with clang-cl once CMake 4.4 or newer is available.
