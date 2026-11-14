# Shader Graph Lab

This source-only prototype tests the boundary between **dynamic graph data** and
**trusted shader implementations**. It is independent of `ArtifactCore` and is
not wired into the application.

Run the example from the repository root:

```powershell
python -B experiments/ShaderGraphLab/dynamic_graph_prototype.py
```

The prototype reads `registry.json` and `dynamic_add.json`, validates their
stable operation/socket IDs and types, checks single-writer inputs and cycles,
topologically orders the active graph, then emits a deterministic HLSL body and
layout-free semantic SHA-256 over active node IDs, operation/version/implementation
identity, parameters, links, and output. A change in the selected trusted
implementation changes the semantic hash even when the document graph is
otherwise identical. The example computes `(0.25 + 0.5) * 2` as a
shader candidate. `dynamic_subtract.json` is a focused `0.75 - 0.25` graph for
the closed scalar Subtract operation. `dynamic_divide.json` exercises the
legacy positive denominator floor with a negative divisor, which distinguishes
it from ordinary signed division. `dynamic_vector4_divide.json` exercises the
same source rule across zero, negative epsilon, positive epsilon, and a negative
large denominator. The candidate generator writes all four lowering outputs
and updates their input, registry, and candidate digests in the manifest. The
JSON results record the actual HLSL strings and semantic hashes.

Run the dependency-free local graph editor against an existing document:

```powershell
python -B experiments/ShaderGraphLab/local_graph_lab.py --document path/to/graph.json
```

