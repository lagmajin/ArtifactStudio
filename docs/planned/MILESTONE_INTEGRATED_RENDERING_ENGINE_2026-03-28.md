# マイルストーン: Integrated Rendering Engine

> 2026-03-28 作成

## 目的

`Artifact` のレンダーを、`video-only` の出力処理から、動画と音声を同一ジョブとして扱える統合レンダリングへ段階的に移行する。

この milestone は `FFmpeg` を最終出力エンジンとして使うが、レンダリング本体そのものを FFmpeg に依存させるわけではない。  
本体は `composition -> frame / audio` の生成を担当し、FFmpeg はエンコードと mux を担当する。

---

## 現状

現在の render queue は video フレームを書き出す経路が主で、音声は後段 mux の土台がある段階に留まる。

既存資産としては次がある。

- `ArtifactRenderQueueService` の video render ループ
- `ArtifactAbstractComposition::getAudio()`
- `ArtifactAudioLayer::getAudio()`
- `FFmpegAudioEncoder::muxAudioWithVideo()`
- `Render.Settings` / `RenderAudioSettings`

ただし、まだ次のものが未整備。

- 統合 render job の音声契約
- video render と audio render の同一ジョブ化
- final mux の失敗時フォールバック
- audio source の生成と差し替えの導線

---

## 方針

### 原則

1. `video-only` 経路を壊さない
2. 音声は optional にする
3. まずは post-render mux を統合する
4. 将来、音声生成を job 内に統合しやすい形にする
5. 失敗時に中間生成物を追跡できるようにする

### 想定フロー

1. composition から video frames を render する
2. audio source を render する、または既存 audio file を受け取る
3. FFmpeg で final output に mux する
4. 失敗したら video-only 成果を残す

---

## Phase 1: Job Contract

### 目的

統合レンダリングのための job 設定を明示する。

### 作業項目

- integrated render の enable / disable
- audio source path
- audio codec
- audio bitrate
- temporary video output path

### 完了条件

- job から統合 render の意図が読める
- 既存 job の互換を壊さない

---

## Phase 2: Finalize Hook

### 目的

video render 後に audio mux を呼べるようにする。

### 作業項目

- video-only temp file を作る
- render 成功後に mux する
- mux 失敗時の error message を job に返す
- temp file cleanup の方針を決める

### 完了条件

- video+audio の final file が作れる
- mux 失敗時に video-only 成果を残せる

---

## Phase 3: Audio Generation Bridge

### 目的

composition 由来の audio を統合 render に差し込む。

### 作業項目

- `ArtifactAbstractComposition::getAudio()` を render job から利用する
- `AudioSegment` を一時 wav / pcm に落とす経路を作る
- timeline range に応じた audio cut を定義する

### 完了条件

- 動画と同じ job で音声も生成できる
- seek / range 変更時の挙動が説明できる

---

## Phase 4: Workflow and UI

### 目的

レンダー UI から統合出力を選べるようにする。

### 作業項目

- audio toggle
- audio codec / bitrate
- integrated render badge
- temp / mux / final の状態表示

### 完了条件

- 統合 render と video-only が明確に区別できる

---

## Non-Goals

- 全レンダーエンジンの全面再設計
- NLE 風の多トラック audio editor
- 生配信向けの realtime mux

---

## Related

- `docs/planned/MILESTONE_UNIFIED_AUDIO_VIDEO_RENDER_OUTPUT_2026-03-28.md`
- `Artifact/src/Render/ArtifactRenderQueueService.cppm`
- `ArtifactCore/src/Image/FFmpegAudioEncoder.cppm`
- `Artifact/src/Composition/ArtifactAbstractComposition.cppm`
- `Artifact/src/Layer/ArtifactAudioLayer.cppm`

## Current Status

2026-03-28 時点では、統合レンダリングは未整備。  
この milestone で `video-only` と `video+audio` を同じ job モデルへ寄せる。
