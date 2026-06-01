# M-FE-6a Script Menu / Macro Entry Execution

`Batch / Macro / Script Entry` を、単なる将来構想ではなく、ユーザー拡張の正式入口として再開するための execution メモ。

## Why Now

- `menu.py` loader と ExtendScript-style runtime の計画はすでにある
- ただしユーザー視点では、`Script menu`, `macro`, `batch`, `command palette` が別々に見えやすい
- まずは「どこからユーザー拡張を入れるか」を固定しないと、custom workflow が散らばる

## Current Ground Truth

- `M-PY-2 Script Menu / menu.py Loader` がある
- `M-PY-3 ExtendScript-Style Script Runtime` がある
- feature expansion 側にも `macro entry / script hook entry / preset-driven batch jobs` がある

## Core Decision

この段階では `runtime` を広げるより先に、`入口の責務` を固定する。

- `Script menu`: 人が選んで実行する拡張の入口
- `Macro entry`: 繰り返し操作を 1 アクションとして再実行する入口
- `Batch entry`: project / asset / render queue 単位のまとめ処理入口

## First Slice

### 1. Script Menu as Canonical Entry

- `Script` menu を、ユーザー拡張の最上位入口として固定する
- `scripts/menu.py` はここに command を追加するだけに絞る

### 2. Macro Shape Lock

- name
- description
- target scope
- action sequence reference
- optional preset reference

macro を「録画機能」ではなく、まずは再利用 action bundle の shape として定義する

### 3. Command Source Categories

- built-in utility
- script-provided command
- macro command
- batch command

UI 上で source が分かるようにする。

### 4. Safe Reload Loop

- menu reload
- error reporting
- disable broken script source

開発中の custom command が壊れても app 全体を巻き込まない入口にする。

## Non-Goals

- フル plugin marketplace
- unrestricted Python mutation
- 複雑な record-and-playback macro の完成

## Phase Proposal

### Phase 1: Entry Grammar

- script / macro / batch の役割を固定する
- menu item metadata を最小 shape で揃える

### Phase 2: Script Menu Merge

- built-in item と `menu.py` item を同じ surface で見せる
- source badge を持たせる

### Phase 3: Macro Entry

- 固定 action bundle を menu / command palette から呼べるようにする
- preset reference と組み合わせられる形を先に作る

## Recommended First Connections

- `ArtifactScriptMenu`
- `ArtifactMainWindow`
- `ArtifactPythonHookManager`
- command palette entry

## Success Criteria

- ユーザー拡張の入口が `Script menu` に集約される
- macro と script が別実装でも、UI では同じ command family として読める
- custom workflow を docs ではなく app 内導線から再利用できる

## Related

- `docs/planned/MILESTONE_SCRIPT_MENU_PY_LOADER_2026-04-02.md`
- `docs/planned/MILESTONE_EXTENDSCRIPT_STYLE_SCRIPT_RUNTIME_2026-04-06.md`
- `docs/planned/MILESTONE_FEATURE_EXPANSION_2026-03-25.md`