Then open `http://127.0.0.1:8765/` in a browser. The server binds to loopback
only. The editor exposes the registry palette, typed socket connections,
parameter edits, node movement, graph-output selection, deletion, undo/redo,
and explicit save through the prototype's document APIs. Vector4 parameters
use four separate numeric controls for component-level editing, including when
the value is exposed at graph scope. Parameter editors retain focus after a
commit so Vector4 lanes can be tabbed through without losing the editing
position. Newly added nodes are selected and placed into a free workspace slot;
the selection outline and inspector identify the active node. Socket linking can
start from either an input or output. The opposite-side candidates are highlighted
when types match and the edge would not form a cycle; an input with one writer is
shown as a valid rewire candidate, while multi-writer inputs remain blocked.
Rejected candidates remain visible but dimmed. SVG links are drawn from
the socket button bounds using per-draw node/socket indexes; malformed endpoints are skipped while the validator
reports them. Socket hint updates build one adjacency map and per-input link
index plus node/operation indexes, then cache repeated node-pair cycle queries
for that render pass. Rewire cycle checks ignore the single edge being replaced. Input
rows label required/optional state and highlight required
inputs without a writer. Adjacent edits to the same graph parameter within 0.8 seconds share
one Undo step; another parameter or structural action starts a separate step.
Connecting to an input with one existing writer atomically replaces that edge
after validating types and cycles, as one undoable graph edit. Multi-writer
inputs must first be repaired by disconnecting their individual links.
Each connected input link has a Disconnect action, including inputs with multiple
writers or a malformed source endpoint. It removes one indexed edge only when
the document hash still matches the displayed revision, and the edit is undoable.
Nodes whose operation/version is unavailable remain visible at their saved
position with the original identity. A uniquely identified unknown node can be
selected, moved, or deleted; deletion removes its incident links and parameter
bindings. Operation replacement stays disabled until an explicit migration
contract exists, so unknown pin semantics are never guessed.
Portable copy rejects a selection containing unavailable operations before it
reports success; the clipboard format requires a complete registry contract.
Invalid or duplicate node IDs remain visible as read-only cards and are excluded
from selection; malformed layout values keep the node visible but disable drag
so a failed layout edit cannot strand a multi-node move.
Node-scoped and link-scoped validation diagnostics are buttons that select and
focus a uniquely identified endpoint node from their JSON Pointer path; graph-wide
and ambiguous diagnostics remain readable but non-actionable. Identical action, validation, and lowering
diagnostics are shown once.
Deleting the active graph-output node opens an explicit output picker populated
only from surviving nodes with unique IDs. The selection is applied together
with deletion as one undoable action; cancel leaves the document unchanged.
The Graph Parameters inspector lists every
node binding with a direct Detach action. Expose opens a type-filtered chooser to
share an existing graph parameter or create one with a generated stable ID and
editable label. Without `--document`,
the example graph opens as an unsaved session and the Save button is disabled.
The editor tracks dirty state across edits and undo/redo, warns before leaving a
modified page, and offers `Download JSON` as an explicit recovery/export path;
starting the server with `--document` enables atomic Save to that path.
This UI is an R&D workbench; it is not integrated with Artifact or ArtifactCore.
Repairable documents with malformed graph collections keep rendering the
workbench: invalid nodes/links roots fall back to an empty canvas, and malformed
graph-parameter collections, records, values, and bindings are shown as inert
diagnostics instead of crashing the inspector. These repair states still need
dedicated runtime interaction coverage.
Action failures now keep the last valid editor state when the server cannot
return a complete state envelope, and show the action diagnostic message in the
inspector and an `aria-live` status region.
The manifest validator also exercises the UI session action path in-process:
it builds a graph, checks validation/lowering, rejects a bad socket without
changing graph/history, and verifies undo/redo plus save. An ephemeral loopback
HTTP smoke covers the page route, state route, failed socket action, layout edit,
and undo response. Browser rendering and real pointer/keyboard interaction
remain unverified.
The canvas supports 40–200% zoom from the header, middle-button pan, and
Space+left-drag pan. Node movement and wire endpoints are scaled back into
document coordinates. `Focus` centers the selected node (or graph output), and
`Frame All` fits the current node bounds to the viewport. These interactions are
source-reviewed but not browser-tested. The Graph Nodes index filters by node ID
or operation label and selects/focuses a result without changing the document.
Shift-clicking node titles toggles multi-selection; dragging a selected title
moves the group through one atomic layout-batch command and one Undo step.
Delete Selected removes the selected nodes, their links, and graph-parameter
bindings in one command. Deleting the active graph output requires choosing an
output from a surviving node first.
`Arrange` places the selection (or all nodes when nothing is selected) into
left-to-right data-flow columns. Nodes in each column retain a stable order
based on their prior y/x position and ID; nodes left in a cycle by topological
ordering share a final column so malformed graphs can still be visually
reorganized. When a feasible horizontal placement exists, existing links that
cross the selection boundary also constrain the block so incoming nodes remain
to the left and outgoing nodes remain to the right. The operation changes only
layout and is one undoable batch.
For fine cleanup, `Align left` / `Align top` use the minimum selected x/y
coordinate, while horizontal/vertical distribution preserves the first and last
positions and spaces intermediate nodes evenly. Distribution requires three
selected nodes; all four layout actions are single undoable batches. Repeated
node-drag, canvas-scroll, and window-resize events coalesce to at most one
scheduled wire redraw per animation frame; node positions still track every
pointer event.
With the canvas focused, Ctrl/Cmd+D duplicates, Delete removes, Ctrl/Cmd+A
selects all, Ctrl/Cmd+Z undoes, Shift+Ctrl/Cmd+Z or Ctrl/Cmd+Y redoes, F focuses
the selection, and Escape cancels a pending socket connection. Text controls
keep their normal editing shortcuts.
Ctrl/Cmd+C writes a version-2 subgraph JSON envelope to the system clipboard;
Ctrl/Cmd+V reads that envelope and pastes it with fresh node and graph-parameter
IDs. Copied layouts are normalized around their top-left node position; paste
places their bounds near the visible canvas center while preserving all relative
node positions. Version-1 clipboard envelopes remain readable. The same-session clipboard is the fallback when browser clipboard access is
unavailable. The payload contains selected nodes, only internal links, exposed
parameter values/bindings for those nodes, and relevant registry contract locks.
Paste validates each internal link against the registered socket types, existing
writers, and cycles in an isolated copy of the incoming subgraph; malformed links
already present in the target document are left untouched. Clipboard parameter
records must have unique IDs and unique node-parameter bindings, and each binding
must match the cloned node's registered parameter type and shared value before it
is remapped to the fresh node and graph-parameter IDs.
Duplicate creates fresh node IDs with a 32-unit layout offset, copies selected
node values and internal links, and adds copied nodes to the same exposed graph
parameter bindings when their records and types are valid. Malformed graph
parameter records and bindings remain available for repair and are skipped when
creating duplicate bindings. Copy likewise exports only valid exposed parameter
records bound to the selection; unrelated malformed records do not block a copy,
and a malformed record bound to the selection is rejected. Deleting nodes removes their matching bindings;
valid graph parameters with no bindings are removed, while malformed parameter
records are retained. Expose and Detach skip unrelated malformed records while
still rejecting duplicate IDs, already-exposed values, and malformed target
bindings; joining an existing parameter also validates every current binding's
node, registered parameter type, and value consistency. Detach removes a valid graph parameter when its last binding is
removed, while retaining a malformed parameter record for repair. Links crossing
the selection boundary are not copied; the original output remains selected. The
group is one undoable edit.
Session edits now prepare the post-edit document and a detached next-history
list before committing either; failed command composition cannot truncate redo
history. Undo/redo cursors and the parameter-merge key advance only after the
snapshot operation succeeds, and save clears the merge key only after writing.
HTTP actions have per-operation required/allowed field sets, reject unknown or
missing fields, validate JSON depth/value constraints at ingress, and require
`delete.replacementOutput` to be a two-string pair.
The browser queues action requests in invocation order and applies each response
before sending the next action, preventing an older state snapshot from
overwriting a later edit in the editor. If an action response is lost or cannot
be decoded, the editor reloads `/api/state` before releasing the queue. State
responses are checked for the document, validation diagnostics, compilation
warnings or rejection diagnostics, history, and dirty/save metadata before they
replace the current UI state.
Action responses additionally require a boolean result; diagnostic arrays are
shape-checked before they are formatted into the status line.

