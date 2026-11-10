# Milestone: Core Container Replacement

**最終更新:** 2026-11-10

## Update 2026-11-09 — FileWatcher path index migration

- Replaced the private `std::unordered_map` with the named `NameMap` container. Existing watch, unwatch, callback replacement, polling, and synchronization behavior stays behind the same API.
- Added a 48-path growth/removal contract case and registered the NameMap/ArtifactHashMap module dependencies in the standalone project.
- **Verification:** MSVC 19.51 built the FileWatcher standalone target and all 14 contract tests passed via CTest.

## Update 2026-11-09 — TypedColumn owned storage migration

- Replaced `TypedColumn`'s private parsed-value `std::vector` with `ArtifactCore::Array`. The constructor input remains `std::vector<String>` for source compatibility.
- Added a storage-growth and input-release contract case, and corrected the standalone module file set base directory so its `.cppm` dependency configures correctly.
- **Verification:** MSVC 19.51 built the standalone target and all 21 `TypedColumnStandaloneTest` cases passed via CTest.

## Update 2026-11-09 — NLE edit-history storage migration

- Replaced the private undo/redo `std::vector` stacks with named `NamedVector` instances. Public NLE APIs and serialized project/history entries remain unchanged; depth trimming now uses the named front-removal operation.
- **Verification:** MSVC 19.51 successfully compiled both `ArtifactCoreNLE/src/NLE/Core.cpp` and `tests/ArtifactCore/NLETest.cpp` through their Ninja object targets. The existing `ArtifactCoreNLETest` executable was not linked or run because its normal target dependency graph requested 2,623 build steps; runtime history tests remain the next check.

## Update 2026-11-09 — StepSequencer storage migration

- Replaced the private step array's `std::vector` with a named `NamedVector` instance without changing the public step access API.
- Added a standalone 10-case contract suite. It exposed and fixed initialization leaving the default loop at step zero, and signed advancement now widens to 64-bit before modulo so `INT32_MIN` / `INT32_MAX` cannot overflow.
- **Verification:** StepSequencer standalone target built and all 10 contract tests passed on MSVC 19.51.

## Update 2026-11-09 — DebugIdentity registry migration

- Replaced the private live-object registry's `std::vector<DebugIdentity*>` with the existing `NamedVector` API. Snapshot return values remain `std::vector` at the public compatibility boundary.
- **Verification:** DebugIdentity standalone target built and all 10 contract tests passed on MSVC 19.51.

## Update 2026-11-09 — HashSet contract hardening

- `HashSet` copy, move, and swap now keep stateful hash/equality functors paired with their stored buckets.
- Copying rebuilds the same observable iteration order; `values()` returns oldest-to-newest insertion order as its contract tests specify.
- Expanded the standalone contract coverage for stateful functors. All 29 tests passed on MSVC 19.51.

## Update 2026-11-09 — ArtifactArray memory and aliasing hardening

- `Array<T>` uses correctly aligned storage for over-aligned elements. Reallocation now cleans up partial construction on exceptions and preserves the old buffer until all elements are relocated; `squeeze()` releases storage for an empty array.
- Self-append and aliased append / insert / replace inputs are staged before growth or shifting. Fixed the typed `Optional<T&>` return in `find()` that prevented its missing-value path from instantiating.
- Added alignment and throwing-copy coverage; the standalone ArtifactArray contract suite passes on MSVC 19.51.

## Update 2026-11-09 — Zero-capacity StaticArray support

- Added `StaticArray<T, 0>` as an empty, constexpr-capable specialization that does not construct a `T`, matching the valid zero-element case of the standard fixed array abstraction.
- Added compile-time and runtime contract coverage, including a non-default-constructible element type.

## Update 2026-11-09 — Array mutation exception safety

- `insert`, `replace`, and `removeAt` now avoid destroying or shifting existing elements before potentially throwing copies complete. Types with non-throwing moves keep the in-place mutation path; other types rebuild using `move_if_noexcept`. Copyable types retain the original elements when rebuilding throws; move-only types with throwing moves retain valid object lifetimes but may leave already-moved elements in a moved-from state.
- Added failure-injection coverage checking values, buffer identity, capacity, and live object counts after failed mutations. The standalone contract suite passes on MSVC 19.51.

## Update 2026-11-09 — Move-only HashSet insertion

