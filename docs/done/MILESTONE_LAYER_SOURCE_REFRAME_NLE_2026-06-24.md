# M-LE-2: Layer Transform からの Crop / Pan 導線と Source Reframe 透明化

**Status:** Completed
**Goal:** NLE ライクな `Crop / Pan` 導線を `Layer Transform` 直下に追加し、既存の `Source Reframe` を再利用しながら、crop 外側を透明として扱う。

## Context

現在の `Source Reframe` は `ArtifactSourceCrop` を使っており、`sourceCrop.enabled` / `cropRect` / `pan` / `zoom` / `rotation` / `anchor` を持っている。
Inspector 側には既に `Source Reframe` セクションがあり、`ArtifactVideoLayer` と `ArtifactImageLayer` の両方で同系統の property を公開している。

このマイルストーンでは、新規の専用クラスを増やすのではなく、既存の `SourceCrop` を再利用して UI 導線を NLE 風にする。

## Scope

- `Layer Transform` の下に `Add Crop / Pan` ボタンを追加する
- ボタン押下で `sourceCrop.enabled` を true にする
- 押下後に `Crop / Pan` セクションへ自動スクロールする
- `Source Reframe` の crop 表示は、元サイズを維持しつつ crop 外側を透明として扱う
- `ArtifactImageLayer` と `ArtifactVideoLayer` の描画経路を同一方針に揃える

## Non-goals

- 新しい crop 専用コアクラスの追加
- 独立した NLE clip editor の新設
- crop ハンドルの viewport 直接編集

## Phases

### Phase 1: 導線

- `Layer Transform` 直下に `Add Crop / Pan` ボタンを置く
- 既存の `Source Reframe` セクションへ誘導する
- 押下時にセクションを自動スクロールする

### Phase 2: 表示

- crop 外側を透明にして、切り抜き後も元サイズの見た目を保つ
- `localBounds()` を crop サイズへ縮めず、レイヤー全体の寸法を維持する
- image / video の双方で同じ表示を確認する

### Phase 3: 整理

- `Source Reframe` 表示名の整理
- `Crop / Pan` 文言の統一
- 必要なら折りたたみや強調表示を詰める

## Completion Note

2026-06-24 時点で、導線・表示・描画の主要 slice は実装済みと判断した。
残りは細かな表記調整や将来の微修正に寄せる。

## Related Files

- `Artifact/src/Widgets/ArtifactPropertyWidget.cppm`
- `Artifact/src/Layer/ArtifactImageLayer.cppm`
- `Artifact/src/Layer/ArtifactVideoLayer.cppm`
- `Artifact/src/Layer/ArtifactSourceCrop.cppm`
