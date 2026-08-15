# MILESTONE: AI Color Grading Suggestion

作成日: 2026-04-21
最終更新: 2026-08-15

## 目的

シーンやフレームの色状態を AI が解析し、カラーグレーディングの提案を返せるようにする。

このマイルストーンは、自動で色を塗り替えるものではない。  
まずは「今の絵に対して何が妥当か」を提案し、既存の color science / LUT / grading UI に自然に載せることを優先する。

---

## 入口

### 1. Color Science Surface
- `ArtifactColorSciencePanel`
- LUT の選択 / 強度 / HDR 設定
- input / working / output color space

### 2. Grading Surface
- `ArtifactColorGradingEngine`
- existing grading node stack
- LUT / preset / manual controls

### 3. AI Context
- active composition
- selected layer / clip
- frame range
- image or frame sequence sample
- current color settings

---

## Phase 1: Scene Analysis

### Goal
提案の元になる色統計を安定して取る。

### Tasks
- 単一フレームの色統計抽出
- 露出、コントラスト、彩度、色温度の推定
- 暗部 / 中間調 / ハイライトの偏りを要約する
- 単一ショットから始める

### Output
- `AIColorAnalysisContext`
- `AIColorAnalysisResult`
- `ColorGradingSuggestionRequest`

### Related
- `Artifact/src/Color/ArtifactColorGradingEngine.cppm`

---

## Phase 2: Suggestion Generation

### Goal
color grading の候補を返せるようにする。

### Tasks
- LUT ベースの提案
- preset ベースの提案
- lift / gamma / gain / curves / hue-sat-lum の候補生成
- auto apply ではなく、比較できる候補として返す

### Output
- `AIColorAnalyzer`
- `ColorGradingSuggester`
- `ColorGradingSuggestionResult`
- `ColorGradingSuggestionCandidate`

### Related
- `docs/planned/MILESTONE_AI_COLOR_GRADING_SUGGESTION_2026-04-11.md`

---

## Phase 3: UI Integration

### Goal
提案を見て選べるようにする。

### Tasks
- `ArtifactColorSciencePanel` に提案入口を置く
- LUT / preset / manual adjustment の比較を出す
- 適用前プレビューを作る
- 既存の LUT 強度や color space 設定と衝突しないようにする

### UI Behavior
- scene analysis の要約が見える
- 候補ごとの見た目差分が分かる
- apply 前に元設定へ戻せる

### Related
- `Artifact/src/Widgets/Color/ArtifactColorSciencePanel.cppm`
- `Artifact/src/Color/ArtifactColorSettings.cppm`

---

## Phase 4: Extended Assistance

### Goal
ショット単位の補助から、作品全体の色補助へ広げる。

### Tasks
- video sequence 単位の分析
- 複数ショットの色差を揃える提案
- user feedback による候補の優先度調整
- style transfer / look matching の実験枠

---

## Completion Criteria

- 単一ショットの色解析ができる
- 適切な LUT / preset / parameter 候補を返せる
- UI 上で候補を比較できる
- 既存の color science / grading 経路を壊さずに適用できる
- manual adjustment の邪魔をしない

## Notes

- 最初は video sequence 全体ではなく単一ショットから始める
- 破壊的な自動変更は後回しにする
- `ArtifactColorSciencePanel` を提案の主入口にする

## 2026-07-25 実装監査

入口として想定された ColorSciencePanel／ColorGradingEngine／LUT 管理は存在するが、`AIColorAnalysisContext`／`AIColorAnalysisResult`／`ColorGradingSuggestionRequest`／`ColorGradingSuggestionCandidate` の専用型と、単一ショットの統計抽出から候補生成までの実装は確認できない。提案比較UI、適用前プレビュー、元設定へ戻す導線、sequence分析やfeedback学習も未確認である。したがって Phase 1〜4 は未実装・runtime未検証とする。

## 2026-08-15 現行コード監査

- `ArtifactColorGradingEngine` の `ColorGradingSuggestion`、`suggestGrading()`、`applySuggestion()` と、Color Science／LUT／preset の既存経路を確認した。
- これはルール／統計ベースの grading suggestion 基盤であり、AIColorAnalysisContext 等の専用 AI 契約、単一フレーム解析から候補比較までの独立 workflow は確認できない。
- 適用前 preview、before／after 比較、Undo を含む提案 UI、selected layer／frame context の自動接続は未確認。既存の color settings／LUT UI は再利用可能な土台として扱う。

判定: **ColorGradingEngine の suggestion／apply 基盤と既存 Color Science UI は実装済み。AI 解析契約、候補比較 UI、context 接続、runtime 検証は pending。**

## Update 2026-08-15

現行コードを追加確認した。`ArtifactColorGradingEngine` の `ColorGradingSuggestion`、`suggestGrading()`、`applySuggestion()` と、Color Science／LUT／preset の既存編集経路が存在する。これはルール／統計ベースの suggestion／apply 基盤であり、既存の色設定を再利用できる。

一方、`AIColorAnalysisContext` 等の専用AI契約、単一フレーム統計から候補比較へ至る独立workflow、適用前preview、before／after比較、提案Undo、selected layer／frame contextの自動接続は未確認。AI解析、候補比較UI、runtime受入れは pending とする。

## Update 2026-08-15

`ArtifactColorGradingEngine` に `AIColorAnalysisResult` と `analyzeSamples()` を追加した。有限な色サンプルから平均輝度、輝度レンジ、平均彩度、赤青差による色温度偏り、暗部／ハイライト比率を read-only に収集し、既存の `suggestGrading()` へ渡せる Phase 1 の解析契約を確保した。空入力・非有限値は安全に無効結果として扱う。

AI context、LUT／preset を含む候補比較、適用前 preview／Undo、UI接続、runtime受入れは引き続き pending。