- Added rvalue overloads for `HashSet::add` / `insert`, enabling copy-disabled keys and preserving duplicate arguments by checking membership before moving them.
- Corrected the repeated collision-chain contract to remove all modeled entries before asserting the set is empty. The standalone suite now exercises 31 cases and passes on MSVC 19.51.

## Update 2026-11-09 — SmallVector storage and lifetime safety

- Heap growth now uses aligned allocation and keeps old elements alive until relocation completes; failed copies clean up the partial destination.
- Inline moves no longer promise unconditional `noexcept`, and partial inline moves clean up constructed elements before propagating exceptions. Self-aliased add/insert values are staged before growth or shifting.
- Resize, insert, remove, and take paths keep `size_` consistent with constructed objects when element operations throw. Added standalone tests for alignment, failed reserve/remove/insert, and throwing inline moves.
- Configured the standalone project with the installed MSVC 19.51 / Ninja toolchain and corrected its module list so `ArtifactOptional.cppm` is built as an interface unit. The watch test also exposed a missing explicit `NamedVector::toStdVector()` conversion in `debugSnapshot()`.
- Removed `noexcept` from `debugSnapshot()` because it builds bounded sample vectors and the explicit compatibility conversion can allocate.
- **Verification:** ArtifactArray, HashSet, and SmallVector standalone targets built successfully and passed CTest on MSVC 19.51. The SmallVector executable reports 43 passing contract cases.

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

## Phase 1: Foundation ✅

- Add `ContainerDebugInfo` and `ContainerName`. — ✅
- Add `NamedVector<T>` as the first `std::vector` replacement surface. — ✅
- Add `NamedList<T>` for code that genuinely needs stable node-style insertion/removal semantics. — ✅
- Export the new modules through `Container`. — ✅
- Use `NamedVector<T>` in new or touched low-risk code first. — ✅

## Phase 2: Migration Candidates

- Asset and frame records using simple `std::vector<T>` storage.
- Small queues and histories currently backed by `QList` / `QVector`.
- Diagnostic snapshots where named containers make bug reports easier to read.
- `DiagnosticResult` now uses `NamedVector<ProjectDiagnostic>` internally as the first real migration example.
- `ValidationRuleRegistry` now uses `NamedVector<std::unique_ptr<IValidationRule>>` internally as another low-risk migration example.
- `FrameDebugBundle::history` now uses `NamedVector<FrameDebugCapture>` internally as a second low-risk migration example outside diagnostics.

## Phase 3: Broader Types

- Add `NamedList<T>` only if linked-list semantics are truly needed.
- Add `SmallVector<T, N>` for hot-path fixed-small collections. — ✅ (2026-06-24)
- Add `NameMap<K, V>` / `IdMap<K, V>` after call sites show stable access patterns. — ✅ (2026-06-24)
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
- `FallbackTracker` and `Logger` now return named vectors from their snapshot APIs
- `PluginRegistry` now returns named vectors from its plugin snapshot APIs
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

## 2026-11-09: DataCache and NameMap indexing

- Replaced `DataCache`'s private `std::unordered_map` index with `NameMap`, retaining the cache's URI-keyed behavior and locking boundary.
- Added `NameMap::findValue` for optional pointer lookup that counts reads without recording a failed access.
- Kept `NameMap` copy semantics by deep-copying entries and metadata. Updated its snapshot sampling to match the existing `std::vector` field contract.
- Fixed `ArtifactHashMap` duplicate `tryEmplace` behavior so existing values are not overwritten and constructor arguments are not evaluated for duplicate keys; made destruction safe after move by allowing `clear()` on a moved-from map.
- Preserved key-sorted `NameMap::keys()` and `values()` results. Added a monotonic access sequence to DataCache LRU ranking because millisecond timestamps can tie on immediate access.
- Added regression coverage for lookup accounting, zero capacity clamping, and replacement at capacity. MSVC 19.51 standalone tests: NameMap 52/52 and DataCache 18/18 passed.
- FileWatcher standalone 14/14 passed on rerun. Its first execution missed one timestamp-poll callback, so timing-based flakiness remains under observation.
- Full DataStandalone CTest was not run because only `ArtifactCoreDataCacheStandaloneTest` was built; the other six registered executables were absent from that build directory.

## 2026-11-09: DataTable column index migration

