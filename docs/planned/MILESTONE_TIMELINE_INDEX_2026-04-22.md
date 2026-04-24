# Timeline Milestone Index

> 2026-04-22 作成

タイムライン系のマイルストーンは数が増えてきたので、役割ごとに棚卸しした入口をここにまとめる。

目的は「どれが古いのか」「どれが今の本筋か」「どれが補助線か」を一目で分けること。

---

## まず見るもの

### 1. 上位計画

- [MILESTONE_TIMELINE_FEATURE_IMPLEMENTATION_2026-04-03.md](/x:/Dev/ArtifactStudio/docs/planned/MILESTONE_TIMELINE_FEATURE_IMPLEMENTATION_2026-04-03.md)
- 右ペイン、keyframe、search、visual language、owner-draw を束ねる親計画

### 2. 今の本筋

- [MILESTONE_TIMELINE_KEYFRAME_EDITING_2026-03-27.md](/x:/Dev/ArtifactStudio/docs/planned/MILESTONE_TIMELINE_KEYFRAME_EDITING_2026-03-27.md)
- [MILESTONE_TIMELINE_TRANSFORM_KEYFRAME_EDITING_2026-04-12.md](/x:/Dev/ArtifactStudio/Artifact/docs/MILESTONE_TIMELINE_TRANSFORM_KEYFRAME_EDITING_2026-04-12.md)
- [MILESTONE_TIMELINE_SEARCH_KEYFRAME_INTEGRATION_2026-03-28.md](/x:/Dev/ArtifactStudio/docs/planned/MILESTONE_TIMELINE_SEARCH_KEYFRAME_INTEGRATION_2026-03-28.md)

### 3. 表示と入力の土台

- [MILESTONE_TIMELINE_RIGHT_PANE_FULL_OWNER_DRAW_2026-04-08.md](/x:/Dev/ArtifactStudio/docs/planned/MILESTONE_TIMELINE_RIGHT_PANE_FULL_OWNER_DRAW_2026-04-08.md)
- [MILESTONE_TIMELINE_VISUAL_LANGUAGE_2026-03-31.md](/x:/Dev/ArtifactStudio/docs/planned/MILESTONE_TIMELINE_VISUAL_LANGUAGE_2026-03-31.md)
- [MILESTONE_DAW_STYLE_INPUT_SURFACE_2026-04-08.md](/x:/Dev/ArtifactStudio/docs/planned/MILESTONE_DAW_STYLE_INPUT_SURFACE_2026-04-08.md)
- [MILESTONE_TIMELINE_SCRUBBAR_FRAME_CACHE_OVERLAY_2026-04-10.md](/x:/Dev/ArtifactStudio/docs/planned/MILESTONE_TIMELINE_SCRUBBAR_FRAME_CACHE_OVERLAY_2026-04-10.md)

### 4. 補助ワークストリーム

- [MILESTONE_TIMELINE_LAYER_SEARCH_2026-03-28.md](/x:/Dev/ArtifactStudio/docs/planned/MILESTONE_TIMELINE_LAYER_SEARCH_2026-03-28.md)
- [MILESTONE_TIMELINE_AUDIO_LAYER_SPECIALIZATION_2026-04-23.md](/x:/Dev/ArtifactStudio/docs/planned/MILESTONE_TIMELINE_AUDIO_LAYER_SPECIALIZATION_2026-04-23.md)
- [MILESTONE_TIMELINE_LAYER_SPECIALIZATION_EXECUTION_2026-04-23.md](/x:/Dev/ArtifactStudio/docs/planned/MILESTONE_TIMELINE_LAYER_SPECIALIZATION_EXECUTION_2026-04-23.md)
- [MILESTONE_TIMELINE_TOOLTIPS_2026-04-10.md](/x:/Dev/ArtifactStudio/docs/planned/MILESTONE_TIMELINE_TOOLTIPS_2026-04-10.md)
- [MILESTONE_TIMELINE_ZOOM_PAN_2026-04-10.md](/x:/Dev/ArtifactStudio/docs/planned/MILESTONE_TIMELINE_ZOOM_PAN_2026-04-10.md)
- [MILESTONE_TIMELINE_WAVEFORM_2026-04-10.md](/x:/Dev/ArtifactStudio/docs/planned/MILESTONE_TIMELINE_WAVEFORM_2026-04-10.md)
- [MILESTONE_TIMELINE_CURVE_EDITOR_MODE_2026-04-10.md](/x:/Dev/ArtifactStudio/docs/planned/MILESTONE_TIMELINE_CURVE_EDITOR_MODE_2026-04-10.md)

