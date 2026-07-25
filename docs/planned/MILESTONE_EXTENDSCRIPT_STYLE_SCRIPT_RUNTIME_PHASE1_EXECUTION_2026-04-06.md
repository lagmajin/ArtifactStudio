# Milestone: ExtendScript-Style Script Runtime Phase 1 Execution

## Static Audit (2026-07-25)

Phase 1 の要求に対して、`ScriptRuntime` が host-facing facade として実装され、`ScriptHostSnapshot`（app/version/project/composition/working directory/selection）、文字列・ファイル実行、logger、success/error/位置情報を提供している。`ScriptContext` は変数表と AST cache を持ち、`BuiltinScriptVM` は context 付き評価と timeout 引数、エラー状態を公開する。

- 1-1〜1-4 の成果物に相当する API は存在し、単発実行と診断形状は確認できる。
- ただし本確認は静的ソース監査であり、実行結果、ファイル失敗時の UI 表示、timeout が実際に VM 評価を停止すること、host snapshot が実アプリ状態へ接続されることまでは検証していない。
- `app/project/composition/selection` は `ScriptHostSnapshot` の値として扱われるため、ExtendScript 風の可変オブジェクト API や Phase 2 の console／履歴は対象外のまま。

判定: Phase 1 は API 形状として完了相当。ただし runtime integration と実行時検証は未確認で、親の M-PY-3 Phase 2 以降は未達。

> 2026-04-06

## Goal

Implement the smallest useful skeleton for `M-PY-3 ExtendScript-Style Script Runtime`.

This phase is about proving the host runtime shape, not building a full scripting ecosystem yet.

## Phase 1 Tasks

### 1-1. Host Runtime Facade

Define a small runtime facade that future script engines can target.

Targets:

- `ArtifactCore::ScriptRuntime` or an equivalent host-facing wrapper
- lifecycle hooks for init / shutdown
- error reporting and logging plumbing

Acceptable outcome:

- the host can create, keep alive, and destroy a runtime instance
- scripts can report errors in a single consistent way

### 1-2. Host Object Surface

Expose a minimal read-only object model for scripts.

Targets:

- `app`
- `project`
- `composition`
- `selection`

Minimum properties:

- project name
- active composition identity
- selected object count or selection list

Acceptable outcome:

- a script can inspect current app state without mutating anything yet

### 1-3. Execution Entry Points

Provide the first execution paths for the runtime.

Targets:

- execute from string
- execute from file
- basic console-style single line execution

Acceptable outcome:

- a script can be run from an in-memory string or a file path
- execution success / failure is reported clearly

### 1-4. Diagnostics / Error Shape

Normalize runtime diagnostics so the UI can consume them later.

Targets:

- error message
- error position or location when available
- success / failure status

Acceptable outcome:

- the console / UI layer can show a useful error message without guessing

## Suggested Implementation Order

1. Host runtime facade
2. Host object surface
3. Execution entry points
4. Diagnostics / error shape

## Initial Files to Inspect

- `ArtifactCore/include/Script/Script.ixx`
- `ArtifactCore/include/Script/Engine/Context/ScriptContext.ixx`
- `ArtifactCore/include/Script/Engine/BuiltinScriptVM.ixx`
- `ArtifactCore/include/Script/Python/PythonEngine.ixx`
- `Artifact/src/Script/ArtifactPythonHookManager.cppm`
- `Artifact/src/Widgets/Menu/ArtifactScriptMenu.cppm`

## Notes

- Keep this phase deliberately small.
- Do not try to recreate Node.js in Phase 1.
- Do not expand beyond host access, execution, and diagnostics.

