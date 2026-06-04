# Debug MCP / Pseudo Breakpoint Plan - 2026-06-04

## Goal

AE-style application debugging in Artifact is hard to reason about with IDE breakpoints alone.
This plan defines a higher-level debug surface that can:

- observe app state in meaningful AE-like vocabulary
- pause the app when a semantic condition becomes true
- expose that state through an MCP server for external inspection

The intent is not to replace the native debugger. It is to add a domain-aware debug layer that answers:

- what composition state are we in now?
- what changed just before things broke?
- can we stop when a specific UI or playback condition appears?

## Existing Foundations

The current project already has useful building blocks.

- `AppDebuggerWidget`
  - unified app-level diagnostics surface
- `TraceRecorder`
  - low-cost trace and lock/event capture
- playback services / playback engine
  - safe app-level pause and frame stepping entry points
- project health / diagnostics services
  - summarized error and warning state

Relevant entry points:

- [docs/WIDGET_MAP.md](/X:/Dev/ArtifactStudio/docs/WIDGET_MAP.md)
- [Artifact/src/Widgets/Diagnostics/AppDebuggerWidget.cppm](/X:/Dev/ArtifactStudio/Artifact/src/Widgets/Diagnostics/AppDebuggerWidget.cppm)
- [Artifact/src/Playback/ArtifactPlaybackEngine.cppm](/X:/Dev/ArtifactStudio/Artifact/src/Playback/ArtifactPlaybackEngine.cppm)
- [Artifact/src/Widgets/ArtifactTimelineWidget.cpp](/X:/Dev/ArtifactStudio/Artifact/src/Widgets/ArtifactTimelineWidget.cpp)

## Non-Goals

- do not add a low-level CPU debugger
- do not stop execution at arbitrary instruction pointers
- do not broadly modify Diligent or DX12 backend code
- do not introduce new global signal/slot wiring

The pause model should remain cooperative and app-driven.

## Concept

### 1. Debug Snapshot

Create one aggregated app snapshot that can be read cheaply and repeatedly.

Suggested contents:

- current project id / composition id / composition name
- selected layer ids and names
- current frame / work area / visible timeline range
- playback state, speed, loop state
- current project health token and summary
- last N trace events
- optional focused property path and value summary

This should be a read-only, app-level state object.

### 2. Condition Probe

Add a small condition evaluator that runs at safe checkpoints.

Safe checkpoints:

- playback tick boundaries
- frame advance boundaries
- timeline selection update boundaries
- project/composition swap boundaries
- diagnostic refresh boundaries

The probe evaluates user-registered conditions against the current `DebugSnapshot`.

### 3. Pseudo Breakpoint

When a condition matches:

- mark a `break hit`
- store the matched condition id and timestamp
- capture the matching `DebugSnapshot`
- request a cooperative pause through app-level playback control

This is a soft stop, not a thread suspension.

### 4. MCP Exposure

Expose the debug state and controls through an MCP server.

The MCP server is the external control surface.
The app remains the source of truth and performs the actual pause/resume behavior.

## Architecture

### In-Process Side

Suggested components:

- `DebugSnapshot`
  - immutable summary object for one observation point
- `DebugSnapshotService`
  - gathers current app state from project/playback/timeline/debug services
- `BreakCondition`
  - typed description of one semantic stop condition
- `ConditionProbeService`
  - stores conditions and evaluates them at checkpoints
- `BreakSessionState`
  - last hit metadata, captured snapshot, paused reason

Suggested placement:

- keep this in parent app code near existing diagnostics/services
- avoid pushing the first iteration into low-level render code

### External Side

Suggested MCP server process:

- Node.js or Python
- reads current debug state through a local bridge
- registers conditions
- requests resume / single-step

Bridge options:

- local HTTP loopback
- named pipe
- local JSON file polling for first prototype

Recommended order:

1. in-process C++ services
2. local bridge
3. MCP server wrapper

## Break Condition Types

Initial condition set should stay narrow and semantic.

### Playback Conditions

- current frame equals target frame
- current frame enters target range
- playback state changes to `Paused` or `Stopped`
- playback loops back to work area start

### Selection Conditions

- selected layer id equals target layer id
- selection count becomes zero or non-zero
- selected layer type changes to video/text/shape/particle

### Diagnostics Conditions

- project health token becomes `warning` or `error`
- render preflight reports blocking diagnostics
- trace contains a specific token

### Property Conditions

- focused property path equals target path
- property numeric value exceeds threshold
- property value changes from a prior snapshot

## Safe Evaluation Points

