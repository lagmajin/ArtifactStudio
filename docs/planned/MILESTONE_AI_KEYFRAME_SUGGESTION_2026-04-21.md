# MILESTONE: AI Keyframe Suggestion

作成日: 2026-04-21
最終更新: 2026-08-15

## 目的

タイムライン上の既存キーフレームや動きのパターンから、AI が「次にこうしたら自然」という提案を返せるようにする。

このマイルストーンは自動置換ではなく、まず提案と比較を作る。  
既存のキーフレーム編集や easing 選択を壊さず、補助として自然に差し込むことを優先する。

---

## 入口

### 1. Timeline Surface
- 選択中のレイヤー
- 選択中のプロパティ
- 既存キーフレーム列
- keyframe marker 表示

### 2. Existing Easing UI
- `EasingLabWidget` を提案比較の可視化に流用する
- easing preset の比較表示をそのまま提案プレビューに使う
- 適用は既存の undo / redo 経路を通す

### 3. AI Context
- active composition
- selected layers
- frame range
- property path
- 最近の編集履歴

---

## Phase 1: Capture and Normalize

### Goal
提案に必要な入力を安定して集める。

### Tasks
- 既存キーフレーム列の抽出
- frame / value / interpolation / tangent 情報の正規化
- 単一レイヤー・単一プロパティから始める
- transform / opacity を先行対象にする

### Output
- `KeyframeSuggestionContext`
- `KeyframeSuggestionSample`
- `KeyframeSuggestionRequest`

---

## Phase 2: Suggestion Engine

### Goal
自然な動き候補を返せるようにする。

### Tasks
- 軌跡の単純化
- linear / ease / hold の候補提示
- easing preset の候補提示
- 必要なら時間軸の再サンプリング
- 提案は自動適用ではなく比較可能な形で返す

### Output
- `AIKeyframeGenerator`
- `KeyframeSuggestionResult`
- `KeyframeSuggestionCandidate`

### Related
- `docs/planned/MILESTONE_AI_ASSISTED_KEYFRAME_GENERATION_2026-04-11.md`

---

## Phase 3: Timeline Integration

### Goal
タイムライン上で提案を見せ、選べるようにする。

### Tasks
- `ArtifactTimelineTrackPainterView` から keyframe context を渡す
- 提案候補を keyframe lane と並べて表示する
- hover / selection / compare を壊さない
- 提案適用は既存の keyframe edit 経路を使う

### UI Behavior
- 元の keyframe と提案を見比べられる
- scrub で easing の形を確認できる
- apply 前に差分が分かる

### Related
- `Artifact/src/Widgets/Timeline/EasingLabWidget.cppm`
- `Artifact/src/Widgets/Timeline/ArtifactTimelineTrackPainterView.cpp`

---

## Phase 4: Advanced Suggestions

### Goal
複数条件からの提案に広げる。

### Tasks
- property ごとの学習
- 複数 keyframe の相互関係の考慮
- movement style の候補化
- ユーザーフィードバックを提案品質に反映する土台

---

## Completion Criteria

- 既存 keyframe から提案を返せる
- 提案と元の keyframe を比較できる
- 既存 undo / redo 経路で適用できる
- タイムライン上で邪魔にならない表示になっている
- 単一レイヤー・単一プロパティで実用に耐える

## Notes

- 最初は transform / opacity に限定する
- 完全自動化は狙わない
- まずは「提案の質」と「比較のしやすさ」を優先する

## 2026-07-25 実装監査

既存の EasingLabWidget、KeyPatternDialog、Timeline／Property 側の keyframe 編集・preview・Undo 経路は確認した。一方、`KeyframeSuggestionContext`／`KeyframeSuggestionSample`／`AIKeyframeGenerator`／`KeyframeSuggestionCandidate` の専用型、既存 keyframe の AI 解析、候補比較の timeline 表示、提案適用の専用導線は確認できない。したがって既存 easing／keyframe 基盤は利用可能だが、Phase 1〜4 と本マイルストーンの Completion Criteria は未実装・runtime未検証とする。

## 2026-08-15 現行コード監査

- `KeyframePatternGenerator::generateFromTrajectory()` は有限値検証と 2D trajectory の再サンプリングから `TrajectoryKeyframe` を生成し、`ArtifactTimelineWidget::applyTrajectoryToProperty()` は既存の keyframe／Undo 経路へ適用する。
- `KeyPatternDialog`／`EasingLabWidget` は候補生成・preview・適用の UI 基盤として存在する。
- ただし `KeyframeSuggestionContext`、候補比較 lane、選択レイヤーからの自動 trajectory 抽出、AI 品質評価・フィードバックの専用契約は確認できない。
- したがって trajectory pattern generation は実装済みだが、本マイルストーンの「AI suggestion」と timeline 比較 workflow は未接続。runtime 検証も未実施。

判定: **既存 keyframe／easing 基盤と trajectory 生成・Undo 適用は実装済み。AI suggestion 契約、候補比較 UI、自動抽出、runtime 検証は pending。**

## Update 2026-08-15

現行コードを追加確認した。`KeyframePatternGenerator::generateFromTrajectory()` は有限値検証と2D trajectoryの再サンプリングから `TrajectoryKeyframe` を生成し、`ArtifactTimelineWidget::applyTrajectoryToProperty()` が既存の keyframe／Undo 経路へ適用する。`KeyPatternDialog`／`EasingLabWidget` には候補生成・preview・適用のUI基盤がある。

一方、`KeyframeSuggestionContext` 等のAI専用契約、選択レイヤーからの自動trajectory抽出、候補比較lane、AI品質評価／feedbackは未確認。trajectory生成と既存適用は実装済み、AI suggestion workflowとruntime検証は pending とする。
