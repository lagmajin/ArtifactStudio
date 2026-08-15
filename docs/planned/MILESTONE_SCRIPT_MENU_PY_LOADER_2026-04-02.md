# Milestone: Script Menu / menu.py Loader (M-PY-2)

**最終更新:** 2026-08-15

## Static Audit (2026-07-25)

`ArtifactScriptMenu` は `scripts/menu.py` のパスを定義し、初回に scaffold とサンプルコメントを作成し、ファイルを開く導線を提供している。しかし、menu.py を PythonEngine でロードして QAction／submenu に変換する実装は確認できない。

- 実装済み: built-in Script menu、scripts root の決定、`menu.py` の雛形生成、hooks/macros の探索と再表示、Hook の有効・無効設定、Python macro/hook の実行、失敗時の表示。
- 未実装または未確認: `menu.addCommand(...)` 相当の Python API、separator／nested submenu、登録の idempotence、script command と built-in item の共通 merge、script source の metadata、menu.py の success/failure log、reload 時の動的 QAction 差し替え、broken source の disable、script loading の enable/disable 設定。
- `ArtifactPythonHookManager` の workspace API は個別関数登録の基盤になるが、menu registration API ではない。したがって hook/macro の実行可能性を menu.py loader の完了根拠にはできない。

## 現行コード監査 (2026-08-15)

`ArtifactScriptMenu` は `menu.py` のパス確保・雛形生成・ファイルを開く導線を持ち、hooks、macros、AE utility、CSX script は個別 QAction として再構築・実行できる。`ArtifactPythonHookManager` と `PythonEngine` には hook／macro／workspace API の実行基盤もある。

一方、`menu.py` 自体を PythonEngine でロードして `menu.addCommand(...)` 等を QAction／submenu に変換する registry は確認できない。separator／nested submenu、idempotent な再登録、built-in との merge、script 側 metadata、reload 時の動的差し替え、失敗時の診断と enable/disable 設定も未実装である。

判定: **既存 script／hook／macro の実行入口は部分実装。menu.py loader と動的 menu API は未実装。**

判定: Phase 1 の「雛形と入口」は部分的に存在するが、loader skeleton の実行、Phase 2〜4 は未達。次の最小作業は PythonEngine の実行環境と UI QAction の寿命を分離した menu registry を追加し、menu.py の登録失敗を既存診断へ流すこと。

## Goal
Build a script entry point that starts as a stable built-in menu and later grows into a Nuke-like `menu.py` system.

## Why
- The current `Script` menu gives us a clean UI entry now.
- A `menu.py` loader lets user scripts add commands without touching core code.
- The same path can later host custom tools, hook toggles, and workspace-specific actions.

## Scope
- Keep the built-in `Script` menu as the primary entry point.
- Load startup scripts from `scripts/menu.py` when present.
- Provide a small Python-side menu API for adding commands and submenus.
- Keep `ArtifactPythonHookManager` as the hook execution backend.
- Allow scripted commands to live alongside fixed menu items.

## Non-Goals
- Full Python plugin marketplace.
- Arbitrary UI object mutation from scripts.
- Replacing all built-in menus with scripts.

## Phases

### Phase 1: Loader Skeleton
- Read `scripts/menu.py` at startup.
- Add a safe import path for script menu helpers.
- Log script load success / failure clearly.

### Phase 2: Menu API
- Expose a minimal `menu.addCommand(...)` style API.
- Allow separators and nested submenus.
- Make menu registration idempotent.

### Phase 3: Built-in + Script Merge
- Merge built-in `Script` menu items with dynamically registered entries.
- Surface hook commands, utility actions, and workspace-specific tools.
- Keep the menu readable even if scripts add many items.

### Phase 4: Guardrails
- Add error reporting for broken script menus.
- Add enable/disable controls for script loading.
- Define a reload workflow for developers.

## First Targets
- `Artifact/src/Widgets/Menu/ArtifactScriptMenu.cppm`
- `Artifact/src/AppMain.cppm`
- `Artifact/src/Script/ArtifactPythonHookManager.cppm`
- `Artifact/include/Script/ArtifactPythonHookManager.ixx`

## Recommended Order
1. Add the loader skeleton.
2. Expose a tiny menu registration API.
3. Merge dynamic commands into the built-in `Script` menu.
4. Add reload / diagnostics support.