## Contract under evaluation

- A version-1 graph document stores node instances, operation ID/version, typed
  links, parameters, layout, and a selected output. Exposing the first graph
  parameter upgrades the editing document to schema version 2, which stores
  typed graph-level values and one or more node-parameter bindings. Existing v1
  documents remain valid without migration.
- Persistence uses strict JSON parsing: duplicate object keys and NaN/Infinity
  tokens are rejected. Canonical serialization sorts object keys but preserves
  array order, detects cyclic Python references, and caps nested JSON at 256
  container levels. The same canonical text feeds command snapshot hashes.
  `load_document` retains unknown JSON fields for repair after validating the
  bounded JSON depth; `save_document` writes a canonical file through a
  same-directory temporary file and atomic replace. The local editor tracks the
  exact loaded file-byte hash and refuses Save if the file changed or vanished,
  so it does not silently overwrite external edits or recreate a deleted file.
  After a conflict, `Reload File` becomes available; it requires explicit
  confirmation, first shows a read-only summary of changed sections, node IDs,
  graph parameters, links, and output, then clears the in-memory Undo history
  and loads the current disk version. Changed node IDs can be selected to fetch
  and inspect the local/disk node JSON side by side. `Download JSON` remains
  available first to preserve the local edits. If the disk bytes have reverted
  to the originally loaded version, comparison clears the stale conflict state
  and lets the user retry Save without discarding local changes.
  A
  save/load/edit/resave probe keeps unknown node and link payloads intact while
  the graph remains repairable and non-compilable.
  The active compiler still rejects unknown schema fields rather than guessing
  their semantics.
- The current typed value set is `Float` and `Vector4`. The registry includes
  explicit `Float → Vector4` RGB replication with alpha fixed to one and
  `Vector4 → Float` X extraction. Links require exact type equality; these
  conversion nodes must be present in the graph, so the compiler never inserts
  an implicit cast.
- A declarative registry supplies UI-facing labels, categories, stable sockets,
  parameter defaults, and a closed implementation ID. The same record can drive
  node palette entries and socket rendering without treating display labels as
  identity. `build_palette_model` emits deterministic palette entries keyed by
  `operationId@semanticVersion` and carries stable socket IDs beside labels;
  duplicate labels/categories remain separate entries.
- `instantiate_node` creates node instance data from that operation record and
  materializes declared parameter defaults while assigning an independent
  stable node ID and editable layout coordinates. `add_node` appends the selected
  registry operation to a copied document without changing existing links or
  output; it rejects duplicate node IDs. Required inputs begin as `incomplete`
  warnings, and the node becomes compilable after valid links and explicit output
  selection.
- `replace_node_operation` changes a node only when input/output socket IDs,
  value types, requiredness, and parameter IDs/types remain compatible. Existing
  links, node identity, layout, and parameter values stay in place; an
  unmaterialized default cannot silently change. Incompatible replacements fail
  atomically. On sealed documents, the old registry pin is removed only after
  its last use and the replacement contract is added to the lock. The editor
  uses a dedicated dialog to expose only signature-compatible palette entries,
  also excluding candidates whose changed default would be rejected when the
  current parameter is not materialized. Links and parameter values remain;
  operation semantics change in one undoable command.
