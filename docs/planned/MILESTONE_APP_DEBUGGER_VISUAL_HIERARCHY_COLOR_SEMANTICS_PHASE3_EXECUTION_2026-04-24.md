# Phase 3 Execution: Failure Focus / Pin / Compare / Filter

> 2026-04-24 作成

## 目的

[`docs/planned/MILESTONE_APP_DEBUGGER_VISUAL_HIERARCHY_COLOR_SEMANTICS_2026-04-23.md`](X:/Dev/ArtifactStudio/docs/planned/MILESTONE_APP_DEBUGGER_VISUAL_HIERARCHY_COLOR_SEMANTICS_2026-04-23.md)
の Phase 3 を、異常時に自動で目が行き、重要項目を pin / compare しやすくするための実行メモ。

この段階では、画面を整えるだけでなく、怪しいところへ最短で到達できる導線を作る。

---

## 方針

1. 異常系を自動で前に出す
2. pin で重要項目を固定する
3. compare で前後差を追いやすくする
4. filter で情報量を減らす

---

## 現状の土台

- [`Artifact/src/Widgets/Diagnostics/AppDebuggerWidget.cppm`](X:/Dev/ArtifactStudio/Artifact/src/Widgets/Diagnostics/AppDebuggerWidget.cppm)
- [`Artifact/src/Widgets/Diagnostics/ArtifactDebugConsoleWidget.cppm`](X:/Dev/ArtifactStudio/Artifact/src/Widgets/Diagnostics/ArtifactDebugConsoleWidget.cppm)
- [`Artifact/src/Widgets/Diagnostics/ProfilerPanelWidget.cppm`](X:/Dev/ArtifactStudio/Artifact/src/Widgets/Diagnostics/ProfilerPanelWidget.cppm)
- [`Artifact/src/Widgets/Diagnostics/FrameDebugViewWidget.cppm`](X:/Dev/ArtifactStudio/Artifact/src/Widgets/Diagnostics/FrameDebugViewWidget.cppm)
- [`Artifact/src/Widgets/Render/ArtifactCompositionRenderController.cppm`](X:/Dev/ArtifactStudio/Artifact/src/Widgets/Render/ArtifactCompositionRenderController.cppm)
- [`Artifact/src/Playback/ArtifactPlaybackEngine.cppm`](X:/Dev/ArtifactStudio/Artifact/src/Playback/ArtifactPlaybackEngine.cppm)

---

## 実装タスク

### 1. 異常系の自動フォーカス

やること:

- failed pass を自動で先頭に寄せる
- missing resource / fallback path を目立つ位置へ送る
- stale cache / slow present のときは summary を強める
- 最後のエラーを見つけやすくする

### 2. Pin の固定導線

やること:

- current frame / capture / report を pin できるようにする
- pin した対象が view の切り替えで消えないようにする
- pin 状態を summary に短く表示する

### 3. Compare の第一切り口

やること:

- current / previous / next をすぐ切り替えられるようにする
- A/B compare を summary で見せる
- diff UI は複雑にしすぎず、まずは要点だけ出す

### 4. Filter の整理

やること:

- trace / state / frame / diagnostics を絞り込めるようにする
- error / warning / info を切り替えやすくする
- filter 状態を見失わないようにする

---

## レイアウト方針

- 異常時は warning banner を上部に固定する
- pin 状態は summary の近くに置く
- compare 対象は中央の主表示か直下に出す
- filter は左ペインに置き、常時見えるようにする

補助ルール:

- 異常を検出したら、詳細より先に要点を出す
- pin したものを最優先で残す
- compare の対象は 1 画面で分かる程度に絞る

---

## 表示ルール

- `failed pass` は最上位の注意対象
- `missing resource` は原因候補として強調
- `fallback` は「今は代替経路」の意味で明示
- `pin` は固定対象として badge 化する
- `compare` は current / previous / next を同じ言い方で統一する

---

## 実装メモ

- `AppDebuggerWidget` に pin / compare / filter の操作面を寄せる
- `FrameDebugViewWidget` は compare 対象を読めるようにする
- `ArtifactDebugConsoleWidget` は failure summary の置き場にする
- `ProfilerPanelWidget` は warning summary の補助にする
- filter は UI の奥に隠しすぎない

