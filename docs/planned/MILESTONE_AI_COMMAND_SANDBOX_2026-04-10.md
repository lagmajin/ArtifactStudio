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

### 2026-07-25 実装監査

`CommandSandbox` の非 shell 実行、明示 allowlist、出力上限、構造化結果、dry-run、schema 登録とテスト入口は記載どおり確認できる。これは OS／CLI command の sandbox であり、`WorkspaceAutomation` の CommandIR dry-run／execute とは別境界である。Phase 2 の allowlist／working-directory／timeout を編集・永続化する UI と、Phase 3 の workspace-bound command preset／root enforcement は確認できないため、Phase 1 完了、Phase 2〜3 未完了と整理する。

---

## Next Execution Slice

Phase 2 では、policy を編集できることより先に、編集対象の境界を固める。

### Phase 2A の着手点

1. `CommandSandboxPolicy` の編集対象を allowlist / working directory / timeout に絞る
2. blocked program list と shell-program gate は diagnostics 専用の扱いに寄せる
3. UI では policy の現在値を見せるだけにして、危険な自動推論を避ける
4. prompt context には command surface の状態だけを短く出す

### Phase 2 完了条件

- policy editing surface で何を変えられるかが明確
- allowlist と workspace default が持続化される
- diagnostics と prompt context に policy 状態が反映される

### Phase 3A の着手点

1. repo-local command preset を workspace-bound の前提でまとめる
2. build / test / search のような典型コマンドを先に固定する
3. 明示オーバーライドなしでは project workspace から外れないようにする
4. workspace root と command root の違いを表示で追えるようにする

### Phase 3 完了条件

- workspace に結びついた developer workflow が見える
- command execution が repo-local 前提で使いやすい
- 明示オーバーライドの責務が曖昧にならない
