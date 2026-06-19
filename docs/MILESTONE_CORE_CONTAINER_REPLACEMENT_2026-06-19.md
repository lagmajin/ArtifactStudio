# Milestone: Core Container Replacement

## Goal

Reduce direct use of `std` and Qt core containers in domain code by growing ArtifactCore-owned container APIs first, then migrating call sites gradually.

The replacement layer should:

- keep `std::vector`, `QVector`, `QList`, `QString`, and friends behind ArtifactCore boundaries
- expose smaller and easier APIs than raw `std` / Qt containers
- avoid template-heavy names at common call sites
- carry debug names and lightweight diagnostic metadata for AI-assisted investigation

## Principles

- Start with contiguous arrays because `std::vector`, `QVector`, and `QList` replacement pressure is highest there.
- Keep Qt at UI and I/O boundaries. Core data structures should not require Qt types for naming or diagnostics.
- Do not remove existing containers in one sweep. New code should prefer ArtifactCore containers; old code migrates per module.
- Prefer named helpers like `count`, `isEmpty`, `add`, `at`, `each`, and `debugInfo` over exposing raw iterator and allocator vocabulary.
- Prefer named helpers like `count`, `isEmpty`, `append`, `insert`, `contains`, `at`, `each`, and `debugInfo` over exposing raw iterator and allocator vocabulary.
- Keep escape hatches local and explicit when a migration needs interop with existing code.

## Phase 1: Foundation

- Add `ContainerDebugInfo` and `ContainerName`.
- Add `NamedVector<T>` as the first `std::vector` replacement surface.
- Add `NamedList<T>` for code that genuinely needs stable node-style insertion/removal semantics.
- Export the new modules through `Container`.
- Use `NamedVector<T>` in new or touched low-risk code first.

## Phase 2: Migration Candidates

- Asset and frame records using simple `std::vector<T>` storage.
- Small queues and histories currently backed by `QList` / `QVector`.
- Diagnostic snapshots where named containers make bug reports easier to read.
- `DiagnosticResult` now uses `NamedVector<ProjectDiagnostic>` internally as the first real migration example.
- `ValidationRuleRegistry` now uses `NamedVector<std::unique_ptr<IValidationRule>>` internally as another low-risk migration example.
- `FrameDebugBundle::history` now uses `NamedVector<FrameDebugCapture>` internally as a second low-risk migration example outside diagnostics.

## Phase 3: Broader Types

- Add `NamedList<T>` only if linked-list semantics are truly needed.
- Add `SmallVector<T, N>` for hot-path fixed-small collections.
- Add `NameMap<K, V>` / `IdMap<K, V>` after call sites show stable access patterns.
- Add string and path replacements separately; avoid coupling text design to container migration.

## Debug Support

Every replacement container should be able to report:

- container name
- value type label
- count
- capacity if available
- approximate bytes if cheap
- empty/non-empty state

This gives AI and humans a stable, compact inspection vocabulary without reading raw implementation details.

## Debug Roadmap

The debug layer should grow from cheap metadata into a useful observation surface. Keep the default path lightweight, and make expensive capture opt-in per container or per debug session.

### D1: Identity And Classification

- `ContainerName`: human-readable instance name.
- `ContainerDomain`: broad subsystem label such as `Timeline`, `Render`, `Selection`, `Asset`, `Cache`, `Diagnostics`, or `Unknown`.
- `ContainerOwner`: optional owner name/id pair for paths like `CompositionEditor.selectedLayers`.
- `ContainerDebugInfo`: compact always-available summary.

Status:

- implemented for the first container generation
- `ContainerDebugInfo` now carries `ContainerDomain` and `ContainerOwner`
- `ARTIFACT_CONTAINER_HERE` and `ARTIFACT_CONTAINER_OWNER` macro helpers are available for call sites

Purpose:

- Lets AI group related containers before reading their contents.
- Makes logs and snapshots readable without exposing implementation types.

