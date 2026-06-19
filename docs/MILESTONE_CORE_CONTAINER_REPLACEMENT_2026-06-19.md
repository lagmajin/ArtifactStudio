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

Purpose:

- Lets AI group related containers before reading their contents.
- Makes logs and snapshots readable without exposing implementation types.

### D2: Version And Mutation Counters

- `version`: increments on every mutating operation.
- `mutationCount`: total number of successful mutations.
- `readCount`: optional count of read/helper calls.
- `failedAccessCount`: increments on out-of-range `at`, invalid remove, or empty `first/last` when failure tracking is enabled.
- `maxCountSeen`: largest observed count.

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

### D5: Snapshots

- `ContainerDebugSnapshot`: full debug observation of one container.
- Includes identity, counters, capacity, approximate bytes, last operation, recent history, and optional element samples.
- `debugSnapshot()` returns this richer structure.
- `debugInfo()` remains the cheap summary.

Purpose:

- Separates cheap always-on telemetry from richer debugging.
- Gives AppDebugger, logs, and future MCP tools one stable object to consume.

### D6: Element Sampling

- `debugSample(count)`: records a bounded first/last sample.
- Element formatting must be explicit:
  - arithmetic types can format directly
  - strings use existing text conversion boundaries
  - custom types can provide a `debugLabel()` or external formatter later
- Do not iterate huge containers by default.

Purpose:

- Lets AI inspect shape and representative contents without dumping everything.

### D7: Watch Conditions

- Add opt-in watch rules:
  - count exceeds threshold
  - count drops to zero unexpectedly
  - failed access occurs
  - mutation happens during a guarded phase
  - version changes between two expected-stable checkpoints
- First implementation can call a callback or append a trace event.
- Later implementation can bridge to `TraceSnapshot` / `AppDebuggerWidget`.

Purpose:

- Turns containers into semantic breakpoints without a low-level debugger.

### D8: JSON And Tooling Bridge

- Add conversion helpers for debug snapshots once the struct shape stabilizes.
- Avoid Qt JSON in the core container module if possible; keep JSON serialization in a diagnostics bridge module.
- Expose snapshots to:
  - AppDebuggerWidget
  - bug report bundles
  - future local MCP debug tools

Purpose:

- Makes the debug data useful outside C++ and readable by AI tooling.

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
- New code can create a named container without mentioning `std::vector` or Qt containers.
- Debug metadata can be read without iterating the container.
- Container mutations produce version/counter changes.
- Invalid access can be observed through counters and snapshots.
- A full debug snapshot can be consumed without knowing whether storage is `std` or Qt backed.
