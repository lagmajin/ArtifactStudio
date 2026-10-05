# Milestone: Timeline Layer Specialization (2026-04-23)

**最終更新:** 2026-10-05
**ステータス:** In Progress
**進捗:** 種別識別・共通 descriptor に加え、Audio / Video / Text / Shape / Image / Particle のTimeline補助表示を段階実装中。
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

### Update 2026-10-05 — Audio fade direct manipulation

- 既存のTimeline clip painterが波形、fade ramp、右クリックの秒数編集を持つことを再確認した。
- fade rampの端点をhover／選択時にハンドルとして描き、Timeline上で左右ドラッグしてfade-in／fade-out長を調整できるようにした。
- ドラッグ中はTimeline visualだけを更新し、release時に既存の`SetLayerPropertyValueCommand`へ1回だけ確定する。コマンド失敗時は表示値を元へ戻す。
- `audio.volume`／`audio.pan`／`audio.clipGainDb`をanimatable化し、Audio channel filter内の専用`Audio Automation`グループへ限定表示する。各property行では実際の`interpolateValue()`をサンプリングしたカーブと既存keyframe markerを表示し、marker編集は既存のUndo経路を再利用する。
- Timeline sourceの静的差分のみ確認。ビルド、Undo/Redo、Audio再生への反映、DPI別ヒット領域は未確認。
- Audio専用の音量／パン／clip gain automation rowとcurve displayは実装済み。runtimeでの曲線表示・編集・Audio再生反映は未確認。

判定: **Phase 1 完了、Phase 2 はwaveform／fade ramp／直接fade編集／volume・pan・clip gain automation rowとcurve displayまで実装。実機受入れpending。Phase 3〜4もpending。**

### Update 2026-10-05 — Video audio-stream indicator

- Video clip visuals carry `ArtifactVideoLayer::hasAudio()` and show an `A` badge; muted clips show `M`, and unavailable/proxy source state is shown with `!`/`P`. These indicators are present in both painter and Diligent snapshots.
- A five-frame thumbnail strip is collected asynchronously for loaded, horizontally and vertically visible video clips. At most one clip extraction batch runs per Timeline instance; completed strips are held in a 48-entry cache keyed by canonical source path, source range, composition rate, and source version.
- Painting only reads cached `QImage` values. FFmpeg extraction and thumbnail scaling run in the background; timeline refresh never synchronously decodes video frames. The thumbnail strip is drawn by both the compatibility painter and the optional Diligent preview. Diligent textures are resolved when the static snapshot generation changes and reused for subsequent presents.
- Static diff review only. Build and runtime display, cancellation during shutdown, visual density, and decoder load were not verified.

判定: **Video Phase 3 は非同期サムネイルストリップ、source/audio状態表示、GPU preview画像表示を実装。実機受入れはpending。**

### Update 2026-10-05 — Video source mapping and GPU thumbnails

- Video clips now report the source frame mapped from the clip's composition in-point, plus whether the source is localized, shared, or unlinked. Compact `L`/`S` badges supplement the title in both painter and Diligent snapshots.
- Thumbnail extraction also runs when the optional GPU preview is active. The static Diligent snapshot carries visible thumbnail tiles; image texture lookup/upload happens only when that snapshot generation changes, and the renderer retains the resulting views between presents.
- Static source review only. Build, GPU upload/runtime behavior, decoder shutdown, and visual alignment remain unverified.

判定: **Videoのソースoffset/link状態とGPU previewサムネイル表示まで実装。実機受入れpending。**

### Update 2026-10-05 — Text style preview

- Text clips continue to sample the evaluated source string at the current timeline frame and now also pass the layer font family/size to the compatibility painter.
- The clip title is drawn in the selected font family at a bounded 9–15 px preview size so large authoring sizes remain legible in compact rows. The Diligent snapshot now carries the same family and bounded pixel size for text clips.
- Static source review only. Build and runtime appearance were not verified.

判定: **Text Phase 4 は文字列とフォントファミリーの簡易プレビューを実装。GPU previewのフォント表示も対応し、実機受入れはpending。**

### Update 2026-10-05 — Shape path preview

- Selected custom-path Shape clips now show a bounded miniature of sampled anchors and incoming/outgoing Bézier handles in both the compatibility painter and Diligent preview.
- The timeline uses a read-only `std::span` view of the existing path vertices; preview sampling is capped at 48 vertices, so large authored paths do not get copied into the timeline visual model.
- The preview follows layer selection. It does not claim to detect whether the separate Layer Editor is currently in path-edit mode; that mode state remains owned by the editor widget and is not exposed through a shared read API.
- Static source review only. Build and runtime density/legibility were not verified.

判定: **Shapeのカスタムパス・アンカー・ハンドルのタイムライン表示を実装。Layer Editorの編集中state連携と実機受入れはpending。**

### Update 2026-10-05 — Image source thumbnail

- Linked still images and the currently cached image-sequence frame now show a fitted thumbnail in both Painter and Diligent timeline surfaces.
- Decoding and scaling run asynchronously for visible clips. Embedded stills use the layer's already-loaded image only and are scaled in the background; the timeline does not synchronously decode them during refresh.
- Results share the bounded 48-entry source-thumbnail cache with video strips and are keyed by source path/version plus sequence frame index (or layer identity for embedded stills).
- Static source review only. Build, decoder concurrency, color parity, and runtime display remain unverified.

判定: **Imageの静止画／現在フレームサムネイル表示を実装。埋め込み静止画はロード済み素材に対応し、実機受入れpending。**

### Update 2026-10-05 — Particle and group-container summaries

- Particle clips now show Playing/Paused, emitter count, billboard mode, and the originating preset name. The preset name is serialized by the layer; older files without that field read as `Custom`. Timeline refresh does not evaluate or advance the simulation.
- Group-container summary bars show the current child-layer count. Dragging the bar moves every child by one shared frame delta through existing per-layer move commands grouped into one Undo macro; a timing-locked or missing child rejects the whole move.
- Dragging a layer row onto a standalone Group Container now changes its parent in the composition node hierarchy. The composition validates node kinds and cycles, and a dedicated undo command restores the prior layer/container parent.
- Static source review only; custom-path edit-session state remains pending because the active session belongs to the separate Layer Editor widget. GroupContainer reparent behavior and Undo/Redo remain unverified at runtime.

判定: **Particle の状態／billboard／preset名、GroupContainerの子数表示・Undo対応の時間移動・レイヤー行からのreparentを実装。Shapeの編集中stateと実機受入れはpending。**

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
