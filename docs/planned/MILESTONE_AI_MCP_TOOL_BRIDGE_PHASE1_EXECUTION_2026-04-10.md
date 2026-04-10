# AI MCP Tool Bridge - Phase 1 Execution

> 2026-04-10

## Purpose

Stabilize the internal AI tool schema so that local AI, cloud AI, and the future MCP transport can all rely on the same source of truth.

This phase focuses on the foundation that already exists in:

- `ArtifactCore::DescriptionRegistry`
- `ArtifactCore::IDescribable`
- `ArtifactCore::AIToolExecutor`
- `ArtifactCore::AIContext`
- `ArtifactCore::AIPromptGenerator`

## Why This Phase Comes First

- Tool calling is only useful if tool names and arguments stay stable.
- MCP transport should not invent a second schema.
- AI context snapshots should be serializable before any external bridge is added.

## Scope

- Normalize registry-backed tool metadata.
- Keep `class`, `method`, and `arguments` as the common internal tool-call shape.
- Make `AIContext` the portable snapshot for tool execution and preview.
- Keep prompt generation and MCP capability export aligned.

## Out Of Scope

- External MCP server process.
- Authentication or permission policy.
- Tool execution UX like confirmation, dry-run, or audit trail.
- Reworking every describable object in one pass.

## Execution Steps

### 1. Tool Schema Contract

- Make the registry output the same tool shape for prompts, previews, and runtime execution.
- Confirm that every tool entry has a stable component name, method name, description, return type, and parameter list.
- Ensure missing metadata degrades safely instead of producing malformed schema.

### 2. Context Snapshot Contract

- Keep `AIContext::toJson()` and `AIContext::fromJson()` symmetrical for the fields used by AI/tool workflows.
- Preserve the fields needed for tool previews and MCP request forwarding.
- Avoid adding transport-specific state into the context model.

### 3. Internal Bridge Alignment

- Keep `ToolBridge` as the single entry point for local tool parsing and execution.
- Keep `McpBridge` as a thin wrapper over the same tool schema and executor.
- Ensure `AIClient` and `ArtifactAICloudWidget` both consume the same schema source.

### 4. Validation Pass

- Add sanity checks for unknown tools, missing method names, and empty argument lists where applicable.
- Make failure cases explicit and easy to surface in logs or UI previews.

## Definition Of Done

- Tool schema is generated from one registry-backed path.
- Local AI and MCP preview show the same available tool set.
- Tool calls can be parsed, routed, and traced without special-case schema copies.
- `AIContext` can be serialized and rehydrated for bridge requests.

## Suggested Next Slice

If this phase is accepted as the active milestone slice, the next implementation target should be:

1. tighten schema validation in `ArtifactCore::ToolBridge`
2. normalize `ArtifactCore::McpBridge` request handling
3. add a small regression test around tool schema generation

