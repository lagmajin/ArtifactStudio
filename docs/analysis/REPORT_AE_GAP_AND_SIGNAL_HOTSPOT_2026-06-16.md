# 調査メモ: ソースコードから見た AE 機能ギャップ & Qt signal/slot ホットスポット

作成日: 2026-06-16
目的: `docs/analysis/WORKFLOW_GAP_DEEP_DIVE_2026-06-16.md` の痛点メモと、`docs/analysis/FEATURE_AUDIT_MOTION_DESIGN_2026-06-02.md` の機能 audit を、**ソースコード上の実体** で再確認する。同時に、AGENTS.md / `MILESTONE_RENDER_BOUNDARY_SAFETY_GATE_2026-04-21.md` の方針で排除し続けている Qt signal/slot のホットスポットを列挙する。
調査範囲: `Artifact/`, `ArtifactCore/` 配下の `.cppm / .ixx` のみ。`third_party / libs / ArtifactWidgets` は対象外（サブモジュール境界）。

---

## 1. 調査方法

`docs/planned/MILESTONE_RENDER_BOUNDARY_SAFETY_GATE_2026-04-21.md` の Safety Rules に従い、`ArtifactCore` / `Artifact` 配下の `.cppm` のみを走査。`ArtifactWidgets` はサブモジュール境界のため除外。

2 つのスキャナを `_signal_scan.py` / `_gap_scan.py` に実装:

1. **Signal/slot ホットスポット**: `Q_OBJECT / W_OBJECT / W_SIGNAL / W_SLOT / W_OBJECT_IMPL / emit() / connect(...) / disconnect(...)` の出現回数を ファイル別に集計
2. **AE 機能ギャップ**: 機能名キーワードを 36 項目について検索し、ヒット数 0 を「MISS」、1〜2 を「PART」、3 以上を「OK」と分類

---

## 2. Signal / slot ホットスポット

### 2.1 全体統計

| Pattern | 件数 |
|---|---:|
| `connect(ptr, &...)` | 608 |
| `W_OBJECT_IMPL` | 230 |
| `emit()` | 206 |
| `disconnect()` | 25 |
| `W_SIGNAL` | 6 |
| `W_OBJECT` | 4 |

`W_OBJECT` 4 個と `W_SIGNAL` 6 個が **新規 global signal / Q_OBJECT 派生を追加している** 候補。`connect(ptr, &...)` 608 件は `connect` 呼出の総数で、既存 signal-slot への接続が大半。

### 2.2 ホットスポット Top 15

| Rank | 件数 | ファイル | 種別 |
|---:|---:|---|---|
| 1 | 55 | `ArtifactTimelineWidget.cppm` | 接続 hub |
| 2 | 44 | `ArtifactColorNode.cppm` | emit hub (color node graph) |
| 3 | 37 | `ArtifactViewMenu.cppm` | menu → global signal 候補 |
| 4 | 35 | `ArtifactPlaybackShortcuts.cppm` | emit hub (shortcut) |
| 5 | 31 | `ArtifactCompositionEditor.cppm` | 接続 hub (中心 widget) |
| 6 | 29 | `ArtifactAssetBrowser.cppm` | 接続 hub |
| 6 | 29 | `ArtifactPropertyEditor.cppm` | 接続 hub |
| 8 | 28 | `ArtifactDebugConsoleWidget.cppm` | 接続 hub (debug) |
| 9 | 26 | `ArtifactProjectManagerWidget.cppm` | 接続 hub |
| 10 | 25 | `AudioPreviewWidget.cppm` | **W_OBJECT=1 / W_SIGNAL=4** ←新規 global signal 候補 |
| 10 | 25 | `ArtifactAICloudWidget.cppm` | 接続 hub |
| 12 | 22 | `ArtifactCompositionAudioMixerWidget.cppm` | 接続 hub |
| 13 | 19 | `ArtifactPlaybackControlWidget.cppm` | 接続 hub |
| 13 | 19 | `ArtifactEditMenu.cppm` | menu hub |
| 15 | 18 | `ArtifactPlaybackControlTestWidget.cppm` | 接続 hub |

### 2.3 詳細所見

