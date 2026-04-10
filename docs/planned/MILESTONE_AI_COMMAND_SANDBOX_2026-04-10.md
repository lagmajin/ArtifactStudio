# AI Command Sandbox / CLI Execution Milestone

This milestone defines the safe command-line execution layer that future AI
agents can use from inside ArtifactStudio.

## Goal

- Let AI execute command-line tools without reaching for a raw shell.
- Make execution explicit: program, argv list, working directory, timeout, and
  policy are all visible.
- Keep the sandbox narrow enough that command execution is auditable and
  predictable.
- Reuse the same command surface from prompt generation, MCP tool calls, and
  built-in tests.

## Scope

- `ArtifactCore/include/AI/CommandSandbox.ixx`
- `ArtifactCore/include/AI/AIPromptGenerator.ixx`
- `ArtifactCore/include/AI/ToolBridge.ixx`
- `ArtifactCore/include/AI/McpBridge.ixx`
- `Artifact/src/Test/ArtifactTestAIToolBridge.cppm`
- `Artifact/src/Test.cppm`

## Non-Goals

- Full terminal emulation.
- Interactive PTY sessions.
- Shell parsing for arbitrary command strings.
- Network sandboxing at the OS level.

## Proposed Model

- `CommandSandboxPolicy`
  - allowed program list
  - blocked program list
  - environment key allowlist
  - working directory
  - timeout
  - output size limit
  - shell-program gate
- `CommandSandboxResult`
  - allowed / executed / finished / timed out / exit code
  - stdout / stderr
  - normalized program and argv
  - policy or validation errors
- `CommandSandbox`
  - validate and execute non-shell command invocations
  - expose a dry-run path for AI planning
  - register itself as an AI describable tool

## Phase Plan

### Phase 1: Direct Execution Sandbox

- Validate program names against policy before execution.
- Execute the command without going through `cmd.exe` or `/bin/sh`.
- Capture stdout/stderr with a hard output cap.
- Return a structured result object for AI and MCP consumers.
- Add regression tests for policy rejection and a harmless allowed command.

### Phase 2: Policy Editing Surface

- Add a UI or settings entry for editing sandbox policy.
- Persist the allowlist and working-directory defaults.
- Make policy changes visible in diagnostics and prompt context.

### Phase 3: Workspace-Bound Developer Workflow

- Preconfigure repo-local commands such as build, test, and search helpers.
- Keep all command execution rooted to the project workspace unless
  explicitly overridden.

## Current Status

- Phase 1 is implemented in the core AI bridge.
- The sandbox is non-shell by default and only allows an explicit command
  allowlist.
- Built-in tests cover schema exposure, dry-run rejection, and one harmless
  allowed command.
