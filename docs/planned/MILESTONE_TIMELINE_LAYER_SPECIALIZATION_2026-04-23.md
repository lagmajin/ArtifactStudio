# Milestone: Timeline Layer Specialization (2026-04-23)

**最終更新:** 2026-08-15
**Status:** 部分実装（種別識別・共通 descriptor・表示モードは実装、専用トラック表示は未完了）
**Goal:** タイムラインウィンドウをレイヤー種別ごとに少しずつ専用化し、共通操作を壊さずに Audio / Video / Text / Shape などの体験を底上げする。

## 2026-08-15 現行コード監査

`ArtifactLayerPanelPresentation` に種別別の timeline descriptor／badge 情報があり、`ArtifactLayerPanelWidget` では Audio、Video、Text、Shape、Image、Particle、3D、Camera などのアイコン識別と AudioOnly／VideoOnly／SelectedOnly 等の表示モードが実装されている。共通タイムラインを保ったまま種別を読ませる Phase 1 の基盤は進んでいる。

一方、Audio は `TrackClipVisual` へ非同期 waveform cache を渡し、painter 側で波形を描画する主要経路まで実装されている。ただしフェード／オートメーション表示、Video のサムネイルストリップ／ソース状態、Text の文字列プレビュー、Shape のパス補助表示、Particle の専用状態表示は、この監査範囲では確認できない。したがって Phase 2 は波形部分まで部分実装、残りの Phase 2〜4 は未完了で、表示の実機密度・操作感も未検証とする。

`audio.volume`／`audio.pan` は共通の keyframe marker 経路で表示対象になるため、既存のオートメーション編集基盤は再利用できる。一方、clip 内のフェード領域や専用 automation curve を描くデータ契約はまだ存在しないため、今回は新しい仮表示を追加していない。

### Update 2026-08-15

Video clip のTimeline表示に、既存の `ArtifactVideoLayer::isLoaded()` と `VideoStreamInfo` の解像度を使った source state（Loaded／Source unavailable）と解像度サフィックスを追加した。デコードやサムネイル生成をTimeline更新で発生させず、現在の共通clip描画だけでソース状態を読めるようにしている。サムネイルストリップ自体は未実装。

Text clip には `sourceTextAtFrame()` を使った現在フレームの短い内容プレビュー（最大32文字）を追加した。キーフレーム評価後の値を読むため、Source Text アニメーションでもTimeline上の表示が現在フレームに追従する。専用のText track／style previewは未実装。

Shape clip には既存の `shapeType()` と `hasCustomPath()` を使い、Rectangle／Ellipse／Star 等の種別と Path 編集状態を補助表示するようにした。形状ジオメトリの再計算や専用操作は追加していない。

Particle clip には既存の `isPlaying()` と `emitterCount()` を使い、Playing／Paused と emitter 数を表示する補助ラベルを追加した。Form Particle は種別ラベルのみ表示し、設定の再評価やシミュレーション操作は行わない。

Image clip には既存の `isImageSequence()` と `sourcePath()` を使い、Still Image／Image Sequence と Embedded／Linked の状態を表示する補助ラベルを追加した。画像デコードやサムネイル生成はTimeline更新から呼び出していない。

3D Model clip には Primitive／Model と頂点数・ポリゴン数、Camera clip には Perspective／Orthographic と Active状態を表示する補助ラベルを追加した。メッシュ再読込や投影行列の生成はTimeline更新から呼び出していない。

判定: **Phase 1 完了、Phase 2 は波形部分のみ実装、Phase 3〜4 は pending。**

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
- Phase 1 execution memo is absorbed into the parent milestone
  - 共通編集を壊さない段階導入の実行版
- `Video / Text / Shape` は後続で同じ枠組みに載せる

---

## 期待効果

- Audio レイヤーの識別性が上がる
- タイムライン上で素材の種類がすぐ分かる
- 種別ごとの編集が見通しやすくなる
- 共通操作を壊さずに UX を段階改善できる
