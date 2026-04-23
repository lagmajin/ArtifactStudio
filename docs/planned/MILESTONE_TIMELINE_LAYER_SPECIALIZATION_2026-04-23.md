# Milestone: Timeline Layer Specialization (2026-04-23)

**Status:** Planning
**Goal:** タイムラインウィンドウをレイヤー種別ごとに少しずつ専用化し、共通操作を壊さずに Audio / Video / Text / Shape などの体験を底上げする。

---

## ねらい

今のタイムラインは汎用性が高く、共通の編集導線としては十分強い。
ただし、すべてのレイヤーを同じ見た目・同じ操作で扱うと、Audio の波形や Video の素材状態のような「種別ごとの強み」が埋もれやすい。

このマイルストーンでは、タイムライン全体を分割しすぎず、レイヤー種別ごとの専門化を段階的に足す。

---

## 現状の土台

- `ArtifactTimelineWidget` は共通の親として既に存在する
- タイムライン側には `Audio / Video / Text / Shape / Image / Particle` を見分ける処理がある
- `layerTimelineColor()` で種別ごとの色分けもある
- `ArtifactAbstractComposition` 側には `hasAudio()` / `hasVideo()` 相当の判定があり、レイヤー種別ごとの派生が既に前提化している

---

## 改善方針

### Phase 1: 共通タイムライン + 種別別 descriptor
- レイヤーの種別ごとに `track descriptor` を返す
- descriptor には以下を持たせる
  - 表示名
  - 色
  - 補助ラベル
  - 特殊表示の有無
  - その種別だけの簡易アクション

### Phase 2: Audio layer の専用トラック
- 波形の常時表示
- ミュート / ソロ / ロックの見える化
- 音量オートメーションの編集導線
- フェードイン / フェードアウトの補助表示

### Phase 3: Video layer の専用トラック
- サムネイルストリップ
- ソースオフセットやリンク状態の表示
- 音声有無バッジ

### Phase 4: Text / Shape / Image / Particle の補助強化
- Text: 文字列やスタイルの簡易プレビュー
- Shape: パス編集やハンドルの可視化
- Image: 静止画のフレーム表示
- Particle: プレイ状態やプリセットの簡易表示

---

## 実装方針

- タイムラインの共通操作は `ArtifactTimelineWidget` に残す
- レイヤー専用の見た目や補助表示は、描画・ラベル・操作ヒントに寄せる
- 画面全体を `AudioTimelineWidget` などに分割しすぎない
- まずは `Audio` から始めて、1 種別ずつ足す

---

## 最初の着手候補

1. `Audio layer` の専用ヘッダ表示
2. `Audio layer` の波形描画
3. `Video layer` のサムネイル表示
4. `Text layer` の補助ラベル

---

## 連動マイルストーン

- [`MILESTONE_TIMELINE_AUDIO_LAYER_SPECIALIZATION_2026-04-23.md`](X:/Dev/ArtifactStudio/docs/planned/MILESTONE_TIMELINE_AUDIO_LAYER_SPECIALIZATION_2026-04-23.md)
  - Audio layer から先に入る具体案
- [`MILESTONE_TIMELINE_LAYER_SPECIALIZATION_EXECUTION_2026-04-23.md`](X:/Dev/ArtifactStudio/docs/planned/MILESTONE_TIMELINE_LAYER_SPECIALIZATION_EXECUTION_2026-04-23.md)
  - 共通編集を壊さない段階導入の実行版
- `Video / Text / Shape` は後続で同じ枠組みに載せる

---

## 期待効果

- Audio レイヤーの識別性が上がる
- タイムライン上で素材の種類がすぐ分かる
- 種別ごとの編集が見通しやすくなる
- 共通操作を壊さずに UX を段階改善できる
