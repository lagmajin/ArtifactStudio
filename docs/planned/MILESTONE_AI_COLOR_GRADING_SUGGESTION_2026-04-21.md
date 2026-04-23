# MILESTONE: AI Color Grading Suggestion

作成日: 2026-04-21

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
