# Phase 2 Execution: Layout / Navigation / Inspector Priority

> 2026-04-24 作成

## 目的

[`docs/planned/MILESTONE_APP_DEBUGGER_VISUAL_HIERARCHY_COLOR_SEMANTICS_2026-04-23.md`](X:/Dev/ArtifactStudio/docs/planned/MILESTONE_APP_DEBUGGER_VISUAL_HIERARCHY_COLOR_SEMANTICS_2026-04-23.md)
の Phase 2 を、実際に「どこから読むか」を決めるための実行メモ。

この段階では色よりも、レイアウトと navigation の順序を整える。

---

## 方針

1. 左から右へ読む順を決める
2. 1 画面 1 主役を守る
3. 詳細は右側へ寄せる
4. navigation と inspector を同じ強さにしない

---

## 現状の土台

- [`Artifact/src/Widgets/Diagnostics/AppDebuggerWidget.cppm`](X:/Dev/ArtifactStudio/Artifact/src/Widgets/Diagnostics/AppDebuggerWidget.cppm)
- [`Artifact/src/Widgets/Diagnostics/ArtifactDebugConsoleWidget.cppm`](X:/Dev/ArtifactStudio/Artifact/src/Widgets/Diagnostics/ArtifactDebugConsoleWidget.cppm)
- [`Artifact/src/Widgets/Diagnostics/ProfilerPanelWidget.cppm`](X:/Dev/ArtifactStudio/Artifact/src/Widgets/Diagnostics/ProfilerPanelWidget.cppm)
- [`Artifact/src/Widgets/Diagnostics/FrameDebugViewWidget.cppm`](X:/Dev/ArtifactStudio/Artifact/src/Widgets/Diagnostics/FrameDebugViewWidget.cppm)
- [`Artifact/src/Widgets/Render/ArtifactCompositionRenderController.cppm`](X:/Dev/ArtifactStudio/Artifact/src/Widgets/Render/ArtifactCompositionRenderController.cppm)

---

## 実装タスク

### 1. 左ペインの役割を固定する

やること:

- `Trace / State / Frame / Diagnostics` の入口を左にまとめる
- filter / pin / search の入口も左に寄せる
- 左ペインは「探す場所」に固定する

### 2. 中央の主表示を 1 つに絞る

やること:

- 現在の対象に応じて中央の主表示を切り替える
- 常に複数の主役を並べない
- state か frame か trace のどれかを強く出す

### 3. 右ペインを詳細 inspector にする

やること:

- raw text / expanded details / copyable report を右へ寄せる
- 読み込みの順番を「要点 -> 詳細」にする
- 長文は折りたたんでから開く

### 4. 現在対象の追跡を明確にする

やること:

- current project / composition / layer / frame の追跡を上部に出す
- 選択対象の切り替わりを見失わないようにする
- 選択が変わったら、表示領域も自然に追従させる

---

## レイアウト方針

- Left: navigation / filter / pinned views
- Center: current main surface
- Right: inspector / raw detail / copy actions
- Top: state summary
- Bottom: secondary trace or status line

補助ルール:

- 左と右で同じ密度にしない
- detail は中央に置かない
- 重要な warning は上部 summary にも出す

---

## 表示ルール

- `Trace` は時系列の一覧
- `State` は current snapshot
- `Frame` は 1 フレーム要約
- `Diagnostics` は warning / error / report

これらをタブではなく「見え方の階層」として扱う。

---

## 実装メモ

- `AppDebuggerWidget` のタブ順を navigation-first にする
- `FrameDebugViewWidget` は中央主表示候補にする
- `ArtifactDebugConsoleWidget` は詳細と report の置き場にする
- `ProfilerPanelWidget` は state summary の補助にする
- collapse 状態を記憶して、毎回広がりすぎないようにする

---

## Work Tickets

### P2-T1 Navigation Column

対象:

- `Artifact/src/Widgets/Diagnostics/AppDebuggerWidget.cppm`

内容:

- 左ペインに navigation / search / pin をまとめる
- 先に探す場所を固定する
- 現在のカテゴリが分かるようにする

完了条件:

- どこから読むかが迷いにくい

### P2-T2 Main Surface Selection

対象:

- `Artifact/src/Widgets/Diagnostics/AppDebuggerWidget.cppm`
- `Artifact/src/Widgets/Diagnostics/FrameDebugViewWidget.cppm`

内容:

- central surface を 1 主役に絞る
- state / frame / trace を必要に応じて切り替える
- 情報の重なりを減らす

完了条件:

- 中央に何を見るかが明確になる

### P2-T3 Detail Inspector

対象:

- `Artifact/src/Widgets/Diagnostics/ArtifactDebugConsoleWidget.cppm`
- `Artifact/src/Widgets/Diagnostics/ProfilerPanelWidget.cppm`

内容:

- right side に詳細 inspector を寄せる
- raw text と copyable report を分ける
- 要点と詳細の順を守る

完了条件:

- 詳細が主表示を邪魔しない

### P2-T4 Current Target Tracking

対象:

- `Artifact/src/Widgets/Diagnostics/AppDebuggerWidget.cppm`
- `Artifact/src/Widgets/Diagnostics/FrameDebugViewWidget.cppm`
- `Artifact/src/Widgets/Render/ArtifactCompositionRenderController.cppm`

内容:

- current project / composition / layer / frame を追いやすくする
- 選択変更時の追従を見失わないようにする
- 上部 summary と view を同期させる

完了条件:

- いま何を見ているかが追える

---

## 完了条件

- 左から右へ読む順が自然になる
- 中央の主役が 1 つに絞れる
- 右側に詳細が逃げる
- current target を見失いにくい

---

## 変更しないこと

- diagnostics の情報量そのもの
- render / playback の意味
- 既存の signal/slot の増加
- QtCSS の追加

---

## リスク

- レイアウトを分けすぎると、逆に散らかる
- 中央の主役を頻繁に切り替えると読みにくくなる
- 詳細を右に寄せすぎると、見落としやすくなる

---

## 次の Phase への橋渡し

Phase 2 が終わると、Phase 3 で異常時の自動フォーカスや、重要項目の pin / compare を進めやすくなる。

---

## 関連文書

- [`docs/planned/MILESTONE_APP_DEBUGGER_VISUAL_HIERARCHY_COLOR_SEMANTICS_2026-04-23.md`](X:/Dev/ArtifactStudio/docs/planned/MILESTONE_APP_DEBUGGER_VISUAL_HIERARCHY_COLOR_SEMANTICS_2026-04-23.md)
- [`docs/planned/MILESTONE_APP_DEBUGGER_VISUAL_HIERARCHY_COLOR_SEMANTICS_PHASE1_EXECUTION_2026-04-24.md`](X:/Dev/ArtifactStudio/docs/planned/MILESTONE_APP_DEBUGGER_VISUAL_HIERARCHY_COLOR_SEMANTICS_PHASE1_EXECUTION_2026-04-24.md)
- [`docs/planned/MILESTONE_APP_INTERNAL_DEBUGGER_2026-04-17.md`](X:/Dev/ArtifactStudio/docs/planned/MILESTONE_APP_INTERNAL_DEBUGGER_2026-04-17.md)
