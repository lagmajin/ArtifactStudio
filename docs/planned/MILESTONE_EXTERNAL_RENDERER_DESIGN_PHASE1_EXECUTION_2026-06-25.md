# External Renderer Design Phase 1 Execution (2026-06-25)

## Goal

Make `M-RE-1` actionable by defining the first thin slice of an external renderer workflow without replacing the built-in renderer.

## Current Reality

`ArtifactRenderQueueService` already exists and already owns queue jobs, preflight, progress, failure reporting, and render backend selection. That makes it the natural place to host the first external-renderer handoff.

The current queue model is already close to the required boundary:

- composition id and name
- frame range
- output path and format
- resolution and fps
- audio options
- render backend
- overlay transform
- preflight diagnostics

So Phase 1 does not need a new distributed system. It needs a snapshot job contract.

## Phase 1 Scope

1. Define a `RenderJobSnapshot` DTO family that is pure data.
2. Add a serialization path from `ArtifactRenderQueueService` jobs to a JSON job file.
3. Add a complementary load path so the renderer can be driven from `--job <file>`.
4. Keep stdout/stderr and exit code as the first communication channel.
5. Reserve progress and cancellation for later IPC work.

## Snapshot Shape

Phase 1 should carry only what the renderer needs to start:

- composition id / name
- frame start / end / fps
- resolution / pixel ratio / alpha mode
- output path / format
- quality preset
- layer snapshot
- effect snapshot
- asset resolve data
- diagnostics flags

The snapshot must not carry live `ArtifactAbstractLayer` or `ArtifactAbstractEffect` instances.

## Minimal Deliverables

- a JSON schema note in the design doc
- a job file example
- a parent-producer / child-consumer execution flow
- a return contract for exit code, error string, and output summary

## Non-Goals For Phase 1

- no full bidirectional RPC
- no shared GPU context
- no process pool
- no distributed rendering
- no live object sharing over IPC

## Recommended Next Step

Implement a snapshot serializer on top of the existing render queue job model, then add a tiny headless entrypoint that can read `--job` and report progress back to stdout.

## Status

Phase 1 is feasible and can be started now. The smallest useful slice is job snapshot generation plus a CLI entrypoint contract.