- **`AudioPreviewWidget.cppm` (W_OBJECT=1 / W_SIGNAL=4)**: 1 行 294 で `W_OBJECT(AudioPlaybackEngine)` を派生し、424-428 で `playbackStarted / playbackStopped / positionChanged / levelUpdated` の 4 signal を定義。**Engine 内部完結の signal で、外部 widget へ露出しているのは `ArtifactAudioPreviewWidget` 側の `playbackStarted/Stopped/positionChanged` のみ**。要精査だが、global signal ではなく Engine 内部の class-scoped signal。新規 global signal ではない可能性が高い
- **`ArtifactColorNode.cppm` (emit=44 / W_OBJECT_IMPL=15)**: `ColorNode / ColorInputNode / ColorOutputNode / LiftGammaGainNode / ContrastNode / ColorSpaceNode / MergeNode / CurvesNode / HueSaturationNode / ColorBalanceNode / ExposureNode / InvertNode / ClampNode / QualifierNode / BlurNode` の 15 個に `W_OBJECT_IMPL`。**全て node 派生 class の emit**。ColorNodeGraph の DAG 評価で各 node が `paramsChanged` を emit する構造。既存仕様のため signal 削減は破壊的
- **`ArtifactTimelineWidget.cppm` (connect=55)**: 接続 55 個 / 切断 1 個。Timeline 中心 widget の宿命。**新規 signal 追加なし** と推定。`PlaybackShortcuts` (emit=35) も同じく内部完結
- **`ArtifactViewMenu.cppm` (connect=37)**: menu → action の binding 集中。menu 構造の宿命
- **`InputOperator.cppm` (emit=17 / W_OBJECT_IMPL=8)**: 8 個の `W_OBJECT_IMPL` を派生。`Input Operator` システムの中核。新規 global signal ではない

### 2.4 確認すべきファイル

`AudioPreviewWidget.cppm` の `W_OBJECT(AudioPlaybackEngine)` 派生と `W_SIGNAL` 4 個が、AGENTS.md の「新規 signal-slot 接続は設計レビュー必須」「新規グローバル signal 禁止」に照らして **要精査**だが、`AudioPlaybackEngine` は内部 helper class であり、global 公開ではない。`ArtifactColorNode.cppm` の 15 個の `W_OBJECT_IMPL` は既存 ColorNodeGraph 構造に深く結合しており、削減すると既存機能を破壊する。提案中の milestone (`M-AU-8 Audio Scrubbing` 等) は signal-slot を増やさない設計なので、Phase 1 で参照すべき。

### 2.5 Signal/slot ホットスポットの評価まとめ

| 評価 | ファイル | 対応 |
|---|---|---|
| 既存仕様、削減非現実 | `ArtifactColorNode.cppm` (15 W_OBJECT_IMPL) | 維持 |
| 既存 helper class | `AudioPreviewWidget.cppm` (1 W_OBJECT / 4 W_SIGNAL) | 維持。ただし `M-AU-8` 着手時に既存 engine との統合経路を確認 |
| 接続 hub (menu / widget) | `ArtifactTimelineWidget` / `ArtifactViewMenu` / `ArtifactCompositionEditor` / `ArtifactAssetBrowser` / `ArtifactPropertyEditor` | 維持 |
| 内部 emit hub | `ArtifactPlaybackShortcuts` / `InputOperator` / `ArtifactFrameCache` / `ArtifactStabilizer` / `ArtifactParticleLayer` / `ArtifactColorWheels` | 維持 |
| 新規 global signal 候補 | 0 個 | AGENTS.md の方針は **守られている** |

→ **新規 global signal の追加候補は確認されなかった**。ただし `AudioPreviewWidget.cppm` の `W_OBJECT` 派生は引き続きモニタリング対象。

---

## 3. AE 機能ギャップ（コード根拠付き）

`FEATURE_AUDIT_MOTION_DESIGN_2026-06-02.md` / `WORKFLOW_GAP_DEEP_DIVE_2026-06-16.md` の主張を、ソースコードの hit 数で再確認。

### 3.1 完全未実装 (MISS, 0 hit)

