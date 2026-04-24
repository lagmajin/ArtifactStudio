# Milestone: Timeline Layer Specialization Execution (2026-04-23)

**Status:** Planning
**Parent:** `MILESTONE_TIMELINE_LAYER_SPECIALIZATION_2026-04-23`
**Goal:** タイムラインのレイヤー種別ごとの専用表示を、共通編集を壊さない範囲で段階導入する。

---

## まずやること

`Audio / Video / Text / Shape / Image / Particle` を全部別物として扱うのではなく、
共通の timeline shell の上に「その種別で見たい最小情報」だけ載せる。

最初の目標は、見た目の差ではなく「区別できること」を増やすこと。

---

## 現状の土台

- タイムラインは既に共通の `ArtifactTimelineWidget` がある
- レイヤー種別の判定や色分けはすでに存在する
- `Audio` には波形や状態の補助表示を足しやすい
- `Video` にはサムネイルや素材状態を足しやすい
- `Particle` には再生状態やプリセット名の補助表示が足しやすい

---

## Phase 1: 種別 descriptor の整理

- レイヤー種別ごとの簡易 descriptor を返す
- descriptor には次を含める
  - 表示ラベル
  - 色
  - 補助ラベル
  - 専用バッジの有無
  - その種別だけの軽いヒント

---

## Phase 2: Audio / Video の最小専用化

### Audio

- 波形の見え方
- ミュート / ソロ / ロック
- クリップの有効範囲

### Video

- サムネイルストリップ
- ソース解像度
- デコード / リンク状態の補助表示

---

## Phase 3: Text / Shape / Image / Particle の補助強化

- Text: 内容やスタイルの短い補助
- Shape: パス編集中であることが分かる補助
- Image: 静止画であることを明示
- Particle: 再生状態、billboard mode、プリセット名を軽く表示

---

## Phase 4: 共通操作との整合

- 選択 / 移動 / トリム / ソロ / 隠しの操作は共通のまま維持する
- 補助表示は操作を増やしすぎない
- 種別専用の見た目で、共通編集の邪魔をしない

---

## 実装メモ

- まずは表示専用で始める
- 編集系の専用操作は後から足す
- タイムライン左ペインと右ペインで種別の意味を揃える
- 既存の `layerTimelineColor()` と `describeLayerPresentation()` を活かす

---

## 完了条件

- Audio / Video の差が一目で分かる
- Particle / Text / Shape / Image に最低限の補助がある
- 共通編集操作は変わらない
- 種別専用化が見た目だけで終わらない

---

## リスク

- 情報を載せすぎるとタイムラインがうるさくなる
- 種別ごとに実装を増やしすぎると保守が重くなる
- 共通 shell から逸れすぎると、また別UI が増える

---

## 参照

- [`MILESTONE_TIMELINE_LAYER_SPECIALIZATION_2026-04-23.md`](x:/Dev/ArtifactStudio/docs/planned/MILESTONE_TIMELINE_LAYER_SPECIALIZATION_2026-04-23.md)
- [`MILESTONE_TIMELINE_AUDIO_LAYER_SPECIALIZATION_2026-04-23.md`](x:/Dev/ArtifactStudio/docs/planned/MILESTONE_TIMELINE_AUDIO_LAYER_SPECIALIZATION_2026-04-23.md)
- [`WIDGET_MAP.md`](x:/Dev/ArtifactStudio/docs/WIDGET_MAP.md)

---

## 次の一手

1. Audio と Video のどちらを先に専用化するか決める
2. descriptor の最小フィールドを決める
3. 表示だけで足りるか、補助アクションまで入れるか決める