---

## Work Tickets

### P3-T1 Failure Banner

対象:

- `Artifact/src/Widgets/Diagnostics/AppDebuggerWidget.cppm`
- `Artifact/src/Widgets/Diagnostics/ArtifactDebugConsoleWidget.cppm`

内容:

- failed pass / missing resource / fallback を上部 banner にまとめる
- いま危ないものを先に見せる
- 最後のエラーを追いやすくする

完了条件:

- 異常時に最初に見る場所が決まる

### P3-T2 Pin State Model

対象:

- `Artifact/src/Widgets/Diagnostics/AppDebuggerWidget.cppm`
- `Artifact/src/Widgets/Diagnostics/FrameDebugViewWidget.cppm`

内容:

- capture / frame / report の pin を扱う
- pin 状態を summary に出す
- view 切り替えで消えないようにする

完了条件:

- 重要対象を固定できる

### P3-T3 Compare Quick Switch

対象:

- `Artifact/src/Widgets/Diagnostics/FrameDebugViewWidget.cppm`
- `Artifact/src/Playback/ArtifactPlaybackEngine.cppm`

内容:

- current / previous / next の切り替えを速くする
- A/B compare の対象を分かりやすくする
- first cut は text summary でよい

完了条件:

- 比較対象をすぐ追える

### P3-T4 Filter Chips

対象:

- `Artifact/src/Widgets/Diagnostics/AppDebuggerWidget.cppm`
- `Artifact/src/Widgets/Diagnostics/ArtifactDebugConsoleWidget.cppm`

内容:

- trace / state / frame / diagnostics の filter を置く
- error / warning / info を切り替えられるようにする
- filter 状態を迷わず読めるようにする

完了条件:

- 情報量を絞りやすい

---

## 完了条件

- 異常系が自動で目に入る
- pin した対象を見失いにくい
- compare の切り替えが分かりやすい
- filter で必要な情報だけに寄せられる

---

## 変更しないこと

- diagnostics の元データ
- render / playback の意味
- 既存の message / report 形式
- QtCSS と新しい global signal/slot

---

## リスク

- 自動フォーカスを強くしすぎると、通常時の静けさが崩れる
- pin / compare / filter を増やしすぎると操作が重くなる
- banner が増えると逆に常時警告画面に見える

---

## 次の Phase への橋渡し

Phase 3 が終わると、Phase 4 で export / report / share をまとめるときに、再現操作と視線誘導をそのまま bundle 化しやすくなる。

---

## 関連文書

- [`docs/planned/MILESTONE_APP_DEBUGGER_VISUAL_HIERARCHY_COLOR_SEMANTICS_2026-04-23.md`](X:/Dev/ArtifactStudio/docs/planned/MILESTONE_APP_DEBUGGER_VISUAL_HIERARCHY_COLOR_SEMANTICS_2026-04-23.md)
- [`docs/planned/MILESTONE_APP_DEBUGGER_VISUAL_HIERARCHY_COLOR_SEMANTICS_PHASE1_EXECUTION_2026-04-24.md`](X:/Dev/ArtifactStudio/docs/planned/MILESTONE_APP_DEBUGGER_VISUAL_HIERARCHY_COLOR_SEMANTICS_PHASE1_EXECUTION_2026-04-24.md)
- [`docs/planned/MILESTONE_APP_DEBUGGER_VISUAL_HIERARCHY_COLOR_SEMANTICS_PHASE2_EXECUTION_2026-04-24.md`](X:/Dev/ArtifactStudio/docs/planned/MILESTONE_APP_DEBUGGER_VISUAL_HIERARCHY_COLOR_SEMANTICS_PHASE2_EXECUTION_2026-04-24.md)
- [`docs/planned/MILESTONE_APP_INTERNAL_DEBUGGER_2026-04-17.md`](X:/Dev/ArtifactStudio/docs/planned/MILESTONE_APP_INTERNAL_DEBUGGER_2026-04-17.md)