---

## 状態別まとめ

### Completed / Foundation

以下は基本的に土台として完了済み、または今の計画に吸収済み。

- `M-TL-1` Layer Basic Operations
- `M-TL-2` Layer View Sync
- `M-TL-3` Work Area / Range Unification
- `M-TL-4` Timeline TrackView Owner-Draw Migration
- `M-TL-8` Timeline QGraphicsScene Elimination
- `M-TL-11` Timeline Right Pane Full Owner-Draw

補足:

- `M-TL-4` は `M-TL-11` と `M-TL-8` に吸収された見方でよい
- `M-TL-8` は右ペインに関しては完了扱い
- 右ペインの今後は「機能追加」と「見た目/入力の磨き込み」が主になる

### Active / Current

今の作業対象として追うならここ。

- `M-TL-10` Timeline Feature Implementation / Interaction Surface
- `M-TL-5` Timeline Keyframe Editing
- `M-TL-6` Timeline Layer Search
- `M-TL-7` Timeline Search / Keyframe Integration
- `M-TL-9` Timeline Visual Language
- `M-TL-12` DAW-Style Input Surface
- `M-TL-13` Timeline Scrub Bar Frame Cache Overlay

### Specialized Follow-ups

本筋の下にぶら下がる、必要時に掘るテーマ。

- [MILESTONE_TIMELINE_TRANSFORM_KEYFRAME_EDITING_2026-04-12.md](/x:/Dev/ArtifactStudio/Artifact/docs/MILESTONE_TIMELINE_TRANSFORM_KEYFRAME_EDITING_2026-04-12.md)
- [MILESTONE_TIMELINE_TOOLTIPS_2026-04-10.md](/x:/Dev/ArtifactStudio/docs/planned/MILESTONE_TIMELINE_TOOLTIPS_2026-04-10.md)
- [MILESTONE_TIMELINE_ZOOM_PAN_2026-04-10.md](/x:/Dev/ArtifactStudio/docs/planned/MILESTONE_TIMELINE_ZOOM_PAN_2026-04-10.md)
- [MILESTONE_TIMELINE_WAVEFORM_2026-04-10.md](/x:/Dev/ArtifactStudio/docs/planned/MILESTONE_TIMELINE_WAVEFORM_2026-04-10.md)

---

## どう読むか

注記:

- `M-TL` の番号は古い計画と新しい補助線でぶつかることがある
- 迷ったら番号よりファイル名を優先する
- この index では「今の本筋」と「補助線」を優先して読む

### 右ペインのキーフレーム操作を広げたい

1. `M-TL-10`
2. `M-TL-5`
3. `M-TL-7`
4. `M-TKF`

### 右ペインの描画/入力を整えたい

1. `M-TL-10`
2. `M-TL-11`
3. `M-TL-9`
4. `M-TL-12`

### 探索やジャンプを強くしたい

1. `M-TL-6`
2. `M-TL-7`
3. `M-TL-11`

### 細かい UX を詰めたい

1. `Timeline Enhanced Tooltips`
2. `Timeline Layer Specialization`
3. `Timeline Zoom & Pan Enhancement`
4. `Timeline Audio Waveform Display`

---

## 旧文書の扱い

古い文書は消さずに残す。

- 既に完了したものは `Completed / Foundation` として読む
- 右ペインの旧 `QGraphicsScene` 系は、今は履歴として扱う
- `Phase 1/2/3/4` 実行メモは、親計画の進行記録として残す
- 仕様が分かれて見えるときは、まずこの index を見てから個別文書へ降りる
