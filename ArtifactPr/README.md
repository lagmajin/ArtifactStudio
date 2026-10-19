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

A standalone widget contract executable can be enabled with `ARTIFACT_BUILD_PR_STANDALONE_TESTS=ON` together with `ARTIFACT_BUILD_PR=ON`. It constructs the Timeline panel independently of `ArtifactPrMainWindow` and checks that a deterministic sequence, track, and clip are represented in the widget tree. Build and runtime verification are still pending.
