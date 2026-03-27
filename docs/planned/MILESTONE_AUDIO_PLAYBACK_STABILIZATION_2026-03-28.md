# マイルストーン: Audio Playback Stabilization

> 2026-03-28 作成

## 目的

Audio playback を、素材や再生開始タイミングの差に対してより安定させる。

このマイルストーンは、`ArtifactPlaybackEngine`、`AudioRenderer`、`AudioRingBuffer`、`ArtifactVideoLayer` の接続を見直し、
`underflow` / `overflow` / format mismatch / start-up glitch を減らすことを狙う。

---

## 背景

現状の audio path は、再生ループが `composition_->getAudio()` を都度積み、`AudioRenderer` が ring buffer から hardware へ吐き出す構造になっている。

そのため、以下が起きやすい。

- 再生開始直後の underrun
- seek 後の buffer 再構築不足
- 素材ごとの sample rate / channel 差による揺れ
- レイヤーが一瞬でも供給を落とした時の無音化

---

## 方針

### 原則

1. start 時は先に少し溜めてから出す
2. stop / seek 時に stale buffer を残さない
3. hardware clock に対して過剰に追従しすぎない
4. format / sample rate の違いはできるだけ renderer 側で吸収する
5. 失敗した時は silence で落とし、クラッシュや大きな glitch にしない

### 想定対象

- `ArtifactPlaybackEngine`
- `AudioRenderer`
- `AudioRingBuffer`
- `QtAudioBackend`
- `WASAPIBackend`
- `ArtifactAudioLayer`
- `ArtifactVideoLayer`

---

## 既存資産

- `Artifact/src/Playback/ArtifactPlaybackEngine.cppm`
- `ArtifactCore/src/Audio/AudioRenderer.cppm`
- `ArtifactCore/src/Audio/AudioRingBuffer.cppm`
- `ArtifactCore/src/Audio/QtAudioBackend.cppm`
- `ArtifactCore/src/Audio/WASAPIDevice.cppm`
- `Artifact/src/Layer/ArtifactAudioLayer.cppm`
- `Artifact/src/Layer/ArtifactVideoLayer.cppm`
- `Artifact/docs/MILESTONE_AUDIO_ENGINE_2026-03.md`
- `docs/planned/MILESTONE_AUDIO_LAYER_INTEGRATION_2026-03-27.md`

---

## Phase 1: Start-up Pre-roll

### 目的

再生開始直後の underrun を減らす。

### 作業項目

- backend start 前に一定量 buffer を貯める
- start 時に ring buffer を消さない
- pre-roll threshold と steady-state threshold を分ける

### 完了条件

- 再生開始直後の無音や途切れが減る
- start と close の責務が分かれる

---

## Phase 2: Seek / Stop Hygiene

### 目的

seek / stop 後の stale data を整理する。

### 作業項目

- stop 時に buffer を clear
- closeDevice 時に buffer を clear
- seek 時に audioNextFrame_ と buffer を再同期

### 完了条件

- seek 後に古い音が混ざらない
- stop / restart で音の残骸が出ない

---

## Phase 3: Format Normalization

### 目的

素材ごとの audio format 差を吸収する。

### 作業項目

- sample rate の正規化
- mono / stereo の扱い統一
- `ArtifactAudioLayer` と `ArtifactVideoLayer` の供給形式整理

### 完了条件

- 素材差で再生安定性が崩れにくい
- renderer 側の変換が一貫する

---

## Phase 4: Clock / Buffer Feedback

### 目的

audio state を見える化して再生の追い込みをしやすくする。

### 作業項目

- underflow / overflow の簡易 diagnostics
- buffer 水位の可視化
- audio clock と frame clock のずれ監視

### 完了条件

- どこで崩れているか分かる
- playback service の追従を調整しやすい

---

## Recommended Order

1. Phase 1
2. Phase 2
3. Phase 3
4. Phase 4

---

## Current Status

2026-03-28 時点で、`ArtifactPlaybackEngine` の pre-roll と `AudioRenderer` の buffer hygiene を入れて、開始直後の underrun を減らす方向へ進めている。

あわせて `AudioRenderer` の underflow / overflow カウンタを公開し、Phase 4 の diagnostics へつなげられる状態にした。

この文書は、`MILESTONE_AUDIO_ENGINE_2026-03.md` の派生として、実運用の glitch を減らすための workstream として扱う。
