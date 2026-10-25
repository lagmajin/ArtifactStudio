**最終更新:** 2026-10-25

**ステータス:** In Progress

- 2026-10-25 log chronology: a clang-cl configure log from 11:08 predates Artifact commit `77b24e8d` (11:14), which restricted DockFoundation `/Zf` to the MSVC compiler ID; the 11:26 clang-cl build reused that older generated tree and therefore its `/Zf` scan error is stale evidence, not a failure reproduced against current CMake sources. A later GNU Clang application build log records 26 sccache compile failures, with server socket resets (`os error 10054`) / response-read failures; this is a cache-server transport failure, not a compiler diagnostic. No reconfigure or build was run in this audit.

- 2026-10-25 tracked-file re-audit (using each child repository’s `git ls-files`): Artifact has 42 remaining `.cppm` module providers and one `module X;` source snapshot (`composition_codex_original.cppm`) outside the active CMake manifest; two frozen-area `.cppm` files were excluded from content inspection. ArtifactCore has 45 remaining `.cppm` module providers; its two frozen-area files were excluded. ArtifactWidgets has no tracked `.cppm` files. Static CMake inspection confirms Artifact classifies collected `_IMPL` `.cppm`/`.cpp` sources for Clang scans, ArtifactCore builds its implementation-source list from `_IMPL` manifests with frontend-specific scan handling, and ArtifactWidgets scans migrated module-aware `.cpp` sources. This does not verify generated P1689 output or compile/link behavior.

- Static module-hygiene audit of 22 `.cpp` units touched by recent Clang fixes and implementation migrations found no `#include` after the named `module` declaration; every checked file uses LF and ends with LF. This confirms source layout/line endings only, not Clang compilation.
- Audited ArtifactPr's static Clang source classification: all 28 `ARTIFACT_PR_IMPL_SOURCES` entries currently have a named-module attachment, so its `.cpp` classifier's module-declaration rule covers these implementations; there are no import-only `.cpp` entries in that list. The two executable entry sources are checked separately for imports. This is a source-list/regex audit only; no CMake configure or P1689 scan was run.
- Followed up on a literal-path scan of module-aware `.cpp` sources: `ImageMorphEffect.cpp` is registered through the `_distort_effect` CMake loop, which constructs its interface/implementation paths and adds them to `APP_MODULES` / `APP_IMPL`; the Artifact Clang classifier scans `_IMPL` sources. Two in-tree consumers import it. Four other candidates had no literal CMake path or in-tree module importer in this read-only pass; they were not enabled. This confirms CMake source registration statically, not generated target membership or compilation.
- Re-audited the current source manifests: `ArtifactWidgets` and the optional `ArtifactPr` source tree contain no remaining `.cppm` files; the remaining Artifact/ArtifactCore manifest entries are predominantly exported primary module interfaces and must not be treated as ordinary implementation-unit renames. All 17 Artifact render implementation-reference rows resolve to existing `.cpp` + `.ixx` files, with the source `module` attachment matching the mapped module name. Static source/configuration inspection only; no configure, scan, compile, or link was run.
- Migrated the three empty/whitespace-only `ArtifactWidgets` implementation placeholders (`BackendSettingWidget`, `KnobEditorWidget`, and `NodeWireGraphicItem`) back to `.cpp` and added their paths to the explicit migrated-source list. Their contents were preserved byte-for-byte (including the existing BOM/blank lines); they declare/import no modules, so this is source classification cleanup rather than a Clang module-scan fix. No CMake generation or compilation was run.
- Added direct `import Utils.Id;` dependencies to the preview pipeline, solid-layer, raster-layer, and video-layer implementation units, which spell `ArtifactCore::LayerID` directly. This makes the implementation-unit dependency explicit; no P1689 scan or compile was run, so the effect on the prior Clang module-ownership/source-location diagnostics remains unverified.
- A later GNU Clang 22.1.3 application build log shows `MediaPlaybackController.ixx` compiled successfully (the earlier unknown `DecodedVideoFrame` diagnostic did not recur), then the broader build failed on unrelated effects/color sources through sccache; those diagnostics need separate triage before treating the Clang lane as green.
- A fresh direct-object checkpoint with the current GNU Clang 22.1.3 Ninja commands compiled the migrated `ArtifactApplicationManager.cpp`, `ArtifactPreviewWorker.cpp`, `ArtifactRenderElement.cpp`, `LayerEditorRenderScheduler.cpp`, `LayerEditorViewWheelController.cpp`, and `CompositionEmptyOverlay.cpp`. `UndoManager.ixx` and `Utils.Id.ixx` interfaces also compiled individually. `ArtifactAudioMiniWidget.cpp` and `ArtifactRenderQueueEncoder.cpp` still crash Clang after reporting `Id` from named module `Utils.Id` cannot be attached to another module; recompiling the `Utils.Id` interface first did not change the result. `ArtifactPreviewCompositionPipeline.cpp` and its `ArtifactPreviewLayerEffects.cpp` split likewise crash the frontend with all referenced PCM files present. These are bounded direct compiler invocations, not a Ninja target build or final link.
- Further Clang isolation probes for `ArtifactAudioMiniWidget.cpp` showed that `Artifact.Service.Project` + `Artifact.Layer.Audio` fails even when imported as the only pair, while `Artifact.Service.Project` + `Artifact.Composition.Abstract` and `Artifact.Composition.Abstract` + `Artifact.Layer.Audio` both compile in isolation with the current module map and matching OpenMP flags. This narrows the failure to the combined Project/Audio dependency closure; the original `Utils.Id` ownership diagnostic appears downstream of the compiler failure, but that interpretation remains a hypothesis. No application source or public API was changed for this probe.
- Implemented a private same-module access split for `ArtifactAudioMiniWidget`: Project lookup keeps the `ArtifactCompositionPtr` alive in an opaque handle, while a second TU performs audio-layer waveform extraction using only Composition+Audio imports. Marker editing retrieves `ArtifactInOutPoints` through the retained handle, preserving composition lifetime and existing editing behavior. Both implementation sources are registered in `ArtifactSources.cmake`; static diff/manifest/line-ending checks pass.
- Direct compilation against the existing GNU Clang module map still stops with the `Utils.Id` ownership diagnostic while compiling the widget TU, before it verifies the new access split. The PCM dependency closure was not regenerated, and no CMake or target build was run. Thus this source change and its P1689 ordering/linkage remain unverified; clang-cl and final application link are also unverified. The handle uses one nothrow allocation per `refresh()` to retain composition ownership until the next refresh or widget destruction; refresh is triggered by composition changes, not per-frame transport updates.
- The widget implementation now directly imports `Utils.Id`, because it defines `setComposition(CompositionID const&)` and stores a `CompositionID` value in its PImpl; previously only the public interface imported this module. The existing direct compile predates this source correction, so its Id diagnostic does not validate the current import set. No scan or compile was run after adding the direct import.
- Static review of `ArtifactRenderQueueEncoder.cpp` found that its `Render.Queue.Manager` import had no use in the implementation; it has been removed to reduce the module dependency graph. `Artifact.Render.Queue.Job` remains the source of the job declarations used by this implementation. No Clang P1689 scan or compile was run for this import reduction.
- Moved `drawCloneSelectionOverlay` from `ArtifactPreviewCompositionPipeline.cpp` into the existing `ArtifactPreviewOtherLayers.cpp` implementation unit in the same module. The pipeline no longer imports `Artifact.Layer.Clone` or includes overlay-only Qt transform headers; overlay geometry, colors, line widths, and invocation order remain unchanged by static diff inspection. This source move has not been P1689-scanned or compiled, so cross-TU linkage and Clang source-location impact remain unverified.
- GNU Clang 22.1.3 bounded object builds pass for Render Menu (Queue/Project access units), Composition Viewer Footer (Project/Playback access units), Timeline layer-test helpers, all 12 new Application Manager access TUs, and `ArtifactApplicationManager.cpp`. The AppManager source-location ceiling was resolved by moving manager ownership/access behind small type-erased helper APIs; the AppManager object now compiles. The full `Artifact.exe` link and complete `ArtifactAppRuntime` build remain unverified.
- The GNU Clang tree was regenerated after registering the manager access interface and implementation units. Their P1689 scans and object builds pass. clang-cl has not been regenerated against these latest files: available CMake versions are 4.2.2 and 4.3.1, while the project requires 4.4+ for clang-cl modules.
- Fresh configure/generate now succeeds for upstream GNU Clang 22.1.3 and clang-cl 22.1.3 with CMake 4.4.4. Current compile rules classify the frontends separately, use C++23, and omit `/std:c++latest`, `/experimental:module`, and the MSVC-only forced include for Clang. The earlier GNU Clang full build reached about 7,100/7,703 actions before the original pipeline translation unit failed with `malformed or corrupted precompiled file: ran out of source locations`; that failure also reproduced with sccache disabled. Full production builds remain incomplete.
- Split `ArtifactPreviewCompositionPipeline.cpp`'s layer-specific draw branches into five same-module implementation files (effects, solid layers, image/text raster layers, video, and other layers) and registered them in `ArtifactSources.cmake`. GNU-style Clang and clang-cl P1689 scans pass for all six files in the current generated trees. Symbol inspection found and fixed an anonymous-namespace linkage mismatch; a partial lld-link probe produced no unresolved `tryDraw*Preview` helper symbols. A direct compile against the old PCM set now stops on stale/out-of-date or missing transitive module files. Building one object through Ninja expanded to 7,737 actions; that broad run was interrupted after 113 actions. Thus the current CMake rules and scan outputs are verified, but current dependency BMI regeneration, compilation, and final target link are not.
- Audited 682 deleted `.cppm` → `.cpp` sibling pairs against their HEAD blobs. 645 pairs match after line-ending/final-LF normalization. The other 37 consist of 26 direct include/import or narrow API-name corrections and 11 BOM/blank-line changes. All 16 newly added module imports resolve to an in-scope exported provider. The audit caught mojibake in three Japanese comments in `ArtifactCore/src/ImageProcessing/Halide/Glow.cpp`; restored those original bytes. It also caught migration-created CRLF in 53 sources and missing final LF in 29; normalized all migrated `.cpp` sources to LF with a final newline. No migration-introduced U+FFFD remains.
- Rechecked active module source registration by basename. Two Artifact `.cppm` files are not mentioned by active CMake: `src/Test/ArtifactTestRenderQueue.cppm` declares an exported module (test interface), and `composition_codex_original.cppm` is an unregistered source snapshot. Neither is an implementation-unit migration candidate; both remain untouched.
- A workspace-root `.cppm` search also found files under the separate `Artifact_dev_review` checkout (`codex/review-artifact-fixes`) and three root-level `temp*.cppm` scratch files. The active project CMake files do not reference these paths; they are excluded from this migration inventory and were left untouched.
- The previous `x64-Debug-ClangXX-MsvcAbi-current` generated tree lacked helper source rules; both GNU Clang and clang-cl trees have now been reconfigured and regenerated from the current source state. The fresh rules contain the six expected implementation sources and their P1689 scanner actions.
- Static inventory of the four in-scope repositories found no remaining active, in-scope implementation translation unit that needs a `.cppm` to `.cpp` migration. Remaining `.cppm` files are module interfaces or are outside active migration scope. Module-aware `.cpp` sources outside `ArtifactSources.cmake` were checked against direct target lists and Clang scan classification; the active direct sources are covered by `_IMPL` collections or the explicit Clang source list. This is source inspection plus current P1689 scan evidence; current dependency BMI regeneration, object compilation, and final link remain unverified.
- Corrected the `Artifact3DPrimitiveSubmitter` module name in `Artifact/cmake/ArtifactRenderModuleReferences.cmake` to match the implementation and exported interface declaration (`Artifact.Render.ThreeDPrimitiveSubmitter.Contract`). Static inspection found this stale reference; the map is currently included by CMake but no consumer of this variable was found. No configure/build was run.
- Migrated four unregistered Artifact implementation sources and ten unregistered ArtifactCore sources (including non-module and test translation units) from `.cppm` to `.cpp`. These paths were absent from explicit build manifests; matching ArtifactCore module-reference metadata and its path-check helper were updated. No targets were enabled or behavior changed; LF endings are enforced.
- Moved four empty/non-module `ArtifactWidgets` `.cppm` translation units to `.cpp` and added them to the explicit migration source list used to stabilize the glob-based target inventory. They contain no module declarations or imports; no widget behavior changed. LF endings are enforced; no configure or build was run.
- Migrated `Artifact/src/Render/ArtifactRenderLayerPipeline.cppm` to `.cpp` and updated all active source references. The module declaration, imports, and existing rendering/view/layer behavior remain unchanged; LF endings are enforced. Clang scanning and compilation remain unverified.
- Migrated `Artifact/src/Widgets/ArtifactTimelineWidget.cppm` to `.cpp` and updated all active source references. The module declaration, imports, and existing rendering/view/layer behavior remain unchanged; LF endings are enforced. Clang scanning and compilation remain unverified.
- Migrated `Artifact/src/Widgets/Render/ArtifactCompositionEditor.cppm` to `.cpp` and updated all active source references. The module declaration, imports, and existing rendering/view/layer behavior remain unchanged; LF endings are enforced. Clang scanning and compilation remain unverified.
- Migrated `Artifact/src/Widgets/Render/ArtifactCompositionRenderController.cppm` to `.cpp` and updated all active source references. The module declaration, imports, and existing rendering/view/layer behavior remain unchanged; LF endings are enforced. Clang scanning and compilation remain unverified.
- Migrated `Artifact/src/Widgets/Viewer/ArtifactContentsViewer.cppm` to `.cpp` and updated all active source references. The module declaration, imports, and existing rendering/view/layer behavior remain unchanged; LF endings are enforced. Clang scanning and compilation remain unverified.
- Migrated `Artifact/src/Layer/ArtifactVideoLayer.cppm` to `.cpp` and updated all active source references. The module declaration, imports, and existing rendering/view/layer behavior remain unchanged; LF endings are enforced. Clang scanning and compilation remain unverified.
- Migrated `Artifact/src/Widgets/Timeline/ArtifactTimelineTrackPainterView.cppm` to `.cpp` and updated `Artifact/cmake/ArtifactSources.cmake`. The module declaration, imports, persistence/timeline behavior, and registration remain unchanged; LF endings are enforced. Clang scanning and compilation remain unverified; this extension-only move does not claim to resolve any prior frontend/resource failures.
- Migrated `Artifact/src/Project/ArtifactProject.cppm` to `.cpp` and updated `Artifact/cmake/ArtifactSources.cmake`. The module declaration, imports, persistence/timeline behavior, and registration remain unchanged; LF endings are enforced. Clang scanning and compilation remain unverified; this extension-only move does not claim to resolve any prior frontend/resource failures.
- Migrated `Artifact/src/Render/ArtifactRenderQueueService.cppm` to `.cpp` and updated active source / explicit module-reference paths. The module declaration, imports, startup/render service behavior, and metadata definitions remain unchanged; LF endings are enforced. Clang scan and compilation remain unverified.
- Migrated `Artifact/src/Render/ArtifactIRenderer.cppm` to `.cpp` and updated active source / explicit module-reference paths. The module declaration, imports, startup/render service behavior, and metadata definitions remain unchanged; LF endings are enforced. Clang scan and compilation remain unverified.
- Migrated `Artifact/src/AppMain.cppm` to `.cpp` and updated active source / explicit module-reference paths. The module declaration, imports, startup/render service behavior, and metadata definitions remain unchanged; LF endings are enforced. Clang scan and compilation remain unverified.
- Migrated `Artifact/src/Widgets/Render/TransformGizmo.cppm` to `.cpp` and updated `Artifact/cmake/ArtifactSources.cmake`. The implementation module, imports, and existing behavior remain unchanged; LF endings are enforced. Clang scan/compile remains unverified in this tree. Previous Clang implementation compile failures for this unit are recorded separately and are not claimed fixed by the extension migration.
- Migrated `Artifact/src/Preview/ArtifactPreviewCompositionPipeline.cppm` to `.cpp` and updated `Artifact/cmake/ArtifactSources.cmake`. The implementation module, imports, and existing behavior remain unchanged; LF endings are enforced. Clang scan/compile remains unverified in this tree. Previous Clang implementation compile failures for this unit are recorded separately and are not claimed fixed by the extension migration.
- Migrated `Artifact/src/Layer/ArtifactSolid2DLayer.cppm` to `.cpp` and updated `Artifact/cmake/ArtifactSources.cmake`. The implementation module, imports, and existing behavior remain unchanged; LF endings are enforced. Clang scan/compile remains unverified in this tree. Previous Clang implementation compile failures for this unit are recorded separately and are not claimed fixed by the extension migration.
- Migrated `Artifact/src/Widgets/ArtifactFilmstripWidget.cppm` to `.cpp` and updated `Artifact/cmake/ArtifactSources.cmake`. The implementation module, imports, and existing behavior remain unchanged; LF endings are enforced. Clang scan/compile remains unverified in this tree. Previous Clang implementation compile failures for this unit are recorded separately and are not claimed fixed by the extension migration.
- Migrated `Artifact/src/Undo/UndoManager.cppm` to `.cpp` and updated `Artifact/cmake/ArtifactSources.cmake`. The implementation module, imports, composition/undo behavior, and registration remain unchanged; LF endings are enforced. Clang scanning and compilation remain unverified.
- Migrated `Artifact/src/Composition/ArtifactAbstractComposition.cppm` to `.cpp` and updated `Artifact/cmake/ArtifactSources.cmake`. The implementation module, imports, composition/undo behavior, and registration remain unchanged; LF endings are enforced. Clang scanning and compilation remain unverified.
- Migrated `Artifact/src/Widgets/Timeline/ArtifactLayerPanelWidget.cppm` to `.cpp` and updated `Artifact/cmake/ArtifactSources.cmake`. The module declaration, imports, and existing service/timeline behavior remain unchanged; LF endings are enforced. Clang scanning and compilation remain unverified.
- Migrated `Artifact/src/Service/ArtifactProjectService.cppm` to `.cpp` and updated `Artifact/cmake/ArtifactSources.cmake`. The module declaration, imports, and existing service/timeline behavior remain unchanged; LF endings are enforced. Clang scanning and compilation remain unverified.
- Migrated `Artifact/src/Widgets/ArtifactMainWindow.cppm` to `.cpp` and updated `Artifact/cmake/ArtifactSources.cmake`. The module declaration, imports, feature guards, and existing UI/render behavior remain unchanged; LF endings are enforced. Clang scanning and compilation remain unverified.
- Migrated `Artifact/src/Widgets/Render/ArtifactCompositionRenderOverlay.cppm` to `.cpp` and updated `Artifact/cmake/ArtifactSources.cmake`. The module declaration, imports, feature guards, and existing UI/render behavior remain unchanged; LF endings are enforced. Clang scanning and compilation remain unverified.
- Full `Artifact/src` inventory still contains four tracked implementation `.cppm` files outside the source manifest: `ArtifactColorWheels`, `ArtifactStabilizer`, `ArtifactTimelineClock`, and `ArtifactTextRenderTarget`. No module import sites were found. The TextRenderTarget interface already defines its methods inline and its excluded runtime target lists only the interface; adding the `.cppm` risks duplicate definitions, so it was not registered. The other three remain untouched pending ownership/feature confirmation.
- Migrated `Artifact/src/Render/DiligentImmediateSubmitter.cppm` to `.cpp` and updated the source manifest, explicit render slice, and render module-reference inventory. The Diligent implementation, imports, module identity, and GPU/hot-path behavior are unchanged; LF endings are enforced. Clang scan and compilation remain unverified.
- Migrated `Artifact/src/Render/PrimitiveRenderer2D.cppm` to `.cpp` and updated the source manifest, explicit render slice, and render module-reference inventory. The Diligent implementation, imports, module identity, and GPU/hot-path behavior are unchanged; LF endings are enforced. Clang scan and compilation remain unverified.
- Migrated `Artifact/src/Widgets/ArtifactProjectManagerWidget.cppm` to `.cpp` and updated `Artifact/cmake/ArtifactSources.cmake`. The module declaration, imports, current widget behavior, and registration remain unchanged; LF endings are enforced. Clang scanning and compilation remain unverified.
- Migrated `Artifact/src/Widgets/ArtifactInspectorWidget.cppm` to `.cpp` and updated `Artifact/cmake/ArtifactSources.cmake`. The module declaration, imports, current widget behavior, and registration remain unchanged; LF endings are enforced. Clang scanning and compilation remain unverified.
- Migrated `Artifact/src/Widgets/Menu/ArtifactLayerMenu.cppm` to `.cpp` and updated `Artifact/cmake/ArtifactSources.cmake`. Module declaration, imports, existing menu behavior, and registration remain unchanged; LF endings are enforced. Clang scanning and compilation remain unverified.
- Migrated `Artifact/src/Widgets/CommandPalette/ArtifactCommandPaletteWidget.cppm` to `.cpp`, registered its `.ixx` and `.cpp` paths, and kept both out of `APP_MODULES` / `APP_IMPL` when the existing feature option is OFF. When enabled, the feature macro now reaches `ArtifactAppRuntime`, which compiles `ArtifactMainWindow`; Clang scanning therefore includes the optional implementation. Default-off behavior is preserved. LF endings are enforced; CMake configure and compilation remain unverified.
- Migrated `Artifact/src/Widgets/Render/ArtifactCompositionRenderWidget.cppm` to `.cpp` and updated `Artifact/cmake/ArtifactSources.cmake`. The module declaration, imports, viewport/timeline rendering behavior, and registration remain unchanged; LF endings are enforced. Clang scanning and compilation remain unverified.
- Migrated `Artifact/src/Widgets/Timeline/ArtifactDiligentTimelineRenderWindow.cppm` to `.cpp` and updated `Artifact/cmake/ArtifactSources.cmake`. The module declaration, imports, viewport/timeline rendering behavior, and registration remain unchanged; LF endings are enforced. Clang scanning and compilation remain unverified.
- Migrated `Artifact/src/Layer/Artifact3DModelLayer.cppm` to `.cpp` and updated `Artifact/cmake/ArtifactSources.cmake`. The module declaration, imports, layer behavior, and registration remain unchanged; LF endings are enforced. Clang scanning and compilation remain unverified.
- Migrated `Artifact/src/Layer/ArtifactTextLayer.cppm` to `.cpp` and updated `Artifact/cmake/ArtifactSources.cmake`. The module declaration, imports, layer behavior, and registration remain unchanged; LF endings are enforced. Clang scanning and compilation remain unverified.
- Migrated `Artifact/src/Worker/FarmWorkerMain.cppm` to `.cpp` and updated the conditional `ArtifactWorker` target plus the Clang direct-source scan list. Its module declaration, import, and worker behavior remain unchanged; LF endings are enforced. CMake configure and compilation remain unverified.
- Migrated `Artifact/src/Render/ArtifactTextGlyphSubmitter.cppm` to `.cpp` in both private runtime targets. The source is outside `ArtifactSources.cmake`, so the Clang implementation scan classifier now explicitly includes it; its `Artifact.Render.TextGlyphSubmitter` module declaration, imports, and rendering behavior are unchanged. LF endings are enforced; no CMake configure or compile was run.
- Migrated `Artifact/src/Widgets/Asset/ArtifactAssetBrowser.cppm` to `.cpp` and updated `Artifact/cmake/ArtifactSources.cmake`. Module implementation, imports, existing behavior, and registration remain unchanged; LF endings are enforced. Clang scanning and compilation remain unverified.
- Migrated `Artifact/src/Tool/ArtifactPuppetTool.cppm` to `.cpp` and updated `Artifact/cmake/ArtifactSources.cmake`. Module implementation, imports, existing behavior, and registration remain unchanged; LF endings are enforced. Clang scanning and compilation remain unverified.
- Migrated `Artifact/src/Widgets/AI/ArtifactAICloudWidget.cppm` to `.cpp` and updated `Artifact/cmake/ArtifactSources.cmake`. Module implementation, imports, existing UI behavior, and registration remain unchanged; LF endings are enforced. Clang scanning and compilation remain unverified.
- Migrated `Artifact/src/Widgets/ArtifactExpressionCopilotWidget.cppm` to `.cpp` and updated `Artifact/cmake/ArtifactSources.cmake`. Module implementation, imports, copilot UI behavior, and registration remain unchanged; LF endings are enforced. This changes the source from Clang's former `.cppm` path; Clang scanning and compilation are unverified.
- Migrated `Artifact/src/Widgets/ArtifactPropertyWidget.cppm` to `.cpp` and updated `Artifact/cmake/ArtifactSources.cmake`. The implementation module, imports, existing UI behavior, and registration remain unchanged; LF endings are enforced. Clang scanning and compilation remain unverified.
- Migrated `Artifact/src/Widgets/Menu/ArtifactViewMenu.cppm` to `.cpp` and updated `Artifact/cmake/ArtifactSources.cmake`. The implementation module, imports, existing UI behavior, and registration remain unchanged; LF endings are enforced. Clang scanning and compilation remain unverified.
- Migrated `Artifact/src/Widgets/Dialog/ApplicationSettingDialog.cppm` to `.cpp` and updated `Artifact/cmake/ArtifactSources.cmake`. The implementation module, imports, behavior, and registration remain unchanged; LF endings are enforced. Clang scanning and compilation remain unverified.
- Migrated `Artifact/src/Service/ArtifactEffectService.cppm` to `.cpp` and updated `Artifact/cmake/ArtifactSources.cmake`. The implementation module, imports, behavior, and registration remain unchanged; LF endings are enforced. Clang scanning and compilation remain unverified.
- Migrated `Artifact/src/Service/ArtifactAudioService.cppm` to `.cpp` and updated `Artifact/cmake/ArtifactSources.cmake`. Transport ownership, module imports, audio service behavior, and registration remain unchanged; LF endings are enforced. Clang scanning and compilation remain unverified.
- Migrated `Artifact/src/Layer/ArtifactAbstractLayer.cppm` to `.cpp` and updated `Artifact/cmake/ArtifactSources.cmake`. The shared layer implementation module, imports, behavior, and registration remain unchanged; LF endings are enforced. Clang scanning and compilation remain unverified.
- Migrated `Artifact/src/Layer/ArtifactAbstractLayerPropertyRouting.cppm` to `.cpp` and updated `Artifact/cmake/ArtifactSources.cmake`. Module implementation, imports, property routing behavior, and registration remain unchanged; LF endings are enforced. Clang scanning and compilation remain unverified.
- Migrated `Artifact/src/Widgets/Render/ArtifactLayerEditorWidget.cppm` to `.cpp` and updated `Artifact/cmake/ArtifactSources.cmake`. The implementation module, imports, existing UI behavior, and registration remain unchanged; LF endings are enforced. Clang scanning and compilation remain unverified.
- Migrated `Artifact/src/Widgets/Control/ArtifactPlaybackControlWidget.cppm` to `.cpp` and updated `Artifact/cmake/ArtifactSources.cmake`. The implementation module, imports, existing UI behavior, and registration remain unchanged; LF endings are enforced. Clang scanning and compilation remain unverified.
- Migrated `Artifact/src/Widgets/Diagnostics/AppDebuggerWidget.cppm` to `.cpp` and updated Artifact CMake source registrations. The module declaration, imports, existing implementation, and behavior remain unchanged; LF endings are enforced. Clang scanning and compilation remain unverified.
- Migrated `Artifact/src/Widgets/Color/ArtifactColorSciencePanel.cppm` to `.cpp` and updated Artifact CMake source registrations. The module declaration, imports, existing implementation, and behavior remain unchanged; LF endings are enforced. Clang scanning and compilation remain unverified.
- Migrated `Artifact/src/Widgets/Dialog/CreatePlaneLayerDialog.cppm` to `.cpp` and updated `Artifact/cmake/ArtifactSources.cmake`. The implementation module declaration, imports, existing behavior, and registration remain unchanged; LF endings are enforced. Clang scanning and compilation remain unverified.
- Migrated `Artifact/src/Layer/ArtifactParametricCompositionLayer.cppm` to `.cpp` and updated `Artifact/cmake/ArtifactSources.cmake`. The implementation module declaration, imports, existing behavior, and registration remain unchanged; LF endings are enforced. Clang scanning and compilation remain unverified.
- Migrated `Artifact/src/Widgets/ArtifactPropertyWidgetShared.cppm` to `.cpp` and updated `Artifact/cmake/ArtifactSources.cmake`. The module implementation, imports, existing behavior, and registration remain unchanged; LF endings are enforced. Clang scanning and compilation remain unverified.
- Migrated `Artifact/src/Layer/ArtifactAbstractLayerPersistence.cppm` to `.cpp` and updated `Artifact/cmake/ArtifactSources.cmake`. The module implementation, imports, existing behavior, and registration remain unchanged; LF endings are enforced. Clang scanning and compilation remain unverified.
- Migrated `Artifact/src/Layer/ArtifactAbstractLayerPhysicsRouting.cppm` to `.cpp` and updated `Artifact/cmake/ArtifactSources.cmake`. The module implementation, imports, layer behavior, and registration remain unchanged; LF endings are enforced. Clang scanning and compilation remain unverified.
- Migrated `Artifact/src/Layer/ArtifactAbstractLayerComponentRouting.cppm` to `.cpp` and updated `Artifact/cmake/ArtifactSources.cmake`. The module implementation, imports, layer behavior, and registration remain unchanged; LF endings are enforced. Clang scanning and compilation remain unverified.
- Migrated `Artifact/src/Layer/ArtifactAbstractLayerTransform.cppm` to `.cpp` and updated `Artifact/cmake/ArtifactSources.cmake`. The module implementation, imports, layer behavior, and registration remain unchanged; LF endings are enforced. Clang scanning and compilation remain unverified.
- Migrated `Artifact/src/Layer/ArtifactAbstractLayerLayoutRouting.cppm` to `.cpp` and updated `Artifact/cmake/ArtifactSources.cmake`. The module implementation, imports, layer behavior, and registration remain unchanged; LF endings are enforced. Clang scanning and compilation remain unverified.
- Migrated `Artifact/src/Widgets/Render/ArtifactTimelineLayerTestWidget.cppm` to `.cpp` and updated `Artifact/cmake/ArtifactSources.cmake`. The module implementation, imports, widget behavior, and registration remain unchanged; LF endings are enforced. Clang scanning and compilation remain unverified.
- Migrated `Artifact/src/Widgets/ArtifactSecondaryPreviewWindow.cppm` to `.cpp` and updated `Artifact/cmake/ArtifactSources.cmake`. The module implementation, imports, widget behavior, and registration remain unchanged; LF endings are enforced. Clang scanning and compilation remain unverified.
- Migrated `Artifact/src/Layer/ArtifactAbstractLayerPropertyGroups.cppm` to `.cpp` and updated `Artifact/cmake/ArtifactSources.cmake`. The module implementation, imports, layer behavior, and registration remain unchanged; LF endings are enforced. Clang scanning and compilation remain unverified.
- Migrated `Artifact/src/Layer/ArtifactAbstractLayerFractureRuntime.cppm` to `.cpp` and updated `Artifact/cmake/ArtifactSources.cmake`. The module implementation, imports, layer behavior, and registration remain unchanged; LF endings are enforced. Clang scanning and compilation remain unverified.
- Migrated `Artifact/src/Layer/ArtifactAbstractLayerImpl.cppm` to `.cpp` and updated `Artifact/cmake/ArtifactSources.cmake`. The module implementation, imports, layer behavior, and registration remain unchanged; LF endings are enforced. Clang scanning and compilation remain unverified.
- Migrated `Artifact/src/Layer/ArtifactLayerFactory.cppm` to `.cpp` and updated `Artifact/cmake/ArtifactSources.cmake`. The module implementation, imports, layer factory behavior, and registration remain unchanged; LF endings are enforced. Clang scanning and compilation remain unverified.

### Continued implementation migrations (2026-10-25)

- Migrated `Artifact/src/Widgets/Timeline/ArtifactLayerPanelPresentation.cppm` to `.cpp` and updated `Artifact/cmake/ArtifactSources.cmake`. The interface module name `Artifact.Widgets.LayerPanelWidget`, implementation declaration, imports, and UI behavior are unchanged; LF endings are preserved. Clang scanning and compilation remain unverified.
- Migrated `Artifact/src/Effects/Rasterizer/GlowEffect.cppm` to `.cpp` and updated `Artifact/cmake/ArtifactSources.cmake`. The module declaration and GPU/CPU effect implementation are unchanged; source line endings are LF. The stale `.cppm` mention in an old `Artifact/CMakeLists.txt` history comment is descriptive only, not a live source registration. Clang scanning and compilation remain unverified.
- Migrated `Artifact/src/Tool/ArtifactMotionSketchTool.cppm` to `.cpp` and updated `Artifact/cmake/ArtifactSources.cmake`. The public interface, module declaration, imports, and tool behavior are unchanged; LF endings are enforced. Clang scanning and compilation remain unverified.
- Extended `ArtifactWidgets/CMakeLists.txt` Clang classification to cover both clang-cl and upstream GNU-style Clang. Legacy implementation `.cppm` files retain their scan setting, with `-x c++` applied only for GNU-style Clang; `.cpp` sources containing named module declarations or imports are explicitly scanned. CMake configure/generate and compilation were not run.
- Migrated `Artifact/src/Widgets/ArtifactShaderGraphWidget.cppm` to `.cpp` and updated `Artifact/cmake/ArtifactSources.cmake`. Its public interface, module declaration, three imports, and widget behavior are unchanged; LF endings are enforced. Clang scanning and compilation remain unverified.
- Migrated `Artifact/src/Widgets/ArtifactToolBar.cppm` to `.cpp` and updated `Artifact/cmake/ArtifactSources.cmake`. The public interface, `Widgets.ToolBar` module declaration, imports, Verdigris implementation, and toolbar behavior are unchanged; line endings are LF. Clang scanning and compilation remain unverified.
- Migrated `Artifact/src/Widgets/ArtifactToolOptionsBar.cppm` to `.cpp` and updated `Artifact/cmake/ArtifactSources.cmake`. The public interface, `Widgets.ToolOptionsBar` module declaration, imports, Verdigris implementation, and tool option behavior are unchanged; line endings are LF. Clang scanning and compilation remain unverified.
- Migrated `Artifact/src/AI/AIClient.cppm` to `.cpp` and updated `Artifact/cmake/ArtifactSources.cmake`. The public `AI.Client` interface, module declaration, imports, AI provider behavior, and implementation are unchanged; LF endings are enforced. Clang scanning and compilation remain unverified.
- Migrated `Artifact/src/Widgets/Menu/ArtifactFileMenu.cppm` to `.cpp` and updated `Artifact/cmake/ArtifactSources.cmake`. The public `Artifact.Menu.File` interface, module declaration, imports, approved File Menu icon references, and menu behavior are unchanged; LF endings are enforced. Clang scanning and compilation remain unverified.
- Migrated `Artifact/src/Project/ArtifactProjectManager.cppm` to `.cpp` and updated `Artifact/cmake/ArtifactSources.cmake`. The public `Artifact.Project.Manager` interface, module declaration, imports, Verdigris implementation, and project management behavior are unchanged; LF endings are enforced. Clang scanning and compilation remain unverified.
- Migrated `Artifact/src/Widgets/Render/ArtifactSoftwareRenderTestWidget.cppm` to `.cpp` and updated `Artifact/cmake/ArtifactSources.cmake`. The existing test widget interface, module declaration, imports, and behavior are unchanged; LF endings are enforced. This is an extension-only migration and does not add software renderer functionality. Clang scanning and compilation remain unverified.
- Migrated `Artifact/src/Widgets/Menu/ArtifactCompositionMenu.cppm` to `.cpp` and updated `Artifact/cmake/ArtifactSources.cmake`. The public `Menu.Composition` interface, module declaration, imports, menu actions, and behavior are unchanged; LF endings are enforced. Clang scanning and compilation remain unverified.
- Migrated `Artifact/src/Widgets/Menu/ArtifactAnimationMenu.cppm` to `.cpp` and updated `Artifact/cmake/ArtifactSources.cmake`. The public `Menu.Animation` interface, module declaration, imports, and existing menu behavior are unchanged; no audio/Transport feature was edited. LF endings are enforced. Clang scanning and compilation remain unverified.
- Migrated `Artifact/src/Widgets/Render/ArtifactTextGizmo.cppm` to `.cpp` and updated `Artifact/cmake/ArtifactSources.cmake`. The public `Artifact.Widgets.TextGizmo` interface, module declaration, imports, approved gizmo behavior, and rendering/input logic are unchanged; LF endings are enforced. Clang scanning and compilation remain unverified.
- Migrated `Artifact/src/Layer/ArtifactNoiseLayer.cppm` to `.cpp` and updated `Artifact/cmake/ArtifactSources.cmake`. The public `Artifact.Layers.Noise` interface, module declaration, imports, procedural layer behavior, and GPU implementation are unchanged; no rendering logic was edited. LF endings are enforced. Clang scanning and compilation remain unverified.
- Migrated `Artifact/src/Layer/ArtifactProcedural3DLayer.cppm` to `.cpp` and updated `Artifact/cmake/ArtifactSources.cmake`. The public `Artifact.Layer.Procedural3D` interface, module declaration, imports, and 3D layer behavior are unchanged; rendering logic was not edited. LF endings are enforced. Clang scanning and compilation remain unverified.
- Migrated `Artifact/src/Widgets/ArtifactCompositionAudioMixerWidget.cppm` to `.cpp` and updated `Artifact/cmake/ArtifactSources.cmake`. The public `Artifact.Widgets.CompositionAudioMixer` interface, module declaration, imports, and existing audio mixer UI behavior are unchanged; no Audio/Transport logic was edited. LF endings are enforced. Clang scanning and compilation remain unverified.
- Migrated `Artifact/src/Widgets/Control/ArtifactPlaybackControlTestWidget.cppm` to `.cpp` and updated `Artifact/cmake/ArtifactSources.cmake`. The public test-widget interface, module declaration, imports, playback service calls, and widget behavior are unchanged; LF endings are enforced. Clang scanning and compilation remain unverified.
- Migrated `Artifact/src/Widgets/Render/ArtifactSoftwareRenderInspectors.cppm` to `.cpp` and updated `Artifact/cmake/ArtifactSources.cmake`. The public `Artifact.Widgets.SoftwareRenderInspectors` interface, module declaration, imports, and inspector behavior are unchanged; no software renderer feature was added. LF endings are enforced. Clang scanning and compilation remain unverified.
- Migrated `Artifact/src/Widgets/AudioMixerWidget.cppm` to `.cpp` and updated `Artifact/cmake/ArtifactSources.cmake`. The public `Artifact.Widgets.AudioMixer` interface, module declaration, imports, plugin editor UI, and mixer behavior are unchanged; no audio or transport behavior was edited. LF endings are enforced. Clang scanning and compilation remain unverified.
- Migrated `Artifact/src/Widgets/ArtifactCompositionAudioMixerPresentation.cppm` to `.cpp` and updated `Artifact/cmake/ArtifactSources.cmake`. This second implementation unit retains the existing `Artifact.Widgets.CompositionAudioMixer` module declaration, imports, presentation behavior, and Audio/Transport calls. LF endings are enforced. Clang scanning and compilation remain unverified.
- Migrated `Artifact/src/Widgets/Render/ArtifactContentGizmo.cppm` to `.cpp` and updated `Artifact/cmake/ArtifactSources.cmake`. The public `Artifact.Widgets.ContentGizmo` interface, module declaration, imports, and existing gizmo behavior are unchanged. Earlier individual compilation was blocked by missing provider PCMs; this migration has not rerun dependency scanning or compilation.
- Migrated `Artifact/src/Layer/ArtifactCloneLayer.cppm` to `.cpp` and updated `Artifact/cmake/ArtifactSources.cmake`. The public `Artifact.Layer.Clone` interface, module declaration, imports, clone generation, and renderer-abstraction path are unchanged; no renderer behavior was edited. LF endings are enforced. Clang scanning and compilation remain unverified.
- Completed the current `ArtifactPr` implementation migration: all 28 implementation manifest entries use `.cpp`; no implementation `.cppm` entries remain. This is a static source-manifest result only; compiler lanes are unverified.

### ArtifactPr implementation migration (2026-10-25)

- Migrated `ArtifactPr/src/ArtifactPrMainWindow.cppm` to `.cpp` and updated `ARTIFACT_PR_IMPL_SOURCES`. The implementation module declaration, imports, and UI behavior are unchanged; LF endings are preserved. Clang scanning and compilation remain unverified.
- `ArtifactPr` now has 26 `.cpp` and 2 `.cppm` implementation units in its 28-entry manifest. No CMake configure, build, or tests were run.
- Migrated `ArtifactPr/src/ArtifactPrEditorEngine.cppm` to `.cpp` and updated the implementation manifest. The editor engine module declaration, imports, and implementation are unchanged; LF endings are preserved. Clang scanning and compilation remain unverified.
- `ArtifactPr` now has 27 `.cpp` and 1 `.cppm` implementation unit in its 28-entry manifest; `GpuProgramMonitor.cppm` remains for a separate GPU-sensitive review. No CMake configure, build, or tests were run.
- Migrated `ArtifactPr/src/GpuProgramMonitor.cppm` to `.cpp` and updated the implementation manifest. The Diligent renderer implementation, module declaration, and imports were preserved unchanged; LF endings are preserved. No GPU behavior was edited. Clang scanning and compilation remain unverified.
- `ArtifactPr` now has 28 `.cpp` implementation units in its 28-entry manifest. No implementation `.cppm` files remain in ArtifactPr; no CMake configure, build, or tests were run.

# マイルストーン: Module 実装単位の `.cppm` → `.cpp` 段階移行と Clang / sccache 対応

**最終更新:** 2026-10-25
- Migrated four unregistered Artifact implementation sources and ten unregistered ArtifactCore sources (including non-module and test translation units) from `.cppm` to `.cpp`. These paths were absent from explicit build manifests; matching ArtifactCore module-reference metadata and its path-check helper were updated. No targets were enabled or behavior changed; LF endings are enforced.
- Moved four empty/non-module `ArtifactWidgets` `.cppm` translation units to `.cpp` and added them to the explicit migration source list used to stabilize the glob-based target inventory. They contain no module declarations or imports; no widget behavior changed. LF endings are enforced; no configure or build was run.
- Migrated `Artifact/src/Render/ArtifactRenderLayerPipeline.cppm` to `.cpp` and updated all active source references. The module declaration, imports, and existing rendering/view/layer behavior remain unchanged; LF endings are enforced. Clang scanning and compilation remain unverified.
- Migrated `Artifact/src/Widgets/ArtifactTimelineWidget.cppm` to `.cpp` and updated all active source references. The module declaration, imports, and existing rendering/view/layer behavior remain unchanged; LF endings are enforced. Clang scanning and compilation remain unverified.
- Migrated `Artifact/src/Widgets/Render/ArtifactCompositionEditor.cppm` to `.cpp` and updated all active source references. The module declaration, imports, and existing rendering/view/layer behavior remain unchanged; LF endings are enforced. Clang scanning and compilation remain unverified.
- Migrated `Artifact/src/Widgets/Render/ArtifactCompositionRenderController.cppm` to `.cpp` and updated all active source references. The module declaration, imports, and existing rendering/view/layer behavior remain unchanged; LF endings are enforced. Clang scanning and compilation remain unverified.
- Migrated `Artifact/src/Widgets/Viewer/ArtifactContentsViewer.cppm` to `.cpp` and updated all active source references. The module declaration, imports, and existing rendering/view/layer behavior remain unchanged; LF endings are enforced. Clang scanning and compilation remain unverified.
- Migrated `Artifact/src/Layer/ArtifactVideoLayer.cppm` to `.cpp` and updated all active source references. The module declaration, imports, and existing rendering/view/layer behavior remain unchanged; LF endings are enforced. Clang scanning and compilation remain unverified.
- Migrated `Artifact/src/Widgets/Timeline/ArtifactTimelineTrackPainterView.cppm` to `.cpp` and updated `Artifact/cmake/ArtifactSources.cmake`. The module declaration, imports, persistence/timeline behavior, and registration remain unchanged; LF endings are enforced. Clang scanning and compilation remain unverified; this extension-only move does not claim to resolve any prior frontend/resource failures.
- Migrated `Artifact/src/Project/ArtifactProject.cppm` to `.cpp` and updated `Artifact/cmake/ArtifactSources.cmake`. The module declaration, imports, persistence/timeline behavior, and registration remain unchanged; LF endings are enforced. Clang scanning and compilation remain unverified; this extension-only move does not claim to resolve any prior frontend/resource failures.
- Migrated `Artifact/src/Render/ArtifactRenderQueueService.cppm` to `.cpp` and updated active source / explicit module-reference paths. The module declaration, imports, startup/render service behavior, and metadata definitions remain unchanged; LF endings are enforced. Clang scan and compilation remain unverified.
- Migrated `Artifact/src/Render/ArtifactIRenderer.cppm` to `.cpp` and updated active source / explicit module-reference paths. The module declaration, imports, startup/render service behavior, and metadata definitions remain unchanged; LF endings are enforced. Clang scan and compilation remain unverified.
- Migrated `Artifact/src/AppMain.cppm` to `.cpp` and updated active source / explicit module-reference paths. The module declaration, imports, startup/render service behavior, and metadata definitions remain unchanged; LF endings are enforced. Clang scan and compilation remain unverified.
- Migrated `Artifact/src/Widgets/Render/TransformGizmo.cppm` to `.cpp` and updated `Artifact/cmake/ArtifactSources.cmake`. The implementation module, imports, and existing behavior remain unchanged; LF endings are enforced. Clang scan/compile remains unverified in this tree. Previous Clang implementation compile failures for this unit are recorded separately and are not claimed fixed by the extension migration.
- Migrated `Artifact/src/Preview/ArtifactPreviewCompositionPipeline.cppm` to `.cpp` and updated `Artifact/cmake/ArtifactSources.cmake`. The implementation module, imports, and existing behavior remain unchanged; LF endings are enforced. Clang scan/compile remains unverified in this tree. Previous Clang implementation compile failures for this unit are recorded separately and are not claimed fixed by the extension migration.
- Migrated `Artifact/src/Layer/ArtifactSolid2DLayer.cppm` to `.cpp` and updated `Artifact/cmake/ArtifactSources.cmake`. The implementation module, imports, and existing behavior remain unchanged; LF endings are enforced. Clang scan/compile remains unverified in this tree. Previous Clang implementation compile failures for this unit are recorded separately and are not claimed fixed by the extension migration.
- Migrated `Artifact/src/Widgets/ArtifactFilmstripWidget.cppm` to `.cpp` and updated `Artifact/cmake/ArtifactSources.cmake`. The implementation module, imports, and existing behavior remain unchanged; LF endings are enforced. Clang scan/compile remains unverified in this tree. Previous Clang implementation compile failures for this unit are recorded separately and are not claimed fixed by the extension migration.
- Migrated `Artifact/src/Undo/UndoManager.cppm` to `.cpp` and updated `Artifact/cmake/ArtifactSources.cmake`. The implementation module, imports, composition/undo behavior, and registration remain unchanged; LF endings are enforced. Clang scanning and compilation remain unverified.
- Migrated `Artifact/src/Composition/ArtifactAbstractComposition.cppm` to `.cpp` and updated `Artifact/cmake/ArtifactSources.cmake`. The implementation module, imports, composition/undo behavior, and registration remain unchanged; LF endings are enforced. Clang scanning and compilation remain unverified.
- Migrated `Artifact/src/Widgets/Timeline/ArtifactLayerPanelWidget.cppm` to `.cpp` and updated `Artifact/cmake/ArtifactSources.cmake`. The module declaration, imports, and existing service/timeline behavior remain unchanged; LF endings are enforced. Clang scanning and compilation remain unverified.
- Migrated `Artifact/src/Service/ArtifactProjectService.cppm` to `.cpp` and updated `Artifact/cmake/ArtifactSources.cmake`. The module declaration, imports, and existing service/timeline behavior remain unchanged; LF endings are enforced. Clang scanning and compilation remain unverified.
- Migrated `Artifact/src/Widgets/ArtifactMainWindow.cppm` to `.cpp` and updated `Artifact/cmake/ArtifactSources.cmake`. The module declaration, imports, feature guards, and existing UI/render behavior remain unchanged; LF endings are enforced. Clang scanning and compilation remain unverified.
- Migrated `Artifact/src/Widgets/Render/ArtifactCompositionRenderOverlay.cppm` to `.cpp` and updated `Artifact/cmake/ArtifactSources.cmake`. The module declaration, imports, feature guards, and existing UI/render behavior remain unchanged; LF endings are enforced. Clang scanning and compilation remain unverified.
- Full `Artifact/src` inventory still contains four tracked implementation `.cppm` files outside the source manifest: `ArtifactColorWheels`, `ArtifactStabilizer`, `ArtifactTimelineClock`, and `ArtifactTextRenderTarget`. No module import sites were found. The TextRenderTarget interface already defines its methods inline and its excluded runtime target lists only the interface; adding the `.cppm` risks duplicate definitions, so it was not registered. The other three remain untouched pending ownership/feature confirmation.
- Migrated `Artifact/src/Render/DiligentImmediateSubmitter.cppm` to `.cpp` and updated the source manifest, explicit render slice, and render module-reference inventory. The Diligent implementation, imports, module identity, and GPU/hot-path behavior are unchanged; LF endings are enforced. Clang scan and compilation remain unverified.
- Migrated `Artifact/src/Render/PrimitiveRenderer2D.cppm` to `.cpp` and updated the source manifest, explicit render slice, and render module-reference inventory. The Diligent implementation, imports, module identity, and GPU/hot-path behavior are unchanged; LF endings are enforced. Clang scan and compilation remain unverified.
- Migrated `Artifact/src/Widgets/ArtifactProjectManagerWidget.cppm` to `.cpp` and updated `Artifact/cmake/ArtifactSources.cmake`. The module declaration, imports, current widget behavior, and registration remain unchanged; LF endings are enforced. Clang scanning and compilation remain unverified.
- Migrated `Artifact/src/Widgets/ArtifactInspectorWidget.cppm` to `.cpp` and updated `Artifact/cmake/ArtifactSources.cmake`. The module declaration, imports, current widget behavior, and registration remain unchanged; LF endings are enforced. Clang scanning and compilation remain unverified.
- Migrated `Artifact/src/Widgets/Menu/ArtifactLayerMenu.cppm` to `.cpp` and updated `Artifact/cmake/ArtifactSources.cmake`. Module declaration, imports, existing menu behavior, and registration remain unchanged; LF endings are enforced. Clang scanning and compilation remain unverified.
- Migrated `Artifact/src/Widgets/CommandPalette/ArtifactCommandPaletteWidget.cppm` to `.cpp`, registered its `.ixx` and `.cpp` paths, and kept both out of `APP_MODULES` / `APP_IMPL` when the existing feature option is OFF. When enabled, the feature macro now reaches `ArtifactAppRuntime`, which compiles `ArtifactMainWindow`; Clang scanning therefore includes the optional implementation. Default-off behavior is preserved. LF endings are enforced; CMake configure and compilation remain unverified.
- Migrated `Artifact/src/Widgets/Render/ArtifactCompositionRenderWidget.cppm` to `.cpp` and updated `Artifact/cmake/ArtifactSources.cmake`. The module declaration, imports, viewport/timeline rendering behavior, and registration remain unchanged; LF endings are enforced. Clang scanning and compilation remain unverified.
- Migrated `Artifact/src/Widgets/Timeline/ArtifactDiligentTimelineRenderWindow.cppm` to `.cpp` and updated `Artifact/cmake/ArtifactSources.cmake`. The module declaration, imports, viewport/timeline rendering behavior, and registration remain unchanged; LF endings are enforced. Clang scanning and compilation remain unverified.
- Migrated `Artifact/src/Layer/Artifact3DModelLayer.cppm` to `.cpp` and updated `Artifact/cmake/ArtifactSources.cmake`. The module declaration, imports, layer behavior, and registration remain unchanged; LF endings are enforced. Clang scanning and compilation remain unverified.
- Migrated `Artifact/src/Layer/ArtifactTextLayer.cppm` to `.cpp` and updated `Artifact/cmake/ArtifactSources.cmake`. The module declaration, imports, layer behavior, and registration remain unchanged; LF endings are enforced. Clang scanning and compilation remain unverified.
- Migrated `Artifact/src/Worker/FarmWorkerMain.cppm` to `.cpp` and updated the conditional `ArtifactWorker` target plus the Clang direct-source scan list. Its module declaration, import, and worker behavior remain unchanged; LF endings are enforced. CMake configure and compilation remain unverified.
- Migrated `Artifact/src/Render/ArtifactTextGlyphSubmitter.cppm` to `.cpp` in both private runtime targets. The source is outside `ArtifactSources.cmake`, so the Clang implementation scan classifier now explicitly includes it; its `Artifact.Render.TextGlyphSubmitter` module declaration, imports, and rendering behavior are unchanged. LF endings are enforced; no CMake configure or compile was run.
- Migrated `Artifact/src/Widgets/Asset/ArtifactAssetBrowser.cppm` to `.cpp` and updated `Artifact/cmake/ArtifactSources.cmake`. Module implementation, imports, existing behavior, and registration remain unchanged; LF endings are enforced. Clang scanning and compilation remain unverified.
- Migrated `Artifact/src/Tool/ArtifactPuppetTool.cppm` to `.cpp` and updated `Artifact/cmake/ArtifactSources.cmake`. Module implementation, imports, existing behavior, and registration remain unchanged; LF endings are enforced. Clang scanning and compilation remain unverified.
- Migrated `Artifact/src/Widgets/AI/ArtifactAICloudWidget.cppm` to `.cpp` and updated `Artifact/cmake/ArtifactSources.cmake`. Module implementation, imports, existing UI behavior, and registration remain unchanged; LF endings are enforced. Clang scanning and compilation remain unverified.
- Migrated `Artifact/src/Widgets/ArtifactExpressionCopilotWidget.cppm` to `.cpp` and updated `Artifact/cmake/ArtifactSources.cmake`. Module implementation, imports, copilot UI behavior, and registration remain unchanged; LF endings are enforced. This changes the source from Clang's former `.cppm` path; Clang scanning and compilation are unverified.
- Migrated `Artifact/src/Widgets/ArtifactPropertyWidget.cppm` to `.cpp` and updated `Artifact/cmake/ArtifactSources.cmake`. The implementation module, imports, existing UI behavior, and registration remain unchanged; LF endings are enforced. Clang scanning and compilation remain unverified.
- Migrated `Artifact/src/Widgets/Menu/ArtifactViewMenu.cppm` to `.cpp` and updated `Artifact/cmake/ArtifactSources.cmake`. The implementation module, imports, existing UI behavior, and registration remain unchanged; LF endings are enforced. Clang scanning and compilation remain unverified.
- Migrated `Artifact/src/Widgets/Dialog/ApplicationSettingDialog.cppm` to `.cpp` and updated `Artifact/cmake/ArtifactSources.cmake`. The implementation module, imports, behavior, and registration remain unchanged; LF endings are enforced. Clang scanning and compilation remain unverified.
- Migrated `Artifact/src/Service/ArtifactEffectService.cppm` to `.cpp` and updated `Artifact/cmake/ArtifactSources.cmake`. The implementation module, imports, behavior, and registration remain unchanged; LF endings are enforced. Clang scanning and compilation remain unverified.
- Migrated `Artifact/src/Service/ArtifactAudioService.cppm` to `.cpp` and updated `Artifact/cmake/ArtifactSources.cmake`. Transport ownership, module imports, audio service behavior, and registration remain unchanged; LF endings are enforced. Clang scanning and compilation remain unverified.
- Migrated `Artifact/src/Layer/ArtifactAbstractLayer.cppm` to `.cpp` and updated `Artifact/cmake/ArtifactSources.cmake`. The shared layer implementation module, imports, behavior, and registration remain unchanged; LF endings are enforced. Clang scanning and compilation remain unverified.
- Migrated `Artifact/src/Layer/ArtifactAbstractLayerPropertyRouting.cppm` to `.cpp` and updated `Artifact/cmake/ArtifactSources.cmake`. Module implementation, imports, property routing behavior, and registration remain unchanged; LF endings are enforced. Clang scanning and compilation remain unverified.
- Migrated `Artifact/src/Widgets/Render/ArtifactLayerEditorWidget.cppm` to `.cpp` and updated `Artifact/cmake/ArtifactSources.cmake`. The implementation module, imports, existing UI behavior, and registration remain unchanged; LF endings are enforced. Clang scanning and compilation remain unverified.
- Migrated `Artifact/src/Widgets/Control/ArtifactPlaybackControlWidget.cppm` to `.cpp` and updated `Artifact/cmake/ArtifactSources.cmake`. The implementation module, imports, existing UI behavior, and registration remain unchanged; LF endings are enforced. Clang scanning and compilation remain unverified.
- Migrated `Artifact/src/Widgets/Diagnostics/AppDebuggerWidget.cppm` to `.cpp` and updated Artifact CMake source registrations. The module declaration, imports, existing implementation, and behavior remain unchanged; LF endings are enforced. Clang scanning and compilation remain unverified.
- Migrated `Artifact/src/Widgets/Color/ArtifactColorSciencePanel.cppm` to `.cpp` and updated Artifact CMake source registrations. The module declaration, imports, existing implementation, and behavior remain unchanged; LF endings are enforced. Clang scanning and compilation remain unverified.
- Migrated `Artifact/src/Widgets/Dialog/CreatePlaneLayerDialog.cppm` to `.cpp` and updated `Artifact/cmake/ArtifactSources.cmake`. The implementation module declaration, imports, existing behavior, and registration remain unchanged; LF endings are enforced. Clang scanning and compilation remain unverified.
- Migrated `Artifact/src/Layer/ArtifactParametricCompositionLayer.cppm` to `.cpp` and updated `Artifact/cmake/ArtifactSources.cmake`. The implementation module declaration, imports, existing behavior, and registration remain unchanged; LF endings are enforced. Clang scanning and compilation remain unverified.
- Migrated `Artifact/src/Widgets/ArtifactPropertyWidgetShared.cppm` to `.cpp` and updated `Artifact/cmake/ArtifactSources.cmake`. The module implementation, imports, existing behavior, and registration remain unchanged; LF endings are enforced. Clang scanning and compilation remain unverified.
- Migrated `Artifact/src/Layer/ArtifactAbstractLayerPersistence.cppm` to `.cpp` and updated `Artifact/cmake/ArtifactSources.cmake`. The module implementation, imports, existing behavior, and registration remain unchanged; LF endings are enforced. Clang scanning and compilation remain unverified.
- Migrated `Artifact/src/Layer/ArtifactAbstractLayerPhysicsRouting.cppm` to `.cpp` and updated `Artifact/cmake/ArtifactSources.cmake`. The module implementation, imports, layer behavior, and registration remain unchanged; LF endings are enforced. Clang scanning and compilation remain unverified.
- Migrated `Artifact/src/Layer/ArtifactAbstractLayerComponentRouting.cppm` to `.cpp` and updated `Artifact/cmake/ArtifactSources.cmake`. The module implementation, imports, layer behavior, and registration remain unchanged; LF endings are enforced. Clang scanning and compilation remain unverified.
- Migrated `Artifact/src/Layer/ArtifactAbstractLayerTransform.cppm` to `.cpp` and updated `Artifact/cmake/ArtifactSources.cmake`. The module implementation, imports, layer behavior, and registration remain unchanged; LF endings are enforced. Clang scanning and compilation remain unverified.
- Migrated `Artifact/src/Layer/ArtifactAbstractLayerLayoutRouting.cppm` to `.cpp` and updated `Artifact/cmake/ArtifactSources.cmake`. The module implementation, imports, layer behavior, and registration remain unchanged; LF endings are enforced. Clang scanning and compilation remain unverified.
- Migrated `Artifact/src/Widgets/Render/ArtifactTimelineLayerTestWidget.cppm` to `.cpp` and updated `Artifact/cmake/ArtifactSources.cmake`. The module implementation, imports, widget behavior, and registration remain unchanged; LF endings are enforced. Clang scanning and compilation remain unverified.
- Migrated `Artifact/src/Widgets/ArtifactSecondaryPreviewWindow.cppm` to `.cpp` and updated `Artifact/cmake/ArtifactSources.cmake`. The module implementation, imports, widget behavior, and registration remain unchanged; LF endings are enforced. Clang scanning and compilation remain unverified.
- Migrated `Artifact/src/Layer/ArtifactAbstractLayerPropertyGroups.cppm` to `.cpp` and updated `Artifact/cmake/ArtifactSources.cmake`. The module implementation, imports, layer behavior, and registration remain unchanged; LF endings are enforced. Clang scanning and compilation remain unverified.
- Migrated `Artifact/src/Layer/ArtifactAbstractLayerFractureRuntime.cppm` to `.cpp` and updated `Artifact/cmake/ArtifactSources.cmake`. The module implementation, imports, layer behavior, and registration remain unchanged; LF endings are enforced. Clang scanning and compilation remain unverified.
- Migrated `Artifact/src/Layer/ArtifactAbstractLayerImpl.cppm` to `.cpp` and updated `Artifact/cmake/ArtifactSources.cmake`. The module implementation, imports, layer behavior, and registration remain unchanged; LF endings are enforced. Clang scanning and compilation remain unverified.
- Migrated `Artifact/src/Layer/ArtifactLayerFactory.cppm` to `.cpp` and updated `Artifact/cmake/ArtifactSources.cmake`. The module implementation, imports, layer factory behavior, and registration remain unchanged; LF endings are enforced. Clang scanning and compilation remain unverified.
**ステータス:** In Progress

## 目的

C++20 module の境界と公開 API（主に `.ixx`）を維持しながら、module implementation unit の拡張子を `.cppm` から `.cpp` へ少しずつ移す。同時に MSVC、clang-cl、本家 clang (`clang++`) のビルド経路を整え、キャッシュ可能な構成では sccache を活用する。

一括リネームは行わない。拡張子による暗黙の分類を減らし、ソースの module 宣言と明示 manifest を基準にする。

## スコープ

- `module X;` で始まる実装単位を対象にし、まず依存の少ないファイルを1つずつ `.cpp` に移す。
- `export module X;` を持つ primary interface / partition は、分類と依存スキャンが安定するまで `.ixx` / `.cppm` に残す。
- CMake の module source manifest、依存スキャン、MSVC の `CompileAsCpp` 回避策を compiler-neutral な分類へ段階的に整理する。
- clang-cl と本家 clang のコンパイル、リンク、module dependency scan を別の検証レーンとして扱う。
- sccache の利用可否と実際の cache hit を compiler・translation unit 種別ごとに記録する。非対応時は通常コンパイルへフォールバックし、正しさをキャッシュに依存させない。

## 現状監査（2026-10-22）

### ソース量の目安

Tracked source file を宣言行で分類した数。各移行ログの値は対応する ArtifactCore commit tree を直接集計した。`.cpp` は拡張子で数え、`.cppm` は宣言行で分類している。ビルド対象数そのものではない。

| リポジトリ | `.cpp` | `export module` のある `.cppm` | 実装形式の `.cppm` | module 宣言のない `.cppm` | `.ixx` |
|---|---:|---:|---:|---:|---:|
| 親 ArtifactStudio | 328 | 0 | 27 | 0 | 29 |
| Artifact | 27 | 55 | 609 | 6 | 700 |
| ArtifactCore | 162 | 45 | 314 | 20 | 851 |
| ArtifactWidgets | 47 | 0 | 0 | 4 | 63 |

### CMake の現行責務

- ルート `CMakeLists.txt` はローカル module target の `CMAKE_CXX_SCAN_FOR_MODULES` を有効にし、clang-cl 用に `clang-scan-deps` を探す。ただし探索先には Windows / Visual Studio 固有パスが含まれる。
- ルートでは `ARTIFACT_USE_SCCACHE` が既定 ON。本家 clang++ GNU frontend は launcher を使い、clang-cl と MSVC-style frontend は module/PDB 対応が不足するため launcher を無効化する。非対応 module command は sccache が bypass する。
- `ArtifactCore/cmake/ArtifactCoreSources.cmake` は source manifest を明示管理し、`.cppm` を interface sources と implementation sources に分けている。新しい実装 `.cpp` を登録する規則は別途必要。
- `ArtifactWidgets/CMakeLists.txt` は `.cpp` / `.cppm` / `.ixx` を glob し、`export module` の有無で module / implementation に分類する。MSVC では実装 `.cppm` に `CompileAsCpp` を付けている。
- `Artifact/CMakeLists.txt` は manifest source を宣言内容で分類する。`.cppm` には MSVC `CompileAsCpp` / upstream clang `-x c++` を適用し、移行済み `.cpp` module implementation には clang-cl / upstream clang の P1689 scan を明示する。

### 外部仕様から分かること

- CMake 3.30 は CXX module source を内容スキャンし、CXX として認識される拡張子を対象にする。拡張子そのものは module 種別を決めない。ただし compiler と generator の対応が前提で、Ninja / Ninja Multi-Config / Visual Studio generator の制約を確認する必要がある。
- [CMake 4.4 release notes](https://cmake.org/cmake/help/latest/release/4.4.html) と [C++ modules manual](https://cmake.org/cmake/help/latest/manual/cmake-cxxmodules.7.html) では、clang-cl 19.1+ の named modules 対応は 4.4 で追加され、Visual Studio generator は対象外。プロジェクト最小の CMake 3.30 は clang-cl module scanning の基準にできない。
- [sccache README](https://github.com/mozilla/sccache/blob/main/README.md#c20-modules) は Clang named modules を部分対応とし、Clang の `-fmodule-file` / `-fmodule-output` / `--precompile` 等を cache 対象候補に挙げる。MSVC `/interface` / `/ifcOutput` 等の IFC と GCC modules は未対応。CMake の `@*.modmap` response-file を渡す Clang module command は [sccache issue #2650](https://github.com/mozilla/sccache/issues/2650) で非 cacheable と報告されており、現行版での解消状態を cache stats と `--show-stats` で確認する。
- Clang の module dependency discovery は `clang-scan-deps` による P1689 scan を前提とする。

## 検証マトリクス

| レーン | Host / compiler | CMake / generator | Module scan | sccache の期待値 | 状態 |
|---|---|---|---|---|---|
| 基準 | Windows / MSVC `cl.exe` | CMake 4.4.4 / Ninja | IFC 経路 | plain `.cpp` は単独 command で miss→hit、module interface / `/reference` 実装は bypass | ContentViewer の隔離 DLL、ArtifactCoreEnvironment archive、launcher 無効構成での ArtifactWidgets.dll link が成功。sccache opt-in の並列 OpenUSD `tf.pdb` compile は C1041/C1090 で失敗 |
| Windows LLVM | Windows / clang-cl | CMake 4.4.4 / Ninja | `clang-scan-deps` と P1689 を確認 | ArtifactCoreEnvironment implementation は1 miss→1 hit。interface は `multiple input files` で bypass | Pane / DialogPlacement / ToggleGlyphButton の隔離 DLL と ArtifactCoreEnvironment archive が成功。root 全体は未完了 |
| **先行対象** | Windows / upstream `clang++`、MSVC ABI (`--target=x86_64-pc-windows-msvc`) | CMake 4.4.4 / Ninja | 実 target の P1689 scan と `.cpp` implementation を確認 | Core target 3 miss→3 hit。ToggleGlyphButton は2 miss→2 hit。過去 pilot の warm-cache hit は別記 | Environment target archive と ClickableLabel / DialogPlacement / ToggleGlyphButton の隔離 DLL 成功。ArtifactCore target は OpenUSD `arch` の `demangle.cpp` / `errno.cpp` で停止 |
| LLVM frontend 比較 | Windows / `clang-cl` | CMake 4.4.4 / Ninja | `clang-scan-deps` と P1689 を確認 | `.cpp` hit、module interface bypass を実測 | sccache smoke 成功、root full target 未完了 |
| 後続候補 | Linux / `clang++` GNU frontend | 3.30 以上 / Ninja | P1689 scan、`.cpp` implementation unit を確認 | Clang named-module 対応範囲で段階検証 | 未実行 |

### `.cpp` implementation-unit pilot（ArtifactWidgets / MSVC）

- `ArtifactWidgets/src/Dock/Pane.cppm` の内容を保ったまま `Pane.cpp` へ移し、`ArtifactWidgets/CMakeLists.txt` の明示 source 登録と `.cpp eol=lf` を追加した。
- 既存 `out/build/x64-Debug-Agent` を再構成した MSVC + Ninja で `Pane.ixx` の依存スキャン後に `src/Dock/Pane.cpp.obj` が正常にコンパイルされた。よって `.cpp` が implementation source として扱われ、module interface の依存も解決することを確認した。
- `ArtifactWidgets.dll` の最終リンクは CMake の `vs_link_dll` 内で manifest resource compiler が `no such file or directory` と報告して停止した。同じ `rc.exe` と `manifest.rc` を手動実行すると成功するため、最終成果物のリンク成功は未確認であり、MSVC lane 全体の受け入れとはしない。
- ビルドは既存 MSVC 構成の依存全体を再構築し 2,111 action に達した。既存コード由来の警告が複数出たが、`Pane.cpp` のコンパイル失敗はなかった。
- 追加 pilot として `ArtifactWidgets/src/Common/AlignmentComboBox.cppm` を `AlignmentComboBox.cpp` へ移し、CMake の implementation source list に登録した。CMake 4.4.4 / clang-cl / Qt6 の隔離 target で実ソースを compile/link した。Widgets 全体の再リンクは未確認。
- `ArtifactWidgets/src/Common/ClickableLabel.cppm` を `ClickableLabel.cpp` へ移し、同じ明示 implementation source list に登録。CMake 4.4.4 / Ninja / clang-cl 22.1.3 + Qt6 の隔離 DLL を compile/link。続けて upstream clang++ GNU frontend + Windows MSVC ABI の Qt6 隔離 DLL も compile/link 成功。Verdigris `W_OBJECT_IMPL` を含む実装で両 frontend の module scan / object compile / link を確認した。
- upstream clang++ ClickableLabel pilot を sccache launcher 下で clean/build 2回実行し、interface `.ixx` と implementation `.cpp` の4 compile request がすべて hit。直前の同一 command の compile cache が既に存在していたため、同 pilot での fresh miss は測っていない。
- `ArtifactWidgets/src/Common/DialogPlacement.cppm` を `.cpp` へ移行し、明示 source list に登録。event filter と dialog centering を含む source を clang-cl と upstream clang++ の両方で scan / compile / Qt6 DLL link。upstream clang++ の sccache 下で clean/build 2回、interface / implementation 合計4 compile hit。cache entry はその前の初回 pilot build で作成済み。
- `ArtifactWidgets/src/Common/ToggleGlyphButton.cppm` を `.cpp` へ移行。CMake の明示 source list に追加し、clang-cl と upstream clang++ + MSVC ABI の両方で P1689 scan / interface・implementation compile / Qt6 DLL link 成功。upstream clang++ + sccache 0.18 で clean build 2回、初回2 miss・2回目2 hit、bypass なし。
- `ArtifactWidgets/src/Viewer/ContentViewer.cppm` を `.cpp` へ移行。明示 source list に追加し、CMake 4.4.4 / Ninja / Qt6 で両 Clang frontend の P1689 scan、interface / implementation compile、DLL link が成功。同じ隔離 target は MSVC 19.51 でも IFC scan / compile / DLL link 成功。sccache を通した2回の MSVC build では `-interface` 1件と `/reference` 1件が各回 bypass。
- `ArtifactWidgets/src/Time/TimeCodeLabel.cppm` を `.cpp` へ移行し、明示 implementation source list に追加した。公開 `Widgets.TimeCodeLabel` interface はそのまま保持し、実装本文は拡張子移行と LF 改行だけを変更した。
- `ArtifactWidgets/src/Color/ColorViewLabel.cppm` を `.cpp` へ移行し、明示 implementation list に追加した。公開 interface、`W_OBJECT_IMPL`、Impl 所有、color accessors、paintEvent の実装を維持し、本文を変えず LF 改行へ正規化した。
- `ArtifactWidgets/src/Common/CollapsibleSection.cppm` を `.cpp` へ移行し、明示 implementation list に追加した。公開 `CollapsibleSection` interface の全 method definition、`W_OBJECT_IMPL`、Impl 所有を保ち、本文を変えず LF 改行へ正規化した。
- 追加で clang-cl の実ソース経路を root `ArtifactCoreEnvironment` target で検証。CMake 4.4.4 / Ninja が `EnvironmentVariable.cpp` の P1689 scan と dyndep を生成し、clang-cl compile と static library link に成功。clang-cl では移行済み `.cpp` のみを scan 対象にし、GNU frontend の `-x c++` は付けない。

### ArtifactCore EnvironmentVariable / upstream clang++

- `ArtifactCore/src/EnvironmentVariable/EnvironmentVariable.cppm` を `.cpp` へ移し、source manifest、module reference map、split target の source path を更新した。ArtifactCore の `.cpp` に LF 指定を追加。
- Upstream Clang 22.1.3 GNU frontend + `x86_64-pc-windows-msvc` ABI、CMake 4.4.4 / Ninja で root configure と `ArtifactCoreEnvironment` target build に成功。vcpkg `FindStb.cmake` の directory を `CMAKE_MODULE_PATH` に足す必要があった。P1689 scan、`.cpp` compile、static library archive まで確認。
- `.cppm` implementation が clang++ で interface 扱いされないよう `-x c++` を付ける。Clang GNU lane では MSVC 用 `/reference` と source `OBJECT_DEPENDS` の `.ifc/.obj/.dd` 手動依存を除外し、CMake の module map / dyndep に任せる。これにより `clang-scan-deps` の正しい module attachment requirement と既存 MSVC object ordering が競合していた cycle を解消。
- `ArtifactCoreEnvironment` に限った検証であり、他 split targets、Qt / Diligent 統合、アプリ全体 link は未確認。

- 追加確認として `ArtifactCoreColorCollection` target を起動した。`arch` target の `stackTrace.cpp` は親 CMake から `NOMINMAX` を与えると compile できた。一方 OpenUSD `arch` は `demangle.cpp` の `cxxabi.h` / Itanium ABI 分岐、`errno.cpp` の `strerror_r`、`attributes.cpp` の compiler/platform 分岐で停止した。OpenUSD submodule は変更していない。試した `-U__clang__` target flag は MSVC STL の Clang builtin detection を壊すため撤回。ColorCollection の source compile には到達せず、別 split target の module 設定は未確認。

### sccache を使った実 target 再ビルド（2026-10-22）

- root configure で `ARTIFACT_USE_SCCACHE=ON` と sccache 0.18.0 の実行ファイルを明示し、GNU frontend launcher として認識された。OpenUSD の `arch` target には Windows SDK `min/max` macro を抑止する `NOMINMAX` を親側から追加し、`stackTrace.cpp` object の compile 成功を確認。
- `ArtifactCoreEnvironment` を sccache 下で2回 clean/build。1回目は3 compile miss、2回目は `.ixx` 2件と implementation `.cpp` 1件の3 hit。累計 6 compile requests / 3 hits / 3 misses / 0 non-cacheable calls。両方 static archive まで成功。
- clang-cl lane も opt-in (`ARTIFACT_USE_SCCACHE_WITH_CLANG_CL=ON`) で `ArtifactCoreEnvironment` を2回 clean/build。今回の2回分は合計6 compile requests、実装 `.cpp` が1 miss・1 hit、module interface が `multiple input files` により各回2件 bypass。両回で P1689 scan と static archive 生成に成功。
- cache hit 時も `ArtifactCoreEnvironment.lib` の生成と Ninja dyndep scan を確認。異なる build の成果物との bytewise 比較、広い target の cache behavior は未確認。
- MSVC 19.51 の ContentViewer 隔離 target を sccache 0.18 経由で2回 build。各回の module interface (`-interface`) と module implementation (`/reference`) は bypass (合計4 calls)。通常 `plain_implementation.cpp` は単独 compile で1 miss→1 hit。MSVC root configure で launcher を opt-in にし `ArtifactCoreEnvironment` archive は成功したが、`ArtifactWidgets` build は OpenUSD `tf` の同一 PDB へ複数 compile が並列書き込みする時に `C1041/C1090` で停止した。同 compile は sccache 経由の単独実行と直接 cl.exe では成功し、parallel compile の launcher 相互作用が疑われる。MSVC 用 launcher opt-in は root CMake から撤去し既定無効を維持した。launcher 無効構成では dedicated build tree で `ArtifactWidgets` target が 576 action を完了し、`bin/Debug/ArtifactWidgets.dll` の link に成功した。

### clang-cl: CMake 4.4 + actual Pane source

- CMake 4.2.2 は clang-cl の module scan/generate rule を提供せず、CMake 4.4.4 では同じ lane の rule が利用できた。clang-cl を使うルート CMake 構成は 4.4 以上と Ninja に限定する gate を加えた。ArtifactCore の移行済み `.cpp` で実 target scan / compile / archive も確認した。
- CMake 4.4.4 / clang-cl 22.1.3 / Ninja で、実際の `ArtifactWidgets/src/Dock/Pane.cpp` と `include/Dock/Pane.ixx` をコンパイルし、Qt6 Core / Widgets をリンクした `PaneClangClSmoke.dll` が生成された。
- Root configure は OpenMP の flags、`libomp` 名、VS toolset の `libomp.lib` パスを build-dir cache で明示すると configure/generate まで通った。既定の `find_package(OpenMP)` compiler probe 自動検出は引き続き別課題。別の root configure 再試行では vcpkg manifest install が長時間走ったため中断した。
- Root の実 target build は OpenUSD static build 中に clang-cl が `/wddeprecated`, `/wdunused-local-typedefs` など無効な warning id を拒否して停止した。これは Pane の compile 失敗ではなく、third-party compiler options を clang-cl 向けに切り分ける必要がある。
- したがって clang-cl の実ファイル compile/link feasibility と `ArtifactCoreEnvironment` は確認済み。root 全体 build と OpenMP 自動検出は未成立。

### sccache v0.18 実測

- Windows sccache v0.18.0 で upstream clang++ の通常 `.cpp` forced rebuild は 1 hit / 0 miss。module smoke の `.ixx` interface、importer `.cpp`、implementation `.cpp` は各 1 hit となり、build/run exit 0。
- clang-cl の module smoke では `.ixx` interface が `multiple input files` で non-cacheable、importer `.cpp` と implementation `.cpp` は 2 hits / 0 misses。build/run exit 0。
- 初回 compile はそれぞれ cache miss で完了し、非対応 command の bypass を確認した。従って clang-cl は sccache opt-in の現行方針を保ち、cache hit を前提条件にしない。
- P1689 の `requires: Smoke.Greeting` は primary module attachment の正規情報だった。最小 CMake/Ninja smoke では処理でき、root split target cycle は ArtifactCore に残っていた MSVC `OBJECT_DEPENDS` / `/reference` 手動依存との重複に起因した。
- GNU frontend lane で implementation scan を有効に戻し、MSVC 固有 module reference と file-level object edges を除外した後、`ArtifactCoreEnvironment` の実 target で `.cppm` と `.cpp` の両方の scan/build が成功。scan を無効化する回避策は採用しない。
- **次の作業:** OpenUSD `CXXDefaults.cmake` に GNU-like Clang + MSVC ABI を区別する公開 option は見つからず、compiler ID `Clang` は `clangdefaults.cmake` へ分岐する。現状は split target を止めるため、module map dependency ordering は未確認。clang-cl root build の third-party warning flags、OpenMP 自動検出、全体 link も残る。
- CMake 4.2.2 の `Compiler/Clang-CXX.cmake` は `CMAKE_CXX_SCANDEP_SOURCE` と Clang module map / BMI flags を GNU frontend の場合だけ設定する。MSVC frontend へ同じ GNU-style `-x c++`, `-MD`, `-MF` を流用すると clang-cl が未対応引数として警告する。MSVC frontend 用の P1689 command translation / module map flags、または CMake の対応版更新が必要。
- これは pilot の `.cpp` compile エラーではない。clang-cl lane は未成立。

ユーザー決定により **Windows upstream `clang++` + MSVC ABI を先行対象**とする。Windows clang++ は **MSVC ABI と Windows SDK / vcpkg triplet (`x64-windows`) を共有し、GNU driver frontend として動かすレーン**を先に成立させる。`clang-cl` と同一視せず、CMake の `MSVC` 判定、MSVC 専用 compiler options / `/reference` / `std.ifc`、ATL・SDK の探索、Qt / Diligent / vcpkg の ABI・toolchain 整合を個別に監査する。先行受入は Core / Widgets の小さな module subset で、GUI 全体は別の段階とする。

### Windows clang++ (MSVC ABI) feasibility gate

- この環境には Visual Studio LLVM 配下の `clang++.exe` と `clang-scan-deps.exe` がある。ただし導入済み実体が upstream LLVM 配布版と同一のオプション・module 対応とは限らないため、配布元と版を確認してから preset に固定する。
- CMake は GNU frontend と認識するため、既存 `ARTIFACT_FRONTEND_MSVC` 経路には自動で入らない。一方、プロジェクトには `MSVC` 判定に結びついた Windows SDK / ATL セットアップ、MSVC `/reference`・`.ifc` の供給、MSVC module flag が多数残る。GNU frontend で必要な `--target`、MSVC STL include / library paths、COFF linker / resource compiler の選択、および各依存ライブラリの ABI を設計する必要がある。
- vcpkg の `x64-windows` ABI は方向性として合うが、現在の `cmake/toolchain-msvc-v143.cmake` は `cl.exe` と v143 の検出を行う。clang++ 用にこの toolchain を流用せず、compiler/toolchain の初期化を分ける。
- Windows clang++ を `--driver-mode=cl` で MSVC frontend として扱う案は upstream `clang++` の GNU driver という要件から逸れるため、先行対象には採らない。
- 手元の CMake 4.2.2 `Compiler/Clang-CXX.cmake` は GNU frontend の Clang 16+ に P1689 scan rule を用意する。`Windows-Clang.cmake` は Windows + GNU frontend を MinGW/GNU toolchain lane として選び、`-fuse-ld=lld-link` と GNU-style link rules を構成するため、GNU frontend のままで MSVC ABI を使うには linker/runtime/library rules を toolchain で明示上書きする必要がある。これはプロジェクト全体に preset を追加する前に、隔離 smoke project で検証する。
- Windows clang++ の driver probe (`--target=x86_64-pc-windows-msvc` なし) は MSVC 互換 target と Windows SDK header paths を選び、MSVC 14.51 と ATL include は VS Insiders 側を指した。smoke toolchain もこの toolset に合わせた。vcpkg manifest は `x64-windows` のまま使える見込みだが、CMake ABI tag と compiler hash が一致するか未確認。
- GNU-style `-###` link command は MSVC CRT defaultlib と SDK / toolset library paths を示した。CMake 4.2.2 の既定 Windows GNU-frontend rule にある `-nostartfiles` / `-nostdlib` が COFF link と衝突したため、`CMAKE_USER_MAKE_RULES_OVERRIDE_CXX` で GCC/MinGW の標準 library 初期値と Clang の MSVC ABI linker defaults を設定し、Clang driver の `lld-link` 経路を維持する smoke 用 rules override を作成した。
- ルート + `ArtifactCore` / `ArtifactWidgets` / `Artifact` / `DockFoundation` の CMake 監査では、explicit `/reference;Module=...ifc`、MSVC std.ifc producer、MSVC `/std` `/experimental:module`、`VS_SETTINGS CompileAsCpp` に加え、MSVC linker 専用 link option と library/layout 前提が見つかった。MSVC frontend を真似て compiler variant を上書きするのではなく、Clang GNU frontend 用の標準 module map と compile rule を保ちつつ、Windows MSVC ABI の COFF link/runtime/search path を toolchain 側で設定する必要がある。上記 module-reference source overrides は Clang lane では除外・置換する設計が必要で、repo-wide GNU frontend 対応は小さな subset の成功後に扱う。
- 実行環境には CMake 4.2.2、clang++ / clang-cl / clang-scan-deps 22.1.3、Ninja、MSVC 14.51、Windows SDK 10.0.26100、`C:/vcpkg/installed/x64-windows` が存在する。sccache v0.18.0 は isolated cache smoke で upstream clang++ の `.cpp` hit と module interface / importer / implementation hit を記録した。
- **合格条件:** CMake が `Clang` + GNU frontend と検出し、Windows MSVC ABI の最小 smoke module provider / importer が Ninja dyndep で順序付けられ、`.cpp` implementation unit をコンパイル・リンクできる。続いて Qt / vcpkg / Diligent の最小依存を一つずつ追加する。
- **結果 (2026-10-21):** `tools/clang-msvc-abi-smoke` と `cmake/toolchain-clangxx-msvc-abi.cmake` で CMake 4.2.2 + Ninja 1.12.1 + clang++ 22.1.3 の configure/build に成功した。GNU frontend / `x86_64-pc-windows-msvc` を確認し、通常 importer と、別 primary module `Smoke.Greeting` に属する `module Smoke.Implementation;` 実装単位の両方から `Smoke.Greeting` を import する構成で compile/link と Windows 実行 exit 0 を確認した。実装ユニット自身が primary `Smoke.Greeting` に attachment する場合は clang-scan-deps 22.1.3 が自己 `requires` を出力し、CMake 4.4.4 / Ninja で cycle を作ることも確認した。
- **制約:** isolated smoke は独立 primary module の実装を検証したが、ArtifactCore の多くを占める同名 primary interface attachment は scanner cycle により未成立。ArtifactCore / Widgets 全体の `/reference`・`std.ifc`・compiler option / link 統合、vcpkg / Qt / Diligent の本家 clang++ 対応は未確認。

## フェーズと合格条件

### Phase 0: 分類と基準を固定

- `.ixx` interface、`.cppm` interface / partition、`.cppm` implementation、通常 `.cpp` を manifest 上で区別する。
- compiler ID、frontend variant、CMake version、generator、`clang-scan-deps` の実体を configure summary で確認できるようにする。
- 現行 MSVC 構成の module scan / link baseline を採取する。
- **合格:** 1ファイルの移行後に、提供 module と import dependency の登録先が一意に追跡できる。

### Phase 1: 小さな implementation unit を `.cpp` に移す

- 依存が少なく、対応する public interface が明確な implementation unit を選ぶ。
- `.cppm` から `.cpp` への rename と、そのファイルを列挙する manifest 変更だけに差分を限定する。
- `.cpp` でも module scan が有効であり、provider / importer の dyndep 順序が維持されることを確認する。
- **合格:** MSVC と clang-cl、本家 clang の対象 subset でコンパイル・リンクが成立し、同名 module / unresolved symbol / stale BMI がない。

#### Pilot 1: `ArtifactWidgets/src/Dock/Pane.cppm` → `.cpp`（2026-10-21）

- `module DockWidget;` の implementation unit で、interface は `include/Dock/Pane.ixx`。
- `W_OBJECT_IMPL(Pane)`、constructor、destructor が実装ファイルに各1箇所あることを静的確認した。移動ファイルには元ファイルと同じ内容・BOMを維持し、改行のみ LF へ正規化した。Git working tree が CRLF だった旧 `.cppm` と新 `.cpp` の blob hash は異なるため、内容・BOM と LF-only を別々に確認した。
- `ArtifactWidgets/CMakeLists.txt` の `src/*.cpp` glob と、pilot `.cpp` を `WIDGETS_IMPL` に明示追加する manifest を確認した。明示登録で CMakeLists の変更を発生させ、次回 configure で source list が更新される。MSVC の `CompileAsCpp` 指定は `.cppm` のみを対象とする。
- `ArtifactWidgets/.gitattributes` に `*.cpp text eol=lf` を追加し、このサブモジュールで `.cpp` の LF を固定した。
- **状態:** MSVC + Ninja で `.cpp` の module dependency scan / object compile 済み。最終 DLL link は manifest resource wrapper エラーで未完了、runtime も未確認。

### `.cpp` implementation-unit pilot（ArtifactWidgets / MSVC）

- `ArtifactWidgets/src/Dock/Pane.cppm` の内容を保ったまま `Pane.cpp` へ移し、`ArtifactWidgets/CMakeLists.txt` の明示 source 登録と `.cpp eol=lf` を追加した。
- 既存 `out/build/x64-Debug-Agent` を再構成した MSVC + Ninja で `Pane.ixx` の依存スキャン後に `src/Dock/Pane.cpp.obj` が正常にコンパイルされた。よって `.cpp` が implementation source として扱われ、module interface の依存も解決することを確認した。
- `ArtifactWidgets.dll` の最終リンクは CMake の `vs_link_dll` 内で manifest resource compiler が `no such file or directory` と報告して停止した。同じ `rc.exe` と `manifest.rc` を手動実行すると成功するため、最終成果物のリンク成功は未確認であり、MSVC lane 全体の受け入れとはしない。
- ビルドは既存 MSVC 構成の依存全体を再構築し 2,111 action に達した。既存コード由来の警告が複数出たが、`Pane.cpp` のコンパイル失敗はなかった。

### clang-cl root configure blocker

- clang-cl の root configure は既存 scan override の下で ArtifactWidgets の `find_package(OpenMP)` 内部 `try_compile` に到達するが、CMake `FindOpenMP` の試験コンパイルが `clang-scan-deps` へ clang-cl 固有の引数を渡せず `.ddi` 生成に失敗した。
- これは pilot の `.cpp` compile エラーではない。`OpenMP` 検出時の scanner / try-compile 境界を別途解決する必要があり、clang-cl lane は未成立。- OpenMP の候補 flags / libs / spec date (`-Xclang -fopenmp`, VS `libomp.lib`, `202011`) と Qt の `HAVE_STDATOMIC=ON` を build-dir cache だけに指定すると依存 configure は通過したが、generate 時に CMake 4.2.2 が module target の clang-cl scan rule を提供せず停止した。
- CMake 4.2.2 の `Compiler/Clang-CXX.cmake` は `CMAKE_CXX_SCANDEP_SOURCE` と Clang module map / BMI flags を GNU frontend の場合だけ設定する。MSVC frontend へ同じ GNU-style `-x c++`, `-MD`, `-MF` を流用すると clang-cl が未対応引数として警告する。MSVC frontend 用の P1689 command translation / module map flags、または CMake の対応版更新が必要。
- これは pilot の `.cpp` compile エラーではない。clang-cl lane は未成立。
### Phase 2: CMake の拡張子依存を縮小

- `.cppm` 固定の implementation list、`CompileAsCpp` 指定、force list の根拠を整理する。
- public interface の登録は `FILE_SET CXX_MODULES` に保ち、実装単位は通常 source として登録する。
- interface と implementation の判定を、可能な範囲で明示 manifest 化する。内容スキャンの重複・曖昧判定を増やさない。
- **合格:** 移行対象の拡張子を戻さなくても MSVC と Clang の依存グラフが一致する。

### Phase 3: 移行バッチを広げる

- 末端・低 fan-in の implementation unit から開始し、service、layer、composition の順に進める。
- 1バッチごとに renamed source、manifest、module hygiene、link/runtime の確認をまとめる。
- Artifact / ArtifactCore / ArtifactWidgets は各 child repository で順に更新し、親の gitlink は child の整合後に反映する。
- **合格:** `.cppm` は真の module interface / partition に限定され、残存 implementation の例外と理由が記録されている。

### Phase 4: sccache を実測で有効化

- まず本家 clang の通常 `.cpp`、次に Clang module interface / importer、最後に clang-cl の対応 flag 構成を別々に試す。
- 既定無効・明示 opt-in から始め、sccache statistics の compile / hit / miss / unsupported を記録する。
- MSVC IFC は非対応のまま cache bypass を許容する。clang-cl で launcher を使う場合も module scanner と compiler invocation の双方を測る。
- **合格:** cache hit が再現し、clean / incremental build の出力が uncached build と一致し、非対応 compiler では通常起動へ戻る。

#### 初期 opt-in

ルート `CMakeLists.txt` に `ARTIFACT_USE_SCCACHE_WITH_CLANG_CL`（既定 OFF）を追加した。MSVC-style frontend は module/PDB の並列 compile が未解決のため launcher を無効に保つ。sccache v0.18.0 の隔離 smoke では clang-cl の通常 `.cpp` module units が cache hit となり、module interface command は bypass した。

## 実施制約と未確認事項

- CMake 3.30 の clang-cl module support、root full build の clang-cl/OpenMP の自動検出、GUI 全体の upstream clang++ 対応は未検証。Upstream Clang root target build は OpenUSD `arch` で停止する。Windows MSVC ABI 向けの isolated Qt / module pilots は成功しているが、OpenUSD は `__clang__` を優先して Itanium/GNU ABI 分岐を選び、Windows SDK 上で `cxxabi.h` / `strerror_r` / constructor-entry declarations に失敗する。コンパイラ識別マクロの上書きでは `defines.h` が再定義するため解消できず、portable な親側 workaround は確認できていない。OpenUSD submodule patch はユーザー指示により適用しない。
- sccache v0.18.0 で `ArtifactCoreEnvironment` の実 target hit と隔離 smoke の hit/bypass を確認した。ArtifactStudio full build の cache hit rate と uncached build との成果物比較は未検証。
- 本家 clang を Windows GUI / Diligent の正式対応に含めるか、まず portable subset とするかは feasibility 結果で判断する。

## 参考資料

- [CMake 3.30 C++ modules manual](https://cmake.org/cmake/help/v3.30/manual/cmake-cxxmodules.7.html)
- [Latest CMake C++ modules manual](https://cmake.org/cmake/help/latest/manual/cmake-cxxmodules.7.html)
- [sccache README: C++20 Modules](https://github.com/mozilla/sccache/blob/main/README.md#c20-modules)
- [Clang Standard C++ Modules](https://clang.llvm.org/docs/StandardCPlusPlusModules.html)
- Existing migration milestone: `MILESTONE_CPP_HPP_ELIMINATION_2026-06-12.md`

### sccache を使った Clang module 実測（2026-10-21）

- root configure で `ARTIFACT_USE_SCCACHE=ON` を指定すると upstream clang++ 用 launcher として認識された。ただし再 configure は `ArtifactCore` の `find_package(Stb REQUIRED)` で停止し、新 build graph は生成されなかった。
- 既存 upstream clang++ / MSVC ABI module smoke target を sccache launcher で2回 clean/build した。両回とも interface `.ixx`、importer `.cpp`、implementation `.cpp` の3 compile request がすべて cache hit。累計 6 requests / 6 hits / 0 misses / 0 non-cacheable calls。
- この測定は isolated module smoke に限る。root `ArtifactCoreEnvironment` の sccache compile hit は未確認。

### 次の migration pilot（2026-10-22）

- MSVC 19.51 / CMake 4.4.4 / Ninja の dedicated `x64-Debug-MSVC-SccachePilot` build tree で `ArtifactWidgets` target を launcher 無効構成で再実行し、Ninja 576 action 完了、`ArtifactWidgets.dll` link 成功を確認した。sccache opt-in で起きた OpenUSD PDB エラーは再現しなかった。
- 次の小規模 pilot として `ArtifactWidgets/src/Time/TimeCodeLabel.cppm` を `TimeCodeLabel.cpp` に移し、CMake の明示 implementation list に加えた。公開 interface とソース本文を保持し、改行だけ LF に正規化した。MSVC 19.51 の本体 `ArtifactWidgets` target は依存 CMake 再生成後に576 actionを完了し、`ArtifactWidgets.dll` のリンクに成功。upstream clang++ MSVC ABI と clang-cl の独立 pilot でも P1689 scan / compile / Qt DLL link に成功した。独立 pilot は `TimeCodeEditor.ixx` が import するが宣言に使わない `Frame.Position` を最小スタブで補っているため、実 Core module との統合確認は MSVC 本体 target の証拠を使う。両 Clang lane の sccache compile request ごとの hit/miss 数はこの pilot では採取していない。



### Upstream clang++ Artifact target 再開（2026-10-22）

- 先行対象の Windows upstream clang++ / MSVC ABI build tree で `Artifact` target を再実行した。ArtifactCore と ArtifactWidgets の関連 target は再構築後に成功しているが、Artifact 全体は未完了。
- `ArtifactPreviewCompositionPipeline.cppm` が Clang 22.1.3 の `malformed or corrupted precompiled file: ran out of source locations` で停止。最新再実行の診断は約2.135 GB（99%）で、`Artifact.Layer.Text` import の位置で失敗した。別試行でも約2.14–2.15 GBに達し、隣接する `ArtifactProject.cppm` と `ArtifactLayerFactory.cppm` にも同種の上限超過が出た。
- 失敗時 modmap は154個の module BMI を列挙し、主要 BMI は約68–85 MiB。直接 import を確認すると、`Artifact.Layer.Abstract` は約30 module、`Artifact.Composition.Abstract` は約24 moduleを実装ファイル全体の推移依存として取り込んでいる。Qt `qcompilerdetection.h` が同 translation unit に約1,451回、約80 MiBのsource location entryとして現れる診断も得た。
- Preview 実装の未使用標準／Qt／OpenCV header を整理し、実使用する型の直接 import は維持した。再ビルドでも2.13 GB・99%で失敗し、実装ファイルの include 整理だけでは解消しないと確認した。この整理は clang++ full target を通す修正としては未成立。`Artifact.Layer.Abstract` / `Artifact.Composition.Abstract` の公開依存を安全に分割するか、Clang の module AST source-location limit を回避できるかが次の調査対象。clang++ full target、clang-cl full target、Artifact 全体での sccache 統計は引き続き未確認。
### ColorViewLabel migration / compiler cache pilot（2026-10-22）

- MSVC 19.51 / CMake 4.4.4 / Ninja の dedicated `x64-Debug-MSVC-SccachePilot` で `ArtifactWidgets` target を再生成後に build。436 action を実行し、`ColorViewLabel.cpp` の module scan、object compile、`ArtifactWidgets.dll` link が成功した（sccache launcher 無効）。
- 独立 Qt6 pilot は実 `ColorViewLabel.ixx` / `.cpp` を使い、`Color.Float` のみテスト用BMI stubで補完。CMake 4.4.4 / Ninja の upstream clang++ + Windows MSVC ABI と clang-cl で P1689 scan、interface/implementation compile、DLL link が成功した。Clang pilot は実 ArtifactCore dependency graph の代替証拠ではない。
- sccache 0.18.0 + upstream clang++ で cache buster を分離し、clean/build を2回実施。compile requests 6、初回3 misses、2回目3 hits、non-cacheable calls 0、失敗0。
- 同じ測定を clang-cl で実施。compile requests 6、cacheable compile 2件は1 miss→1 hit、`multiple input files` reason の bypass 4件、失敗0。いずれの2回目も DLL link 成功。


### CollapsibleSection migration / compiler cache pilot（2026-10-22）

- MSVC 19.51 / CMake 4.4.4 / Ninja の dedicated `x64-Debug-MSVC-SccachePilot` で `ArtifactWidgets` target を再生成後に build。436 action を実行し、`CollapsibleSection.cpp` の module scan、object compile、`ArtifactWidgets.dll` link が成功した（sccache launcher 無効）。
- Qt6 pilot は実 `CollapsibleSection.ixx` / `.cpp` と Verdigris include を使用し、CMake 4.4.4 / Ninja の upstream clang++ + Windows MSVC ABI と clang-cl で P1689 scan、interface / implementation compile、DLL link が成功した。独自 module import のstubは使っていない。
- sccache 0.18.0 + upstream clang++ で cache buster を分離し、clean/buildを2回実施。compile requests 4、初回2 misses、2回目2 hits、non-cacheable calls 0、失敗0。
- 同じ測定を clang-cl で実施。compile requests 4、cacheable implementation compile は1 miss→1 hit、module interface command は `multiple input files` により2回 bypass、失敗0。両回 DLL link 成功。

### Upstream Clang root feasibility check（2026-10-22）

- root `arch` target を実行し、Windows clang++ MSVC ABI の OpenUSD 依存 blocker を再現した。`demangle.cpp` は `cxxabi.h` 不在、`errno.cpp` は `strerror_r` 不在、`attributes.cpp` は Windows constructor-entry types 不在で失敗する。
- 親 CMake から Clang builtin macro を先読み後に解除する preinclude shim、OpenUSD compiler macros の configure-scope / target-scope 上書きを一時検証した。いずれも source/directory scope と OpenUSD `defines.h` の再定義により root-level の安全な workaround にはならず、試験変更は残していない。OpenUSD submodule は変更していない。
- isolated Qt pilots での upstream `clang++ --target=x86_64-pc-windows-msvc` scan / compile / link 成功は維持される。これを OpenUSD を含む root build の対応証拠と混同しない。
- Pixar の OpenUSD `dev` branch の現行 `defines.h` / `attributes.h` / `demangle.cpp` / `errno.cpp` も確認した。`__clang__` 優先の compiler 判定と、Windows MSVC ABI では使えない `cxxabi.h` / `strerror_r` 分岐は残っている。参照: [defines.h](https://raw.githubusercontent.com/PixarAnimationStudios/OpenUSD/dev/pxr/base/arch/defines.h)、[attributes.h](https://raw.githubusercontent.com/PixarAnimationStudios/OpenUSD/dev/pxr/base/arch/attributes.h)、[demangle.cpp](https://raw.githubusercontent.com/PixarAnimationStudios/OpenUSD/dev/pxr/base/arch/demangle.cpp)、[errno.cpp](https://raw.githubusercontent.com/PixarAnimationStudios/OpenUSD/dev/pxr/base/arch/errno.cpp)。
- 現行 upstream にも修正がないため、root full Clang build は blocker 付きで記録する。OpenUSD submodule の修正・gitlink 更新は行わず、親側の安全な workaround またはユーザーの明示依頼を待つ。

### OpticalFlowResult migration / three-compiler pilot（2026-10-22）

- `ArtifactCore/src/Analyze/OpenCV/OptifalFlowResult.cppm` は `module Analyze.OpticalFlow;` と空の実装コメントだけを持ち、型と関数は public interface `.ixx` 内で inline 定義されている。source body は変更せず `.cpp` へ移し、`ArtifactCoreSources.cmake`、`ArtifactCoreModuleReferences.cmake`、`ArtifactCore/CMakeLists.txt` の専用 Analyze target source registration を更新した。
- MSVC 19.51 の既存 `ArtifactCoreAnalyze` compile command を `.cpp` source で再現し、OpenCV headers / BMI reference を含む implementation compile に成功した。続いて再生成後の `ArtifactCoreAnalyze` target が完了し、新しい `.cpp.obj` が生成された。
- OpenCV 4.12.0 を使う isolated CMake 4.4.4 / Ninja pilot では、upstream clang++ (`--target=x86_64-pc-windows-msvc`) と clang-cl の両方が `.cpp` implementation / `.ixx` interface の P1689 scan、compile、`OpticalFlowPilot.dll` link に成功した。clang-cl は CMake の `/TP` に未使用警告を出したが完了した。
- OpenUSD を含む full Clang root target は別記 blocker のまま。isolated pilot は root dependency graph の証拠ではなく、この migration の sccache hit/miss は未計測。

### Utils.Tag migration / three-compiler pilot（2026-10-22）

- `ArtifactCore/src/Utils/Tag.cppm` を本文保持で `.cpp` へ移し、`ArtifactCoreSources.cmake`、`ArtifactCoreModuleReferences.cmake`、`ArtifactCore/CMakeLists.txt` の MSVC reference-map 分岐を更新した。implementation が直接 import する module は `Utils.String.UniString` 1件。public `Tag.ixx` は `UniString` と `StringLike` を import する。
- MSVC 19.51 の再生成後 `ArtifactCore` target は完了し、`ArtifactCore.dir/src/Utils/Tag.cpp.obj` と `ArtifactCore.lib` が生成された。
- CMake 4.4.4 / Ninja の isolated upstream clang++ MSVC ABI と clang-cl pilots で、実 `Tag.cpp` / `Tag.ixx` の P1689 scan・compile・`TagPilot.dll` link が成功した。`UniString` と `StringLike` は stub BMI / 最小 implementation で補った。clang-cl は既知の `/TP` unused warning を表示したが build は成功。
- この pilot の sccache hit/miss は未計測。OpenUSD root blocker は独立して継続する。

### Utils.NameGenerator migration / compiler and cache pilot（2026-10-22）

- `ArtifactCore/src/Utils/NameGenerator.cppm` を本文保持で `.cpp` へ移し、`ArtifactCoreSources.cmake`、`ArtifactCoreModuleReferences.cmake`、`ArtifactCore/CMakeLists.txt` の MSVC reference-map 分岐を更新した。implementation は `Core.ArtifactString` と `Container.NameMap` を import し、MSVC reference map は `Container.Debug` / `Container.NamedVector` も含む。`Container.NameMap.ifc` の既存 `OBJECT_DEPENDS` を維持した。
- MSVC 19.51 / CMake 4.4.4 / Ninja で CMake を再生成した。`ArtifactCore` の広域 target build は依存再構築 5,054 action に拡大したため中断。対象 object の直接指定でも CMake の order-only target dependency により外部依存 build が必要になったが、Windows SDK `fxc.exe` を一時 PATH に加えた実行で `NameGenerator.ixx`、`Container.NameMap`、`Core.ArtifactString` と `NameGenerator.cpp.obj` の compile を確認した。`ArtifactCore.lib` / 全 target link は今回未確認。
- CMake 4.4.4 / Ninja の isolated pilot では実 `NameGenerator.cpp` / `NameGenerator.ixx` を使用し、依存 `Core.ArtifactString` と `Container.NameMap` のみ pilot stub で補った。upstream clang++ (`--target=x86_64-pc-windows-msvc`) と clang-cl native scan rule の両方で P1689 scan、interface / implementation compile、`NameGeneratorPilot.dll` link が成功した。clang-cl は既知の `/TP` unused warning を出したが build は成功。
- sccache 0.18.0 の clean build 2回では、upstream clang++ が累計 8 requests / 4 misses / 4 hits / 0 bypass / 0 failure。clang-cl は累計 8 requests のうち `.cpp` compile が 1 miss → 1 hit、module compile 6件は `multiple input files` で non-cacheable、0 failure。全 build で DLL link 成功。
- ソース bytes は改行を LF に正規化した比較で旧 `.cppm` と一致し、実 `.cpp` は LF-only。OpenUSD root blocker は別途継続する。

### Utils.Path migration / real Qt module dependency and cache pilot（2026-10-22）

- `ArtifactCore/src/Utils/Path.cppm` を本文保持で `.cpp` へ移し、`ArtifactCoreSources.cmake`、`ArtifactCoreModuleReferences.cmake`、`ArtifactCore/CMakeLists.txt` の MSVC reference-map 分岐を更新した。実装の PImpl constructor / destructor を維持し、public `Path.ixx` とその `Utils.String.Like` import は変更していない。
- MSVC 19.51 / CMake 4.4.4 / Ninja で root build tree を再生成し、実 `Path.ixx` / `StringLike.ixx` interface object と `Path.cpp.obj` の scan / compile を確認した。全 `ArtifactCore.lib` link は未確認。
- CMake 4.4.4 / Ninja の upstream clang++ + Windows MSVC ABI と clang-cl では、実 `Path.cpp`、実 `Path.ixx`、実 `StringLike.ixx`、Qt6 Core を組み合わせた P1689 scan・compile・`PathPilot.dll` link が成功した。両 lane で Qt の `QStringLiteral` が `qMakeStringPrivate` の TU-local entity exposure warning を出したが、build は成功。
- sccache 0.18.0 の2 clean build は upstream clang++ が6 requests / 3 misses / 3 hits / 0 bypass / 0 failure。clang-cl は6 requests中 `.cpp` が1 miss→1 hit、interface compile 4件が `multiple input files` で non-cacheable、0 failure。全 build で DLL link 成功。
- 実 `.cpp` は LF-only。ソース module declaration と公開 interface は維持し、OpenUSD full-root blocker は独立して継続する。

### AnimationDynamics migration / real Core dependency and cache pilot（2026-10-22）

- `ArtifactCore/src/Animation/AnimationDynamics.cppm` を本文保持で `.cpp` に移し、source manifest、module reference map、MSVC 用 source reference-map 分岐を更新した。実装の直接 import は `Core.ArtifactString` 1件。pilot は実 `Core.ArtifactString` → `Core.ArtifactArray` → `Core.ArtifactOptional` module graph を含め、依存 stub を使っていない。
- Clang pilot が `AnimationDynamics.ixx` 内の `export namespace ArtifactCore { ... }` にある内側の `export` 2箇所を拒否したため、namespace export block の内側にある関数宣言から冗長な `export` を削除した。関数は引き続き export namespace 内にある。修正後、upstream clang++ + MSVC ABI と clang-cl native scan rule の双方で実 module dependency の P1689 scan、compile、`AnimationDynamicsPilot.dll` link が成功した。
- MSVC 19.51 / CMake 4.4.4 / Ninja で root CMake を再生成し、`AnimationDynamics.ixx` scan / compile と `AnimationDynamics.cpp.obj` compile が実 Core graph で成功した。`ArtifactCore.lib` 全体の再リンクは未確認。
- sccache 0.18.0 の2 clean build は upstream clang++ が10 requests / 5 misses / 5 hits / 0 bypass / 0 failure。clang-cl は10 requests中 implementation `.cpp` が1 miss→1 hit、8 module compile calls は `multiple input files` で non-cacheable、0 failure。すべての Clang build で DLL link 成功。

### Clang migration exception: FileInfo module partition

- `src/File/FileInfo.cpp` は `module File:InfoImpl; import :Info;` で implementation partition を宣言し、`include/File/FileInfo.ixx` は `export module File:Info;` を宣言する。root CMake はこのソースを Clang scan から除外し、MSVC 用の専用 `ArtifactCoreFile` reference と object ordering を持つ。
- CMake 4.4.4 / upstream clang++ isolated pilot で `.cpp` partition implementation を P1689 scan に含めると、`cmake_ninja_dyndep` は implementation object が `File:Info` を提供するが `CXX_MODULES` file set に見つからないと停止した。scan を無効にし、interface BMI を明示参照しても `FileInfo` 宣言が implementation unit から見えず compile できなかった。
- **扱い:** 既存移行でソースは `.cpp` になっている。通常のP1689対象にはせず、両Clang frontendで明示partition PCM参照を維持する。`Image/OpenEXR.cpp` は同じ初期 `CXX_SCAN_FOR_MODULES OFF` groupに含まれるが、primary implementationなので後段のClang分類でscan対象へ戻す。両者は異なる例外である。

### Platform.Power migration / Windows API and sccache pilot（2026-10-22）

- `ArtifactCore/src/Platform/Power.cppm` を本文保持で `.cpp` に移し、`ArtifactCoreSources.cmake`、`ArtifactCoreModuleReferences.cmake`、`ArtifactCore/CMakeLists.txt` の専用 Core source registration を更新した。module declaration `Platform.Power`、`Power.ixx`、Windows API calls、`winmm` link requirement は維持した。
- MSVC 19.51 / Visual Studio 18 generator で実 interface と `.cpp` を module scan・compileし、`PlatformPowerPilot.dll` を link した。
- CMake 4.4.4 / Ninja の upstream clang++ 22.1.3 + Windows MSVC ABI と clang-cl 22.1.3 でも、実 interface / implementation の P1689 scan、compile、`winmm` を含む DLL link が成功した。clang-cl は CMake の `/TP` unused warning を出したが完了した。
- sccache 0.18.0 の clean build 2回では、upstream clang++ は4 requests / 2 misses / 2 hits / 0 bypass / 0 failure。clang-cl は4 requests中 `.cpp` が1 miss→1 hit、module interface compile 2件は `multiple input files` で non-cacheable、0 failure。すべてのbuildで DLL link 成功。
- この移行では `.cpp` に `-x c++` を付けずとも CMake / clang++ が implementation として扱うことを確認した。ArtifactCore 全体の archive link と runtime 呼び出しは未確認。

### Platform.ShellUtils migration / Qt Core+Gui dependency pilot（2026-10-22）

- `ArtifactCore/src/Platform/ShellUtils.cppm` を本文保持で `.cpp` に移し、`ArtifactCoreSources.cmake`、`ArtifactCoreModuleReferences.cmake`、`ArtifactCore/CMakeLists.txt` の Platform source registration を更新した。public `ShellUtils.ixx` と `Platform.ShellUtils` module declaration は維持した。
- MSVC 19.51 / Visual Studio 18 generator で `Platform.Power` と `Platform.ShellUtils` の実 interfaces / implementations をscan・compileし、Qt6 Core/Gui、`winmm`、`shell32` を含む `PlatformUtilitiesPilot.dll` を link した。
- CMake 4.4.4 / Ninja の upstream clang++ 22.1.3 + Windows MSVC ABI と clang-cl 22.1.3 でも、同じ実 source pair の P1689 scan、compile、DLL link が成功した。clang-cl は既知の `/TP` unused warning を表示したが完了した。
- sccache 0.18.0 の2 clean buildでは upstream clang++ が8 requests / 4 misses / 4 hits / 0 bypass / 0 failure。clang-cl は8 requests中、2 implementation `.cpp` が各1 miss→1 hit、4 module interface calls は `multiple input files` によりnon-cacheable、0 failure。両方とも2回目にDLL link成功。
- 既存関数をそのまま保持した拡張子移行であり、ArtifactCore全体のPlatform archive linkと実行時のshell actionは未確認。

### FloatRGBA conversion implementation migration / real Color module graph（2026-10-22）

- `ArtifactCore/src/Color/FloatRGBA.cppm` の `operator FloatColor()` 実装を `.cpp` に移し、source manifest、module reference map（`FloatRGBA` provider と `Color.Float` dependency）、MSVC source reference branch を更新した。`FloatRGBA.ixx` とその `Color.Float` import は維持した。
- 実 dependency graphを保つ Clang pilot は、実 `FloatRGBA.cpp` / `FloatRGBA.ixx` / `FloatColor.ixx` / `ColorTransferFunction.ixx` と、`FloatColor.cppm` の本文をbyte-equivalentで `.cpp` に置いた検証用 implementation copy を使った。CMake 4.4.4 が pilot 内 `.cppm` をmodule interfaceとしてコンパイルし、`module Color.Float;` を拒否したためであり、repository側の `FloatColor.cppm` は変更していない。3 module interface、2 implementationsのP1689 scan / compile、DLL linkが両Clang frontendで成功した。
- `ColorTransferFunction.ixx` の `export namespace ArtifactCore` 内にあった冗長な `export` を1箇所削除した。namespace内宣言のexport状態は維持した。修正後は実interfacesを含む Clang scan / compile / link が成功した。
- MSVC 19.51 / Visual Studio 18 generatorでは、CMake 4.4.4 pilot source 内に実 `FloatColor.cppm` の byte-equivalent `.cpp` copy を置き、実 FloatRGBA implementationと Color module graphのscan / compile / `FloatRGBAPilot.dll` linkが成功した。
- sccache 0.18.0 clean build 2回で upstream clang++ は10 requests / 5 misses / 5 hits / 0 bypass / 0 failure。clang-cl は10 requests中、`.cpp` 2件が各1 miss→1 hit、interface compile 6件は `multiple input files` でnon-cacheable、0 failure。各2回目もDLL link成功。
- 2026-10-22 に root Clang側の実 `.cpp` を使う object-only sccache pilotを再実行。4 cacheable compile requestsが初回4 misses、clean後4 hitsとなり、追加miss 0・cache error 0。両build成功。
- ArtifactCore全体のColor target archive linkとruntime conversion呼び出しは未確認。

### ColorLuminance implementation migration / three-compiler cache pilot（2026-10-22）

- `ArtifactCore/src/Color/ColorLuminance.cppm` を本文保持で `.cpp` に移し、`ArtifactCoreSources.cmake`、`ArtifactCoreModuleReferences.cmake`、`ArtifactCore/CMakeLists.txt` のsource / MSVC reference registrationを更新した。`Color.Luminance` interfaceと関数本体は変更していない。
- 実 `ColorLuminance.ixx` / `.cpp` の依存scan、compile、DLL linkがMSVC 19.51、upstream clang++ 22.1.3 + Windows MSVC ABI、clang-cl 22.1.3で成功した。
- sccache 0.18.0の2 clean buildでは、upstream clang++が4 requests / 2 misses / 2 hits / 0 bypass / 0 failure。clang-clは4 requests中 `.cpp` が1 miss→1 hit、module interface compile 2件が `multiple input files` でnon-cacheable、0 failure。両laneとも各2回目のDLL linkが成功した。
- ArtifactCore全体のarchive linkと実consumer runtime呼び出しは未確認。

### Graphics.GPUCapabilities migration / implementation-only API（2026-10-22）

- `ArtifactCore/src/Graphics/GPUCapabilities.cppm` を本文保持で `.cpp` へ移行した。`normalize()`、`nextTimelineToken()`、`indirectCount()`、`hasUsableMemoryBudget()` は計算処理のみで、GPU APIやQtへの直接依存を持たない。公開 `GPUCapabilities.ixx` と `Graphics.GPUCapabilities` module name は変更していない。
- `ArtifactCoreSources.cmake` と `ArtifactCoreModuleReferences.cmake` の登録、および `ArtifactCore/CMakeLists.txt` のMSVC GPU implementation IFC指定2か所を `.cppm` / `.cpp` 両対応にした。ArtifactCore の `.gitattributes` で `CMakeLists.txt` / `*.cmake` のLFを固定し、移行元CRLFを新 `.cpp` とともにLFへ揃えた。正規化後の実装内容は旧 `.cppm` とbyte一致した。
- CMake 4.3.1 / Ninja / upstream clang++ 22.1.3 + `x86_64-pc-windows-msvc` で実ファイルのP1689 scan、interface / implementation compile、DLL linkに成功。さらにroot ArtifactCore graphでも844件のmodule scan完了後、`GPUCapabilities.ixx` と `GPUCapabilities.cpp` の両objectが生成され、再実行は `no work to do` となった。
- sccache 0.18.0 を使った独立pilotのclean build 2回で、対象compileは2 missesの後に2 hitsとなり、両回DLL link成功。統計差分で計測し、既存cache全体の統計はリセットしていない。
- MSVC 19.51 / Visual Studio 18 generatorでも実ファイルのinterface / implementation compile、IFC参照、DLL linkが成功した。clang-cl はこの環境のCMake 4.3.1 scanner ruleが `-MF` をclang-clへ誤渡しするため、この移行でのscanner / compile / linkは未確認。既存のCMake 4.4.x clang-cl pilot結果と混同しない。
- root Clang scan前に、vcpkg `Taskflow::Taskflow` が `/wd4324` を無条件伝播していたことを確認した。`ArtifactCore/CMakeLists.txt` でupstream Clang GNU frontendに限ってこのMSVC専用optionを除外し、root scanを通した。ArtifactCore全体のarchive linkとruntime呼び出しは未確認。
### Frame.SkipTracker migration / Qt Core implementation（2026-10-22）

- `ArtifactCore/src/Frame/FrameSkipTracker.cppm` を本文保持で `.cpp` へ移行し、source manifest、module reference map、MSVC primary IFC reference 分岐を更新した。旧実装と新 `.cpp` はLF正規化後にbyte一致した。
- clang++ が `FrameSkipTracker.ixx` の `export namespace` 内にある3つの冗長な `export` を拒否したため、型・enumの公開範囲を保って内側の指定だけを削除した。root CMake 4.3.1 / Ninja / clang++ 22.1.3 + MSVC ABIで、interfaceと新implementationのP1689 scan・compileが成功した。
- Qt6 Coreを含む隔離DLLがupstream clang++とMSVC 19.51の双方でリンク成功。sccache 0.18.0はunique probe keyで2 clean buildを実施し、初回2 misses、2回目2 hits、両方link成功。失敗カウンタはこの2回で増えていない。
- clang-clのこのsource pairは未検証。現行CMake 4.3.1ではclang-cl P1689 scanner ruleが `/MF` を正しく扱わず、CMake compiler sanity testで停止する。ArtifactCore全体のarchive link/runtime呼び出しは未確認。
### Frame.Time migration / RationalTime module dependency（2026-10-22）

- `ArtifactCore/src/Frame/FrameTime.cppm` を本文保持で `.cpp` に移し、source manifest、module reference map（`Frame.Time` provider と `Time.Rational` dependency）、MSVC source reference-map 分岐を更新した。LF 正規化後の本文は旧 `.cppm` と一致し、`FrameTime.ixx` と module declaration は維持した。
- root CMake 4.3.1 / Ninja / upstream clang++ 22.1.3 + Windows MSVC ABI で実際の `FrameTime.cpp` P1689 scan と compile が成功し、依存 `RationalTime.ixx` と `FrameTime.ixx` も生成・compileされた。再度の対象 build は no-op。
- Qt6 Core と実 `Frame.Time` / `Time.Rational` interfaces を使う object-only isolated pilot は成功。sccache 0.18.0 clean build 2回で対象3 compile unitsは初回3 misses、2回目3 hits、追加miss 0、各build成功。
- MSVC 19.51 の isolated DLL pilot では `FrameTime.cpp` 自体と module interface compile は成功したが、既存 `RationalTime.cppm` を standalone target へ追加しても interface attachment を再現できず依存実装 compile が失敗した。したがって、このpilotでのDLL linkは未確認であり、MSVC root target の完了証拠とは扱わない。clang-cl もこのファイルでは未確認（ローカルCMake 4.3.1のscanner引数制約）。ArtifactCore全体のarchive link/runtime呼び出しは未確認。
### Utils.HashValue migration / Qt Core + MSVC ABI compile and cache pilot（2026-10-22）

- `ArtifactCore/src/Utils/HashValue.cppm` を本文保持で `.cpp` に移し、`ArtifactCoreSources.cmake` と `ArtifactCoreModuleReferences.cmake` の実装登録を更新した。実装は module import を持たず、Qt Core APIを利用する。public `HashValue.ixx` と `Utils.HashValue` 名は変更していない。
- CMake 4.4.4 / Ninja の upstream clang++ 22.1.3 (`x86_64-pc-windows-msvc`) pilotで、実 interface と `.cpp` の P1689 scan、compile、Qt6 Coreを含む DLL link が成功した。sccache 0.18.0 clean build 2回では2 compile requestsが初回2 misses、2回目2 hits、cache error 0。
- MSVC 19.51 / Visual Studio 18 / CMake 4.4.4 でも、同じ実 interface / `.cpp` / Qt6 Core構成の scan、compile、DLL link が成功した。
- 旧 `.cppm` と新 `.cpp` は改行をLFに正規化した比較で本文一致し、新 `.cpp` はLF-only。root ArtifactCore全体のarchive link/runtime呼び出し、clang-cl laneは未確認。

### Utils.Id migration / Qt + Boost UUID compiler pilot（2026-10-22）

- `ArtifactCore/src/Utils/Id.cppm` を本文保持で `.cpp` に移し、`ArtifactCoreSources.cmake` と `ArtifactCoreModuleReferences.cmake` のprovider登録を更新した。`Utils.Id` は interface-only import を持ち、public `Id.ixx` / module名は維持。MSVC per-source IFC overrideは既存CMakeに存在しないことを確認した。
- CMake 4.4.4 / Ninja の upstream clang++ 22.1.3 + Windows MSVC ABI pilotで、実 `Id.ixx` / `Id.cpp` のP1689 scan、compile、Qt6 Core + Boost.UUIDを含むDLL linkが成功した。wobjectdefs.h include pathはArtifactWidgets既存配置を参照した。sccache 0.18.0 clean build 2回でimplementation compileは1 miss→1 hit、cache errors 0。
- MSVC 19.51 / Visual Studio 18 / CMake 4.4.4 でも、同じ実 source pairとQt/Boost headersでDLL link成功。
- 旧 `.cppm` と `.cpp` はLF正規化後に本文一致し、新 `.cpp` をLF-onlyへ統一した。root ArtifactCore全体のarchive link/runtime UUID操作、clang-cl laneは未確認。


### Utils.MultipleTag migration / root Clang + dual compiler pilot（2026-10-22）

- ArtifactCore/src/Utils/MulitpleTag.cppm を本文保持で .cpp に移し、ArtifactCoreSources.cmake と ArtifactCoreModuleReferences.cmake のprovider登録を更新した。直接のmodule importはなく、Qt CoreをGMFで利用する。Utils.MultipleTag と MultipleTag.ixx は維持した。
- root CMake 4.3.1 / Ninja / upstream clang++ 22.1.3 + Windows MSVC ABI で、実 .cpp と MultipleTag.ixx のscan・compileが成功した。root dyndep generation時に同時更新済みの HashValue.cpp / Id.cpp もscanを通過した。
- CMake 4.4.4 isolated pilotsでは、実 source pairを使ったupstream clang++ + Qt6 Core と MSVC 19.51 のDLL linkが成功。sccache 0.18.0 clean build 2回で2 compile requestsが初回2 misses、2回目2 hits、cache errors 0。
- 旧 .cppm と新 .cpp はLF正規化後に本文一致し、.cpp はLF-only。ArtifactCore全体のarchive link/runtimeタグ操作、clang-cl laneは未確認。


### Utils.Fingerprint migration / Qt Core and root Clang validation（2026-10-22）

- ArtifactCore/src/Utils/AssetFingerprint.cppm を本文保持で .cpp に移し、ArtifactCoreSources.cmake と ArtifactCoreModuleReferences.cmake のprovider登録を更新した。直接のmodule importはなく、Qt Coreの QFile / QCryptographicHash APIを使う。公開 Utils.Fingerprint interfaceとmodule名は維持。
- root CMake 4.3.1 / Ninja / upstream clang++ 22.1.3 + Windows MSVC ABI で実 .cpp / interfaceのP1689 scan・compileが成功し、対象 implementation objectが生成された。
- CMake 4.4.4 isolated pilotではupstream clang++とMSVC 19.51の双方でQt6 Coreを含むDLL linkに成功。sccache 0.18.0 clean build 2回で2 compile requestsが初回2 misses、2回目2 hits、cache errors 0。
- 旧 .cppm と新 .cpp はLF正規化後に本文一致し、新 .cpp をLF-onlyに統一した。ArtifactCore全体archive link、実ファイルhash runtime、clang-cl laneは未確認。


### Utils.ExplorerUtils migration / imported module and three compiler lanes（2026-10-22）

- ArtifactCore/src/Utils/ExplorerUtils.cppm を本文保持で .cpp に移し、ArtifactCoreSources.cmake、ArtifactCoreModuleReferences.cmake のprovider/imported-reference、ArtifactCore/CMakeLists.txt のMSVC per-source reference mappingと imported-module reference map の Utils.String.UniString dependencyを更新した。依存 Utils.String.UniString と公開API/module名は維持した。ExplorerUtils.ixx の export namespace 内冗長な export 指定を削除し、clang++ が拒否する既存構文を公開性を変えず修正した。
- CMake 4.4.4 / Ninja の upstream clang++ + Windows MSVC ABI、clang-cl、MSVC 19.51 の各pilotで実 ExplorerUtils.cpp / public interfacesをscan・compileし、Qt Coreと UniString の実 interface dependencyを使ったDLL linkが成功した。UniString implementationはpilot内に実 cppm の本文コピーを .cpp として配置し、既存 repository sourceは変更していない。MSVCでは既存 StringConvertor.ixx 由来 warning C5202 が1件。
- root CMake 4.3.1 / Ninja / upstream clang++ では ExplorerUtils.cpp / .ixx scanとcompileが成功した。
- upstream clang++ / sccache clean build 5 compile unitsは5 miss→5 hit。clang-cl clean buildでは各回3 non-cacheable calls、計2 cache hits、cache errors 0。upstream clang++ clean buildは5 misses→5 hits、cache errors 0。両laneで2回DLL link成功。
- CMake 4.3.1のclang-cl scanner limitationは継続する一方、CMake 4.4.4 native clang-cl scan ruleでは本source pairのP1689 scan/compile/linkが通った。ArtifactCore全体のarchive link/runtime Explorer起動は未確認。


### Color.XYZ migration / two imported modules and three compiler lanes（2026-10-22）

- ArtifactCore/src/Color/XYZColor.cppm を本文保持で .cpp に移し、ArtifactCoreSources.cmake、ArtifactCoreModuleReferences.cmake のprovider / Color.Float / Utils.String.UniString imported registrations、および ArtifactCore/CMakeLists.txt のMSVC reference branchを更新した。public XYZColor.ixx と Color.XYZ APIは維持。
- root CMake 4.3.1 / Ninja / upstream clang++ 22.1.3 + Windows MSVC ABIで実 XYZColor.cpp / interfaceのP1689 scanとcompileが成功。
- CMake 4.4.4 isolated pilotは実 XYZColor implementationと Color.Float / UniString interfacesを用い、依存implementation本文のcopyを検証用 .cpp に配置。upstream clang++、clang-cl、MSVC 19.51の各laneでDLL link成功。MSVC側で既存 StringConvertor.ixx由来warningが1件、エラー0。clang-clは既知の /TP unused warningを表示。
- upstream clang++ sccache clean buildsでは8 compile unitsが8 misses→8 hits、cache error 0。旧 .cppm と新 .cpp はLF正規化後に本文一致し、.cppはLF-only。ArtifactCore全体archive link / runtime conversionは未確認。

### Math.Rotation implementation extension migration（2026-10-22）

- `ArtifactCore/src/Math/Rotation.cppm` は `module Math.Rotation;` で始まる実装単位で、公開宣言は `include/Math/Rotation.ixx` にある。`export module` を持たず、`.cpp` 移行の対象条件に合う。
- 実装内容・module declaration・公開 interface は変えず `Rotation.cpp` へ移し、`ArtifactCoreSources.cmake` と `ArtifactCoreModuleReferences.cmake` の source path を更新した。汎用 CMake source-property mapping が `CORE_IMPL` を元に適用されるため、source-specific な `CMakeLists.txt` 登録は追加していない。
- CMake 4.3.1 / Ninja / upstream clang++ 22.1.3 (`x86_64-pc-windows-msvc`) で再生成後、`Rotation.ixx` の interface object / `Math.Rotation.pcm` と `Rotation.cpp.obj` を生成した。dyndep に implementation object → `Math.Rotation.pcm` の依存が記録された。
- 対象を clean して2回ビルドし、sccache 統計の差分は4 compile requests / 4 hits / 0 misses。既存 cache error は 1 のまま増えず、non-cacheable calls も増えなかった。
- `ArtifactCore` 集約ターゲットは OpenUSD `arch` の再ビルド中に停止した。`demangle.cpp` で Windows環境に `cxxabi.h` がなく、`attributes.cpp` でも `Arch_ConstructorEntry` 等が未定義となった。変更した Rotation source の失敗ではない。ArtifactCore archive link は未確認で、OpenUSD submodule は変更していない。
- clang-cl 22.1.3 / CMake 4.4.4 の root configure は完了したが、当初は OpenUSD `arch` の PCH が `/wddeprecated` 等の clang-cl 非対応 warning option で失敗した。後述の root compatibility helper を追加し、Rotation の isolated pilot で clang-cl compile を確認した。OpenUSD submodule は変更していない。
- このファイルでの MSVC lane とリンクも未確認。旧 `.cppm` と新 `.cpp` はLF正規化後に内容一致し、新 `.cpp` はLF-only。

### OpenUSD clang-cl warning option compatibility（2026-10-22）

- OpenUSD の `CXXHelpers.cmake` は compiler ID が Clang でも `MSVC=TRUE` を見て `/wddeprecated` などを生成する。clang-cl は warning 番号形式の `/wdNNNN` と異なり、これらの名前指定を受理せず、`arch` の PCH compile 前に停止していた。OpenUSD source/submodule は変更していない。
- ルート CMake に OpenUSD の configure 中だけ使う `cmake/openusd-clangcl-compat/CXXHelpers.cmake` を追加し、clang-cl frontend に限って `_disable_warning()` が `-Wno-*` を生成するようにした。その他のコンパイラの warning flag 選択と OpenUSD の MSVC ABI / PCH / link 分岐は維持する。
- 最初の再検証時は build directory 名が `ClangCL-CMake444` でも、実際に使える PATH の CMake が 4.2.2、Visual Studio 同梱版が 4.3.1 で、4.4.4 executable がなかった。4.2.2 は compiler feature configure 時に clang-cl module dependency scanner 未対応で停止し、OpenUSD helper を読み込む段階へ到達しなかった。その後、公式 CMake 4.4.4 ZIP を検証して再試行した。
- CMake 4.4.4 公式 Windows ZIP を一時ディレクトリへ展開し、公式 SHA-256 と一致することを確認して再構成した。OpenUSD `arch` の生成 flags が3つの `/wd<名前>` から `-Wno-deprecated` 等へ変わり、`pch.cpp.obj` は clang-cl でコンパイル成功した。
- Clang frontend の OpenUSD defaults が Windows SDK の `min` / `max` macroを抑止しないため、root 側の `arch` target に `NOMINMAX` を設定した。これにより `systemInfo.cpp` / `timing.cpp` / `stackTrace.cpp` の min/max macro衝突は解消した。
- 実 `Rotation.ixx` を `CXX_MODULES` file set、実 `Rotation.cpp` を実装 source として参照する isolated Ninja project を `out/pilots/clangcl-rotation` に構成した。CMake 4.4.4 / clang-cl 22.1.3 で両ファイルの P1689 scan、interface object / module IFC 生成、実装 object compile、static archive 作成が成功した。`-TP` unused warning のみ。
- root `arch` target 全体は引き続き失敗する。残件は OpenUSD の `demangle.cpp` が MSVC ABI環境にない `cxxabi.h` を要求すること、`attributes.cpp` が Windows 向け型を Clang compiler macro分岐で定義しないこと、`errno.cpp` が ClangをMSVC ABIとして扱わず `strerror_r` を要求すること。これらは OpenUSD source 内の compiler/OS 判定であり、同 submodule を変更せず root 側で回避できる範囲を確認中。

### Math.Interpolate implementation migration / three compiler lanes（2026-10-22）

- `ArtifactCore/src/Math/Interpolate.cppm` は import のない primary-interface attachment (`module Math.Interpolate;`) で、本文は空 namespace のみ。`.cpp` へ移し、source manifest と module provider reference を更新した。LF 正規化後、旧実装 blob と新 `.cpp` は byte 一致し、末尾 newline を含む LF-only を確認した。
- 実 interface `Geometry/Interpolate.ixx` の `export namespace ArtifactCore` 内に、さらに9個の redundant `export` 指定があり Clang が拒否していた。内側の指定を除去した。外側の export namespace により公開範囲は維持される。
- root CMake 4.4.4 / Ninja / upstream clang++ 22.1.3 (`x86_64-pc-windows-msvc`) で root configure と実 `Interpolate.cpp` object target が成功した。1,722 actions の module graph を通過し、`CXX.dd` に implementation object → `Math.Interpolate.pcm` の依存が生成された。
- 同じ実 `.ixx` / `.cpp` の isolated CMake 4.4.4 Ninja static archive が、clang-cl 22.1.3、upstream clang++ 22.1.3 + MSVC ABI、MSVC 19.51 の全 lane で成功した。MSVC lane は SDK `INCLUDE` / `LIB` を明示した。
- LF 版の upstream clang++ pilot を clean 再ビルドした際、sccache 統計は2 compile requests / 2 cache hits、追加 miss 0、cache errors 0 だった。これは同一 pilot の反復ビルドで module interface と implementation の両方が cache hit した結果。

### Math.ArtifactMinMax implementation migration / three compiler lanes（2026-10-22）

- `ArtifactCore/src/Math/ArtifactMinMax.cppm` は標準ヘッダーのみを global module fragment で include し、primary-interface attachment `module Math.ArtifactMinMax;` を持つ実装単位だった。実装本文を `.cpp` に移し、`ArtifactCoreSources.cmake` と `ArtifactCoreModuleReferences.cmake` の source/provider 登録を更新した。公開 interface と module name は維持した。
- 実 `ArtifactMinMax.ixx` / `.cpp` を使う isolated CMake 4.4.4 + Ninja pilot が upstream clang++ 22.1.3 + `x86_64-pc-windows-msvc`、clang-cl 22.1.3、MSVC 19.51 の各 lane で P1689 scan、compile、static archive 作成まで成功した。clang-cl では `-TP` unused warning が出たが処理は成功した。
- root CMake 4.4.4 / Ninja / upstream clang++ 22.1.3 でも configure と `ArtifactMinMax.cpp` object target が成功し、`CXX.dd` は implementation object → `Math.ArtifactMinMax.pcm` の依存を記録した。
- root clang++ build の対象を clean して再ビルドした sccache 統計差分は2 requests / 2 hits / 0 misses / 0 errors。統計は共有 cache 全体の累積値から対象 clean build 前後を比較した。
- ArtifactCore 全体 archive link と runtime は未確認。clang-cl の OpenUSD `arch` は既知の upstream `cxxabi.h` / compiler-macro / MSVC ABI 障害で未解決のまま。

### Text.Style implementation migration / upstream clang++（2026-10-22）

- `ArtifactCore/src/Text/TextStyle.cppm` は `<utility>` のみを global module fragment で include し、`module Text.Style;` を宣言する実装単位だった。本文を `.cpp` に移し、`ArtifactCoreSources.cmake`、`ArtifactCoreModuleReferences.cmake`、`ArtifactCore/CMakeLists.txt` の明示登録を更新した。module name と public interface は維持した。
- upstream clang++ 22.1.3 / CMake 4.4.4 / Ninja の実 `ArtifactCoreText` split target で interface の P1689 scan・IFC/object compile と、実装 object compile が成功した。`CXX.dd` は TextStyle implementation object が `Text.Style.pcm` を使い、interface が `Utils.String.UniString`、`Utils.Optional`、`FloatRGBA` を使う依存を記録した。対象 pair の clean 後に root build で再ビルドし、sccache 統計差分は11 requests / 11 hits / 0 misses / 0 errors（module graph に伴う他の compile requests を含む）。
- 同じ interface を standalone project で試すと、その project に Qt include path と imported module がないため scan が `QString` 不在で停止した。これは独立プロジェクトの依存不足。root clang-cl target は指定 object に到達する前に OpenUSD `arch` の `errno.cpp` (`strerror_r` / MSVC ABI)、`demangle.cpp` (`cxxabi.h`)、`attributes.cpp` (Windows constructor attribute) で停止したため、この移行の clang-cl レーンは未検証。MSVC lane も未検証。
- Clang が拒否した既存 `TextStyle.ixx` の `export namespace ArtifactCore` 内にある7個の重複 `export` 指定を削除した。外側の export namespace は維持し、module の公開範囲を保った。`.cpp` はLFで末尾改行あり。

### Media.TimeStamp implementation migration / three compiler lanes（2026-10-22）

- `ArtifactCore/src/Video/MediaTimeStamp.cppm` は import のない `module Media.TimeStamp;` 実装単位だった。`.cpp` に移し、source manifest、provider mapping、`ArtifactCore/CMakeLists.txt` の Media split target 登録、および MSVC reference IFC 出力先を選ぶ拡張子条件を更新した。
- 実 `MediaTimeStamp.ixx` / `.cpp` の isolated CMake 4.4.4 + Ninja pilot は upstream clang++ 22.1.3 (`x86_64-pc-windows-msvc`)、clang-cl 22.1.3、MSVC 19.51 の全 lane で P1689 scan、compile、static archive 作成に成功した。
- root CMake 4.4.4 / Ninja / upstream clang++ 22.1.3 の実 `ArtifactCoreMedia` split target で `MediaTimeStamp.cpp` object が成功した。関連 CMake 条件も新 `.cpp` パスで評価されることを確認した。
- `.cpp` の既存本文を保持し、LF で末尾改行を付けた。ArtifactCore 全体 archive link と runtime は未確認。

### MonochromeCS implementation migration / three compiler lanes（2026-10-22）

- `ArtifactCore/src/ImageProcessing/DirectCompute/MonochromeCS.cppm` は標準ヘッダーのみで `module MonochromeCS;` を実装する単位だった。`.cpp` に移し、`ArtifactCoreSources.cmake` と `ArtifactCoreModuleReferences.cmake` の登録を更新した。追加の ArtifactCore/CMakeLists.txt 例外登録はなかった。
- 実 `MonochromeCS.ixx` / `.cpp` の isolated CMake 4.4.4 + Ninja project は upstream clang++ 22.1.3 + Windows MSVC ABI、clang-cl 22.1.3、MSVC 19.51 の各 lane で scan、compile、static archive 作成に成功した。
- root CMake 4.4.4 / Ninja / upstream clang++ 22.1.3 の ArtifactCore object target でも `MonochromeCS.cpp` を compile できた。
- 旧実装本文を保持し、新 `.cpp` は LF・末尾改行に統一した。ArtifactCore 全体 archive link と runtime は未確認。

### AddBlockNoise implementation migration / three compiler lanes（2026-10-22）

- `ArtifactCore/src/ImageProcessing/OpenCV/AddBlockNoise.cppm` は依存 import のない小さな `AddBlockNoise` 実装だった。`.cpp` に移し、source manifest と module provider mapping を更新した。
- 実 `AddBlockNoise.ixx` / `.cpp` の isolated CMake 4.4.4 + Ninja project は upstream clang++ 22.1.3 + Windows MSVC ABI、clang-cl 22.1.3、MSVC 19.51 の全 lane で scan、compile、static archive 作成に成功した。
- root CMake 4.4.4 / Ninja / upstream clang++ 22.1.3 でも実 ArtifactCore object target の `AddBlockNoise.cpp` を compile できた。
- 旧実装本文を保持し、新 `.cpp` は LF・末尾改行に統一した。ArtifactCore 全体 archive link と runtime は未確認。

### Platform.Windows.Process implementation migration / three compiler lanes（2026-10-22）

- `ArtifactCore/src/Platform/ProcessHelperWin.cppm` は import のない `module Platform.Windows.Process;` 実装単位だった。`.cpp` に移し、source manifest、module provider mapping、Platform split target source list を更新した。
- 実 `ProcessHelperWin.ixx` / `.cpp` の isolated CMake 4.4.4 + Ninja project は upstream clang++ 22.1.3 + Windows MSVC ABI、clang-cl 22.1.3、MSVC 19.51 の各 lane で scan、compile、static archive 作成に成功した。
- root CMake 4.4.4 / Ninja / upstream clang++ 22.1.3 の実 `ArtifactCorePlatform` object target で `.cpp` implementation object が成功した。
- module 名と公開宣言を保ち、新 `.cpp` は LF・末尾改行にした。ArtifactCore 全体 archive link と runtime は未確認。

### Codec.AbstractEncoder implementation migration / three compiler lanes（2026-10-22）

- `ArtifactCore/src/Video/AbstractEncoder.cppm` を `.cpp` に移し、source manifest、module provider mapping、`ArtifactCore/CMakeLists.txt` の Video split target source list と IFC `/reference` の source path を更新した。
- 実 interface / implementation の isolated CMake 4.4.4 + Ninja project は upstream clang++ 22.1.3 + Windows MSVC ABI、clang-cl 22.1.3、MSVC 19.51 の全 lane で compile / static archive 作成に成功した。MSVC pilot は Qt の要求に合わせ `/Zc:__cplusplus` と VS/Windows SDK の `INCLUDE` / `LIB` / `PATH` を設定した。
- root CMake 4.4.4 / Ninja / upstream clang++ 22.1.3 の `ArtifactCoreVideo` target で新 `AbstractEncoder.cpp` object が成功し、Video split IFC 参照が維持された。
- module 名と public API は変更せず、新 `.cpp` を LF・末尾改行にした。ArtifactCore 全体 archive link と runtime は未確認。

### ImageProcessing.AutoGamma.CV implementation migration / three compiler lanes（2026-10-22）

- `ArtifactCore/src/ImageProcessing/OpenCV/AutoGammaCV.cppm` は import なしで実装 module に attachment し、実装本体は空 namespace だった。`.cpp` に移し、source manifest と module provider mapping を更新した。public interface は OpenCV header のみを直接 include し、外部 module import はない。
- 実 `AutoGammaCV.ixx` / `.cpp` の isolated CMake 4.4.4 + Ninja project は upstream clang++ 22.1.3 + Windows MSVC ABI、clang-cl 22.1.3、MSVC 19.51 で scan・compile・static archive 作成に成功した。OpenCV include directory を明示した。
- root CMake 4.4.4 / Ninja / upstream clang++ 22.1.3 の実 ArtifactCore object target でも `AutoGammaCV.cpp` を compile できた。
- 同じ候補探索で `FileInfo` partition (`File:Info`) の移行を試したが、CMake dyndep が `.cpp` source を `CXX_MODULES` file set 外の module provider として拒否したため、移行・登録変更を取り消して元の `.cppm` に戻した。partition implementation の scanner / provider 分類を整理できるまで対象外とする。
- `.cpp` はLF・末尾改行。ArtifactCore 全体 archive link と runtime は未確認。

### Script.Intrinsics implementation migration / three compiler lanes（2026-10-22）

- `ArtifactCore/src/Script/Engine/Func/ExprIntrinsics.cppm` は import のない実装単位だった。`.cpp` に移し、source manifest と module provider mapping を更新した。CMake の個別 source 条件や additional reference mapping はない。
- 実 `ExprIntrinsics.ixx` / `.cpp` の isolated CMake 4.4.4 + Ninja project は upstream clang++ 22.1.3 + Windows MSVC ABI、clang-cl 22.1.3、MSVC 19.51 の全 lane で scan・compile・static archive 作成に成功した。
- root CMake 4.4.4 / Ninja / upstream clang++ 22.1.3 の実 ArtifactCore object target で `.cpp` が成功し、root compiler command は既定の sccache launcher を通った。
- module name / public declarations を維持し、新 `.cpp` はLF・末尾改行にした。ArtifactCore 全体 archive link と runtime は未確認。

### Render.Worker implementation migration / three compiler lanes（2026-10-22）

- `ArtifactCore/src/Render/RenderWorker.cppm` は `QRunnable` 実装を含み、外部 module import のない実装単位だった。`.cpp` に移し、source manifest と module provider mapping を更新した。
- 実 `RenderWorker.ixx` / `.cpp` の isolated CMake 4.4.4 + Ninja project は upstream clang++ 22.1.3 + Windows MSVC ABI、clang-cl 22.1.3、MSVC 19.51 の全 lane で scan・compile・static archive 作成に成功した。Qt Core include path と `QT_CORE_LIB` を設定した。
- root CMake 4.4.4 / Ninja / upstream clang++ 22.1.3 の ArtifactCore object target で `.cpp` compile に成功し、コンパイル command は sccache launcher 経由だった。対象 object を削除して再ビルドした統計差分は1 request / 1 hit / 0 misses / 0 cache errors。
- public module name と API を維持し、新 `.cpp` はLF・末尾改行にした。ArtifactCore 全体 archive link と runtime は未確認。

### Graphics.ImageCache implementation migration / three compiler lanes（2026-10-22）

- `ArtifactCore/src/Graphics/GPUTextureCacheManager.cppm` は Qt Core (`QList` / `QUuid`) を使う実装単位だった。`.cpp` に移し、source manifest、provider mapping、`ArtifactCore/CMakeLists.txt` の module IFC reference condition を更新した。
- 実 `GPUTextureCacheManager.ixx` / `.cpp` の isolated CMake 4.4.4 + Ninja project は upstream clang++ 22.1.3 + Windows MSVC ABI、clang-cl 22.1.3、MSVC 19.51 の全 lane で scan・compile・static archive 作成に成功した。Qt Core headers / `QT_CORE_LIB` を設定した。
- root CMake 4.4.4 / Ninja / upstream clang++ 22.1.3 の ArtifactCore object target で `.cpp` compile に成功した。対象 object を削除して再ビルドした sccache 統計差分は1 request / 1 hit / 0 misses / 0 errors。
- module name / public API は維持し、新 `.cpp` はLF・末尾改行にした。ArtifactCore 全体 archive link と runtime は未確認。

### Render.Statics implementation migration / three compiler lanes（2026-10-22）

- `ArtifactCore/src/Render/RenderStatics.cppm` は Qt `QElapsedTimer` include を持つ `module Render.Statics;` 実装だった。`.cpp` に移し、source manifest、provider mapping、`ArtifactCore/CMakeLists.txt` の `Render.Statics` / `std` IFC reference 条件を新 path に更新した。
- 実 interface / implementation の isolated CMake 4.4.4 + Ninja project は upstream clang++ 22.1.3 + Windows MSVC ABI、clang-cl 22.1.3、MSVC 19.51 の全 lane で scan・compile・static archive 作成に成功した。Qt Core include / define を指定した。
- root CMake 4.4.4 / Ninja / upstream clang++ 22.1.3 でも ArtifactCore object target が成功した。対象 object の再ビルドで sccache 統計差分は1 request / 1 hit / 0 misses / 0 errors。
- public module name と declarations を維持し、新 `.cpp` はLF・末尾改行にした。ArtifactCore 全体 archive link と runtime は未確認。

### Blend2D_CS implementation migration / three compiler lanes（2026-10-22）

- `ArtifactCore/src/ImageProcessing/DirectCompute/Blend2D_CS.cppm` は OpenCV `cv::Mat` を使う実装で、追加の root CMake source exception はなかった。`.cpp` に移し、source manifest と module provider mapping を更新した。
- 実 `Blend2D_CS.ixx` / `.cpp` の isolated CMake 4.4.4 + Ninja project は upstream clang++ 22.1.3 + Windows MSVC ABI、clang-cl 22.1.3、MSVC 19.51 の全 lane で scan・compile・static archive 作成に成功した。OpenCV include directory を明示した。
- root CMake 4.4.4 / Ninja / upstream clang++ 22.1.3 の実 ArtifactCore object target でも compile 成功。対象 object を削除して再ビルドした sccache 統計差分は1 request / 1 hit / 0 misses / 0 errors。
- module name と public declarations を維持し、新 `.cpp` はLF・末尾改行にした。ArtifactCore 全体 archive link と runtime は未確認。

### CompressQuality implementation migration / three compiler lanes（2026-10-22）

- `ArtifactCore/src/Image/CompressQuality.cppm` は Qt `QString` を使う実装 attachment だった。`.cpp` に移し、source manifest、provider mapping、`ArtifactCore/CMakeLists.txt` の `CompressQuality` IFC 参照条件を更新した。
- 実 `CompressQuality.ixx` / `.cpp` の isolated CMake 4.4.4 + Ninja project は upstream clang++ 22.1.3 + Windows MSVC ABI、clang-cl 22.1.3、MSVC 19.51 の全 lane で scan・compile・static archive 作成に成功した。Qt Core include / define と interface の相対 DllExport header include root を設定した。
- root CMake 4.4.4 / Ninja / upstream clang++ 22.1.3 の ArtifactCore object target で `.cpp` compile に成功した。対象 object の再ビルドで sccache 差分は1 request / 1 hit / 0 misses / 0 errors。
- module name / public API は維持し、新 `.cpp` はLF・末尾改行にした。ArtifactCore 全体 archive link と runtime は未確認。

### Image.CvMatPaintEngine implementation migration / three compiler lanes（2026-10-22）

- `ArtifactCore/src/Image/CvMatPaintEngine.cppm` は Qt Gui paint API を使う実装 attachment だった。`.cpp` に移し、source manifest と module provider mapping を更新した。root CMake に追加の per-source exception はない。
- 実 `CvMatPaintEngine.ixx` / `.cpp` の isolated CMake 4.4.4 + Ninja project は upstream clang++ 22.1.3 + Windows MSVC ABI、clang-cl 22.1.3、MSVC 19.51 の全 lane で scan・compile・static archive 作成に成功した。Qt Core/Gui include paths と defines を設定した。
- root CMake 4.4.4 / Ninja / upstream clang++ 22.1.3 の実 ArtifactCore object target でも compile 成功。対象 object の再ビルドで sccache 差分は1 request / 1 hit / 0 misses / 0 errors。
- module name / public API は維持し、新 `.cpp` はLF・末尾改行にした。ArtifactCore 全体 archive link と runtime は未確認。

### Graphics.GPU.Info implementation migration（2026-10-22）

- `ArtifactCore/src/GPU/GPUInfo.cppm` は実質空の module implementation attachment で、旧 `vendor()` / `renderer()` 実装はコメントアウトされていた。`.cpp` に移し、source manifest、module provider mapping、`ArtifactCore/CMakeLists.txt` の自己 IFC reference 条件を更新した。
- CMake 4.4.4 / Ninja / upstream clang++ 22.1.3 + Windows MSVC ABI の実 ArtifactCore object target で P1689 scan と `.cpp` compile が成功し、`Graphics.GPU.Info` IFC 依存も `CXX.dd` に記録された。
- 対象 object のみ削除して root target を再ビルドした sccache 統計差分は1 request / 1 hit / 0 misses / 0 errors。
- clang-cl / MSVC の isolated lane、ArtifactCore 全体 archive link、runtime は未確認。新 `.cpp` はLF・末尾改行にした。

### Math.RotationTurns implementation migration / three compiler lanes（2026-10-22）

- `ArtifactCore/src/Math/RotationTurns.cppm` は module import のない実装単位だった。`.cpp` に移し、source manifest と module provider mapping を更新した。追加の `ArtifactCore/CMakeLists.txt` source exception はなかった。
- 実 `RotationTurns.ixx` / `.cpp` の isolated CMake 4.4.4 project は upstream clang++ 22.1.3 + Windows MSVC ABI / Ninja、clang-cl 22.1.3 / Ninja、MSVC 19.51 / Visual Studio generator の全 lane で P1689 scan または MSVC module scan、compile、static archive 作成に成功した。
- root CMake 4.4.4 / Ninja / upstream clang++ 22.1.3 の実 ArtifactCore object target でも compile 成功。対象 object の再ビルドで sccache 統計差分は1 request / 1 hit / 0 misses / 0 errors。
- public module name / API を維持し、新 `.cpp` はLF・末尾改行にした。ArtifactCore 全体 archive link と runtime は未確認。

### Windows Clang / OpenUSD `arch` compatibility follow-up（2026-10-22）

- `ArtifactCore` target を upstream clang++ MSVC ABI で起動したところ、ArtifactCore のコンパイルへ進む前に OpenUSD `arch` の `attributes.cpp` が Windows 用 `Arch_ConstructorEntry` / `Arch_ConstructorInit` 宣言を認識せず停止した。
- 親 `CMakeLists.txt` は OpenUSD ソースを編集せず、ビルドディレクトリに `attributes.h` の互換コピーを生成する。Windows Clang では compiler attributes の Clang 分岐を保ちつつ、Windows constructor 定義が Windows 分岐へ到達するよう constructor 条件だけを狭める。
- `arch` を再ビルドし、`attributes.cpp` と `stackTrace.cpp` の compile が進むことを確認した。現在の次の blocker は `demangle.cpp` の MSVC ABI 環境にない `cxxabi.h` と、`errno.cpp` が Windows Clang を MSVC strerror path と認識しない `strerror_r` 呼び出し。OpenUSD submodule は clean で未変更。全 ArtifactCore build / link は未完了。

### ArtifactCore upstream clang++ full-target investigation（2026-10-22）

- 上記 OpenUSD `arch` の互換対応を通過し、upstream clang++ 22.1.3 / Windows MSVC ABI / Ninja で `ArtifactCore` の P1689 scan と多数の module compile に進んだ。sccache 経由で実行されている。
- 5回の段階的な build pass で、`PyroSimulation.cppm`、`Diagnostics/Trace.ixx`、stb 実装、複数の GPU/image/physics/script implementation を含む多くの object compile が成功した。ArtifactCore の compile 全体、archive link、runtime は未完了。
- Clang が顕在化させた直接 include / import 不足を局所修正した。例: Diligent `PipelineState.h`、`Frame.Debug`、`Container.Debug`、`Math.Interpolate`、`Render.ImageBuffer`、`Image.ExportOptions`、`Image.ImageF32x4RGBAWithCache`、Qt `QString`、標準 `<span>` / `<vector>` / `<map>`。
- 継続 blocker は、`Utils.String.Like` / `Utils.String.View` / `Utils.Path` の重複 `toQString` 宣言、`ScriptContext` の同一型前方宣言競合、`LayerMatte` / `FallbackPolicy` の二重 export、Mesh の nested default initializer、`NetworkRPCServer` module provider 不在、`Audio.Modulation.Router` / `Event.Bus` の provider 順序または CMake target dependency、及び `ContainerName` を使う複数 module の明示 import 漏れである。これらは `.cppm`→`.cpp` 対象単体の失敗ではなく、ArtifactCore全体を upstream clang++ で通すための module 境界・登録課題として分離する。
- sccache の再ヒット統計は今回の全体 target pass では集計していない。既に記録済みの単体 object hit 実績と、全体 target の成功は区別する。次は module provider の未登録・順序を確認し、API重複や unrelated interface 修正を広げずに blocker を小さく分割する。

### ArtifactCore upstream clang++ target build pass（2026-10-22）

- CMake 4.4.4 / Ninja / upstream clang++ 22.1.3（Windows MSVC ABI）/ sccache の構成で `ArtifactCore` target の全 object compile と `ArtifactCore.lib` archive link が完了した。
- 依存循環を避けるため、ArtifactCore が直接使う `Event.Bus`、`Audio.Modulation.Modulator` / `Router`、`NetworkRPCServer` の provider を ArtifactCore target に統合し、分割 target 側から外した。
- `Math.Random` を `float2` / `float3` の唯一の型所有 module とし、`Particle` は再 export する形に整理した。`ParticleSystem.cppm` に `QVector3D` の直接 include を加えた。
- これで前項の blocker として記録した module 重複、provider順序、`ContainerName`、Router の名前衝突を越えて ArtifactCore target が完了した。アプリ全体の link、runtime、clang-cl / MSVC の root project ビルドは未確認。
- `sccache` は build tree 内の絶対パスで CMake launcher に設定されている。現在の PowerShell PATH からは `sccache` コマンド名を解決できず、今回の全体ビルドで cache hit 数は未集計。以前の個別 object hit 実績と区別する。
### LLVM 23.1.4 Windows upstream clang++ / Artifact target follow-up（2026-10-22）

- LLVM 23.1.4 公式 Windows x64 配布の SHA-256 を release metadata と照合し、独立 Ninja build tree を作成した。最初は J: の空き容量不足で PCM 出力に失敗したため、build tree を空き容量のある V: に移して CMake を再構成した。
- Artifact target の最初の pass は `Artifact/src/Widgets/ArtifactColorPaletteWidget.cppm` の `#ifndef Q_MOC_RUN` が閉じられず、`module Artifact.Widgets.ColorPaletteWidget;` が条件コンパイル下に入る Clang scanner error で停止した。標準ヘッダーの skip guard を module declaration 前で閉じる最小修正を加え、Artifact と親の `main` に push 済み。
- `ArtifactColorPaletteWidget.cppm` の guard 修正後、LLVM 23.1.4 で Artifact の依存 scan と多数の implementation compile が進んだ。`ArtifactCompositionPlaybackController` / `ArtifactAudioScrubController` / `ArtifactPlaybackEngine` / `ArtifactPlaybackService` / `ArtifactProjectManager` の Verdigris 型登録重複、`UndoManager.ixx` の `SharedPtr` import 漏れ、`VSTEffect.ixx` の `Core.ArtifactString` import 漏れ、`GauusianBlur.cppm` の Qt 型 include 漏れを修正した。
- Artifact full target は未完了。Clang 23.1.4 が `ArtifactPreviewCompositionPipeline.cppm` で 1,260 files / 約964.86 MB、`ArtifactLayerFactory.cppm` で1,160 files / 約976.44 MB、`ArtifactProject.cppm` で1,490 files / 約966.30 MBを読み込んだ後、`ran out of source locations` で停止した。Preview の video frame access を独立 interface に分離する試行でも、次の `Artifact.Layer.Solid2D` import で同じ上限に達したため、その試行コードは残していない。Artifact 全体 link と runtime は未確認。
- full target の compile command は WinGet 配布の `sccache` launcher 経由だった。Artifact 全体の cache 統計は未集計。別途、isolated smoke の cache hit 統計を以下に記録する。
### LLVM 23.1.4 isolated module / sccache smoke（2026-10-22）

- `tools/clang-msvc-abi-smoke` を CMake 4.2.2 / Ninja / upstream clang++ 23.1.4 / `x86_64-pc-windows-msvc` で configure し、P1689 dependency scan、`.ixx` module interface、module importer/implementation、通常 `.cpp` translation units の compile と両 executable の link に成功した。
- sccache 0.18.0 は WinGet install path から指定した。clean build 2回後の統計は15 compile requests、5 cache hits、10 misses、0 errors。通常 TU と module-related commandsを合わせた部分 hit が確認できた。これは smoke project の結果であり、Artifact 全体 build の cache rate ではない。
- Artifact full target は Clang 23.1.4 の source-location 上限で停止したため、Artifact 全体 link / runtime と全体 sccache 統計は未確認。

### Artifact implementation `.cpp` attachment pilot（2026-10-22）

- `Artifact/src/Composition/ArtifactCompositionSetting.cppm` は `module Artifact.Composition.Setting;` のみを持つ import-free implementation unit だったため、同じ module name の `.cpp` に移し、`Artifact/cmake/ArtifactSources.cmake` の `ARTIFACT_APP_IMPL_SOURCES` 登録を更新した。Artifact 側 `.gitattributes` に `.cpp` / `.cmake` の `eol=lf` を明示追加した。公開 `ArtifactCompositionSetting.ixx` と module API は維持した。`.cpp` と変更した manifest は LF。
- upstream clang++ 23.1.4 の CMake 4.4.4 / Ninja 1.13.2 target で実ソースを P1689 scan し、`Artifact.Composition.Setting` provider attachment として認識することを確認。Artifact の既存 build tree にある同 module の実 BMI を指定した CMake 生成 compile command で object を生成した。sccache 0.18.0 で同じ command を2回実行し、2 compile requests / 2 cache hits / 0 errors。
- clang-cl 22.1.3 と upstream clang++ 23.1.4 の両方で、CMake 4.4.4 の isolated executable に実 `.cpp` を登録し、同名 module の最小 stub interface と importer を用いて P1689 scan、compile、link、実行に成功した。これは拡張子・module attachment・scanner の pilot であり、Artifact の実 `ArtifactCompositionSetting.ixx` 依存を使う clang-cl 統合確認ではない。
- 各 isolated CMake pilot は clean build を2回実行した。clang-cl は 6 compile requests のうち4 cacheable executions がすべて hit、0 miss / 0 cache errors（2 requests は multiple inputs で non-cacheable）。clang++ は6 requests、3 misses / 3 hits、0 cache errors。Artifact 全体の build / link / runtime は引き続き未確認。

### Artifact.Gui.Manager implementation migration / three compiler lanes（2026-10-22）

- `Artifact/src/GUI/ArtifactGuiManager.cppm` は import-free implementation unit で、constructor / destructor の defaulted definitions のみを持つ。module name と公開 QObject interface を変えず `.cpp` へ移し、`Artifact/cmake/ArtifactSources.cmake` の明示 implementation manifest を更新した。
- 実 `ArtifactGuiManager.ixx` / `.cpp` を使う CMake 4.4.4 / Ninja smoke で、upstream clang++ 23.1.4（Windows MSVC ABI）、clang-cl 22.1.3、MSVC 19.51 の各 lane が P1689 または MSVC module scan、interface / implementation / importer compile、static library と executable link に成功し、3 executable の実行も成功した。Qt Core をリンクした。
- upstream clang++ の clean build と clean rebuild は sccache 0.18.0 下で合計6 compile requests、3 misses / 3 hits、0 errors。build tree は隔離 smoke 用で、Artifact 全体の sccache 統計ではない。
- 実 Artifact target の object-only Ninja build は source manifest の再構成後に6,622 actionsへ依存展開したため約180 actionsで中断した。したがって root target への統合 object build / Artifact 全体 link は今回未確認。`.cpp` と manifest は LF、`git diff --check` は成功。

### Core.SystemStats implementation migration / three compiler lanes（2026-10-22）

- `Artifact/src/Core/SystemStats.cppm` は Windows / POSIX の system metrics 実装を含む `module Core.SystemStats;` implementation unit で、外部 module import はない。`.cpp` に移し、`Artifact/cmake/ArtifactSources.cmake` の明示 implementation manifest を更新した。公開 interface と実装本文は維持し、移行後 source は LF。
- 実 `SystemStats.ixx` / `.cpp` と importer を使う CMake 4.4.4 / Ninja smoke で、upstream clang++ 23.1.4（Windows MSVC ABI）、clang-cl 22.1.3、MSVC 19.51 の各 lane が P1689 または MSVC module scan、interface / implementation / importer compile、static library と executable link、executable runtime に成功した。
- upstream clang++ の clean build と clean rebuild は sccache 0.18.0 下で合計6 compile requests、3 misses / 3 hits、0 errors。Artifact 全体の cache 統計ではない。
- `git check-attr eol` は `.cppm` に `eol=lf` を示したが、移行前 worktree は CRLF だった。`.cpp` 化の際に LF へ揃え、Git の正規化後は元実装から100% renameとして判定された。Artifact 全体 target / link は引き続き未確認。

### Artifact.Widgets.LayerLockIndicator implementation migration / three compiler lanes（2026-10-22）

- `Artifact/src/Widgets/Timeline/LayerLockIndicator.cppm` は Qt Widgets / Verdigris を使う implementation unit で、module import はない。`W_OBJECT_IMPL`、widget の状態操作と既存 paint implementation を含む本文をそのまま `.cpp` に移し、`Artifact/cmake/ArtifactSources.cmake` の明示 implementation manifest を更新した。公開 `.ixx` は変更していない。
- 実 `LayerLockIndicator.ixx` / `.cpp` と importer の CMake 4.4.4 / Ninja smoke で、upstream clang++ 23.1.4（Windows MSVC ABI）、clang-cl 22.1.3、MSVC 19.51 の全 lane が dependency scan、interface / implementation / importer compile、static library と executable link に成功した。clang-cl は CMake の `-TP` に unused argument 警告を出したが build は成功。
- upstream clang++ + sccache 0.18.0 の初回 build は3 misses、clean rebuild は3 hits、cache errors 0。後続の同 smoke clean build では既存 cache から6 requestsすべて hitした。
- `QApplication` を使う executable runtime は Qt platform initialization 中に停止し、widget 生成前に到達しなかったため未確認。移行した `.cpp` と manifest は LF、rename diff は100%。Artifact 全体 target / link は未確認。

### Artifact.Generator.Abstract implementation migration / three compiler lanes（2026-10-22）

- `Artifact/src/Generator/AbstractGenerator.cppm` は module import のない implementation unit で、raw `Impl*` の生成と破棄だけを定義する。本文と `Artifact.Generator.Abstract` module name を維持して `.cpp` に移し、`Artifact/cmake/ArtifactSources.cmake` の explicit implementation manifest を更新した。
- 実 `AbstractGenerator.ixx` / `.cpp` と importer を使う CMake 4.4.4 / Ninja smoke で、upstream clang++ 23.1.4（Windows MSVC ABI）、clang-cl 22.1.3、MSVC 19.51 の各 lane が module dependency scan、interface / implementation / importer compile、static library / executable link、executable runtime に成功した。clang-cl は unused `-TP` warning を出したが build と run は成功。
- upstream clang++ + sccache 0.18.0 の clean build / clean rebuild は合計6 requests、3 misses / 3 hits、cache errors 0。`.cpp` と manifest は LF、rename は100%。Artifact 全体 target / link は未確認。

### ArtifactWidgets.AbstractDialog implementation migration（2026-10-22）

- `ArtifactWidgets/src/Common/AbstractDialog.cppm` は `Widgets.Dialog.Abstract` の import-free implementation unit。公開 `AbstractDialog.ixx` と実装の挙動を維持して `.cpp` に移し、Clang のコンパイル境界を明確にするため直接使う Qt 型のヘッダーを global module fragment に追加し、`ArtifactWidgets/CMakeLists.txt` の明示 implementation list を更新した。
- `.cpp` は LF、module declaration 前に必要な Qt ヘッダーを配置し、`git diff --check` は成功。ビルド／依存スキャン／リンクはプロジェクト指示に従って今回は実行していない。

### ArtifactWidgets.GlowFrame implementation migration（2026-10-22）

- `ArtifactWidgets/src/Effect/WidgetGlowFrame.cppm` は `Widgets.Effects.GlowFrame` の import-free implementation unit で、公開 interface を import する利用箇所は2つ。`.cpp` に移し、`ArtifactWidgets/CMakeLists.txt` の明示 implementation list を更新した。描画・focus 表示の実装は変更していない。
- `.cpp` は LF。ビルド／依存スキャン／リンクはプロジェクト指示に従って今回は実行していない。

### ArtifactWidgets.TimeCodeEditor implementation migration（2026-10-22）

- `ArtifactWidgets/src/Time/TimeCodeEditor.cppm` は `Widgets.TimeCodeEditor` の空の import-free implementation unit だったため、module 宣言を維持して `.cpp` に移し、明示 implementation list を更新した。公開 `TimeCodeEditor.ixx` は変更していない。
- `.cpp` は LF。ビルド／依存スキャン／リンクはプロジェクト指示に従って今回は実行していない。

### ArtifactWidgets.AppProgress Windows implementation migration（2026-10-22）

- `ArtifactWidgets/src/Common/AppProgress_Win.cppm` は `AppProgress` module の import-free Windows implementation unit で、`RenderQueueManagerWidget` から実際に利用されている。module 名と taskbar progress 処理を維持して `.cpp` に移し、明示 implementation list を更新した。
- `.cpp` は LF。ビルド／依存スキャン／リンクはプロジェクト指示に従って今回は実行していない。

### ArtifactWidgets.DragSpinBox implementation migration（2026-10-22）

- `ArtifactWidgets/src/Common/DragSpinBox.cppm` は `DragSpinBox` module の実装で、`Core.ArtifactMath` を import する。Artifact の composition / plane creation dialogs が `DragSpinBox` / `DoubleDragSpinBox` を利用している。module import と操作実装を維持して `.cpp` へ移し、CMake の明示 implementation list を更新した。
- 直接使う Qt の spin box、event、point、widget ヘッダーを global module fragment に置き、`.cpp` は LF に揃えた。ビルド／依存スキャン／リンクはプロジェクト指示に従って今回は実行していない。

### ArtifactWidgets.InterpretFootageDialog implementation migration（2026-10-22）

- `ArtifactWidgets/src/Dialog/InterpretFootageDialog.cppm` は `Widgets.Dialog.InterpretFootage` implementation unit で、`Media.SourceInterpret` を import する。Artifact の Asset Browser がこの dialog を利用している。module import と既存 UI 実装を維持して `.cpp` へ移し、明示 implementation list を更新した。
- 直接使う Qt dialog / widget / string headers と `<memory>` を global module fragment に置き、`.cpp` は LF。ビルド／依存スキャン／リンクはプロジェクト指示に従って今回は実行していない。

### ArtifactWidgets.EditableLabel implementation migration（2026-10-22）

- `ArtifactWidgets/src/Common/EditableLabel.cppm` は `Widgets.EditableLabel` implementation unit で、`Common.ClikableLabel` を import する。Artifact の composition / plane dialogs と render queue panel から利用されている。既存の editing / animation implementation と module import を維持して `.cpp` へ移し、明示 implementation list を更新した。
- 直接使う animation、layout、event、widget、string の Qt headers を global module fragment に追加し、`.cpp` は LF。ビルド／依存スキャン／リンクはプロジェクト指示に従って今回は実行していない。

### ArtifactWidgets.EnhancedSlider implementation migration（2026-10-22）

- `ArtifactWidgets/src/Common/EnhancedSlider.cppm` は `EnhancedSlider` module implementation で、Float Color Picker が horizontal / vertical slider として利用する。`Core.ArtifactMath` import と custom painting / pointer interaction implementation を保ったまま `.cpp` へ移し、CMake の explicit implementation list を更新した。
- 直接使う slider、widget、geometry、color、palette、pen、object の Qt headers を global module fragment に置いた。`.cpp` は LF。ビルド／依存スキャン／リンクはプロジェクト指示に従って今回は実行していない。

### ArtifactWidgets.ColorWheel implementation migration（2026-10-22）

- `ArtifactWidgets/src/Color/ColorWheel.cppm` は `Widgets.ColorWheel` implementation unit で、`Core.ArtifactMath` と `Color.Float` を import する。Float Color Picker の色選択 UI から利用される。import、QImage の既存 wheel cache、色選択・paint implementation を保ったまま `.cpp` へ移し、CMake explicit implementation list を更新した。
- 直接使う color、paint event、pen、point、rect、size の Qt headers を global module fragment に置き、既存 UTF-8 BOM を保持して `.cpp` を LF に揃えた。ビルド／依存スキャン／リンクはプロジェクト指示に従って今回は実行していない。

### ArtifactWidgets.KeyboardOverlayDialog implementation migration（2026-10-22）

- `ArtifactWidgets/src/Dialog/KeyboardOverlayDialog.cppm` は `Widgets.KeyboardOverlayDialog` implementation unit で、`Input.Operator` を import する。Artifact Help menu から起動され、key binding を検索・表示する。既存 UI / filtering / shortcut lookup と import を維持して `.cpp` に移し、CMake explicit implementation list を更新した。
- 直接使う abstract item view、GUI application、QObject、QString、QWidget のヘッダーを global module fragment に追加した。source は LF。ビルド／依存スキャン／リンクはプロジェクト指示に従って今回は実行していない。

### ArtifactWidgets.VectorScopeWidget implementation migration（2026-10-22）

- `ArtifactWidgets/src/Color/VectorScopeWidget.cppm` は `VectorScopeWidget` implementation で、`Core.ArtifactMath` と `AbstractWidget` を import する。Artifact の Color Science panel と Composition Editor から利用される。module imports と既存 vectorscope 実装を維持して `.cpp` に移し、CMake の通常 implementation list に追加した。legacy scope list からは旧 `.cppm` entry を外し、同じ実装の重複登録を避けた。
- 直接使う paint/resize event、pen、point、QString、QRgb と math headers を global module fragment に置いた。source は LF。ビルド／依存スキャン／リンクはプロジェクト指示に従って今回は実行していない。

### ArtifactWidgets.WaveformScopeWidget implementation migration（2026-10-22）

- `ArtifactWidgets/src/Color/WaveformScopeWidget.cppm` は `WaveformScopeWidget` implementation unit で、`Core.ArtifactMath` を import する。Artifact の Color Science panel と Composition Editor から利用される。既存 QtConcurrent / QFutureWatcher の実装と module import を維持して `.cpp` へ移し、CMake の通常 implementation list に追加、専用 legacy list から旧 path を除いた。
- 直接使う image pixel、color、font、paint/resize event、pen、rect、QString、QObject の headers を global module fragment に追加し、`.cpp` を LF に揃えた。async completion の動作は変更していない。ビルド／依存スキャン／リンクはプロジェクト指示に従って今回は実行していない。

### ArtifactWidgets.HistgramWidget implementation migration（2026-10-22）

- `ArtifactWidgets/src/Color/HistgramWidget.cppm` は `HistgramWidget` module implementation unit で、Artifact の Color Science panel と Composition Editor から HistogramWidget として利用される。module 名・公開 interface・histogram / paint implementation を維持して `.cpp` に移し、CMake の通常 implementation list に追加した。これで専用 legacy scope implementation list は不要になったため削除した。
- 直接使う QRgb、widget、color、font、pen、rect、QString headers を global module fragment に置き、`.cpp` を LF に揃えた。描画・集計動作は変更していない。ビルド／依存スキャン／リンクはプロジェクト指示に従って今回は実行していない。

### ArtifactWidgets.ParadeScopeWidget implementation migration（2026-10-22）

- `ArtifactWidgets/src/Color/ParadeScopeWidget.cppm` は `ParadeScopeWidget` implementation unit で、Color Science panel、Composition Editor、Contents Viewer から利用される。module 名・公開 interface・非同期 scope 集計・描画処理を保って `.cpp` に移し、CMake の明示 implementation list に登録した。
- 直接使う Qt の QObject / QRect / QString / Qt、color / font / paint event / pen / resize event / QRgb headers を global module fragment に追加し、LF 改行に揃えた。動作経路は変更していない。ビルド／依存スキャン／リンクは実行していない。

### ArtifactWidgets.FloatColorPicker implementation migration（2026-10-22）

- `ArtifactWidgets/src/Dialog/FloatColorPicker.cppm` は `FloatColorPickerDialog` module implementation unit で、Color Palette、Property Editor、Composition Editor、複数の作成／設定 dialog から import される。module 名・公開 interface・color selection UI と既存処理を維持し、implementation source を `.cpp` に移して CMake の明示リストに登録した。
- 直接使う QChar / QString / QOverload 定義 / QFont / QPalette / QAbstractSpinBox / QWidget headers を global module fragment に追加し、LF 改行へ揃えた。既存 UI 接続と signal 経路は変更していない。ビルド／依存スキャン／リンクは実行していない。

### ArtifactWidgets.AbstractWidget implementation migration（2026-10-22）

- `ArtifactWidgets/src/AbstractWidget.cppm` は `AbstractWidget` module implementation unit で、scope widgets、video preview、image viewer の interface から参照される。`Widgets.Effects.GlowFrame` import と implementation body を保って `.cpp` へ移し、CMake の明示 implementation list に登録した。
- `QFocusEvent` / `QResizeEvent` / `QWidget` の直接 headers を global module fragment に加え、LF 改行へ揃えた。公開 interface・挙動は変更せず、ビルド／依存スキャン／リンクは実行していない。

### ArtifactWidgets.RenderQueueManagerWidget implementation migration（2026-10-22）

- `ArtifactWidgets/src/Render/RenderQueueManagerWidget.cppm` は `Widgets.Render.Queue` implementation unit で、Artifact `AppMain` が public module を import している。4 つの `W_OBJECT_IMPL`、既存 module imports と implementation body を維持して `.cpp` へ移し、CMake の明示 implementation list に登録した。
- source が直接使う Qt model / view / image / object / string / geometry / style-option headers を global module fragment に追加し、LF 改行に揃えた。UI 接続や描画・queue 処理は変更していない。ビルド／依存スキャン／リンクは実行していない。

### ArtifactWidgets.SyntaxHighlighter implementation migration（2026-10-22）

- `ArtifactWidgets/src/Code/SyntaxHighlighter.cppm` は `Code.SyntaxHighlighter` implementation unit で、`CodeEditor.cppm` が module を import する。module name・public interface・既存 highlighter 実装を維持して `.cpp` に移し、CMake の明示 implementation list に登録した。
- 直接使う QColor / QFont / QObject / QString headers を global module fragment に追加し、LF 改行へ揃えた。動作は変更していない。ビルド／依存スキャン／リンクは実行していない。

### ArtifactWidgets.CodeEditor implementation migration（2026-10-22）

- `ArtifactWidgets/src/Code/CodeEditor.cppm` は `CodeEditor` implementation unit で、`Code.SyntaxHighlighter` を import する。module import、`W_OBJECT_IMPL`、既存 editor / line-number rendering implementation を維持して `.cpp` へ移し、CMake の明示 implementation list に登録した。
- 直接使う QChar / QColor / QFont / QFrame / QPaintEvent / QPalette / QRect / QSize / QString / Qt headers を global module fragment に追加し、LF 改行に揃えた。既存 signal connections を含め動作は変更していない。ビルド／依存スキャン／リンクは実行していない。

### ArtifactWidgets.FloatColorPickerButton implementation migration（2026-10-22）

- `ArtifactWidgets/src/Button/FloatColorPickerButton.cppm` は `FloatColorPickerButton` implementation unit で、`Color.Float` と `FloatColorPickerDialog` を import する。module imports、`W_OBJECT_IMPL`、picker 起動と色の反映処理を維持して `.cpp` に移し、CMake の明示 implementation list に登録した。
- 直接使う QColor / QDialog / QPalette / QWidget headers を global module fragment に追加し、LF 改行に揃えた。既存 click connection と signal 経路は変更していない。ビルド／依存スキャン／リンクは実行していない。

### ArtifactWidgets.AudioBusWidget implementation migration（2026-10-22）

- `ArtifactWidgets/src/Audio/AudioBusWidget.cppm` は `AudioBusWidget` implementation unit で、`Audio.Mixer` / `Audio.Bus` / `Core.ArtifactString` を import する。これらの import、`W_OBJECT_IMPL`、既存 mixer / bus UI 処理を維持して `.cpp` に移し、CMake の明示 implementation list に登録した。
- 直接使う QPaintEvent / QString / Qt / QWidget / `std::shared_ptr` の headers を global module fragment に加え、LF 改行に揃えた。既存 UI connections と signal 経路は変更していない。ビルド／依存スキャン／リンクは実行していない。

### ArtifactWidgets.BasicImageViewWidget implementation migration（2026-10-22）

- `ArtifactWidgets/src/Image/BasicImageViewWidget.cppm` は `BasicImageViewWidget` implementation unit で、`Widgets.Effects.GlowFrame` を import する。module import、`W_OBJECT_IMPL`、既存 image view / file-drop loading implementation を維持して `.cpp` に移し、CMake の明示 implementation list に登録した。
- 直接使う QDebug / QFocusEvent / QGraphicsPixmapItem / QImage / QObject / QPainter / QPixmap / QPoint / QRectF / QSize / QString / QUrl / QWheelEvent / QWidget / Qt headers を global module fragment に加え、LF 改行へ揃えた。既存 image loading connection と描画動作は変更していない。ビルド／依存スキャン／リンクは実行していない。

### ArtifactWidgets.WinJumpList implementation migration（2026-10-22）

- `ArtifactWidgets/src/Common/WinJumpList.cppm` は Windows Shell COM を使う import-free `WinJumpList` implementation unit で、対応する public `WinJumpList.ixx` と module 名が一致する。実装と公開 API を維持して `.cpp` に移し、明示 implementation source list に登録した。
- 既存 Windows 専用 include と Jump List 処理は変更していない。`.cpp` と `.cppm` の LF 属性を `ArtifactWidgets/.gitattributes` に明記した。ビルド／依存スキャン／リンクは実行していない。

### ArtifactWidgets.WinTaskbarProgress implementation migration（2026-10-22）

- `ArtifactWidgets/src/Common/WinTaskbarProgress.cppm` は Windows Shell Taskbar COM の import-free implementation unit で、対応する public `WinTaskbarProgress.ixx` と module 名が一致する。既存の progress state 実装を保って `.cpp` へ移し、明示 implementation source list に登録した。
- 現行 Artifact / ArtifactWidgets source には呼び出し元が見つからなかった。public interface と実装は削除せず、直接使う Windows / QWidget headers を維持する。ビルド／依存スキャン／リンクは実行していない。

### ArtifactWidgets.CommandAction implementation migration（2026-10-22）

- `ArtifactWidgets/src/Common/CommandAction.cppm` は `CommandAction` の import-free implementation unit で、`include/Common/CommandAction.ixx` が同名 module interface を宣言する。`W_OBJECT_IMPL`、PImpl ownership、既存 QAction connection、method bodies を変更せず `.cpp` に移し、明示 implementation source list に登録した。
- implementation は既存どおり interface module から型宣言を得る。`.cpp` は LF。ビルド／依存スキャン／リンクは実行していない。別途確認候補の既存 `hasCommand()` 実装上の懸念は `Insight.md` に未検証として記録した。

### ArtifactWidgets.ConsoleWidget implementation migration（2026-10-22）

- `ArtifactWidgets/src/Console/ConsoleWidget.cppm` は `ConsoleWidget` module の implementation unit で、`include/Console/ConsoleWidget.ixx` が対応する public interface を宣言する。既存の `W_OBJECT_IMPL`、Impl ownership、console append / clear 処理を保って `.cpp` に移し、明示 implementation source list に登録した。
- Qt headers は既存どおり global module fragment にあり、既存 context-menu / action connections を含め動作変更はない。source は LF。ビルド／依存スキャン／リンクは実行していない。

### ArtifactWidgets.KnobSlider implementation migration（2026-10-22）

- `ArtifactWidgets/src/Knob/KnobSlider.cppm` は `Widget.Knob.Slider` module implementation unit で、`include/Knob/KnobSlider.ixx` が同 module の public interface を宣言する。`W_OBJECT_IMPL` と既存 event / paint method bodies を維持して `.cpp` へ移し、明示 implementation source list に登録した。
- interface include への依存を避けるため、source が署名に使う `QPaintEvent` / `QPoint` / `QWheelEvent` headers を global module fragment に追加した。deprecated API と実装動作は変更していない。`impl_` 初期化に関する別件の未検証懸念は `Insight.md` に記録した。ビルド／依存スキャン／リンクは実行していない。

### ArtifactWidgets.RotaryKnob implementation migration（2026-10-22）

- `ArtifactWidgets/src/Knob/KnobEditorWidget/RotaryKnob.cppm` は `Widgets.Knob.Rotary` module implementation unit で、`include/Knob/RotaryKnob.ixx` が対応 interface を宣言する。`W_OBJECT_IMPL` と Impl の constructor / destructor、widget constructor / destructor を維持して `.cpp` へ移し、明示 implementation source list に登録した。
- deprecated API と公開 interface は変更していない。source は LF。ビルド／依存スキャン／リンクは実行していない。

### ArtifactWidgets.AbstractKnobEditor implementation migration（2026-10-22）

- `ArtifactWidgets/src/Knob/AbstractKnobEditor.cppm` は `Widgets.Knob` implementation unit で、`include/Knob/AbstractKnobEditor.ixx` が同 module の public interface を宣言する。`W_OBJECT_IMPL`、Impl ownership、drag/drop acceptance と event methods を維持して `.cpp` へ移し、明示 implementation source list に登録した。
- source が直接使う `QWidget` を global module fragment に追加した。deprecated API と処理は変更していない。ビルド／依存スキャン／リンクは実行していない。

### ArtifactWidgets.VolumeSlider implementation migration（2026-10-22）

- `ArtifactWidgets/src/Knob/VolumeSlider.cppm` は import-free `VolumeSlider` implementation unit で、`include/Audio/VolumeSlider.ixx` が対応する public interface を宣言する。既存 `W_OBJECT_IMPL` と class / method definitions を維持して `.cpp` に移し、明示 implementation source list に登録した。
- source が直接使う `QWidget` header を global module fragment に追加した。repository source 内での利用元は見つからなかった。既存の空 method bodies や public API は変更していない。ビルド／依存スキャン／リンクは実行していない。

### ArtifactWidgets.HeadPanel implementation migration（2026-10-22）

- `ArtifactWidgets/src/Common/HeadPanel.cppm` は `HeadPanel` module implementation unit で、`include/Dock/HeadPanel.ixx` が `HeaderPanel` public interface を宣言する。`W_OBJECT_IMPL`、layout / child widget construction、既存 context-menu style collapse connection を維持して `.cpp` へ移し、明示 implementation source list に登録した。
- implementation に `Widgets.Utils.CSS` の symbol use がないことを確認し、不要な module import と `using namespace ArtifactCore` を除いて Clang module dependency を減らした。直接使う `QString` header を global module fragment に加えた。UI behavior と public interface は変更していない。ビルド／依存スキャン／リンクは実行していない。

### ArtifactWidgets.RayTracerWidget implementation migration（2026-10-22）

- `ArtifactWidgets/src/Render/RayTracerWidget.cppm` は `ArtifactWidgets.Render.RayTracerWidget` implementation unit で、`include/Render/RayTracerWidget.ixx` が対応 interface を宣言する。`Render` module import、既存 ray tracing / image / paint / key handling implementation を保ったまま `.cpp` へ移し、明示 implementation source list に登録した。
- source が直接使う `QString` / `QColor` / `QRect` headers を global module fragment に明示した。描画・計算経路は変更していない。ビルド／依存スキャン／リンクは実行していない。

### ArtifactWidgets.PreviewControlWidget implementation migration（2026-10-22）

- `ArtifactWidgets/src/Preview/PreviewControlWidget.cppm` は `Preview.ControlWidget` implementation unit で、`include/Preview/PreviewControlWidget.ixx` が対応する public interface を宣言する。`W_OBJECT_IMPL`、Impl ownership、既存 method definitions を維持して `.cpp` へ移し、明示 implementation source list に登録した。
- source の Qt dependencies は既に global module fragment に直接 include されている。現行 repository source 内に利用元は見つからなかった。空の既存 method bodies と public interface は変更していない。ビルド／依存スキャン／リンクは実行していない。

### ArtifactWidgets.ArtifactBasicVideoPreviewWidget implementation migration（2026-10-22）

- `ArtifactWidgets/src/Video/ArtifactBasicVideoPreviewWidget.cppm` は `Widgets.VideoPreview` implementation unit で、`include/Video/ArtifactBasicVideoPreviewWidget.ixx` が同名 public interface を宣言する。FFmpeg decoder / thumbnail / video-frame module imports と既存 implementation を保って `.cpp` へ移し、明示 implementation source list に登録した。
- source が直接使う `QAudioFormat` / `QByteArray` / `QDragEnterEvent` / `QObject` / `QPixmap` / `QSize` / `QString` headers を global module fragment に追加した。video decode / preview behavior は変更していない。ビルド／依存スキャン／リンクは実行していない。

### ArtifactWidgets.ModelViewer implementation migration（2026-10-22）

- `ArtifactWidgets/src/Viewer/ModelViewer.cppm` は `Widgets.ModelViewer` implementation unit で、`include/3D/ModelViewer.ixx` が同名 public interface を宣言する。既存 `ModelViewer::Impl` definition と constructor / destructor definitions を保持して `.cpp` に移し、明示 implementation source list に登録した。
- source 内で使用されていなかった `QList` include を取り除き、不要な Qt header dependency を減らした。stub の挙動や public interface は拡張していない。ビルド／依存スキャン／リンクは実行していない。

### ArtifactWidgets.GraphicsFloatImageItem implementation migration（2026-10-22）

- `ArtifactWidgets/src/Graphics/GraphicsItemFloatImageItem.cppm` は `GraphicsFloatImageItem` module implementation unit として分類されているため `.cpp` に移し、明示 implementation source list に登録した。使用されていなかった `QList` include を削除した。
- 既存 source は public `GraphicsFloatImageItem` method definitions を含まず、private class stub だけを持つ。この API / implementation mismatch は変更・解決していないため `Insight.md` に記録した。ビルド／依存スキャン／リンクは実行していない。

### ArtifactCore.Rotate implementation migration（2026-10-22）

- `ArtifactCore/src/Transform/Rotate.cppm` は `module Rotate;` implementation unit で、`include/Transform/Rotate.ixx` の非 inline method definitions を持つ。本文を `.cpp` に移し、明示 manifest `cmake/ArtifactCoreSources.cmake` の source path を更新した。
- source は追加 module import / source-specific CMake reference / scan-disable rule を持たないため、既存 scanner classification を維持する。public interface と method behavior は変更していない。ビルド／依存スキャン／リンクは実行していない。

### ArtifactCore.AssetConverter implementation migration（2026-10-22）

- `ArtifactCore/src/Asset/AssetConverter.cppm` は `AssetConverter` module の image conversion implementation unit で、`include/Asset/AssetConverter.ixx` の宣言を定義する。本文を `.cpp` に移し、`ArtifactCoreSources.cmake` / `ArtifactCoreModuleReferences.cmake` / Asset domain source list の自己 path を更新した。`AssetImporter` consumer mapping は public interface path を参照するため変更していない。
- source が直接使う `<memory>` / `<utility>` / `QByteArray` / `QSize` / `QString` headers を global module fragment に明示した。変換処理と public API は変更していない。ビルド／依存スキャン／リンクは実行していない。

- `ArtifactCore/src/Asset/AssetMetaFile.cppm` は `Asset` module の実装ユニットで、公開 `AssetMetaFile.ixx` の定義を含む。module 名、定義本文と API を保って `.cpp` に移し、source manifest、module reference map、Asset domain source list の自己 path を更新した。`AssetType` 依存と `AssetImporter` consumer は変更していない。ビルド／依存スキャン／リンクは未実行。

- `ArtifactCore/src/Asset/AssetDatabase.cppm` は `Asset.Database` module の実装ユニットで、path normalization、asset identity 操作、JSON import/export の定義を持つ。module 名・`AssetType` import・実装本文を維持して `.cpp` に移し、明示 source manifest、module reference map、Asset domain source list の自己 path を更新した。MSVC の Asset domain reference 設定と importer consumer は変更していない。ビルド／依存スキャン／リンクは未実行。

- `ArtifactCore/src/Asset/ImageAsset.cppm` は `ImageAsset` module の短い実装ユニットで、`ImageAsset.ixx` の定義を持つ。module 名、`IO.ImageImporter` / `Image.Raw` / `Utils.String.UniString` imports と本文を保って `.cpp` に移し、明示 source manifest、module reference map、Asset domain source list の自己 path を更新した。Tracked source の宣言行を再集計し、表の Core / Widgets 件数を現在値へ合わせた。ビルド／依存スキャン／リンクは未実行。

- `ArtifactCore/src/Time/RealTime.cppm` は `Time.Real` module の PImpl 実装ユニットで、公開 interface の全 method 定義を持つ。module 名、状態更新ロジックと PImpl ownership を保って `.cpp` に移し、明示 source manifest と module reference map の自己 path を更新した。外部 module import はない。ビルド／依存スキャン／リンクは未実行。

- `ArtifactCore/src/Time/TimeCode.cppm` を `Time.Code` module implementation unit のまま `.cpp` に移した。module 名、`Time.Rational` import と既存実装を維持し、Core の明示 source manifest、module reference map、MSVC 用 source-specific `/reference` 条件の path を更新した。実装が使う `std::max` / `int64_t` に `<algorithm>` / `<cstdint>` を直接追加した。ビルド／依存スキャン／リンクは未実行。

- `ArtifactCore/src/Time/TimeCodeRange.cppm` を `TimeCodeRange` implementation unit のまま `.cpp` に移した。module 名と `Time.Rational` import、既存実装を維持し、明示 source manifest、module reference map、MSVC 用 source-specific `/reference` 条件の path を更新した。`int64_t` 用の `<cstdint>` を global module fragment に追加した。ビルド／依存スキャン／リンクは未実行。

- `ArtifactCore/src/Frame/FrameOffset.cppm` を `Frame.Offset` implementation unit のまま `.cpp` に移した。module 宣言、既存 imports と arithmetic / time conversion 実装を保ち、Core source manifest、module reference map の provider・全 imported reference rows、および MSVC source-specific `/reference` 条件を更新した。既存の直接標準 header include は維持。ビルド／依存スキャン／リンクは未実行。

- `ArtifactCore/src/Media/MediaFrame.cppm` を `Media.MediaFrame` implementation unit のまま `.cpp` に移した。module 名、既存 PImpl と constructor / destructor definitions を保ち、明示 source manifest、module reference map、Media source list の path を更新した。`std::make_unique` の直接依存として `<memory>` を追加した。ビルド／依存スキャン／リンクは未実行。

- `ArtifactCore/src/Graphics/Effect/CreativeEffect.cppm` を `Graphics.Effect.Creative` implementation unit のまま `.cpp` に移した。実装本文と標準 header 群を維持し、明示 source manifest と module reference map の provider path を更新した。CMake は Graphics Effect 群の共通 reference 分岐を使い、個別 source path 条件はないことを確認した。ビルド／依存スキャン／リンクは未実行。

- `ArtifactCore/src/Image/PNGImage.cppm` を `Image.Png` implementation unit のまま `.cpp` に移した。module 宣言、PNG header parsing の本文と直接 header 群を保ち、source manifest、module reference map、MSVC 用 source-specific `Image.Png` reference 条件の path を更新した。ビルド／依存スキャン／リンクは未実行。

- `ArtifactCore/src/Mask/DepthMask.cppm` を `Core.Mask.DepthMask` implementation unit のまま `.cpp` に移した。alpha 計算の本文を維持し、manifest・module reference map・Core source list の path を更新した。実装で使う `Q_UNUSED` / `QVector4D` に対応し `QtGlobal` / `QVector4D` を直接 include した。ビルド／依存スキャン／リンクは未実行。

- `ArtifactCore/src/Image/ImageF32x4.cppm` を `ImageF32x4` implementation unit のまま `.cpp` に移した。OpenCV `cv::Mat` PImpl、constructor / destructor、dimension accessor を維持し、明示 source manifest・module reference map・MSVC 用 `/reference` と既存 `OBJECT_DEPENDS` 条件の path を更新した。ビルド／依存スキャン／リンクは未実行。

- `ArtifactCore/src/Transform/StaticTransform3D.cppm` を `Transform._3D` implementation unit のまま `.cpp` に移した。PImpl・copy/move ownership と method definitions を維持し、明示 source manifest と module reference map の provider path を更新した。source-specific CMake 設定・外部 module import はない。ビルド／依存スキャン／リンクは未実行。

- `ArtifactCore/src/Common/Point2D.cppm` を `Core.Point2D` implementation unit のまま `.cpp` に移した。Point2DF / Point2DI の PImpl 定義と accessors を維持し、`int32_t` 用 `<cstdint>` を追加。source manifest、module reference map と MSVC primary IFC reference 条件を新 path に更新した。ビルド／依存スキャン／リンクは未実行。

- `ArtifactCore/src/Channel/Channel.cppm` を `Channel` implementation unit のまま `.cpp` に移した。VideoChannel / VideoFrame の実装と既存 module dependencies を維持し、直接 `size_t` を使っていた箇所を `std::size_t` と `<cstddef>` に明示した。明示 source manifest、module reference map の provider / imported rows、CMake の generated-reference source 条件を更新した。ビルド／依存スキャン／リンクは未実行。

- `ArtifactCore/src/ImageProcessing/AbstractImageEffect.cppm` を `ImageProcessing` implementation unit のまま `.cpp` に移した。public virtual method 群、partition imports の CMake `/reference` list と implementation を維持し、明示 source manifest・module reference map・個別 source 条件の path を更新した。`size_t` を `std::size_t` にし、`<cstddef>` を直接追加。ビルド／依存スキャン／リンクは未実行。

### Geometry.LayerBounds implementation migration（2026-10-22）

- `ArtifactCore/src/Geometry/LayerBoundsCalculator.cppm` は `Geometry.LayerBounds` implementation unit で、対応する `LayerBoundsCalculator.ixx` に `calculate` / `calculateAABB` / `contains` の宣言が揃っている。module 宣言と実装本文を維持して `.cpp` に移した。
- Core source manifest、module reference map、MSVC generated-reference source 条件、および LayerBounds / LayerAlignment standalone target の source path を更新した。既存 include 群と geometry behavior は変更していない。
- ビルド、依存スキャン、テスト、CMake は実行していない。

### Graphics.GPUAlgorithms implementation migration（2026-10-22）

- `ArtifactCore/src/Graphics/GPUAlgorithms.cppm` は独立した `Graphics.GPUAlgorithms` implementation unit で、`GPUAlgorithms.ixx` の `prefixSum` / `compaction` / `radixSort` と実装が対応する。module 宣言、関数本体、algorithm / limits / DLL export headers を維持して `.cpp` に移した。
- Core source manifest と module reference map の provider path を更新した。個別 `/reference` 条件や standalone CMake source 参照は存在しないことを確認した。挙動変更はない。
- ビルド、依存スキャン、テスト、CMake は実行していない。

### Color.Harmonizer implementation migration（2026-10-22）

- `ArtifactCore/src/Color/ColorHarmonizer.cppm` は `Color.Harmonizer` implementation unit で、公開 interface の6つの調和色 API に対応する定義を持つ。module 宣言、Color.Float / Color.Conversion imports、QList / cmath headers と既存の色計算を維持して `.cpp` に移した。
- Core source manifest、provider / imported module reference rows、MSVC source-specific IFC reference 条件、ColorSpaceStandalone の source path、および Qt header audit script の対象 path を更新した。色計算の挙動変更はない。
- ビルド、依存スキャン、テスト、CMake は実行していない。

### Composition.Registry implementation migration（2026-10-22）

- `ArtifactCore/src/Composition/CompositionRegistry.cppm` は `Composition.Registry` implementation unit で、公開 interface の global / register / unregister / lookup / names / clear API を実装する。module 宣言、NameMap / Container.Debug imports、Qt headers と既存 registry behavior を保って `.cpp` に移した。
- 専用 composition target の source path、Core source manifest と依存スキャンに関する CMake コメントを更新した。実装本文は変更していない。
- ビルド、依存スキャン、テスト、CMake は実行していない。

### Math.Bezier implementation migration（2026-10-22）

- `ArtifactCore/src/Geometry/BezierCalculator.cppm` は `Math.Bezier` implementation unit で、evaluateQuadratic / evaluateCubic / evaluatePath / getTangentAngle / evaluateTangent の定義が interface と一致する。module 宣言、Qt headers、標準 headers、実装本文を維持して `.cpp` に移した。
- Core source manifest、provider module reference、MSVC source-specific IFC 条件、BezierPathSamplerStandalone の source path を更新した。幾何計算の挙動変更はない。
- ビルド、依存スキャン、テスト、CMake は実行していない。

### Color.Swatch implementation migration（2026-10-22）

- `ArtifactCore/src/Color/ColorSwatch.cppm` は `Color.Swatch` implementation unit で、公開 interface の palette access / copy-move / GPL import-export API を実装する。module 宣言、Color.Float / ArtifactString / TrackedPtr imports、既存 implementation と file I/O を保って `.cpp` に移した。
- Core source manifest、provider / imported module reference rows、2つの MSVC source-specific IFC / OBJECT_DEPENDS 条件、ColorSwatchStandalone の source path を更新した。palette behavior は変更していない。
- ビルド、依存スキャン、テスト、CMake は実行していない。

### Frame.Rate implementation migration（2026-10-22）

- `ArtifactCore/src/Frame/FrameRate.cppm` は `Frame.Rate` implementation unit で、FrameRate の construction / copy-move / rational FPS / display / JSON serialization / operators を公開宣言に対応して実装する。module 宣言、Serialization / UniString imports、直接 Qt / standard headers と本文を維持して `.cpp` に移した。
- Core source manifest、provider / imported module reference rows、2つの MSVC IFC / OBJECT_DEPENDS 条件、FrameRateStandalone の source path を更新した。FPS・serialization behavior は変更していない。
- ビルド、依存スキャン、テスト、CMake は実行していない。

### Graphics.Effect.Creative.Manager implementation migration（2026-10-22）

- `ArtifactCore/src/Graphics/Effect/CreativeEffectManager.cppm` は `Graphics.Effect.Creative.Manager` implementation unit で、constructor / destructor、add / remove、effect stack の適用定義を持つ。module 宣言、既存 imports / headers / implementation を維持して `.cpp` に移した。
- Core source manifest と provider module reference の path を更新した。個別 IFC 条件・standalone source path は見つからなかった。render/effect behavior は変更していない。
- ビルド、依存スキャン、テスト、CMake は実行していない。

### Property.LinkManager implementation migration（2026-10-22）

- `ArtifactCore/src/Property/PropertyLinkManager.cppm` は `Property.LinkManager` implementation unit で、instance / addLink / updateTargets / links / clear の公開 API 定義が揃っている。module 宣言、既存実装を維持して `.cpp` に移した。
- Core source manifest、provider map、MSVC provider IFC 条件を更新した。実装が直接使う `QMetaType` を global module fragment に明示 include した。プロパティ更新動作は変更していない。
- ビルド、依存スキャン、テスト、CMake は実行していない。

### Script.AngelScript.Behaviour implementation migration（2026-10-22）

- `ArtifactCore/src/Script/AngelScript/AngelScriptBehaviour.cppm` は `Script.AngelScript.Behaviour` implementation unit で、public interface の load / start / update / destroy API の定義が揃っている。module 宣言、直接 `<string>` include、AngelScript lifecycle 実装を保って `.cpp` に移した。
- Core source manifest、provider module reference と MSVC IFC 条件の source path を更新した。script lifecycle behavior は変更していない。
- ビルド、依存スキャン、テスト、CMake は実行していない。

### Configuration.ConfigSchema implementation migration（2026-10-22）

- `ArtifactCore/src/Configuration/ConfigSchema.cppm` は `Configuration.ConfigSchema` implementation unit で、公開 API の schema registration / validation / lookup / defaults 定義が揃っている。module 宣言、既存 imports / headers、mutex と validation 実装を保って `.cpp` に移した。
- Core source manifest、configuration split target source list、Container debug IFC options、provider / imported module reference rows を更新した。設定検証動作は変更していない。
- ビルド、依存スキャン、テスト、CMake は実行していない。

### Configuration.LayeredConfigStore implementation migration（2026-10-22）

- `ArtifactCore/src/Configuration/LayeredConfigStore.cppm` は `Configuration.LayeredConfigStore` implementation unit で、層の load / save / resolve / import-export / reload API を実装する。module 宣言、既存 imports / Qt headers、FastSettingsStore integration と本文を維持して `.cpp` に移した。
- Core source manifest、configuration split target の implementation list、provider / imported module reference rows を更新した。起動設定仕様と configuration milestone の実装パスも現行名へ直し、両文書の最終更新日を記録した。
- ビルド、依存スキャン、テスト、CMake は実行していない。

### Script.AngelScript.Engine implementation migration（2026-10-22）

- `ArtifactCore/src/Script/AngelScript/AngelScriptEngine.cppm` は `Script.AngelScript.Engine` implementation unit で、singleton lifecycle / SDK initialization / module compile / function execution API を実装する。module 宣言、条件付き AngelScript SDK includes と実装本文を保って `.cpp` に移した。
- Core source manifest と provider module reference を更新した。SDK headers は既存どおり global module fragment 内にあり、macro / script behavior は変更していない。
- ビルド、依存スキャン、テスト、CMake は実行していない。

### Core.FastSettingsStore implementation migration（2026-10-22）

- `ArtifactCore/src/Core/FastSettingsStore.cppm` は `Core.FastSettingsStore` implementation unit で、CBOR file load / atomic save / cache / batch API を実装する。module 宣言、Qt / mutex headers と実装本文を維持して `.cpp` に移した。
- Core source manifest、MSVC provider / std IFC source condition、module reference、FastSettingsStoreStandalone の source path を更新した。CBOR persistence behavior は変更していない。
- ビルド、依存スキャン、テスト、CMake は実行していない。


### UI.Layout.State implementation migration（2026-10-22）

- `ArtifactCore/src/UI/LayoutState.cppm` は `UI.Layout.State` implementation unit で、JSON / QSettings / FastSettingsStore serialization API と schema registration の定義が公開 interface に対応している。module 宣言、imports、headers と実装本文を維持し `.cpp` に移した。
- ArtifactCore source manifest、UI split target source path、MSVC IFC / OBJECT_DEPENDS 条件、provider と Serialization import の module reference rows を更新した。Clang frontend の最終設定では `.cpp` implementation scan と module map に移行する。保存・migration behavior は変更していない。
- ビルド、依存スキャン、テスト、CMake は実行していない。


### File.TypeDetector implementation migration（2026-10-22）

- `ArtifactCore/src/File/FileTypeDetector.cppm` は `File.TypeDetector` implementation unit で、公開 interface の constructor / destructor / detect API と拡張子・magic 判定の定義が揃っている。直接使う Qt Core headers、module 宣言と実装本文を保って `.cpp` に移した。
- ArtifactCore の File split target source list、明示 source manifest、provider module reference を更新した。Clang lane は `.cpp` を P1689 scan / module map に載せ、MSVC は既存の明示 IFC reference と interface object ordering を使う。file type 判定動作は変更していない。
- ビルド、依存スキャン、テスト、CMake は実行していない。


### Asset.File implementation migration（2026-10-22）

- `ArtifactCore/src/Asset/AbstractAssetFile.cppm` は `Asset.File` implementation unit で、公開 interface の PImpl lifecycle、asset identity / metadata / dependency / load-state API を実装する。`W_OBJECT_IMPL`、既存 module imports、Qt / Verdigris headers と実装本文を維持して `.cpp` に移した。
- Asset split target source list、明示 source manifest、MSVC の NamedVector / Container.Debug IFC options、provider / imported module reference rows を新 path へ更新した。Clang frontend は `.cpp` を module map scan に載せ、既存の MSVC 専用参照／object ordering を除去する。asset behavior は変更していない。
- ビルド、依存スキャン、テスト、CMake は実行していない。


### Coordinate.Profile implementation migration（2026-10-22）

- `ArtifactCore/src/Coordinate/CoordinateProfile.cppm` は `Coordinate.Profile` implementation unit で、value / space / profile / resolver の公開 API と JSON 登録・schema migration を実装する。module 宣言、Qt / standard headers、Serialization imports と実装本文を保って `.cpp` に移した。
- Coordinate split target source list、明示 source manifest、Serialization IFC options、provider / imported module reference rows を新 path に更新した。Clang frontend は `.cpp` を P1689 scan / module map に載せる。座標変換・serialization behavior は変更していない。
- ビルド、依存スキャン、テスト、CMake は実行していない。


### Color.LUTWriter implementation migration（2026-10-22）

- `ArtifactCore/src/Color/LUTWriter.cppm` は `Color.LUTWriter` implementation unit で、Cube / 3dl memory export と file write API の定義が公開 interface に揃っている。`Color.LUT` import、Qt Core headers と出力実装を保って `.cpp` に移した。
- Color source manifest、既存の MSVC module reference source condition、provider / `Color.LUT` imported reference rows を新 path に更新した。Clang frontend は `.cpp` を P1689 scan / module map に載せる。LUT のフォーマット・出力 behavior は変更していない。
- ビルド、依存スキャン、テスト、CMake は実行していない。


### ArtifactCore stale `.cppm` source conditions repair（2026-10-22）

- CMake source-specific IFC / compile-option branches still compared `_artifact_impl_relative` against `.cppm` for 14 files that are already registered as `.cpp`: AnimationDynamics, ColorLuminance, FloatRGBA, XYZColor, FrameSkipTracker, GPUInfo, GPUTextureCacheManager, CompressQuality, RenderStatics, ExplorerUtils, NameGenerator, Path, Tag, and MediaTimeStamp.
- Each corresponding `.cpp` exists in the source manifest and has a provider row in the module reference map. Updated only the stale CMake conditions so the intended MSVC IFC / ordering settings apply to the actual source path. Clang continues to use its P1689 module map lane.
- ビルド、依存スキャン、テスト、CMake は実行していない。


### Core.AspectRatio implementation migration（2026-10-22）

- `ArtifactCore/src/Core/AspectRatio.cppm` は `Core.AspectRatio` implementation unit で、公開 interface の constructor / copy-move / ratio / string parse API を実装する。module 宣言、標準 / Qt headers、UniString import と実装本文を保って `.cpp` に移した。
- Core source manifest、既存の MSVC module reference source condition、provider / UniString imported reference rows を新 path に更新した。decimal parsing のロジックと PImpl ownership は変更していない。
- ビルド、依存スキャン、テスト、CMake は実行していない。


### Command.LayerLockManager implementation migration（2026-10-22）

- `ArtifactCore/src/Command/LayerLockManager.cppm` は `Command.LayerLockManager` implementation unit で、constructor / destructor、lock acquire / release、remote state、expiry purge API の定義が揃っている。`Event.Bus` import、`W_OBJECT_IMPL`、既存実装を保って `.cpp` に移した。
- source manifest、Command split target source list、Event.Bus / Memory.SharedPtr の MSVC IFC options、provider module reference を新 path に更新した。実装で直接使う Qt Core の `QList` / `QObject` / `QString` を global module fragment に明示 include した。
- ビルド、依存スキャン、テスト、CMake は実行していない。


### Command.Lambda implementation migration（2026-10-22）

- `ArtifactCore/src/Command/LambdaCommand.cppm` は `Command.Lambda` implementation unit で、公開 interface の constructor / destructor / undo / redo 定義が揃っている。module 宣言と command behavior を維持して `.cpp` に移した。
- Command split target source list、明示 source manifest、provider module reference を新 path に更新した。未使用の `<QUndoStack>` を実際に使う `<QUndoCommand>` へ置き換え、`std::move` 用の `<utility>` を直接 include した。
- ビルド、依存スキャン、テスト、CMake は実行していない。


### Asset.Importer implementation migration（2026-10-22）

- `ArtifactCore/src/Asset/AssetImporter.cppm` は `Asset.Importer` implementation unit で、公開 interface の import / supported-extension / type-detection API と asset metadata 登録処理を実装する。module 宣言、imports、Qt headers と importer behavior を維持して `.cpp` に移した。
- Core source manifest、明示 source registration、provider / imported module reference rows を新 path に更新した。Clang lane は `.cpp` implementation scan / P1689 module map を使い、MSVC は既存の explicit IFC references を継続する。
- ビルド、依存スキャン、テスト、CMake は実行していない。


### Layer.LayerStrip implementation migration（2026-10-22）

- `ArtifactCore/src/Layer/LayerStrip.cppm` は `Layer.LayerStrip` implementation unit で、public interface の PImpl lifecycle と frame range API の定義が揃っている。module 宣言、`<cstdint>` / `<utility>` headers、実装を保って `.cpp` に移した。
- ArtifactCore source manifest と provider module reference を新 path に更新した。専用 split target / source-specific CMake 条件に旧 path はなく、追加変更は不要だった。
- ビルド、依存スキャン、テスト、CMake は実行していない。


### Script.Builtin.Manager implementation migration（2026-10-22）

- `ArtifactCore/src/Script/Engine/Func/BuiltinManager.cppm` は `Script.Builtin.Manager` implementation unit で、公開 interface の register / remove / call / list API を実装する。module 宣言、既存 headers と関数呼び出し・lookup behavior を保って `.cpp` に移した。
- ArtifactCore source manifest と provider module reference を新 path に更新した。専用 split target / source-specific CMake 条件に旧 path はなく、Clang の `.cpp` implementation scan / P1689 module map に載る。
- ビルド、依存スキャン、テスト、CMake は実行していない。


### Script.Enviroment implementation migration（2026-10-22）

- `ArtifactCore/src/Script/Engine/Enviroment/EnvironmentManager.cppm` は `Script.Enviroment` implementation unit で、公開 interface の constructor / destructor 定義を実装する。module 宣言、`Script.Engine.Value` import と既存 implementation を保って `.cpp` に移した。
- ArtifactCore source manifest と provider module reference、CMake の MSVC IFC reference / interface object dependency の両 source-specific 条件を新 path に更新した。Clang lane は `.cpp` implementation scan / P1689 module map を使う。
- ビルド、依存スキャン、テスト、CMake は実行していない。


### Core.Diagnostics.FallbackPolicy implementation migration（2026-10-22）

- `ArtifactCore/src/Diagnostics/FallbackPolicy.cppm` は `Core.Diagnostics.FallbackPolicy` implementation unit で、fallback tracker の記録・検索・policy API を実装する。module 宣言、`Container.NamedVector` import、既存実装を保って `.cpp` に移した。
- ArtifactCore source manifest と provider / imported module reference、CMake の MSVC IFC option と container registry object dependency の source-specific 条件を新 path に更新した。fallback event behavior は変更していない。
- ビルド、依存スキャン、テスト、CMake は実行していない。


### Core.Diagnostics.DiagnosticEngine implementation migration（2026-10-22）

- `ArtifactCore/src/Diagnostics/DiagnosticEngine.cppm` は `Core.Diagnostics.DiagnosticEngine` implementation unit で、rule registry と validate-all / delta / cache API を実装する。module 宣言と validation behavior を保って `.cpp` に移し、`std::ptrdiff_t` / `QStringList` の直接利用に `<cstddef>` / `<QStringList>` を追加した。
- ArtifactCore source manifest、provider / `Core.Diagnostics.ProjectDiagnostic` reference、MSVC IFC source condition と `DiagnosticEngineStandalone` の source path を更新した。診断キャッシュ・検証動作は変更していない。
- ビルド、依存スキャン、テスト、CMake は実行していない。


### Light.IESProfile implementation migration（2026-10-22）

- `ArtifactCore/src/Light/IESProfile.cppm` は `Light.IESProfile` implementation unit で、IES parse / file load、candela sampling、LUT 構築 API を実装する。module 宣言、imports、既存実装を保って `.cpp` に移した。
- source manifest、provider module reference、`ArtifactCoreLight` split source と MSVC IFC / object dependency の各 path を更新した。parse / sample / LUT behavior は変更していない。
- ビルド、依存スキャン、テスト、CMake は実行していない。


### Preview.Settings implementation migration（2026-10-22）

- `ArtifactCore/src/Preview/PreviewSettings.cppm` は `Preview.Settings` implementation unit で、preview settings の PImpl lifecycle、getter / setter、reset API を定義する。module 宣言と設定値の挙動を保って `.cpp` に移した。
- ArtifactCore source manifest、provider module reference、Preview split target source list の path を更新した。preview 設定 API は変更していない。
- ビルド、依存スキャン、テスト、CMake は実行していない。


### Core.Database implementation migration（2026-10-22）

- `ArtifactCore/src/Database/Database.cppm` は `Core.Database` implementation unit で、Qt SQL connection / prepared query、SQLite transaction、migration runner の API を実装する。module 宣言と database behavior を保って `.cpp` に移し、直接利用する Qt / standard headers を明示した。
- ArtifactCore source manifest、provider / `Core.Database.Types` references、`ArtifactCoreDatabase` target source と MSVC IFC / object dependency 条件を新 path に更新した。
- ビルド、依存スキャン、テスト、CMake は実行していない。


### ArtifactCore.Plugin.Registry implementation migration（2026-10-22）

- `ArtifactCore/src/Plugin/PluginRegistry.cppm` は `ArtifactCore.Plugin.Registry` implementation unit で、plugin register / activate / lookup / enumeration API を実装する。module 宣言、imports と registry behavior を保って `.cpp` に移した。
- ArtifactCore source manifest、Plugin split target、initial scan-off source list、MSVC の IFC / interface-object dependencies と NamedVector option list の path をすべて更新した。Clang lane は implementation source を再度 scan 対象にし、P1689 依存を生成する。
- ビルド、依存スキャン、テスト、CMake は実行していない。


### Core.AI.TieredAIManager implementation migration（2026-10-22）

- `ArtifactCore/src/AI/TieredAIManager.cppm` は `Core.AI.TieredAIManager` implementation unit で、agent configuration、request routing、cloud escalation、background analysis API を実装する。module 宣言、imports、既存 PImpl と request behavior を保って `.cpp` に移した。
- ArtifactCore source manifest、imported module references、AI split target、initial scan-off / MSVC IFC / interface-object / NamedVector source conditions と AI feature map path を更新した。Clang lane は migrated `.cpp` を implementation source collection から P1689 scan に載せる。
- ビルド、依存スキャン、テスト、CMake は実行していない。


### Core.AI.APIKeyManager implementation migration（2026-10-22）

- `ArtifactCore/src/AI/APIKeyManager.cppm` は `Core.AI.APIKeyManager` implementation unit で、provider key / proxy の設定 load / save / lookup API を実装する。module 宣言、imports と persistence behavior を保って `.cpp` に移した。
- ArtifactCore source manifest と provider / imported module references、AI split target、MSVC explicit-reference exception と configuration interface object dependency の source path を更新した。Clang lane では implementation source collection を通じて P1689 scan が有効になる。
- ビルド、依存スキャン、テスト、CMake は実行していない。


### Property.Group implementation migration（2026-10-22）

- `ArtifactCore/src/Property/PropertyGroup.cppm` は `Property.Group` implementation unit で、public interface の PImpl lifecycle、property add / remove / lookup / sort API を実装する。module 宣言と実装を保って `.cpp` に移した。
- ArtifactCore source manifest、provider module reference と MSVC の source-specific IFC reference path を更新した。PImpl ownership、container と PropertyGroup behavior は変更していない。
- ビルド、依存スキャン、テスト、CMake は実行していない。


### Core.Diagnostics.ProjectDiagnostic implementation migration（2026-10-22）

- `ArtifactCore/src/Diagnostics/ProjectDiagnostic.cppm` は `Core.Diagnostics.ProjectDiagnostic` implementation unit で、diagnostic factory と result の集計・検索・merge API を実装する。診断内容・カテゴリ・severity の挙動を保って `.cpp` に移した。
- ArtifactCore source manifest、provider / `Container.Debug.Registry` module references、MSVC の IFC reference / NamedVector interface object dependency と `DiagnosticEngineStandalone` source path を新 path に更新した。
- ビルド、依存スキャン、テスト、CMake は実行していない。


### Time.Rational implementation migration（2026-10-22）

- `ArtifactCore/src/Time/RationalTime.cppm` は `Time.Rational` implementation unit で、有理時間値の算術・比較・scale 変換 API を実装する。既存の overflow fallback と丸め挙動を含め、ソース内容を保って `.cpp` に移した。
- ArtifactCore source manifest と provider module reference を新 path に更新した。
- ビルド、依存スキャン、テスト、CMake は実行していない。


### Transform._2D implementation migration（2026-10-22）

- `ArtifactCore/src/Transform/StaticTransform2D.cppm` は `Transform._2D` implementation unit で、PImpl の lifecycle、copy / move、transform 値の getter / setter を実装する。公開 interface と所有権挙動を保ったまま `.cpp` に移した。
- ArtifactCore source manifest と provider module reference を新 path に更新した。
- ビルド、依存スキャン、テスト、CMake は実行していない。


### asio_async_file_writer implementation migration（2026-10-22）

- `ArtifactCore/src/IO/asio_async_file_writer.cppm` は `asio_async_file_writer` implementation unit で、Boost.Asio thread pool 経由の非同期ファイル書き込み API を実装する。既存の worker / callback / atomic shutdown 挙動を含め、ソース内容を保って `.cpp` に移した。
- ArtifactCore source manifest と provider module reference を新 path に更新した。
- ビルド、依存スキャン、テスト、CMake は実行していない。


### Core.Position2D implementation migration（2026-10-22）

- `ArtifactCore/src/Core/Position2D.cppm` は `Core.Position2D` implementation unit で、`Position2D` / `Position2DF` の PImpl lifecycle、copy / move と座標 getter / setter を実装する。公開 API と実装内容を保って `.cpp` に移した。
- ArtifactCore source manifest、MSVC の source-specific IFC reference、provider module reference と `Position2DStandalone` source path を新 path に更新した。
- ビルド、依存スキャン、テスト、CMake は実行していない。


### Media.Info implementation migration（2026-10-22）

- `ArtifactCore/src/Media/MediaInfo.cppm` は `Media.Info` implementation unit で、MediaInfo / MediaInfoBuilder の PImpl と metadata API 定義を実装する。公開 API と既存実装をそのまま `.cpp` に移した。
- ArtifactCore source manifest、Media split target source list と provider module reference を新 path に更新した。
- ビルド、依存スキャン、テスト、CMake は実行していない。


### Core.UniformScale implementation migration（2026-10-22）

- `ArtifactCore/src/Core/UniformScale.cppm` は `Core.UniformScale` implementation unit で、scale 値の PImpl lifecycle と getter を実装する。公開 API と実装内容を保って `.cpp` に移した。
- source manifest、provider module reference、MSVC の source-specific IFC reference と `UniformScaleStandalone` source path を新 path に更新した。
- ビルド、依存スキャン、テスト、CMake は実行していない。


### Diagnostics.CrashHandler implementation migration（2026-10-22）

- `ArtifactCore/src/Diagnostics/CrashHandler.cppm` は `Diagnostics.CrashHandler` implementation unit で、Windows unhandled exception filter、report path / content 生成と pending report ingestion を実装する。Windows API、report format、handler behavior を保って `.cpp` に移した。
- ArtifactCore source manifest、provider module reference と MSVC の source-specific IFC reference を新 path に更新した。
- ビルド、依存スキャン、テスト、CMake は実行していない。


### Core.Localization implementation migration（2026-10-22）

- `ArtifactCore/src/Localization/Localization.cppm` は `Core.Localization` implementation unit で、locale selection、translation load / fallback、plural formatting と reload API を実装する。既存の言語決定・翻訳・Event.Bus 通知動作を保って `.cpp` に移した。
- ArtifactCore source manifest、Localization split target source list / source property と provider module reference を新 path に更新した。
- ビルド、依存スキャン、テスト、CMake は実行していない。


### NLE.OTIO implementation migration（2026-10-22）

- `ArtifactCore/src/NLE/OTIO.cppm` は `NLE.OTIO` implementation unit で、OpenTimelineIO JSON の track / clip / transition / marker 読み書きを実装する。schema mapping と既存 import / export behavior を保って `.cpp` に移した。
- ArtifactCore source manifest、NLE split target、MSVC IFC options / interface object dependencies と provider module reference を新 path に更新した。
- ビルド、依存スキャン、テスト、CMake は実行していない。


### VST3.Interfaces implementation migration（2026-10-22）

- `ArtifactCore/src/VST3/VST3Loader.cppm` は `VST3.Interfaces` implementation unit で、VST3 TUID conversion、dynamic library entry loading と host / editor ABI adapter を実装する。既存 ABI / loader behavior を保って `.cpp` に移した。
- ArtifactCore source manifest、VST3 source list、source compile property、provider map と CMake の実装説明コメントを新 path に更新した。
- ビルド、依存スキャン、テスト、CMake は実行していない。


### Core.Simulation.OpenVDBVolumeReference implementation migration（2026-10-22）

- `ArtifactCore/src/Simulation/OpenVDBVolumeReference.cppm` は volume metadata inspect、density grid snapshot load と Pyro field copy API を実装する。既存処理を `.cpp` に移し、`std::size_t` 使用に必要な `<cstddef>` を直接 include した。
- ArtifactCore source manifest、split target source list、MSVC の source-specific IFC references と provider module reference を新 path に更新した。
- ビルド、依存スキャン、テスト、CMake は実行していない。


### Thread.Helper implementation migration（2026-10-22）

- `ArtifactCore/src/Thread/ThreadHelper.cppm` は `Thread.Helper` implementation unit で、shared background QThreadPool、thread snapshot / diagnostics と scoped thread naming API を実装する。pool 設定、同期、diagnostic behavior を変更せず `.cpp` に移した。
- ArtifactCore source manifest、Thread split target、MSVC IFC reference / interface object dependency と provider module reference を新 path に更新した。
- ビルド、依存スキャン、テスト、CMake は実行していない。


### UI.ShortcutBindings implementation migration（2026-10-22）

- `ArtifactCore/src/UI/ShortcutBindings.cppm` は `UI.ShortcutBindings` implementation unit で、shortcut ID serialization、key-event conversion と binding override API を実装する。既存の key mapping / override behavior を保って `.cpp` に移した。
- 実装で直接使う `std::array` / `std::size_t` に `<array>` / `<cstddef>` を追加した。CMake source path、MSVC IFC reference / interface object dependency、manifest と provider map を更新し、この source の `CXX_SCAN_FOR_MODULES OFF` を解除して Clang implementation dependency scan を有効にした。
- ビルド、依存スキャン、テスト、CMake は実行していない。


### Asset.Manager implementation migration（2026-10-22）

- `ArtifactCore/src/Asset/AssetManager.cppm` は `Asset.Manager` implementation unit で、source reference count、decoded payload cache、version invalidation と registry snapshot restore を実装する。既存の asset lifecycle behavior を保って `.cpp` に移した。
- Clang の直接 include を明確にするため `<QString>`、`<QUuid>`、`<cstdint>` を追加し、未使用の `QFileSystemWatcher` include を除去した。ArtifactCore source manifest、Asset split target、provider / imported module references と追跡済み Visual Studio backup project の source paths を新 path に更新した。
- ビルド、依存スキャン、テスト、CMake は実行していない。


### Source.ISource implementation migration（2026-10-22）

- `ArtifactCore/src/Source/ISource.cppm` は file / generated source の metadata、read / reload / relink と source capability API を実装する。既存 source cache behavior を保って `.cpp` に移した。
- 直接利用する Qt 型（`QByteArray` / `QDateTime` / `QSize` / `QString` / `qint64`）の include を明示し、ArtifactCore source manifest、provider / `Memory.SharedPtr` / `Utils.Id` references、MSVC IFC references を新 path に更新した。
- ビルド、依存スキャン、テスト、CMake は実行していない。


### Frame.Position implementation migration（2026-10-22）

- `ArtifactCore/src/Frame/FramePosition.cppm` は frame index と rational time 間の値変換を実装する。公開 `Frame.Position` interface と演算を保って `.cpp` に移した。
- source manifest、provider mapping、MSVC source-specific IFC reference 条件、および追跡済み Visual Studio backup project の source paths を更新した。実装本文は改行を LF に揃えた以外は同一。
- ビルド、依存スキャン、テスト、CMake は実行していない。


### Geometry.ResolutionRemap implementation migration（2026-10-22）

- `ArtifactCore/src/Geometry/ResolutionRemap.cppm` は解像度変更時の位置・bounds・scale・transform remap と影響情報を計算する。既存挙動を保ち `.cpp` に移した。
- source manifest、provider mapping と MSVC source-specific IFC reference 条件を更新した。Qt / 標準ヘッダーの直接 include は既に揃っており追加変更は不要。実装本文は LF 化以外同一。
- Clang の `.cpp` module implementation scan 適用対象に含まれる。ビルド、依存スキャン、テスト、CMake は実行していない。


### GenerateTestImage implementation migration（2026-10-22）

- `ArtifactCore/src/Generate/GenerateTestImage.cppm` はカラーバー、グラデーション、チェッカーボード等を呼び出し時に RGBA float buffer へ生成する utility である。描画 frame loop からの参照は見つからず、既存挙動を保って `.cpp` に移した。
- ArtifactCore の core / Generate source lists、provider mapping と `Core.Parallel` IFC reference の source path を更新した。実装本文は LF 化以外同一。
- Clang の `.cpp` module implementation scan 適用対象に含まれる。ビルド、依存スキャン、テスト、CMake は実行していない。


### Geometry.LayerAlignment implementation migration（2026-10-22）

- `ArtifactCore/src/Geometry/LayerAlignment.cppm` はユーザー操作による layer align / distribute と layout collision resolution を実装する。frame loop からの参照は見つからず、既存挙動を保って `.cpp` に移した。
- source manifest、provider mapping と MSVC source-specific IFC reference 条件を新 path に更新した。直接利用する Qt / 標準ヘッダーは既に global module fragment に含まれており追加変更は不要。本文は LF 化以外同一。
- Clang の `.cpp` module implementation scan 適用対象に含まれる。ビルド、依存スキャン、テスト、CMake は実行していない。


### Color.OCIOConfig implementation migration（2026-10-22）

- `ArtifactCore/src/Color/OCIOConfig.cppm` は OpenColorIO 設定の file / JSON load、組み込み preset と color space / display metadata を実装する。color conversion の per-pixel 経路ではなく、既存処理を保って `.cpp` に移した。
- source manifest、provider mapping と MSVC source-specific IFC reference 条件を新 path に更新した。Qt / OpenColorIO の直接 include は既に global module fragment に揃っている。本文は LF 化以外同一。
- Clang の `.cpp` module implementation scan 適用対象に含まれる。ビルド、依存スキャン、テスト、CMake は実行していない。


### StarfieldGenerator implementation migration（2026-10-22）

- `ArtifactCore/src/Generate/StarfieldGenerator.cppm` は恒星・星雲・惑星データを RGBA float buffer に生成する公開 utility である。現行 repo 内での呼び出し元は見つからず、既存実装を保って `.cpp` に移した。
- core / Generate source lists、provider mapping と `Core.Parallel` / `Math.Random` の MSVC IFC reference source path を更新した。実装本文は LF 化以外同一。
- Clang の `.cpp` module implementation scan 対象になる。ビルド、依存スキャン、テスト、CMake は実行していない。


### Analyze.Histgram implementation migration（2026-10-22）

- `ArtifactCore/src/Analyze/Histgram.cppm` は OpenCV image から channel histogram を計算する utility である。現行 app/widget に呼び出し元は見つからず、描画 frame path 外の一回計算として既存実装を `.cpp` に移した。
- Analyze target / source manifest、provider mapping、Container IFC reference 条件、および追跡済み Visual Studio backup project の paths を更新した。本文は LF 化以外同一。
- Clang の `.cpp` module implementation scan 対象に含まれる。ビルド、依存スキャン、テスト、CMake は実行していない。


### Animation.KeyframeEditingTools implementation migration（2026-10-22）

- `ArtifactCore/src/Animation/KeyframeEditingTools.cppm` はキー編集操作（thin / easing / scale / smooth / quantize / mirror）と keyframe 変換 utility を実装する。現行 repo 内の呼び出し元は見つからず、既存挙動を保って `.cpp` に移した。
- Audio split target の source list / source 条件 / object-order 条件、source manifest、provider と imported-module mappings を新 path に更新した。実装本文は LF 化以外同一。
- Clang の `.cpp` module implementation scan 対象になる。ビルド、依存スキャン、テスト、CMake は実行していない。


### Export.Lottie.Exporter implementation migration（2026-10-22）

- `ArtifactCore/src/Export/Lottie/LottieExporter.cppm` は Lottie JSON validation / serialization、keyframe compression と file output を実装するユーザー実行の export path である。既存処理を保って `.cpp` に移した。
- Export target source list、scanner exception list、provider map、専用 IFC / object dependency paths を更新した。実装本文は LF 化以外同一。Clang final configuration は migrated `.cpp` の P1689 scan を有効化し、旧 MSVC-only module edges を除去する。
- ビルド、依存スキャン、テスト、CMake は実行していない。

### Composition.TemplateSlot implementation migration（2026-10-22）

- `ArtifactCore/src/Composition/TemplateSlot.cppm` はテンプレート slot / variation / output variant の JSON serialization と schema registration を実装するコールドパスである。module declaration、serialization registration と既存処理を保って `.cpp` に移した。
- Composition source manifest と provider / imported-module mappings を新 path に更新した。Clang の `.cpp` implementation scan が有効になる。実装本文は LF 化以外同一。
- ビルド、依存スキャン、テスト、CMake は実行していない。

### Composition.TemplateLock implementation migration（2026-10-22）

- `ArtifactCore/src/Composition/TemplateLock.cppm` は protected region / editable field / template lock schema の JSON serialization と schema registration を実装するコールドパスである。module declaration、serialization registration と既存処理を保って `.cpp` に移した。
- Composition source manifest と provider / imported-module mappings を新 path に更新した。Clang の `.cpp` implementation scan が有効になる。実装本文は LF 化以外同一。
- ビルド、依存スキャン、テスト、CMake は実行していない。

### Composition.ExportMatrix implementation migration（2026-10-22）

- `ArtifactCore/src/Composition/ExportMatrix.cppm` は export preset / variant / cell rule の JSON serialization と export cell 設定の解決を行う。ユーザー操作による export setup path であり、画像 frame rendering には入らない。既存 registration と処理を保って `.cpp` に移した。
- Composition source manifest と provider / imported-module mappings を新 path に更新した。Clang の `.cpp` implementation scan が有効になる。実装本文は LF 化以外同一。
- ビルド、依存スキャン、テスト、CMake は実行していない。

### Composition.PreCompose implementation migration（2026-10-22）

- `ArtifactCore/src/Composition/PreCompose.cppm` はユーザー操作による precompose / unprecompose、nesting map と project snapshot restore を実装する。常時描画 frame path からの呼び出しは確認されず、既存処理を `.cpp` に移した。
- Composition source manifest、module provider / imported-module mappings、専用 MSVC `/reference` source path を新 path に更新した。移行後の `.cpp` は Clang P1689 scan 対象となり、Clang lane では旧手動 module edge が除去される。実装本文は LF 化以外同一。
- ビルド、依存スキャン、テスト、CMake は実行していない。

### IO.ImageImporter implementation migration（2026-10-22）

- `ArtifactCore/src/IO/Image/ImageImporter.cppm` は PSD/PSB と OpenImageIO / stb 経由の画像 file open / decode を実装する。呼び出しは明示的な asset metadata refresh / image read 時で、毎 frame の描画経路ではない。decode behavior を保って `.cpp` に移した。
- source manifest、module provider map、MSVC source-specific `/reference` と PSD interface object dependency の path を更新した。Clang lane では migrated `.cpp` の P1689 scan が module dependencies を扱い、旧手動 edge を除去する。実装本文は LF 化以外同一。
- ビルド、依存スキャン、テスト、CMake は実行していない。

### Export.Lottie.RigExporter implementation migration（2026-10-22）

- `ArtifactCore/src/Export/Lottie/LottieRigExporter.cppm` はユーザー実行の Lottie rig export 時に frame ごとの rig transform をサンプリングし keyframe を構成する。常時 playback / render frame path からは呼ばれず、既存処理を `.cpp` に移した。
- Export source list、scanner exception list、provider / imported-module mappings、専用 IFC / object dependency paths を更新した。Clang の migrated `.cpp` scan で module dependencies を扱う。実装本文は LF 化以外同一。
- ビルド、依存スキャン、テスト、CMake は実行していない。

### Core.AI.CloudAgent implementation migration（2026-10-22）

- `ArtifactCore/src/AI/CloudAgent.cppm` は AI 機能から呼ばれる cloud provider request / response 処理と agent factory を実装する。ユーザー操作時の network path であり、描画 frame path には入らない。既存処理を `.cpp` に移した。
- source manifest、provider / imported-module mappings と専用 MSVC `/reference` source selector を新 path に更新した。MSVC の明示 references は維持し、Clang lane では `.cpp` P1689 scan を使う。`/TP` は Clang finalization 時に除去される。実装本文は LF 化以外同一。
- ビルド、依存スキャン、テスト、CMake は実行していない。

### Network.CollaborationWebSocket implementation migration（2026-10-22）

- `ArtifactCore/src/Network/CollaborationWebSocket.cppm` は協調編集 WebSocket の接続状態、heartbeat / reconnect、operation / lock / presence message の送受信を実装する。利用は collaboration session の通信時で、描画 frame path には入らない。既存処理（元ファイルは LF）をそのまま `.cpp` に移した。
- source manifest、provider map と Network target source list を更新した。追加の import mapping や source-specific MSVC dependency edge はないため、Clang の `.cpp` P1689 scan に依存解決を任せる。
- ビルド、依存スキャン、テスト、CMake は実行していない。

### Core.AI.LlamaAgent implementation migration（2026-10-22）

- `ArtifactCore/src/AI/LlamaLocalAgent.cppm` はユーザー操作によるローカル LLM backend 初期化、model load / inference、streaming response を実装する。推論時に計算負荷はあるが playback/render frame path ではない。既存処理を `.cpp` に移した。
- AI source list、provider / imported-module mappings と source-specific MSVC `/reference` selector を新 path に更新した。Clang lane では `.cpp` P1689 scan が有効になる。実装本文は LF 化以外同一。
- ビルド、依存スキャン、テスト、CMake は実行していない。

### Core.AI.ObjectDetector implementation migration（2026-10-22）

- `ArtifactCore/src/AI/ObjectDetector.cppm` は AI/tool から明示的に呼ばれる detection metadata、bounding box drawing と mask rasterization を実装する。現行 repo 内に playback/render frame path からの呼び出しはない。既存処理を `.cpp` に移した。
- AI source list、provider / imported-module mappings、MSVC `/reference` / object dependencies、scanner exception と NamedVector compile option selector を新 path に更新した。Clang finalization は migrated `.cpp` を P1689 scan 対象にする。実装本文は LF 化以外同一。
- ビルド、依存スキャン、テスト、CMake は実行していない。
### Generator.NoiseImage implementation migration（2026-10-22）

- `ArtifactCore/src/ImageProcessing/NoiseImageGenerator.cppm` を `NoiseImageGenerator.cpp` に移行し、source manifest、module reference map、source-specific CMake dependency 条件を更新した。
- 公開 interface は維持。実装本文は LF 改行へ正規化した以外に変更なし。リポジトリ内に直接の呼び出し元は見つからず、実行頻度は静的検索からは特定できない。
- ArtifactCore 集計は `.cpp` 131、実装形式 `.cppm` 343。
- ビルド・テスト・CMake configure/generate は未実施（指示により）。
### Audio.SimpleWav implementation migration（2026-10-22）

- `ArtifactCore/src/Audio/SimpleWav.cppm` を `SimpleWav.cpp` に移行し、Audio split target、source manifest、module reference map の source path を更新した。公開 `Audio.SimpleWav` interface は維持。
- WAV のファイル読み込み実装で、repo 内の利用箇所は音声レイヤー／Asset Browser 側。実装本文は LF 改行への正規化以外変更なし。
- ArtifactCore 集計は `.cpp` 132、実装形式 `.cppm` 342。
- ビルド・テスト・CMake configure/generate は未実施（指示により）。
### File.InfoImpl partition migration（2026-10-22）

- `ArtifactCore/src/File/FileInfo.cppm` を `FileInfo.cpp` に移行した。`module File:InfoImpl` partition と `import :Info` はそのまま維持し、split File target の source lists、module reference map、partition IFC の source-specific compile option selector を更新した。
- 現在の Core / Artifact ソースから `FileInfo` の呼び出し元は見つからず、実装は PImpl の生成・破棄のみ。実装本文は LF 改行への正規化以外変更なし。
- ArtifactCore 集計は `.cpp` 133、実装形式 `.cppm` 341。
- ビルド・テスト・CMake configure/generate は未実施（指示により）。
### Codec.Encoder.Setting implementation migration（2026-10-22）

- `ArtifactCore/src/Codec/EncoderSetting.cppm` を `EncoderSetting.cpp` に移行した。source manifest、module reference map、source-specific CMake configuration selector を更新し、`Codec.Encoder.Setting` interface は維持。
- エンコード出力設定の保存・取得実装であり、フレーム処理本体ではない。実装本文は LF 改行への正規化以外変更なし。
- ArtifactCore 集計は `.cpp` 134、実装形式 `.cppm` 340。
- ビルド・テスト・CMake configure/generate は未実施（指示により）。
### Color.Lab implementation migration（2026-10-22）

- `ArtifactCore/src/Color/LabColor.cppm` を `LabColor.cpp` に移行し、source manifest、module reference map、source-specific compile option selector を更新した。`Color.Lab` public interface は維持。
- repo 内の利用箇所は `FloatColorPicker` の色変換。描画・合成 path ではない。実装本文は LF 改行への正規化以外変更なし。
- ArtifactCore 集計は `.cpp` 135、実装形式 `.cppm` 339。
- ビルド・テスト・CMake configure/generate は未実施（指示により）。
### Audio.LipSyncTrack implementation migration（2026-10-22）

- `ArtifactCore/src/Audio/LipSyncTrack.cppm` を `LipSyncTrack.cpp` に移行し、Audio split target、source manifest、module reference map を更新した。公開 `Audio.LipSyncTrack` interface は維持。
- 明示操作で音声を解析して口形イベントを生成する経路。`Audio.SimpleWav` を import する既存 `.cppm` importer と移行済み `.cpp` provider の構成を追加した。実装本文は LF 改行への正規化以外変更なし。
- ArtifactCore 集計は `.cpp` 136、実装形式 `.cppm` 338。
- ビルド・テスト・CMake configure/generate は未実施（指示により）。
### Analyze.ImageAnalyzer implementation migration（2026-10-22）

- `ArtifactCore/src/Analyze/ImageAnalyzer.cppm` を `ImageAnalyzer.cpp` に移行し、analyze split target、source manifest、module reference map、source-specific IFC/object dependency 条件を更新した。公開 `Analyze.Histogram` module interface は維持。
- repo 内の外部利用は Color Science panel の pixel sample と spatial spectrum 操作。実装本文は LF 改行への正規化以外変更なし。clang GNU frontend の module scan は既存設定のまま有効。
- ArtifactCore 集計は `.cpp` 137、実装形式 `.cppm` 337。
- ビルド・テスト・CMake configure/generate は未実施（指示により）。
### Codec.FFmpegThumbnailExtractor implementation migration（2026-10-22）

- `ArtifactCore/src/Codec/FFmpegThumbnailExtractor.cppm` を `.cpp` に移行し、Media split target の source list / `CORE_IMPL` 除外、source manifest、module reference map の参照先を更新した。公開 interface は維持。
- Asset Browser / Timeline の要求時に非同期サムネイルを抽出する既存経路。通常 playback frame からの呼び出しではない。実装本文は LF 改行への正規化以外変更なし。
- ArtifactCore 集計は `.cpp` 138、実装形式 `.cppm` 336。
- ビルド・テスト・CMake configure/generate は未実施（指示により）。
### Analyze.SmartPalette implementation migration（2026-10-22）

- `ArtifactCore/src/Analyze/SmartPaletteAnalyzer.cppm` を `.cpp` に移行し、analyze split target、source manifest、module reference map、source-specific module references / `NamedVector` compile options を更新した。公開 `Analyze.SmartPalette` interface を維持。
- Color Palette widget の明示的な画像パレット抽出操作から呼ばれる。module scan は有効で、clang P1689 scan 対象に含まれる。実装本文は LF 改行への正規化以外変更なし。
- ArtifactCore 集計は `.cpp` 139、実装形式 `.cppm` 335。
- ビルド・テスト・CMake configure/generate は未実施（指示により）。
### Audio.FormantExtractor implementation migration（2026-10-22）

- `ArtifactCore/src/Audio/FormantExtractor.cppm` を `.cpp` に移行し、Audio split target、source manifest、module reference map を更新した。公開 `Audio.FormantExtractor` interface は維持。
- 現在の利用箇所は `LipSyncTrack` の明示的な音声解析。先に `.cpp` 化した `LipSyncTrack.cpp` から参照される implementation provider となる。実装本文は LF 改行への正規化以外変更なし。
- ArtifactCore 集計は `.cpp` 140、実装形式 `.cppm` 334。
- ビルド・テスト・CMake configure/generate は未実施（指示により）。
### Audio.Analyze implementation migration（2026-10-22）

- `ArtifactCore/src/Audio/AudioAnalyzer.cppm` を `.cpp` に移行し、Audio split target、source manifest、module reference map を更新した。公開 `Audio.Analyze` interface は維持。
- LipSyncTrack / FormantExtractor と Animation menu の明示的な音声分析から利用される。再生 callback 経路ではない。`.cpp` importer から `.cpp` provider を参照する module dependency の一例となる。実装本文は LF 改行への正規化以外変更なし。
- ArtifactCore 集計は `.cpp` 141、実装形式 `.cppm` 333。
- ビルド・テスト・CMake configure/generate は未実施（指示により）。
### Application.AppSettings implementation migration（2026-10-22）

- `ArtifactCore/src/Application/ArtifactAppSettings.cppm` を `.cpp` に移行し、source manifest、module reference map、source-specific CMake compile configuration / dependency selector を更新した。公開 `Application.AppSettings` interface と既存 `W_OBJECT_IMPL` は維持。
- 起動時設定と設定 UI の変更処理。描画・再生 hot path ではない。実装本文は LF 改行への正規化以外変更なし。
- ArtifactCore 集計は `.cpp` 142、実装形式 `.cppm` 332。
- ビルド・テスト・CMake configure/generate は未実施（指示により）。
### IO.VectorExport implementation migration（2026-10-22）

- `ArtifactCore/src/IO/VectorExport.cppm` を `.cpp` に移行し、IO split target、source manifest、module reference map、source-specific module dependency references を更新した。公開 `IO.VectorExport` interface は維持。
- Artifact の render queue から SVG sequence export 時に利用される。通常 playback / preview ではなく、ユーザー指定の書き出し経路。実装本文は LF 改行への正規化以外変更なし。
- ArtifactCore 集計は `.cpp` 143、実装形式 `.cppm` 331。
- ビルド・テスト・CMake configure/generate は未実施（指示により）。
### IO.ImageExporter implementation migration（2026-10-22）

- `ArtifactCore/src/IO/Image/ImageExporter.cppm` を `.cpp` に移行し、IO split target、source manifest、module reference map、source-specific IFC / object dependency 設定を更新した。公開 `IO.ImageExporter` interface と既存 `W_OBJECT_IMPL` は維持。
- Artifact の offscreen renderer / render queue から静止画を出力する既存経路。実装本文は LF 改行への正規化以外変更なし。
- ArtifactCore 集計は `.cpp` 144、実装形式 `.cppm` 330。
- ビルド・テスト・CMake configure/generate は未実施（指示により）。
### Image.PSDDocument implementation migration（2026-10-22）

- `ArtifactCore/src/Image/PSDDocument.cppm` を `.cpp` に移行し、source manifest、module reference map、source-specific compile references を更新した。公開 `Image.PSDDocument` interface は維持。
- PSD import/export の静止画経路で、Import PSD dialog、project service、image importer から利用される。実装本文は LF 改行への正規化以外変更なし。
- ArtifactCore 集計は `.cpp` 145、実装形式 `.cppm` 329。
- ビルド・テスト・CMake configure/generate は未実施（指示により）。
### Image.Raw implementation migration（2026-10-22）

- `ArtifactCore/src/Image/RawImage.cppm` を `.cpp` に移行し、source manifest、module reference map、source-specific module dependency references を更新した。公開 `Image.Raw` interface は維持。
- 広く import される基本画像型の型サイズ判定と validity check の実装。関数本体・データ表現は変更なし。本文は LF 改行への正規化以外同一。
- ArtifactCore 集計は `.cpp` 146、実装形式 `.cppm` 328。
- ビルド・テスト・CMake configure/generate は未実施（指示により）。
### Image.DeepImageBuffer implementation migration（2026-10-22）

- `ArtifactCore/src/Image/DeepImageBuffer.cppm` を `.cpp` に移行し、source manifest、module reference map、source-specific module dependency references を更新した。公開 `Image.DeepImageBuffer` interface は維持。
- Artifact render queue の multi-channel / deep export 用 provider。通常 frame rendering 経路ではない。実装本文とデータ表現は変更せず、LF 改行への正規化のみ。
- ArtifactCore 集計は `.cpp` 147、実装形式 `.cppm` 327。
- ビルド・テスト・CMake configure/generate は未実施（指示により）。
### Frame.Range implementation migration（2026-10-22）

- `ArtifactCore/src/Frame/FrameRange.cppm` を `.cpp` に移行し、source manifest、module reference map、NLE OTIO の `/reference` 設定、FrameRange の source-specific object dependency 条件を更新した。公開 `Frame.Range` interface と serialization registration は維持。
- Timeline / composition frame range 操作から広く利用される基盤 provider。実装本文は LF 改行への正規化以外変更なし。
- ArtifactCore 集計は `.cpp` 148、実装形式 `.cppm` 326。
- ビルド・テスト・CMake configure/generate は未実施（指示により）。
### MeshImporter implementation migration（2026-10-22）

- `ArtifactCore/src/Geometry/MeshImporter.cppm` を `.cpp` に移行し、source manifest、module reference map、source-specific module references / object dependency selector、OpenUSD 有効時の `CXX.dd` dependency source path を更新した。公開 `MeshImporter` interface は維持。
- 3D model layer / viewer の明示的 asset import 経路で利用。importer ロジックと third-party dependencies は変更せず、実装本文は LF 改行への正規化以外同一。
- ArtifactCore 集計は `.cpp` 149、実装形式 `.cppm` 325。
- ビルド・テスト・CMake configure/generate は未実施（指示により）。OpenUSD 有効構成の compile は未確認。
### Preview.Quality implementation migration（2026-10-22）

- `ArtifactCore/src/Preview/PreviewQuality.cppm` を `.cpp` に移行し、source manifest と module reference map を更新した。公開 `Preview.Quality` interface は維持。
- Artifact composition / render controller の preview quality 設定に使われる provider。renderer 本体・品質選択ロジックは変更せず、実装本文は LF 改行への正規化以外同一。
- ArtifactCore 集計は `.cpp` 150、実装形式 `.cppm` 324。
- ビルド・テスト・CMake configure/generate は未実施（指示により）。
### Procedural3DGenerators implementation migration（2026-10-22）

- `ArtifactCore/src/Geometry/Procedural3DGenerators.cppm` を `.cpp` に移行し、source manifest、module reference map、明示 `CORE_IMPL` 登録、source-specific compile option / object dependency selector を更新した。公開 `Procedural3DGenerators` interface は維持。
- Procedural 3D generator provider から Mesh / ShapeExtrude / Math.Noise などを利用する。実装本文は LF 改行への正規化以外同一。
- MSVC 用の明示 `/reference` と object ordering は保持した。Clang GNU frontend と clang-cl の `.cpp` scan 有効化経路では P1689 dependency map に切り替わる想定だが、この source の scan / compile / link は未確認。
- ArtifactCore 集計は `.cpp` 151、実装形式 `.cppm` 323。
- ビルド・テスト・CMake configure/generate は未実施（指示により）。
### Math.Bezier.Sampler implementation migration（2026-10-22）

- `ArtifactCore/src/Geometry/BezierPathSampler.cppm` を `.cpp` に移行し、明示 source manifest、module reference map、および source-specific `/reference` selector の参照先を更新した。`Math.Bezier.Sampler` interface と `Math.Bezier` import は維持。
- Bezier path の標準・等距離・適応サンプリングと tangent sampling を実装する。関数の処理、戻り値、公開 API は変えず、本文は LF 正規化以外同一。
- ArtifactCore 集計は `.cpp` 152、実装形式 `.cppm` 322。
- ビルド・テスト・CMake configure/generate は未実施（指示により）。
### STB image implementation translation unit migration（2026-10-22）

- module 宣言を持たない `ArtifactCore/src/IO/Image/StbImageImplementation.cppm` を `.cpp` に移し、明示 source manifest のみ更新した。`STB_IMAGE_IMPLEMENTATION` と stb header include は維持。
- ArtifactCore 集計は `.cpp` 153、実装形式 `.cppm` 322、module 宣言のない `.cppm` は21。
- Clang の P1689 scan / compile を含む実 target は未確認。ビルド・テスト・CMake configure/generate は未実施（指示により）。
### Platform module-free source migration（2026-10-22）

- module 宣言を持たない `ArtifactCore/src/Platform/Platform.cppm` を `.cpp` に移行し、source manifest と Platform target の明示 source path を更新した。`allocConsole` / `allocConsle` の定義は変更していない。
- ArtifactCore 集計は `.cpp` 154、実装形式 `.cppm` 322、module 宣言のない `.cppm` は20。
- ビルド・テスト・CMake configure/generate は未実施（指示により）。
### Audio.Decibels implementation migration（2026-10-22）

- `ArtifactCore/src/Audio/AudioDecibels.cppm` を `.cpp` に移行し、Audio split target、source manifest、module provider / `Utils.String.UniString` import mapping の参照先を更新した。公開 `Audio.Decibels` interface は維持。
- AudioVolume の linear/dB 変換 API から利用される値型。実装本文・数値処理・所有権挙動は変更せず、LF 正規化以外同一。
- ArtifactCore 集計は `.cpp` 155、実装形式 `.cppm` 321、module 宣言のない `.cppm` は20。
- ビルド・テスト・CMake configure/generate は未実施（指示により）。
### Geometry.ShapeExtrude implementation migration（2026-10-22）

- `ArtifactCore/src/Geometry/ShapeExtrude.cppm` を `.cpp` に移行し、source manifest、module provider map、MSVC source-specific compile option / object dependency selector の source 条件を更新した。公開 `Geometry.ShapeExtrude` interface と `Mesh` import は維持。
- 輪郭の ear clipping と mesh extrusion を実装する。現行呼び出し箇所は `Procedural3DGenerators::generateTextExtrude`。幾何処理・データ構造・戻り値は変更せず、LF 改行へ正規化して末尾の余分な空行を整理した。
- ArtifactCore 集計は `.cpp` 156、実装形式 `.cppm` 320、module 宣言のない `.cppm` は20。

### Color.Float implementation migration（2026-10-22）

- `ArtifactCore/src/Color/FloatColor.cppm` を `.cpp` に移行し、明示 source manifest、module reference map、MSVC source-specific `/reference` 条件を更新した。公開 `Color.Float` interface と `Color.TransferFunction` import は維持。
- 実装ソースの内容は保持し、LF 改行に統一した。Clang / MSVC の scan・compile・link は本ターンでは未実行。
- ArtifactCore 集計は `.cpp` 157、実装形式 `.cppm` 319、module 宣言のない `.cppm` は20。

### Image.Cryptomatte.ExrWriter implementation migration（2026-10-22）

- `ArtifactCore/src/Image/Cryptomatte/CryptoExrWriter.cppm` を `.cpp` に移行し、明示 source manifest、module reference map、MSVC source-specific `/reference` 条件を更新した。公開 `Image.Cryptomatte.ExrWriter` interface、`Image.Cryptomatte.Pixel` import、既存の `Image.Cryptomatte.Image` / `Image.Cryptomatte.Pixel` provider 参照は維持。
- 実装ソースの内容は維持し、LF 改行に統一した。Clang / MSVC の scan・compile・link は本ターンでは未実行。
- ArtifactCore 集計は `.cpp` 158、実装形式 `.cppm` 318、module 宣言のない `.cppm` は20。

### NetworkRPCClient implementation migration（2026-10-22）

- `ArtifactCore/src/Network/NetworkRPCClient.cppm` を `.cpp` に移行し、source manifest、`ArtifactCoreNetwork` 分割 target の実装 source list、module reference map を更新した。公開 `NetworkRPCClient` interface と `NetworkRPCClient.ixx` への module attachment は維持。
- 実装ソースの内容は維持し、LF 改行に統一した。Clang / MSVC の scan・compile・link は本ターンでは未実行。
- ArtifactCore 集計は `.cpp` 159、実装形式 `.cppm` 317、module 宣言のない `.cppm` は20。

### Core.Scale.Zoom implementation migration（2026-10-22）

- `ArtifactCore/src/Transform/ZoomScale.cppm` を `.cpp` に移行し、source manifest、module reference map、MSVC source-specific `/reference` 条件を更新した。公開 `Core.Scale.Zoom` interface と `Core.ArtifactString` provider 参照は維持。
- 実装ソースの内容は維持し、LF 改行に統一した。Clang / MSVC の scan・compile・link は本ターンでは未実行。
- ArtifactCore 集計は `.cpp` 160、実装形式 `.cppm` 316、module 宣言のない `.cppm` は20。

### Color.Conversion implementation migration（2026-10-22）

- `ArtifactCore/src/Color/ColorConversion.cppm` を `.cpp` に移行し、source manifest、module reference map、MSVC source-specific `/reference` 条件を更新した。公開 `Color.Conversion` interface と `Color.Float` provider 参照は維持。
- `ColorBlendMode` の画素単位 blend 分岐から呼ばれるため、変換ロジックは変更せず拡張子と参照先だけ更新した。性能計測および Clang / MSVC の scan・compile・link は本ターンでは未実施。
- ArtifactCore 集計は `.cpp` 161、実装形式 `.cppm` 315、module 宣言のない `.cppm` は20。

### NetworkRPCServer implementation migration（2026-10-22）

- `ArtifactCore/src/Network/NetworkRPCServer.cppm` を `.cpp` に移行し、source manifest、`ArtifactCoreNetwork` / `ArtifactCore` 間の既存 source 振り分け、source-specific compile option、module reference map を更新した。公開 `NetworkRPCServer` interface と `Container.NamedVector` / `Container.Debug` provider 参照は維持。
- 実装ソースの内容は維持し、LF 改行に統一した。Clang / MSVC の scan・compile・link は本ターンでは未実施。
- ArtifactCore 集計は `.cpp` 162、実装形式 `.cppm` 314、module 宣言のない `.cppm` は20。

### Artifact.Layer.Composition.Factory implementation migration（2026-10-22）

- `Artifact/src/Layer/ArtifactCompositionLayerFactory.cppm` を `.cpp` に移行し、明示 source manifest を更新した。公開 factory interface と `Artifact.Layer.Composition` / `Memory.SharedPtr` imports は維持。
- 実装ソースの内容は維持し、LF 改行に統一した。Artifact target の Clang/MSVC scan・compile・link は本ターンでは未実施。
- Artifact 集計は `.cpp` 12、実装形式 `.cppm` 624、module 宣言のない `.cppm` は6。

### Artifact.Project.HookBridge implementation migration（2026-10-22）

- `Artifact/src/Project/ArtifactProjectHookBridge.cppm` を `.cpp` に移行し、Artifact の明示 source manifest を更新した。公開 `Artifact.Project.HookBridge` interface と `Artifact.Script.Hooks` import は維持。
- project manager の lifecycle hook 呼び出しをつなぐ実装内容は維持し、LF 改行に統一した。Clang / MSVC の scan・compile・link は本ターンでは未実施。
- Artifact 集計は `.cpp` 13、実装形式 `.cppm` 623、module 宣言のない `.cppm` は6。

### Artifact.Widgets.LayerEditor.ModePresentation implementation migration（2026-10-22）

- `Artifact/src/Widgets/LayerEditorModePresentation.cppm` を `.cpp` に移行し、Artifact の明示 source manifest を更新した。公開 `Artifact.Widgets.LayerEditor.ModePresentation` interface、`Artifact.Widgets.LayerEditor.ViewportChrome` / `Tool` imports は維持。
- widget 初期化と edit/display mode 変更時に呼ばれる UI readout 更新の実装内容・呼出頻度は変更していない。LF 改行に統一し、Clang / MSVC の scan・compile・link は本ターンでは未実施。
- Artifact 集計は `.cpp` 14、実装形式 `.cppm` 622、module 宣言のない `.cppm` は6。

### Artifact.Project.Items implementation migration（2026-10-22）

- `Artifact/src/Project/ArtifactProjectItems.cppm` を `.cpp` に移行し、Artifact の明示 source manifest を更新した。公開 `Artifact.Project.Items` interface と `Utils.Id` / `Utils.String.UniString` imports は維持。
- folder child の作成実装は変更せず、LF 改行に統一した。Clang / MSVC の scan・compile・link は本ターンでは未実施。
- Artifact 集計は `.cpp` 15、実装形式 `.cppm` 621、module 宣言のない `.cppm` は6。

### Artifact.Widgets.DialogButtons implementation migration（2026-10-22）

- `Artifact/src/Widgets/Dialog/ArtifactDialogButtons.cppm` を `.cpp` に移行し、Artifact の明示 source manifest を更新した。公開 `Artifact.Widgets.DialogButtons` interface と `Application.AppSettings` import は維持。
- 既存 dialog のボタン行生成、配置設定、文言、click wiring は変更していない。LF 改行に統一し、Clang / MSVC の scan・compile・link は本ターンでは未実施。
- Artifact 集計は `.cpp` 16、実装形式 `.cppm` 620、module 宣言のない `.cppm` は6。

### Artifact.Widgets.FxStudio.Dialog implementation migration（2026-10-22）

- `Artifact/src/Widgets/FxStudio/ArtifactFxStudioDialog.cppm` を `.cpp` に移行し、Artifact の明示 source manifest を更新した。公開 `Artifact.Widgets.FxStudio.Dialog` interface、`Artifact.FxStudio.Session` / `Artifact.FxStudio.PresetCatalog` / `Core.ArtifactString` imports は維持。
- dialog UI、既存 event wiring、session 操作の内容は変更していない。LF 改行に統一し、Clang / MSVC の scan・compile・link は本ターンでは未実施。
- Artifact 集計は `.cpp` 17、実装形式 `.cppm` 619、module 宣言のない `.cppm` は6。

### Artifact CMake compiler scan classification（2026-10-22）

- `Artifact/CMakeLists.txt` の implementation source 判定を `.cppm` に加えて、named module declaration を持つ `.cpp` に拡張した。clang-cl / upstream clang では両形式に `CXX_SCAN_FOR_MODULES` を設定し、`-x c++` は upstream clang の `.cppm` のみ、MSVC `CompileAsCpp` は `.cppm` のみへ適用する。
- Artifact 子リポジトリの `.gitattributes` に `CMakeLists.txt text eol=lf` を追加し、編集対象の作業ツリー改行を LF に統一した。CMake configure / generate とビルドは未実施。

### Artifact.Menu.Edit implementation migration（2026-10-22）

- `Artifact/src/Widgets/Menu/ArtifactEditMenu.cppm` を `.cpp` に移行し、Artifact の明示 source manifest を更新した。公開 `Artifact.Menu.Edit` interface と imports は維持。
- Undo / redo、selection、clipboard、layer 編集の既存 UI 操作は変更していない。LF 改行を維持し、Clang / MSVC の scan・compile・link は本ターンでは未実施。
- Artifact 集計は `.cpp` 27、実装形式 `.cppm` 609、module 宣言のない `.cppm` は6。

### Artifact.Menu.Effect implementation migration（2026-10-22）

- `Artifact/src/Widgets/Menu/ArtifactEffectMenu.cppm` を `.cpp` に移行し、Artifact の明示 source manifest を更新した。公開 `Artifact.Menu.Effect` interface と module imports は維持。
- Effect menu の catalog 表示、追加・削除 UI、既存 service / event 利用は変更していない。LF 改行を維持し、Clang / MSVC の scan・compile・link は本ターンでは未実施。
- Artifact 集計は `.cpp` 24、実装形式 `.cppm` 612、module 宣言のない `.cppm` は6。

### Artifact.Menu.Render implementation migration（2026-10-22）

- `Artifact/src/Widgets/Menu/ArtifactRenderMenu.cppm` を `.cpp` に移行し、Artifact の明示 source manifest を更新した。公開 `Menu.Render` interface と実装内容は維持。
- Render menu の構築、queue 操作、既存 service 呼び出しは変更していない。LF 改行を維持し、Clang / MSVC の scan・compile・link は本ターンでは未実施。
- Artifact 集計は `.cpp` 23、実装形式 `.cppm` 613、module 宣言のない `.cppm` は6。

### Artifact.Menu.Test implementation migration（2026-10-22）

- `Artifact/src/Widgets/Menu/ArtifactTestMenu.cppm` を `.cpp` に移行し、Artifact の明示 source manifest を更新した。公開 `Menu.Test` interface と既存 test menu 実装は維持。
- UI、action wiring、テスト用 widget の呼び出しは変更していない。LF 改行を維持し、Clang / MSVC の scan・compile・link は本ターンでは未実施。
- Artifact 集計は `.cpp` 26、実装形式 `.cppm` 610、module 宣言のない `.cppm` は6。

### Artifact.Menu.Time implementation migration（2026-10-22）

- `Artifact/src/Widgets/Menu/ArtifactTimeMenu.cppm` を `.cpp` に移行し、Artifact の明示 source manifest を更新した。公開 `Menu.Time` interface と module imports は維持。
- transport の各操作が `ArtifactPlaybackService` を使う既存実装を維持し、UI wiring / 動作は変更していない。LF 改行を維持し、Clang / MSVC の scan・compile・link は本ターンでは未実施。
- Artifact 集計は `.cpp` 25、実装形式 `.cppm` 611、module 宣言のない `.cppm` は6。

### Artifact.Menu.Script implementation migration（2026-10-22）

- `Artifact/src/Widgets/Menu/ArtifactScriptMenu.cppm` を `.cpp` に移行し、Artifact の明示 source manifest を更新した。公開 `Artifact.Menu.Script` interface、`Artifact.Script.Hooks` / C# / Python engine imports は維持。
- script menu の構築とユーザー操作実装は変更していない。LF 改行を維持し、Clang / MSVC の scan・compile・link は本ターンでは未実施。
- Artifact 集計は `.cpp` 22、実装形式 `.cppm` 614、module 宣言のない `.cppm` は6。

### Artifact.Menu.Help implementation migration（2026-10-22）

- `Artifact/src/Widgets/Menu/ArtifactHelpMenu.cppm` を `.cpp` に移行し、Artifact の明示 source manifest を更新した。公開 `Menu.Help` interface と実装内容は維持。
- Help メニューの構築、診断情報 export、ドキュメント操作は変更していない。LF 改行を維持し、Clang / MSVC の scan・compile・link は本ターンでは未実施。
- Artifact 集計は `.cpp` 21、実装形式 `.cppm` 615、module 宣言のない `.cppm` は6。

### Artifact.Menu.Option implementation migration（2026-10-22）

- `Artifact/src/Widgets/Menu/ArtifactOptionMenu.cppm` を `.cpp` に移行し、Artifact の明示 source manifest を更新した。公開 `Menu.Option` interface と実装の内容は維持。
- メニュー構築・環境設定操作の実装は変更していない。LF 改行を維持し、Clang / MSVC の scan・compile・link は本ターンでは未実施。
- Artifact 集計は `.cpp` 20、実装形式 `.cppm` 616、module 宣言のない `.cppm` は6。

### Artifact.Widgets.ProjectFilterProxyModel implementation migration（2026-10-22）

- `Artifact/src/Widgets/ProjectFilterProxyModel.cppm` を `.cpp` に移行し、Artifact の明示 source manifest を更新した。公開 `Artifact.Widgets.ProjectFilterProxyModel` interface と module imports は維持。
- Project View の検索・絞り込み model 実装は変更していない。利用箇所は Project Manager に限られる。LF 改行を維持し、Clang / MSVC の scan・compile・link は本ターンでは未実施。
- Artifact 集計は `.cpp` 19、実装形式 `.cppm` 617、module 宣言のない `.cppm` は6。

### Artifact.Widgets.ProjectMemoModel implementation migration（2026-10-22）

- `Artifact/src/Widgets/ArtifactProjectMemoModel.cppm` を `.cpp` に移行し、Artifact の明示 source manifest を更新した。公開 `Artifact.Widgets.ProjectMemoModel` interface は維持。
- widget のユーザー操作・初期化時に使われる memo model の実装内容は変更していない。LF 改行に統一し、Clang / MSVC の scan・compile・link は本ターンでは未実施。
- Artifact 集計は `.cpp` 18、実装形式 `.cppm` 618、module 宣言のない `.cppm` は6。
- Clang P1689 scan / compile、MSVC compile / link は未確認。ビルド・テスト・CMake configure/generate は実行していない。

### Artifact.Widgets.ClipBufferModel implementation migration（2026-10-22）

- `Artifact/src/Widgets/ArtifactClipBufferModel.cppm` を `.cpp` に移行し、Artifact の明示 implementation source list を更新した。公開 `Artifact.Widgets.ClipBufferModel` interface と model 操作は維持。
- クリップ履歴の追加・削除・上限処理の実装内容は変更していない。LF 改行を維持し、clang-cl / upstream clang の scan・compile・link は本ターンでは未実施。

### Artifact.Widgets.ClipBufferWidget implementation migration（2026-10-22）

- `Artifact/src/Widgets/ArtifactClipBufferWidget.cppm` を `.cpp` に移行し、Artifact の明示 implementation source list を更新した。公開 `Artifact.Widgets.ClipBufferWidget` interface と model import は維持。
- コピー履歴の表示、消去・貼り付け操作、既存の event bus 利用は変更していない。LF 改行を維持し、clang-cl / upstream clang の scan・compile・link は本ターンでは未実施。

### Artifact.Widgets.ContextShortcutProvider implementation migration（2026-10-22）

- `Artifact/src/Widgets/ArtifactContextShortcutProvider.cppm` を `.cpp` に移行し、Artifact の明示 implementation source list を更新した。公開 `Artifact.Widgets.ContextShortcutProvider` interface と `UI.ShortcutBindings` import は維持。
- mode 別ショートカット一覧の構築・順序・説明文は変更していない。混在していた作業ツリー改行を LF に整え、Git の元 blob と一致することを確認。clang-cl / upstream clang の scan・compile・link は本ターンでは未実施。

### Artifact.Widgets.PropertyEditor.Bool implementation migration（2026-10-22）

- `Artifact/src/Widgets/PropertyEditor/ArtifactPropertyEditorBool.cppm` を `.cpp` に移行し、Artifact の明示 implementation source list を更新した。同一 `Artifact.Widgets.PropertyEditor` module の他 implementation と公開 interface は維持。
- Boolean property editor の UI、値反映、既存 signal 接続は変更していない。CRLF 混在を LF に整え、Git の元 blob と一致することを確認。clang-cl / upstream clang の scan・compile・link は本ターンでは未実施。

### Artifact.Widgets.PropertyEditor.Enum implementation migration（2026-10-22）

- `Artifact/src/Widgets/PropertyEditor/ArtifactPropertyEditorEnum.cppm` を `.cpp` に移行し、Artifact の明示 implementation source list を更新した。同一 `Artifact.Widgets.PropertyEditor` module の他 implementation と公開 interface は維持。
- enum property editor の選択肢表示、値反映、既存 signal 接続は変更していない。CRLF 混在を LF に整え、Git の元 blob と一致することを確認。clang-cl / upstream clang の scan・compile・link は本ターンでは未実施。

### Artifact.Widgets.PropertyEditor.DashPattern implementation migration（2026-10-22）

- `Artifact/src/Widgets/PropertyEditor/ArtifactPropertyEditorDashPattern.cppm` を `.cpp` に移行し、Artifact の明示 implementation source list を更新した。同一 `Artifact.Widgets.PropertyEditor` module の他 implementation と公開 interface は維持。
- preset/custom pattern UI と既存 signal 接続は変更していない。CRLF 混在を LF に整え、Git の元 blob と一致することを確認。clang-cl / upstream clang の scan・compile・link は本ターンでは未実施。

### Artifact.Widgets.PropertyEditor.Path implementation migration（2026-10-22）

- `Artifact/src/Widgets/PropertyEditor/ArtifactPropertyEditorPath.cppm` を `.cpp` に移行し、Artifact の明示 implementation source list を更新した。同一 `Artifact.Widgets.PropertyEditor` module の他 implementation と公開 interface は維持。
- path 入力・browse dialog・既存 signal 接続は変更していない。CRLF 混在を LF に整え、Git の元 blob と一致することを確認。clang-cl / upstream clang の scan・compile・link は本ターンでは未実施。

### Artifact.Widgets.PropertyEditor.Abstract implementation migration（2026-10-22）

- `Artifact/src/Widgets/PropertyEditor/ArtifactPropertyEditorAbstract.cppm` を `.cpp` に移行し、Artifact の明示 implementation source list を更新した。同一 `Artifact.Widgets.PropertyEditor` module の派生 implementation と公開 interface は維持。
- commit / preview handler と scrub の既定動作は変更していない。CRLF 混在を LF に整え、Git の元 blob と一致することを確認。clang-cl / upstream clang の scan・compile・link は本ターンでは未実施。

### Artifact.Widgets.PropertyEditor.String implementation migration（2026-10-22）

- `Artifact/src/Widgets/PropertyEditor/ArtifactPropertyEditorString.cppm` を `.cpp` に移行し、Artifact の明示 implementation source list を更新した。同一 `Artifact.Widgets.PropertyEditor` module の他 implementation と公開 interface は維持。
- string / multiline / font editor の UI と既存 event bus subscription は変更していない。実装 blob の一致を確認。clang-cl / upstream clang の scan・compile・link は本ターンでは未実施。

### Artifact.Widgets.PropertyEditor.Color implementation migration（2026-10-22）

- `Artifact/src/Widgets/PropertyEditor/ArtifactPropertyEditorColor.cppm` を `.cpp` に移行し、Artifact の明示 implementation source list を更新した。同一 `Artifact.Widgets.PropertyEditor` module と承認済み `FloatColorPicker` import は維持。
- 色選択、表示色の反映、既存 signal 接続は変更していない。CRLF 混在を LF に整え、Git の元 blob と一致することを確認。clang-cl / upstream clang の scan・compile・link は本ターンでは未実施。

### Artifact.Widgets.PropertyEditor.ObjectReference implementation migration（2026-10-22）

- `Artifact/src/Widgets/PropertyEditor/ArtifactPropertyEditorObjectRef.cppm` を `.cpp` に移行し、Artifact の明示 implementation source list を更新した。同一 `Artifact.Widgets.PropertyEditor` module と object picker / project service imports は維持。
- object reference の表示、pick / clear、既存 drag/drop 処理は変更していない。CRLF 混在を LF に整え、Git の元 blob と一致することを確認。clang-cl / upstream clang の scan・compile・link は本ターンでは未実施。

### Artifact.Widgets.PropertyEditor.Rotation implementation migration（2026-10-22）

- `Artifact/src/Widgets/PropertyEditor/ArtifactPropertyEditorRotation.cppm` を `.cpp` に移行し、Artifact の明示 implementation source list を更新した。同一 `Artifact.Widgets.PropertyEditor` module と scrub 対応 interface は維持。
- rotation knob、numeric input、scrub の処理は変更していない。CRLF 混在を LF に整え、Git の元 blob と一致することを確認。clang-cl / upstream clang の scan・compile・link は本ターンでは未実施。

### Artifact.Widgets.PropertyEditor.AnimatorCount implementation migration（2026-10-22）

- `Artifact/src/Widgets/PropertyEditor/ArtifactPropertyEditorAnimatorCount.cppm` を `.cpp` に移行し、Artifact の明示 implementation source list を更新した。同一 `Artifact.Widgets.PropertyEditor` module と ArtifactPropertyWidget からの利用は維持。
- animator count の表示、preset menu、property 追加操作は変更していない。実装 blob の一致を確認。clang-cl / upstream clang の scan・compile・link は本ターンでは未実施。

### Artifact.Widgets.PropertyEditor implementation migration（2026-10-22）

- `Artifact/src/Widgets/PropertyEditor/ArtifactPropertyEditor.cppm` を `.cpp` に移行し、Artifact の明示 implementation source list を更新した。公開 `Artifact.Widgets.PropertyEditor` interface と実装 imports は維持。
- Property Editor の生成・更新経路、既存 event wiring と value handling は変更していない。実装 blob の一致を確認。clang-cl / upstream clang の scan・compile・link は本ターンでは未実施。

### Artifact.Widgets.PropertyEditor.Shared implementation migration（2026-10-22）

- `Artifact/src/Widgets/PropertyEditor/ArtifactPropertyEditorShared.cppm` を `.cpp` に移行し、Artifact の明示 implementation source list を更新した。共有 helper 定義と同一 module の import 境界は維持。
- 共有 property control の UI、既存 paint / event handling は変更していない。実装 blob の一致を確認。clang-cl / upstream clang の scan・compile・link は本ターンでは未実施。

### Artifact.Widgets.PropertyEditor.TextAnimatorColor implementation migration（2026-10-22）

- `Artifact/src/Widgets/PropertyEditor/ArtifactPropertyEditorTextAnimatorColor.cppm` を `.cpp` に移行し、Artifact の明示 implementation source list を更新した。公開 Property Editor module と承認済み `FloatColorPicker` imports は維持。
- Text Animator color / gradient controls の操作・custom paint は変更していない。実装 blob の一致を確認。clang-cl / upstream clang の scan・compile・link は本ターンでは未実施。

### Artifact.Widgets.PropertyEditor.Numeric implementation migration（2026-10-22）

- `Artifact/src/Widgets/PropertyEditor/ArtifactPropertyEditorNumeric.cppm` を `.cpp` に移行し、Artifact の明示 implementation source list を更新した。公開 Property Editor module と既存 imports は維持。
- numeric controls、scrub / slider preview 処理は変更していない。実装 blob の一致を確認。clang-cl / upstream clang の scan・compile・link は本ターンでは未実施。

### Artifact.Widgets.ContextShortcutHelperWidget implementation migration（2026-10-22）

- `Artifact/src/Widgets/ArtifactContextShortcutHelperWidget.cppm` を `.cpp` に移行し、Artifact の明示 implementation source list を更新した。公開 interface、provider import、唯一の `W_OBJECT_IMPL` は維持。
- mode 別 shortcut tree と既存 UI の更新処理は変更していない。実装 blob の一致を確認。clang-cl / upstream clang の scan・compile・link は本ターンでは未実施。

### Artifact.Widgets.PythonHookManagerWidget implementation migration（2026-10-22）

- `Artifact/src/Widgets/ArtifactPythonHookManagerWidget.cppm` を `.cpp` に移行し、Artifact の明示 implementation source list を更新した。公開 interface、`Artifact.Script.Hooks` import、唯一の `W_OBJECT_IMPL` は維持。
- hook 一覧の表示・refresh・既存 UI action wiring は変更していない。実装 blob の一致を確認。clang-cl / upstream clang の scan・compile・link は本ターンでは未実施。

### Artifact.Widgets.RecoveryWorkspace implementation migration（2026-10-22）

- `Artifact/src/Widgets/ArtifactRecoveryWorkspaceWidget.cppm` を `.cpp` に移行した。`Artifact/CMakeLists.txt` の MSVC dyndep 用固定 source 登録を更新。公開 interface と唯一の `W_OBJECT_IMPL` は維持。
- Recovery workspace の ledger 表示・更新処理は変更していない。実装 blob の一致を確認。clang-cl / upstream clang の scan・compile・link は本ターンでは未実施。

### Artifact.Widgets.ProjectMemoWidget implementation migration（2026-10-22）

- `Artifact/src/Widgets/ArtifactProjectMemoWidget.cppm` を `.cpp` に移行し、Artifact の明示 implementation source list を更新した。公開 interface、migrated `ProjectMemoModel` import、唯一の `W_OBJECT_IMPL` は維持。
- memo list の表示、追加・削除・編集操作は変更していない。実装 blob の一致を確認。clang-cl / upstream clang の scan・compile・link は本ターンでは未実施。

### Artifact.NoiseEffect implementation migration（2026-10-22）

- `Artifact/src/Effects/Glow/NoiseEffect.cppm` を `.cpp` に移行し、`Artifact/CMakeLists.txt` の全 Artifact implementation list と `Artifact/cmake/ArtifactSources.cmake` の manifest を更新した。公開 `NoiseEffect.ixx`、`Artifact.Effect.Abstract` / `Utils.String.UniString` imports、constructor / destructor 実装は維持。
- 既存 `build` tree の MSVC / Visual Studio 18 で CMake 再構成が完了し、生成された `ArtifactEffectsNoise.vcxproj` が `NoiseEffect.cpp` を登録することを確認した。実 target build は Diligent / OpenUSD / ArtifactCore など依存全体の再ビルドが始まったため中断し、MSVC での実 compile / archive link は未確認。
- upstream clang++ 22.1.3（Windows MSVC ABI）/ CMake 4.2 / Ninja の隔離 smoke で、実 `NoiseEffect.cpp` を P1689 scan・compile し static archive を作成した。依存 module は最小 stub interface を使うため Artifact の実依存 link までは検証していない。sccache 0.18 clean rebuild では4 compile requestsすべて hit、0 miss / 0 cache errors。


### Artifact.Effect.Field implementation migration（2026-10-22）

- `Artifact/src/Effects/ArtifactAbstractField.cppm` を `.cpp` に移行し、`Artifact/CMakeLists.txt` と `Artifact/cmake/ArtifactSources.cmake` を更新。module interface、module 名、constructor 実装と依存 import は変更していない。
- root MSVC / Visual Studio 18 の CMake configure / generate 後、`ArtifactEffectContract.vcxproj` が `ArtifactAbstractField.cpp` を登録することを確認した。upstream clang++ 22.1.3 / Windows MSVC ABI / Ninja の隔離 smoke では実 `.cpp` の P1689 scan、compile、static archive link に成功。imported module は最小 stub interface のため Artifact 実 target の compile / link と runtime は未確認。


### Artifact.Composition._2D implementation migration（2026-10-22）

- `Artifact/src/Composition/ArtifactComposition2D.cppm` を `.cpp` に移行し、`Artifact/cmake/ArtifactSources.cmake` を更新。public module interface、PImpl の生成・破棄、copy 禁止境界はそのまま。
- root MSVC / Visual Studio 18 の CMake configure / generate 後、`ArtifactAppRuntime.vcxproj` が `ArtifactComposition2D.cpp` を登録することを確認した。upstream clang++ 22.1.3 / Windows MSVC ABI / Ninja の隔離 smoke では実 `.cpp` の P1689 scan、compile、static archive link に成功。最小 stub interface を使うため Artifact 実 target link は未確認。sccache 0.18 は初回2 misses、clean rebuild 2 hits、cache errors 0。
- Clang は既存 public module 名 `Artifact.Composition._2D` の `_2D` に reserved module name 警告を出した。module 名はこの拡張子移行では変更していない。


### ArtifactCore.Video.Transitions.CrossfadeTransition implementation migration（2026-10-22）

- `ArtifactCore/src/Video/Transitions/CrossfadeTransition.cppm` を `.cpp` に移行し、Video target list、`ArtifactCoreSources.cmake`、`ArtifactCoreModuleReferences.cmake` の source path を更新。静的 registrar と creator 登録処理は維持し、実装本文は末尾改行の追加以外変更していない。
- root MSVC / Visual Studio 18 の CMake configure / generate 後、`ArtifactCoreVideo.vcxproj` が `CrossfadeTransition.cpp` を登録することを確認した。upstream clang++ 22.1.3 / Windows MSVC ABI / Ninja の隔離 smoke で実 `.cpp` の P1689 scan、compile、static archive link に成功。依存 module は stub interface のため ArtifactCoreVideo 実 target link は未確認。sccache 0.18 は初回4 misses、clean rebuild 4 hits、cache errors 0。


### ArtifactCore.Graphics.PSO.Cache implementation migration（2026-10-22）

- `ArtifactCore/src/Graphics/PSOCache.cppm` を `.cpp` に移行。source manifest、module reference map、MSVC 用 explicit `/reference` と `OBJECT_DEPENDS` の source 条件を更新した。cache path sanitation、directory creation、public module interface は変更していない。
- root MSVC / Visual Studio 18 の CMake configure / generate 後、`ArtifactCore.vcxproj` が `PSOCache.cpp` を登録することを確認した。upstream clang++ 22.1.3 / Windows MSVC ABI / Ninja の隔離 smoke で実 `.cpp` の P1689 scan、compile、static archive link に成功。Qt / imported modules は最小 stub を使うため ArtifactCore 実 target link は未確認。sccache 0.18 は初回4 misses、clean rebuild 4 hits、cache errors 0。


### Artifact.Project.TreeFilterProxyModel implementation migration（2026-10-22）

- `Artifact/src/Project/TreeFilterProxyModel.cppm` を `.cpp` に移行し、`Artifact/cmake/ArtifactSources.cmake` の source manifest を更新。公開 interface とフィルター設定処理は変更していない。
- upstream clang++ 22.1.3 / Windows MSVC ABI / Ninja の隔離 smoke で、実 interface と実 `.cpp` の P1689 scan・compile・static archive link に成功した。Qt は最小 stub を用いたため Artifact 実 target の link/runtime は未確認。sccache 0.18 は初回2 misses、clean rebuild 2 hits、cache errors 0。
- root CMake 4.2 / Visual Studio 18 configure・generate が成功し、`ArtifactAppRuntime.vcxproj` が `TreeFilterProxyModel.cpp` を登録することを確認した。Artifact 実 target は未ビルド。


### Artifact.Layer.DefaultsSupport implementation migration（2026-10-22）

- `Artifact/src/Layer/ArtifactLayerDefaultsSupport.cppm` を `.cpp` に移行し、`Artifact/cmake/ArtifactSources.cmake` を更新した。既定 layer behavior の実装 blob は 964 bytes で完全一致し、公開 interface と動作は変更していない。
- root CMake 4.2 / Visual Studio 18 configure・generate が成功し、`ArtifactAppRuntime.vcxproj` が `ArtifactLayerDefaultsSupport.cpp` を登録することを確認。Artifact 実 target は未ビルド。
- upstream clang++ 22.1.3 / Windows MSVC ABI / Ninja の隔離 smoke で、実装 `.cpp` と対応 module stub の P1689 scan、compile、static archive link に成功。sccache 0.18 は初回 2 misses、clean rebuild 2 hits、cache errors 0。実 Artifact interface との compile/link/runtime は未確認。


### Artifact.Widgets.NativeHelper implementation migration（2026-10-22）

- `Artifact/src/Widgets/Native/NativeHelper.cppm` を `.cpp` に移行し、`Artifact/cmake/ArtifactSources.cmake` を更新した。Windows DWM 呼び出しと `Widgets.NativeHelper` module interface はそのまま。実装 blob は 974 bytes で完全一致。
- root CMake 4.2 / Visual Studio 18 configure・generate が成功し、`ArtifactAppRuntime.vcxproj` が `NativeHelper.cpp` を登録することを確認。Artifact 実 target は未ビルド。
- upstream clang++ 22.1.3 / Windows MSVC ABI / Ninja で実 interface・実 `.cpp`・importer の P1689 scan、compile、static library と executable link が成功した。Win32 / Qt API は隔離 smoke 内の stub を使用し、DWM runtime 動作は検証していない。sccache 0.18 は初回 3 misses、clean rebuild 3 hits、bypass と cache errors は 0。


### Artifact.Composition.Settings implementation migration（2026-10-22）

- `Artifact/src/Composition/CompositionSettings.cppm` を `.cpp` に移行し、`Artifact/cmake/ArtifactSources.cmake` を更新した。PImpl、コピー構築、settings accessor 実装は変更せず、旧実装 blob と新ファイルは 1205 bytes で完全一致。
- root CMake 4.2 / Visual Studio 18 configure・generate が成功し、`ArtifactAppRuntime.vcxproj` が `CompositionSettings.cpp` を登録することを確認。Artifact 実 target は未ビルド。
- upstream clang++ 22.1.3 / Windows MSVC ABI / Ninja smoke で、実 `CompositionSettings.ixx`・実 `.cpp`・importer と P1689 scan、compile、static library / executable link に成功。未使用の imported modules と Qt 型は smoke provider / header を使用するため、Artifact 実依存 link/runtime は未確認。sccache 0.18 は初回 6 misses、clean rebuild 6 hits、bypass と cache errors は 0。


### Artifact.Localization.Localization implementation migration（2026-10-22）

- `Artifact/src/Localization/Localization.cppm` を `.cpp` に移行し、`Artifact/cmake/ArtifactSources.cmake` を更新した。実装 blob は BOM を含め 1636 bytes で完全一致し、作業ファイルは LF。
- root CMake 4.2 / Visual Studio 18 configure・generate が成功し、`ArtifactAppRuntime.vcxproj` が `Localization.cpp` を登録することを確認。Artifact 実 target と locale runtime は未検証。
- upstream clang++ 22.1.3 / Windows MSVC ABI / Ninja smoke で、実 `Localization.ixx`・実 `.cpp`・`Utils.String.UniString` provider・importer の P1689 scan、compile、static library / executable link が成功。QString / UniString は隔離 smoke stub を使用。sccache 0.18 の初回 clean build は 4 misses、clean rebuild は 4 hits、cache errors / bypass は 0。warm cache の別 build tree では 8 requests が hit。


### Artifact.Composition.Settings clang-cl lane follow-up（2026-10-22）

- CMake 4.4.4 / Ninja / clang-cl 22.1.3 で、実 `CompositionSettings.ixx`・`.cpp`・importer の dependency scan、compile、static library / executable link が成功した。Qt 型と unused imported module providers は isolated smoke fixture を使用し、Artifact 実依存 link / runtime は未確認。
- CMake が `-TP` unused argument warning を出したが build は成功。sccache 0.18.0 は 12 requests、うち4件を実行して2 misses / 2 hits、8 calls は `multiple input files` で非 cacheable、cache errors 0。


### Artifact.Service.ClipboardManager implementation migration / dual Clang lanes（2026-10-22）

- `Artifact/src/Service/ArtifactClipboardService.cppm` を `.cpp` に移行し、`Artifact/cmake/ArtifactSources.cmake` を更新した。Clipboard API と既存の Qt clipboard / `QImage` 使用は変更せず、実装 blob は LF で 1044 bytes 完全一致。
- root CMake 4.2 / Visual Studio 18 configure・generate が成功し、`ArtifactAppRuntime.vcxproj` が `ArtifactClipboardService.cpp` を登録することを確認。Artifact 実 target は未ビルド。
- CMake 4.4.4 / Ninja の隔離 smoke で、実 `ArtifactClipboardService.ixx`・`.cpp`・`Utils.String.UniString` provider・importer を upstream clang++ 22.1.3（Windows MSVC ABI）と clang-cl 22.1.3 の両レーンで scan、compile、static library / executable link できた。Qt clipboard / image 型と UniString provider は smoke stub を使い、Qt 実依存 link/runtime は未確認。
- sccache 0.18.0 の upstream clang++ は初回 4 misses、clean rebuild 4 hits、errors / bypass 0。clang-cl は8 requests中4件が cacheable で2 misses / 2 hits、4 calls は `multiple input files` のため bypass、cache errors 0。clang-cl の CMake `-TP` unused warning は出たが build は成功。


### Artifact.Widgets.LayerEditor.EventSubscriptions implementation migration / three compiler lanes（2026-10-22）

- `Artifact/src/Widgets/LayerEditorEventSubscriptions.cppm` を `.cpp` に移行し、`Artifact/cmake/ArtifactSources.cmake` を更新。6つの既存 event subscription の構築・callback wiring は変更せず、実装 blob は LF で1733 bytes完全一致。
- root CMake 4.2 / Visual Studio 18 configure・generate 後、`ArtifactAppRuntime.vcxproj` が `LayerEditorEventSubscriptions.cpp` を登録することを確認。Artifact 実 target は未ビルド。
- CMake 4.4.4 smoke は実 `LayerEditorEventSubscriptions.ixx`・`.cpp`・importer を使い、upstream clang++ 22.1.3 / Windows MSVC ABI / Ninja、clang-cl 22.1.3 / Ninja、MSVC 19.51.36257 / Visual Studio 18 の全 lane で scan・compile・static library / executable link に成功。`Event.Bus` と `Artifact.Event.Types` は最小 provider stub を使うため、Artifact 実依存 link/runtime は未確認。
- upstream clang++ の sccache 0.18.0 は初回5 misses、clean rebuild5 hits、bypass / errors 0。clang-cl は10 requests中4件が cacheable で2 misses / 2 hits、6 calls は `multiple input files` のため bypass、errors 0。MSVC smoke は sccache なしで成功。

### Artifact.Widgets.LayerEditor.RenderScheduler implementation migration（2026-10-22）

- `Artifact/src/Widgets/LayerEditorRenderScheduler.cppm` を `.cpp` に移行し、`Artifact/cmake/ArtifactSources.cmake` の実装ソース登録を更新。公開 module interface と QTimer による既存の coalesce / cancel 動作は変更していない。実装は `<atomic>` と `<functional>` を直接 include するよう依存を明示した（移行前は interface の include に依存して clang++ が `std::memory_order` / `std::function` を解決できなかった）。この2 include 以外の本文は元 blob と同一。
- 実 interface と実 `.cpp` を使った隔離 smoke で、upstream clang++ 22.1.3 / Windows MSVC ABI / Ninja、clang-cl 22.1.3 / Ninja、MSVC 19.51.36257 / Visual Studio 18 がそれぞれ module scan・compile・static library link に成功。`QTimer` と `QWidget` は最小 stub のため Qt 実 link / runtime は未確認。sccache 0.18.0 の upstream clang++ は初回2 misses、clean rebuild 2 hits、bypass / errors 0。Artifact 実 target は未ビルド。

### Artifact.Widgets.LayerEditor.ViewWheelController implementation migration（2026-10-22）

- `Artifact/src/Widgets/LayerEditorViewWheelController.cppm` を `.cpp` に移行し、`Artifact/cmake/ArtifactSources.cmake` の implementation source 登録を更新。公開 module interface と wheel delta の pan / zoom 判定は変更せず、移行後本文は元 blob と byte-for-byte 一致。
- 実 interface と `.cpp` を使った隔離 smoke で upstream clang++ 22.1.3 / Windows MSVC ABI / Ninja、clang-cl 22.1.3 / Ninja、MSVC 19.51.36257 / Visual Studio 18 の module scan・compile・static library link が成功。`QPoint` は最小 stub のため Qt 実 link / runtime は未確認。Artifact 実 target は未ビルド。
- sccache 0.18.0 の upstream clang++ は cold build 2 misses、clean rebuild 2 hits、bypass / errors 0。

### Artifact.Widgets.LayerEditor.ReleaseController implementation migration（2026-10-22）

- `Artifact/src/Widgets/LayerEditorReleaseController.cppm` を `.cpp` に移行し、`Artifact/cmake/ArtifactSources.cmake` の implementation source 登録を更新。public module interface と release event の既存 state / callback 処理は変更せず、Git rename detection で本文変更なし（100% rename）。
- 実 interface と `.cpp` を使った隔離 smoke で upstream clang++ 22.1.3 / Windows MSVC ABI / Ninja、clang-cl 22.1.3 / Ninja、MSVC 19.51.36257 / Visual Studio 18 の module scan・compile・static library link が成功。`Qt` / `QPointF` は最小 stub のため Qt 実依存 link / runtime は未確認。Artifact 実 target は未ビルド。
- sccache 0.18.0 の upstream clang++ は cold build 2 misses、clean rebuild 2 hits、bypass / errors 0。

### Artifact.Widgets.LayerEditor.FrameBackground implementation migration（2026-10-22）

- `Artifact/src/Widgets/LayerEditorFrameBackground.cppm` を `.cpp` に移行し、`Artifact/cmake/ArtifactSources.cmake` の implementation source 登録を更新。public interface と背景描画の既存分岐は変更せず、Git rename detection で本文変更なし（100% rename）。
- root CMake 4.4.4 / Visual Studio 18 configure・generate が成功し、`ArtifactAppRuntime.vcxproj` に `LayerEditorFrameBackground.cpp` が登録されたことを確認。implementation が利用する `QImage::isNull()` 用に `<QImage>` を global module fragment に直接 include した。
- 実 interface と `.cpp` を使った隔離 smoke で upstream clang++ 22.1.3 / Windows MSVC ABI / Ninja、clang-cl 22.1.3 / Ninja、MSVC 19.51.36257 / Visual Studio 18 の module scan・compile・static library link が成功。`Core.ArtifactMath`、renderer、context mode、color、math module は型・API を合わせた最小 provider stub、`QImage` は最小 stub のため Artifact 実依存 link/runtime は未確認。sccache 0.18.0 upstream clang++ cold build は5 misses、clean rebuild は5 hits、bypass / errors 0。Artifact 実 target は未ビルド。
### Artifact.Widgets.LayerEditor.FrameViewState implementation migration（2026-10-22）

- `Artifact/src/Widgets/LayerEditorFrameViewState.cppm` を `.cpp` に移行し、`Artifact/cmake/ArtifactSources.cmake` の implementation source 登録を更新。既存の frame view begin / restore 処理は変更せず、実装が利用する `QWidget` / `QSize` を global module fragment から直接 include。
- 実 interface と `.cpp` を使った隔離 smoke で upstream clang++ 22.1.3 / Windows MSVC ABI / Ninja、clang-cl 22.1.3 / Ninja、MSVC 19.51.36257 / Visual Studio 18 の module scan・compile・static library link が成功。renderer、Core.ArtifactMath、Math.Vec、Qt は最小 provider/header stub のため Artifact 実依存 link/runtime は未確認。sccache 0.18.0 upstream clang++ の clean rebuild は全 compile hit、bypass / errors 0。Artifact 実 target は未ビルド。

### Artifact.Render.Element implementation migration（2026-10-22）

- `Artifact/src/Render/ArtifactRenderElement.cppm` を `.cpp` に移行し、`Artifact/cmake/ArtifactSources.cmake` の implementation source 登録を更新。PImpl の生成・破棄と public module interface は変更せず、Git rename detection で本文変更なし（100% rename）。
- 実 interface、実 `.cpp` と importer を使った隔離 smoke で upstream clang++ 22.1.3 / Windows MSVC ABI / Ninja、clang-cl 22.1.3 / Ninja、MSVC 19.51.36257 / Visual Studio 18 の module scan・compile・static library / executable link が成功。sccache 0.18.0 upstream clang++ の clean rebuild は全 compile hit、bypass / errors 0。smoke executable は未実行。

### Artifact.Animation.LayerEffectEnvelope implementation migration（2026-10-22）

- `Artifact/src/Animation/ArtifactLayerEffectEnvelope.cppm` を `.cpp` に移行し、`Artifact/cmake/ArtifactSources.cmake` の implementation source 登録を更新。envelope sampling の計算と public interface は変更せず、Git rename detection で本文変更なし（100% rename）。
- 実 interface と `.cpp` を使った隔離 smoke で upstream clang++ 22.1.3 / Windows MSVC ABI / Ninja、clang-cl 22.1.3 / Ninja、MSVC 19.51.36257 / Visual Studio 18 の module scan・compile・static archive link が成功。`Core.ArtifactMath` は最小 provider stub、`wobjectdefs.h` は実物を使い Qt include path と `/Zc:__cplusplus` を設定。Artifact 実依存 link/runtime は未確認。
- sccache 0.18.0 upstream clang++ の clean rebuild は全 cacheable compile が hit、bypass / errors 0。

### Artifact.Render.CompositionChangeDetector implementation migration（2026-10-22）

- `Artifact/src/Render/CompositionChangeDetector.cppm` を `.cpp` に移行し、`Artifact/cmake/ArtifactSources.cmake` を更新。mutex 保護された changed-layer 集合、full-redraw 判定、reset / debugInfo の実装は変更せず、Git rename detection は100%。独立実装 unit から使う `QMutexLocker` / `QSet` / `QString` を直接 include。
- 実 interface と `.cpp`、importer を用いた smoke で upstream clang++ 22.1.3 / Windows MSVC ABI / Ninja、clang-cl 22.1.3 / Ninja、MSVC 19.51.36257 / Visual Studio 18 の module scan・compile・static library / executable link が成功。Qt mutex/set/string は最小 stub のため実 Qt link/runtime は未確認。実行ファイルは未実行。
- sccache 0.18.0 upstream clang++ の clean rebuild は全 compile cacheable calls が hit、bypass / errors 0。

### Artifact.Layer.Particle3D implementation migration（2026-10-22）

- `Artifact/src/Layer/ArtifactParticleLayer3D.cppm` を `.cpp` に移行し、`Artifact/cmake/ArtifactSources.cmake` の source path を更新。既存 module 名、3D flag と JSON type / layerType の処理は変更しない。
- 実装 `.cpp`、軽量化した `Artifact.Layer.Particle` provider module、importer の隔離 smoke で upstream clang++ 22.1.3 / Windows MSVC ABI / Ninja、clang-cl 22.1.3 / Ninja、MSVC 19.51.36257 / Visual Studio 18 の module scan・compile・static library / executable link が成功。Qt JSON と base Particle layer は stub のため Artifact 実依存 link/runtime は未確認。smoke executable は未実行。
- root CMake 4.4.4 / Visual Studio 18 configure・generate が成功し、ArtifactAppRuntime.vcxproj が ArtifactParticleLayer3D.cpp を登録することを確認。Artifact 実 target は未ビルド。
- sccache 0.18.0 upstream clang++ の clean rebuild は全 cacheable compile hit、bypass / errors 0。

### Artifact.Preview.Worker implementation migration（2026-10-22）

- `Artifact/src/Preview/ArtifactPreviewWorker.cppm` を `.cpp` に移行し、`Artifact/cmake/ArtifactSources.cmake` の source path を更新。PImpl の生成・破棄と QObject base initialization は維持。
- 実 interface と `.cpp`、importer を用いた隔離 smoke で upstream clang++ 22.1.3 / Windows MSVC ABI / Ninja、clang-cl 22.1.3 / Ninja、MSVC 19.51.36257 / Visual Studio 18 の module scan・compile・static library / executable link が成功。QObject / QThread は最小 Qt stub、`wobjectdefs.h` は空 stub のため Qt 実依存 link/runtime は未確認。smoke executable は未実行。
- sccache 0.18.0 upstream clang++ の clean rebuild は全 cacheable compile hit、bypass / errors 0。

### Artifact.Layer.Composition.Resolver implementation migration（2026-10-22）

- `Artifact/src/Layer/ArtifactCompositionLayerResolver.cppm` を `.cpp` に移行し、`Artifact/cmake/ArtifactSources.cmake` の source path を更新。ProjectService から composition weak reference を lock して返す実装は変更せず、Git rename detection で本文変更なし（100%）。
- 実 interface と `.cpp`、importer を使った隔離 smoke で upstream clang++ 22.1.3 / Windows MSVC ABI / Ninja、clang-cl 22.1.3 / Ninja、MSVC 19.51.36257 / Visual Studio 18 の module scan・compile・static library / executable link が成功。ProjectService / Composition / SharedPtr provider は最小 stub のため Artifact 実依存 link/runtime は未確認。smoke executable は未実行。
- sccache 0.18.0 upstream clang++ clean rebuild は全 cacheable compile hit、bypass / errors 0。

### Artifact.Audio.Effects.DCBlocker implementation migration（2026-10-22）

- `Artifact/src/Audio/Effects/DCBlockerEffect.cppm` を `.cpp` に移行し、`Artifact/cmake/ArtifactSources.cmake` を更新。process / parameter adapter は変更せず、DSP 実処理は既存 `AudioDCBlocker` に委譲したまま。
- 実 interface と `.cpp`、importer に対する隔離 smoke で upstream clang++ 22.1.3 / Windows MSVC ABI / Ninja、clang-cl 22.1.3 / Ninja、MSVC 19.51.36257 / Visual Studio 18 の module scan・compile・static library / executable link が成功。AudioSegment / AudioEffect / DCBlocker / String provider は最小 stub のため ArtifactCore 実依存 link と音声処理 runtime は未確認。smoke executable は未実行。
- sccache 0.18.0 upstream clang++ clean rebuild は cacheable compile requests が全 hit、bypass / errors 0。

### Artifact.Render.CompositionRevealPreparation source-only module migration（2026-10-22）

- `Artifact/src/Render/ArtifactCompositionRevealPreparation.cppm` を `.cpp` に移行し、`Artifact/cmake/ArtifactSources.cmake` の登録を更新。module provider の実装内容は変更していない。
- 隔離 smoke target で upstream clang++ 22.1.3 と clang-cl 22.1.3 の Ninja module scan、compile、static library / executable link が成功。stub provider を使ったため Artifact 実依存・runtime は未確認、smoke executable は未実行。
- upstream clang++ + sccache 0.18.0 の cold build と増分 build が成功。合計 5 compilation requests、5 misses、0 failures。C++ module BMI は再利用されず、現状この経路でキャッシュ効果は確認できていない。

### Artifact.Widgets.ProjectHealthSummary source-only module migration（2026-10-22）

- `Artifact/src/Widgets/ArtifactProjectHealthSummary.cppm` を `.cpp` に移行し、`Artifact/cmake/ArtifactSources.cmake` の登録を更新。summary 生成処理と module 宣言は変更せず、100% rename。
- 隔離 smoke target で upstream clang++ 22.1.3、clang-cl 22.1.3、MSVC 19.51.36257 / Visual Studio 18 の Ninja / Visual Studio module scan、compile、static library / executable link が成功。QString は簡易 stub のため Qt 実依存・Artifact 全体の link/runtime は未確認、smoke executable は未実行。
- upstream clang++ + sccache 0.18.0 の cold / incremental build が成功。2 compile requests、2 misses、0 failures。module BMI のキャッシュ効果は未確認。

### Artifact.Menu.LayerTransformFieldUndoCommands implementation migration（2026-10-22）

- `Artifact/src/Widgets/Menu/ArtifactLayerMenuTransformFieldUndoCommands.cppm` を `.cpp` に移行し、`Artifact/cmake/ArtifactSources.cmake` を更新。Undo command 実装と module declaration は変更せず、100% rename。
- 実 provider と Composition / Undo の最小 stub provider を用いた隔離 module smoke で upstream clang++ 22.1.3、clang-cl 22.1.3、MSVC 19.51.36257 / Visual Studio 18 の scan、compile、static library / executable link が成功。Composition / Undo stub のため Artifact 実依存・runtime は未確認、executable は未実行。
- upstream clang++ + sccache 0.18.0 の cold / incremental build が成功。4 compile requests、4 misses、0 failures。BMI 再利用は確認できていない。

### Artifact.Widgets.LayerAlignmentPresets source-only module migration（2026-10-22）

- `Artifact/src/Widgets/Timeline/ArtifactLayerAlignmentPresets.cppm` を `.cpp` に移行し、`Artifact/cmake/ArtifactSources.cmake` の source-only module registration を更新。整列・分布処理と module 宣言は変更せず、100% rename。
- 実 provider を使い、Composition / Layer / Geometry API を置換した隔離 module smoke で upstream clang++ 22.1.3、clang-cl 22.1.3、MSVC 19.51.36257 / Visual Studio 18 の scan、compile、static library / executable link が成功。stub API のため実依存 integration・runtime は未確認、executable は未実行。
- upstream clang++ + sccache 0.18.0 の cold / incremental build が成功。7 compile requests、7 misses、0 failures。module BMI 再利用は確認できていない。

### Artifact.Timeline.KeyframeUndoCommand source-only module migration（2026-10-22）

- `Artifact/src/Widgets/Timeline/ArtifactTimelineKeyframeUndoCommand.cppm` を `.cpp` に移行し、`Artifact/cmake/ArtifactSources.cmake` の source-only module 登録を更新。Undo command の実装と module 宣言は変更せず、100% rename。
- 実 provider を使い、UndoCommand / QString のみ置換した隔離 module smoke で upstream clang++ 22.1.3、clang-cl 22.1.3、MSVC 19.51.36257 / Visual Studio 18 の scan、compile、static library / executable link が成功。Qt / ArtifactCore 実依存と importer を含む実アプリ統合・runtime は未確認。smoke executable は未実行。
- upstream clang++ + sccache 0.18.0 の cold / incremental build が成功。3 compile requests、3 misses、0 failures。module BMI の再利用は確認できていない。

### sccache C++ module clean-rebuild verification（2026-10-22）

- 初回 build の直後に通常の incremental build を行う従来計測では、後者が compile action を実行しないため cache hit 検証にならなかった。Artifact.Timeline.KeyframeUndoCommand の隔離 module target で `sccache --zero-stats` → build → CMake clean → 同条件で再 build を実施した。
- upstream clang++ 22.1.3 / Ninja / sccache 0.18.0 で 6 compile requests がすべて cache hit、cache miss / error / compilation failure は 0。clean 後も module provider の BMI/object を復元し、importer の compile と executable link が成功した。これは隔離 fixture の evidence であり、Artifact 全体の clean build を代替しない。
- 以降の sccache 検証は cold build と clean rebuild を対にする。前節までの cold + incremental の miss 数は初回 build の miss であり、incremental build が hit しなかった意味ではない。

### Artifact.Timeline.KeyframeApplyCommands source-only module migration（2026-10-22）

- `Artifact/src/Widgets/Timeline/ArtifactTimelineKeyframeApplyCommands.cppm` を `.cpp` に移行し、`Artifact/cmake/ArtifactSources.cmake` の module source path を更新。Keyframe interpolation / roving command と module declaration は変更せず、100% rename。
- 実 provider を使い、Artifact / Property / Event / Undo API を最小 stub に置換した隔離 module smoke で upstream clang++ 22.1.3、clang-cl 22.1.3、MSVC 19.51.36257 / Visual Studio 18 の dependency scan、compile、static library / executable link が成功。実アプリの Qt / ArtifactCore integration と runtime は未確認、smoke executable は未実行。
- upstream clang++ + sccache 0.18.0 cold build → clean → rebuild で20 compile requests 中10 hit、10 miss、0 failures。最初の10件は cold build、clean rebuild では同条件の10件を cache hit で再利用した。

### Artifact.Widgets.ProjectManagerControls source-only module migration（2026-10-22）

- `Artifact/src/Widgets/ArtifactProjectManagerControls.cppm` を `.cpp` に移行し、`Artifact/cmake/ArtifactSources.cmake` の source-only module 登録を更新。ウィジェット helper 実装と module declaration は変更せず、100% rename。
- 既存 Visual Studio 18 / MSVC 19.51.36257 の実プロジェクト build tree で CMake を再生成し、`ArtifactAppRuntime.vcxproj` に `.cpp` provider が登録されたことを確認。対象 provider の ClCompile と唯一の importer `ArtifactProjectManagerWidget.cppm` の ClCompile がともに成功し、`ArtifactProjectManagerControls.cpp.ifc` と importer object が生成された。provider source object は 2,040,903 bytes、IFC は 30,865,542 bytes。
- `Artifact` 全体 target の試行は CMake 再構成後、OpenUSD / SPIRV-Tools / Diligent など大量の依存 project 再ビルドへ展開したため停止。全体 link は未確認。clang++ / clang-cl / sccache の本候補専用 smoke は未実施。

### Artifact.Widgets.ProjectViewHoverPopup module ownership correction（2026-10-22）

- 元の `.cppm` は別 module `Artifact.Widgets.ProjectViewHoverPopup` を宣言しながら、`Artifact.Widgets.ProjectManagerWidget` interface が所有する `HoverThumbnailPopupWidget` のメンバー関数を定義していた。clang-cl + Qt 6 の compile で module ownership 診断を再現。
- 実装を `module Artifact.Widgets.ProjectManagerWidget;` の module implementation unit とし、自己 module import と `export` を除去。CMake 登録を `ARTIFACT_APP_MODULE_SOURCES` から `ARTIFACT_APP_IMPL_SOURCES` に移し、`.cpp` の named-module implementation として P1689 scan 対象にした。
- clang-cl 22.1.3 + 実 Qt 6 headers の隔離 target で provider/interface、implementation unit、importer の compile と static library / executable link が成功。upstream clang++ は未確認。
- MSVC fixture は空き容量不足で PDB/IFC 書き込み時に失敗。J: が 0 bytes free と確認され、fixture clean 後も約1.1 MB のみ回復したため MSVC compile は未確認。Artifact 全体 build/link も未確認。

## 2026-10-23 実装ログ（本スレッド継続分）

### Clang / sccache fixture と root configure

- CMake 4.4.4 は `J:/dev/tools/cmake-4.4.4-windows-x86_64/bin/cmake.exe` を使用。VS 同梱 CMake 4.3.1 は Clang modules 用 P1689 scan の動作が合わないため fixture に使わない。
- CMakePresets.json に upstream clang++ MSVC ABI preset を用意し、vcpkg chainload、MSVC ABI target、Windows SDK / DXC runtime discovery を確認。root configure / generate は成功したが、ArtifactCore 全体の build/link は OpenUSD 等の広い prerequisite を含むため未完了。
- sccache 0.18.0 の clang++ module compile では daemon の TEMP/TMP が重要。V: の専用 temp に向けて server を再起動すると clean/warm fixture が cold miss → warm hit になった。

### 先行 migration の検証

- `Artifact/src/Widgets/LayerEditorMaskPressController.cppm`、`LayerEditorViewMoveController.cppm`、`LayerEditorMaskHoverController.cppm`、`LayerEditorViewKeyInputController.cppm`、`LayerEditorMaskMoveController.cppm` を `.cpp` 化し、Artifact source manifest / explicit CMake registrations を更新。実 public interfaces と移行 source を使う clang-cl / clang++ fixture で P1689 scan・compile (対象によっては link) を確認。sccache の cacheable implementation source は cold miss / warm hit、clang-cl module IFC は multiple input files により bypass。
- `Artifact/src/Widgets/Render/ViewportColorPipeline.cppm` を `.cpp` 化し、manifest と CMake の明示 source を更新。両 Clang fixture と sccache cold/warm を確認。steady-state QString key 再構築は Insight に記録。
- `ArtifactCore/src/Plugin/PluginCommon.cppm`、`src/Script/Engine/Syntax/ASTNode.cppm`、`src/Layer/LayerState.cppm` を `.cpp` 化し、ArtifactCore manifests / CMake split targets / IFC mappings を更新。両 Clang fixture の scan・compile・link 成功。root ArtifactCore 全体 build/link は未確認。
- `LayerState` 後の upstream clang++ root preset reconfigure/generate は成功し、移行 object files が root Ninja graph に登録された。全 build は広域 prerequisite のため実行完了していない。
- source manifest checker の stale 判定、module hygiene partition ownership check は修正後に検証。ただし workspace source manifest には今回対象外の既存違反が残る。

### 今回の追加 migration

- `ArtifactCore/src/Render/RendererQueueSetting.cppm` → `.cpp`: sources / module-reference manifest / CMake IFC mapping を更新。両 Clang fixture scan・compile・link 成功。clang++ sccache 6 miss → 6 hit、clang-cl implementation 2 miss → 2 hit（IFCは複数入力でbypass）。名前 setter と copy constructor の既存未実装は Insight に分離記録。
- `ArtifactCore/src/Audio/AudioStereoMixer.cppm` → `.cpp`: CMake / manifests 更新。処理本文は LF 正規化後に元ファイルと一致。両 Clang fixture 成功。clang++ 6 miss → 6 hit、clang-cl implementation 2 miss → 2 hit（IFC 4 calls/batch bypass）。
- `ArtifactCore/src/Color/Sarturation.cppm` → `.cpp`: compiler-specific IFC source reference を含む CMake と manifests 更新。両 Clang fixture 成功。clang++ 4 miss → 4 hit、clang-cl implementation 2 miss → 2 hit（IFC 2 calls/batch bypass）。
- `ArtifactCore/src/Audio/AudioWaveform.cppm` → `.cpp`: sources / module refs 更新。両 Clang fixture 成功。clang++ 6 miss → 6 hit、clang-cl implementation 2 miss → 2 hit（IFC 4 calls/batch bypass）。`process()` の `waveformData_.resize()` と repo 内 caller 未発見を既存 Insight に追加。allocation 挙動は変更していない。
- `ArtifactCore/src/ImageProcessing/BroadcastColors.cppm` → `.cpp`: module partition のcompiler-specific IFC source conditionsを含む CMake / manifests 更新。両 Clang fixture 成功。clang++ 7 miss → 7 hit、clang-cl implementation 2 miss → 2 hit（IFC 5 calls/batch bypass）。
- `ArtifactCore/src/ImageProcessing/LeaveColor.cppm` → `.cpp`: module partition の IFC mappings と manifests 更新。両 Clang fixture 成功。clang++ 7 miss → 7 hit、clang-cl implementation 2 miss → 2 hit（IFC 5 calls/batch bypass）。
- これらの実装本文は Git HEAD の元 `.cppm` と LF 正規化後に byte 一致。各回 module hygiene と diff check を実施。fixture の外部依存は最小 provider stubs を使うため、root 実依存 build/link の代替ではない。


### upstream clang++ root ArtifactCore build（2026-10-23）

- CMake 4.4.4 で `x64-Debug-ClangXX-MsvcAbi` を V: 上の専用 build directory に configure / generate。Clang 22.1.3 GNU frontend、`--target=x86_64-pc-windows-msvc`、Windows SDK 10.0.26100.0、DXC、sccache 0.18.0 を検出し、構成成功。
- `ArtifactCore` target を並列3で build し、3,159 actions が全て成功。OpenUSD 依存 build、ArtifactCore の P1689 scan、`.cppm` / `.cpp` module implementation compile、`ArtifactCoreEnvironment.lib` と `ArtifactCore.lib` の link を確認。移行済みの Affine / Blur / ColorTransform / OpenCV sources も root graph からコンパイルされた。実行ファイルは起動していない。
- Clang / Qt / OpenUSD の既存 warning は出たが error はなく exit code 0。Artifact application 全体の link / runtime は未確認。
- `check_module_hygiene.py` は pass。`check_source_manifests.py` は今回の migration paths について未登録を報告しない一方、別の Artifact / ArtifactCore source registrations と focused pack の不一致を報告して全体 fail。対象外の既存 manifest 状態として追跡する。
- workspace の ArtifactCore で `.cppm` を `.cpp` に移した37件を Git HEAD と比較し、LF 正規化後の本文一致を確認した。今回の変更による処理差分はない。


### clang-cl preset の vcpkg 再利用と root build 確認（2026-10-23）

- `x64-Debug-ClangCL` preset は vcpkg root / installed directory / manifest mode を明示していなかったため、CMake 初回 configure が manifest mode で `C:/vcpkg` に追加依存を展開しようとし、C: 空き容量 0 で失敗した。
- preset に upstream clang++ lane と同じ `C:/vcpkg/scripts/buildsystems/vcpkg.cmake`、`VCPKG_ROOT`、`VCPKG_INSTALLED_DIR`、`x64-windows` triplet、`VCPKG_MANIFEST_MODE=OFF` を追加。新規 build directory で CMake 4.4.4 configure / generate が成功し、既存 installed dependencies を再利用した。clang-cl sccache は既定 OFF のままにした。
- clang-cl ArtifactCore root target build を試行。OpenUSD prerequisite 群の初回構築が 3,048 actions に展開し、約1,097 action 時点で遅い Sdf parser compile 群に達したため、clang++ 全 target 成功済みの状態で作業時間を制限し中断した。ArtifactCore target link は未確認。移行対象専用 clang-cl fixtures は過去ログで scan / compile / link 済み。
- 修正後の clang-cl CMake preset configure は成功。全 target clang-cl build は pending verification として残す。

- 追加で `ArtifactCore/src/Audio/AudioWaveform.cpp` の実ソースを使う clang-cl / CMake 4.4.4 fixture を sccache 0.18.0 で clean → build → clean → rebuild。P1689 scan、module compile、executable link が成功。cold は implementation 2 miss・module command 4 bypass、clean rebuild は implementation 2 hit・module command 4 bypass、cache error / compilation failure 0。smoke executable は起動していない。


### ArtifactCore.ImageProcessing.SimpleChoker implementation migration（2026-10-23）

- `ArtifactCore/src/ImageProcessing/SimpleChoker.cppm` を `.cpp` へ移行。module body は LF 正規化後に Git HEAD と byte 一致。`ArtifactCoreSources.cmake`、`ArtifactCoreModuleReferences.cmake`、MSVC /reference source condition の path を更新した。
- upstream clang++ 22.1.3 / CMake 4.4.4 root build で CMake configure / generate、P1689 scan、`SimpleChoker.cpp` compile、`ArtifactCore.lib` relink が成功。sccache 0.18.0 は compile request 1 miss、cache error / compilation failure 0。最初の試行は server の temporary file が既定 TEMP 上で書けず失敗し、V: temp/cache で server を再起動後に成功した。
- clang-cl root CMake も再生成成功し、Ninja graph の `SimpleChoker.cpp.obj` input として登録された。clang-cl 全 ArtifactCore build は OpenUSD 依存の初回構築に時間を要するため未完了。AudioWaveform の実ソース clang-cl fixture は以前のログに記録したとおり compile/link と sccache cold / warm を確認済み。
- 実装本文に `process()` ごとの `new float4[width * height]` / `delete[]` がある。拡張子移行では変更していない。実呼び出し頻度とフレーム上の確保影響は未計測のため Insight に調査事項として記録した。


### ArtifactCore.Image.Bitmap implementation migration（2026-10-23）

- `ArtifactCore/src/Image/BitmapImage.cppm` を `.cpp` へ移行。内容は LF 正規化後に Git HEAD と byte 一致。`ArtifactCoreSources.cmake`、`ArtifactCoreModuleReferences.cmake`、MSVC IFC source mapping を更新。
- upstream clang++ 22.1.3 root build で自動 CMake regenerate、P1689 scan、`.cpp` compile、`ArtifactCore.lib` relink が成功。sccache cache errors / compilation failures は 0。
- clang-cl 22.1.3 の実 interface + 実 implementation fixture で CMake 4.4.4 P1689 scan、compile、static library link が成功。sccache cold build は implementation 1 miss・interface command 1 bypass、clean rebuild は implementation 1 hit・interface 1 bypass、error / failure 0。
- clang-cl root CMake も再生成成功。Ninja query で `src/Image/BitmapImage.cpp` を ArtifactCore の scanned object input として確認した。root 全体 clang-cl build は未完了のまま。

- clang-cl root build の再開を試みたが、現行 source graph で1,952 actionsとなり、parallel 6でも約2分で24 actionsまでだった。対象 `.cpp` object だけを Ninja に直接指定した試行も target-level ordering が OpenUSD Sdf prerequisite を含む1,029-action chainを起動し、8 actions後に停止。root clang-cl artifact link は未確認。短い正確な compiler/link evidence は実ソース fixture を基準とする。


### ArtifactCore.ImageProcessing.Emboss implementation migration（2026-10-23）

- `ArtifactCore/src/ImageProcessing/Emboss.cppm` を `.cpp` へ移行。implementation body は LF 正規化後に Git HEAD と byte 一致。`ArtifactCoreSources.cmake`、`ArtifactCoreModuleReferences.cmake`、MSVC IFC source condition を更新。
- upstream clang++ 22.1.3 の実 ArtifactCore target で CMake auto-regenerate、P1689 scan、`.cpp` compile、`ArtifactCore.lib` relink が成功。sccache request 1 miss、cache error / compilation failure 0。
- clang-cl 22.1.3 の実 Emboss interface / implementation fixture で P1689 scan、compile、static library link に成功。依存 provider は最小 stub。sccache cold build は implementation 1 miss・module commands 5 bypass、clean rebuild は implementation 1 hit・5 bypass、error / failure 0。
- 実装には `width * height` 全画素分の `new[]` scratch buffer がある。移行で処理は変更していない。frame evaluation における呼び出し頻度・allocation impact を Insight に記録した。


### ArtifactCore.Image.ImageF16x4 implementation migration（2026-10-23）

- `ArtifactCore/src/Image/ImageF16x4.cppm` を `.cpp` へ移行。内容は LF 正規化後に Git HEAD と byte 一致。source manifest、module-reference manifest、MSVC IFC source mapping を更新。
- upstream clang++ 22.1.3 の root ArtifactCore build で自動 configure / generate、P1689 scan、`.cpp` compile、`ArtifactCore.lib` relink が成功。sccache 1 miss、error / failure 0。
- clang-cl 22.1.3 の実 ImageF16x4 interface / implementation fixture で P1689 scan、compile、static library link が成功。依存 module providers は最小 stubs。sccache cold build は implementation 1 miss・interface commands 4 bypass、clean rebuild は implementation 1 hit・4 bypass、error / failure 0。


### ArtifactCore.Image.Cryptomatte.Pixel implementation migration（2026-10-23）

- `ArtifactCore/src/Image/Cryptomatte/CryptoPixel.cppm` を `.cpp` へ移行。実装本文は LF 正規化後に Git HEAD と byte 一致。source manifest、module-reference manifest、MSVC IFC source mapping を更新。
- upstream clang++ 22.1.3 root ArtifactCore target で CMake configure / generate、P1689 scan、`.cpp` compile、`ArtifactCore.lib` relink が成功。sccache 1 miss、error / failure 0。
- clang-cl 22.1.3 の実 CryptoPixel interface / implementation と Qt Core headers を使う fixture で P1689 scan、compile、static library link 成功。sccache cold build は implementation 1 miss・interface command 1 bypass、clean rebuild は implementation 1 hit・1 bypass、error / failure 0。

### ArtifactCore.Image.Cryptomatte.Image and ExrWriter implementation migrations（2026-10-23）

- `ArtifactCore/src/Image/Cryptomatte/CryptoImage.cppm`、`CryptoExrWriter.cppm` をそれぞれ `.cpp` に移行。双方とも実装本文は LF 正規化後に Git HEAD と byte 一致。source manifest / module-reference manifest と CMake IFC source mapping を更新。
- CryptoImage は upstream clang++ 22.1.3 の root ArtifactCore target で P1689 scan、compile、`ArtifactCore.lib` relink が成功。clang-cl 22.1.3 fixture は CryptoPixel と CryptoImage の実 interface / implementation を使い scan・compile・static link が成功。clean rebuild は実装 2/2 sccache hit、interface compile は複数入力扱いで bypass。
- CryptoExrWriter は clang-cl 22.1.3 fixture で CryptoPixel / CryptoImage / ExrWriter interfaces と3 implementation units を含め、P1689 scan・compile・static link が成功。cold build は実装3 miss・interface3 bypass、clean rebuild は実装3/3 hit・interface3 bypass。upstream clang++ root target も CMake再構成・scan・CryptoExrWriter compile・ArtifactCore.lib relink が成功し、sccache 1 miss、cache error / compilation failure 0。
- EXR writer はファイル出力のコールドパス。拡張子移行のみで処理や一時 allocation は変更していない。root app build / runtime と clang-cl root ArtifactCore 全体 link は未確認。

### ArtifactCore.Video.Transitions.BlockDissolve registration migration（2026-10-23）

- `ArtifactCore/src/Video/Transitions/BlockDissolveTransition.cppm` を `.cpp` に移行。実装 unit は遷移 factory への静的登録のみで、処理コードは変更なし。本文は LF 正規化後に Git HEAD と byte 一致。source manifest / module-reference manifest / Video implementation target の登録を更新。
- upstream clang++ 22.1.3 の `ArtifactCoreVideo` target で CMake 再構成、125 action の依存 scan/build、対象 `.cpp` compile、`ArtifactCoreVideo.lib` link が成功。sccache 60 compile miss、cache errors / compilation failures 0。ArtifactCore 全体 target は別途この候補では実行していない。
- clang-cl 22.1.3 fixture で実 BlockDissolve interface / implementation を、NLE / Video imported module の最小スタブと組み合わせ P1689 scan、compile、static link まで成功。clean rebuild も成功。module interface compile は複数入力扱いで sccache bypass、cacheable compilation failure / cache error は 0。fixture executable は起動していない。

### ArtifactCore.ImageProcessing.ColorTransform.SelectiveColor implementation migration（2026-10-23）

- `ArtifactCore/src/ImageProcessing/ColorTransform/SelectiveColor.cppm` を `.cpp` に移行。本文は LF 正規化後に Git HEAD と byte 一致し、QImage 処理を含む実装や allocation / pixel algorithm は変更していない。source manifest、module-reference manifest と CMake 内の compiler-specific IFC mapping / dependency path をすべて更新。
- upstream clang++ 22.1.3 の root ArtifactCore target で CMake 再構成、P1689 scan、対象 `.cpp` compile、`ArtifactCore.lib` relink に成功。sccache 1 miss、cache error / compilation failure 0。
- clang-cl 22.1.3 fixture は実 SelectiveColor interface / implementation と Qt 6 Core/Gui headers、Parallel provider stub を使い scan・compile・static link に成功。cold compile は1 miss、clean rebuild は1 hit、cache errors / compilation failures 0。interface command 2件は複数入力扱いで bypass。fixture executable は起動していない。
- `check_module_hygiene.py` は pass。`check_source_manifests.py` は今回の移行 path を違反として報告しない。

### ArtifactCore.ImageProcessing.ColorTransform.Colorama implementation migration（2026-10-23）

- `ArtifactCore/src/ImageProcessing/ColorTransform/Colorama.cppm` を `.cpp` に移行。実装本文は LF 正規化後に Git HEAD と byte 一致し、既存の QImage / pixel processing 実装には変更なし。source manifest / module-reference manifest / CMake IFC source mapping を更新。
- upstream clang++ 22.1.3 の root ArtifactCore target で CMake 再構成、P1689 scan、対象 `.cpp` compile、`ArtifactCore.lib` relink 成功。sccache 1 miss、cache errors / compilation failures 0。
- clang-cl 22.1.3 fixture は実 Colorama interface / implementation と Qt 6 Core/Gui headers、Parallel provider stub を使い scan・compile・static link 成功。cold compile 1 miss、clean rebuild 1 hit。interface commands 2件は複数入力扱いで bypass、cache errors / compilation failures 0。fixture executable は起動していない。
- `check_module_hygiene.py` は pass。`check_source_manifests.py` は今回の移行 path を違反として報告しない。

### ArtifactCore.ImageProcessing.ColorTransform.LevelsCurves implementation migration（2026-10-23）

- `ArtifactCore/src/ImageProcessing/ColorTransform/LevelsCurves.cppm` を `.cpp` に移行。本文は LF 正規化後に Git HEAD と byte 一致し、Levels / Curves の処理は変更なし。source manifest / module-reference manifest / CMake IFC source mapping を更新。
- upstream clang++ 22.1.3 の root ArtifactCore target で CMake 再構成、P1689 scan、対象 `.cpp` compile、`ArtifactCore.lib` relink 成功。sccache 1 miss、cache errors / compilation failures 0。
- clang-cl 22.1.3 fixture は実 LevelsCurves interface / implementation と Qt 6 Core/Gui headers、Parallel provider stub を使い scan・compile・static link 成功。cold compile 1 miss、clean rebuild 1 hit。interface commands 2件は複数入力扱いで bypass、cache errors / compilation failures 0。fixture executable は起動していない。
- `check_module_hygiene.py` は pass。`check_source_manifests.py` は今回の移行 path を違反として報告しない。

### ArtifactCore.ImageProcessing.ColorTransform.TriChromaticShift implementation migration（2026-10-23）

- `ArtifactCore/src/ImageProcessing/ColorTransform/TriChromaticShift.cppm` を `.cpp` に移行。本文は LF 正規化後に Git HEAD と byte 一致。pixel processing / allocation は変更せず、source manifest / module-reference manifest / CMake IFC source mapping を更新。
- upstream clang++ 22.1.3 の root ArtifactCore target で CMake 再構成、P1689 scan、対象 `.cpp` compile、`ArtifactCore.lib` relink 成功。sccache 1 miss、cache errors / compilation failures 0。
- clang-cl 22.1.3 fixture は実 TriChromaticShift interface / implementation と、Particle / FloatRGBA / ImageF32x4_RGBA / Core.Parallel provider stubs を使用して scan・compile・static link 成功。cold compile 1 miss、clean rebuild 1 hit。interface commands 5件は複数入力扱いで bypass、cache errors / compilation failures 0。fixture executable は起動していない。
- `check_module_hygiene.py` は pass。`check_source_manifests.py` は今回の移行 path を違反として報告しない。

### ArtifactCore.ImageProcessing partition batch migration: Duotone / StrobeLight / TiltShift（2026-10-23）

- `ArtifactCore/src/ImageProcessing/Duotone.cppm`、`StrobeLight.cppm`、`TiltShift.cppm` を `.cpp` にまとめて移行。3実装本文はいずれも LF 正規化後に Git HEAD と byte 一致。partition interfaces / processing logic は変更していない。source manifest、module-reference manifest と CMake partition implementation / IFC mappings を更新。
- upstream clang++ 22.1.3 root ArtifactCore target は CMake 再構成後、3ソースの P1689 scan、各 compile、`ArtifactCore.lib` relink に成功。sccache 3 misses、cache errors / compilation failures 0。
- clang-cl 22.1.3 fixture は3つの実 partition interfaces / implementations と primary ImageProcessing module を含み、Particle / ImageF32x4_RGBA / Core.Parallel をstub providerにして P1689 scan・compile・static link 成功。cold build 3 misses、clean rebuild 3 hits、cache errors / compilation failures 0。7 interface compile commands は複数入力扱いで bypass。fixture executable は起動していない。
- `check_module_hygiene.py` は pass。`check_source_manifests.py` は3つの移行 path を違反として報告しない。

### ArtifactCore.ImageProcessing keying partition batch migration: ChromaKey / DifferenceKey / LumaKey（2026-10-23）

- `ArtifactCore/src/ImageProcessing/ChromaKey.cppm`、`DifferenceKey.cppm`、`LumaKey.cppm` を `.cpp` にまとめて移行。各本文は LF 正規化後に Git HEAD と byte 一致。画像処理アルゴリズムと allocation は変更せず、source manifest / module-reference manifest / CMake partition / IFC source mappings を更新した。別実装の `src/Image/ChromaKey.cppm` は維持。
- upstream clang++ 22.1.3 root ArtifactCore target で再構成、3ソースの P1689 scan・compile、`ArtifactCore.lib` relink 成功。sccache 3 misses、cache errors / compilation failures 0。
- clang-cl 22.1.3 fixture は3つの実 partition interfaces / implementations を含み、Particle / ImageF32x4_RGBA / SurfaceColorContract / Core.Parallel をstub providerにして scan・compile・static link 成功。初回 fixture は Particle stub に必要な `float3` 宣言がなく失敗したが、stub を補い直した後のビルドは成功。clean rebuild 3 hits、cache errors / compilation failures 0。interface commands 8件は複数入力扱いで bypass。fixture executable は起動していない。
- `check_module_hygiene.py` は pass。`check_source_manifests.py` はこの3 migration paths を違反として報告しない。

### ArtifactCore.ImageProcessing partition batch migration: Median / Scatter / Echo（2026-10-23）

- `ArtifactCore/src/ImageProcessing/Median.cppm`、`Scatter.cppm`、`Echo.cppm` を `.cpp` にまとめて移行。各実装本文は LF 正規化後に Git HEAD と byte 一致。partition interfaces / processing behavior は変更なし。source manifest、module-reference manifest、CMake partition implementation / IFC mapping を更新。
- upstream clang++ 22.1.3 root ArtifactCore target で再構成、3ソースの P1689 scan・compile、`ArtifactCore.lib` relink 成功。sccache 3 misses、cache errors / compilation failures 0。
- clang-cl 22.1.3 fixture は3つの実 partition interfaces / implementations と primary ImageProcessing module を含み、Particle / ImageF32x4_RGBA / Core.Parallel / Math.Random をstub providerにして scan・compile・static link 成功。cold build 3 misses、clean rebuild 3 hits、cache errors / compilation failures 0。8 interface commands は複数入力扱いで bypass。fixture executable は起動していない。
- `check_module_hygiene.py` は pass。`check_source_manifests.py` は今回の3 migration paths を違反として報告しない。

### ArtifactCore.ImageProcessing OpenCV partition batch migration: Monochrome / ChromaSpread / AnisotropicFlowBlur（2026-10-23）

- `ArtifactCore/src/ImageProcessing/OpenCV/Monochrome.cppm` と `src/ImageProcessing/ChromaSpread.cppm`、`AnisotropicFlowBlur.cppm` を `.cpp` にまとめて移行。各本文は LF 正規化後に Git HEAD と byte 一致。OpenCV / pixel processing logic は変更せず、source manifest / module-reference manifest / CMake partition and IFC mappings を更新。
- upstream clang++ 22.1.3 root ArtifactCore target で再構成、3ソースの P1689 scan・compile、`ArtifactCore.lib` relink 成功。sccache 3 misses、cache errors / compilation failures 0。
- clang-cl 22.1.3 fixture は実 Monochrome / ChromaSpread / AnisotropicFlowBlur / StructureTensor interfaces と OpenCV 4 headers を使い scan・compile・static link 成功。OpenCV image / Parallel providers は最小 stub。初回 fixture は directory base overlap と stub の `setFromCVMat` 不足で失敗したが、CMake file set を整理し stub API を補正した後のビルドは成功。clean rebuild 3 hits、cache errors / compilation failures 0。8 interface commands は複数入力扱いで bypass。fixture executable は起動していない。
- `check_module_hygiene.py` は pass。`check_source_manifests.py` は3つの migration paths を違反として報告しない。
### ArtifactCore.ImageProcessing OpenCV partition batch migration: GlitchCV / LiftCV / Noise（2026-10-23）

- `ArtifactCore/src/ImageProcessing/OpenCV/GlitchCV.cppm`、`LiftCV.cppm`、`Noise.cppm` を `.cpp` として取り込み。各実装本文は LF 正規化後に Git HEAD と一致。OpenCV / pixel processing logic は変更していない。source manifest、module-reference manifest、MSVC IFC mapping は `.cpp` を参照していることを確認。
- upstream clang++ 22.1.3 root `ArtifactCore` target は `ninja: no work to do`。既存 build graph に3ソースが登録済みで、前回の統合ビルドで対象を compile / link 済み。今回の変更状態で再ビルドを要求したが対象actionなし。
- clang-cl 22.1.3 fixture は実3 interface / implementation units と OpenCV headers を使い、P1689 scan、compile、static library link に成功。Math.Random / Core.Parallel は provider stub。sccache clean build 3 misses、clean rebuild 3 hits、cache error / compilation failure 0。interface compile commands 10件は複数入力のため非cacheable。fixture executable は起動していない。
- `check_module_hygiene.py` は pass。manifest / CMake に旧3 `.cppm` パスの残存なし。`BloomCV` / `MergeCV` は元実装自体が空stubであるため、実装移行バッチから除外した。
### ArtifactCore.ImageProcessing.OpenCV VHS_CV implementation migration（2026-10-23）

- `ArtifactCore/src/ImageProcessing/OpenCV/VHS_CV.cppm` を `.cpp` へ移行。実装本文は LF 正規化後に Git HEAD と一致。OpenCV / pixel processing behavior は変更せず、source manifest、module-reference manifest、MSVC IFC generation condition を更新。
- upstream clang++ 22.1.3 root `ArtifactCore` target で CMake configure / generate、P1689 scan、対象 `.cpp` compile、`ArtifactCore.lib` relink に成功。
- clang-cl 22.1.3 fixture は実 VHS_CV interface / implementation と OpenCV headers を使い scan・compile・static link 成功。Math.Random / Core.Parallel はprovider stubs。sccache cold build はimplementation 1 miss、clean rebuild は1 hit。cache errors / compilation failures 0。interface compile commands 4件は複数入力扱いで非cacheable。fixture executable は起動していない。
- `check_module_hygiene.py` は pass。`check_source_manifests.py` は対象 migration path を違反として報告しない。
### ArtifactCore.ImageProcessing.OpenCV face detection and tracking implementation migrations（2026-10-23）

- `ArtifactCore/src/ImageProcessing/OpenCV/FaceDetectionEngine.cppm` と `FaceTracker.cppm` を `.cpp` に移行。両implementation bodyはLF正規化後にGit HEADと一致。source manifest、module-reference manifest、MSVC IFC source条件、OBJECT_DEPENDS条件を `.cpp` パスへ更新。FaceDetection / FaceTrackerの相互module依存は維持。
- upstream clang++ 22.1.3 root `ArtifactCore` targetでCMake configure / generate、両ソースP1689 scan・compile、`ArtifactCore.lib` relink成功。
- clang-cl 22.1.3 fixtureは実両interface / implementation、実CvUtils、Qt Core/Gui・OpenCV headersでscan・compile・static link成功。`Memory.TrackedPtr`は実装では未使用のimportなのでprovider stubを使用。cold build実装2 miss、clean rebuild実装2 hit、cache errors / compilation failures 0。interface commandsは複数入力のためbypass。fixture executableは起動していない。
- `check_module_hygiene.py` pass。CMake/manifestに旧 `.cppm` pathの残存なし。
### ArtifactCore.ImageProcessing.OpenCV Puppet / Roto Brush implementation migrations（2026-10-23）

- `ArtifactCore/src/ImageProcessing/OpenCV/OpenCVPuppetEngine.cppm` と `OpenCVRotoBrushEngine.cppm` を `.cpp` へ移行。各本文はLF正規化後にGit HEADと一致。OpenCV編集処理は変更していない。source manifest、module-reference manifest、MSVC IFC source conditionsとOBJECT_DEPENDSを新拡張子に更新。`ArtifactCore.ImageProcessing.OpenCV.RotoBrushEngine`を参照するAI RotoBrush moduleは維持。
- upstream clang++ 22.1.3 root `ArtifactCore` targetでCMake configure / generate、両ソースP1689 scan・compile、`ArtifactCore.lib` relink成功。
- clang-cl 22.1.3 fixtureは実両interface / implementationとOpenCV headers、Core.Parallel provider stubを使いscan・compile・static link成功。sccache clean buildは両実装2 miss、clean rebuildは2 hit。cache errors / compilation failures 0。interface commands 10件は複数入力扱いでbypass。fixture executableは起動していない。
- `check_module_hygiene.py` pass。CMake / manifestに旧 `.cppm` pathの残存なし。
### ArtifactCore.ImageProcessing glow and echo implementation migrations（2026-10-23）

- `ArtifactCore/src/ImageProcessing/ChromaSpreadGlow.cppm` と `EdgeEcho.cppm` を `.cpp` に移行。両本文はLF正規化後にGit HEADと一致。source manifest / module-reference manifest、MSVC IFC mappingとEdgeEcho固有OBJECT_DEPENDSを更新。
- upstream clang++ 22.1.3 root `ArtifactCore` targetで両ソースP1689 scan・compile、`ArtifactCore.lib` relink成功。
- clang-cl 22.1.3 fixtureは実ChromaSpreadGlow / ChromaSpread interfacesとprovider modulesでscan・compile・static link成功。sccache初回1 miss、clean rebuild 1 hit、errors / compilation failures 0。module interface commands 16件は複数入力のためbypass。EdgeEchoを含むfixtureでは、HEADの旧 `.cppm` と現`.cpp`の双方に存在する `FloatRGBA` 未宣言で実装compileが停止。拡張子変更によるエラーではない。EdgeEchoはupstream clang++でcompile・link成功。
- `check_module_hygiene.py` pass。両pathの旧`.cppm`参照なし。
### ArtifactCore.ImageProcessing motion and vector glitch implementation migrations（2026-10-23）

- `ArtifactCore/src/ImageProcessing/MotionTrail.cppm` と `VectorFlowGlitch.cppm` を `.cpp` へ移行。両本文はLF正規化後にGit HEADと一致。source manifest / module-reference manifestとMSVC IFC mappingを更新。VectorFlowGlitchの既存StructureTensor partition依存を維持。
- upstream clang++ 22.1.3 root `ArtifactCore` targetで対象両ソースのP1689 scan・compile、`ArtifactCore.lib` relink成功。依存変更による前回移行EdgeEcho再compileも通過。
- clang-cl 22.1.3 fixtureは実MotionTrail / VectorFlowGlitch / StructureTensor interfacesとImage / Particle / Core.Parallel provider stubsでscan・compile・static link成功。sccache cold build 2 misses、clean rebuild 2 hits、cache errors / compilation failures 0。interface commands 16件は複数入力扱いでbypass。fixture executableは起動していない。
- `check_module_hygiene.py` pass。対象pathに旧`.cppm`参照なし。
### ArtifactCore.ImageProcessing repair and halftone implementation migrations（2026-10-23）

- `ArtifactCore/src/ImageProcessing/FilmRepair.cppm`、`Halftone.cppm`、`WireRemove.cppm`を `.cpp`へ移行。3本文はLF正規化後にGit HEADと一致。source manifest / module-reference manifestとCMake内MSVC IFC referencesを更新。
- upstream clang++ 22.1.3 root `ArtifactCore` targetで3ソースP1689 scan・compile、`ArtifactCore.lib` relink成功。
- clang-cl 22.1.3 fixtureは実3 interfaces / implementationとOpenCV headers、Image / Particle / Core.Parallel provider stubsでscan・compile・static link成功。sccache cold build 3 misses、clean rebuild 3 hits、cache errors / compilation failures 0。interface commands 14件は複数入力扱いでbypass。fixture executableは起動していない。
- `check_module_hygiene.py` pass。対象3 pathに旧`.cppm`参照なし。
### ArtifactCore.ImageProcessing neuromorphic displacement and structure tensor migrations（2026-10-23）

- `ArtifactCore/src/ImageProcessing/NeuromorphicDisplace.cppm` と `StructureTensor.cppm` を `.cpp`へ移行。各本文はLF正規化後にGit HEADと一致。source manifest / module-reference manifestとMSVC IFC conditionsを更新。
- upstream clang++ 22.1.3 root `ArtifactCore` targetで両ソースP1689 scan・compile、`ArtifactCore.lib` relink成功。
- clang-cl 22.1.3 fixtureは実両interface / implementationとOpenCV headers、Image / Particle / FloatRGBA / Core.Parallel provider stubsでscan・compile・static link成功。sccache cold build 2 misses、clean rebuild 2 hits、cache errors / compilation failures 0。interface commands 14件は複数入力扱いでbypass。fixture executableは起動していない。
- `check_module_hygiene.py` pass。対象2 pathに旧`.cppm`参照なし。

### ArtifactCore.ImageProcessing distortion and directional blur implementation migrations（2026-10-23）

- `ArtifactCore/src/ImageProcessing/Distortion.cppm` と `SharpenDirectionalBlur.cppm` を `.cpp` に移行。両本文は LF 正規化後に Git HEAD と一致。source manifest / module-reference manifest、MSVC IFC mapping 条件を新拡張子に更新。module interface と実装内容は変更していない。
- upstream clang++ 22.1.3 の root `ArtifactCore` targetで両ソースの P1689 scan・compile、`ArtifactCore.lib` relink 成功。
- clang-cl 22.1.3 の隔離 fixture で `SharpenDirectionalBlur.cpp` は実 interface / implementation と `Core.Parallel` provider stub を使い scan・compile・static link 成功。Qt include path が不足した初回 fixture はスキャンで停止し、include path を加えた再実行で通過。`Distortion.cpp` の clang-cl fixture は未実施。
- `check_module_hygiene.py` pass。対象 CMake / manifests に旧 `.cppm` path の残存なし。

### ArtifactCore OpenCV Draw / Glow implementation migrations（2026-10-23）

- `ArtifactCore/src/ImageProcessing/OpenCV/Draw.cppm` と `Glow.cppm` を `.cpp` に移行。両本文は LF 正規化後に Git HEAD と一致。source manifest / module-reference manifest、MSVC IFC mapping と Glow の OBJECT_DEPENDS を更新。
- upstream clang++ 22.1.3 root `ArtifactCore` targetで両ソースの P1689 scan・compile、`ArtifactCore.lib` relink 成功。sccache cold rebuild は2 miss、warm rebuild は2 hit。cache error / compilation failure 0。
- clang-cl 22.1.3 fixture は実 Draw / Glow interfaces と両実装、OpenCV headers、`Core.Parallel` / `Image` / `Math.Random` provider stubsで scan・compile・static link 成功。初回 fixture は Math.Random stub に不足した `makeRandomStream` があり失敗したため、API を補って再実行した。
- `check_module_hygiene.py` pass。対象 CMake / manifests に旧 `.cppm` path の残存なし。

### ArtifactCore DirectCompute shader wrapper batch: BroadcastColors / ChromaKey / Emboss（2026-10-23）

- `BroadcastColorsCS.cppm` / `ChromaKeyCS.cppm` / `EmbossCS.cppm` を `.cpp` に移行。3本文は LF 正規化後に Git HEAD と一致。source manifest / module-reference manifest と共通 MSVC IFC mapping 条件を更新。Diligent wrapper の実装と依存内容は変更していない。
- upstream clang++ 22.1.3 root `ArtifactCore` targetで3ソースの P1689 scan・compile、`ArtifactCore.lib` relink 成功。sccache cold rebuild は3 miss、warm rebuild は3 hit。cache error / compilation failure 0。
- clang-cl 22.1.3 fixture は実3 interfaces / implementationと Diligent / Qt / OpenCV headers、Image provider stubsで scan・compile・static link 成功。初期 fixture の不足を補うため `PLATFORM_WIN32=1` と Image cache provider 型を追加した。
- `check_module_hygiene.py` pass。対象 CMake / manifests に旧 `.cppm` path の残存なし。

### ArtifactCore DirectCompute wrapper batch: LeaveColor / LumaKey / Median（2026-10-23）

- `LeaveColorCS.cppm` / `LumaKeyCS.cppm` / `MedianCS.cppm` を `.cpp` に移行。3本文は LF 正規化後に Git HEAD と一致。source manifest / module-reference manifest と共通 MSVC IFC mapping 条件を更新。
- upstream clang++ 22.1.3 root `ArtifactCore` targetで3ソースの P1689 scan・compile、`ArtifactCore.lib` relink 成功。sccache cold rebuild は3 miss、warm rebuild は3 hit。cache error / compilation failure 0。
- clang-cl 22.1.3 fixture は実3 interfaces / implementation、Diligent / Qt / OpenCV headers、Image provider stubsで scan・compile・static link 成功。
- `check_module_hygiene.py` pass。対象 CMake / manifests に旧 `.cppm` path の残存なし。

### ArtifactCore DirectCompute wrapper batch: Scatter / SimpleChoker / Threshold（2026-10-23）

- `ScatterCS.cppm` / `SimpleChokerCS.cppm` / `ThresholdCS.cppm` を `.cpp` に移行。3本文は LF 正規化後に Git HEAD と一致。source manifest / module-reference manifest と共通 MSVC IFC mapping 条件を更新。
- upstream clang++ 22.1.3 root `ArtifactCore` targetで3ソースの P1689 scan・compile、`ArtifactCore.lib` relink 成功。sccache cold rebuild は3 miss、warm rebuild は3 hit。cache error / compilation failure 0。
- clang-cl 22.1.3 fixture は実3 interfaces / implementation、Diligent / Qt / OpenCV headers、Image provider stubsで scan・compile・static link 成功。
- `check_module_hygiene.py` pass。対象 CMake / manifests に旧 `.cppm` path の残存なし。

### ArtifactCore Audio DSP implementation batch: BassTreble / BitCrusher / DCBlocker（2026-10-23）

- `AudioBassTreble.cppm` / `AudioBitCrusher.cppm` / `AudioDCBlocker.cppm` を `.cpp` に移行。3本文は LF 正規化後に Git HEAD と一致。source manifest / module-reference manifest と CMake audio-source registration を更新。DSP処理、interface、module importsは変更していない。
- upstream clang++ 22.1.3 の `ArtifactCoreAudio` targetで3ソースの P1689 scan・compile、`ArtifactCoreAudio.lib` relink 成功。sccache cold rebuild は3 miss、warm rebuild は3 hit。cache error / compilation failure 0。
- clang-cl 22.1.3 fixture は実3 interfaces / implementation、`Audio.Effect` / `Audio.Segment` / `Core.ArtifactString` の実module群と Qt headers で scan・compile・static link 成功。
- `check_module_hygiene.py` pass。対象 CMake / manifests に旧 `.cppm` path の残存なし。

### ArtifactCore Audio dynamics batch: Compressor / DeEsser / Expander（2026-10-23）

- `AudioCompressor.cppm` / `AudioDeEsser.cppm` / `AudioExpander.cppm` を `.cpp` に移行。3本文は LF 正規化後に Git HEAD と一致。source manifest / module-reference manifest と CMake audio-source registration を更新。DSP処理、interface、module importsは変更していない。
- upstream clang++ 22.1.3 の `ArtifactCoreAudio` targetで3ソースの P1689 scan・compile、`ArtifactCoreAudio.lib` relink 成功。sccache cold rebuild は3 miss、warm rebuild は3 hit。cache error / compilation failure 0。
- clang-cl 22.1.3 fixture は実3 interfaces / implementation、`Audio.Effect` / `Audio.Segment` / `Core.ArtifactString` の実module群と Qt headers で scan・compile・static link 成功。
- `check_module_hygiene.py` pass。対象 CMake / manifests に旧 `.cppm` path の残存なし。

### ArtifactCore Audio reverb family batch: Reverb / PlateReverb / SpringReverb（2026-10-23）

- `AudioReverb.cppm` / `AudioPlateReverb.cppm` / `AudioSpringReverb.cppm` を `.cpp` に移行。3本文は LF 正規化後に Git HEAD と一致。source manifest / module-reference manifest と CMake audio-source registration を更新。DSP処理、interface、module importsは変更していない。
- upstream clang++ 22.1.3 の `ArtifactCoreAudio` targetで3ソースの P1689 scan・compile、`ArtifactCoreAudio.lib` relink 成功。sccache cold rebuild は3 miss、warm rebuild は3 hit。cache error / compilation failure 0。
- clang-cl 22.1.3 fixture は実3 interfaces / implementation、`Audio.Effect` / `Audio.Segment` / `Core.ArtifactString` の実module群と Qt headers で scan・compile・static link 成功。
- `check_module_hygiene.py` pass。対象 CMake / manifests に旧 `.cppm` path の残存なし。

### ArtifactCore Audio modulation batch: Chorus / Phaser / Vibrato（2026-10-23）

- `AudioChorus.cppm` / `AudioPhaser.cppm` / `AudioVibrato.cppm` を `.cpp` に移行。3本文は LF 正規化後に Git HEAD と一致。source manifest / module-reference manifest と CMake audio-source registration を更新。DSP処理、interface、module importsは変更していない。
- upstream clang++ 22.1.3 の `ArtifactCoreAudio` targetで3ソースの P1689 scan・compile、`ArtifactCoreAudio.lib` relink 成功。sccache cold rebuild は3 miss、warm rebuild は3 hit。cache error / compilation failure 0。
- clang-cl 22.1.3 fixture は実3 interfaces / implementation、`Audio.Effect` / `Audio.Segment` / `Core.ArtifactString` の実module群と Qt headers で scan・compile・static link 成功。
- `check_module_hygiene.py` pass。対象 CMake / manifests に旧 `.cppm` path の残存なし。


### ArtifactCore Audio filter and EQ implementation batch: HighLowPass / ParametricEQ（2026-10-23）

- `AudioHighLowPass.cppm` と `AudioParametricEQ.cppm` を `.cpp` に移行。両本文は LF 正規化後に Git HEAD と一致。source manifest / module-reference manifest と CMake audio-source registration を更新。DSP処理、interface、module importsは変更していない。
- upstream clang++ 22.1.3 root `ArtifactCoreAudio` targetで両ソースの P1689 scan・compile、`ArtifactCoreAudio.lib` relink 成功。
- clang-cl 22.1.3 fixture は実両 interfaces / implementations、`Container.NamedVector` と Debug / Registry providers を含め scan・compile・static link 成功。sccache cold compilation 14 misses、2回目は対象実装2件がhit。interface等の複数入力扱い22件はbypass。cache errors / compilation failures 0。
- `check_module_hygiene.py` pass。対象2 pathに旧 `.cppm` 参照なし。


### ArtifactCore Audio character processing batch: TapeSaturation / Tone / TransientShaper（2026-10-23）

- `AudioTapeSaturation.cppm` / `AudioTone.cppm` / `AudioTransientShaper.cppm` を `.cpp` に移行。3本文はLF正規化後にGit HEADと一致。source manifest / module-reference manifest とCMake audio-source registrationを更新。DSP処理、interface、module importsは変更していない。
- upstream clang++ 22.1.3 root `ArtifactCoreAudio` targetで3ソースのP1689 scan・compile、`ArtifactCoreAudio.lib` relink成功。
- clang-cl 22.1.3 fixtureは実3 interfaces / implementationsでscan・compile・static link成功。sccache cold build 3 misses、warm build 3 hits。cache errors / compilation failures 0。
- `check_module_hygiene.py` pass。対象3 pathに旧`.cppm`参照なし。


### ArtifactCore Audio delay implementation batch: Delay / PingPongDelay（2026-10-23）

- `AudioDelay.cppm` と `AudioPingPongDelay.cppm` を `.cpp` に移行。本文はLF正規化後にGit HEADと一致。source manifest / module-reference manifest とCMake audio-source registrationを更新。DSP処理、interface、module importsは変更していない。
- upstream clang++ 22.1.3 root `ArtifactCoreAudio` targetでP1689 scan・compile、`ArtifactCoreAudio.lib` relink成功。
- clang-cl 22.1.3 fixtureは両実interface / implementationと `Audio.RingBuffer` interfaceを含めscan・compile・static link成功。sccache cold build 2 misses、warm build 2 hits。cache errors / compilation failures 0。
- `check_module_hygiene.py` pass。対象2 pathに旧`.cppm`参照なし。


### ArtifactCore Audio channel utility batch: DownMixer / Panner（2026-10-23）

- `AudioDownMixer.cppm` と `AudioPanner.cppm` を `.cpp` に移行。本文はLF正規化後にGit HEADと一致。source manifest / module-reference manifest とCMake audio-source registrationを更新。アルゴリズム、interface、module importsは変更していない。
- upstream clang++ 22.1.3 root `ArtifactCoreAudio` targetでP1689 scan・compile、`ArtifactCoreAudio.lib` relink成功。
- clang-cl 22.1.3 fixtureは実両interfaces / implementationsでscan・compile・static link成功。sccache cold build 2 misses、warm build 2 hits。cache errors / compilation failures 0。
- `check_module_hygiene.py` pass。対象2 pathに旧`.cppm`参照なし。


### ArtifactCore Audio storage batch: Cache / RingBuffer（2026-10-23）

- `AudioCache.cppm` と `AudioRingBuffer.cppm` を `.cpp` に移行。本文はLF正規化後にGit HEADと一致。source manifest / module-reference manifest とCMake audio-source registrationを更新。実装動作、interface、module importsは変更していない。
- upstream clang++ 22.1.3 root `ArtifactCoreAudio` targetで両ソースのP1689 scan・compile、`ArtifactCoreAudio.lib` relink成功。
- clang-cl 22.1.3 fixtureは両実interfaces / implementationsでscan・compile・static link成功。sccache cold build 2 misses、warm build 2 hits。cache errors / compilation failures 0。
- `check_module_hygiene.py` pass。対象2 pathに旧`.cppm`参照なし。


### ArtifactCore Audio output utilities batch: Rasterizer / Writer（2026-10-23）

- `AudioRasterizer.cppm` と `AudioWriter.cppm` を `.cpp` に移行。本文はLF正規化後にGit HEADと一致。source manifest / module-reference manifest とCMake audio-source registrationを更新。処理内容、interfaces、module importsは変更していない。
- upstream clang++ 22.1.3 root `ArtifactCoreAudio` targetで両ソースのP1689 scan・compile、`ArtifactCoreAudio.lib` relink成功。
- clang-cl 22.1.3 fixtureは両実interfaces / implementationsでscan・compile・static link成功。sccache cold build 2 misses、warm build 2 hits。cache errors / compilation failures 0。
- `check_module_hygiene.py` pass。対象2 pathに旧`.cppm`参照なし。


### ArtifactCore Audio metering batch: Volume / Spectrum（2026-10-23）

- `AudioVolume.cppm` と `AudioSpectrum.cppm` を `.cpp` に移行。本文はLF正規化後にGit HEADと一致。source manifest / module-reference manifest とCMake audio-source registrationを更新。処理内容、interfaces、module importsは変更していない。
- upstream clang++ 22.1.3 root `ArtifactCoreAudio` targetで両ソースのP1689 scan・compile、`ArtifactCoreAudio.lib` relink成功。
- clang-cl 22.1.3 fixtureは実両interfaces / implementationsと `Audio.Decibels`、`Utils.String.UniString` / `StringLike` providersでscan・compile・static link成功。QtGui include pathもfixtureに必要だった。sccache cold build 2 misses、warm build 2 hits。cache errors / compilation failures 0。
- `check_module_hygiene.py` pass。対象2 pathに旧`.cppm`参照なし。


### ArtifactCore Audio mixer batch: Bus / Mixer（2026-10-23）

- `AudioBus.cppm` と `AudioMixer.cppm` を `.cpp` に移行。本文はLF正規化後にGit HEADと一致。source manifest / module-reference manifest とCMake audio-source registrationを更新。処理内容、interfaces、module importsは変更していない。
- upstream clang++ 22.1.3 root `ArtifactCoreAudio` targetで両ソースのP1689 scan・compile、`ArtifactCoreAudio.lib` relink成功。
- clang-cl 22.1.3 fixtureは両実interfaces / implementationsと `Utils.Id`、`Memory.SharedPtr` / `TrackedPtr`、音声effect / panner provider群でscan・compile・static link成功。sccache cold build 2 misses、warm build 2 hits。cache errors / compilation failures 0。
- `check_module_hygiene.py` pass。対象2 pathに旧`.cppm`参照なし。


### ArtifactCore Audio Windows backend batch: WASAPI / ASIOStub（2026-10-23）

- `WASAPIBackend.cppm` と `ASIOBackendStub.cppm` を `.cpp` に移行。本文はLF正規化後にGit HEADと一致。source manifest / module-reference manifest とCMake audio-source registrationを更新。backend実装とmodule importsは変更していない。
- upstream clang++ 22.1.3 root `ArtifactCoreAudio` targetで両ソースのP1689 scan・compile、`ArtifactCoreAudio.lib` relink成功。
- clang-cl 22.1.3 fixtureはWindows SDKを使い、実両interfaces / implementationsと `Audio.Backend` providerでscan・compile・static link成功。sccache cold build 2 misses、warm build 2 hits。cache errors / compilation failures 0。
- `check_module_hygiene.py` pass。対象2 pathに旧`.cppm`参照なし。


### ArtifactCore Audio spatial rendering batch: SpatialRenderer / SpeakerLayout（2026-10-23）

- `Audio/Spatial/SpatialRenderer.cppm` と `SpeakerLayout.cppm` を `.cpp` に移行。本文はLF正規化後にGit HEADと一致。ArtifactCore CMakeのAudio implementation source registrationを更新。DSP処理、interfaces、module importsは変更していない。
- upstream clang++ 22.1.3 root `ArtifactCoreAudio` targetで両ソースのP1689 scan・compile、`ArtifactCoreAudio.lib` relink成功。
- clang-cl 22.1.3 fixtureは両実interfaces / implementationsと `SpatialMath` / `SpatialParams` providersでscan・compile・static link成功。sccache cold build 2 misses、warm build 2 hits。cache errors / compilation failures 0。
- `check_module_hygiene.py` pass。CMake Audio source listに旧2 `.cppm` pathなし。


### ArtifactCore Video transition registrar batch: Cube / Doors / Flip（2026-10-23）

- `CubeTransition.cppm` / `DoorsTransition.cppm` / `FlipTransition.cppm` を `.cpp` に移行。本文はLF正規化後にGit HEADと一致。source manifest / module-reference manifest / CMake source registrationを更新。Cube用の既存MSVC IFC compile optionとobject dependency条件も `.cpp` pathへ追従。
- upstream clang++ 22.1.3 root `ArtifactCoreVideo` targetで3ソースのP1689 scan・compile、`ArtifactCoreVideo.lib` relink成功。
- clang-cl 22.1.3 fixtureは実3 interfacesと `NLE.Core` / frame / video providersでscan・compile・static link成功。sccache cold build 3 misses、warm build 3 hits。cache errors / compilation failures 0。
- `check_module_hygiene.py` pass。対象3 pathに旧`.cppm` CMake / manifest参照なし。


### ArtifactCore Video transition registrar batch: Dissolve / GradientWipe / IrisWipe（2026-10-23）

- `DissolveTransition.cppm` / `GradientWipeTransition.cppm` / `IrisWipeTransition.cppm` を `.cpp` に移行。本文は LF 正規化後に Git HEAD と一致。source manifest / module-reference manifest / CMake source registration を更新。処理内容、interfaces、module imports は変更していない。
- upstream clang++ 22.1.3 root `ArtifactCoreVideo` target で3ソースの P1689 scan・compile、`ArtifactCoreVideo.lib` relink 成功。
- clang-cl 22.1.3 fixture は実3 interfaces と `NLE.Core` / frame / video providers で scan・compile・static link 成功。sccache cold build 3 misses、warm build 3 hits。cache errors / compilation failures 0。
- `check_module_hygiene.py` pass。対象3 pathに旧 `.cppm` CMake / manifest 参照なし。


### ArtifactCore Video transition registrar batch: Slide / Zoom（2026-10-23）

- `SlideTransition.cppm` と `ZoomTransition.cppm` を `.cpp` に移行。両本文は LF 正規化後に Git HEAD と一致。source manifest / module-reference manifest / CMake source registration を更新。処理内容、interfaces、module imports は変更していない。
- upstream clang++ 22.1.3 root `ArtifactCoreVideo` target で両ソースの P1689 scan・compile、`ArtifactCoreVideo.lib` relink 成功。
- clang-cl 22.1.3 fixture は実両 interfaces / implementations と既存 transition providers で scan・compile・static link 成功。sccache cold build 2 misses、warm rebuild 2 hits。cache errors / compilation failures 0。module interface commands 2件は複数入力扱いでbypass。
- `check_module_hygiene.py` pass。対象2 pathに旧 `.cppm` CMake / manifest 参照なし。


### ArtifactCore Video transition registrar batch: LinearWipe / RadialWipe / LightLeak（2026-10-23）

- `LinearWipeTransition.cppm` / `RadialWipeTransition.cppm` / `LightLeakTransition.cppm` を `.cpp` に移行。3本文は LF 正規化後に Git HEAD と一致。source manifest / module-reference manifest / CMake source registration を更新。処理内容、interfaces、module imports は変更していない。
- upstream clang++ 22.1.3 root `ArtifactCoreVideo` target で3ソースの P1689 scan・compile、`ArtifactCoreVideo.lib` relink 成功。
- clang-cl 22.1.3 fixture は実3 interfaces / implementations と既存 transition providers で scan・compile・static link 成功。sccache cold compile 3 misses、以降の2回のソース再compileは各3 hits。最終統計は3 misses / 6 hits（各sourceの複数入力module interface commandは非cache）。cache errors / compilation failures 0。
- `check_module_hygiene.py` pass。対象3 pathに旧 `.cppm` CMake / manifest 参照なし。


### ArtifactCore Video transition registrar batch: Wipe / GlitchDisplace（2026-10-23）

- `WipeTransition.cppm` と `GlitchDisplaceTransition.cppm` を `.cpp` に移行。両本文は LF 正規化後に Git HEAD と一致。source manifest / module-reference manifest / CMake source registration を更新。registrar、interface、module imports は変更していない。
- upstream clang++ 22.1.3 root `ArtifactCoreVideo` target で両ソースの P1689 scan・compile、`ArtifactCoreVideo.lib` relink 成功。
- clang-cl 22.1.3 fixture は実両 interfaces / implementations と既存 transition providers で scan・compile・static link 成功。sccache cold build 2 misses、warm rebuild 2 hits。cache errors / compilation failures 0。module interface commands 2件は複数入力扱いでbypass。
- `check_module_hygiene.py` pass。対象2 pathに旧 `.cppm` CMake / manifest 参照なし。


### ArtifactCore Graphics creative effect batch: Halftone / Posterize（2026-10-23）

- `HalftoneEffect.cppm` と `PosterizeEffect.cppm` を `.cpp` に移行。両本文は LF 正規化後に Git HEAD と一致。source manifest / module-reference manifest / CMake source registration を更新。画像処理動作、interfaces、module imports は変更していない。
- upstream clang++ 22.1.3 は root Ninja build の両 object targetで P1689 scan・compile 成功。main `ArtifactCore` library 全体の link は今回実行していない。
- clang-cl 22.1.3 fixture は実両 interfaces / implementations と `Graphics.Effect.Creative` / `Channel` / `Core.Parallel` providersで scan・compile・static link 成功。sccache cold build 2 misses、warm rebuild 2 hits。cache errors / compilation failures 0。provider module commands 5件は複数入力扱いでbypass。
- `check_module_hygiene.py` pass。対象2 pathに旧 `.cppm` CMake / manifest 参照なし。


### ArtifactCore Graphics creative effect batch: ColorVibrance / Pixelate（2026-10-23）

- `ColorVibranceEffect.cppm` と `PixelateEffect.cppm` を `.cpp` に移行。両本文は LF 正規化後に Git HEAD と一致。source manifest / module-reference manifest / CMake source registration を更新。画像処理動作、interfaces、module imports は変更していない。
- upstream clang++ 22.1.3 は root Ninja build の両 object targetで P1689 scan・compile 成功。main `ArtifactCore` library 全体の link は今回実行していない。
- clang-cl 22.1.3 fixture は実両 interfaces / implementations と直近で追加した `Graphics.Effect.Creative` / `Channel` / `Core.Parallel` providersで scan・compile・static link 成功。sccache cold build 2 misses、warm rebuild 2 hits。cache errors / compilation failures 0。
- `check_module_hygiene.py` pass。対象2 pathに旧 `.cppm` CMake / manifest 参照なし。


### ArtifactCore Graphics geometric effect batch: Fisheye / Mirror / Kaleidoscope（2026-10-23）

- `FisheyeEffect.cppm` / `MirrorEffect.cppm` / `KaleidoscopeEffect.cppm` を `.cpp` に移行。3本文は LF 正規化後に Git HEAD と一致。source manifest / module-reference manifest を更新。画像処理動作、interfaces、module imports は変更していない。
- upstream clang++ 22.1.3 は root Ninja build の3 object targetで P1689 scan・compile 成功。main `ArtifactCore` library 全体の link は今回実行していない。
- clang-cl 22.1.3 fixture は実3 interfaces / implementations と既存 `Channel` / `Core.Parallel` providersで scan・compile・static link 成功。sccache cold build 3 misses、warm rebuild 3 hits。cache errors / compilation failures 0。
- `check_module_hygiene.py` pass。対象3 pathに旧 `.cppm` CMake / manifest 参照なし。


### ArtifactCore Graphics creative effect batch: EdgeEcho / LightPressure / TemporalFossil（2026-10-23）

- `EdgeEchoEffect.cppm` / `LightPressureEffect.cppm` / `TemporalFossilEffect.cppm` を `.cpp` に移行。3本文は LF 正規化後に Git HEAD と一致。source manifest / module-reference manifest を更新。効果処理、interfaces、module imports は変更していない。
- upstream clang++ 22.1.3 は root Ninja build の3 object targetで P1689 scan・compile 成功。main `ArtifactCore` library 全体の link は今回実行していない。
- clang-cl 22.1.3 fixture は実3 interfaces / implementations と既存 `Channel` / `Core.Parallel` providersで scan・compile・static link 成功。sccache cold build 3 misses、warm rebuild 3 hits。cache errors / compilation failures 0。
- `check_module_hygiene.py` pass。対象3 pathに旧 `.cppm` CMake / manifest 参照なし。


### ArtifactCore Graphics creative effect batch: ChromaticAberration / Emboss（2026-10-23）

- `ChromaticAberrationEffect.cppm` と `EmbossEffect.cppm` を `.cpp` に移行。両本文は LF 正規化後に Git HEAD と一致。source manifest / module-reference manifest を更新。画像処理動作、interfaces、module imports は変更していない。
- upstream clang++ 22.1.3 は root Ninja build の両 object targetで P1689 scan・compile 成功。main `ArtifactCore` library 全体の link は今回実行していない。
- clang-cl 22.1.3 fixture は実両 interfaces / implementations と既存 `Graphics.Effect.Creative` / `Channel` / `Core.Parallel` providersで scan・compile・static link 成功。sccache cold build 2 misses、warm rebuild 2 hits。cache errors / compilation failures 0。
- `check_module_hygiene.py` pass。対象2 pathに旧 `.cppm` CMake / manifest 参照なし。


### ArtifactCore Graphics creative effect batch: DepthMelt / Glitch / OldTV（2026-10-23）

- `DepthMeltEffect.cppm` / `GlitchCreativeEffect.cppm` / `OldTVEffect.cppm` を `.cpp` に移行。3本文は LF 正規化後に Git HEAD と一致。source manifest / module-reference manifest を更新。効果処理、interfaces、module imports は変更していない。
- upstream clang++ 22.1.3 は root Ninja build の3 object targetで P1689 scan・compile 成功。main `ArtifactCore` library 全体の link は今回実行していない。
- clang-cl 22.1.3 fixture は実3 interfaces / implementations と `Channel` / `Core.Parallel` / `Math.Noise` providersで scan・compile・static link 成功。sccache cold build 3 misses、warm rebuild 3 hits。cache errors / compilation failures 0。provider interface commands 4件は複数入力扱いでbypass。
- `check_module_hygiene.py` pass。対象3 pathに旧 `.cppm` CMake / manifest 参照なし。


### ArtifactCore Graphics creative effect batch: PigmentSeparation / SurfaceMemory（2026-10-23）

- `PigmentSeparationEffect.cppm` と `SurfaceMemoryEffect.cppm` を `.cpp` に移行。両本文は LF 正規化後に Git HEAD と一致。source manifest / module-reference manifest を更新。効果処理、interfaces、module imports は変更していない。
- upstream clang++ 22.1.3 は root Ninja build の両 object targetで P1689 scan・compile 成功。main `ArtifactCore` library 全体の link は今回実行していない。
- clang-cl 22.1.3 fixture は実両 interfaces / implementations と既存 `Channel` / `Core.Parallel` / `Math.Noise` providersで scan・compile・static link 成功。sccache cold build 2 misses、warm rebuild 2 hits。cache errors / compilation failures 0。
- `check_module_hygiene.py` pass。対象2 pathに旧 `.cppm` CMake / manifest 参照なし。

### ArtifactCore Transform batch: Camera / Light（2026-10-23）

- `Camera.cppm` と `Light.cppm` を `.cpp` に移行。両本文は LF 正規化後に Git HEAD と一致。source manifest / module-reference manifest / CMake source registration と、MSVC IFC compile-option の対象パスを更新。挙動、interfaces、module imports は変更していない。
- upstream clang++ 22.1.3 root Ninja build で両 object targetおよび関連 interface modules の P1689 scan・compile 成功。main `ArtifactCore` library 全体の link は今回実行していない。
- clang-cl 22.1.3 fixture は実 `Float3` / `Math.Vec` / `Camera` / `Light` modulesと両 implementation を compile・static link 済み。GLM experimental define を product target と合わせて指定。fixture の最後の warm build は既に最新で no-op、sccache は直前の実コンパイル2件を hit と記録。cache errors / compilation failures 0。
- `check_module_hygiene.py` pass、日付 audit は missing / invalid 0。対象2 pathに旧 `.cppm` CMake / manifest 参照なし。


### ArtifactCore Transform coordinate batch: Scale2D / ViewportTransformer（2026-10-23）

- `Scale2D.cppm` と `ViewportTransformer.cppm` を `.cpp` に移行。source manifest / module-reference manifest を更新し、ViewportTransformer の MSVC IFC compile-option と Logger object dependency 条件も新パスに追従。本文の処理・module imports・public interfaces は変更していない。
- upstream clang++ 22.1.3 root Ninja build で両 source の P1689 scan と object compile が成功。再構成により Camera / Light も再スキャンされたが変更なしで成功。ArtifactCore library 全体の link は未実施。
- clang-cl fixture は今回の2 module を含まず、clang-cl 検証は未実施。次回 fixture に module providers を加えたうえで確認する。
- `check_module_hygiene.py` pass。CMake / source manifest / module-reference manifest に旧2 `.cppm` pathなし。


### ArtifactCore Graphics compute batch: Histogram / LUT3DComputer（2026-10-23）

- `Histogram.cppm` と `LUT3DComputer.cppm` を `.cpp` に移行。本文は LF 正規化後に Git HEAD と一致。source manifest / module-reference manifest / CMake source registration を更新。LUT3DComputer の MSVC IFC reference options、HLSL interface object dependency、および private-source 登録コメント／パスも追従。
- upstream clang++ 22.1.3 root Ninja build で両 implementation の P1689 scan・compile 成功。ArtifactCore library 全体の link は未実施。
- clang-cl fixture ではこの GPU module dependency closure を構成していないため、本 batch の clang-cl compile/link は未確認。
- `check_module_hygiene.py` pass。CMake / source manifest / module-reference manifest に旧2 `.cppm` pathなし。


### ArtifactCore Thread / Render farm batch: PreciseTicker / ProgressAggregator（2026-10-23）

- `PreciseTicker.cppm` と `ProgressAggregator.cppm` を `.cpp` に移行。本文は LF 正規化後に Git HEAD と一致。source manifest / module-reference manifest と、CMake の module source / compile option / object dependency path を更新。実装内容と interface は変更していない。
- upstream clang++ 22.1.3 root Ninja build で両実装の P1689 scan・compile 成功。
- clang-cl 22.1.3 fixture に実 interface と `Render.Farm.Types` / `Utils.Id` provider を追加し、両実装の compile・static link 成功。cold sccache 2 misses、touch 後の warm rebuild 2 hits。cache errors / compilation failures 0。interface multi-input compile 3件は非 cacheable。
- `check_module_hygiene.py` pass。CMake / source manifest / module-reference manifest に旧2 `.cppm` pathなし。


### ArtifactCore Core parallel runtime batch: Parallel（2026-10-23）

- `Core/Parallel.cppm` を `.cpp` に移行。本文は LF 正規化後に Git HEAD と一致。source manifest / module-reference manifest と MSVC IFC reference condition を更新。実装・interface・TBB 呼び出しは変更していない。
- upstream clang++ 22.1.3 root Ninja build で object compile 成功。
- clang-cl 22.1.3 fixture（既存 `Common/Parallel.ixx` interface を使用）で compile・static link 成功。cold sccache 1 miss、touch 後 warm rebuild 1 hit。cache errors / compilation failures 0。
- `check_module_hygiene.py` pass。CMake / source manifest / module-reference manifest に旧 `Core/Parallel.cppm` pathなし。


### ArtifactCore IPC batch: IPCChannel / RenderFarmSharedBuffer（2026-10-23）

- `IPCChannel.cppm` と `RenderFarmSharedBuffer.cppm` を `.cpp` に移行。本文は LF 正規化後に Git HEAD と一致。source manifest / module-reference manifest / IPC target source lists / MSVC IFC options と object dependencies を新パスへ更新。通信処理・public interfaces・module imports は変更していない。
- upstream clang++ 22.1.3 root Ninja build で両実装および `IPCChannel` / `RenderFarmSharedBuffer` / `SharedMemoryRingBuffer` interface modules の P1689 scan・compile 成功。ArtifactCore library 全体の link は未実施。
- clang-cl 22.1.3 fixture に `IPCChannel` と IPC interfaces を加え、compile・static link 成功。cold fixture rebuild は既存 broad fixture 178 compile requests 中 70 hits / 1 miss、107 non-cacheable module calls、失敗0。touch 後の warm `IPCChannel` rebuild は1 hit。`RenderFarmSharedBuffer` は画像 module dependency closure のため clang-cl fixture 未確認。
- `check_module_hygiene.py` pass。CMake / source manifest / module-reference manifest に旧2 `.cppm` pathなし。


### ArtifactCore Render farm support batch: LogCollector / RenderJobModel（2026-10-23）

- `LogCollector.cppm` と `RenderJobModel.cppm` を `.cpp` に移行。本文は LF 正規化後に Git HEAD と一致。source manifest / module-reference manifest と MSVC IFC compile-option / object dependency source paths を更新。実装・public interface・module imports は変更していない。
- upstream clang++ 22.1.3 root Ninja build で両実装の P1689 scan・compile 成功。
- clang-cl 22.1.3 fixture に実両 interfaces を追加し、compile・static link 成功。cold fixture rebuild は broad rebuild 182 compile requests 中 71 hits / 2 misses、109 non-cacheable module calls、失敗0。touch 後 warm rebuild は2 hits、cache errors 0。
- `check_module_hygiene.py` pass。CMake / source manifest / module-reference manifest に旧2 `.cppm` pathなし。


### ArtifactCore Image boundary batch: GpuImageUpload / FFmpegAudioEncoder（2026-10-23）

- `GpuImageUpload.cppm` と `FFmpegAudioEncoder.cppm` を `.cpp` に移行。本文は LF 正規化後に Git HEAD と一致。source manifest / module-reference manifest と FFmpegAudioEncoder の MSVC IFC source reference を更新。public interfaces・実装・module imports は変更していない。
- upstream clang++ 22.1.3 root Ninja build で両実装の P1689 scan・compile 成功。
- clang-cl 22.1.3 fixture に実 interfaces と ImageSurfaceView / SurfaceColorContract / Color.TransferFunction providers を加えて compile・static link 成功。cold sccache 2 misses、touch 後 warm rebuild 2 hits。cache errors / compilation failures 0。FFmpegAudioEncoder では既存の FFmpeg deprecated `sample_fmts` 4件と nodiscard `avformat_write_header` 1件の warning が出たが compile/link は成功。
- `check_module_hygiene.py` pass。CMake / source manifest / module-reference manifest に旧2 `.cppm` pathなし。


### ArtifactCore Image processing/cache batch: ImageYUV420 / ImageF32x4_With_Cache（2026-10-23）

- `ImageYUV420.cppm` と `ImageF32x4_With_Cache.cppm` を `.cpp` に移行。本文は LF 正規化後に Git HEAD と一致。source manifest / module-reference manifest と CMake の MSVC IFC source option paths を更新。YUV420 の existing `ImageF32x4_RGBA.ixx` object dependency は維持。実装・interface・module imports は変更していない。
- upstream clang++ 22.1.3 root Ninja build で両実装の P1689 scan・compile 成功。
- clang-cl 22.1.3 fixture では `ImageYUV420` と its image/color/parallel interface dependency closure を compile し static link 成功。fixture 初回は `Color.Float` provider が欠けて失敗したため provider `FloatColor.ixx` を追加し、再実行で成功。warm YUV420 rebuild は sccache 1 hit、cache errors / compilation failures 0。OpenCV FLANN headers から既存 CRT deprecation warnings 4件。
- `ImageF32x4_With_Cache` も clang-cl 22.1.3 fixture に Diligent Core / GraphicsEngine / Win32 include paths、`PLATFORM_WIN32=1`、wobject provider を加えて compile・static link 成功。2つの image implementation object の強制 warm rebuild は sccache 2 hits、miss/error/failure 0。OpenCV FLANN headers 由来の CRT deprecation warnings 4件は YUV420 側に出た。
- `check_module_hygiene.py` pass。CMake / source manifest / module-reference manifest に旧2 `.cppm` pathなし。

### ArtifactCore GPU compute implementation batch: ScopeComputer / ProceduralTexture（2026-10-23）

- `Graphics/Compute/ScopeComputer.cppm` と `ImageProcessing/ProceduralTexture.cppm` を `.cpp` に移行。両本文は LF 正規化後に Git HEAD と一致。source manifest / module-reference manifest と ProceduralTexture の MSVC IFC source condition / OBJECT_DEPENDS path を更新。module interface、shader dispatch、実装ロジックは変更していない。
- upstream clang++ 22.1.3 root `ArtifactCore` build で両実装の P1689 scan・compile 成功し、`ArtifactCore.lib` を再 link。
- clang-cl root build は CMake regeneration 後に OpenUSD を含む約2,000 action の rebuild へ広がったため、対象の compile 前に中断。clang-cl の対象2実装は未確認。
- `check_module_hygiene.py` pass。対象 CMake / manifests に旧 `.cppm` path の残存なし。
### ArtifactCore color math batch: ColorBlendMode / ColorSpace（2026-10-23）

- `Color/ColorBlendMode.cppm` と `Color/ColorSpace.cppm` を `.cpp` に移行。両本文は LF 正規化後に Git HEAD と一致。source manifest / module-reference manifest を更新し、MSVC IFC mapping と ColorSpace の `ColorGamutConversion.ixx` object dependency 条件も `.cpp` に変更。色演算と公開 interface は変更していない。
- upstream clang++ 22.1.3 root `ArtifactCore` build で両実装の P1689 scan・compile 成功、`ArtifactCore.lib` relink 成功。
- 対象2ファイルの warm rebuild は sccache 2 hits、miss / cache error / compilation failure 0。
- `check_module_hygiene.py` pass。対象 CMake / manifests に旧 `.cppm` path の残存なし。
### ArtifactCore color adjustment batch: AutoColorMatch / ColorLUT（2026-10-23）

- `Color/AutoColorMatch.cppm` と `Color/ColorLUT.cppm` を `.cpp` に移行。両本文は LF 正規化後に Git HEAD と一致。source manifest / module-reference manifest と AutoColorMatch の MSVC IFC mapping condition、ColorLUT の MSVC IFC mapping / NamedVector object dependency paths を更新。既存の画像・色処理と Qt boundary は変更していない。
- upstream clang++ 22.1.3 root `ArtifactCore` build で両実装の P1689 scan・compile 成功し、`ArtifactCore.lib` を relink。
- 2実装の warm rebuild は sccache 2 hits、miss / cache error / compilation failure 0。
- `check_module_hygiene.py` pass。対象 CMake / manifests に旧 `.cppm` path の残存なし。
### ArtifactCore Shape implementation batch: ShapeGroup / ShapeLayer / ShapePath（2026-10-23）

- `Shape/ShapeGroup.cppm`、`ShapeLayer.cppm`、`ShapePath.cppm` を `.cpp` に移行。3本文は LF 正規化後に Git HEAD と一致。source manifest と独立 `ArtifactCoreShape` target source list を更新。partition interfaces と描画・シリアライズ処理は変更していない。
- upstream clang++ 22.1.3 の `ArtifactCoreShape` target で3実装の P1689 scan・compile、`ArtifactCoreShape.lib` link 成功。warm rebuild は sccache 3 hits、miss / cache error / compilation failure 0。
- `check_module_hygiene.py` pass。対象 CMake / source manifest に旧3 `.cppm` path の残存なし。
### ArtifactCore animation transform batch: AnimatableTransform2D / AnimatableTransform3D（2026-10-23）

- `Animation/AnimatableTransform2D.cppm` と `AnimatableTransform3D.cppm` を `.cpp` に移行。両本文は LF 正規化後に Git HEAD と一致。source manifest / module-reference manifest と MSVC IFC source conditions / explicit object ordering dependencies を `.cpp` path に更新。アニメーション計算と公開 interfaces は変更していない。
- upstream clang++ 22.1.3 root `ArtifactCore` build で両 implementation の P1689 scan・compile 成功、`ArtifactCore.lib` relink 成功。
- 2実装の warm rebuild は sccache 2 hits、miss / cache error / compilation failure 0。
- `check_module_hygiene.py` pass。対象 CMake / manifests に旧2 `.cppm` path の残存なし。

### ArtifactCore text rasterization/layout batch: GlyphAtlas / GlyphLayout（2026-10-23）

- `Text/GlyphAtlas.cppm` と `Text/GlyphLayout.cppm` を `.cpp` に移行。両本文は LF 正規化後に Git HEAD と一致。source manifest / module-reference manifest と `ArtifactCoreText` source paths を更新。glyph rasterization / layout behavior は変更していない。
- upstream clang++ 22.1.3 `ArtifactCoreText` target で両実装の P1689 scan・compile、`ArtifactCoreText.lib` link が成功。
- warm rebuild は sccache 2 hits、miss / cache error / compilation failure 0。
- `check_module_hygiene.py` pass。対象 CMake / manifests に旧2 `.cppm` path の残存なし。

### ArtifactCore shader source batch: BasicGeometryShader / BasicShaders / BasicVertexShader（2026-10-23）

- `Graphics/Shader/BasicGeometryShader.cppm`、`BasicShaders.cppm`、`BasicVertexShader.cppm` を `.cpp` に移行。各本文は LF 正規化後に Git HEAD と一致。source manifest / module-reference manifest と `ArtifactCore/CMakeLists.txt` の3件専用 IFC reference 条件を `.cpp` path に更新。shader source behavior は変更していない。
- upstream clang++ 22.1.3 `ArtifactCore` target で3実装の P1689 scan・compile と `ArtifactCore.lib` link が成功。
- warm rebuild は sccache 3 hits、miss / cache error / compilation failure 0。
- `check_module_hygiene.py` pass。対象 CMake / manifests に旧3 `.cppm` path の残存なし。

### ArtifactCore compute shader pipeline batch: GPUCompressionPipeline / MaskCutoutPipeline / MaskPathRasterizerPipeline（2026-10-23）

- `Graphics/Shader/Compute/GPUCompressionPipeline.cppm`、`MaskCutoutPipeline.cppm`、`MaskPathRasterizerPipeline.cppm` を `.cpp` に移行。各本文は LF 正規化後に Git HEAD と一致。source manifest / module-reference manifest と `ArtifactCore/CMakeLists.txt` の IFC compile options・object dependency 条件を `.cpp` path に更新。pipeline behavior は変更していない。
- upstream clang++ 22.1.3 `ArtifactCore` target で3実装の P1689 scan・compile と `ArtifactCore.lib` link が成功。
- warm rebuild は sccache 3 hits、miss / cache error / compilation failure 0。
- `check_module_hygiene.py` pass。対象 CMake / manifests に旧3 `.cppm` path の残存なし。

### ArtifactCore render farm batch: CheckpointStore / RenderFarmMaster / RenderFarmWorker（2026-10-23）

- `Render/CheckpointStore.cppm`、`RenderFarmMaster.cppm`、`RenderFarmWorker.cppm` を `.cpp` に移行。各本文は LF 正規化後に Git HEAD と一致。source manifest / module-reference manifest、3件の IFC compile-option 条件と RenderFarmMaster の object dependency を `.cpp` path に更新。render farm / checkpoint behavior は変更していない。
- upstream clang++ 22.1.3 `ArtifactCore` target で3実装の P1689 scan・compile と `ArtifactCore.lib` link が成功。
- warm rebuild は sccache 3 hits、miss / cache error / compilation failure 0。
- `check_module_hygiene.py` pass。対象 CMake / manifests に旧3 `.cppm` path の残存なし。

### ArtifactCore 2D physics batch: FluidSolver2D / Physics2D / SandSim2D（2026-10-23）

- `Physics/FluidSolver2D.cppm`、`Physics2D.cppm`、`SandSim2D.cppm` を `.cpp` に移行。各本文は LF 正規化後に Git HEAD と一致。source manifest / module-reference manifest、Physics2D / FluidSolver2D の IFC・object dependency 条件と SandSim2D の明示 CORE_IMPL 登録を `.cpp` path に更新。solver behavior は変更していない。
- upstream clang++ 22.1.3 `ArtifactCore` target で3実装の P1689 scan・compile と `ArtifactCore.lib` link が成功。
- warm rebuild は sccache 3 hits、miss / cache error / compilation failure 0。
- `check_module_hygiene.py` pass。対象 CMake / manifests に旧3 `.cppm` path の残存なし。

### ArtifactCore image core batch: ImageF32x4_RGBA / OpenEXR（2026-10-23）

- `Image/ImageF32x4_RGBA.cppm` と `Image/OpenEXR.cppm` を `.cpp` に移行。各本文は LF 正規化後に Git HEAD と一致。source manifest / module-reference manifest と ArtifactCore の compile-option / scan-control / MSVC IFC・object dependency 条件を `.cpp` path に更新。pixel storage / OpenEXR processing behavior は変更していない。
- upstream clang++ 22.1.3 `ArtifactCore` target で両実装の P1689 scan・compile と `ArtifactCore.lib` link が成功。
- warm rebuild は sccache 2 hits、miss / cache error / compilation failure 0。
- `check_module_hygiene.py` pass。対象 CMake / manifests に旧2 `.cppm` path の残存なし。

### ArtifactCore geometry batch: Fracture / MeshDeform（2026-10-23）

- `Geometry/Fracture.cppm` と `MeshDeform.cppm` を `.cpp` に移行。各本文は LF 正規化後に Git HEAD と一致。source manifest / module-reference manifest、両実装の IFC compile-option 条件と Fracture の object dependency を `.cpp` path に更新。geometry processing behavior は変更していない。
- upstream clang++ 22.1.3 `ArtifactCore` target で両実装の P1689 scan・compile と `ArtifactCore.lib` link が成功。
- warm rebuild は sccache 2 hits、miss / cache error / compilation failure 0。
- `check_module_hygiene.py` pass。対象 CMake / manifests に旧2 `.cppm` path の残存なし。

### ArtifactCore atmospheric field batch: AtmosphereFog / NoiseField（2026-10-23）

- `Render/AtmosphereFog.cppm` と `NoiseField.cppm` を `.cpp` に移行。各本文は LF 正規化後に Git HEAD と一致。source manifest / module-reference manifest と `ArtifactCore/CMakeLists.txt` の IFC compile-option 条件を `.cpp` path に更新。fog / noise field behavior は変更していない。
- upstream clang++ 22.1.3 `ArtifactCore` target で両実装の P1689 scan・compile と `ArtifactCore.lib` link が成功。
- warm rebuild は sccache 2 hits、miss / cache error / compilation failure 0。
- `check_module_hygiene.py` pass。対象 CMake / manifests に旧2 `.cppm` path の残存なし。

### ArtifactCore mask geometry batch: PathMorph / VolumeMask（2026-10-23）

- `Mask/PathMorph.cppm` と `VolumeMask.cppm` を `.cpp` に移行。各本文は LF 正規化後に Git HEAD と一致。source manifest / module-reference manifest と `ArtifactCore/CMakeLists.txt` の IFC compile-option 条件を `.cpp` path に更新。mask geometry behavior は変更していない。
- upstream clang++ 22.1.3 `ArtifactCoreMask` target で両実装の P1689 scan・compile と `ArtifactCoreMask.lib` link が成功。
- warm rebuild は sccache 2 hits、miss / cache error / compilation failure 0。
- `check_module_hygiene.py` pass。対象 CMake / manifests に旧2 `.cppm` path の残存なし。

### ArtifactCore layer core batch: Layer2D / LayerMatte（2026-10-23）

- `Layer/Layer2D.cppm` と `LayerMatte.cppm` を `.cpp` に移行。各本文は LF 正規化後に Git HEAD と一致。source manifest / module-reference manifest と `ArtifactCore/CMakeLists.txt` の IFC compile-option・object dependency 条件を `.cpp` path に更新。layer / matte behavior は変更していない。
- upstream clang++ 22.1.3 `ArtifactCore` target で両実装の P1689 scan・compile と `ArtifactCore.lib` link が成功。
- warm rebuild は sccache 2 hits、miss / cache error / compilation failure 0。
- `check_module_hygiene.py` pass。対象 CMake / manifests に旧2 `.cppm` path の残存なし。

### ArtifactCore event diagnostics batch: EventBus / EventBusDebugger（2026-10-23）

- `Event/EventBus.cppm` と `EventBusDebugger.cppm` を `.cpp` に移行。各本文は LF 正規化後に Git HEAD と一致。source manifest / module-reference manifest、EventBus の `CORE_IMPL` 振り分けと両ソースの IFC compile-option / object dependency 条件を `.cpp` path に更新。event dispatch / diagnostics behavior は変更していない。
- upstream clang++ 22.1.3 で `ArtifactCore` と `ArtifactCoreEvent` targets の P1689 scan・compile、および `ArtifactCore.lib` / `ArtifactCoreEvent.lib` links が成功。
- warm rebuild は sccache 2 hits、miss / cache error / compilation failure 0。
- `check_module_hygiene.py` pass。対象 CMake / manifests に旧2 `.cppm` path の残存なし。

### ArtifactCore composition model batch: CompositionBuffer2D / ParametricComposition（2026-10-23）

- `Composition/CompositionBuffer2D.cppm` と `ParametricComposition.cppm` を `.cpp` に移行。各本文は LF 正規化後に Git HEAD と一致。source manifest / module-reference manifest と `ArtifactCore/CMakeLists.txt` の implementation source paths を更新し、類似名の `OpenCVCompositionBuffer2D.cppm` は変更対象から除外した。composition buffer / parametric composition behavior は変更していない。
- upstream clang++ 22.1.3 `ArtifactCoreComposition` target で両実装の P1689 scan・compile と `ArtifactCoreComposition.lib` link が成功。
- warm rebuild は sccache 2 hits、miss / cache error / compilation failure 0。
- `ParametricComposition.cpp` では既存の Qt `QCryptographicHash::addData` deprecated warning 5件を確認。`check_module_hygiene.py` pass、対象 CMake / manifests に旧2 `.cppm` path の残存なし。

### ArtifactCore UI selection/navigation batch: SelectionManager / ViewOrientationNavigator（2026-10-23）

- `UI/SelectionManager.cppm` と `ViewOrientationNavigator.cppm` を `.cpp` に移行。各本文は LF 正規化後に Git HEAD と一致。source manifest / module-reference manifest と `ArtifactCore/CMakeLists.txt` の IFC compile-option・object dependency 条件を `.cpp` path に更新。selection / orientation behavior は変更していない。
- upstream clang++ 22.1.3 `ArtifactCoreUI` target で両実装の P1689 scan・compile と `ArtifactCoreUI.lib` link が成功。
- warm rebuild は sccache 2 hits、miss / cache error / compilation failure 0。
- `check_module_hygiene.py` pass。対象 CMake / manifests に旧2 `.cppm` path の残存なし。

### ArtifactCore video codec batch: FFmpegEncoder / FFMpegVideoDecoder（2026-10-23）

- `Image/FFmpegEncoder.cppm` と `Codec/FFMpegVideoDecoder.cppm` を `.cpp` に移行。各本文は LF 正規化後に Git HEAD と一致。source manifest / module-reference manifest と `ArtifactCore/CMakeLists.txt` の video target source / IFC compile-option 条件を `.cpp` path に更新。FFmpeg encode/decode behavior は変更していない。
- upstream clang++ 22.1.3 `ArtifactCoreVideo` target で両実装の P1689 scan・compile と `ArtifactCoreVideo.lib` link が成功。
- warm rebuild は sccache 2 hits、miss / cache error / compilation failure 0。
- `check_module_hygiene.py` pass。対象 CMake / manifests に旧2 `.cppm` path の残存なし。

### ArtifactCore tracking batch: MotionTracker / PlanarTracker（2026-10-23）

- `Tracking/MotionTracker.cppm` と `PlanarTracker.cppm` を `.cpp` に移行。各本文は LF 正規化後に Git HEAD と一致。source manifest / module-reference manifest と `ArtifactCore/CMakeLists.txt` の tracking target / IFC / object dependency path を `.cpp` に更新。tracking behavior は変更していない。
- upstream clang++ 22.1.3 `ArtifactCoreTracking` target で両実装の P1689 scan・compile と `ArtifactCoreTracking.lib` link が成功。
- warm rebuild は sccache 2 hits、miss / cache error / compilation failure 0。
- `check_module_hygiene.py` pass。対象 CMake / manifests に旧2 `.cppm` path の残存なし。
### ArtifactCore Core value batch: KeyFrame / Opacity（2026-10-23）

- `Core/KeyFrame.cppm` と `Core/Opacity.cppm` を `.cpp` に移行。両本文は LF 正規化後に Git HEAD と一致。source manifest / module-reference manifest と MSVC IFC source mapping conditions を `.cpp` path に更新。keyframe / opacity behavior と module interfaces は変更していない。
- upstream clang++ 22.1.3 root `ArtifactCore` build で両implementation の P1689 scan・compile 成功し、`ArtifactCore.lib` を relink。
- 2実装の warm rebuild は sccache 2 hits、miss / cache error / compilation failure 0。
- `check_module_hygiene.py` pass。対象 CMake / manifests に旧2 `.cppm` path の残存なし。
### ArtifactCore media import batch: ImageSequenceSource / MediaProbe（2026-10-23）

- `Media/ImageSequenceSource.cppm` と `Media/MediaProbe.cppm` を `.cpp` に移行。両本文は LF 正規化後に Git HEAD と一致。source manifest / module-reference manifest、MSVC IFC mappings と ImageSequenceSource の interface object dependencies を `.cpp` path に更新。画像連番読み込み・probe 処理は変更していない。
- upstream clang++ 22.1.3 `ArtifactCoreMedia` target で両実装の P1689 scan・compile 成功、`ArtifactCoreMedia.lib` link 成功。
- 対象2実装の warm rebuild は sccache 2 hits、miss / cache error / compilation failure 0。
- `check_module_hygiene.py` pass。対象 CMake / manifests に旧2 `.cppm` path の残存なし。
### ArtifactCore media source batch: MediaSource / MediaReader（2026-10-23）

- `Media/MediaSource.cppm` と `Media/MediaReader.cppm` を `.cpp` に移行。両本文は LF 正規化後に Git HEAD と一致。source manifest / module-reference manifest と MSVC IFC mapping conditions を `.cpp` path に更新。MediaSource / MediaReader interface と decoding flow は変更していない。
- upstream clang++ 22.1.3 `ArtifactCoreMedia` target で両実装の P1689 scan・compile 成功、`ArtifactCoreMedia.lib` link 成功。
- 2実装の warm rebuild は sccache 2 hits、miss / cache error / compilation failure 0。
- `check_module_hygiene.py` pass。対象 CMake / manifests に旧2 `.cppm` path の残存なし。
### ArtifactCore media decoder batch: MediaAudioDecoder / MediaImageFrameDecoder（2026-10-23）

- `Media/MediaAudioDecoder.cppm` と `MediaImageFrameDecoder.cppm` を `.cpp` に移行。両本文は LF 正規化後に Git HEAD と一致。source manifest / module-reference manifest、MSVC IFC mappings と MediaAudioDecoder の Container debug object dependencies を `.cpp` path に更新。デコード処理と公開 interfaces は変更していない。
- upstream clang++ 22.1.3 `ArtifactCoreMedia` target で両実装の P1689 scan・compile 成功、`ArtifactCoreMedia.lib` link 成功。
- 2実装の warm rebuild は sccache 2 hits、miss / cache error / compilation failure 0。
- `check_module_hygiene.py` pass。対象 CMake / manifests に旧2 `.cppm` path の残存なし。
### ArtifactCore media controller batch: MediaPlaybackController / MediaEditorController（2026-10-23）

- `Media/MediaPlaybackController.cppm` と `MediaEditorController.cppm` を `.cpp` に移行。両本文は LF 正規化後に Git HEAD と一致。source manifest / module-reference manifest と複数 IFC mapping を `.cpp` path に更新。controller behavior と public interfaces は変更していない。
- upstream clang++ 22.1.3 `ArtifactCoreMedia` target で両実装の P1689 scan・compile 成功、`ArtifactCoreMedia.lib` link 成功。
- 2実装の warm rebuild は sccache 2 hits、miss / cache error / compilation failure 0。
- `check_module_hygiene.py` pass。対象 CMake / manifests に旧2 `.cppm` path の残存なし。
### ArtifactCore playback / time remap batch: PlaybackClock / TimeRemap（2026-10-23）

- `Playback/PlaybackClock.cppm` と `Time/TimeRemap.cppm` を `.cpp` に移行。両本文は LF 正規化後に Git HEAD と一致。source manifest / module-reference manifest、Playback target source list、TimeRemap の Analyze IFC mappings と OptifalFlowResult object dependency を `.cpp` path に更新。transport ownership / playback state / time-remap processing は変更していない。
- upstream clang++ 22.1.3 の `ArtifactCorePlayback` と `ArtifactCoreAnalyze` targets で両実装の P1689 scan・compile と各 static library link が成功。
- 2実装の warm rebuild は sccache 2 hits、miss / cache error / compilation failure 0。
- `check_module_hygiene.py` pass。対象 CMake / manifests に旧2 `.cppm` path の残存なし。
### ArtifactCore IO / IPC boundary batch: AsyncImageWriterManager / SharedMemoryRingBuffer（2026-10-23）

- `IO/Image/AsyncImageWriterManager.cppm` と `IPC/SharedMemoryRingBuffer.cppm` を `.cpp` に移行。両本文は LF 正規化後に Git HEAD と一致。source manifest / module-reference manifest、AsyncImageWriterManager の MSVC IFC mapping と IPC target source list を更新。writer queue / shared-memory ring behavior は変更していない。
- upstream clang++ 22.1.3 `ArtifactCore` と `ArtifactCoreIPC` targets で両実装の P1689 scan・compile、static library links が成功。
- 対象2実装の warm rebuild は sccache 2 hits、miss / cache error / compilation failure 0。
- `check_module_hygiene.py` pass。対象 CMake / manifests に旧2 `.cppm` path の残存なし。

### ArtifactCore Media Foundation codec batch: MFEncoder / MFFrameExtractor（2026-10-23）

- `Codec/MFEncoder.cppm` と `Codec/MFFrameExtractor.cppm` を `.cpp` に移行。両本文は LF 正規化後に Git HEAD と一致。source manifest / module-reference manifest と `ArtifactCore/CMakeLists.txt` の Media Foundation source / IFC mappings を `.cpp` path に更新。encode / frame extraction behavior と公開 interfaces は変更していない。
- upstream clang++ 22.1.3 `ArtifactCore` target で両実装の P1689 scan・compile と `ArtifactCore.lib` link が成功。
- 2実装の warm rebuild は sccache 2 hits、miss / cache error / compilation failure 0。
- `check_module_hygiene.py` pass。対象 CMake / manifests に旧2 `.cppm` path の残存なし。

### ArtifactCore external control batch: MidiInput / OscInput（2026-10-23）

- `Control/MidiInput.cppm` と `Control/OscInput.cppm` を `.cpp` に移行。両本文は LF 正規化後に Git HEAD と一致。source manifest / module-reference manifest と `ArtifactCore/CMakeLists.txt` の Control source / MSVC IFC・object dependency mappings を `.cpp` path に更新。入力処理・公開 interfaces は変更していない。
- upstream clang++ 22.1.3 `ArtifactCoreControl` target で両実装の P1689 scan・compile と `ArtifactCoreControl.lib` link が成功。
- 2実装の warm rebuild は sccache 2 hits、miss / cache error / compilation failure 0。
- `check_module_hygiene.py` pass。対象 CMake / manifests に旧2 `.cppm` path の残存なし。

### ArtifactCore expression evaluation batch: ExpressionEvaluator / ExpressionParser / ExpressionValue（2026-10-23）

- `Script/Expression/ExpressionEvaluator.cppm`、`ExpressionParser.cppm`、`ExpressionValue.cppm` を `.cpp` に移行。3本文は LF 正規化後に Git HEAD と一致。source manifest / module-reference manifest と `ArtifactCore/CMakeLists.txt` のソース・MSVC IFC / object dependency mapping を `.cpp` path に更新。式の評価・解析・値表現と公開 interfaces は変更していない。
- upstream clang++ 22.1.3 `ArtifactCore` target で3実装の P1689 scan・compile と `ArtifactCore.lib` link が成功。
- 3実装の warm rebuild は sccache 3 hits、miss / cache error / compilation failure 0。
- `check_module_hygiene.py` pass。対象 CMake / manifests に旧3 `.cppm` path の残存なし。

### ArtifactCore UI input batch: InputOperator / InputOperatorManager（2026-10-23）

- `UI/InputOperator.cppm` と `UI/InputOperatorManager.cppm` を `.cpp` に移行。両本文は LF 正規化後に Git HEAD と一致。source manifest / module-reference manifest と `ArtifactCore/CMakeLists.txt` の UI source / MSVC IFC・object dependency mappings を `.cpp` path に更新。UI 入力処理と公開 interfaces は変更していない。
- upstream clang++ 22.1.3 `ArtifactCoreUI` target で両実装の P1689 scan・compile と `ArtifactCoreUI.lib` link が成功。既存コードの Qt 6 `QKeySequence` int 変換に deprecated warning が1件発生。
- 2実装の warm rebuild は sccache 2 hits、miss / cache error / compilation failure 0。
- `check_module_hygiene.py` pass。対象 CMake / manifests に旧2 `.cppm` path の残存なし。

### ArtifactCore embedded Python batch: PythonEngine / CorePythonAPI（2026-10-23）

- `Script/Python/PythonEngine.cppm` と `CorePythonAPI.cppm` を `.cpp` に移行。両本文は LF 正規化後に Git HEAD と一致。source manifest / module-reference manifest と `ArtifactCore/CMakeLists.txt` の implementation / MSVC IFC・object dependency mappings を `.cpp` path に更新。組み込み Python engine と composition bridge の behavior / public interfaces は変更していない。
- upstream clang++ 22.1.3 `ArtifactCore` target で両実装の P1689 scan・compile と `ArtifactCore.lib` link が成功。
- 2実装の warm rebuild は sccache 2 hits、miss / cache error / compilation failure 0。
- `check_module_hygiene.py` pass。対象 CMake / manifests に旧2 `.cppm` path の残存なし。

### ArtifactCore text subsystem batch: TextAnimator / TextShapingBackend（2026-10-23）

- `Text/TextAnimator.cppm` と `Text/TextShapingBackend.cppm` を `.cpp` に移行。両本文は LF 正規化後に Git HEAD と一致。source manifest / module-reference manifest と `ArtifactCore/CMakeLists.txt` の text source / MSVC IFC・object dependency mappings を `.cpp` path に更新。text animation / shaping behavior と public interfaces は変更していない。
- upstream clang++ 22.1.3 `ArtifactCoreText` target で両実装の P1689 scan・compile と `ArtifactCoreText.lib` link が成功。
- 2実装の warm rebuild は sccache 2 hits、miss / cache error / compilation failure 0。
- `check_module_hygiene.py` pass。対象 CMake / manifests に旧2 `.cppm` path の残存なし。

### ArtifactCore script execution batch: ScriptContext / BuiltinScriptVM（2026-10-23）

- `Script/Engine/Context/ScriptContext.cppm` と `Script/Engine/BultinScriptVM/BuiltinScriptVM.cppm` を `.cpp` に移行。両本文は LF 正規化後に Git HEAD と一致。source manifest / module-reference manifest と `ArtifactCore/CMakeLists.txt` の implementation / MSVC IFC・object dependency mappings を `.cpp` path に更新。script context / builtin VM behavior と public interfaces は変更していない。
- upstream clang++ 22.1.3 `ArtifactCore` target で両実装の P1689 scan・compile と `ArtifactCore.lib` link が成功。
- 2実装の warm rebuild は sccache 2 hits、miss / cache error / compilation failure 0。
- `check_module_hygiene.py` pass。対象 CMake / manifests に旧2 `.cppm` path の残存なし。

### ArtifactCore video transition factory batch: TransitionFactory（2026-10-23）

- `Video/TransitionFactory.cppm` を `.cpp` に移行。本文は LF 正規化後に Git HEAD と一致。source manifest / module-reference manifest と `ArtifactCore/CMakeLists.txt` の Video target source / MSVC IFC mapping を `.cpp` path に更新。factory behavior と public interface は変更していない。
- upstream clang++ 22.1.3 `ArtifactCoreVideo` target で P1689 scan・compile と `ArtifactCoreVideo.lib` link が成功。warm rebuild は sccache 1 hit、miss / cache error / compilation failure 0。
- `SpinTransition.cppm` は module-reference manifest に存在するが source manifest / target 実装リストに未登録のため、今回は移行対象から外した。未登録のままの理由確認を `Insight.md` に記録。
- `check_module_hygiene.py` pass。対象 CMake / source manifest に旧 `TransitionFactory.cppm` path の残存なし。

### ArtifactCore audio render / decode batch: AudioRenderer / FFMpegAudioDecoder（2026-10-23）

- `Audio/AudioRenderer.cppm` と `Codec/FFMpegAudioDecoder.cppm` を `.cpp` に移行。両本文は LF 正規化後に Git HEAD と一致。source manifest / module-reference manifest と `ArtifactCore/CMakeLists.txt` の Audio target source / MSVC IFC・object dependency mappings を `.cpp` path に更新。audio render / FFmpeg decode behavior と public interfaces は変更していない。
- upstream clang++ 22.1.3 `ArtifactCoreAudio` target で両実装の P1689 scan・compile と `ArtifactCoreAudio.lib` link が成功。`AudioRenderer` に既存の `std::atomic_*` shared_ptr overload deprecated warning が3件。
- 2実装の warm rebuild は sccache 2 hits、miss / cache error / compilation failure 0。
- `check_module_hygiene.py` pass。対象 CMake / manifests に旧2 `.cppm` path の残存なし。

### ArtifactCore script engine batch: CSharpScriptEngine / ScriptRuntime（2026-10-23）

- `Script/CSharpScriptEngine.cppm` と `Script/ScriptRuntime.cppm` を `.cpp` に移行。両本文は LF 正規化後に Git HEAD と一致。source manifest / module-reference manifest と `ArtifactCore/CMakeLists.txt` の implementation / MSVC IFC・object dependency mappings を `.cpp` path に更新。C# bridge / script runtime behavior と public interfaces は変更していない。
- upstream clang++ 22.1.3 `ArtifactCore` target で両実装の P1689 scan・compile と `ArtifactCore.lib` link が成功。`CSharpScriptEngine` に既存 `std::getenv` deprecated warning が1件。
- 2実装の warm rebuild は sccache 2 hits、miss / cache error / compilation failure 0。
- `check_module_hygiene.py` pass。対象 CMake / manifests に旧2 `.cppm` path の残存なし。

### ArtifactCore video processing batch: Stabilizer / GStreamerDecoder（2026-10-23）

- `Video/Stabilizer.cppm` と `Codec/GStreamerDecoder.cppm` を `.cpp` に移行。両本文は LF 正規化後に Git HEAD と一致。source manifest / module-reference manifest と `ArtifactCore/CMakeLists.txt` の Video target source / MSVC IFC・object dependency mappings を `.cpp` path に更新。stabilization / GStreamer decoder behavior と public interfaces は変更していない。
- upstream clang++ 22.1.3 `ArtifactCoreVideo` target で両実装の P1689 scan・compile と `ArtifactCoreVideo.lib` link が成功。
- 2実装の warm rebuild は sccache 2 hits、miss / cache error / compilation failure 0。
- `check_module_hygiene.py` pass。対象 CMake / manifests に旧2 `.cppm` path の残存なし。

### ArtifactCore image / color processing batch: IBKKeyer / ColorGrading（2026-10-23）

- `ImageProcessing/Keying/IBKKeyer.cppm` と `ColorCollection/ColorGrading.cppm` を `.cpp` に移行。両本文は LF 正規化後に Git HEAD と一致。source manifest / module-reference manifest と `ArtifactCore/CMakeLists.txt` の implementation / MSVC IFC mappings を `.cpp` path に更新。keying / color grading behavior と public interfaces は変更していない。
- upstream clang++ 22.1.3 で `ArtifactCore` と `ArtifactCoreColorCollection` targets の P1689 scan・compile / static library links が成功。
- 2実装の warm rebuild は sccache 2 hits、miss / cache error / compilation failure 0。
- `check_module_hygiene.py` pass。対象 CMake / manifests に旧2 `.cppm` path の残存なし。

### ArtifactCore material / mesh model batch: Material / Mesh（2026-10-23）

- `Material/Material.cppm` と `Mesh/Mesh.cppm` を `.cpp` に移行。両本文は LF 正規化後に Git HEAD と一致。source manifest / module-reference manifest と `ArtifactCore/CMakeLists.txt` の implementation / MSVC IFC mappings を `.cpp` path に更新。material / mesh model behavior と public interfaces は変更していない。
- upstream clang++ 22.1.3 で `ArtifactCore` と `ArtifactCoreMaterial` targets の P1689 scan・compile / static library links が成功。
- 2実装の warm rebuild は sccache 2 hits、miss / cache error / compilation failure 0。
- `check_module_hygiene.py` pass。対象 CMake / manifests に旧2 `.cppm` path の残存なし。

### ArtifactCore scene / tracking batch: SceneNode / NccTracker（2026-10-23）

- `Scene/SceneNode.cppm` と `Track/NccTracker.cppm` を `.cpp` に移行。両本文は LF 正規化後に Git HEAD と一致。source manifest / module-reference manifest と `ArtifactCore/CMakeLists.txt` の Scene / Track target source / MSVC IFC mappings を `.cpp` path に更新。scene graph / NCC tracking behavior と public interfaces は変更していない。
- upstream clang++ 22.1.3 で実登録先の `ArtifactCoreScene` / `ArtifactCoreTrack` targets の P1689 scan・compile と static library links が成功。
- warm rebuild の sccache は 2 hits / 1 miss（3 compile requests total; SceneNode の初回 target 取り違えもあり、warm 集計に含まれる）。errors / compilation failures 0。
- `check_module_hygiene.py` pass。対象 CMake / manifests に旧2 `.cppm` path の残存なし。

### ArtifactCore mask editing batch: RotoMask / RotoMaskEditor（2026-10-23）

- `Mask/RotoMask.cppm` と `UI/RotoMaskEditor.cppm` を `.cpp` に移行。両本文は LF 正規化後に Git HEAD と一致。source manifest / module-reference manifest と `ArtifactCore/CMakeLists.txt` の Mask / UI target source / MSVC IFC・object dependency mappings を `.cpp` path に更新。mask model / editor behavior と public interfaces は変更していない。
- upstream clang++ 22.1.3 で `ArtifactCoreMask` / `ArtifactCoreUI` targets の P1689 scan・compile と static library links が成功。
- warm rebuild は sccache 2 hits、miss / cache error / compilation failure 0。
- `check_module_hygiene.py` pass。対象 CMake / manifests に旧2 `.cppm` path の残存なし。

### ArtifactCore composition / math batch: CompositionFinalEffect / NoiseGenerator（2026-10-23）

- `Composition/CompositionFinalEffect.cppm` と `Math/NoiseGenerator.cppm` を `.cpp` に移行。両本文は LF 正規化後に Git HEAD と一致。source manifest / module-reference manifest と `ArtifactCore/CMakeLists.txt` の Composition / Math source・MSVC IFC / object dependency mappings を `.cpp` path に更新。composition final effect / noise generation behavior と public interfaces は変更していない。
- upstream clang++ 22.1.3 で実際の登録先 `ArtifactCoreComposition` / `ArtifactCore` targets の P1689 scan・compile と static library links が成功。
- 2実装の warm rebuild は sccache 2 hits、miss / cache error / compilation failure 0。
- `check_module_hygiene.py` pass。対象 CMake / manifests に旧2 `.cppm` path の残存なし。

### ArtifactCore transform / rig batch: TransformHelper / Rig2D（2026-10-23）

- `Transform/TransformHelper.cppm` と `Rig/Rig2D.cppm` を `.cpp` に移行。両本文は LF 正規化後に Git HEAD と一致。source manifest / module-reference manifest と `ArtifactCore/CMakeLists.txt` の implementation path、TransformHelper の MSVC compile-option mapping、Rig の専用 target source path を更新。public `.ixx` と transform / rig behavior は変更していない。
- upstream clang++ 22.1.3 で実登録先の `ArtifactCore` / `ArtifactCoreRig` targets の P1689 scan・compile と static library links が成功。`Rig2D.ixx` から Qt `QStringLiteral` の TU-local entity exposure warning が1件出た。
- 2実装の warm rebuild は sccache 2 hits、miss / cache error / compilation failure 0。
- `check_module_hygiene.py` pass。対象 CMake / manifests に旧2 `.cppm` path の残存なし。

### ArtifactCore AI segmentation batch: ONNX / RotoBrush（2026-10-23）

- `AI/OnnxDmlLocalAgent.cppm`、`OnnxImageSegmenter.cppm`、`RotoBrushImageSegmenter.cppm` を `.cpp` に移行。3本文は LF 正規化後に Git HEAD と一致。source manifest / module-reference manifest と `ArtifactCore/CMakeLists.txt` の source、module scan、MSVC compile-option・dependency、ONNX compile-definition mappings を `.cpp` path に更新。public `.ixx` と segmentation behavior は変更していない。
- upstream clang++ 22.1.3 で `ArtifactCoreAI` の P1689 scan、3実装の compile、static library link が成功。現在の構成では ONNX Runtime の条件付き backend code は無効。
- 3実装の warm rebuild は sccache 3 hits、miss / cache error / compilation failure 0。
- `check_module_hygiene.py` pass。対象 CMake / manifests に旧3 `.cppm` path の残存なし。

### ArtifactCore diagnostics / string utilities batch: Logger / UniString（2026-10-23）

- `Diagnostics/Logger.cppm` と `Utils/UniString.cppm` を `.cpp` に移行。両本文は LF 正規化後に Git HEAD と一致。source manifest / module-reference manifest と `ArtifactCore/CMakeLists.txt` の source、MSVC compile-option・object dependency mappings を `.cpp` path に更新。logging / Unicode string behavior と public `.ixx` は変更していない。
- upstream clang++ 22.1.3 で `ArtifactCore` の両実装 P1689 scan・compile と static library link が成功。
- 2実装の warm rebuild は sccache 2 hits、miss / cache error / compilation failure 0。
- `check_module_hygiene.py` pass。対象 CMake / manifests に旧2 `.cppm` path の残存なし。

### ArtifactCore property / NLE / audio host batch: AbstractProperty / NLE Core / CLAPHost（2026-10-23）

- `Property/AbstractProperty.cppm`、`NLE/Core.cppm`、`CLAP/CLAPHost.cppm` を `.cpp` に移行。3本文は LF 正規化後に Git HEAD と一致。source manifest / module-reference manifest と `ArtifactCore/CMakeLists.txt` の source、MSVC IFC / object dependency、scanner 条件を `.cpp` path に更新。public `.ixx` と property / NLE / CLAP host behavior は変更していない。
- upstream clang++ 22.1.3 で実登録先の `ArtifactCore` / `ArtifactCoreNLE` / `ArtifactCoreAudio` targets の P1689 scan・compile と static library links が成功。`CLAPHost` に既存 `std::getenv` deprecated warning が2件。
- 3実装の warm rebuild は sccache 3 hits、miss / cache error / compilation failure 0。
- `check_module_hygiene.py` pass。対象 CMake / manifests に旧3 `.cppm` path の残存なし。

### ArtifactCore particle / scripting batch: ParticleSystem / ArtifactScript（2026-10-23）

- `Particle/ParticleSystem.cppm` と `Script/ArtifactScript/ArtifactScript.cppm` を `.cpp` に移行。両本文は LF 正規化後に Git HEAD と一致。source manifest / module-reference manifest と `ArtifactCore/CMakeLists.txt` の core / audio staging source lists、compile-option / object dependency mappings を `.cpp` path に更新。public `.ixx` と simulation / scripting behavior は変更していない。
- upstream clang++ 22.1.3 で `ArtifactCore` の両実装 P1689 scan・compile と static library link が成功。`ArtifactScript` に coalescing-assignment 演算子の文字列リテラル由来の既存 trigraph warnings が3件。
- 2実装の warm rebuild は sccache 2 hits、miss / cache error / compilation failure 0。
- `check_module_hygiene.py` pass。対象 CMake / manifests に旧2 `.cppm` path の残存なし。

### ArtifactCore image / physics / shader-node batch: ImageTransformCV / FractureEngine / ArtifactShaderNode（2026-10-23）

- `Image/OpenCV/ImageTransformCV.cppm`、`Physics/FractureEngine.cppm`、`ShaderNode/ArtifactShaderNode.cppm` を `.cpp` に移行。3本文は LF 正規化後に Git HEAD と一致。source manifest / module-reference manifest と `ArtifactCore/CMakeLists.txt` の image / shader-node source・MSVC IFC / object dependency mappings を `.cpp` path に更新。public interfaces と image transform / fracture / shader node behavior は変更していない。
- upstream clang++ 22.1.3 で `ArtifactCore` の P1689 scan、3実装 compile、static library link が成功。`ImageTransformCV` の `module Image; import :ImageTransform;` partition dependency も scan / compile 成功。
- 3実装の warm rebuild は sccache 3 hits、miss / cache error / compilation failure 0。
- `check_module_hygiene.py` pass。対象 CMake / manifests に旧3 `.cppm` path の残存なし。

### ArtifactCore simulation / volume conversion batch: PyroSimulation / MeshToVolume（2026-10-23）

- `Simulation/PyroSimulation.cppm` と `Render/MeshToVolume.cppm` を `.cpp` に移行。両本文は LF 正規化後に Git HEAD と一致。source manifest / module-reference manifest と `ArtifactCore/CMakeLists.txt` の implementation source、MSVC IFC / object dependency mappings を `.cpp` path に更新。public `.ixx` と simulation / volume conversion behavior は変更していない。
- upstream clang++ 22.1.3 で `ArtifactCore` の両実装 P1689 scan・compile と static library link が成功。
- 2実装の warm rebuild は sccache 2 hits、miss / cache error / compilation failure 0。
- `check_module_hygiene.py` pass。対象 CMake / manifests に旧2 `.cppm` path の残存なし。

### ArtifactCore job queue batch: GPUThreadPool / RendererQueueManager（2026-10-23）

- `Graphics/GPUThreadPool.cppm` と `Render/RendererQueueManager.cppm` を `.cpp` に移行。両本文は LF 正規化後に Git HEAD と一致。source manifest / module-reference manifest と `ArtifactCore/CMakeLists.txt` の RendererQueueManager MSVC IFC / object dependency mappings を `.cpp` path に更新。public `.ixx` と queue / scheduling behavior は変更していない。
- upstream clang++ 22.1.3 で `ArtifactCore` の両実装 P1689 scan・compile と static library link が成功。
- 2実装の warm rebuild は sccache 2 hits、miss / cache error / compilation failure 0。
- `check_module_hygiene.py` pass。対象 CMake / manifests に旧2 `.cppm` path の残存なし。

### ArtifactCore GPU resource planning batch: GPUTexture / GPUAppendConsume（2026-10-23）

- `Graphics/GPUTexture.cppm` と `Graphics/GPUAppendConsume.cppm` を `.cpp` に移行。両本文は LF 正規化後に Git HEAD と一致。source manifest / module-reference manifest と `ArtifactCore/CMakeLists.txt` の GPUTexture MSVC IFC mapping を `.cpp` path に更新。public `.ixx`、texture metadata / append-consume planning behavior は変更していない。
- upstream clang++ 22.1.3 で `ArtifactCore` の両実装 P1689 scan・compile と static library link が成功。
- 2実装の warm rebuild は sccache 2 hits、miss / cache error / compilation failure 0。
- `check_module_hygiene.py` pass。対象 CMake / manifests に旧2 `.cppm` path の残存なし。

### ArtifactCore volume processing batch: VolumeRenderer / VolumeModifier / VolumePostProcess（2026-10-23）

- `Render/VolumeRenderer.cppm`、`VolumeModifier.cppm`、`VolumePostProcess.cppm` を `.cpp` に移行。3本文は LF 正規化後に Git HEAD と一致。source manifest / module-reference manifest と `ArtifactCore/CMakeLists.txt` の MSVC IFC mappings を `.cpp` path に更新。public `.ixx`、volume sampling / modification / post-process behavior は変更していない。
- upstream clang++ 22.1.3 で `ArtifactCore` の3実装 P1689 scan・compile と static library link が成功。
- 3実装の warm rebuild は sccache 3 hits、miss / cache error / compilation failure 0。
- `check_module_hygiene.py` pass。対象 CMake / manifests に旧3 `.cppm` path の残存なし。

### ArtifactCore GPU compute foundation batch: Compute / GPUComputeContext（2026-10-23）

- `Graphics/Compute.cppm` と `Graphics/GPUComputeContext.cppm` を `.cpp` に移行。両本文は LF 正規化後に Git HEAD と一致。source manifest / module-reference manifest と `ArtifactCore/CMakeLists.txt` の implementation source、MSVC IFC mappings を `.cpp` path に更新。public `.ixx`、ComputeExecutor の PSO/SRB 操作、GpuContext の device/context / shader compile behavior は変更していない。
- upstream clang++ 22.1.3 で両実装の P1689 scan・compile と `ArtifactCore.lib` link が成功。
- 2実装の warm rebuild は sccache 2 hits、miss / cache error / compilation failure 0。
- 対象 source/CMake/manifests に旧2 `.cppm` path の残存なし。

### ArtifactCore GPU simulation compute batch: BoidsCompute / ParticleCompute（2026-10-23）

- `Graphics/BoidsCompute.cppm` と `Graphics/ParticleCompute.cppm` を `.cpp` に移行。両本文は LF 正規化後に Git HEAD と一致。source manifest / module-reference manifest と `ArtifactCore/CMakeLists.txt` の source、MSVC IFC mappings を `.cpp` path に更新。public `.ixx`、GPU simulation / upload / dispatch behavior は変更していない。
- upstream clang++ 22.1.3 で両実装の P1689 scan・compile と `ArtifactCore.lib` link が成功。
- 2実装の warm rebuild は sccache 2 hits、miss / cache error / compilation failure 0。
- 対象 source/CMake/manifests に旧2 `.cppm` path の残存なし。

### ArtifactCore DirectCompute image kernels batch: GlowCS / NegateCS（2026-10-23）

- `ImageProcessing/DirectCompute/GlowCS.cppm` と `NegateCS.cppm` を `.cpp` に移行。両本文は LF 正規化後に Git HEAD と一致。source manifest / module-reference manifest と `ArtifactCore/CMakeLists.txt` の source、MSVC IFC mappings を `.cpp` path に更新。public `.ixx` と既存の DirectCompute behavior は変更していない。
- upstream clang++ 22.1.3 で両実装の P1689 scan・compile と `ArtifactCore.lib` link が成功。
- 2実装の warm rebuild は sccache 2 hits、miss / cache error / compilation failure 0。
- 対象 source/CMake/manifests に旧2 `.cppm` path の残存なし。

### ArtifactCore DirectCompute shader utility batch: GlowCS / NegateCS / SinglePassShader（2026-10-23）

- `ImageProcessing/DirectCompute/GlowCS.cppm`、`NegateCS.cppm`、`SinglePassShader.cppm` を `.cpp` に移行。本文は LF 正規化後に Git HEAD と一致。source manifest / module-reference manifest、`ArtifactCore/CMakeLists.txt` の IFC と `Core.ArtifactString` object-order mapping を `.cpp` path に更新。public `.ixx` と処理内容は変更していない。
- upstream clang++ 22.1.3 で3実装の P1689 scan・compile と `ArtifactCore.lib` link が成功。
- 3実装の warm rebuild は sccache 3 hits、miss / cache error / compilation failure 0。
- 対象 source/CMake/manifests に旧3 `.cppm` path の残存なし。

### ArtifactCore GPU rendering support batch: TextureManager / GPURayTracer（2026-10-23）

- `Graphics/Texture/TextureManager.cppm` と `Render/GPURayTracer.cppm` を `.cpp` に移行。両本文は LF 正規化後に Git HEAD と一致。source manifest / module-reference manifest と `ArtifactCore/CMakeLists.txt` の IFC mappings を `.cpp` path に更新。public `.ixx`、texture creation / ray tracing code は変更していない。
- upstream clang++ 22.1.3 で両実装の P1689 scan・compile と `ArtifactCore.lib` link が成功。検証構成は Windows Vulkan enabled / D3D12 unsupported であり、runtime DXR 動作確認ではない。
- 2実装の warm rebuild は sccache 2 hits、miss / cache error / compilation failure 0。
- 対象 source/CMake/manifests に旧2 `.cppm` path の残存なし。

### ArtifactCore GPU particle simulation batch: SandGPUCompute / MpmCompute（2026-10-23）

- `Graphics/SandGPUCompute.cppm` と `Graphics/MpmCompute.cppm` を `.cpp` に移行。両本文は LF 正規化後に Git HEAD と一致。source manifest / module-reference manifest、`ArtifactCore/CMakeLists.txt` の IFC mappings と Sand 専用 `CORE_IMPL` 追加判定を `.cpp` path に更新。public `.ixx`、simulation / GPU dispatch behavior は変更していない。
- upstream clang++ 22.1.3 で両実装の P1689 scan・compile と `ArtifactCore.lib` link が成功。
- 2実装の warm rebuild は sccache 2 hits、miss / cache error / compilation failure 0。
- 対象 source/CMake/manifests に旧2 `.cppm` path の残存なし。

### ArtifactCore GPU graphics pipeline batch: LayerBlendPipeline / ParticleRenderer（2026-10-23）

- `Graphics/LayerBlendPipeline.cppm` と `Graphics/ParticleRenderer.cppm` を `.cpp` に移行。両本文は LF 正規化後に Git HEAD と一致。source manifest / module-reference manifest と `ArtifactCore/CMakeLists.txt` の IFC / object-order mappings を `.cpp` path に更新。public `.ixx`、blend / particle rendering behavior は変更していない。
- upstream clang++ 22.1.3 で両実装の P1689 scan・compile と `ArtifactCore.lib` link が成功。
- 2実装の warm rebuild は sccache 2 hits、miss / cache error / compilation failure 0。
- 対象 source/CMake/manifests に旧2 `.cppm` path の残存なし。

### ArtifactCore GPU renderer batch: MeshRenderer / RayTracingManager（2026-10-23）

- `Graphics/MeshRenderer.cppm` と `Graphics/RayTracingManager.cppm` を `.cpp` に移行。両本文は LF 正規化後に Git HEAD と一致。source manifest / module-reference manifest と `ArtifactCore/CMakeLists.txt` の IFC / object-order mappings を `.cpp` path に更新。RayTracingManager の GraphicsHelper provider mapping も更新。public `.ixx`、mesh / ray tracing code は変更していない。
- upstream clang++ 22.1.3 で両実装の P1689 scan・compile と `ArtifactCore.lib` link が成功。
- 2実装の warm rebuild は sccache 2 hits、miss / cache error / compilation failure 0。
- 対象 source/CMake/manifests に旧2 `.cppm` path の残存なし。

### ArtifactCore MPM solver integration batch: MpmSolver2D（2026-10-23）

- `Physics/MpmSolver2D.cppm` を `.cpp` に移行。本文は LF 正規化後に Git HEAD と一致。source manifest / module-reference manifest と `ArtifactCore/CMakeLists.txt` の IFC / object-order mappings を `.cpp` path に更新。public `.ixx`、MPM solver / GPU dispatch behavior は変更していない。
- upstream clang++ 22.1.3 で P1689 scan・compile と `ArtifactCore.lib` link が成功。
- warm rebuild は sccache 1 hit、miss / cache error / compilation failure 0。
- 対象 source/CMake/manifests に旧 `src/Physics/MpmSolver2D.cppm` path の残存なし。


### ArtifactCore acoustic model batch: seven implementations（2026-10-23）

- `Acoustic/AudioTaskSynthesizer.cppm`、`FrictionModel.cppm`、`ModalResonator.cppm`、`RainModel.cppm`、`SpatialSystem.cppm`、`WindModel.cppm`、`WaveModel.cppm` を `.cpp` に移行。7本文は LF 正規化後に Git HEAD と一致。source manifest と `ARTIFACTCORE_ACOUSTIC_MODULES` 登録を `.cpp` に更新。public `Acoustic.ixx` / `AcousticSystem.ixx` と音響処理は変更していない。
- upstream clang++ 22.1.3 で専用 `ArtifactCoreAcoustic` target の P1689 scan・compile と `ArtifactCoreAcoustic.lib` link が成功。`ArtifactCore` 通常targetには含まれないため専用targetを直接構築。`ModalResonator::Trigger` に既存の `-Winconsistent-missing-override` 警告1件。
- warm rebuild は sccache 8 hits（7実装と再コンパイルされた1 interface）、miss / cache error / compilation failure 0。
- 対象 source/CMake/manifests に旧7 `.cppm` path の残存なし。


### ArtifactCore physics solver batch: ClothSolver3D / SoftBodySolver（2026-10-23）

- `Physics/ClothSolver3D.cppm` と `Physics/SoftBodySolver.cppm` を `.cpp` に移行。両本文は LF 正規化後に Git HEAD と一致。source manifest を `.cpp` path に更新。public module 名、`PhysicsSystem` の IFC 参照、solver 処理は変更していない。
- upstream clang++ 22.1.3 で両実装の P1689 scan・compile と `ArtifactCore.lib` link が成功。依存利用側 `PhysicsSystem.cppm` も再ビルド成功。
- warm rebuild は sccache 3 hits（2実装と再コンパイルされた利用側）、miss / cache error / compilation failure 0。
- 対象 source/CMake/manifests に旧2 `.cppm` path の残存なし。


### ArtifactCoreAI description module batch: 16 interfaces（2026-10-23）

- `AI/3DLightingDescriptions`、`AIAnalysisDescriptions`、`AnimationUIDescriptions`、`CoreDescriptions`、`DataAssetDescriptions`、`DescriptionExamples`、`EffectsUtilityDescriptions`、`ExportImportDescriptions`、`FinalUtilitiesDescriptions`、`FluidMaskingDescriptions`、`ImageMathDescriptions`、`LayerDescriptions`、`MoreDescriptions`、`RenderAudioColorDescriptions`、`TransitionsGeneratorDescriptions`、`UIWidgetsDescriptions` を `.cppm` から `.cpp` に移行。16本文は LF 正規化後に Git HEAD と一致。`ARTIFACTCORE_MODULE_SOURCES` と `ARTIFACTCORE_AI_MODULES` の明示登録を `.cpp` に更新し、module interface としての所属を維持。共通 `Core.AI.Describable` import、公開 module 宣言と説明内容は変更していない。
- upstream clang++ 22.1.3 で `ArtifactCoreAI` の P1689 scan・compile と `ArtifactCoreAI.lib` link が成功。
- warm rebuild は sccache 16 hits、miss / cache error / compilation failure 0。
- 対象 source/CMake/manifests に旧16 `.cppm` path の残存なし。


### ArtifactCore core utility modules: Atomic / Optional / HashMap（2026-10-23）

- `Core/ArtifactAtomic.cppm`、`ArtifactOptional.cppm`、`ArtifactHashMap.cppm` を `.cpp` に移行。3本文は LF 正規化後に Git HEAD と一致。`ARTIFACTCORE_MODULE_SOURCES` を更新し、`ArtifactCore/CMakeLists.txt` の ArtifactArray / container consumer object-order paths を `.cpp.obj` に合わせた。module declarations と container behavior は変更していない。
- upstream clang++ 22.1.3 で `ArtifactCore` の scan・compile と `ArtifactCore.lib` link が成功。`ArtifactHashMap` → `ArtifactOptional` と ArtifactArray / ArtifactDict / NamedVector consumer も再ビルド成功。
- warm rebuild では広いmodule依存再構築が発生し、sccache 262 hits、miss / cache error / compilation failure 0。
- 対象 source/CMake/manifests に旧3 `.cppm` path または `.cppm.obj` dependency の残存なし。


### ArtifactCore memory utility modules: SharedPtr / TrackedPtr / UniquePtr（2026-10-23）

- `Memory/SharedPtr.cppm`、`TrackedPtr.cppm`、`UniquePtr.cppm` を `.cpp` に移行。3本文は LF 正規化後に Git HEAD と一致。source manifest と module-reference manifest の29 provider references、`ArtifactCore/CMakeLists.txt` の11 object-order paths を `.cpp` / `.cpp.obj` に更新。module declarations、NamedVector dependency、ownership behavior は変更していない。
- upstream clang++ 22.1.3 で `ArtifactCore` の3 module scan・compile、29 consumer references の解決、`ArtifactCore.lib` link が成功。
- warm rebuild は sccache 197 hits、miss / cache error / compilation failure 0。
- 対象 source/CMake/manifests に旧3 `.cppm` path / `.cppm.obj` references の残存なし。


### ArtifactCore Qt audio backend: QtAudioBackend（2026-10-23）

- `Audio/QtAudioBackend.cppm` を `.cpp` に移行。本文は LF 正規化後に Git HEAD と一致。source manifest、`AudioRenderer` の module-reference provider source、`ArtifactCore/CMakeLists.txt` の module file-set / object-order paths を `.cpp` に更新。audio backend behavior と public module interface は変更していない。
- upstream clang++ 22.1.3 で `ArtifactCoreAudio` の P1689 scan・compile と `ArtifactCoreAudio.lib` link が成功。`AudioRenderer` consumer も再ビルド成功。既存 `AudioRenderer.cpp` の deprecated shared_ptr atomic overload warnings 3件。
- warm rebuild は sccache 2 hits、miss / cache error / compilation failure 0。
- 対象 source/CMake/manifests に旧 `src/Audio/QtAudioBackend.cppm` path の残存なし。


### ArtifactCore image implementation batch: ChromaKey / FloatImage / ImageExports / JPEGImage（2026-10-23）

- `Image/ChromaKey.cppm`、`FloatImage.cppm`、`ImageExports.cppm`、`JPEGImage.cppm` を `.cpp` に移行。4本文は Git HEAD と一致し、改行は LF。`ArtifactCoreSources.cmake` の source registrations のみ更新し、公開 interface / image processing behavior は変更していない。追加の module-reference provider や object-order mapping は不要だった。
- upstream clang++ 22.1.3 で4ファイルの P1689 scan・compile と `ArtifactCore.lib` link が成功。
- warm rebuild は sccache 4 hits、miss / cache error / compilation failure 0。
- 対象 source/manifests に旧4 `.cppm` path の残存なし。


### ArtifactCore OpenCV implementation batch: AutoContrastCV / OpenCVCompositionBuffer2D（2026-10-23）

- `ImageProcessing/OpenCV/AutoContrastCV.cppm` と `Composition/OpenCVCompositionBuffer2D.cppm` を `.cpp` に移行。両本文は Git HEAD と一致し LF。`ArtifactCoreSources.cmake` と `ArtifactCore/CMakeLists.txt` の Composition split-target implementation registration を `.cpp` path に更新。公開 module interface と処理内容は変更していない。
- upstream clang++ 22.1.3 で両ファイルの P1689 scan・compile、`ArtifactCore.lib` と `ArtifactCoreComposition.lib` の link が成功。既存 `ParametricComposition.cpp` に Qt deprecated `addData` warnings 5件。
- warm rebuild は sccache 2 hits、miss / cache error / compilation failure 0。
- 対象 source/CMake registrations に旧2 `.cppm` path の残存なし。


### ArtifactCore non-module implementation batch: MeshEncoder / KnobList / WorkAround（2026-10-23）

- `Geometry/MeshEncoder.cppm`、`Knob/KnobList.cppm`、`Utils/WorkAround.cppm` を `.cpp` に移行。3本文は Git HEAD と一致し LF。`ArtifactCoreSources.cmake` の implementation source registrations のみ変更し、公開 module interface / provider mapping / behavior は変更していない。
- upstream clang++ 22.1.3 で3ファイルの P1689 scan・compile と `ArtifactCore.lib` link が成功。既存 `KnobList` 先頭の `#pragma once` による warning 1件。
- warm rebuild は sccache 3 hits、miss / cache error / compilation failure 0。
- 対象 source/manifest に旧3 `.cppm` path の残存なし。


### ArtifactCore collaboration module interface batch: six exported modules（2026-10-23）

- `Collaborate/CollabOperations.cppm`、`CollaborationProtocol.cppm`、`CollaborationSession.cppm`、`CollaborationSessionAdapter.cppm`、`CollabReview.cppm`、`CollabSchema.cppm` を `.cpp` に移行。6本文は Git HEAD と一致し LF。`ArtifactCore/CMakeLists.txt` の `ArtifactCoreCollaborate` CXX_MODULES file set と、Protocol の `ArtifactCoreSources.cmake` 登録を更新。module names / exports / collaboration behavior は変更していない。
- upstream clang++ 22.1.3 で6 interface の P1689 scan・compile と `ArtifactCoreCollaborate.lib` link が成功。Clang は既存の Qt `QStringLiteral` に関する TU-local entity exposure warning を計308件報告したが build は成功。
- warm rebuild は sccache 6 hits、miss / cache error / compilation failure 0。
- 対象 source/CMake/manifests に旧6 `.cppm` path の残存なし。


### ArtifactCore diagnostic module interface batch: DiagnosticRegistry / CoreDiagnostic.Test（2026-10-23）

- `Diagnostic/DiagnosticRegistry.cppm` と `Diagnostics/CoreDiagnostic.Test.cppm` を `.cpp` に移行。両本文は Git HEAD と一致し LF。`ArtifactCoreSources.cmake` の module source registrations と `ArtifactCore/CMakeLists.txt` の `ArtifactCoreAcoustic` module file set registration を更新。module names / exports / diagnostic behavior は変更していない。
- upstream clang++ 22.1.3 で2 interface の P1689 scan・compile と `ArtifactCore.lib` / `ArtifactCoreAcoustic.lib` link が成功。Acoustic 側の既存 `ModalResonator::Trigger` missing-override warning 1件。
- warm rebuild は sccache 2 hits、miss / cache error / compilation failure 0。
- 対象 source/CMake/manifests に旧2 `.cppm` path の残存なし。


### ArtifactCore split module interface batch: ExternalControl / CameraTracker（2026-10-23）

- `Control/ArtifactExternalControlManager.cppm` と `Tracking/CameraTracker.cppm` を `.cpp` に移行。両本文は Git HEAD と一致し LF。`ArtifactCoreSources.cmake` と各 `ArtifactCoreControl` / `ArtifactCoreTracking` CXX_MODULES file set registration を更新。module names / exports / behavior は変更していない。
- upstream clang++ 22.1.3 で両 interface の P1689 scan・compile と `ArtifactCoreControl.lib` / `ArtifactCoreTracking.lib` link が成功。
- warm rebuild は sccache 2 hits、miss / cache error / compilation failure 0。
- 対象 source/CMake/manifests に旧2 `.cppm` path の残存なし。


### ArtifactCore base module batch: ImageF32x1 / Knob / CreativeEffectFactory（2026-10-23）

- `ImageProcessing/ImageF32x1.cppm`、`Knob/Knob.cppm`、`Graphics/Effect/CreativeEffectFactory.cppm` を `.cpp` に移行。3本文は Git HEAD と一致し LF。`ArtifactCoreSources.cmake` の module / implementation registrations と、`ArtifactCoreModuleReferences.cmake` の Factory provider source 2箇所を更新。module names / exports / factory behavior は変更していない。
- upstream clang++ 22.1.3 で3ファイルの P1689 scan・compile と `ArtifactCore.lib` link が成功。
- warm rebuild は sccache 3 hits、miss / cache error / compilation failure 0。
- 対象 source/CMake/manifests に旧3 `.cppm` path の残存なし。


### ArtifactCore Render/UI source batch: ImageBuffer partition / ViewportOperator（2026-10-23）

- `Render/ImageBuffer.cppm` と `UI/ViewportOperator.cppm` を `.cpp` に移行。両本文は Git HEAD と一致し LF。`ArtifactCoreSources.cmake` と `ArtifactCore/CMakeLists.txt` の UI split-target implementation registration を更新。Render.ImageBuffer partition, public interface, UI behavior は変更していない。
- upstream clang++ 22.1.3 で両ファイルの P1689 scan・compile と `ArtifactCore.lib` / `ArtifactCoreUI.lib` link が成功。依存再構築で既存 `InputOperator.cpp` に Qt deprecated API warning 1件。
- warm rebuild は sccache 2 hits、miss / cache error / compilation failure 0。
- 対象 source/CMake/manifests に旧2 `.cppm` path の残存なし。


### ArtifactCore remaining non-frozen module batch: PlaybackManager / PhysicsSystem（2026-10-23）

- `Video/PlaybackManager.cppm` と `Physics/PhysicsSystem.cppm` を `.cpp` に移行。両本文は Git HEAD と一致し LF。PlaybackManager は source manifest、Video implementation list / source compile options、module-reference manifest を更新。PhysicsSystem は source manifest と CMake の path-specific compile-option branch / object dependencies を `.cpp` に更新。Transport / physics behavior は変更していない。
- upstream clang++ 22.1.3 で P1689 scan・compile と `ArtifactCoreVideo.lib` / `ArtifactCore.lib` link が成功。
- PlaybackManager warm rebuild は sccache 1 hit。PhysicsSystem は初回 warm probe で miss 1（failure 0）、次の同一 rebuild で hit 1、miss / cache error / compilation failure 0。
- 対象 source/CMake/manifests/comments に旧2 `.cppm` path の残存なし。


### ArtifactCore Video implementation partition: FFmpegEncoder.Helpers（2026-10-23）

- `Image/FFmpegEncoder.Helpers.cppm` を `.cpp` に移行。本文は Git HEAD と一致し LF。`ArtifactCore/CMakeLists.txt` の Video CXX_MODULES file set と `ArtifactCoreModuleReferences.cmake` の `Encoder.FFmpegEncoder` provider source を `.cpp` path に更新。partition / codec behavior は変更していない。
- upstream clang++ 22.1.3 で P1689 scan・compile と `ArtifactCoreVideo.lib` link が成功。
- warm rebuild は sccache 1 hit、miss / cache error / compilation failure 0。
- 対象 CMake/manifests に旧 `FFmpegEncoder.Helpers.cppm` path の残存なし。


### ArtifactCore GPU compute implementation batch: five computers（2026-10-23）

- `Graphics/Compute/{CurveComputer,DuotoneComputer,EchoBlendComputer,EdgeEchoComputer,HalftoneComputer}.cppm` を `.cpp` に移行。5件とも既存 public `.ixx` が module file set に登録済みで、module-reference manifest も追跡していたため、未登録ソースの target 有効化はしていない。source manifest と module-reference provider path を `.cpp` に更新。
- 本家 clang++ 22.1.3 の P1689 scan / compile で、EchoBlend に直接 `<vector>`、EdgeEcho に `RefCntAutoPtr.hpp` の include 不足と旧 format enum 名が見つかったため、必要な include と既存 Diligent の `TEX_FORMAT_RGBA32_FLOAT` 名に修正。GPU の処理経路・アルゴリズムは変更していない。
- `ArtifactCore` target と `ArtifactCore.lib` link が成功。warm rebuild では今回再コンパイルされた4 translation units が sccache 4 hits、miss / error / compilation failure 0。CurveComputer は前段 build で既に成功済み。
- 全5ファイルを LF に統一し、3つの clang portability 修正を除く実装本文が Git HEAD と同一であることを確認。対象 source/manifests に旧5 `.cppm` path の残存なし。


### ArtifactCore video transition implementation: SpinTransition（2026-10-23）

- `Video/Transitions/SpinTransition.cppm` を `.cpp` に移行。`ArtifactCoreSources.cmake`、`ArtifactCore/CMakeLists.txt` の Video implementation list、module-reference provider path を更新。Video の既存 source group に登録済みであることを確認してから移行した。
- upstream clang++ 22.1.3 で P1689 scan / compile と `ArtifactCoreVideo.lib` link が成功。移行後に単体 warm rebuild し、sccache 1 hit、miss / error / compilation failure 0。
- 実装本文は Git HEAD と一致し、LF。静的 registrar を含めて target の object にコンパイルされた。Spin の描画処理・登録動作は変更していない。対象 manifest に旧 `.cppm` path の残存なし。


### Artifact Audio implementation batch: mixer / scrub controller（2026-10-23）

- `Artifact/src/Audio/ArtifactAudioMixer.cppm` と `ArtifactAudioScrubController.cppm` を `.cpp` に移行し、`Artifact/cmake/ArtifactSources.cmake` の実装パスを更新。両者は既存の公開 `.ixx` と consumer を持ち、`ArtifactAppRuntime` に登録済み。宣言、W_OBJECT 実装、Audio / scrub behavior は変更していない。
- upstream clang++ 23.1.4 の CMake 生成 P1689 scan が両 `.cpp` で成功。既存 `.pcm` と CMake module map を使った実際の C++ compile も両方成功。sccache cold compile は2 misses / failure 0、同一コマンドの warm compile は2 hits / miss・cache error・failure 0。ArtifactAppRuntime 全体の再構築は約6,645 action（OpenUSD / Diligent / ArtifactCore 含む）を再実行するため、約1,898 action で停止した。よってアプリ全体の link は未確認。
- 以前の CMake source-option 登録で移行済み ArtifactCore `.cppm` を指す stale path が root `CMakeLists.txt` に残っていたため、該当する compile-option source paths を `.cpp` に揃えた。motion-path GPU module の名前生成 path も同様に更新した。
- 次に行うこと: `ArtifactAppRuntime` 全体 link は、CMake 再生成後の外部依存と移行済み ArtifactCore object 群が揃った build tree で確認する。

### Artifact Audio layer implementation batch: AudioLayer / SpatialAudioLayer（2026-10-23）

- `Artifact/src/Layer/ArtifactAudioLayer.cppm` と `ArtifactSpatialAudioLayer.cppm` を `.cpp` に移行し、`Artifact/cmake/ArtifactSources.cmake` の登録を更新。両者は公開 `.ixx` interface があり、既存の `ArtifactAppRuntime` target に登録済み。layer behavior と宣言は変更していない。
- upstream clang++ 23.1.4 の CMake 生成 P1689 scan と、既存 `.pcm` module map を使う実 compile が両方成功。単体 compile の sccache 初回は2 miss / failure 0、同一 compile command の再実行は2 hits / miss・error・failure 0。
- 生成 object は `ArtifactAudioLayer.cpp.obj` (1,651,996 bytes) と `ArtifactSpatialAudioLayer.cpp.obj` (1,309,921 bytes)。ArtifactAppRuntime 全体 link は未確認で、Audio mixer / scrub controller と同じく大規模な依存再構築を伴う。
- 対象ファイルは LF、Git HEAD と実装本文が一致。対象 manifest に旧2 `.cppm` path の残存なし。module hygiene check は pass。

### Artifact Dock style implementation: DockGlowStyle（2026-10-23）

- `Artifact/src/Widgets/Dock/DockGlowStyle.cppm` を `.cpp` に移行し、`Artifact/cmake/ArtifactSources.cmake` の登録を更新。公開 `Widgets.Dock.GlowStyle.ixx` と既存 module / widget behavior は変更していない。
- upstream clang++ 23.1.4 で公開 interface の P1689 scan と PCM 生成、implementation の P1689 scan と compile が成功。module map は同 module の PCM 1件のみを参照する。
- 実装 compile の sccache 再実行は hit。検証時の統計は3 requests / 3 hits / 0 misses / 0 errors / 0 failures。
- 実装本文は Git HEAD と一致し、LF。Artifact 全体 link は未確認。

### Artifact IO read implementation batch: DirectStorageReader / AsyncAssetReadScheduler（2026-10-23）

- `Artifact/src/IO/DirectStorageReader.cppm` と `AsyncAssetReadScheduler.cppm` を `.cpp` に移行し、`Artifact/cmake/ArtifactSources.cmake` の登録を更新。公開 `.ixx` interface、DirectStorage fallback、非同期 read scheduler の挙動は変更していない。
- upstream clang++ 23.1.4 の CMake 生成 P1689 scan と既存 module PCM を使う compile が両方成功。改行を LF に揃えた後も compile 成功。sccache warm compile は2 hits / miss・error・failure 0。
- 両実装本文は Git HEAD と一致し LF。対象 manifest に旧 `.cppm` path の残存なし。ArtifactAppRuntime 全体 link は未確認。

### Artifact Color Correction effects implementation batch: BlackAndWhite / Brightness / ColorBalance（2026-10-23）

- `Artifact/src/Effects/ColorCorrection/{BlackAndWhiteEffect,BrightnessEffect,ColorBalanceEffect}.cppm` を `.cpp` に移行。`Artifact/cmake/ArtifactSources.cmake` と `Artifact/CMakeLists.txt` の target source / compile-option path を更新し、effect module interface と処理内容は変更していない。
- upstream clang++ 23.1.4 で CMake 生成 P1689 scan と compile が3件とも成功。sccache warm compile は3 hits / miss・error・failure 0。
- 3実装本文は Git HEAD と一致し、LF。対象 CMake files に旧3 `.cppm` path の残存なし。Artifact effects target の link は未確認。

### Artifact Color Correction effects implementation batch: Colorama / ColorTint / ColorWheels / Curves / Exposure（2026-10-23）

- `Artifact/src/Effects/ColorCorrection/{ColoramaEffect,ColorTintEffect,ColorWheelsEffect,CurvesEffect,ExposureEffect}.cppm` を `.cpp` に移行。`Artifact/cmake/ArtifactSources.cmake` と `Artifact/CMakeLists.txt` の source / compile-option path を更新。公開 module interface と effect behavior は変更していない。
- upstream clang++ 23.1.4 で5件すべて P1689 scan と compile が成功。sccache warm compile は5 hits / miss・error・failure 0。
- 全実装本文は Git HEAD と一致し LF。対象 CMake files に旧5 `.cppm` path の残存なし。Color Correction target 全体の link は未確認。

### Artifact Color Correction effects implementation batch: ChannelMixer / Fill / GradientRamp / Grayscale / HueAndSaturation（2026-10-23）

- Artifact/src/Effects/ColorCorrection/{ChannelMixerEffect,FillEffect,GradientRampEffect,GrayscaleEffect,HueAndSaturation}.cppm を .cpp に移行。Artifact/cmake/ArtifactSources.cmake と Artifact/CMakeLists.txt の source / compile-option path を更新。module interface と effect behavior は変更していない。
- upstream clang++ 23.1.4 で5件すべて P1689 scan と compile が成功。sccache warm compile は5 hits / miss・error・failure 0。
- 全実装本文は Git HEAD と一致し、LF。対象 CMake files に旧5 .cppm path の残存なし。Color Correction target 全体の link は未確認。

### Artifact Color Correction effects implementation batch: Invert / Levels / PhotoFilter / Posterize / SelectiveColor（2026-10-23）

- `Artifact/src/Effects/ColorCorrection/{InvertEffect,LevelsEffect,PhotoFilterEffect,PosterizeEffect,SelectiveColorEffect}.cppm` を `.cpp` に移行。`Artifact/cmake/ArtifactSources.cmake` と `Artifact/CMakeLists.txt` の target source path を更新した。
- CMake configure で EffectService の明示登録 loop が InvertEffect を `.cppm` 固定で再追加していたことが判明。実在する `.cpp` を優先し、なければ `.cppm` を使うよう更新。upstream clang++ 23.1.4 で再configure成功。
- 5件すべて P1689 scan / compile 成功。sccache warm compile は5 hits / miss・error・failure 0。実装本文は Git HEAD と一致し、LF。target link は未確認。

### Artifact Color Correction effects implementation batch: LUT / ShadowHighlight / Threshold / Tritone / Vibrance（2026-10-23）

- Artifact/src/Effects/ColorCorrection/{LUTEffect,ShadowHighlightEffect,ThresholdEffect,TritoneEffect,VibranceEffect}.cppm を .cpp に移行。Artifact/cmake/ArtifactSources.cmake と Artifact/CMakeLists.txt の登録を更新し、implementation / behavior は変更していない。
- upstream clang++ 23.1.4 で5件すべて P1689 scan と compile が成功。LUTEffect は ArtifactAppRuntime、残る4件は ArtifactEffectsColor の生成 rule を使用した。sccache warm compile は5 hits / miss・error・failure 0。
- 全実装本文は Git HEAD と一致し LF。Color Correction target 全体の link は未確認。

### Artifact Distort effects implementation batch: Magnify / PinchBulge / PolarCoordinates / Ripple（2026-10-23）

- Artifact/src/Effects/Distort/{MagnifyEffect,PinchBulgeEffect,PolarCoordinatesEffect,RippleEffect}.cppm を .cpp に移行。Artifact/cmake/ArtifactSources.cmake と Artifact/CMakeLists.txt の共通 source / distortion-target registrations を更新。effect behavior は変更していない。
- upstream clang++ 23.1.4 で4件すべて P1689 scan と ArtifactEffectsDistort compile が成功。sccache warm compile は4 hits / miss・error・failure 0。
- 実装本文は Git HEAD と一致し LF。ArtifactEffectsDistort target の link は未確認。

### Artifact Blur effects implementation batch: AnisotropicFlow / ApertureShape / ReactionDiffusion（2026-10-23）

- Artifact/src/Effects/Blur/{AnisotropicFlowBlurEffect,ApertureShapeBlurEffect,ReactionDiffusionBlurEffect}.cppm を .cpp に移行。Artifact/cmake/ArtifactSources.cmake と Artifact/CMakeLists.txt の source registration を更新。blur behavior は変更していない。
- upstream clang++ 23.1.4 で3件すべて P1689 scan と ArtifactEffectsBlur compile が成功。sccache warm compile は3 hits / miss・error・failure 0。
- 実装本文は Git HEAD と一致し LF。ArtifactEffectsBlur target 全体の link は未確認。

### Artifact Stylize effects implementation batch: Dithering / Bevel / Kaleidoscope / Kuwahara（2026-10-23）

- Artifact/src/Effects/{Dithering/DitheringEffect,Bevel/BevelEffect,Kaleidoscope/KaleidoscopeEffect,Kuwahara/KuwaharaEffect}.cppm を .cpp に移行。Artifact/cmake/ArtifactSources.cmake と Artifact/CMakeLists.txt の source registrations を更新。
- Kaleidoscope / Dithering / Kuwahara の pinned APP_IMPL loop を実在する .cpp 優先に変更。basename が同じ別モジュール Rasterizer/KaleidoscopeEffect.cppm は .cppm のまま維持。
- upstream clang++ 23.1.4 で4件すべて P1689 scan と ArtifactEffectsStylize compile が成功。sccache warm compile は4 hits / miss・error・failure 0。各実装本文は Git HEAD と一致し LF。target link は未確認。

### Artifact Blur implementation batch: BlurEffect / GauusianBlur（2026-10-23）

- `Artifact/src/Effects/Blur/{BlurEffect,GauusianBlur}.cppm` を `.cpp` に移行し、`Artifact/cmake/ArtifactSources.cmake` と `Artifact/CMakeLists.txt` の登録を更新。Rasterizer 側の同名系モジュールは `.cppm` のまま維持した。
- upstream clang++ 23.1.4 で2件とも P1689 scan と compile が成功。warm sccache は2 hits / miss・error・failure 0。
- 実装本文は Git HEAD と一致し LF。ArtifactAppRuntime 全体の link は未確認。
### Artifact Keying implementation batch: LumaKey / ChromaKey / DifferenceKey / IBKKeyer（2026-10-23）

- `Artifact/src/Effects/Keying/{LumaKeyEffect,ChromaKeyEffect,DifferenceKeyEffect,IBKKeyerEffect}.cppm` を `.cpp` に移行し、共通manifestと `ArtifactEffectsKeying` 専用ソース登録を更新。公開 `.ixx` と処理本文は維持した。
- upstream clang++ 23.1.4（Windows MSVC ABI）で4件すべて P1689 scan / compile が成功。sccache cold は4 miss、warm は4 hit。cache error と compile failure は0。
- 全実装本文はGit HEADと一致しLF。module hygiene と diff check は成功。`ArtifactEffectsKeying` 全体の archive link は未確認。
### Artifact Glow implementation batch: 8 effects（2026-10-23）

- `Artifact/src/Effects/Glow/{ChromaticGlowEffect,EdgeBloomEffect,GlowEffect,LiquidGlowEffect,LuminescenceCausticsEffect,PhysicalHalationEffect,ReactiveGlowEffect,ResidualGlowEffect}.cppm` を `.cpp` に移行し、共通manifest、application source list、`ArtifactEffectsGlow` 登録を更新。
- Glow variant の APP_IMPL 登録は既存 `.cpp` を優先し、未移行ソース向けに `.cppm` fallback を維持。置換時に同名末尾を持つ `Rasterizer/GlowEffect.cppm` と `DirectionalGlowEffect.cppm` の登録を保持することも確認した。
- upstream clang++ 23.1.4（Windows MSVC ABI）で8件すべて P1689 scan / compile 成功。sccache cold 8 miss、warm 8 hit。cache error と compile failure は0。
- 全実装本文は Git HEAD と一致し LF。module hygiene と diff check は成功。 `ArtifactEffectsGlow` 全体のarchive linkは未確認。

### Artifact Generate implementation batch: SimpleRain / RadioWaves（2026-10-23）

- `Artifact/src/Effects/Generate/{SimpleRainEffect,RadioWavesEffect}.cppm` を `.cpp` に移行し、共通manifest、application source list、`ArtifactEffectsGenerate` 専用source listを更新。module interfaceと実装内容は維持した。
- upstream clang++ 23.1.4（Windows MSVC ABI）で両件のP1689 scan / compileが成功。sccache cold 2 miss、warm 2 hit。cache errorとcompile failureは0。
- 両実装本文はGit HEADと一致しLF。module hygieneとdiff checkは成功。`ArtifactEffectsGenerate` 全体のarchive linkは未確認。
### Artifact Transform implementation batch: Bend / Twist（2026-10-23）

- `Artifact/src/Effects/Transform/{BendTransform,TwistTransform}.cppm` を `.cpp` に移行し、`Artifact/cmake/ArtifactSources.cmake` の implementation manifestを更新。公開module interfaceとtransform処理は変更していない。
- upstream clang++ 23.1.4（Windows MSVC ABI）で両件のP1689 scan / `ArtifactAppRuntime` object compileが成功。sccache cold 2 miss、warm 2 hit。cache errorとcompile failureは0。
- 両実装本文はGit HEADと一致しLF。module hygieneとdiff checkは成功。`ArtifactAppRuntime` 全体のlinkは未確認。
### Artifact Rasterizer temporal implementation batch: 14 effects（2026-10-23）

- `Artifact/src/Effects/Rasterizer/{Echo,Feedback,FrameBlend,FrameAverage,FrameAccumulation,FreezeFrame,TemporalDenoise,TemporalMedian,TemporalSmear,TimeBlur,TimeWarp,TrailFade,SlitScan,Strobe}Effect.cppm` を `.cpp` に移行。common manifest、application source list、`ArtifactEffectsTemporal` module / implementation listsを更新した。
- 全14件は `module Artifact.Effect.Rasterizer.*;` のimplementation unit。upstream clang++ 23.1.4（Windows MSVC ABI）で14件すべてP1689 scan / target object compile成功。sccache warm compileも14件すべてhit。`ArtifactEffectsTemporal` archive linkは未確認。
- 全実装本文はGit HEADと一致しLF。対象manifestに旧14 `.cppm` pathの残存なし。module hygieneとdiff check成功。
- 同時調査したOfxの2 `.cppm` は `export module` を宣言するinterface sourceで、implementation移行の対象外と判明したため拡張子変更を戻した。Clang単体compile時に現れたPCM / export redeclaration問題は `Insight.md` に記録。
### Artifact Rasterizer motion-flow implementation batch: 5 effects（2026-10-23）

- `Artifact/src/Effects/Rasterizer/{OpticalFlowBlur,VectorBlur,VectorFlowGlitch,LightTrails,MotionTrail}Effect.cppm` を `.cpp` に移行。共通manifest、application source list、`ArtifactEffectsMotion` module / implementation listsを更新した。
- 5件はすべて `module Artifact.Effect.Rasterizer.*;` のimplementation unit。upstream clang++ 23.1.4（Windows MSVC ABI）でP1689 scan / target object compileが成功し、warm sccacheは5件すべてhit。`ArtifactEffectsMotion` archive linkは未確認。
- 全実装本文はGit HEADと一致しLF。対象manifestに旧5 `.cppm` pathの残存なし。module hygieneとdiff check成功。
### Artifact Rasterizer patterns implementation batch: 5 effects（2026-10-23）

- `Artifact/src/Effects/Rasterizer/{Bricks,HexGrid,Halftone,Stripes,Voronoi}Effect.cppm` を `.cpp` に移行し、共通manifest、application source list、`ArtifactEffectsPatterns` の登録を更新。
- 5件はすべて `module Artifact.Effect.Rasterizer.*;` のimplementation unit。upstream clang++ 23.1.4（Windows MSVC ABI）でP1689 scan / target object compileが全件成功し、warm sccacheは5件すべてhit。target archive linkは未確認。
- 全実装本文はGit HEADと一致しLF。対象manifestに旧5 `.cppm` pathの残存なし。module hygieneとdiff check成功。
### Artifact effects finishing implementation batch: 4 effects（2026-10-23）

- `Artifact/src/Effects/{Rasterizer/RadialBlurEffect,Satin/SatinEffect,Stroke/StrokeEffect,Rasterizer/VignetteEffect}.cppm` を `.cpp` に移行し、common manifest、application source list、`ArtifactEffectsFinishing` のsource registrationを更新。
- 4件はimplementation unit。upstream clang++ 23.1.4（Windows MSVC ABI）でP1689 scan / target object compileが全件成功し、warm sccacheは4件すべてhit。`ArtifactEffectsFinishing` archive linkは未確認。
- 全実装本文はGit HEADと一致しLF。旧4 `.cppm` pathの残存なし。module hygieneとdiff check成功。
### Artifact Rasterizer temporal-context implementation batch: 2 effects（2026-10-23）

- `Artifact/src/Effects/Rasterizer/{PosterizeTime,ScreenShake}Effect.cppm` を `.cpp` に移行し、共通manifest、application source list、`ArtifactEffectsTemporalContext` のsource registrationを更新。
- 2件とも `module Artifact.Effect.Rasterizer.*;` のimplementation unit。upstream clang++ 23.1.4（Windows MSVC ABI）でP1689 scan / target object compileが成功し、warm sccacheは2件ともhit。target archive linkは未確認。
- 両実装本文はGit HEADと一致しLF。対象2旧pathはmanifestから除去済み。module hygieneとdiff check成功。
### Artifact Rasterizer contextual implementation batch: 4 effects（2026-10-23）

- `Artifact/src/Effects/Rasterizer/{DifferenceMatte,Edge,Ghost,PixelSort}Effect.cppm` を `.cpp` に移行し、共通manifest、application source list、`ArtifactEffectsContextual` の登録を更新。
- 4件すべて `module Artifact.Effect.Rasterizer.*;` のimplementation unit。upstream clang++ 23.1.4（Windows MSVC ABI）でP1689 scan / target object compileが成功し、warm sccacheは4件すべてhit。target archive linkは未確認。
- 全実装本文はGit HEADと一致しLF。対象manifestに旧4 `.cppm` pathなし。module hygieneとdiff check成功。
### Artifact Rasterizer chromatic and digital implementation batches（2026-10-23）

- Chromatic packの `ChromaticAberrationEffect` / `ChromaticReliefEffect` と Digital packの `DataMoshEffect` / `GlitchEffect` / `FilmDamageEffect` / `DeBlinkEffect` / `DeflickerEffect` を `.cppm` から `.cpp` に移行。共通manifest、application source list、`ArtifactEffectsChromatic` / `ArtifactEffectsDigital` の登録を更新。
- 7件すべて `module Artifact.Effect.Rasterizer.*;` のimplementation unit。upstream clang++ 23.1.4（Windows MSVC ABI）で全件P1689 scan / target object compileが成功し、warm sccacheは7件すべてhit。両packのarchive linkは未確認。
- 全実装本文はGit HEADと一致しLF。旧7 `.cppm` pathの残存なし。module hygieneとdiff check成功。
### Artifact effects filters implementation batch: 6 effects（2026-10-23）

- `Artifact/src/Effects/{LinearWipe/LinearWipeEffect,Liquify/LiquifyEffect,Mosaic/MosaicEffect,Spherize/SpherizeEffect,Sharpen/SharpenEffect,FindEdges/FindEdgesEffect}.cppm` を `.cpp` に移行し、共通manifest、application source list、`ArtifactEffectsFilters` 登録を更新。
- 6件すべて `module` implementation unit。upstream clang++ 23.1.4（Windows MSVC ABI）でP1689 scan / target object compileが全件成功。warm sccacheは6件すべてhit。target archive linkは未確認。
- 全実装本文はGit HEADと一致しLF。旧6 source pathsはmanifestから除去済み。module hygieneとdiff check成功。
### Artifact effects shadows implementation batch: 3 effects（2026-10-23）

- `Artifact/src/Effects/{DropShadow/DropShadowEffect,InnerShadow/InnerShadowEffect,RadialShadow/RadialShadowEffect}.cppm` を `.cpp` に移行し、共通manifest、application source list、`ArtifactEffectsShadows` の登録を更新。
- 3件とも `module Artifact.Effect.Rasterizer.*;` のimplementation unit。upstream clang++ 23.1.4（Windows MSVC ABI）でP1689 scan / target object compileが成功し、warm sccacheは3件すべてhit。archive linkは未確認。
- 全実装本文はGit HEADと一致しLF。module hygieneとdiff check成功。
### Artifact effects optics implementation batch: LensDistortion / OpticsCompensation（2026-10-23）

- `Artifact/src/Effects/{LensDistortion/LensDistortionEffect,OpticsCompensation/OpticsCompensationEffect}.cppm` を `.cpp` に移行し、共通manifest、application source list、`ArtifactEffectsOptics` の登録を更新。
- 2件とも `module` implementation unit。upstream clang++ 23.1.4（Windows MSVC ABI）でP1689 scan / target object compileが成功し、warm sccacheは2件ともhit。target archive linkは未確認。
- 両実装本文はGit HEADと一致しLF。module hygieneとdiff check成功。
### Artifact effects color/noise implementation batch: 3 effects（2026-10-23）

- `Artifact/src/Effects/{LiftGammaGainEffect,WhiteBalanceEffect,AddNoise/AddNoiseEffect}.cppm` を `.cpp` に移行し、共通manifest、application source list、`ArtifactEffectsColor` / `ArtifactEffectsNoise` の登録を更新。Noise packのNoiseEffectは既に `.cpp` 移行済みで、これにより同packの2実装が揃った。
- 3件ともimplementation unit。upstream clang++ 23.1.4（Windows MSVC ABI）でP1689 scan / target object compileが成功し、warm sccacheは3件すべてhit。target archive linkは未確認。
- 全実装本文はGit HEADと一致しLF。module hygieneとdiff check成功。
### Artifact effect runtime implementation batch: AbstractEffect / FrameSampler / GlobalEffectManager（2026-10-23）

- `Artifact/src/Effects/{ArtifactAbstractEffect,ArtifactEffectFrameSampler,ArtifactGlobalEffectManager}.cppm` を `.cpp` に移行。`Artifact/CMakeLists.txt` と `Artifact/cmake/ArtifactSources.cmake` の登録を更新した。
- 3件とも module implementation unit。upstream clang++ 23.1.4（Windows MSVC ABI）でP1689 scan / target object compileが成功し、warm sccache compileも3件すべて成功。AbstractEffectには既存の `getenv` deprecated warningが1件ある。
- 全実装本文はGit HEADと一致しLF。module hygieneとdiff check成功。target archive linkは未確認。


### Artifact export implementation batch: 7 writers and session units（2026-10-23）

- `Artifact/src/Export/{ArtifactExportGamefaceWriter,ArtifactExportLottieWriter,ArtifactExportNoesisXamlWriter,ArtifactExportPreRenderPipeline,ArtifactExportRmlUiWriter,ArtifactExportSession,ArtifactExportUnityUxmlWriter}.cppm` を `.cpp` に移行し、`Artifact/CMakeLists.txt` と `Artifact/cmake/ArtifactSources.cmake` の明示登録を更新。7件すべて公開 `.ixx` と対になる `module` implementation unit で、P1689 scan は全件成功した。
- upstream clang++ 23.1.4 / Windows MSVC ABI で Gameface / Lottie / Noesis Xaml / RmlUi / Unity UXML の5件は object compile 成功。warm sccache は5 requests / 5 hits / 0 misses / 0 errors / 0 failures。
- PreRenderPipeline / Session は `Artifact.Render.CompositionViewDrawing.pcm` が既存 build tree にないためcompile未完了。依存の `ArtifactOffscreenCompositionRenderer.cppm` は個別scan / compile成功したが、`ArtifactCompositionViewDrawing.cppm` のcompileでclang frontendがexit 70でクラッシュし、診断はsource-manager location usage 2.14 GB / available 99%を報告。2件の対象translation unit自体のエラーとは判定していない。
- 7件すべての実装本文はGit HEADと一致し、LF。module hygiene と diff check は成功。`ArtifactAppRuntime` 全体のlinkは未確認。


### Artifact ColorNode implementation pair: Node / NodeGraph（2026-10-23）

- `Artifact/src/Color/{ArtifactColorNode,ArtifactColorNodeGraph}.cppm` を `.cpp` に移行し、`Artifact/CMakeLists.txt` と `Artifact/cmake/ArtifactSources.cmake` の `ArtifactColorNode` target registrationsを更新。両方とも対になる公開 `.ixx` を持つimplementation unit。
- CMake 4.4.4 / Ninja / upstream clang++ 23.1.4（Windows MSVC ABI）で両実装のP1689 scanとobject compileが成功。生成済みtarget archive commandも成功し、`ArtifactColorNode.lib` を作成した。warm sccacheは2 requests / 2 hits / 0 misses / 0 errors / 0 failures。
- `ninja ArtifactColorNode` 自体は依存再構築を3,179 stepsと見積もり、OpenUSD等550 stepsを進めたところで停止したため、通常のNinja target build完了としては扱わない。対象実装・既存interface objectからのarchive生成は直接コマンドで確認した。
- 両実装本文はGit HEADと一致しLF。module hygieneとdiff check成功。リンクは対象targetのstatic archiveまでで、アプリ統合linkは未確認。


### Artifact Distort implementation batch: DisplacementMap / TimeDisplacement / TurbulentDisplace（2026-10-23）

- 3実装を `.cppm` から `.cpp` へ移行済みの状態を確認し、Distort の明示 APP_IMPL pinning が旧 `TimeDisplacementEffect.cppm` を再追加していた箇所を `.cpp` 優先・`.cppm` fallback に修正した。
- upstream clang++ 23.1.4 / Windows MSVC ABI で3件すべて P1689 scan と object compile に成功。sccache cold は3 miss、warm は3 hit。累積の既存 cache error / compilation failure は増えていない。
- 既存7 objectを含めた `ArtifactEffectsDistort.lib` を直接再アーカイブし成功。Ninja の dyndep target は third-party を含む2,430 actionsを要求したため486 actionsで停止し、対象 scan / compile と archive のみに範囲を絞って検証した。通常のNinja target build完了とは扱わない。
- CMake configure / generate 成功。distort target 全体の依存ビルドとアプリ統合linkは未確認。


### Artifact Wave / SurfaceFX implementation batch（2026-10-23）

- `Artifact/src/Effects/Wave/WaveEffect.cppm` と `SurfaceFX/SurfaceFXEffect.cppm` を `.cpp` へ移行し、`Artifact/cmake/ArtifactSources.cmake`、application source list、各専用 target (`ArtifactEffectsWave` / `ArtifactEffectsSurfaceFX`) の実装登録を更新した。
- upstream clang++ 23.1.4 / Windows MSVC ABI で両件の P1689 scan / object compile と各 static archive の再生成に成功。sccache cold 2 miss、warm 2 hit。既存累積 cache error / compilation failure 数は増えていない。
- 両実装本文はGit HEADと一致しLF。古い `.cppm` registration は対象 manifest から除去済み。CMake configure / generate も成功。
- target 全体の通常Ninja buildと、Artifactアプリ統合linkは未確認。前回のdyndep targetがthird-partyまで再構築することを確認済みのため、既存DDI群から対象module mapを生成して変更対象を限定した。


### Artifact spatial-specialty implementation batch: DirectionalGlow / AutoMosaic（2026-10-24）

- `Artifact/src/Effects/DirectionalGlowEffect.cppm` と `AutoMosaicEffect.cppm` を `.cpp` へ移行し、共通 source manifest、application source list、`ArtifactEffectsGlow` / `ArtifactEffectsAutoMosaic` の target source registrationを更新した。
- upstream clang++ 23.1.4 / Windows MSVC ABI で両件の P1689 scan / object compile と各 static archive の再生成に成功。sccache cold 2 miss、warm 2 hit。累積 cache error / compilation failure は増えていない。
- 両実装本文はGit HEADと一致しLF。CMake configure / generate成功。target全体の通常Ninja buildとArtifactアプリ統合linkは未確認。


### Artifact Generator Particle implementation pair（2026-10-24）

- `Artifact/src/Generator/{ArtifactParticleGenerator,ParticleEmitterDescription}.cppm` を `.cpp` に移行し、`Artifact/cmake/ArtifactSources.cmake` の明示 implementation source pathsを更新した。両方が `module Artifact.Generator.Particle;` を提供するimplementation unitで、`ArtifactParticleGenerator.ixx` のpublic module interfaceは維持した。
- upstream clang++ 23.1.4 / Windows MSVC ABI で2件のP1689 scan / object compileが成功。sccache cold 2 miss、warm 2 hit。実装本文はGit HEADと一致しLF。
- `ArtifactAppRuntime` は1,063 module DDIを持ち、現build treeで207 DDIが未生成だったため、全target build / application linkは未確認。対象interfaceと2 implementationのDDIから限定module mapを生成し、両objectをcompileした。

### Artifact Generator Clone / Effector implementation batch（2026-10-24）

- `Artifact/src/Generator/{CloneGenerator,AbstractGeneratorEffector}.cppm` を `.cpp` に移行し、`Artifact/cmake/ArtifactSources.cmake` の explicit implementation source pathsを更新した。両方とも対応する public `.ixx` を持つ implementation unit。
- upstream clang++ 23.1.4 / Windows MSVC ABI で `CloneGenerator.cpp` は既存の通常 CMake module mapを用いた object compileに成功。P1689 scanも成功。
- `AbstractGeneratorEffector.cpp` はP1689 scan成功。手動生成した局所module mapでのcompileは、`Artifact.Layer.Abstract.pcm` が間接依存する `Utils.Result` 等のPCM不足および既存PCMのstale状態で停止した。interface自体には `Memory.SharedPtr` と `Artifact.Layer.Abstract` の importがある。source固有エラーとは断定せず、対象objectはcompile成功扱いにしない。再configureは成功しているが、通常ArtifactAppRuntimeのdyndep build / app linkは未完了。
- `CloneGenerator.cpp` の通常CMake object compileは通った。これら2件のsource bodyは元の `.cppm` から保持し、拡張子と明示source pathだけを変えた。ArtifactCore子リポジトリは変更していない。
### Artifact color management / grading implementation pair（2026-10-24）

- `Artifact/src/Color/{ArtifactColorManagement,ArtifactColorGradingEngine}.cppm` を `.cpp` に移行し、`Artifact/cmake/ArtifactSources.cmake` と `Artifact/CMakeLists.txt` の explicit source pathsを更新した。public `.ixx` interfacesと module namesは維持した。
- CMake configure / generate は CMake 4.2、Ninja、upstream clang++ 23.1.4（GNU frontend / Windows MSVC ABI）で成功。両sourceのP1689 scanとtarget object compileが成功し、各専用static archive (`ArtifactColorManagement.lib`, `ArtifactColorGrading.lib`) も再生成した。
- sccache 0.18.0 の最終warm compile 2件は2 hits / 0 misses。既存集計のcache errors 4 / compilation failures 5は増加しなかった。
- 両source本文は Git HEAD の `.cppm` と一致し、LF-only。通常のNinja target buildは3,181 actionsへ広がり534 actions後に停止したため、Ninja管理下のtarget全体buildと app linkは未確認。対象DDIからCMake `cmake_ninja_dyndep` で生成した正規module mapでcompileし、既存interface objectを含むarchiveを再生成して確認した。
### Artifact Playback Service implementation pair（2026-10-24）

- `Artifact/src/Service/{ArtifactPlaybackService,ArtifactPlaybackShortcuts}.cppm` を `.cpp` に移行し、`Artifact/cmake/ArtifactSources.cmake` の explicit source pathsを更新。両公開 `.ixx` は維持。
- upstream clang++ 23.1.4 / Windows MSVC ABIで両interfaceと両implementationの個別compileに成功。最初の失敗は限定dyndep mapにimport依存閉包を含めなかった手動検証上の問題。正しい既存CMake build treeの通常生成module mapでは4 compile成功。
- CMake configure / generate成功。全target build、sccache warm hit、およびArtifact executable linkは未確認。

### Artifact ColorScience / OCIO manager implementation pair（2026-10-24）

- `Artifact/src/Color/{ArtifactColorScienceManager,ArtifactOCIOManager}.cppm` を `.cpp` に移行し、`Artifact/cmake/ArtifactSources.cmake` と `Artifact/CMakeLists.txt` の明示登録（app source manifest、cycle pinning、OCIO cycle ownership list）を更新した。両方とも既存 public `.ixx` と module nameを維持した。
- CMake configure / generate は CMake 4.2 / Ninja / upstream clang++ 23.1.4（GNU frontend / Windows MSVC ABI）で成功。両 public interfaceを順にcompileしてPCMを生成後、implementation sourceのP1689 scan / object compileも成功した。`Artifact.Event.Types` provider DDIも含むCMake `cmake_ninja_dyndep` mapを使い、ColorScienceとOCIO間のmodule依存を確認した。
- sccache 0.18.0のwarm compileは2 requests / 2 hits / 0 misses。cache errors 4、compilation failures 5の既存集計は増えていない。両source本文はGit HEADと一致しLF-only。古い `.cppm` pathはArtifact CMake / source manifestに残っていない。
- この pairは `ArtifactAppRuntime` 所属のため専用static archiveはない。通常Ninjaの全dyndep buildとArtifact executable linkは未確認。実装は対象5 DDIからCMake生成した限定dyndep mapを使った直接compileで確認した。

### Artifact Composition implementation follow-up（2026-10-24）

- 現行ツリーでは `ArtifactInOutPoints.cpp` と `ArtifactParametricCompositionResolver.cpp` が存在し、`Artifact/cmake/ArtifactSources.cmake` も `.cpp` を登録している。`ArtifactCompositionPlaybackController` は `.cppm` のままで、同 manifest も `.cppm` を参照しているため、3件セットの移行完了とは扱わない。
- PlaybackController を `.cpp` として試した際は、`ArtifactPlaybackAudioDiagnostics` の宣言元 `Artifact.Playback.Engine` の直接 import 不足が判明した。直接 importを加える案では Playback.Engine interface が PlaybackControllerを importする循環になるため、Controllerの移行は保留した。
- InOutPoints / ParametricCompositionResolver の現行 `.cpp` に対する Clang object compile は、この追記時点で成功を確認できていない。過去の configure / scan記録だけを compile成功の根拠にはしない。

### Artifact Film / MotionBlur implementation batch（2026-10-24）

- `Artifact/src/Effect/{ArtifactFilmEffects,ArtifactMotionBlur}.cppm` を `.cpp` に移し、`Artifact/cmake/ArtifactSources.cmake` の implementation pathsを更新した。公開 `.ixx` と module名（`Artifact.Effect.Film` / `Artifact.Effect.MotionBlur`）は維持した。
- CMake 4.2 / Ninja / Clang GNU frontend / Windows MSVC ABI で configure / generate成功。両 `.cpp` の P1689 scanと、Ninja生成の sccache compile commandを用いた object compileを各2回実行し、全4回成功した。コンパイルには既存のCMake module mapを使った。
- 移動後のソース bytes はそれぞれ Git HEAD の `.cppm` 本文と一致し、LF-only。対象2件は `ArtifactAppRuntime` に属するため専用 static archiveはない。通常のNinja object buildはOpenUSD / Diligentを含む5,567 actionsへ広がり、54 actionsまで進んだ段階で停止した。対象 object compileは成功しているが、Ninja target完了とArtifact executable linkは未確認。
- sccache の作業全体の統計は、開始時の記録から requests 1458→1514、hits 1125→1127、misses 270→322、cache errors 4のまま、compilation failures 7→8。対象2ソースの4回の compileは成功したが、途中の広域Ninja buildで失敗した translation unitは特定できていない。

### Artifact Effect implementation batch: CornerPin / Preset / Transition（2026-10-24）

- `Artifact/src/Effect/{ArtifactCornerPinEffect,ArtifactEffectPreset,ArtifactTransition}.cppm` を `.cpp` に移し、`Artifact/cmake/ArtifactSources.cmake` の登録を更新した。対応する公開 `.ixx` と module名（`Artifact.Effect.CornerPin` / `Artifact.Effect.Preset` / `Artifact.Effect.Transition`）は維持した。
- CMake 4.2 / Ninja / Clang 22.1.3 GNU frontend / Windows MSVC ABI で configure / generate成功。fresh P1689 scan後、各 implementation objectを既存CMake module mapで2回ずつ compileし、全6回成功した。mapの全PCM providerが存在することも事前確認した。
- sccache 0.18.0 の差分は6 requests、3 misses、3 hits、cache errors 0増、compilation failures 0増。ソースbytesは移行前のGit HEAD本文と一致しLF-only。旧 `.cppm` のCMake / manifest参照は対象範囲から除去した。
- 3件とも `ArtifactAppRuntime` に属するため専用static archiveはない。Ninjaの全依存buildとArtifact executable linkは未確認。直前の同target要求がOpenUSD / Diligentを含む5,567 actionsへ展開したため、今回は限定compileに留めた。

### Artifact Mask implementation pair: LayerMask / MaskPath（2026-10-24）

- `Artifact/src/Mask/{LayerMask,MaskPath}.cppm` を `.cpp` に移し、`Artifact/cmake/ArtifactSources.cmake` の登録を更新した。public `.ixx` と module名（`Artifact.Mask.LayerMask` / `Artifact.Mask.Path`）は維持した。両sourceは相互の module ownershipを変えず、LayerMask implementationは既存どおり `Artifact.Mask.Path` interfaceをimportする。
- CMake 4.2 / Ninja / Clang 22.1.3 GNU frontend / Windows MSVC ABI で configure / generate成功。fresh P1689 scan後、既存CMake module mapの全PCM provider存在を確認して各 objectを2回 compileし、全4回成功した。sccache差分は4 requests / 2 misses / 2 hits、cache errorsとcompilation failuresは各0増。
- `LayerMask.cppm` のworktree bytesはGit HEADよりCRLFだったため `.cpp` 移動時にLFへ正規化した。正規化後の両 `.cpp` 本文はGit HEADと一致し、LF-only。旧 `.cppm` pathはsource manifestから除去済み。
- 両objectは `ArtifactAppRuntime` に属するため専用static archiveはない。通常Ninja target buildとArtifact executable linkは未確認。前回同targetの依存要求はOpenUSD / Diligentを含む5,567 actionsへ広がったため、対象に限定したcompileで検証した。


### Artifact Project health / statistics implementation pair（2026-10-24）

- `Artifact/src/Project/{ArtifactProjectHealthChecker,ArtifactProjectStatistics}.cppm` を `.cpp` に移し、`Artifact/cmake/ArtifactSources.cmake` の implementation pathsを更新した。public `.ixx` と module名は維持した。
- Clang 22.1.3 GNU frontend / Windows MSVC ABIで両sourceのfresh P1689 scanが成功し、sccache経由のobject compileを各2回実行して全4回成功した。Statistics implementationは `ProjectItem` を使うため、所有元 `Artifact.Project.Items` の直接importを追加した。
- HealthChecker compile前に stale `Memory.SharedPtr.pcm` が削除済み `.cppm` pathを参照していたため、現行 `ArtifactCore/src/Memory/SharedPtr.cpp` のDDIからbuild-tree内module mapを作り直し、PCM/objectを再生成した。ArtifactCoreのsourceや登録は変更していない。
- Project source本文は旧ファイルをLFで読み直した結果、Statisticsの直接import追加以外はGit HEADの `.cppm` と一致。両 `.cpp` はLF-only。CMake configure / generate成功。通常のNinja全target buildとアプリlinkは未確認。


### Artifact Composition Space cache / Onion Skin history implementation pair（2026-10-24）

- `Artifact/src/Widgets/Render/{ArtifactCompositionSpaceGpuCache,ArtifactCompositionOnionSkinHistory}.cppm` を `.cpp` に移し、`Artifact/cmake/ArtifactSources.cmake` の明示 implementation pathsを更新した。public `.ixx` と module名は維持。両者は同じRender UI領域で、同じ `Artifact.Render.IRenderer` interfaceを直接importする。
- CMake 4.2 / Ninja / Clang 22.1.3 GNU frontend / Windows MSVC ABIでconfigure / generateと両sourceのfresh P1689 scanが成功。各module mapの53 PCM providerすべてを確認し、Ninja生成のsccache compile commandで各objectを2回ずつコンパイル、計4回成功した。
- 両source本文は旧 `.cppm` のLF正規化内容と一致し、LF-only。全target build、アプリlink、sccache統計差分は未確認。
- 同じProject領域のImporter / Managerも候補として試行したが、ImporterのClang frontendが約2.1GBのAST読み込み後にクラッシュしたため、両sourceはtracked HEADの `.cppm` に戻してmanifestも元に復元した。未検証の状態で移行済みに数えない。


### Artifact Render Queue UI implementation pair（2026-10-24）

- `Artifact/src/Widgets/Render/{ArtifactRenderQueuePresetSelector,ArtifactRenderQueueJobPanel}.cppm` を `.cpp` に移し、`Artifact/cmake/ArtifactSources.cmake` の明示 implementation pathsを更新した。public `.ixx` とmodule名を維持。Render Queueのプリセット選択とジョブ詳細表示を同じまとまりとして移行した。
- CMake 4.2 / Ninja / Clang 22.1.3 GNU frontend / Windows MSVC ABIでconfigure / generate、両sourceのfresh P1689 scanが成功。Queue Job / Queue Service / 両Widgetの公開interfaceを個別compileしてPCMを生成後、各sourceの3個・27個のmodule map providerを確認。Ninja生成sccache commandで各objectを2回ずつcompileし、計4回成功した。
- 両implementationのsource bodyは旧 `.cppm` のLF正規化内容と一致しLF-only。全target build、アプリlink、sccache hit/miss差分は未確認。


### Artifact Plugin loader / sandbox implementation pair（2026-10-24）

- `Artifact/src/Plugin/{PluginSandbox,PluginLoader}.cppm` を `.cpp` に移し、`Artifact/cmake/ArtifactSources.cmake` の明示 implementation pathsを更新。両public `.ixx` とmodule名は維持した。PluginLoaderからPluginSandboxへの依存は一方向で、Sandbox側にLoaderへのimportはない。
- CMake 4.2 / Ninja / Clang 22.1.3 GNU frontend / Windows MSVC ABIでconfigure / generateと両sourceのfresh P1689 scanが成功。既存CMake module mapの5/7 PCM providerがすべて存在し、Ninja生成sccache compile commandによる両objectの2回ずつのcompile（計4回）も成功した。
- 両source bodyは元の `.cppm` のLF正規化内容と一致し、LF-only。全target build、アプリlink、sccache hit/miss差分は未確認。


### Artifact Render motion blur / depth of field compute pass pair（2026-10-24）

- `Artifact/src/Render/{ArtifactMotionBlurPass,ArtifactDepthOfFieldPass}.cppm` を `.cpp` に移し、`Artifact/cmake/ArtifactSources.cmake` と `Artifact/CMakeLists.txt` の explicit implementation pathsを更新した。public `.ixx` とmodule名は維持した。
- Diligent compute passのshader・dispatch・resource処理には変更を加えていない。CMake 4.2 / Ninja / Clang 22.1.3 GNU frontend / Windows MSVC ABIでconfigure / generateと両sourceのfresh P1689 scanが成功。`ArtifactRender` targetの既存module mapsにある5/6 PCM providerを確認し、Ninja生成sccache commandで各objectを2回ずつcompile、計4回成功した。
- 両source bodyは元 `.cppm` のLF正規化内容と一致しLF-only。`ArtifactRender` 全体buildとArtifact executable linkは未確認。


### Artifact FrameCache / FinalPostProcess implementation pair（2026-10-24）

- `Artifact/src/Render/{ArtifactFrameCache,ArtifactFinalPostProcess}.cppm` を `.cpp` に移し、`Artifact/cmake/ArtifactSources.cmake` と `Artifact/CMakeLists.txt` の explicit implementation pathsを更新した。public `.ixx` とmodule名は維持。
- FrameCacheの既存cache behavior、画像表現、hot-path処理、およびFinalPostProcessのDiligent処理には変更を加えていない。CMake 4.2 / Ninja / Clang 22.1.3 GNU frontend / Windows MSVC ABIでconfigure / generate、両sourceのfresh P1689 scanが成功。`ArtifactRender` targetの既存module mapsにある6/9 PCM providersを確認し、Ninja生成sccache commandで各objectを2回ずつcompile、計4回成功した。
- 両source bodyは旧 `.cppm` のLF正規化内容と一致しLF-only。全target buildとArtifact executable linkは未確認。


### Artifact Layer PhysicsBridge / FluidRuntimeState implementation pair（2026-10-24）

- `Artifact/src/Layer/{ArtifactLayerPhysicsBridge,ArtifactLayerFluidRuntimeState}.cppm` を `.cpp` に移し、`Artifact/cmake/ArtifactSources.cmake` の explicit implementation pathsを更新した。public `.ixx` とmodule名は維持。どちらもlayer simulation runtimeの経路である。
- CMake 4.2 / Ninja / Clang 22.1.3 GNU frontend / Windows MSVC ABIでconfigure / generateと両sourceのfresh P1689 scanが成功。既存CMake module mapsの25/55 PCM providerはファイルとして揃っていたが、PhysicsBridge compile時に古いArtifactCore PCMが削除済み `.cppm` source pathsを参照した。現行ArtifactCore `.cpp` sourcesから `ArtifactOptional`、`SoftBodySolver`、`ClothSolver3D`、`FluidSolver2D`、`Physics2D`、`MpmSolver2D`、`PhysicsSystem` のbuild-tree object/PCMを依存順に再生成後、各Layer objectを2回ずつcompileして計4回成功した。ArtifactCoreのsourceやmanifestは変更していない。
- 両Layer source bodyは旧 `.cppm` のLF正規化内容と一致しLF-only。全target buildとArtifact executable linkは未確認。


### Artifact RenderManager / 3D primitive submitter implementation pair（2026-10-24）

- `Artifact/src/Render/{ArtifactRenderManager,Artifact3DPrimitiveSubmitter}.cppm` を `.cpp` に移し、`Artifact/cmake/ArtifactSources.cmake` の明示implementation pathsを更新した。両public `.ixx` とmodule名を維持し、`ArtifactAppRuntime` targetに所属する。
- `Artifact3DPrimitiveSubmitter` はDiligent submission unitだが、GPU処理・staging behaviorには変更なし。CMake 4.2 / Ninja / Clang 22.1.3 GNU frontend / Windows MSVC ABIでconfigure / generate、両sourceのfresh P1689 scanが成功。既存module mapsの1/3 PCM providersを確認し、Ninja生成sccache commandで各objectを2回ずつcompile、計4回成功した。
- 両source bodyは元 `.cppm` のLF正規化内容と一致しLF-only。全target buildとArtifact executable linkは未確認。


### ArtifactRenderSupport: RamPreviewController / HDRMonitor implementation pair（2026-10-24）

- `Artifact/src/Render/{ArtifactRamPreviewController,ArtifactHDRMonitor}.cppm` を `.cpp` に移し、`Artifact/cmake/ArtifactSources.cmake` と `Artifact/CMakeLists.txt` の explicit `ARTIFACT_RENDER_SUPPORT_IMPL` pathsを更新した。両public `.ixx` とmodule名は維持した。
- CMake 4.2 / Ninja / Clang 22.1.3 GNU frontend / Windows MSVC ABIでconfigure / generateと `ArtifactRenderSupport` 用のfresh P1689 scanが成功。既存module mapsの11/13 PCM providersはすべて存在し、Ninja生成sccache commandで各objectを2回ずつcompile、計4回成功した。
- 両source bodyは元 `.cppm` のLF正規化内容と一致しLF-only。`ArtifactRenderSupport` 全体buildとArtifact executable linkは未確認。


### ArtifactRender: OffscreenRenderer2D / GPUTextureCacheManager implementation pair（2026-10-24）

- `Artifact/src/Render/{ArtifactOffscreenRenderer2D,GPUTextureCacheManager}.cppm` を `.cpp` に移し、`Artifact/cmake/ArtifactSources.cmake` と `Artifact/CMakeLists.txt` の `ArtifactRender` source pathsを更新した。public `.ixx` とmodule名を維持した。
- D3D12/Vulkan/Diligent renderer・texture-cache実装の処理内容は変更していない。CMake 4.2 / Ninja / Clang 22.1.3 GNU frontend / Windows MSVC ABIでconfigure / generate、両sourceのfresh P1689 scanが成功。既存module mapsの14/15 PCM providersがすべて存在し、Ninja生成sccache commandで各objectを2回ずつcompile、計4回成功した。
- 両source bodyは元 `.cppm` のLF正規化内容と一致しLF-only。`ArtifactRender` 全体buildとArtifact executable linkは未確認。


### Artifact Render Queue: BatchRenderer / QueueEncoder migration attempt（2026-10-24、未完了・reverted）

- `ArtifactBatchRenderer` と `ArtifactRenderQueueEncoder` は両方 `ArtifactAppRuntime` の明示 manifestに登録され、public `.ixx` とimplementation moduleの名前・依存方向も確認できたため、移行を試みた。
- CMake configure / generateと両implementationのfresh P1689 scanは成功したが、Ninjaから2つの `.ddi` を直接指定すると4,449 actionsへ広がったため245 actions時点で停止した。`ArtifactBatchRenderer.ixx` の既存interfaceをsccache経由でcompileすると、Clang 22.1.3がAST address-space約2.13GB（診断上99%）でfrontend crashし、implementation objectのcompileへ進めなかった。
- 実装単位自体のClang互換性を確認できなかったため、この2件は `.cppm` と元manifest pathへ戻した。Clang全体の失敗とは断定せず、このinterface依存閉包のmemory制限として記録する。


### Artifact Render: AccumulatorPass / RenderScheduler implementation pair（2026-10-24）

- `Artifact/src/Render/{ArtifactAccumulatorPass,ArtifactRenderScheduler}.cppm` を `.cpp` に移した。`Artifact/cmake/ArtifactSources.cmake` を更新し、RenderSupport専用targetにも直接登録される `ArtifactRenderScheduler` は `Artifact/CMakeLists.txt` のsource pathも更新した。両public `.ixx` とmodule名は維持した。targetはそれぞれ `ArtifactAppRuntime` と `ArtifactRenderSupport` に分かれる。
- CMake 4.2 / Ninja / Clang 22.1.3 GNU frontend / Windows MSVC ABIでconfigure / generate、両sourceのfresh P1689 scanが成功。DDI requirementsと既存module mapsを照合し、参照PCMが全て存在することを確認。Ninja生成sccache compiler commandで各objectを2回ずつcompile、計4回成功した。warm repeatは2 requests / 2 cache hits、cache errorsとcompilation failuresの既存値に増加なし。
- 両source bodyはGit HEADの`.cppm`本文と一致しLF-only。通常のNinja object targetは4,449 actionsへ広がるため実行せず、`ArtifactRenderSupport`全体build、Artifact executable link、テストは未確認。


### Artifact 3D primitive submitter / pipeline adapter Clang verification（2026-10-24）

- `Artifact/src/Render/{Artifact3DPrimitiveSubmitter,Artifact3DPrimitivePipelineAdapter}.cpp` の同一3D submitter境界をまとめて検証した。両sourceは `ArtifactAppRuntime` に属し、public contract / ShaderManager のimport方向を維持している。
- CMake 4.2 / Ninja / Clang 22.1.3 GNU frontend / Windows MSVC ABIで両sourceのfresh P1689 scanが成功。P1689 requirementsは既存 `.cppm` scan結果とも一致した。既存module mapsの全provider PCM（3件・65件）が存在することを確認し、各objectを2回ずつsccache compileして計4回成功。warm repeatで2 requests / 2 cache hits、miss・cache error・compilation failureの増加なし。
- object compileのみで、`ArtifactAppRuntime` build / Artifact executable link / runtimeは未確認。


### Artifact Diagnostics widgets: FramePipelineView / FrameStateDiff implementation pair（2026-10-24）

- `Artifact/src/Widgets/Diagnostics/{FramePipelineViewWidget,FrameStateDiffWidget}.cppm` を `.cpp` に移し、`Artifact/cmake/ArtifactSources.cmake` のimplementation pathsを更新した。両public `.ixx` とmodule名、`ArtifactAppRuntime` ownershipを維持した。
- CMake 4.2 / Ninja / Clang 22.1.3 GNU frontend / Windows MSVC ABIでconfigure / generateとfresh P1689 scanが成功した。既存Clang module mapsの全直接依存PCMが揃っていることを確認し、2つのpublic interface BMIと2つのimplementation objectを個別にcompileした。
- `FramePipelineViewWidget.cpp` が `Graphics.RenderGraph` 所有の `RenderResourceHandle` を直接使っていたため、implementation側に明示importを追加した。公開interfaceは変更していない。追加後のfresh scanとcompileが成功した。`FrameStateDiffWidget.cpp` もcompile成功。
- 4 compile commandのwarm repeatは全て成功し、sccache統計は4 requests / 4 hits増、miss・cache error・compilation failureの増加なし。通常のNinja target build、Artifact executable link、runtimeとテストは未確認。


### Artifact Diagnostics Profiler: Overlay / Panel implementation pair（2026-10-24）

- `Artifact/src/Widgets/Diagnostics/{ProfilerOverlayWidget,ProfilerPanelWidget}.cppm` を `.cpp` に移し、`Artifact/cmake/ArtifactSources.cmake` のimplementation pathsを更新した。両public `.ixx`、module名、`ArtifactAppRuntime` ownershipを維持した。
- CMake 4.2 / Ninja / Clang 22.1.3 GNU frontend / Windows MSVC ABIでconfigure / generateとfresh P1689 scanが成功。既存Clang module mapsの直接依存PCMを確認し、2つのinterface BMIと2つのimplementation objectを個別compileした。
- 両実装で `ArtifactCore::toStdString` を用いるため、`Core.ArtifactString` の直接importを各 `.cpp` に追加した。公開interfaceは変更していない。追加後のfresh scanとClang compileが成功した。
- 4 compile commandのwarm repeatは全て成功し、sccache統計は4 requests / 4 hits増、miss・cache error・compilation failureの増加なし。通常のNinja target build、Artifact executable link、runtimeとテストは未確認。


### Artifact Diagnostics views: Trace Timeline / Frame Resource Inspector implementation pair（2026-10-24）

- `Artifact/src/Widgets/Diagnostics/{TraceTimelineWidget,FrameResourceInspectorWidget}.cppm` を `.cpp` に移し、`Artifact/cmake/ArtifactSources.cmake` のimplementation pathsを更新した。両public `.ixx` とmodule名、`ArtifactAppRuntime` ownershipを維持した。
- CMake 4.2 / Ninja / Clang 22.1.3 GNU frontend / Windows MSVC ABIでconfigure / generateと両sourceのfresh P1689 scanが成功し、旧 `.cppm` scanのrequires集合と一致した。両interface BMIとimplementation objectを個別compileした。FrameResourceInspectorの直接依存は既存どおり `Core.ArtifactMath` 1件。
- 4 compile commandのwarm repeatは全て成功し、sccache統計は4 requests / 4 hits増、miss・cache error・compilation failureの増加なし。両sourceはLF-only。通常のNinja target build、Artifact executable link、runtimeとテストは未確認。


### Artifact Diagnostics debug tooling: Render Harness / Debug Console implementation pair（2026-10-24）

- `Artifact/src/Widgets/Diagnostics/{DebugRenderHarnessWidget,ArtifactDebugConsoleWidget}.cppm` を `.cpp` に移し、`Artifact/cmake/ArtifactSources.cmake` のimplementation pathsを更新した。public `.ixx` と module ownershipを維持した。
- CMake 4.2 / Ninja / Clang 22.1.3 GNU frontend / Windows MSVC ABIでconfigure / generateと両sourceのfresh P1689 scanが成功した。Render Harnessのrequiresは旧 `.cppm` scanと一致した。Debug Consoleは `Playback.State` の直接import追加を反映してrequiresが1件増えた。両interface BMIとimplementation objectを個別compileした。
- Clang compileで既存の名前解決問題が明らかになった。Harnessは `ArtifactCore::BlendModeCatalog` とqualifyし、Debug Consoleは `Playback.State` を直接importしたうえで `ArtifactCore::PlaybackState` を参照するよう修正した。どちらも公開interfaceには変更なし。修正後のfresh scanとcompileが成功した。
- 4 compile commandのwarm repeatは全て成功し、sccache統計は4 requests / 4 hits増、miss・cache error・compilation failureの増加なし。通常のNinja target build、Artifact executable link、runtimeとテストは未確認。


### Artifact Layer Abstract support implementation batch（2026-10-24）

- `Artifact/src/Layer/{ArtifactAbstractLayerBounds,ArtifactAbstractLayerPhysicsSupport,ArtifactAbstractLayerFractureSupport}.cppm` を `.cpp` に移し、`Artifact/cmake/ArtifactSources.cmake` のimplementation pathsを更新した。3 sourceはいずれも既存module `Artifact.Layer.Abstract` のimplementation unitであり、共有public `.ixx` は変更していない。
- CMake 4.2 / Ninja / Clang 22.1.3 GNU frontend / Windows MSVC ABIでconfigure / generate、3 sourceのfresh P1689 scanが成功し、旧 `.cppm` scanのrequires集合と一致した。共有 `Artifact.Layer.Abstract` BMIと既存dependency module mapsの96 / 101 / 96 PCM providerが全て存在し、3 object compileに成功した。
- 各implementation objectのwarm repeatも成功し、sccache統計は3 requests / 3 hits増、miss・cache error・compilation failureの増加なし。全source本文はGit HEADの `.cppm` と一致し、LF-only。`ArtifactAppRuntime` 全体build、アプリlink、runtimeとテストは未確認。


### Artifact Widgets styling implementation pair: CommonStyle / InspectorStyle（2026-10-24）

- `Artifact/src/Widgets/{CommonStyle,ArtifactInspectorStyle}.cppm` を `.cpp` に移し、`Artifact/cmake/ArtifactSources.cmake` のimplementation pathsを更新した。両public `.ixx` とmodule名、`ArtifactAppRuntime` ownershipを維持した。
- CMake 4.2 / Ninja / Clang 22.1.3 GNU frontend / Windows MSVC ABIでconfigure / generate、両sourceのfresh P1689 scan、各interface BMIとimplementation objectの個別compileが成功した。P1689 requiresは両sourceとも旧 `.cppm` scanと一致した。
- 各interface / implementation compileのwarm repeatも成功し、sccache統計は4 requests / 4 hits増、miss・cache error・compilation failureの増加なし。両sourceはLF-onlyで、移行時に実装内容の変更なし。全体target build、アプリlink、runtimeとテストは未確認。

### Artifact AVSyncBridge / CompositionPlaybackController implementation pair（2026-10-24）

- `Artifact/src/AVSync/AVSyncBridge.cppm` と `Artifact/src/Composition/ArtifactCompositionPlaybackController.cppm` を `.cpp` に移し、`Artifact/cmake/ArtifactSources.cmake` の明示source pathsを更新した。public `.ixx` とmodule名は維持した。
- CMake 4.2 / Ninja / Clang 22.1.3 GNU frontend / Windows MSVC ABIで両sourceのfresh P1689 scanとobject compileが成功した。PlaybackControllerはClangが `ArtifactPlaybackAudioDiagnostics` の所有元 `Artifact.Playback.Engine` を直接importするよう要求したため、実装 `.cpp` にのみimportを追加して再scan / compileした。両objectのwarm repeatも成功し、sccache集計は2 requests / 2 hits、0 misses増加だった。
- 両sourceはLF-only。object-level compileと限定scanのみで、ArtifactAppRuntime全体build、executable link、runtimeは未確認。

### Artifact Project core trio migration attempt（2026-10-24、未完了・reverted）

- `ArtifactProject` / `ArtifactProjectImporter` / `ArtifactProjectManager` は `ArtifactAppRuntime` のsource manifest登録、module名、public interfaceを確認して一時的に `.cpp` へ移した。CMake configure / generateと3 sourceのfresh P1689 scanは成功した。
- 各implementation objectの単独compileはClang 22.1.3 frontendがAST読込約2.1GB（診断上99%）でcrashし、objectを生成できなかった。`.cpp` 移行のcompile成立を確認できないため、3ファイルとmanifest entryを元の `.cppm` に戻した。これは拡張子変更によるsource errorとは断定しない。
- 全体build / linkは未確認。Project importer / manager群はmodule dependency closureとfrontend memoryを別途評価してから再検討する。

### Artifact VST Host / Effect implementation pair（2026-10-24）

- `Artifact/src/VST/{VSTHost,VSTEffect}.cppm` を `.cpp` に移し、`Artifact/cmake/ArtifactSources.cmake` の明示source pathsを更新した。public `.ixx` とmodule名を維持し、既存の Host → Effect import方向を保った。
- CMake 4.2 / Ninja / Clang 22.1.3 GNU frontend / Windows MSVC ABIでconfigure / generate、両sourceのfresh P1689 scanと両interface・implementation objectの個別compileが成功した。Host実装は `ArtifactCore::String` の所有元 `Core.ArtifactString` を直接importしていなかったため、実装 `.cpp` に直接importを追加して再scan / compileした。VSTHost compileで既存の配列条件・Windows function pointer cast警告、VSTEffect compileで既存Qt deprecation警告が出たが、今回の拡張子移行によるエラーはなかった。
- object warm repeatは2 requests / 2 cache hits、miss増加なし。sourceはLF-only。ArtifactAppRuntime全体build、executable link、VST runtime動作、testsは未確認。
### Artifact Asset Browser helper implementation pair（2026-10-24）

- `Artifact/src/Widgets/Asset/{ArtifactAssetBrowserPresentation,ArtifactAssetBrowserUndoCommands}.cppm` を `.cpp` に移し、`Artifact/cmake/ArtifactSources.cmake` の明示source pathsを更新した。各module名を維持し、5,365行あるBrowser本体は今回の範囲に含めていない。
- CMake 4.2 / Ninja / Clang 22.1.3 GNU frontend / Windows MSVC ABIでconfigure / generate、両sourceのfresh P1689 scanと個別object compileが成功した。UndoCommandsのmodule mapで不足していた `Artifact.Service.Project` と `Undo.UndoManager` のinterface PCMは既存interface sourceの限定compileで生成。さらに `ProjectItem` の所有元 `Artifact.Project.Items` の直接import不足をClangが報告したため、implementationだけに追加して再scan / compileした。
- 両objectのwarm repeatは2 requests / 2 cache hits、miss増加なし。sourceはLF-only。ArtifactAppRuntime全体build、executable link、UI runtime、testsは未確認。
### Artifact Color Swatch widget / dialog implementation pair（2026-10-24）

- `Artifact/src/Widgets/Color/ArtifactColorSwatchWidget.cppm` と `Artifact/src/Widgets/Dialog/ColorSwatchDialog.cppm` を `.cpp` に移し、`Artifact/cmake/ArtifactSources.cmake` の明示pathsを更新した。両sourceは `ArtifactAppRuntime` 内のswatch編集体験を構成する別moduleで、module名を維持した。
- CMake 4.2 / Ninja / Clang 22.1.3 GNU frontend / Windows MSVC ABIでconfigure / generate、両sourceのfresh P1689 scanとindividual object compileが成功した。ColorSwatchDialogが必要とする `Translation.Manager` と各own module interface PCMは既存 `.ixx` を限定compileして準備した。両sourceをLF-onlyに揃え、fresh scan / compileを再確認した。
- object warm repeatは2 requests / 2 cache hits、miss増加なし。ArtifactAppRuntime全体build、executable link、UI runtime、testsは未確認。

### Artifact Color Science Panel migration attempt（2026-10-24、未完了・reverted）

- `ArtifactColorSciencePanel.cppm` の一時 `.cpp` 移行でCMake configure / generateとfresh P1689 scanは成功したが、implementation object用module mapの不足PCMを補う段階で既存 `Artifact3DGizmo.ixx` interface compileがnested `export` declaration errorになった。上流PCMを生成できず、Panel object compileは未到達。
- Science Panelの `.cpp` path / manifest変更は取り消して `.cppm` に戻した。観測したエラーはPanel本文のcompile結果ではない。依存interfaceの問題を別途調査後に再評価する。
### Artifact OFX Host / EffectImpl implementation pair（2026-10-24）

- `Artifact/src/Effects/Ofx/{ArtifactOfxHost,ArtifactOfxEffectImpl}.cppm` を `.cpp` に移し、`Artifact/cmake/ArtifactSources.cmake` の明示pathsを更新した。Host → EffectImplのmodule依存方向を維持。
- CMake 4.2 / Ninja / Clang 22.1.3 GNU frontend / Windows MSVC ABIでconfigure / generate、両sourceのfresh P1689 scanとindividual object compileが成功した。Hostでは非export forward declaration後のexport定義をClangが拒否したため、`OfxPluginDescriptor` と `ArtifactOfxHost` の先行宣言をexportし、Hostを再scan / compileした。EffectImplでは `Artifact.Effect.Context` の直接import不足を補って再scan / compileした。
- 両objectのwarm repeatは2 requests / 2 cache hits、miss増加なし。Host sourceをLF-onlyに揃えた。ArtifactAppRuntime全体build、executable link、OFX plugin runtime、testsは未確認。
### Artifact Playback Engine implementation（2026-10-24）

- `Artifact/src/Playback/ArtifactPlaybackEngine.cppm` を `.cpp` に移し、`Artifact/cmake/ArtifactSources.cmake` の明示pathを更新した。public module名を維持し、source behaviorは変更していない。
- CMake 4.2 / Ninja / Clang 22.1.3 GNU frontend / Windows MSVC ABIでfresh P1689 scanと個別object compileが成功した。ArtifactAppRuntime全体build、executable link、playback runtime、testsは未確認。

### Artifact Tool CameraTracker / PointTracker implementation pair（2026-10-24）

- `Artifact/src/Tool/{ArtifactCameraTrackerTool,ArtifactPointTrackerTool}.cppm` を `.cpp` に移し、`Artifact/cmake/ArtifactSources.cmake` の明示pathsを更新した。両方ともArtifactAppRuntime内のtracking tool implementationsで、module名を維持した。
- CMake 4.2 / Ninja / Clang 22.1.3 GNU frontend / Windows MSVC ABIで両sourceのfresh P1689 scanと個別object compileが成功した。CameraTrackerで `Artifact.Layer.InitParams`、PointTrackerで `Memory.SharedPtr` の直接import不足を実装側に補った。CameraTracker compile前に古いArtifactCore PCMが削除済み `.cppm` source pathを参照していたため、ArtifactCoreの現行 `Tracking/CameraTracker.cpp` をbuild tree内だけでfresh scan / compileしてPCMを更新した。ArtifactCore sourceは編集していない。
- 両objectのwarm repeatは3件（Playback Engineを含む）で3 cache hits、miss増加なし。両Tool sourceはLF-only。全target build / app link / runtime / testsは未確認。

### Artifact Playback ControlWidget migration attempt（2026-10-24、未完了・reverted）

- `ArtifactPlaybackControlWidget.cppm` の一時 `.cpp` 移行でCMake configure / generateとfresh P1689 scanは成功した。個別compileの依存 `Artifact.Application.Manager.pcm` を用意するため `ArtifactApplicationManager.ixx` を個別compileしたが、Clang frontendがAST約2.15GB（診断上99%）でcrashしたためControlWidget object compileには到達できなかった。
- ControlWidgetは `.cppm` と元manifest pathへ戻した。Playback Engine implementationは別unitとして `.cpp` compile成功済み。この結果はControlWidget本文のClang非互換を示さない。
### Artifact Diagnostics support widgets: EventBusDebugger / FrameDebugView implementation pair（2026-10-24）

- `Artifact/src/Widgets/Diagnostics/{EventBusDebuggerWidget,FrameDebugViewWidget}.cppm` を `.cpp` に移し、`Artifact/cmake/ArtifactSources.cmake` の明示pathsを更新した。両module名と診断UIの既存責務は維持した。
- CMake 4.2 / Ninja / Clang 22.1.3 GNU frontend / Windows MSVC ABIでconfigure / generate、両implementationのfresh P1689 scanとindividual object compileが成功した。両public `.ixx` もbuild tree内で個別compileし、implementationのPCMを用意した。両sourceはLF-onlyへ揃えた後もfresh scan / compileが成功。
- object warm repeatは2 requests / 2 cache hits、miss増加なし。ArtifactAppRuntime全体build、executable link、UI runtime、testsは未確認。
### Artifact Brush Tool / Smart Guides implementation pair（2026-10-24）

- `Artifact/src/Tool/ArtifactBrushTool.cppm` と `Artifact/src/Core/Guides/ArtifactSmartGuidesManager.cppm` を `.cpp` に移し、`Artifact/cmake/ArtifactSources.cmake` の明示pathsを更新した。両module名を維持し、source behaviorは変更していない。
- Clang 22.1.3 GNU frontend / Windows MSVC ABIでCMake configure / generate、両implementationのfresh P1689 scanとindividual object compileが成功した。SmartGuidesのinterface closureに含まれる `TransformGizmo.ixx` と `ArtifactSmartGuidesManager.ixx` では `export namespace` 内の重複 `export class` 指定を除き、interface compileに成功した。SmartGuides implementation compileも成功。TransformGizmo自身のimplementation scanは成功したが、object compileはAST約2.14GB（診断上99%）でfrontend crashし未確認。
- 両objectのwarm repeatは2 requests / 2 cache hits、miss増加なし。implementation sourcesと変更interfaceはLF-only。ArtifactAppRuntime全体build、executable link、全consumer link、runtime、testsは未確認。
### Artifact InteractiveShell / AssetDirectoryModel implementation pair（2026-10-24）

- `Artifact/src/Application/ArtifactInteractiveShell.cppm` と `Artifact/src/Asset/AssetDirectoryModel.cppm` を `.cpp` に移し、`Artifact/cmake/ArtifactSources.cmake` の明示source pathsを更新した。public `.ixx` とmodule名を維持した。`ArtifactPythonHookManager` は依存する `Artifact.Application.Manager` interfaceのClang compileがメモリ上限で成立しなかったため、`.cppm` とmanifest entryへ戻した。
- CMake 4.2 / Ninja / Clang 22.1.3 GNU frontend / Windows MSVC ABIでconfigure / generate成功。両sourceのfresh P1689 scan、individual implementation object compile、warm repeatが成功した。warm repeatはsccache requests +2 / hits +2、misses・errors・failures増加なし。
- 両sourceはLF-only。ArtifactAppRuntime全体build、executable link、runtime、testsは未確認。
### Artifact diagnostics widgets: PerformanceProfiler / ProblemView implementation pair（2026-10-24）

- `Artifact/src/Widgets/ArtifactPerformanceProfilerWidget.cppm` と `ArtifactProblemViewWidget.cppm` を `.cpp` に移し、`Artifact/cmake/ArtifactSources.cmake` のsource pathsを更新した。public `.ixx` とmodule名を維持した。
- CMake 4.2 / Ninja / Clang 22.1.3 GNU frontend / Windows MSVC ABIでconfigure / generate、両sourceのfresh P1689 scanとobject compileが成功。ProblemViewでは未生成だった `Artifact.Widgets.ProjectHealthSummary` PCM用にprovider sourceを個別compileし、ProblemView public interfaceもfresh scan / compileしてからimplementationを検証した。
- 両objectのwarm repeatは2 requests / 2 cache hits、misses・errors・failures増加なし。implementation sourceはLF-only。ArtifactAppRuntime全体build、executable link、runtime、testsは未確認。

### Artifact audio preview UI: AudioPreview 成功 / AudioMiniWidget reverted（2026-10-24）

- `Artifact/src/Widgets/AudioPreviewWidget.cppm` は `.cpp` に移しsource manifestを更新。public interfaceとimplementationのfresh P1689 scan / 個別compileが成功した。warm repeatはsccache 1 hit、miss / error / failure増加なし。
- 同じバッチ候補の `ArtifactAudioMiniWidget.cppm` はinterface compileとfresh P1689 scanが成功したものの、implementation object compileがClang frontendのAST読込約2.13GB（診断上99%）でcrashしたため、source extensionとmanifest pathを `.cppm` に戻した。implementation本文のsource errorとは断定しない。
- 最終CMake configure / generate成功。AudioPreviewWidgetのobject compileまで確認し、ArtifactAppRuntime全体build、executable link、audio runtime、testsは未確認。

### Artifact Layer Editor shape interaction controllers（2026-10-24）

- `LayerEditorShapeMoveController.cppm`、`LayerEditorShapeDragController.cppm`、`LayerEditorShapeInputController.cppm` を `.cpp` に移し、`Artifact/cmake/ArtifactSources.cmake` の明示source pathsを更新した。3件とも `module Artifact.Widgets.LayerEditor.*;` のimplementation unitであり、public `.ixx` とmodule名を維持した。
- CMake 4.2 / Ninja / Clang 22.1.3 GNU frontend / Windows MSVC ABIでconfigure / generate成功。3 implementationのfresh P1689 scan / object compileが成功。依存不足の `ShapeDragController`、`ShapeEditSession`、`ShapeHoverController`、`ShapeInputController`、`ShapeMoveController`、`LayerEditor.Geometry` interfacesをbuild tree内で個別scan / compileして依存PCMを生成した。`ShapeInputController` には Clang が要求した所有module `Memory.SharedPtr` の直接importをimplementationだけに追加し、fresh scan / compileを再確認した。
- 3 implementation objectのwarm repeatは3 requests / 3 cache hits、misses・errors・failures増加なし。sourceはLF-only。ArtifactAppRuntime全体build、executable link、UI runtime、testsは未確認。


### Artifact Layer Editor shape hover / press interaction controllers（2026-10-24）

- `LayerEditorShapeHoverController.cppm`、`LayerEditorShapePressController.cppm`、`LayerEditorShapePressInteractionController.cppm` を `.cpp` に移し、`Artifact/cmake/ArtifactSources.cmake` の明示source pathsを更新した。3件とも `module Artifact.Widgets.LayerEditor.*;` implementation unitで、対応public `.ixx` とmodule名を維持した。
- CMake 4.2 / Ninja / Clang 22.1.3 GNU frontend / Windows MSVC ABIでconfigure / generate成功。3 public interfaceと3 implementationのfresh P1689 scan / 個別compileが成功した。HoverControllerは `Memory.SharedPtr` の直接import不足、PressInteractionControllerは `QPointF` の直接include不足を検出し、module宣言前のimplementationだけに補って再scan / compileした。
- 3 implementation objectのwarm repeatは3 requests / 3 cache hits、misses・errors・failures増加なし。sourceはLF-only。ArtifactAppRuntime全体build、executable link、UI runtime、testsは未確認。


### Artifact Layer Editor shape command / edit session / input implementation group（2026-10-24）

- `LayerEditorShapeCommands.cppm`、`LayerEditorShapeEditSession.cppm`、`LayerEditorShapeParameterController.cppm`、`LayerEditorKeyInputController.cppm` を `.cpp` に移し、`Artifact/cmake/ArtifactSources.cmake` の明示source pathsを更新した。4件とも別public `.ixx` を持つ `module Artifact.Widgets.LayerEditor.*;` implementation unitで、module名を維持した。
- CMake 4.2 / Ninja / Clang 22.1.3 GNU frontend / Windows MSVC ABIでconfigure / generate成功。4 public interfaceと4 implementationのfresh P1689 scan / 個別compileが成功した。KeyInput interfaceに必要な `ModalTransformController` PCMをinterface sourceの限定compileで生成した。`ShapeCommands`、`ShapeEditSession`、`ShapeParameterController` implementationでは `Memory.SharedPtr` の直接import不足をClangが検出したため実装側に補い、再scan / compileした。
- 4 implementation objectのwarm repeatは4 requests / 4 cache hits、misses・errors・failures増加なし。sourceはLF-only。ArtifactAppRuntime全体build、executable link、UI runtime、testsは未確認。


### Artifact Layer Editor mask editing implementation group（2026-10-24）

- `LayerEditorMaskDragController.cppm`、`LayerEditorMaskPressInteractionController.cppm`、`LayerEditorMaskOverlay.cppm` を `.cpp` に移し、`Artifact/cmake/ArtifactSources.cmake` の明示source pathsを更新した。3件とも別public `.ixx` を持つ `module Artifact.Widgets.LayerEditor.*;` implementation unitで、module名を維持した。
- CMake 4.2 / Ninja / Clang 22.1.3 GNU frontend / Windows MSVC ABIでconfigure / generate成功。3 public interfaceと、依存する `MaskEditSession` / `MaskHoverController` / `MaskPressController` interfacesを個別fresh scan / compileした後、3 implementationのfresh P1689 scan / object compileが成功した。PressInteractionでは `QPointF` の直接include不足をClangが検出したため、implementationのglobal module fragmentに `#include <QPointF>` を追加して再検証した。
- 3 implementation objectのwarm repeatは3 requests / 3 cache hits、misses・errors・failures増加なし。sourceはLF-only。ArtifactAppRuntime全体build、executable link、UI runtime、testsは未確認。


### Artifact Layer Editor viewport chrome implementation group（2026-10-24）

- `LayerEditorViewportChrome.cppm`、`LayerEditorViewportChromeRenderer.cppm`、`LayerEditorViewportChromeInteractionController.cppm`、`LayerEditorTransformOverlay.cppm` を `.cpp` に移し、`Artifact/cmake/ArtifactSources.cmake` の明示source pathsを更新した。4件とも `module Artifact.Widgets.LayerEditor.*;` implementation unitであり、対応 public `.ixx` と module名を維持した。
- CMake 4.2 / Ninja / Clang 22.1.3 GNU frontend / Windows MSVC ABIでconfigure / generate成功。4 public interfaceと4 implementationのfresh P1689 scan / 個別compileが成功した。ViewportChrome interfaceに必要な `Tool` interface PCMもbuild tree内で限定compileして生成した。今回のsource本文に追加変更はなかった。
- 4 implementation objectのwarm repeatは4 requests / 4 cache hits、misses・errors・failures増加なし。sourceはLF-only。ArtifactAppRuntime全体build、executable link、UI runtime、testsは未確認。


### Artifact Layer Editor shape / surface overlay implementation group（2026-10-24）

- `LayerEditorShapeOverlay.cppm`、`LayerEditorSurfaceOverlay.cppm`、`LayerEditorSurfaceInfo.cppm` を `.cpp` に移し、`Artifact/cmake/ArtifactSources.cmake` の明示source pathsを更新した。3件とも別 public `.ixx` を持つ `module Artifact.Widgets.LayerEditor.*;` implementation unitで、module名を維持した。
- CMake 4.2 / Ninja / Clang 22.1.3 GNU frontend / Windows MSVC ABIでconfigure / generate成功。3 public interfaceと3 implementationのfresh P1689 scan / 個別compileが成功した。ShapeOverlayでは `ArtifactCore::Collider2DShape` のowner module `Physics.Collider2DEdit` の直接import不足をClangが検出したためimplementationだけに追加し、fresh scan / compileを再確認した。
- 3 implementation objectのwarm repeatは3 requests / 3 cache hits、misses・errors・failures増加なし。sourceはLF-only。ArtifactAppRuntime全体build、executable link、overlay runtime、testsは未確認。


### Artifact Layer Editor geometry / transform implementation group（2026-10-24）

- `LayerEditorGeometry.cppm` と `LayerEditorModalTransformController.cppm` を `.cpp` に移し、`Artifact/cmake/ArtifactSources.cmake` の明示source pathsを更新した。両sourceは別public `.ixx` を持つ `module Artifact.Widgets.LayerEditor.*;` implementation unitで、module名を維持した。
- 同バッチの `LayerEditorContextMenu.cppm` も一時 `.cpp` としてCMake configure / generate、public interface compile、fresh P1689 scanまで成功したが、implementation object compileでClang frontendがAST約2.13GB（診断上99%）でcrashした。source errorとは判断できないためsource path / manifestを `.cppm` に戻した。implementationには `Memory.SharedPtr` の直接importを追加したが、compile成立は未確認。
- Geometry と ModalTransform は `ArtifactCore::dynamicPointerCast` / `SharedPtr` のowner `Memory.SharedPtr` を直接importしておらず、implementationだけに追加後、fresh scan / object compileに成功した。両objectのwarm repeatは2 requests / 2 cache hits、misses・errors・failures増加なし。CMake final configure / generate成功。sourceはLF-only。ArtifactAppRuntime全体build、executable link、UI runtime、testsは未確認。

### Artifact Layer Editor interaction state controller migration（2026-10-24、compile未確認）

- `LayerEditorInteractionStateController.cppm` を `.cpp` に移し、`Artifact/cmake/ArtifactSources.cmake` のsource entryを更新した。sourceは独立 public `.ixx` を持ち、`module Artifact.Widgets.LayerEditor.InteractionStateController;` のimplementation unitであることを確認した。
- CMake 4.2 / Ninja / Clang 22.1.3 GNU frontend / Windows MSVC ABIでconfigure / generate成功。対象sourceのfresh P1689 scanは成功し、8件のimportとprimary module declarationを検出した。
- Ninjaで対象object pathを指定すると5267件の依存buildを計画したため40件実行後に停止した。既存の `.cppm.obj.modmap` をbuild tree内で `.cpp.obj.modmap` として再利用して直接compileを試したが、`Artifact.Widgets.LayerEditor.MaskMoveController.pcm` がbuild treeに無く、sccacheが入力不足で停止した。したがってsource compileは未確認であり、ArtifactAppRuntime全体build / link / runtime / testsも未確認。全依存buildを避けるため、このsourceを単独compile済みとは扱わない。

### Artifact text glyph shader source implementation（2026-10-24）

- `ArtifactTextGlyphShaderSources.cppm` を `.cpp` に移し、`Artifact/CMakeLists.txt` のfocused runtime / smoke targetにある3つのsource参照を更新した。独立public `ArtifactTextGlyphShaderSources.ixx` とmodule名を維持した。実装は外部module importを持たない。
- CMake 4.2 / Ninja / Clang 22.1.3 GNU frontend / Windows MSVC ABIでconfigure / generate成功。実装・interface双方のfresh P1689 scan、public interface PCM生成、implementation objectの個別compileが成功した。対象moduleに限定したmodule mapをbuild tree内で用いて検証した。
- LFへ正規化したsourceでcompileを再実行し、その直後のwarm repeatまででsccache requests +2 / hits +1 / misses +1（CRLFからLFへの入力変更分）、errors不変（35）。focused targetのNinja dyndepは985件を計画したため116件で停止し、対象外の全依存scan / buildを続けていない。runtime target、executable link、実行、testsは未確認。

### Artifact Render widget implementations: GridRenderer / ViewportOverlay（2026-10-24）

- `GridRenderer.cppm` と `ViewportOverlay.cppm` を `.cpp` に移行し、`Artifact/cmake/ArtifactSources.cmake` のGridRenderer source entryと `Artifact/CMakeLists.txt` のViewportOverlay 4 referencesを更新した。別public `.ixx` とmodule名を保つimplementation unitであることを確認した。
- CMake 4.2 / Ninja / Clang 22.1.3 GNU frontend / Windows MSVC ABIでconfigure / generate成功。両implementationのfresh P1689 scan、GridRenderer public interfaceのPCM compile、および2 implementation objectの個別compileが成功した。sourceのmodule依存PCMがbuild treeに揃うことを確認し、旧拡張子のmodule mapを新object pathへ複製して直接検証した。
- 2 implementationのwarm repeatでsccache requests +2 / hits +2、追加misses・errorsなし。両sourceはLF-onlyで元内容を保持した。ArtifactAppRuntime / ArtifactRender全target build、link、runtime、testsは未確認。

### Artifact object reference / picker UI implementation group（2026-10-24）

- `ArtifactObjectReferenceWidget.cppm` と `ArtifactObjectPickerDialog.cppm` を `.cpp` に移し、`Artifact/cmake/ArtifactSources.cmake` の2 source entryを更新した。両方とも別public `.ixx` と対応するimplementation module名を維持した。
- CMake 4.2 / Ninja / Clang 22.1.3 GNU frontend / Windows MSVC ABIでconfigure / generate成功。両interface / implementationのfresh P1689 scan、interface PCM生成、implementation object compileが成功した。ClangがObjectPicker interfaceの `QPushButton` 重複宣言と、module purview内にあった `QTreeWidget` / `QLineEdit` forward declarationによる型不一致を検出したため、不要宣言を除きpointer-only forward declarationsをglobal module fragmentへ移した。ObjectReference implementationでは不足していた `QLineEdit` / `QPushButton` 直接includeをglobal module fragmentへ追加した。
- 両implementationのwarm repeatでsccache requests +2 / hits +2、misses 857 / errors 35は不変。変更source / interfaceはLF-only。ArtifactAppRuntime全体build、link、runtime、testsは未確認。

### Artifact timeline UI implementation group: GlobalSwitches / HistoryTimeline成功、AnimationTimeline reverted（2026-10-24）

- `ArtifactTimelineGlobalSwitches.cppm` と `ArtifactHistoryTimelineWidget.cppm` を `.cpp` に移し、`Artifact/cmake/ArtifactSources.cmake` を更新した。両sourceに独立public `.ixx` があり、module名を維持した。CMake 4.2 / Ninja / Clang 22.1.3 GNU frontend / Windows MSVC ABIでconfigure / generate、両interface / implementationのfresh P1689 scan、interface PCM生成、implementation object compileが成功した。
- `ArtifactAnimationTimelineWidget.cppm` も移行してfresh scan / interface PCM compileは成功したが、implementation compileが2回とも `malformed or corrupted precompiled file: 'ran out of source locations'` を報告してClang frontend crashした。該当PCMを特定できていないため、sourceとmanifestを元の `.cppm` pathへ戻した。
- 成功した2 implementationのwarm repeatはsccache requests +2 / hits +2、misses 862 / errors 35は不変。ArtifactAppRuntime全体build / link / runtime / testsは未確認。

### Artifact history UI implementation pair: HistoryTimeline / UndoHistory（2026-10-24）

- `ArtifactUndoHistoryWidget.cppm` を `.cpp` に移し、`Artifact/cmake/ArtifactSources.cmake` を更新した。既移行済み `ArtifactHistoryTimelineWidget.cpp` と合わせ、両sourceのfresh P1689 scanと個別object compileを確認した。UndoHistoryのpublic interface PCMも個別生成し、Clang 22.1.3 GNU frontend / Windows MSVC ABIで成功した。
- UndoHistory interface PCMは約86 MBだった。両implementationのwarm repeatはsccache requests +2 / hits +2、misses 864 / errors 35は不変。
- CMake 4.2 / Ninja configure / generate成功。ArtifactAppRuntime全体build / link / runtime / testsは未確認。

### Artifact Inspector surfaces / interaction implementation pair（2026-10-24）

- `ArtifactInspectorSurfaces.cppm` と `ArtifactInspectorInteraction.cppm` を `.cpp` に移し、`Artifact/cmake/ArtifactSources.cmake` の2 source entryを更新した。両方とも独立public `.ixx` とmodule名を維持し、共通の `Artifact.Widgets.InspectorStyle` moduleをimportする。
- CMake 4.2 / Ninja / Clang 22.1.3 GNU frontend / Windows MSVC ABIでconfigure / generate成功。両interface / implementationのfresh P1689 scan、interface PCM生成、implementation object compileが成功した。
- 両implementationのwarm repeatはsccache requests +2 / hits +2、misses 868 / errors 35は不変。sourceはLF-only。ArtifactAppRuntime全体build / link / runtime / testsは未確認。

### Artifact Inspector effect picker / catalog implementation pair（2026-10-24）

- `ArtifactInspectorEffectPicker.cppm` と `InspectorEffectCatalog.cppm` を `.cpp` に移し、`Artifact/cmake/ArtifactSources.cmake` の2 source entryを更新した。両方とも別public `.ixx` と `module` implementation unitを維持し、EffectPickerはInspectorStyle / EffectCatalog / Effect.Abstractを、CatalogはEffect.Abstractをimportする。
- CMake 4.2.2 / Ninja 1.12.1 / Clang 23.1.4 GNU frontend / Windows MSVC ABIでV:上の既存build treeをconfigure / generateし、両interfaceと2 implementationのfresh P1689 scanを実行した。EffectCatalog、InspectorStyle、EffectPickerのinterface PCM、および2 implementation objectを個別compileして成功した。
- Catalog implementation compileでは `ArtifactEffectStage::PreProcess` をswitchで扱わない既存warningが1件出た。warm repeatは2 requests / 2 sccache hits、misses 875 / errors 35は不変。ソース内容に変更はなく、ArtifactAppRuntime全体build / link / runtime / testsは未確認。

### Artifact Layer Abstract property / transform support implementation pair（2026-10-24）

- `ArtifactLayerMaskPropertySupport.cppm` と `ArtifactLayerTransformSupport.cppm` を `.cpp` に移し、`Artifact/cmake/ArtifactSources.cmake` のsource pathsを更新した。両方とも `module Artifact.Layer.Abstract;` のimplementation unitで、`ArtifactAbstractLayer.ixx` とmodule名を維持した。
- Clang 23.1.4 GNU frontend / Windows MSVC ABIでV:上の既存Ninja treeをconfigure / generateし、両implementationのfresh P1689 scanと個別object compileが成功した。既存module mapが参照する97 / 101 PCMはすべて存在したため、旧 `.cppm` object mapを新 `.cpp` object path用にbuild tree内で複製して検証した。
- 2 implementationのwarm repeatは2 requests / 2 sccache hits、misses 877 / errors 35は不変。変更sourceはLF-onlyで内容はgit正規化後に元blobと一致する。ArtifactAppRuntime全体build / link / runtime / testsは未確認。

### Artifact Layer Abstract persistence / property presentation implementation pair（2026-10-24）

- `ArtifactAbstractLayerTransformPersistence.cppm` と `ArtifactAbstractLayerPropertyPresentation.cppm` を `.cpp` に移し、`Artifact/cmake/ArtifactSources.cmake` のsource pathsを更新した。両方とも `module Artifact.Layer.Abstract;` のimplementation unitで、`ArtifactAbstractLayer.ixx` とmodule名を維持した。
- Clang 23.1.4 GNU frontend / Windows MSVC ABIでV:上の既存Ninja treeをconfigure / generateし、両implementationのfresh P1689 scanと個別object compileが成功した。module mapが参照する98 / 97 PCMに欠落はなかった。
- warm repeatは2 requests / 2 sccache hits、misses 879 / errors 35は不変。変更sourceはLF-onlyでgit正規化後に元blobと一致する。ArtifactAppRuntime全体build / link / runtime / testsは未確認。

### Artifact hierarchy / selection candidate deferred due to Project Manager interface memory cost（2026-10-24）

- `ArtifactHierarchyModel.cppm` と `ArtifactLayerSelectionManager.cppm` は関連候補だが、Hierarchy implementationの既存Clang module mapで `Artifact.Project.Manager.pcm` だけが不足していた。
- 依存PCMが既存build tree内に揃った `ArtifactProjectManager.ixx` を個別fresh scan / compileしたところ、Clang processのworking setが約7.2 GiBまで上がり、空き物理RAMが約3.7 GiBになったためinterface compileを中断した。PCMは生成されず、候補source / manifestは変更していない。個別object source errorとは判定していない。

### Project preset / layer implementation candidates（2026-10-24）

- `ArtifactPresetManager.cppm` を `.cpp` に移し、source manifestを更新した。Clang 23.1.4 GNU frontend / Windows MSVC ABIでfresh P1689 scan / individual object compileが成功した。
- `ArtifactProjectImporter.cppm` は一時 `.cpp` としてfresh scan後にindividual compileを試したが、ClangがAST 2.13 GiB（診断上99%）でfrontend crash / exit 70した。sourceとmanifestは `.cppm` に戻し、source errorとは判断しない。
- 同じ候補探索から `ArtifactSourceCrop.cppm` と `ArtifactCompositionBackgroundLayer.cppm` を `.cpp` に移し、manifestを更新した。両方とも既存module map参照PCMに欠落がなく、fresh P1689 scan / individual object compileが成功した。
- final CMake configure / generate成功。PresetManager / SourceCrop / CompositionBackgroundLayerのwarm repeatは3 requests / 3 sccache hits、misses 882 / errors 35は不変。3 success sourceはLF-onlyでgit正規化後に元blobと一致。ArtifactAppRuntime全体build / link / runtime / testsは未確認。

### Artifact dialog implementation pair: ScreenshotExport / ResolutionRemap（2026-10-24）

- `ArtifactScreenshotExportDialog.cppm` と `ArtifactResolutionRemapDialog.cppm` を `.cpp` に移し、`Artifact/cmake/ArtifactSources.cmake` のsource pathsを更新した。別public `.ixx` とmodule名を維持した。
- Clang 23.1.4 GNU frontend / Windows MSVC ABIで2 implementationと2 interfaceのfresh P1689 scanが成功し、interface PCMの個別compile後に両implementation objectを個別compileした。ScreenshotExportは `Core.ArtifactMath`、ResolutionRemapは `Core.ArtifactMath` / `Geometry.ResolutionRemap` をrequiresとして検出した。CMake configure / generateも成功した。
- implementation warm repeatは2 requests / 2 sccache hits、misses 886 / errors 35 / failures 56は不変。変更sourceはLF-onlyでgit正規化後に元blobと一致。ArtifactAppRuntime全体build / link / runtime / testsは未確認。

### Artifact layer creation dialogs: Camera deferred / Noise success（2026-10-24）

- `CreateNoiseLayerDialog.cppm` を `.cpp` に移し、`Artifact/cmake/ArtifactSources.cmake` を更新した。`CreateCameraLayerDialog.cppm` も一時 `.cpp` として試したが、fresh P1689 scan / interface PCM compile後のimplementation compileでClang frontendがexit 70でcrashしたため、sourceとmanifestを `.cppm` に戻した。Camera crashはsource diagnosticとは判定していない。
- 最終source構成でCMake configure / generate成功。Noise implementationのfresh P1689 scan、interface PCM compile、個別object compileとwarm repeatが成功し、sccache hitが1件増えた。CRLF正規化後のNoise sourceはgit正規化後に元blobと一致する。
- CameraのClang crashによりsccache compilation failuresは56から57へ増加した。sccache errors 35は不変。ArtifactAppRuntime全体build / link / runtime / testsは未確認。

### Artifact layer creation color picker dependency pair: Hooks / Light dialog（2026-10-24）

- `FloatColorPickerHooks.cppm` と `CreateLightLayerDialog.cppm` を `.cpp` に移し、source manifestを更新した。別public `.ixx` とmodule名を維持した。
- Clang 23.1.4 GNU frontend / Windows MSVC ABIで両implementation / interfaceのfresh P1689 scan、interface PCM生成、implementation object compileが成功した。Hooksをproviderとして先にcompileし、そのPCMを使って `CreateLightLayerDialog` をcompileした。CreateLightのrequiresには `Artifact.Widgets.Dialog.FloatColorPickerHooks` が含まれる。
- 両implementationのwarm repeatは2 requests / 2 sccache hits、misses 893 / errors 35 / failures 57は不変。CMake configure / generate成功。変更sourceはLF-onlyでgit正規化後に元blobと一致する。ArtifactAppRuntime全体build / link / runtime / testsは未確認。

### Asset Browser thumbnail / import dialog implementation pair（2026-10-24）

- `AssetThumbnailPipeline.cppm` と `ArtifactImportAssetsDialog.cppm` を `.cpp` に移し、`Artifact/cmake/ArtifactSources.cmake` を更新した。public `.ixx` とmodule名を維持した。
- Clang 23.1.4 GNU frontend / Windows MSVC ABIで両implementation / interfaceのfresh P1689 scan、interface PCM生成、implementation object compileが成功した。ImportAssetsは `Artifact.Widgets.AssetThumbnailPipeline` をrequiresするため、providerのinterface / implementation PCMを先に生成してからconsumerをcompileした。CMake configure / generate成功。
- 2 implementationのwarm repeatは2 requests / 2 sccache hits、misses 897 / errors 35 / failures 57は不変。変更sourceはLF-onlyでgit正規化後に元blobと一致する。ArtifactAppRuntime全体build / link / runtime / testsは未確認。

### Composition viewer / Layer Editor implementation batch（2026-10-24）

- `ArtifactPieMenuWidget.cppm`、`ArtifactLayerEditorPanel.cppm`、`ArtifactRenderLayerEditor.cppm` を `.cpp` に移し、source manifestを更新した。3件とも別public `.ixx` とmodule名を維持した。`ArtifactLayerEditorPanel` / `ArtifactRenderLayerEditor` は `ArtifactLayerEditorWidget.ixx` のprovider PCMを先行生成して検証した。
- 3 implementationと各public interfaceのfresh P1689 scan、およびinterface PCM / implementation objectの個別compileが成功した。LayerPanelの実装compileで不足していた `<QCloseEvent>` 直接includeをglobal module fragmentに追加後、fresh scan / compileが成功した。
- 同じbatchで試した `ArtifactCompositionViewerFooter.cppm` はimplementation compile中にClang frontend exit 70でcrashしたため、sourceとmanifestを `.cppm` に戻した。source diagnosticとは判断していない。
- 最終CMake configure / generate成功。成功した3 implementationのwarm repeatは3 requests / 3 sccache hits、misses 905 / errors 35 / failures 59は不変。sourceはLF-only。ArtifactAppRuntime全体build / link / runtime / testsは未確認。

### Composition viewport implementation batch: Gizmo3D success / Overlay deferred（2026-10-24）

- `Artifact3DGizmo.cppm` を `.cpp` に移し、`Artifact/cmake/ArtifactSources.cmake` を更新した。public `Artifact3DGizmo.ixx` と `Artifact.Widgets.Gizmo3D` module名を維持した。Overlayも同じ手順で一時移行したが、implementation compileのClang frontend crash後に `.cppm` と元manifest pathへ戻した。
- 両interface / implementationのfresh P1689 scanとinterface PCM compileが成功した。両public interfaceの `export namespace Artifact` 内にある冗長なnested `export` tokenを除くとClang PCM compileが通ることを確認した。Gizmo implementation objectもClang 23.1.4 GNU frontend / Windows MSVC ABIで個別compile成功し、既存 `Artifact3DGizmo.cpp` の `ArtifactGizmoAxis::Anchor` 未処理 `-Wswitch` warningを1件報告した。
- Overlay implementation compileはAST source locationsが診断上約2.15 GBに達した後、Clang frontend exit 70でcrashした。source errorとは判定せず、extension / manifestを `.cppm` へ戻した。最終CMake configure / generate成功。sccache warm repeat統計はこのバッチでは取得していない。ArtifactAppRuntime全体build / link / runtime / testsは未確認。

### Artifact viewport and welcome widget implementation batch（2026-10-24）

- `Artifact3DModelViewer.cppm` と `ArtifactWelcomeWidget.cppm` を `.cpp` に移し、`Artifact/cmake/ArtifactSources.cmake` を更新した。別public `.ixx` と既存module名を維持し、両sourceのfresh P1689 scan、interface PCM生成、implementation object compileがClang 23.1.4 GNU frontend / Windows MSVC ABIで成功した。
- ModelViewer compileで `Artifact.Event.Types.ixx` のmodule purviewにある `Artifact3DModelViewer` forward declarationが、ModelViewer本体moduleと別の所属になりClang errorになった。`ModelViewerDisplayModeChangedEvent::source` を `QObject*` にし、Event.Typesのglobal module fragmentに `<QObject>` を直接includeすると、Event.Types PCMおよびModelViewer implementationがcompile成功した。既存consumerのsource比較と `{this, mode}` aggregate initializationも型変換が成立する。全体link / runtimeでの互換性は未確認。
- `ArtifactLayerCompositeTestWidget.cppm` はinterface PCM / P1689 scan成功後、implementation compileがsource locations約2.13 GB（診断上99%）でClang frontend exit 70となったため `.cppm` に戻した。`ArtifactTimelineLayerTestWidget` はfresh scan / interface PCMまで確認したがobject compile前に同じ大きさのmodule closureによるメモリ圧迫を避けて `.cppm` に戻した。`ArtifactContentGizmo` はinterface PCM生成後、依存 `CompositionGizmoUndoCommands` / `Undo.UndoManager` のPCMがなくobject compile不能だったため `.cppm` に戻した。
- `UndoManager.ixx` PCM生成はClang process working set約6.3 GiB、空きRAM約0.4 GiBまで達したため中断した。最終source構成に戻した後CMake configure / generate成功。ModelViewer / Welcome implementationのwarm repeatは2 requests / 2 cache hits、misses 917 / errors 36 / failures 64は不変。`ArtifactAppRuntime` 全体build / link / runtime / testsは未確認。

### Artifact dialog implementation batch: Precompose success / QuickLayerCreation deferred（2026-10-24）

- `Artifact/src/Widgets/Dialog/PrecomposeDialog.cppm` を `.cpp` に移し、Artifact source manifestを更新した。public `.ixx` とmodule名を維持し、Clang 23.1.4 GNU frontend / Windows MSVC ABIでfresh P1689 scan、interface PCM生成、implementation object compileが成功した。warm repeatは1 request / 1 sccache hit、misses 920 / errors 37 / failures 64は不変。
- `QuickLayerCreationDialog.cppm` も一時 `.cpp` としてfresh P1689 scan成功。interface PCM compileには `Artifact.Menu.Layer` PCMが不足していたため、LayerMenu interface PCMを個別生成したが、QuickLayerCreationDialog interface PCM compile中に空きRAMが約0.6 GiBまで低下したため中断し、sourceとmanifestを `.cppm` に戻した。Quick dialog implementation objectは未compile。
- 最終構成でCMake configure / generate成功。Artifact Runtime全体build / link / runtime / testsは未確認。

### Dialog and font picker implementation batch: Precompose / FontPicker success（2026-10-24）

- `PrecomposeDialog.cppm` に加え、`ArtifactFontPickerWidget.cppm` を `.cpp` に移行し、Artifact source manifestを更新した。両方とも対応するpublic `.ixx` とmodule名を維持した。
- 両sourceのfresh P1689 scan、public interface PCM生成、implementation object compileがClang 23.1.4 GNU frontend / Windows MSVC ABIで成功した。2 implementationのwarm repeatは2 requests / 2 sccache hits、misses 922 / errors 37 / failures 64は不変。
- `CreatePlaneLayerDialog.cppm` はfresh scan成功後、interface PCM生成中にClang working set約4.9 GiB、空きRAM約0.7 GiBまで低下したためcompileを中断し、source / manifestを `.cppm` に戻した。`Artifact.Menu.Layer` PCMとCreatePlane implementation objectは未検証。最終構成でCMake configure / generate成功。ArtifactAppRuntime全体build / link / runtime / testsは未確認。

### Artifact layer implementation group: Switch / SDF success, Composition / Parametric deferred（2026-10-24）

- `ArtifactSwitchLayer.cppm` と `ArtifactSDFLayer.cppm` を `.cpp` に移し、`Artifact/cmake/ArtifactSources.cmake` を更新した。Switchは `Artifact/CMakeLists.txt` の明示 `APP_IMPL` entryも `.cpp` pathへ更新した。両方ともpublic `.ixx` とmodule名を維持し、fresh P1689 scan / implementation object compileが成功した。
- Switch implementationのClang診断で `LayerID` のowner module `Utils.Id` の直接import不足を検出した。implementationだけに `import Utils.Id;` を追加し、fresh scan / object compile成功後に拡張子移行を保持した。
- 同groupで `ArtifactCompositionLayer.cppm` と `ArtifactParametricCompositionLayer.cppm` も一時移行しfresh P1689 scanしたが、implementation compileでそれぞれClang frontend crash / exit 70となった。両source / manifestを `.cppm` に戻した。Composition / Parametricのsource diagnosticとは判断していない。
- 全source path復元後の最終CMake configure / generate成功。Switch / SDF object warm repeatは2 requests / 2 sccache hits、misses 924 / errors 37 / failures 67は不変。ArtifactAppRuntime全体build / link / runtime / testsは未確認。

### Artifact layer implementation group: SelectionManager / Paint / Construction success（2026-10-24）

- `ArtifactLayerSelectionManager.cppm`、`ArtifactPaintLayer.cppm`、`ArtifactConstructionLayer.cppm` を `.cpp` に移し、`Artifact/cmake/ArtifactSources.cmake` を更新した。PaintLayerには `Artifact/CMakeLists.txt` の明示 `APP_IMPL` entryもあるため、その `.cppm` pathsを `.cpp` に更新した。
- 3件ともpublic `.ixx` とmodule名を維持し、fresh P1689 scanおよびClang 23.1.4 GNU frontend / Windows MSVC ABIのimplementation object compileが成功した。warm repeatは3 requests / 3 sccache hits、misses 927 / errors 37 / failures 67は不変。
- 最終source構成でCMake configure / generate成功。ArtifactAppRuntime全体build / link / runtime / testsは未確認。

### Artifact 2D layer implementation group: Abstract2D / SVG success, Solid2D deferred（2026-10-24）

- `ArtifactAbstract2DLayer.cppm` と `ArtifactSvgLayer.cppm` を `.cpp` に移し、source manifestを更新した。両方ともpublic `.ixx` とmodule名を維持し、fresh P1689 scan / Clang 23.1.4 GNU frontend / Windows MSVC ABI implementation object compileが成功した。Abstract2Dではmodule名の `_2D` に対する既存 `-Wreserved-module-identifier` warningが1件出た。
- `ArtifactSolid2DLayer.cppm` も一時 `.cpp` としてfresh scan後にcompileしたが、AST source locationsが約2.13 GB（診断上99%）でClang frontend exit 70となり、source / manifestを `.cppm` に戻した。source diagnosticとは判断していない。
- Solid2Dを復元した最終構成でCMake configure / generate成功。Abstract2D / SVGのwarm repeatは2 requests / 2 sccache hits、misses 929 / errors 37 / failures 68は不変。ArtifactAppRuntime全体build / link / runtime / testsは未確認。

### Artifact 2D sand simulation implementation pair: SandSim2D / MpmSand2D success（2026-10-24）

- `ArtifactSandSim2DLayer.cppm` と `ArtifactMpmSand2DLayer.cppm` を `.cpp` に移し、`Artifact/cmake/ArtifactSources.cmake` と `Artifact/CMakeLists.txt` の明示 `APP_IMPL` pathsを更新した。public `.ixx` / module名を維持した。
- 両implementationのfresh P1689 scanとClang 23.1.4 GNU frontend / Windows MSVC ABI object compileが成功した。warm repeatは2 requests / 2 sccache hits、misses 931 / errors 37 / failures 68は不変。最終source構成のCMake configure / generate成功。
- ArtifactAppRuntime全体build / link / runtime / testsは未確認。
### ArtifactCameraLayer migration (2026-10-24)

- Moved `Artifact/src/Layer/ArtifactCameraLayer.cppm` to `.cpp`; the module implementation remains `module Artifact.Layer.Camera;` and its public interface is unchanged.
- Updated the source manifest. Repository CMake search found no separate direct `APP_IMPL` pin for this source.
- Build/Clang compile was not run under the current repository instruction prohibiting build and test commands, so generated source classification and compilation remain unverified.
### ArtifactNLETransitionBridge migration (2026-10-24)

- Moved `Artifact/src/Layer/ArtifactNLETransitionBridge.cppm` to `.cpp`; module name and public interface were unchanged. Updated the source manifest and confirmed no old CMake source path remains.
- CMake configure/generate succeeded with upstream Clang 23.1.4 GNU frontend and sccache enabled. An isolated implementation compile using the existing module map succeeded.
- A Ninja request for only this object expanded to 6632 actions after regeneration and began transitive OpenUSD work; it was stopped after about 112 actions to limit unnecessary build and disk use. Fresh dependency-scan completion and a full graph build remain unverified.
### Grouped Clang implementation migration batch (2026-10-24)

Migrated four implementation units while keeping their module names and public `.ixx` interfaces unchanged:

- `src/Layer/ArtifactNLETransitionBridge.cppm` → `.cpp` (source manifest)
- `src/Effects/ImageMorph/ImageMorphEffect.cppm` → `.cpp` (existing `APP_IMPL` fallback selects `.cpp` when present)
- `src/Widgets/Coordinate/ArtifactPerformanceHUD.cppm` → `.cpp` (source manifest)
- `src/Widgets/WebUI/ArtifactReportDashboard.cppm` → `.cpp` (source manifest)

CMake configure/generate succeeded with upstream Clang 23.1.4 GNU frontend and sccache enabled. Generated Ninja commands contain `.cpp` compile rules for all four and no corresponding `.cppm` compile rules. Fresh P1689 dependency scans succeeded for all four. Isolated Clang implementation compilation succeeded for NLETransitionBridge. The other three stopped in sccache before Clang compilation because required interface PCMs (`Artifact.Effect.Distort.ImageMorph`, `Artifact.Widgets.CompositionRenderController`, `Artifact.Widgets.WebUIHost`) are absent from the build tree.

A Ninja request for only the NLE bridge object expanded to 6632 actions after regeneration and began transitive OpenUSD generation/compilation; it was stopped after roughly 112 actions to limit build and disk use. Full target build and the three PCM-dependent implementation compiles remain unverified.
### Additional implementation batch (2026-10-24)

- `src/Widgets/Render/ArtifactCompositionViewerFooter.cppm` → `.cpp`: manifest updated; fresh P1689 scan succeeded. Implementation compile reached approximately 2.13 GB source-location usage (99%) and Clang frontend exited 70. No source error was reported; full build remains unverified.
- `src/Diagnostics/AppValidationRules.cppm` → `.cpp`: manifest updated; fresh P1689 scan and isolated Clang implementation compile both succeeded with all referenced PCMs present.
- Both public `.ixx` interfaces and logical module names remain unchanged. The Clang compiler path used sccache.
### Render and camera dialog batch (2026-10-24)

- `src/Render/Software/ArtifactSoftwareImageCompositor.cppm` → `.cpp`: manifest updated; fresh P1689 scan and isolated Clang compile succeeded. Implementation content is unchanged; this is a source-unit extension migration only.
- `src/Widgets/Dialog/CreateCameraLayerDialog.cppm` → `.cpp`: manifest updated; fresh P1689 scan succeeded. Implementation compile reached approximately 2.13 GB source-location usage (99%) and Clang frontend exited 70, without a source diagnostic.
- Both public `.ixx` interfaces and logical module names remain unchanged. Full target build remains unverified.
### Layer implementation batch (2026-10-24)

- `src/Layer/ArtifactFormParticleLayer.cppm` → `.cpp`: updated source manifest and the explicit `APP_IMPL` pin in `Artifact/CMakeLists.txt`; fresh P1689 scan and isolated Clang implementation compile succeeded.
- `src/Layer/ArtifactSolidImageLayer.cppm` → `.cpp`: updated source manifest; fresh P1689 scan succeeded. Isolated implementation compile reached approximately 2.13 GB source-location usage (99%) and Clang frontend exited 70, without a source diagnostic.
- Public `.ixx` interfaces and logical module names remain unchanged. Full application target build remains unverified.
### Layer group and light batch (2026-10-24)

- `src/Layer/ArtifactGroupLayer.cppm` → `.cpp` and `src/Layer/ArtifactLightLayer.cppm` → `.cpp`; updated both source manifest entries. Their public `.ixx` interfaces and module names are unchanged.
- Fresh P1689 scans and isolated Clang implementation compiles succeeded for both with the existing module maps and PCM dependencies.
- CMake configure/generate succeeded with upstream Clang 23.1.4 GNU frontend and sccache enabled. sccache cumulative snapshot after these compiles: 2,515 requests, 1,338 hits, 937 misses, 40 cache errors, 71 compilation failures (includes earlier attempts in this build environment).
- Full application target build and link remain unverified.
### Widget implementation batch (2026-10-24)

- `src/Widgets/CollabPresenceWidget.cppm` → `.cpp`
- `src/Widgets/ArtifactLooksPresetBrowser.cppm` → `.cpp`

Both source manifest paths were updated; no direct `APP_IMPL` pins were present. Their public `.ixx` interfaces and module names remain unchanged. The only missing direct PCM in each old module map was its own interface PCM. Both interface units compiled individually first; after migration, CMake configure/generate, fresh P1689 scans, and sccache-launched Clang implementation compiles succeeded. Source contents are byte-identical to the prior `.cppm` blobs.
### Small widget implementation batch (2026-10-24)

- `src/Widgets/StyleSurface.cppm` → `.cpp`; added a direct `#include <QPaintEvent>` in the global module fragment after Clang reported the event type was undeclared. Fresh scan and implementation compile then succeeded.
- `src/Widgets/TimingEventView.cppm` → `.cpp`; fresh P1689 scan and implementation compile succeeded.
- `src/Widgets/PowerShellWidget.cppm` → `.cpp`; fresh P1689 scan and implementation compile succeeded.
- All three interface `.ixx` files were individually compiled first to provide their module PCMs. Updated source manifest paths; public interfaces and logical module names remain unchanged. CMake configure/generate succeeded with sccache enabled.
### Widget service implementation batch (2026-10-24)

Three implementations migrated successfully:

- `src/Widgets/Dock/DockStyleManager.cppm` → `.cpp`
- `src/Widgets/ArtifactStatusBar.cppm` → `.cpp`
- `src/Widgets/Tasks/ArtifactDetachedTaskTray.cppm` → `.cpp`

For all three, interface PCM compile, fresh P1689 dependency scan, and isolated Clang implementation compile succeeded. Source manifest paths were updated; public interfaces and module names were unchanged.

`src/Widgets/ArtifactSecondaryPreviewWindow.cppm` was assessed in the same batch. Its interface PCM compile and fresh scan succeeded, but implementation compile failed at `W_OBJECT_IMPL` with ambiguous `QObject::d_ptr` `operator->` lookup and frontend signal termination. Its source extension and manifest registration were restored to `.cppm`; investigate the Verdigris / Qt module boundary before retrying. Final CMake configure/generate succeeded with this restoration applied.
### Assistant and color widget batch (2026-10-24)

- `src/Widgets/AIChatWidget.cppm` → `.cpp`: interface PCM compile, fresh P1689 scan, and isolated Clang implementation compile succeeded.
- `src/Widgets/ArtifactColorPaletteWidget.cppm` → `.cpp`: interface PCM compile, fresh P1689 scan, and isolated Clang implementation compile succeeded.
- `src/Widgets/ArtifactFilmstripWidget.cppm` was tested in the same batch. Its interface PCM and fresh scan succeeded, but the implementation frontend terminated by signal. Its source path and manifest entry were restored to `.cppm`.
- Updated the two successful source manifest entries; all public interfaces and module names are unchanged. Final CMake configure/generate succeeded after restoring FilmstripWidget.
### Curve editor / expression widget batch (2026-10-24)

- `src/Widgets/ArtifactCurveEditorWidget.cppm` → `.cpp`: source manifest updated; interface PCM compile, fresh P1689 scan, and isolated Clang implementation compile succeeded.
- `src/Widgets/ArtifactExpressionCopilotWidget.cppm` was tested in the same batch. Its interface PCM and fresh scan succeeded, but implementation compile terminated by Clang frontend signal; its source and manifest path were restored to `.cppm`.
- `ArtifactAudioService` was not migrated: its interface compile terminated by frontend signal. `ArtifactPointTrackerGizmo` was not migrated: interface compile was stopped after working set reached about 4.2 GiB and free RAM fell to about 1.48 GiB.
- Final CMake configure/generate succeeded with CurveEditor on `.cpp` and ExpressionCopilot on `.cppm`. Full application build remains unverified.

### Migration boundary audit and reference repair (2026-10-25)

- A static audit of the in-progress Artifact / ArtifactCore working trees found 54 moved `.cpp` files beginning with `export module` (9 in Artifact, 45 in ArtifactCore). These are interface units outside this milestone's implementation-unit migration scope; restored them to `.cppm` and updated explicit CMake source paths while preserving file contents.
- Corrected all nine stale paths in `Artifact/cmake/ArtifactRenderModuleReferences.cmake` for renderer and LOD implementation units already moved to `.cpp`.
- Migrated `Artifact/src/Layer/ArtifactLayerRuntimeRenderSupport.cppm` to `.cpp` and updated `Artifact/cmake/ArtifactSources.cmake`. The public `.ixx`, module name, imports, and source bytes were preserved; this migration has not been compiled yet.
- Migrated `Artifact/src/Layer/ArtifactHierarchyModel.cppm` to `.cpp` and updated the same source manifest. Its public `.ixx`, module declaration, imports, and source bytes are preserved; this migration has not been compiled yet.
- Migrated `Artifact/src/Widgets/ArtifactAlignmentWidget.cppm` to `.cpp` and updated the source manifest. Its public `.ixx`, module declaration, imports, and source bytes are preserved; Clang compilation remains unverified.
- Migrated `Artifact/src/Widgets/Coordinate/ArtifactCoordinateManager.cppm` to `.cpp` and updated the source manifest. Its public `.ixx`, module declaration, imports, and textual implementation are preserved; line endings were normalized from CRLF to repository-standard LF. Clang compilation remains unverified.
- Clarified `ArtifactCore/CMakeLists.txt` comments to state that clang-cl's P1689/PCM path applies to migrated `.cpp` implementation units; this matches its existing extension filter and LLVM's documented distinction between `.cpp` implementation units and `.cppm` importable units.
- Migrated `Artifact/src/Effects/Rasterizer/KaleidoscopeEffect.cppm` to `.cpp`; updated its source manifest and every explicit CMake target reference. The module declaration and implementation text are unchanged. Clang and target builds remain unverified.
- Migrated the module-free import translation units `ArtifactAbstractLayerModulation.cppm`, `ArtifactAbstractLayerRuntimeSnapshot.cppm`, and `ArtifactAbstractLayerCollision.cppm` to `.cpp`, updating the source manifest. Their imports and definitions remain unchanged; CRLF input was normalized to LF. This lets Clang classify them as ordinary C++ import consumers instead of relying on the nonstandard `.cppm` extension.
- Migrated `Artifact/src/Widgets/ArtifactMarkdownNoteEditorWidget.cppm` to `.cpp` and updated its source manifest entry. The implementation text is unchanged and LF is enforced; its Clang P1689 scan and compile are unverified.
- Migrated `Artifact/src/Widgets/ArtifactAudioMiniWidget.cppm` to `.cpp` and updated its source manifest entry. Its public interface and implementation text are unchanged; LF is enforced. Clang P1689 scan and compilation remain unverified.
- Migrated `Artifact/src/Widgets/WebUI/ArtifactWebBridge.cppm` to `.cpp` and updated its source manifest entry. Its implementation text is unchanged, LF is enforced, and it remains in the Clang P1689 scan path through the existing `.cpp` source classification. Compilation is unverified.
- Migrated `Artifact/src/Application/ArtifactApplicationManager.cppm` to `.cpp` and updated its source manifest entry. The public interface and implementation text are preserved with LF endings; Clang scanning and compilation remain unverified.
- Migrated `Artifact/src/Widgets/ArtifactDopeSheetWidget.cppm` to `.cpp`, updating the source manifest and both explicit `APP_IMPL` path checks in `Artifact/CMakeLists.txt`. The UI implementation is unchanged; LF is enforced and Clang compilation remains unverified.
- Migrated `Artifact/src/Widgets/ArtifactAnimationTimelineWidget.cppm` to `.cpp` and updated its source manifest entry. Its public interface and implementation text are preserved; LF is enforced and the Clang scan/compile remains unverified.
- Migrated `Artifact/src/Widgets/Render/ArtifactLayerCompositeTestWidget.cppm` to `.cpp` and updated its source manifest entry. Its public interface and implementation text are unchanged; LF is enforced and Clang compilation remains unverified.
- Migrated `Artifact/src/Widgets/ArtifactAnchorPointTool.cppm` to `.cpp` and updated the source manifest. Its public interface and implementation text are unchanged; LF is enforced and Clang compilation remains unverified.
- Migrated `Artifact/src/Application/ActiveContextService.cppm` to `.cpp` and updated the source manifest. Its existing context and transport delegation behavior is unchanged; LF is enforced and Clang P1689 scan/compile remain unverified.
- Migrated `Artifact/src/Service/ArtifactDetachedTaskService.cppm` to `.cpp` and updated the source manifest. Its task service implementation is unchanged and LF is enforced; Clang scan and compilation remain unverified.
- Migrated `Artifact/src/Layer/ArtifactCompositionLayer.cppm` to `.cpp` and updated its source manifest entry. The public interface and layer implementation text are unchanged; LF is enforced and Clang compilation remains unverified.
- Migrated `Artifact/src/Project/ArtifactProjectModel.cppm` to `.cpp` and updated its source manifest entry. The public interface and project-model implementation text are unchanged; LF is enforced and Clang compilation remains unverified.
- Migrated `Artifact/src/Widgets/Render/ArtifactCompositionCleanup.cppm` to `.cpp` and updated its source manifest entry. Its public interface and dialog implementation text are unchanged; LF is enforced and Clang scanning/compilation remain unverified.
- Migrated `Artifact/src/Service/FootageInterpretService.cppm` to `.cpp` and updated the source manifest. No footage or decoding behavior changed; LF is enforced and Clang P1689 scan/compile remain unverified.
- Rechecked all 17 Artifact renderer and 823 ArtifactCore module-reference entries: every listed implementation source exists. CMake source-file references to the remaining deleted `.cppm` implementation paths were also checked statically and none remain.
- No build, configure, or test command was run in this pass. Compiler classification, full target integration, and Clang compilation remain unverified for this broader in-progress batch.

- Migrated `Artifact/src/Widgets/ArtifactSnapshotCompareWidget.cppm` to `.cpp` and updated its explicit source manifest entry. The module name, interface boundary, and widget implementation are unchanged; the source was normalized to LF. Clang scan/compile remain unverified.
- Migrated `Artifact/src/Widgets/LayerEditorContextMenu.cppm` to `.cpp` and updated its explicit source manifest entry. Its imports, module name, and implementation are unchanged; Clang scan/compile remain unverified.
- Migrated `Artifact/src/Script/ArtifactPythonHookManager.cppm` to `.cpp` and updated its explicit source manifest entry. Source content and module declaration are unchanged; Clang scan/compile remain unverified.
- Migrated `Artifact/src/Widgets/ArtifactCompositionGraphWidget.cppm` to `.cpp` and updated its explicit source manifest entry. The graph widget implementation and module boundary are unchanged; Clang scan/compile remain unverified.
- Migrated `Artifact/src/Project/ArtifactRevisionService.cppm` to `.cpp` and updated its explicit source manifest entry. The revision service implementation, imports, and module declaration are unchanged; Clang scan/compile remain unverified.
- Clarified Artifact manifest comments: public `.ixx` units and source-only module units now explicitly include migrated `.cpp` implementations, matching the existing Clang scan classification. This is comment-only; no CMake configure/build was run.
- Migrated `Artifact/src/Widgets/Dialog/ArtifactCreateCompositionDialog.cppm` to `.cpp` and updated its explicit source manifest entry. The dialog implementation and module declaration are unchanged; Clang scan/compile remain unverified.
- Migrated `Artifact/src/Project/ArtifactProjectImporter.cppm` to `.cpp` and updated its explicit source manifest entry. Import behavior, module declaration, and public interface are unchanged; Clang scan/compile remain unverified.
- Migrated `Artifact/src/Export/Python/ArtifactPythonAPI.cppm` to `.cpp` and updated its explicit implementation manifest entry. The public `.ixx` and module implementation text are retained; the prior CRLF source was normalized to LF. Clang scan/compile remain unverified.
- Reapplied the mechanical migration of `Artifact/src/Widgets/Dialog/QuickLayerCreationDialog.cppm` to `.cpp` and updated `ARTIFACT_APP_IMPL_SOURCES`. Earlier history records that P1689 scanning succeeded but the unrelated interface PCM step exhausted available memory; this turn does not rerun CMake or build, and object compilation remains unverified. The former CRLF source was normalized to LF.
- Migrated `Artifact/src/Widgets/Dialog/ArtifactRenderOutputSettingDialog.cppm` to `.cpp` and updated its implementation manifest entry. Its public interface and module body are preserved; the previous CRLF source was normalized to LF. Clang scan/compile remain unverified.
- Migrated `Artifact/src/Widgets/ArtifactMenuBar.cppm` to `.cpp` and updated its explicit implementation manifest entry. Menu behavior and module declaration are unchanged; Clang scan/compile remain unverified.
- Migrated `Artifact/src/Widgets/CompositionTextEditor.cppm` to `.cpp` and updated its implementation manifest entry. The text editor behavior, module declaration, and `.ixx` API are preserved; prior CRLF was normalized to LF. Clang scan/compile remain unverified.
- Migrated `Artifact/src/Widgets/Timeline/ArtifactTimelineKeyframeModel.cppm` to `.cpp` and updated its implementation manifest entry. Timeline model behavior and property-group filtering are untouched; the former CRLF file was normalized to LF. Clang scan/compile remain unverified.
- Added compiler-specific source classification to `ArtifactPr/CMakeLists.txt`: MSVC compiles implementation `.cppm` as C++, upstream clang++ gets `-x c++` plus CXX module scanning, and clang-cl enables CXX module scanning. Migrated `.cpp` module implementations with named declarations are also explicitly scanned for both Clang frontends. This closes the previously missing ArtifactPr Clang lane in CMake; configure/build validation remains prohibited and unverified.
- Migrated `ArtifactPr/src/PrStatusNotifier.cppm` to `.cpp` and updated `ARTIFACT_PR_IMPL_SOURCES`. Its `ArtifactPr.StatusNotifier` interface and implementation remain unchanged; the source was normalized to LF. The new Clang source scan/compile path is statically configured but unverified by build.
- Migrated `ArtifactPr/src/Translation.cppm` and `ArtifactPr/src/PrShortcut.cppm` to `.cpp`; `ARTIFACT_PR_IMPL_SOURCES` now points to the ordinary C++ implementation paths. Public `.ixx` declarations are unchanged, source bytes were retained with LF endings, and Clang compile results are pending an authorized build.
- Migrated `ArtifactPr/src/MarkerEditDialog.cppm` to `.cpp` and updated the implementation list. Its public dialog API and existing picker/event behavior are unchanged; Clang scanning is covered by the new source rule but compile validation is pending an authorized build.
- Migrated `ArtifactPr/src/PrColorPickerDialog.cppm` to `.cpp` and updated the implementation list. The existing custom palette picker remains unchanged; its source already used LF. Clang compile remains unverified.
- Migrated `ArtifactPr/src/ArtifactPrDemoData.cppm` to `.cpp` and updated the implementation list. The public demo-data API and data values are unchanged; source remains LF. Clang compile is not run.
- Migrated `ArtifactPr/src/ArtifactPrCLI.cppm` to `.cpp` and updated `ARTIFACT_PR_IMPL_SOURCES`; the CLI module API and commands are unchanged. The source is LF, and Clang object compilation was not run.
- Corrected the root Clang P1689 comment to describe the current per-source implementation scan policy; the prior text incorrectly said all module implementation scans were disabled while Artifact and ArtifactPr explicitly opt selected implementation sources in. Comment-only change.
- Migrated `ArtifactPr/src/ShortcutHelpDialog.cppm` to `.cpp` and updated the implementation list. Existing shortcut definitions and UI are unchanged; this exercises the new Clang scan rule for a migrated module that imports `ArtifactPr.Shortcut`. Clang compile remains unverified.
- Migrated `ArtifactPr/src/ExportDialog.cppm` to `.cpp` and updated the implementation list. Export UI and sequence-export calls are unchanged; the `.cpp` module dependency is covered by explicit Clang scan classification, but compile validation is pending.
- Migrated `ArtifactPr/src/ProjectPanel.cppm` to `.cpp` and updated the implementation list. Project tree and conform behavior are unchanged; the `.cpp` module declaration is enabled for Clang scanning by the new source classifier.
- Migrated `ArtifactPr/src/TimecodeOverlayWidget.cppm` to `.cpp` and updated the implementation list. The existing timecode painter and widget API are unchanged; source remains LF, with compile validation pending.
- Migrated `ArtifactPr/src/AppTheme.cppm` to `.cpp` and updated the implementation list. The existing `QProxyStyle` behavior and public theme API are unchanged; Clang scan/compile are not run.
- Migrated `ArtifactPr/src/SequenceCompositor.cppm` to `.cpp` and updated the implementation list. The canonical RGBA composition algorithm is unchanged; its named implementation now takes the explicit Clang P1689 scan path. No compositor build or performance check was run.
- Migrated `ArtifactPr/src/VideoSurface.cppm` to `.cpp` and updated the implementation list. No frame conversion, buffering, or signaling behavior changed; Clang compile/runtime are not verified.
- Migrated `ArtifactPr/src/VideoPlayerWidget.cppm` to `.cpp` and updated the implementation list. Playback and frame conversion behavior remain unchanged; the source implementation now uses the named-module `.cpp` scan lane. Runtime/video validation is not performed.
- Migrated `ArtifactPr/src/ClipEffects.cppm` to `.cpp` and updated `ARTIFACT_PR_IMPL_SOURCES`. Effect calculations and frame-processing code are unchanged; Clang P1689 scan is configured for the module implementation, but compile/performance remain unverified.
- Migrated `ArtifactPr/src/EditCommand.cppm` to `.cpp` and updated the implementation list. Serializable command and undo/redo behavior are unchanged; Clang scan/object verification remains pending.
- Migrated `ArtifactPr/src/TransportBarWidget.cppm` and `ArtifactPr/src/MediaPanel.cppm` to `.cpp`, updating the implementation list. Existing transport controls, media import behavior, and UI event wiring are unchanged; the named `.cpp` units use the explicit Clang scan classifier. Runtime/Clang compile validation remains unverified.
- Migrated `ArtifactPr/src/SequenceAudioRenderer.cppm` to `.cpp` and updated the implementation list. Offline audio rendering logic is unchanged; the module dependency scan is enabled by the Clang source classifier, while compilation and performance remain unverified.
- Migrated `ArtifactPr/src/MediaFrameDecoder.cppm` to `.cpp` and updated the implementation list. Still-image/video decode behavior and the public request/result API are unchanged; Clang scan is configured, but compile/runtime are not verified.
- Migrated `ArtifactPr/src/MediaThumbnailer.cppm` to `.cpp`, updated its implementation-list entry, and added direct `<QCoreApplication>` / `<QEventLoop>` includes for APIs already used by `generateSync`. Thumbnail extraction behavior is unchanged; Clang compilation remains unverified.
- Migrated `ArtifactPr/src/AudioPreviewMixer.cppm` to `.cpp` and updated the implementation list. Existing preview mix, callback, and timing behavior are unchanged; its imported modules are handled by the new Clang scan classification. Audio/runtime validation remains unverified.
- Migrated `ArtifactPr/src/SequenceExporter.cppm` to `.cpp` and updated the implementation list. Video, audio, and image-sequence export behavior are untouched; imported module dependencies now use the explicit `.cpp` Clang scan path. Export/runtime validation was not run.
- Migrated `ArtifactPr/src/TimelinePanel.cppm` to `.cpp` and updated the implementation list. Timeline model/UI behavior and its Verdigris declarations are unchanged; this Clang module compile path is not yet verified.
- Migrated `Artifact/src/Widgets/Render/ArtifactPointTrackerGizmo.cppm` to `.cpp` and updated `Artifact/cmake/ArtifactSources.cmake`. The module declaration, imports, cached drawing scratch buffers, gizmo behavior, and rendering/input logic are unchanged; LF endings are enforced. Prior Clang validation stopped during interface PCM generation under memory pressure, so scanning and implementation compilation remain unverified.
- Migrated `Artifact/src/Render/DiligentBindlessSubmitter.cppm` to `.cpp` and updated the Artifact source manifest and render module-reference map. The module declaration/import, bindless submission code, shader/resource handling, and rendering behavior are unchanged; source line endings are LF. Clang scan/compile and GPU runtime behavior remain unverified.
- Migrated `Artifact/src/Render/DiligentUploadCoordinator.cppm` to `.cpp` and updated the Artifact source manifest. Static inspection found no module declaration or imports; this is a regular C++ translation unit. The upload queue, resource synchronization, backpressure, and ownership code are unchanged; LF endings are used. Clang compile and GPU runtime behavior remain unverified.
- Migrated `Artifact/src/Render/ArtifactBatchRenderer.cppm` to `.cpp` and updated the Artifact source manifest. Its `Artifact.Render.Batch` implementation, three module imports, public `.ixx`, and batch-render behavior are unchanged; LF endings are enforced. Prior Clang work stopped at interface PCM generation due memory pressure, so scan and implementation compile remain unverified.
- Migrated `Artifact/src/Widgets/Render/ArtifactRenderQueueManagerWidget.cppm` to `.cpp` and updated the Artifact source manifest. The `Artifact.Widgets.Render.QueueManager` implementation, imports, public `.ixx`, existing queue controls, and event handling are unchanged; LF endings are enforced. Clang scan and compile remain unverified.
- Migrated `Artifact/src/Widgets/Render/ArtifactDiligentEngineRenderWindow.cppm` to `.cpp` and updated the Artifact source manifest. The implementation module declaration, public `.ixx`, imports, Diligent rendering, and window input behavior are unchanged; LF endings are enforced. Clang scan/compile and GPU runtime behavior remain unverified.
- Migrated `Artifact/src/Render/DiligentDeviceManager.cppm` to `.cpp` and updated the Artifact source manifest plus render module-reference map. The `Artifact.Render.DiligentDeviceManager` declaration, `Artifact.Render.Config` import, D3D12/Vulkan device and swap-chain code are unchanged; LF endings are enforced. Clang scan/compile and GPU backend runtime behavior remain unverified.
- Migrated `Artifact/src/Render/ArtifactRenderQueueEncoder.cppm` to `.cpp` and updated the Artifact source manifest. The `Artifact.Render.Queue.Encoder` implementation, eight imports, public `.ixx`, and backend selection/encoding behavior are unchanged; LF endings are enforced. This module is consumed by the render-job worker; no per-frame logic was changed. Clang scan/compile and encoder runtime remain unverified.
- Restored the executable Artifact `CMakeLists.txt` from the repository version after finding its working-tree contents were a copy of the migration notes, while root CMake calls `add_subdirectory(Artifact)`. Updated 148 distinct active CMake source references for migrated `.cppm` → `.cpp` files; migration history remains in this planned milestone. The real Clang classifier (CXX_SCAN_FOR_MODULES for named `.cpp` implementations, GNU `-x c++` only for remaining `.cppm`, MSVC CompileAsCpp only for `.cppm`) is active again. No CMake configure or build was run.
- Extended Artifact Clang source classification to include `.cpp` implementation units containing `import` declarations even when they have no named `module` declaration. Static manifest inspection found six such translation units. This keeps import-only consumers in explicit P1689 scan classification; no configure or build was run.
- Migrated `Artifact/src/Render/ShaderManager.cppm` to `.cpp` and updated the Artifact source manifest, render module-reference map, and explicit CMake render-slice paths. The module declaration, imports, public `.ixx`, shader/PSO implementation, and Diligent D3D12/Vulkan-neutral behavior are unchanged; LF endings are enforced. Clang scan/compile and backend runtime behavior remain unverified.
- Migrated `Artifact/src/Render/PrimitiveRenderer3D.cppm` to `.cpp` and updated the Artifact source manifest, render module-reference map, and explicit CMake render-slice paths. The implementation module, public `.ixx`, Diligent primitive submission and frame rendering behavior are unchanged; LF endings are enforced. This remains on the shared D3D12/Vulkan Diligent path; Clang scan/compile and GPU runtime behavior are unverified.
- Migrated `Artifact/src/Layer/ArtifactImageLayer.cppm` to `.cpp` and updated the Artifact source manifest. The `Artifact.Layer.Image` implementation, public `.ixx`, imports, image-layer/cache behavior, and rendering code are unchanged; LF endings are enforced. Clang scan/compile and image-layer runtime behavior remain unverified.
- Migrated `Artifact/src/Layer/ArtifactShapeLayer.cppm` to `.cpp` and updated the Artifact source manifest. The `Artifact.Layer.Shape` implementation, public `.ixx`, shape data/persistence and rendering/input behavior are unchanged; LF endings are enforced. Clang scan/compile and shape-layer runtime behavior remain unverified.
- Migrated `Artifact/src/Effect/ArtifactCreativeEffects.cppm` to `.cpp` and updated the Artifact source manifest. The `Artifact.Effect.Creative` implementation, public `include/Effects/ArtifactCreativeEffects.ixx`, imports, effect application and Diligent/CPU paths are unchanged; LF endings are enforced. Clang scan/compile and effect runtime behavior remain unverified.
- Migrated `Artifact/src/Layer/ArtifactParticleLayer.cppm` to `.cpp` and updated the Artifact source manifest. The existing paired `ArtifactParticleLayer3D.cpp` implementation remains in the same `Artifact.Layer.Particle` module; public `.ixx`, module imports, particle layer behavior and rendering code are unchanged. LF endings are enforced; Clang scan/compile and runtime behavior remain unverified.
- Migrated the two remaining registered Artifact implementation units `ArtifactAbstractLayerUtilities.cppm` and `ArtifactCompositionHostFrameSnapshot.cppm` to `.cpp`; updated their explicit source manifest entries. Their named module declarations/imports and implementation behavior are unchanged; LF preserved. Clang P1689 scanning is covered by the existing Artifact `*_IMPL` classifier, but configure and compile remain unverified.
- Extended Clang P1689 classification to directly attached import-only entry points: `Artifact/src/AppMainEntry.cpp` and ArtifactPr GUI/CLI `src/main.cpp` / `src/main_cli.cpp`. They are outside the implementation-source lists, so the existing module scanner did not cover them. No source behavior changed; configure and scan generation remain unverified.
- Updated the ArtifactPr CMake source-convention comment to identify `.cpp` module implementations, matching the fully migrated source list; this is documentation-only.
- Corrected ArtifactCore Clang handling for the already migrated FileInfo partition source: its exclusion now matches `FileInfo.cpp`, not the obsolete `.cppm` path. Both Clang frontends keep P1689 scanning disabled for this special partition and translate its explicit IFC references to PCM references; all other migrated `.cpp` units keep normal scan classification. Static only; configure and compile remain unverified.
- Audited module/import-aware `.cpp` sources outside the two explicit source manifests. Active sources are owned by split `*_IMPL` / `*_MODULES` lists or dedicated targets; direct target entry imports are explicitly classified. Remaining unregistered sources are not enabled by CMake. No stale CMake references to migrated `.cppm` paths were found.
- Reconciled existing Clang diagnostic logs with current source: RationalTime is now a `.cpp` module implementation; ExplorerUtils and FrameSkipTracker no longer contain the nested `export` declarations reported by older logs. The GPUCapabilities missing-IFC record came from an isolated compile without its provider. These historical failures are not evidence of a current full-build failure; current Clang configure/scan/compile remains unverified.
- Reconfigured with CMake 4.4.4 and directly retried the GNU Clang 22 `ArtifactPreviewCompositionPipeline.cpp` compile after removing redundant imports; it still fails near the same ~2.13 GB cumulative AST source-location limit. A temporary extraction of clone selection overlay did not reduce the failure and could not be compiled without a generated `.modmap`, so that experimental source split was reverted. The source-location-limit issue remains open; no full rebuild was run.
- Restricted `/std:c++latest` and `/experimental:module` target options to actual MSVC (`ARTIFACT_COMPILER_MSVC_CL`) across Core, Widgets, top-level smoke targets, and the test host. A fresh CMake 4.4.4 clang-cl configure completed in a separate build directory; its generated CoreAudio compile command retains Clang `-std=c++23` and no longer contains either MSVC-only option. The existing preset directory has a vcpkg manifest-mode cache mismatch and was left intact. No compilation was run in the fresh configure tree.
- Ninja dependency-command inspection shows a single Core `AnimatableTransform2D.cpp` object depends on 2,317 generated commands, while the Preview clone-overlay object would depend on 10,536. These counts explain why the isolated target compiles are not small checks; no commands from those target graphs were executed in the fresh tree.
- Confirmed the generated `AnimatableTransform2D.cpp` scan rule invokes `clang-scan-deps -format=p1689` and contains neither MSVC-only module flag. This verifies Clang scan-rule generation for a migrated `.cpp`; the scan itself and object compile were not run in the fresh tree.
### clang-cl follow-up (2026-10-25)

- A CMake 4.4.4 clang-cl configure completed and generated a `DockManager.ixx` dependency scan command, but the scan failed because `Artifact/DockFoundation/CMakeLists.txt` applied `/Zf` under the broad `if(MSVC)` condition. CMake reports `MSVC` true for clang-cl, so this target-level option bypassed the repository's compiler-ID guard.
- Narrowed that target's option block to `MSVC AND CMAKE_CXX_COMPILER_ID STREQUAL "MSVC"`. This preserves `/Zf` for Microsoft cl.exe and removes it from clang-cl.
- After reconfiguring with CMake 4.4.4, the generated Ninja `DockManager.ixx` scanner rule no longer contained `/Zf`; running its `.ddi` scanner target succeeded and emitted `Dock.Manager`. The `DockManager.ixx` object and remaining DockFoundation interfaces then compiled, and `ArtifactDockFoundation.lib` linked. No tests were run.

- Built `ArtifactCoreEnvironment` in the root clang-cl CMake 4.4.4 tree: `EnvironmentVariable.cpp` (migrated `.cppm`) and both provider interfaces passed dyndep/compile, then `ArtifactCoreEnvironment.lib` linked. Built the full `ArtifactDockFoundation` target: three remaining dock interfaces compiled and the static library linked after the `/Zf` fix. Clang emitted Qt `QStringLiteral` TU-local exposure warnings and `-TP` unused warnings on interface units; these did not fail compilation. No application-wide link was attempted.

- Ran the current clang-cl P1689 scanner commands directly from the generated Ninja rules for migrated `EnvironmentVariable.cpp`, `ArtifactAudioMiniWidget.cpp`, `ArtifactApplicationManager.cpp`, and the six-unit `ArtifactPreviewCompositionPipeline.cpp` split, plus `DockManager.ixx`. All five scans succeeded without `/Zf`; the generated `.ddi` records contained the expected module providers/requirements (10 requirements for AudioMiniWidget, two for ApplicationManager, and 11 for PreviewCompositionPipeline). This validates current scanner execution and dependency discovery, not PCM regeneration, object compilation, or application linking.

- Repeated those five current-tree P1689 scanner commands with upstream GNU-frontend Clang 22.1.3. All succeeded, and the `.ddi` dependency names/counts matched clang-cl (AudioMiniWidget 10, ApplicationManager two, PreviewCompositionPipeline 11; `DockManager.ixx` provides `Dock.Manager`). This confirms the two Clang frontends classify and discover these representative migrated units consistently; module compilation, broader object coverage, and final link remain outstanding.

- After the DockFoundation fix, audited root optional Windows test/viewer targets and found a second `/Zf` leak: these used `ARTIFACT_FRONTEND_MSVC` but passed `/Zf` unconditionally. Gated `/Zf` (including the Plugin Host smoke target) on `ARTIFACT_COMPILER_MSVC_CL`. A CMake 4.4.4 clang-cl configure with Plugin Host enabled generated scan and compile commands with no `/Zf`, `/std:c++latest`, or `/experimental:module`, while retaining `-std=c++23`. Directly ran the generated Plugin Host P1689 scan command; it succeeded with seven `requires` entries. Requesting that scan through Ninja expanded to a 2,814-action OpenUSD dependency graph, so it was stopped; target-integrated scan execution and object compilation remain unverified.
- A fresh GNU-frontend Clang 22 build-tree clean rebuild of `ArtifactCoreEnvironment` succeeded through P1689 scanning, dyndep, compilation of migrated `EnvironmentVariable.cpp` and its interfaces, and static archive link. A source audit also narrowed the remaining broad `if(MSVC)` guards around `/Zf` in the always-configured VideoPreviewBench and the standalone GPU smoke project to actual MSVC; test CMake files already use the frontend-specific guard and were left untouched.