### D2: Version And Mutation Counters

- `version`: increments on every mutating operation.
- `mutationCount`: total number of successful mutations.
- `readCount`: optional count of read/helper calls.
- `failedAccessCount`: increments on out-of-range `at`, invalid remove, or empty `first/last` when failure tracking is enabled.
- `maxCountSeen`: largest observed count.

Status:

- implemented for the first container generation
- `NamedVector<T>` and `NamedList<T>` now track these counters

Purpose:

- Makes before/after snapshot comparison trivial.
- Helps find unexpected clears, growth spikes, and stale-observer bugs.

### D3: Source Location Tracking

- `createdAt`: source file, line, function, and optional label.
- `lastMutatedAt`: source of the most recent successful mutation.
- `lastFailedAccessAt`: source of the most recent invalid access.

API shape:

- Prefer macro helpers at call sites, for example `ARTIFACT_CONTAINER_HERE`.
- Keep source location storage as plain `const char*` plus line number to avoid Qt/string dependencies.

Purpose:

- Answers "who created this?" and "who last changed this?" without a native debugger.

### D4: Mutation History Ring

- `ContainerMutationRecord`: operation, version, count before/after, source location, optional small note.
- Fixed-size ring buffer, default disabled or small.
- Records operations such as `add`, `make`, `reserve`, `clear`, `removeAt`, `removeIf`, `removeFirst`, `removeLast`, and failed access if enabled.

Purpose:

- Gives AI the last few meaningful events instead of a raw stack trace.
- Keeps runaway history memory bounded.

Status:

- implemented for the first container generation
- `NamedVector<T>` and `NamedList<T>` keep a small bounded mutation ring and expose the latest mutation

### D5: Snapshots

- `ContainerDebugSnapshot`: full debug observation of one container.
- Includes identity, counters, capacity, approximate bytes, last operation, recent history, and optional element samples.
- `debugSnapshot()` returns this richer structure.
- `debugInfo()` remains the cheap summary.

Purpose:

- Separates cheap always-on telemetry from richer debugging.
- Gives AppDebugger, logs, and future MCP tools one stable object to consume.

Status:

- implemented for the first container generation
- `debugSnapshot()` now returns identity, counters, source locations, last mutation, and bounded samples

### D6: Element Sampling

- `debugSample(count)`: records a bounded first/last sample.
- Element formatting must be explicit:
  - arithmetic types can format directly
  - strings use existing text conversion boundaries
  - custom types can provide a `debugLabel()` or external formatter later
- Do not iterate huge containers by default.

Purpose:

- Lets AI inspect shape and representative contents without dumping everything.

Status:

- partially implemented for the first container generation
- `debugSample()` captures bounded element addresses and indexes for the first few entries

### D7: Watch Conditions

- Add opt-in watch rules:
  - count exceeds threshold
  - count drops to zero unexpectedly
  - failed access occurs
  - mutation happens during a guarded phase
  - version changes between two expected-stable checkpoints
  - read count grows beyond an expected bound
- First implementation can call a callback or append a trace event.
- Later implementation can bridge to `TraceSnapshot` / `AppDebuggerWidget`.

Purpose:

- Turns containers into semantic breakpoints without a low-level debugger.

Status:

- partially implemented for the first container generation
- `watch(...)` can notify a callback when count, version, read-count, empty-state, mutation, or failed-access conditions match
- `ContainerWatchHit` can be rendered into a compact text line for diagnostics

### D8: JSON And Tooling Bridge

- Add conversion helpers for debug snapshots once the struct shape stabilizes.
- Avoid Qt JSON in the core container module if possible; keep JSON serialization in a diagnostics bridge module.
- Expose snapshots to:
  - AppDebuggerWidget
  - bug report bundles
  - future local MCP debug tools

Purpose:

- Makes the debug data useful outside C++ and readable by AI tooling.

Status:

