# Phase 1 Execution: Visual Hierarchy / Color Semantics

> 2026-04-24 作成

## 目的

[`docs/planned/MILESTONE_APP_DEBUGGER_VISUAL_HIERARCHY_COLOR_SEMANTICS_2026-04-23.md`](X:/Dev/ArtifactStudio/docs/planned/MILESTONE_APP_DEBUGGER_VISUAL_HIERARCHY_COLOR_SEMANTICS_2026-04-23.md)
の Phase 1 を、まずは「どこを見る画面か」が一目で分かる状態に寄せるための実行メモ。

この段階では機能追加よりも、App Debugger の見出し、サマリ、警告の出し方を整える。

---

## 方針

1. まずは情報階層を固定する
2. 色の意味を少数に絞る
3. 正常時と異常時の見え方を分ける
4. 詳細は折りたたみ、要点を先に出す

---

## 現状の土台

- [`Artifact/src/Widgets/Diagnostics/AppDebuggerWidget.cppm`](X:/Dev/ArtifactStudio/Artifact/src/Widgets/Diagnostics/AppDebuggerWidget.cppm)
- [`Artifact/src/Widgets/Diagnostics/ArtifactDebugConsoleWidget.cppm`](X:/Dev/ArtifactStudio/Artifact/src/Widgets/Diagnostics/ArtifactDebugConsoleWidget.cppm)
- [`Artifact/src/Widgets/Diagnostics/ProfilerPanelWidget.cppm`](X:/Dev/ArtifactStudio/Artifact/src/Widgets/Diagnostics/ProfilerPanelWidget.cppm)
- [`Artifact/src/Widgets/Diagnostics/FrameDebugViewWidget.cppm`](X:/Dev/ArtifactStudio/Artifact/src/Widgets/Diagnostics/FrameDebugViewWidget.cppm)
- [`ArtifactCore/include/Frame/FrameDebug.ixx`](X:/Dev/ArtifactStudio/ArtifactCore/include/Frame/FrameDebug.ixx)

---

## 実装タスク

### 1. 上部サマリを固定する

候補:

- current project
- current composition
- current frame
- playback state
- backend / render path

やること:

- 画面冒頭に短い summary 行を置く
- いま見ている対象が何かを先に示す
- 長い詳細は下に回す

### 2. 色セマンティクスを固定する

候補:

- neutral
- info
- warning
- error
- success

やること:

- same meaning, same color に寄せる
- badge tone を既存 theme token で統一する
- `overlay` / `present` / `failed pass` の見分けを明確にする

### 3. セクションの優先度を揃える

やること:

- `Trace` は時系列
- `State` は現在値
- `Frame` は 1 フレームの要約
- `Diagnostics` は警告 / エラー

それぞれを同じ密度で並べず、役割ごとに強さを変える。

### 4. 異常系を前景化する

やること:

- failed pass を先に見せる
- missing resource を badge で示す
- fallback path を text で隠さない
- stale cache / slow present を warning として固定表示する

---

## 実装順

1. `Artifact/src/Widgets/Diagnostics/AppDebuggerWidget.cppm`
2. `Artifact/src/Widgets/Diagnostics/ArtifactDebugConsoleWidget.cppm`
3. `Artifact/src/Widgets/Diagnostics/ProfilerPanelWidget.cppm`
4. `Artifact/src/Widgets/Diagnostics/FrameDebugViewWidget.cppm`

---

## Work Tickets

### P1-T1 Header Summary

対象:

- `Artifact/src/Widgets/Diagnostics/AppDebuggerWidget.cppm`

内容:

- current state summary を上部に出す
- 重要情報の順序を固定する
- 最初に見る場所を 1 箇所にする

完了条件:

- 画面の役割が冒頭で分かる

### P1-T2 Badge Tone Policy

対象:

- `Artifact/src/Widgets/Diagnostics/AppDebuggerWidget.cppm`
- `Artifact/src/Widgets/Diagnostics/ArtifactDebugConsoleWidget.cppm`
- `Artifact/src/Widgets/Diagnostics/ProfilerPanelWidget.cppm`

内容:

- warning / error / info / success の色を揃える
- `overlay` / `present` / `pass` / `resource` の tone を固定する
- same meaning, same color を徹底する

完了条件:

- 同じ種類の情報が同じ見た目で読める

### P1-T3 Section Priority

対象:

- `Artifact/src/Widgets/Diagnostics/ArtifactDebugConsoleWidget.cppm`
- `Artifact/src/Widgets/Diagnostics/FrameDebugViewWidget.cppm`

内容:

- Trace / State / Frame / Diagnostics の優先度を整理する
- 詳細表示を後ろに下げる
- 正常時は静か、異常時は目立つに寄せる

完了条件:

- どこを見るべきかが分かりやすくなる

### P1-T4 Failure Foregrounding

対象:

- `Artifact/src/Widgets/Diagnostics/FrameDebugViewWidget.cppm`
- `Artifact/src/Widgets/Diagnostics/ArtifactDebugConsoleWidget.cppm`

内容:

- failed pass / missing resource / fallback path を前に出す
- stale cache / slow present を warning として扱う
- 隠れやすい異常を埋もれさせない

完了条件:

- 異常系が一目で分かる

---

## 完了条件

- App Debugger で「先に見る場所」が分かる
- 色の意味が少数に絞られる
- 正常時と異常時の見え方が違う
- `overlay` / `present` / `frame` / `resource` が読み分けやすい

---

## 変更しないこと

- diagnostics の中身そのものを大きく変えない
- 新しい global signal/slot を増やさない
- QtCSS を追加しない
- 既存の debug 情報を重複させない

---

## リスク

- 色を増やしすぎると意味がぼける
- 階層を強くしすぎると詳細が見つけにくくなる
- 既存 UI との整合が崩れると逆に読みにくくなる

---

## 次の Phase への橋渡し

Phase 1 が終わると、Phase 2 で navigation / layout / inspector の分割を詰めやすくなる。

---

## 関連文書

- [`docs/planned/MILESTONE_APP_DEBUGGER_VISUAL_HIERARCHY_COLOR_SEMANTICS_2026-04-23.md`](X:/Dev/ArtifactStudio/docs/planned/MILESTONE_APP_DEBUGGER_VISUAL_HIERARCHY_COLOR_SEMANTICS_2026-04-23.md)
- [`docs/planned/MILESTONE_APP_INTERNAL_DEBUGGER_2026-04-17.md`](X:/Dev/ArtifactStudio/docs/planned/MILESTONE_APP_INTERNAL_DEBUGGER_2026-04-17.md)
- [`docs/planned/MILESTONES_BACKLOG.md`](X:/Dev/ArtifactStudio/docs/planned/MILESTONES_BACKLOG.md)