- `edit_node_parameters` resolves the node's pinned operation definition,
  merges the full update set, canonicalizes Float32 values and all four
  Vector4F32 components as finite binary32, and replaces the parameter object
  only after all fields pass validation. Node and parameter IDs are checked at
  API ingress against the stable identifier grammar. The manifest covers tie-to-even
  rounding, subnormal values, signed-zero normalization, stable hashes after
  repeated edits, and undo/redo restoration for Vector4 constants.
  Unknown fields or invalid values leave the caller's graph unchanged.
- `expose_node_parameter` publishes a Float32 or Vector4F32 node parameter and
  can bind additional compatible node parameters to the same stable graph ID.
  `edit_graph_parameter` updates the shared value atomically and synchronizes the
  stored node values; it rejects duplicate bindings to the same node parameter
  across records before changing any values. Direct edits to exposed node
  parameters are rejected.
  `unexpose_node_parameter` detaches one binding while copying the current shared
  value back to that node, and removes the graph parameter record after its final
  binding is detached. The local editor exposes this as an undoable node action.
  Graph parameter labels are presentation metadata; active IDs, types, values,
  and bindings participate in semantic identity. Bindings on unreachable nodes
  are excluded from the active semantic projection.
- `edit_node_layout` accepts only finite binary32 `x`/`y` coordinates. Layout
  changes stay outside semantic hashing, while extra layout keys and invalid
  values are rejected at the editing boundary.
- `edit_node_layout_batch` validates distinct node edits on a private copy and
  applies all coordinates only if every edit is valid. The manifest confirms a
  multi-node move keeps semantic hash/HLSL unchanged and a later invalid
  coordinate leaves the input document untouched.
- `connect_nodes` edits a detached document copy and adds an edge only after
  checking source/output and target/input direction, exact value type, single
  writer, existing link integrity, and cycle formation. Invalid attempts leave
  the caller's document unchanged. Links involving unknown opaque operations
  are retained, while the compiler still refuses to lower those graphs.
- `connect_nodes_batch` applies several such edits against a private working
  document created once for the full batch. Each edge uses the same
  endpoint/type/single-writer/cycle checks; if any later edge fails, the caller's
  original document remains unchanged. Existing node/socket indexes, writer set,
  adjacency, and for multi-edge batches on an acyclic graph deterministic
  topological ranks are built once and updated as edges are accepted.
  Forward-ranked edges
  need no reachability walk; backward-ranked edges still run cycle search and
  refresh ranks. Cyclic existing graphs use cycle search for every edge. The
  private JSON snapshot is not reserialized or copied per edge. `rewire_input`
  validates directly on its already-detached copy instead of snapshotting it a
  second time.
- `add_node_with_connections` composes registry-backed node creation, a batch of
  typed links, and explicit output selection into one detached document result.
  Wrap that result in one `DocumentCommand` for a single undo/redo step; any
  failed link or output validation leaves the input document unchanged. The
  manifest also groups that add/connect/output command with a following parameter
  edit and verifies exact grouped undo/redo plus semantic hash change.
- `disconnect_nodes` removes exactly one addressed edge and preserves all
  remaining links; a missing or ambiguous edge is rejected without mutation.
- `disconnect_nodes_batch` preflights the entire requested edge list against the
  original document using a one-pass exact-edge index, then removes only
  distinct, uniquely matched links. Missing, ambiguous, or duplicated requests
  fail before applying any removal.
- `set_graph_output` validates an output pin and switches the active root.
  `delete_node` removes the node and every incident link; deleting the selected
  output requires the caller to name a valid replacement explicitly. This also
  works for opaque operation nodes: their selected output is repairable, and an
  explicit replacement is required before the node and incident links are
  removed.
- `create_document_command` captures immutable before/after payloads and hashes;
  `apply_document_command` restores them for undo/redo only when the current
  document matches the expected history state. The manifest also edits a known
  node beside opaque nodes/links and confirms undo/redo restores the full payload
  while the document remains repairable, including save/load between the edit,
  undo, and redo. This JSON snapshot prototype checks history semantics, not the
  application's `UndoManager` integration.