Keep the first version small and predictable.

Primary hooks:

- `ArtifactPlaybackEngine`
  - after frame advance
  - before/after pause transitions
- project/playback services
  - after composition or state changes
- timeline orchestration layer
  - after selection/playhead changes if cheap

Avoid first-pass hooks in:

- D3D12 backend internals
- Diligent low-level passes
- hot render loops that lack a stable app-level semantic boundary

## First MCP Tool Set

The first tool set should be read-mostly.

- `get_debug_snapshot`
  - return the latest aggregated app snapshot
- `set_break_condition`
  - register one semantic condition
- `list_break_conditions`
  - enumerate active conditions
- `clear_break_condition`
  - remove one condition
- `resume_debug_session`
  - continue from a soft stop
- `step_one_tick`
  - advance one cooperative tick or one frame boundary
- `get_last_break_hit`
  - return reason, matched condition, and captured snapshot

## UI Integration

Use the existing `AppDebuggerWidget` instead of building a parallel surface.

Minimal UI additions:

- current break status
- active break condition list
- last matched condition
- captured snapshot summary
- last trace events around the hit

Suggested placement:

- new `Breaks` tab, or
- extension of existing `State` / `Trace` tab areas

No new global signal/slot network should be introduced for this.
Prefer polling or existing service refresh patterns already used by diagnostics UI.

## Implementation Phases

### Phase 1 - Snapshot Only

Deliverables:

- `DebugSnapshot` type
- snapshot collection service
- App Debugger text export of snapshot

Success criteria:

- can inspect current AE-like state without a native breakpoint

### Phase 2 - Condition Probe

Deliverables:

- typed `BreakCondition`
- evaluation at playback-safe checkpoints
- cooperative pause on match
- last break hit persistence

Success criteria:

- can stop automatically when a semantic condition is reached

### Phase 3 - App Debugger Integration

Deliverables:

- break status display
- matched condition display
- snapshot and trace context around hit

Success criteria:

- can understand why the app stopped without digging into raw debugger frames

### Phase 4 - MCP Server

Deliverables:

- local bridge from app to external process
- MCP tools for snapshot / condition / resume / step

Success criteria:

- an external agent can inspect and control the debug session

### Phase 5 - Advanced Actions

Possible additions:

- replay last user action
- stop on event sequence
- compare pre-hit and post-hit snapshots
- snapshot export bundle

These are optional and should wait until the read-mostly path is stable.

## Risks

### Performance Risk

If snapshot gathering is too heavy, the debug layer will distort the app behavior.

Mitigation:

- keep the first snapshot textual and selective
- avoid full deep traversal of every layer/property on every tick
- support coarse and detailed snapshot modes later if needed

### Stop Safety Risk

Stopping in the wrong place may leave UI or playback in an inconsistent state.

Mitigation:

- only pause at explicit safe checkpoints
- treat the pause as a transport/control pause, not a low-level execution freeze

### Scope Risk

If the first version tries to inspect everything, it will stall.

Mitigation:

- start with playback, selection, diagnostics, and trace
- add property-level detail only when needed

## Recommended First Vertical Slice

The best first end-to-end slice is:

1. collect `DebugSnapshot`
2. support `frame == X` break condition
3. pause via playback engine
4. show last break hit in `AppDebuggerWidget`
5. expose `get_debug_snapshot`, `set_break_condition`, and `resume_debug_session`

This gives immediate value without entering risky low-level rendering code.

## Initial Prototype Location

A standalone MCP prototype now lives in:

- [tools/debug-mcp-server/README.md](/X:/Dev/ArtifactStudio/tools/debug-mcp-server/README.md)
- [tools/debug-mcp-server/server.js](/X:/Dev/ArtifactStudio/tools/debug-mcp-server/server.js)

It currently runs with a local mock snapshot or an optional bridge file via `ARTIFACT_DEBUG_BRIDGE_FILE`.
The app-side bridge hook now writes a temp-file snapshot by default, so the standalone server can read live state without extra configuration.

## Open Decisions

- bridge transport: HTTP vs named pipe vs file polling
- exact owner of current selection snapshot
- whether `step_one_tick` means one frame, one playback tick, or one UI update cycle
- how much property-tree data belongs in the default snapshot

## Recommendation

Build this as a parent-repo diagnostics feature first, not as a deep render-backend feature.

That keeps the first implementation:

- semantically useful
- low risk for Diligent/DX12 code
- aligned with existing `AppDebuggerWidget` and diagnostics work
- compatible with a future MCP server without redoing the core model