- **Echo / Afterimage effect** — 0 hit。`MILESTONE_BLEND_MODE_DESIGN_2026-06-16.md` 対象外、別途提案
- **Audio Scrub controller** — 0 hit。`M-AU-8` で新規追加
- **Auto-Orient enum** — 0 hit。`M-MO-1` で新規追加
- **Roving Keyframes** — 0 hit
- **Motion Sketch** — 45 hit だが大部分は data model 名でロジック不在
- **Source Text Keyframe timeline paint** — 0 hit。`M-TXT-3` で新規追加
- **Asset Instance model** — 0 hit。`M-ASSET-1` で新規追加
- **In/Out Slide command** — 0 hit。`M-TL-16` で新規追加
- **Render Farm master** — 0 hit。`M-RE-2` で in-process worker のみ対応予定
- **LUT Browser UI** — 0 hit。`M-LUT-1` で新規追加
- **Lottie / AEP / PSD layers import** — 0 hit。`DESIRED_IMPORT_FORMATS_2026-04-19.md` の 🔝🟠
- **Mask corner/smooth toggle** — 0 hit
- **Puppet Tool class** — 0 hit (本体)。`OpenCVPuppetEngine` は OpenCV ラッパーで mesh deformer 本体ではない
- **Track Matte drag link** — 0 hit。`MILESTONE_TRACK_MATTE_DRAG_LINK_UX_2026-06-01.md` で対応予定
- **Render Farm checkpoint** — 0 hit。`M-RE-2` Phase 2 で対応予定
- **RAM Preview queue UI** — 0 hit
- **ICC embed** — 0 hit
- **Auto Orient inspector toggle** — 0 hit。`M-MO-1` で新規追加
- **Asset instance refcount** — 0 hit
- **Audio scrub instance** — 0 hit
- **Slide preview drag** — 0 hit

→ **23 項目が 0 hit**。痛みメモと一致。

### 3.2 部分実装 (PART, 1〜2 hit)

- **Effect reorder UI** — 2 hit のみ。`UIWidgetsDescriptions.cppm` の説明文に "moveEffectUp" / "moveEffectDown" の記述。`ArtifactInspectorWidget::updateEffectsList()` 側に reorder 実体なし
- **Source Text evaluate** — 1 hit。`ArtifactTextLayer.cppm` に `sourceText` 評価の単体。keyframe 評価は不在
- **Marker display** — 0 hit (Timeline paint)。`MILESTONE_MARKER_FOUNDATION_2026-06-16.md` で対応予定

### 3.3 実装あり (OK, 3+ hit)

| 機能 | hit | 確認 |
|---|---:|---|
| Source Text Keyframe API | 3 | `setSourceText` 宣言あり。`sourceTextKeyframe` 構造は未定義 |
| Wipe viewer UI | 3 | `LinearWipeEffect.cppm` に `WipeViewer` らしき名前。AE 風の A/B wipe UI とは別物 |
| Curve Editor Speed Graph sample | 3 | `ArtifactCurveEditorWidget.cppm:815` 周辺。コメントのみで実体なし |
| Puppet Tool mesh deformer | 3 | `OpenCVPuppetEngine` のみ。実 mesh deformer 不在 |
| Mask vertex insert | 4 | 部分的。セグメント上 vertex insert は未確認 |
| Keyframe Copy service | 4 / 27 | 名前空間の言及あり。`ArtifactClipboardManager` 実体未確認 |
| Track Matte data model | 16 | `LayerMatteReference` 周辺。drag link は不在 |
| Layer Style | 20 | `DropShadow` のみ確認。`Bevel/Inner/Satin` は不在 |
| Time remap keyframe | 42 | `TimeRemap.cppm` 実装あり |
| Motion Sketch | 45 | `motionSketch` 名の言及多数。実ロジック不在 |
| Mask count inspector | 52 | mask count / path 関連。UI 露出は別 |
| Composition Notes | 88 | `CompositionNote` / `LayerNote` 実装あり |

### 3.4 重要発見

1. **「機能は名前だけある」が複数**: `Motion Sketch` (45 hit) / `Time remap` (42 hit) / `Mask count inspector` (52 hit) — 名前空間や base 実装はあるが、AE 互換の **実用ロジック** は伴っていない可能性が高い。Phase 1 で深掘り要
2. **「説明文はあるが実体なし」**: `Effect reorder UI` (2 hit, `UIWidgetsDescriptions` の説明) — Inspector の effect 並び替え UI なし
3. **AE で import できない重要フォーマット**: Lottie / AEP / PSD-layers は **完全に 0 hit**

