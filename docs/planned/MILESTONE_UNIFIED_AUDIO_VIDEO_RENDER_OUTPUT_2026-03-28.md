# マイルストーン: Unified Audio / Video Render Output

> 2026-03-28 作成

## 目的

レンダーキューで出した video 出力に、音声を同時に含められるようにする。

ここでいう「同時」は、レンダー時に video を単独で出し、その後の統合工程で audio を mux して最終出力を作ることを含む。まずは安定性を優先し、video-only の既存経路を壊さない。

---

## 現状

現時点のレンダーキューは実質 video-only で動いている。

一方で `ArtifactCore` には、video と audio を後段でまとめるための `FFmpegAudioEncoder::muxAudioWithVideo()` がすでにある。

つまり、必要なのはゼロからの音声実装ではなく、既存の video render と audio mux を一つの出力導線にまとめること。

---

## 方針

### 原則

1. 既存の video-only render を壊さない
2. 音声は optional にする
3. まずは mux 後段の統合から始める
4. 音声ソースは composition / layer から取る
5. 出力失敗時に video-only 成果を失わない

### 想定する出力形

- video-only
- audio-only
- muxed audio+video
- image sequence + separate audio

---

## Phase 1: Output Contract

### 目的

ジョブが video / audio / mux のどれを出すかを明示する。

### 作業項目

- render job に audio 出力モードを追加する
- container / codec / audio codec / bitrate を分離する
- audio 有無でジョブ状態を変えない

### 完了条件

- video-only と muxed の両方を区別できる
- 既存ジョブの意味が壊れない

---

## Phase 2: Audio Source Extraction

### 目的

レンダー用 audio を composition から取り出せるようにする。

### 作業項目

- audio layer / video layer の audio を render 用に収集する
- sample rate / channels を出力設定へ合わせる
- duration / frame range から audio 区間を切る

### 完了条件

- 1 ジョブ分の音声素材を作れる
- seek / range 変更に追従できる

---

## Phase 3: Mux Integration

### 目的

video 出力と audio 出力を最終ファイルにまとめる。

### 作業項目

- `FFmpegAudioEncoder::muxAudioWithVideo()` を render workflow に接続する
- audio codec の既定値を決める
- mux 失敗時の fallback を定義する

### 完了条件

- mp4 / mov 系で音声付き出力ができる
- video-only フォールバックが残る

---

## Phase 4: UI / Workflow

### 目的

音声付き出力を render UI から選べるようにする。

### 作業項目

- output setting dialog に audio toggle を追加する
- audio codec / bitrate の表示を追加する
- render queue で mux 状態を見えるようにする

### 完了条件

- audio 付き出力を選んでも迷わない
- 設定名と実際の挙動が一致する

---

## Non-Goals

- 完全な NLE 風 audio track editor
- 音声のリアルタイム waveform 合成
- 完全な multi-track mux editor
- codec ごとの高度な音声最適化

---

## Related

- `ArtifactCore/src/Image/FFmpegAudioEncoder.cppm`
- `Artifact/src/Render/ArtifactRenderQueueService.cppm`
- `Artifact/src/Render/ArtifactRenderQueuePresets.cppm`
- `docs/planned/MILESTONE_AUDIO_LAYER_INTEGRATION_2026-03-27.md`
- `docs/planned/MILESTONE_AUDIO_PLAYBACK_STABILIZATION_2026-03-28.md`

## Current Status

2026-03-28 時点では、render queue は video-only が基本で、audio は後段 mux のための土台がある段階。
この milestone で、audio / video を一つの出力導線にまとめる。
