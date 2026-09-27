# ArtifactPr

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