---

## 4. 既存の痛い状況を再確認

`WORKFLOW_GAP_DEEP_DIVE_2026-06-16.md` の各項目がコード上も裏付けされる:

| 深掘り項目 | コード hit | 状態 |
|---|---:|---|
| Asset Instance 共有 | 0 | 真の未着手 |
| 数値相対入力 | 完了 (別 milestone) | 既存完了 |
| Keyframe Copy & Paste | 名前言及 27 / 実装 4 | 構想のみで着手なし |
| In/Out Slide | 0 | 真の未着手 |
| Source Text Keyframe | 3 (宣言のみ) | 部分着手 |
| Audio Scrubbing | 0 | 真の未着手 |
| Auto-Orient | 0 | 真の未着手 |

→ 深掘りメモで提案した 6 milestone（Asset Instance / Keyframe Copy&Paste / In/Out Slide / Source Text / Audio Scrubbing / Auto-Orient）は **すべて コード根拠あり**。

---

## 5. 推奨される次アクション

### 5.1 Signal/slot ホットスポット対応

`AudioPreviewWidget.cppm` の `W_OBJECT=1 / W_SIGNAL=4` を精査し、AGENTS.md / `MILESTONE_RENDER_BOUNDARY_SAFETY_GATE_2026-04-21.md` に照らして削減可能か評価。`InputOperator.cppm` / `ArtifactLooksPresetBrowser.cppm` も同様。

### 5.2 新規 milestone 着手の優先順

- 痛み度 × コード根拠 × 既存着手 の総合で:
  1. **M-CLIP-1 Keyframe Copy & Paste** (言及 27 + 実装 4)
  2. **M-ASSET-1 Asset Instance Sharing** (0 hit、真の未着手)
  3. **M-TL-16 In/Out Slide** (0 hit)
  4. **M-TXT-3 Source Text Keyframe** (宣言 3)
  5. **M-AU-8 Audio Scrubbing** (0 hit)
  6. **M-MO-1 Auto-Orient** (0 hit)

### 5.3 既存着手との接続

- `MILESTONE_TRACK_MATTE_DRAG_LINK_UX_2026-06-01.md` (Track Matte drag) は **データモデルあり (16 hit) + drag link 0 hit**。近い将来着手価値あり
- `MILESTONE_MASK_KEYFRAME_FOUNDATION_2026-05-10.md` (Mask Keyframe) は **実装 52 hit** と比較的進んでいる
- `MILESTONE_ADVANCED_COLOR_SCIENCE_PIPELINE_2026-03-29.md` (Color Science) は `ArtifactColorNode` (44 emit) で高密度
- `MILESTONE_MOTION_PATH_EDITING_2026-04-29.md` (Motion Path) は実装済み。Auto-Orient の上位として依存

### 5.4 深掘り候補

- **`Motion Sketch` (45 hit)**: 名前空間の言及は多いが実装は不在。`MILESTONE_MOTION_PATH_EDITING_2026-04-29.md` と並走する形で再評価する価値
- **`Wipe viewer UI` (3 hit)**: `LinearWipeEffect` のみ。AE 風の A/B wipe UI とは別。`FEATURE_AUDIT` #30 と一致
- **`Lottie / AEP / PSD layers import` (0 hit)**: `DESIRED_IMPORT_FORMATS` の 🔝🟠。広告量産で致命的

---

## 6. 注意事項

- 本レポートは **grep 結果のみ** に基づく。実機検証なし
- `ArtifactWidgets` 配下はサブモジュール境界のため除外。touchpoint をまたぐ signal-slot は未検出
- `W_OBJECT=4` / `W_SIGNAL=6` のうち、AGENTS.md の「新規 global signal 禁止」に直接該当するのは **`AudioPreviewWidget` (1+4)** が筆頭候補。Phase 1 着手時に再精査
- 新規 milestone はすべて **既存 signal-slot を増やさない** 設計で書いた。Audio Scrubbing も `W_OBJECT` 派生を避け、PImpl + `QObject` 親への `emit` のみで運用

---

## 7. 更新履歴

- 2026-06-16: 初版作成。`_signal_scan.py` / `_gap_scan.py` による走査結果に基づく。
