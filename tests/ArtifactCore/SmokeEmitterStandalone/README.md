# Smoke Emitter Image R&D

`ArtifactCoreSmokeEmitterStandalone` exercises the `Physics.Fluid` module without
configuring or building the Artifact application. Its image case advances a
fixed 96x72 smoke scene, writes `smoke_emitter.png` and a second
`smoke_emitter_shifted.png` with the emitter moved horizontally, then checks
that the exported pixels respond to that change. It also writes
`smoke_emitter_repeat.png` from the same scene and checks exact pixel equality,
along with density signal, upward transport, and that the plume stays inside
the image's top edge. The 24-frame, 0.6-density scene is a deterministic scalar
density visualization for solver R&D; it does not claim parity with the
application's particle renderer or viewport color management.

Configure and build this directory as its own CMake project, then run the
`ArtifactCoreSmokeEmitterStandaloneTest` executable with
`ARTIFACT_SMOKE_OUTPUT_DIR` set to an absolute output directory. Compare two
exports with:

```powershell
python tests/ArtifactCore/compare_smoke_images.py reference.png candidate.png `
  --max-mean-absolute-error 0.03 --report comparison.json
```

The comparison requires Python and Pillow. Keep the reference PNG outside the
build tree and update it only after reviewing the visual change.