- implemented for the first container generation
- `Container.Debug.Text` provides a lightweight text bridge for `ContainerDebugInfo` and `ContainerDebugSnapshot`
- `Container.Debug.Json` provides Qt JSON conversion helpers for the same snapshot family
- `Container.Debug.Text` also formats `NamedVector<T>` / `NamedList<T>` through their debug snapshots
- `Container.Debug.Text` now includes source location and sample summaries for snapshots
- `Container.Debug.Json` now serializes `ContainerDebugSnapshot.samples` via `NamedVector<ContainerElementSample>`
- `NamedVector<T>` and `NamedList<T>` can be created with `createdAt` metadata through helper factories
- `ValidationRuleRegistry`, `DiagnosticResult`, `FrameDebugSnapshot`, and `FrameDebugBundle` use named containers with debug metadata
- `PluginRegistry` snapshot helpers now build their result buffers with `NamedVector<PluginDescriptor>` before returning `std::vector`
- `EventBusDebugger` snapshot helpers now use `NamedVector` for fire logs and statistics before returning legacy vectors
- `EventBusDebugger` now returns named vectors for fire logs, subscribers, frequency snapshots, and per-event stats
- `FallbackTracker` stores its event log in `NamedVector<FallbackEvent>` and builds filtered snapshots through named buffers
- `TraceSnapshot` crash and event histories now use `NamedVector` while keeping legacy QVector fields for the rest
- `FrameSkipTracker` stores its dispatch history in `NamedVector<FrameDispatchEvent>` while keeping QVector return types
- `TraceFrameLaneRecord::scopes` now uses `NamedVector<TraceScopeRecord>` for lane-local history
- `TraceFrameTimelineRecord::lanes` now uses `NamedVector<TraceFrameLaneRecord>` for frame-local lane history
- `TraceSnapshot::threads` now uses `NamedVector<TraceThreadRecord>` for per-thread state snapshots
- `TraceSnapshot::scopes` and `TraceSnapshot::locks` now use `NamedVector` for trace-level histories
- `SessionLedger` now stores entries and recovery points in `NamedVector` and returns recovery snapshots through named buffers
- `FrameRange` now builds its sampled frame lists through `NamedVector` internally before returning legacy `std::vector`
- `SceneNode` now builds child and descendant lists through `NamedVector` internally before returning legacy `std::vector`
- `PerformanceProfiler` now stores startup events and per-sample histories in `NamedVector` while keeping legacy return types
- `MotionTracker` now builds point-position and keyframe result lists through `NamedVector` internally before returning legacy `std::vector`
- `AudioMixer` now builds bus-name, bus-list, sorted-routing, and side-chain result buffers through `NamedVector` internally before returning legacy `std::vector`
- `ShapeGroup` now builds child lists through `NamedVector` internally before returning legacy `std::vector`
- `ShapePath` now builds subpath and equidistant-sample buffers through `NamedVector` internally before returning legacy `std::vector`
- `GlyphLayout` now builds text-layout glyph buffers through `NamedVector` internally before returning legacy `std::vector`
- `TextShapingBackend` now builds horizontal and vertical glyph buffers through `NamedVector` internally before returning legacy `std::vector`
- `PathMorph` now builds subpath and resampled-point buffers through `NamedVector` internally before returning legacy `std::vector`
- `RotoMask` now builds sampled vertex buffers through `NamedVector` internally before returning legacy `std::vector`
- `AudioAnalyzer` now builds its mono mix buffer through `NamedVector` internally before FFT analysis
- `FrameSkipTracker` now builds recent and skipped-event snapshots through `NamedVector` internally before returning legacy `QVector`
- `EventBusDebugger` now builds frequency and per-event snapshots through `NamedVector` internally before returning legacy `std::vector`
- `EventBusDebugger` now stores its fire log in `NamedVector` while keeping legacy `std::vector` snapshots
- `MatteStack` now stores its node list in `NamedVector` while keeping legacy snapshot APIs
- `MatteStack` now builds its source-layer id lists through `NamedVector` internally before returning legacy `std::vector`
- `AssetSequence` now stores detected sequence groups in `NamedVector`
- `AssetSequence` now stores detected singleton filenames in `NamedVector`
- `MotionTracker` now builds problem-frame and tracker-list snapshots through `NamedVector` internally before returning legacy `std::vector`
- `MotionTracker.TrackResult` now stores frames and failure frames in `NamedVector` with explicit debug names
- `AnimatableValue` now stores keyframes in `NamedVector` and returns copied legacy vectors at the API boundary
- `Geometry.Interpolate` now stores keyframes in `NamedVector` and returns copied legacy vectors at the API boundary
- `Shape.Repeater` now builds repeated path outputs through `NamedVector` internally before returning legacy `std::vector`
- `Shape.AeOperators` now builds merge/offset path outputs through `NamedVector` internally before returning legacy `std::vector`
- `Shape.AeOperators` now builds pucker/twist path outputs through `NamedVector` internally before returning legacy `std::vector`
- `Shape.AeOperators` now builds rounded-corners and wiggle path outputs through `NamedVector` internally before returning legacy `std::vector`
- `Shape.AeOperators` now builds wiggle/zigzag/hand-drawn path outputs through `NamedVector` internally before returning legacy `std::vector`