- Replaced the private `std::unordered_map<std::string, int>` column index with `NameMap`, preserving last-duplicate-header-wins lookup and DataTable's implicit copy behavior.
- Added a copy-independence contract test for column lookup/schema changes.
- Strengthened numeric text parsing to reject valid numeric prefixes followed by trailing text; the pre-existing regression test exposed the issue during the suite run.
- MSVC 19.51 `ArtifactCoreDataTableStandaloneTest`: 41/41 passed.

## 2026-11-09: DataSourceRegistry factory index migration

- Replaced the private `std::unordered_map` with `NameMap`; extension normalization, replacement semantics, and registered-extension listing remain intact.
- `open()` now uses one pointer lookup and returns null for a registered empty factory instead of throwing `std::bad_function_call`.
- Updated DataTable-consuming standalone module lists for the NameMap dependency. Fixed a pre-existing CsvDataSource test diagnostic that called a nonexistent `String::toStdString()` member.
- MSVC 19.51 standalone results: DataSource 11/11, CsvDataSource 12/12, and DataCache 18/18 passed.

## 2026-11-09: PluginRegistry storage migration

- Replaced the private `std::unordered_map` / `std::unordered_set` pair with `NameMap` / `ArtifactSet`; the existing mutex still guards every read and write.
- Added an isolated standalone CMake target around the existing plugin lifecycle contract suite so this module can be built without the full ArtifactCore application build.
- `ArtifactSet` iterates in insertion order, so `activePlugins()` now returns a stable activation order where the previous unordered set made order unspecified. Plugin lifecycle behavior remains covered by all seven existing contract tests.
- MSVC 19.51 `ArtifactCorePluginRegistryStandaloneTest`: 7/7 passed.

## 2026-11-09: ArtifactHashMap contract hardening and IdMap storage migration

- Fixed const iteration so const maps yield const entries, and iterator increment now traverses collision-chain nodes before advancing buckets.
- Added iterator erase with next-entry return, `reserve()` / `rehash()` capacity operations, nonzero bucket storage for a zero request, and reuse of moved-from maps.
- `tryEmplace()` now constructs mapped values directly in the node and avoids temporary mapped objects for duplicate keys.
- Replaced `IdMap`'s private `std::unordered_map` with `ArtifactHashMap`, preserving reserve/squeeze, diagnostics, copy independence, and the explicit `toStdUnorderedMap()` compatibility conversion.
- The IdMap debug snapshot now converts its bounded `NamedVector` samples to the existing `std::vector` snapshot field and no longer promises `noexcept` across that allocation.
- MSVC 19.51 verification passed across nine dependent standalone suites: HashMap 33, IdMap 53, NameMap 52, DataTable 41, DataSource 11, CsvDataSource 12, DataCache 18, FileWatcher 14, and PluginRegistry 7 contract cases (241 total).

## 2026-11-09: ProgressAggregator worker index migration

- Replaced the private `std::unordered_map<QString, WorkerEntry>` with `ArtifactHashMap` and a small `QString` hasher that delegates to Qt's `qHash`; the `QString` equality semantics, mutex boundary, registration replacement behavior, and snapshot `std::vector` API remain unchanged.
- Added module IFC wiring for `Core.ArtifactHashMap` and a contract case covering distinct ASCII and Unicode worker IDs.
- The full target build still stops on an unrelated existing `ScopeComputer.cpp` C2511 declaration mismatch. After reconfiguration, the ProgressAggregator interface object, implementation object, and contract-test source object each compiled successfully in isolation; the linked contract test could not run because the full `ArtifactCore` target did not finish.

## 2026-11-10: Lottie validation index migration

- Replaced the five validation-local `std::unordered_map` indexes in `LottieExporter` with `ArtifactHashMap`, reserving known document/asset/precomp layer counts before insertion. Parent traversal still uses lookup-by-index, asset and layer duplicate checks still use insert-result semantics, and cycle tracking retains default-inserted boolean behavior. Root layers skip constructing an unused visited map, avoiding its eager bucket allocation.
- Added explicit `Core.ArtifactHashMap` IFC and object dependency wiring for the exporter implementation.
- Added Lottie contract cases for duplicate asset IDs and cyclic nested precomp parent references. The exporter implementation and test source objects compile successfully in isolation. The full target remains blocked by the unrelated existing `ScopeComputer.cpp` C2511 mismatch, so these tests have not run yet.

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