- Undo/redo and command grouping re-parse both snapshots as canonical strict
  document objects and verify each SHA-256 before restoring or composing them.
  Malformed history records fail with a stable command diagnostic.
- The local editor retains at most 100 undo commands and 32 MiB of canonical
  before/after snapshot payloads. When the budget is reached, oldest commands
  are pruned; an edit whose own undo record exceeds 32 MiB is rejected before
  either the document or history is committed. Runtime object overhead is not
  included in this payload-byte budget. Registry and graph JSON files are read
  through a 16 MiB byte cap, and saves above the same encoded-byte limit are
  rejected; individual undo snapshots are also capped so every retained
  snapshot remains parseable under the same JSON text limit. HTTP action
  requests are limited to 1 MiB and JSON state/action responses to 64 MiB; the
  response size is checked incrementally before constructing the output body.
- `apply_document_batch` runs multiple edits against a private working copy, so
  a later failure discards earlier edits. `create_document_transaction` makes
  that batch one undoable command. Adjacent commands can also be grouped, but
  only when their before/after hashes form a contiguous history chain.
- Registry loading is strict: unknown operation fields, duplicate `(operationId, semanticVersion)` registrations, duplicate socket IDs, non-string implementation/type tags, boolean schema versions, and inline executable fields are rejected before palette construction. Malformed type tags return structured contract diagnostics rather than leaking Python membership `TypeError`s. File loading validates JSON value depth before iterative forbidden-code-field scanning; parser recursion failures also map to a structured depth diagnostic. Editing APIs use a type-safe registry lookup for serialized node identities so malformed versions remain unavailable-operation diagnostics instead of leaking hash errors. The manifest currently covers unknown fields, duplicate registrations/sockets, and inline executable fields; the new scalar-type, depth, and edit-boundary guards are not yet represented by validator fixtures.
- Registry data cannot contain Python, HLSL, source, expression, or code fields.
  Only implementation IDs present in the prototype's trusted backend table can
  execute. A future new algorithm requires reviewed native implementation and
  versioned semantics; registering arbitrary code is outside this boundary.
- Compilation validates the whole known-schema document before output-rooted
  pruning. Unknown operations/versions fail closed during compilation. The
  persistence probe separately deep-copies JSON without interpreting or
  dropping fields, then changes a known node's parameter/layout while keeping
  an unknown node's extra fields and a link to its unknown socket byte-value
  equivalent as JSON. This does not prove a production editor's visual or
  interactive handling of unknown sockets. Unreachable valid nodes remain
  document data and are excluded from active lowering with a warning.
- Layout is excluded from the active semantic hash. Canonical Float and
  Vector4F32 parameters use finite IEEE-754 binary32 and normalize signed zero.
- `validate_document_for_editing` separates a graph that is valid and
  compilable, an incomplete graph with unconnected required inputs (warnings),
  structurally invalid graphs (errors), and a repairable graph containing
  fields/schema/operation versions this prototype cannot interpret. If the exact
  pinned operation definition becomes available again, the unchanged document
  validates and lowers again; a new or changed semantic version still needs an
  explicit identity migration or a domain-specific transform. Repairable
  documents retain their opaque JSON data and can still be edited and saved;
  compilation remains unavailable until unsupported data is handled explicitly.
  It returns sorted diagnostics and aggregates independent node/link/root
  issues, including graph-parameter shape, binding, type, and value mismatches,
  in one pass, and only reports a semantic hash when compilation
  succeeds. An unavailable operation selected as output is repairable when its
  node exists; a truly absent output node remains a structural error.
- The registry compatibility probe hashes operation ID/version, trusted
  implementation ID, typed socket IDs/requiredness, and typed parameter
  defaults. Same-version parameter-default and input-requiredness drift both
  invalidate the saved contract lock; palette label/category changes remain
  presentation-only. Removed operations produce diagnostics, while a new version
  can coexist with the old one.
  `seal_document` stores locks only for operation versions referenced by the
  document, and lowering checks them before graph validation. A new version
  can coexist with the old one while old graphs still resolve their pinned
  operation; changes to unrelated registry entries do not alter the lock. This is a
  prototype persistence shape. The migration probe permits an explicit version
  pin change only when both versions are registered and their operation
  semantics match exactly apart from the version and operation ID. The probe
  rejects changes to implementation, parameter defaults, and input requiredness,
  and does not migrate across operation IDs. Missing pinned versions are
  rejected instead of silently upgraded; semantic changes require an operation
  specific transform that this prototype does not implement.