## Proposed Debug Types

- `ContainerSourceLocation`
  - `const char* file`
  - `const char* function`
  - `int line`
- `ContainerOwner`
  - `const char* name`
  - `const char* id`
- `ContainerDebugCounters`
  - `version`
  - `mutationCount`
  - `readCount`
  - `failedAccessCount`
  - `maxCountSeen`
- `ContainerMutationRecord`
  - operation name
  - version after operation
  - count before
  - count after
  - source location
  - note
- `ContainerDebugSnapshot`
  - identity
  - info
  - counters
  - source locations
  - recent mutations
  - optional samples

## Implementation Order

1. Extend `Container.Debug` with source location, owner, counters, and mutation record structs.
2. Add counters to `NamedVector<T>` and `NamedList<T>`.
3. Increment `version`, `mutationCount`, and `maxCountSeen` in mutating methods.
4. Track `failedAccessCount` for invalid `at`, invalid remove, and empty `first/last`.
5. Add `debugSnapshot()` without element samples.
6. Add a bounded mutation history ring with a small default capacity.
7. Add optional source-location macro helpers for mutation methods.
8. Add element sampling only after formatter rules are clear.
9. Add diagnostics bridge JSON conversion outside the base container module.
10. Wire a first low-risk container snapshot into an existing diagnostics surface.

## Guardrails

- Do not make debug bookkeeping expensive by default.
- Do not put Qt dependencies into the base container modules.
- Do not expose raw `std::vector` / `std::list` as the primary API.
- Do not force every call site to mention template-heavy debug types.
- Keep source-location capture optional where call-site noise would get high.
- Keep hot render-path adoption opt-in until overhead is measured.

## First Success Criteria

- A public `Container` import exposes named vector primitives.
- Named containers provide `append`, `insert`, `contains`, and `takeAt` style migration helpers.
- Named containers provide `resize`, `takeFirst`, `takeLast`, and `toStdVector` helpers for gradual migration.
- Named containers provide `assign`, `front`, `back`, `popFront`, and `popBack` helpers to mirror common Qt/std call sites.
- Container-level algorithms provide `each`, `removeIf`, and `toStdVector` helpers to bridge existing code during migration.
- New code can create a named container without mentioning `std::vector` or Qt containers.
- Debug metadata can be read without iterating the container.
- Container mutations produce version/counter changes.
- Invalid access can be observed through counters and snapshots.
- A full debug snapshot can be consumed without knowing whether storage is `std` or Qt backed.
