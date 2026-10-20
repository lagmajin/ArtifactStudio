# ArtifactPr

**最終更新:** 2026-10-19

Pr-like editor that reuses `ArtifactCore` as the shared foundation.

`ArtifactPr` is a first-class application target in this repository. It shares
`ArtifactCore` (and the shared GPU device foundation `ArtifactGpuFoundation`)
with `Artifact`, and it deliberately keeps its own NLE-oriented editing model —
sequence / track / clip — rather than reusing the Composition/Layer editor.

Concretely, the two applications split as follows:

- `Artifact` owns the AE-style Composition/Layer editing and rendering surface.
- `ArtifactPr` owns the multi-track NLE surface, program monitor, and export.
- Both consume `ArtifactCore` for media, properties, expression evaluation,
  and the image pipeline; neither application links the other's editor layer.

Any Artifact-side capability that is shared infrastructure (device management,
texture upload, encoding) belongs in the shared foundation targets rather than
in one application's editor modules.

## UI module boundaries

The NLE sequence / track / clip model remains owned by ArtifactPr. The Timeline widget implementation is separated into `ArtifactPr.TimelinePanel`; `ArtifactPr.MainWindow` owns dock assembly and connects the panel to the program-monitor waveform provider. The module boundary is an initial extraction, not a claim that the whole application shell and every panel have already been split into small modules.

A standalone widget contract executable can be enabled with `ARTIFACT_BUILD_PR_TESTS=ON` together with `ARTIFACT_BUILD_PR=ON`.

## Targets

`ArtifactPr/CMakeLists.txt` builds one static library and two thin entry points:

- `ArtifactPrApp` (STATIC) owns every `ArtifactPr.*` module unit.
- `ArtifactPr` (WIN32) contains only `src/main.cpp`; the `--cli` switch inside it keeps the console path on the same `EditorEngine` document/undo/render owner as the GUI.
- `ArtifactPrCLI` contains only `src/main_cli.cpp`.

Module declaration convention, matching `ArtifactCore`: `include/*.ixx` is the module interface unit (`export module ArtifactPr.X;`) and `src/*.cppm` is the module implementation unit (`module ArtifactPr.X;`). A `.cppm` must not redeclare its module as an interface unit, and must not import the module it implements.

## Tests

The contract tests live in `tests/ArtifactPr/` and link `ArtifactPrApp` instead of re-compiling the module set, so one new test costs one source file. They are registered when `ARTIFACT_BUILD_PR=ON` and a GTest-based test option is on. The ArtifactPr suite alone:

```sh
cmake --preset <preset> -DARTIFACT_BUILD_PR=ON -DARTIFACT_BUILD_PR_TESTS=ON
ctest -L artifactpr
```

| test | covers |
| --- | --- |
| `ArtifactPrContractTest` | clip effect math (`fx.*` reading, transition curves, EQ), CPU compositor, timecode formatting, shortcut registry, export format routing, `RenderPlan::isValid` |
| `ArtifactPrEditorEngineTest` | new-project invariants, in/out clamping, render plan quality/range/audio mute-solo-fx filter, snapping, NLE snapshot and project save/load round trip |
| `ArtifactPrTimelinePanelTest` | timeline ruler/track/clip/zoom object-name contract and refresh on `sequenceChanged` |

Deliberately outside this suite: `GpuProgramMonitor` (needs a live Diligent device), real frame decoding (`MediaFrameDecoder`, `MediaThumbnailer`), and `exportSequence` / `renderSequenceAudio` beyond their argument validation.

CMake configure, build and CTest have not been run for these targets; the suite is written and statically reviewed only.

\n