## Evidence limits

The parameter-block reflection contract is schema v1. Its logical binding ID is stable, while the physical register is compiler-assigned. A CPU-only resolver rejects duplicate logical IDs, unsupported schema versions or resource kinds, missing reflected blocks, and collisions in the same HLSL register class; equal indices across distinct classes such as `b0` and `t0` remain valid. This is synthetic metadata validation, not actual compiler reflection.

`adapt_to_resident_generic_abi` provides a second source candidate for Artifact's current `g_P0..g_P15` prelude. The target identity is `artifact.resident-generic-params.scalar16.v1`. Those are sixteen scalar `float` lanes: Float32 consumes one lane, while Vector4F32 consumes four consecutive lanes and lowers to `float4(g_Pn, ..., g_Pn+3)`. Parameters receive lanes in stable parameter-ID order, the total is capped at sixteen scalar lanes, and overflow fails closed. `pack_resident_generic_parameter_values` canonicalizes the candidate's typed values as finite binary32, verifies the value hash against the compile result, requires the binding list to be in stable ID order, writes the matching lane order, and zero-fills unused lanes. It returns the explicit lane count and 64-byte parameter-array extent, separate from the larger `ResidentGenericParams` host structure. Missing/extra values and reordered bindings fail closed. The adapter emits no cbuffer or physical register declaration. A source check compares the adapter capacity against `ArtifactRenderLayerPipeline.cpp`'s HLSL prelude and C++ `p[16]` storage. Source audit also found that the existing resident apply loop copies every `node.parameters` entry into `p[i]` without a local `i < 16` guard; this prototype does not change or protect that production call site. Any integration needs a caller-side capacity check before writing. The target-specific structure hash remains stable across value edits. This is a CPU-side contract probe only; it is not connected to a renderer and does not prove compiler or GPU compatibility.

The prototype supports `Float` and `Vector4` sockets with eleven closed implementations: `float.constant`, `vector4.constant`, scalar `float.add`/`float.subtract`/`float.multiply`, scalar `float.divide.positive_floor_1e5`, component-wise `vector4.add`/`vector4.multiply`, `vector4.divide.positive_floor_1e5`, explicit `float.replicate.rgb_alpha_one`, and explicit `vector4.extract_x`. Scalar subtraction computes `a - b` without clamping. Both division candidates reproduce legacy `max(b, 1e-5)` behavior, including mapping negative divisors to the positive floor; they are deliberately named to distinguish them from conventional division. The Vector4 divide fixture uses four denominator cases and matches each input lane to the source-bound binary32 formula golden. Vector4 arithmetic treats all four components as raw numeric lanes, including alpha; it does not apply color or alpha policy. Active graph parameters lower into a typed HLSL `cbuffer` with references, plus separate semantic, shader-structure, and parameter-value hashes and binding metadata. The block has a stable logical binding ID; its physical register is left unassigned until compiler reflection, avoiding a hard-coded slot that could collide with an existing shader interface. The CPU layout packs Float32 into 4-byte slots without crossing 16-byte registers and starts Vector4F32 at a 16-byte boundary, following [Microsoft's HLSL packing rules](https://learn.microsoft.com/en-us/windows/win32/direct3dhlsl/dx-graphics-hlsl-packing-rules). A mixed fixture checks offsets `0`, `4`, and `16` and a 32-byte block. Synthetic reflection fixtures check slot resolution and duplicate-slot rejection; they do not replace real compiler reflection. Editing a parameter value keeps the HLSL candidate and structure hash stable. This remains text and metadata evidence: no HLSL compiler, GPU resource binding, renderer, backend parity, production persistence adapter, or Blender compatibility is exercised. The local UI's edit/undo/save routes use the Python prototype contracts and do not establish the production API or authorize promotion into `ArtifactCore`.

Rejected documents return a JSON envelope with `status: rejected` and one or more diagnostics containing `code`, `severity`, `path`, and `message`. The manifest validator checks the registry, graph, and saved example output hashes against regeneration, then exercises semantic invariance, opaque retention/editing, registry contract compatibility, and eleven rejection probes. Those cases pin stable diagnostic codes, severity, and JSON Pointer paths. Other structured errors still need broader fixture coverage before promotion. These checks validate the Python prototype contract only; they are not a C++ test or renderer verification.
