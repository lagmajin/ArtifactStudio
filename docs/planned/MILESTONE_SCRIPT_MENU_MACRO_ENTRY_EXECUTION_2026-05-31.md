# M-FE-6a Script Menu / Macro Entry Execution

## Static Audit (2026-07-25)

`ArtifactScriptMenu` は scripts workspace の scaffold、`menu.py` / hooks / macros の探索、macro の QAction 化、Python ファイル実行、失敗時の QMessageBox、メニュー再表示時の再構築まで実装されている。`ae_utility_pack` の固定項目も同じ macro 実行経路に接続されている。

- 実装済み: Script menu を canonical entry として使う基礎、macro folder の reload、script/macro source の個別 QAction、macro 実行時の `artifact_macro_name` / `artifact_macro_file` 注入、Python API の workspace 操作登録。
- 未確認: `id/name/description/targetScope/commandFamily/actionSequenceReference/presetReference/iconName` の共通 descriptor、built-in/script/macro/batch の source category registry、menu と command palette／button launcher の共有 registry、disabled reason と target scope の表示。
- 未実装または未確認: 固定 action bundle としての 5 個以上の built-in macro、record/playback、Undo 経由の冪等 replay、macro metadata の保存、壊れた script source の disable 状態、safe reload の診断イベント。現状の macro はファイル名を表示して Python ファイルを直接実行する入口に留まる。

判定: Script menu / macro execution の基礎は部分実装だが、M-FE-6a の execution slice 1〜3 と成功条件は未達。次は既存 `ArtifactScriptMenu` の直接実行経路を壊さず、共通 command descriptor／registry の最小形を先に定義するのが安全である。

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

### Phase 4: Built-in Macro Starter Pack

- built-in macro を 5 から 10 個だけ先に固定する
- まずは layer utility / parenting / effect apply / preset apply に限定する
- user recording や arbitrary scripting より、repeatable utility command を優先する

### Phase 5: Button Launcher Surface

- macro / script / built-in utility を同じ command family として並べる
- `Script menu` とは別に、よく使う command を常駐ボタンで呼べる surface を用意する
- KBar / MoBar のような「即実行ランチャー」として扱い、authoring system 化は後回しにする

## Execution Slice: 1 to 3

今回の first implementation は、次の 3 段で切る。

### 1. Macro Command Shape

目的:
`macro` を録画再生ではなく、安全な固定 action bundle として定義する。

最低フィールド:

- `id`
- `name`
- `description`
- `targetScope`
- `commandFamily`
- `actionSequenceReference`
- `presetReference` (optional)
- `iconName` (optional)

ルール:

- macro 自体は UI object を直接触らない
- 既存 command / service / action path の再利用を前提にする
- 新しい中央 signal/slot 配線は増やさない
- command が失敗したときは途中状態を黙って飲み込まない

想定 targetScope:

- `selection.layer`
- `selection.effect`
- `composition.active`
- `project.active`

Done:

- script / macro / batch の metadata shape が並べて読める
- 1 つの macro descriptor から menu item と launcher button の両方を組み立てられる
- source category と target scope が UI 側で表示できる

### 2. Built-in Macro Starter Pack

目的:
KBar / MoBar 的な価値が出る最小セットを、script runtime 完成前でも先に使えるようにする。

最初の候補:

- `Align Selected Layers To Comp Center`
- `Move Anchor Point To Layer Center`
- `Create Null And Parent Selection`
- `Apply Favorite Effect To Selection`
- `Apply Preset To Selection`
- `Reveal Selected Layer Transform`
- `Reset Selected Layer Transform`

方針:

- 最初は built-in macro として実装し、後で script/macro registry に寄せる
- 各 macro は 1 つの明確な workflow shortener に絞る
- timeline / inspector / property editor のどこから呼んでも同じ command path を使う

Done:

- 代表 macro が 5 個以上登録される
- selection 不足や対象不一致時の disabled / error messaging が揃う
- menu と command palette の両方から同じ結果で呼べる

### 3. Button Launcher Phase 1

目的:
よく使う macro を、menu を掘らずに 1 click で実行できる surface に載せる。

UI 方針:

- `Button Launcher` は command browser ではなく quick-run surface として扱う
- まずは fixed section のみ
- `Built-in`
- `Macros`
- `Scripts`

初期仕様:

- 押すと即実行
- hover で description / target scope を読める
- source badge を出す
- disabled reason を読める
- icon は既存 Studio icon 方針に従う

非 goal:

- drag-and-drop customization
- user-authored button layout persistence
- full marketplace / package browser

Done:

- 5 から 10 個の頻出 command をボタンで即実行できる
- menu item と button が同じ registry を読む
- 将来 command palette を足しても command family が分裂しない

## Suggested First Built-in Macros

- `Align Selected Layers To Comp Center`
- `Move Anchor Point To Layer Center`
- `Create Null And Parent Selection`
- `Apply Favorite Effect To Selection`
- `Apply Preset To Selection`

## Suggested Order

1. `Macro Command Shape`
2. `Built-in Macro Starter Pack`
3. `Button Launcher Phase 1`
4. `Safe Reload Loop`

## Recommended First Connections

- `ArtifactScriptMenu`
- `ArtifactMainWindow`
- `ArtifactPythonHookManager`
- command palette entry
- macro registry / descriptor provider
- button launcher widget

## Success Criteria

- ユーザー拡張の入口が `Script menu` に集約される
- macro と script が別実装でも、UI では同じ command family として読める
- custom workflow を docs ではなく app 内導線から再利用できる

## Related

- `docs/planned/MILESTONE_SCRIPT_MENU_PY_LOADER_2026-04-02.md`
- `docs/planned/MILESTONE_EXTENDSCRIPT_STYLE_SCRIPT_RUNTIME_2026-04-06.md`
- `docs/planned/MILESTONE_FEATURE_EXPANSION_2026-03-25.md`
