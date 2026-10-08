# MILESTONE: Spatial Room / 初期反射 / 残響の物理化（M-AU-9.7 具体化）

**最終更新:** 2026-10-08
**ステータス:** 設計確定・実装待ち
**親文書:** `docs/planned/MILESTONE_SPATIAL_AUDIO_OBJECT_RENDERING_2026-09-04.md`（M-AU-9）
**対象:** `ArtifactCore/`（`Audio.Spatial.*`、`Audio.Effect.Reverb`）、`Artifact/`（`ArtifactSpatialAudioLayer`）

---

## 1. 背景（確認済み事実）

- `ArtifactCore/src/Audio/Spatial/SpatialRenderer.cppm` は Direct path のみ。距離減衰・cone・`airAbsorption`（gain + 1pole LPF）・VBAP / 解析的バイノーラル（ITD + head-shadow）・spread・LFE send まで実装。room の概念はなし。
- `airAbsorption` は `dist / maxDistance` 正規化の簡易式で、物理の dB/m 特性ではない。
- 残響は2系統に分断:
  - `Artifact/src/Audio/Effects/ReverbEffect.cppm`: DattorroPlate / 8-line FDN Hall / Hybrid の高機能版。`preDelay/decay/diffusion/damping/mod/size/ER` あり。バス Insert 用で Spatial Object とは未接続。
  - `ArtifactCore/src/Audio/AudioReverb.cppm`: 簡易 4-comb。`size_` は保存されるが DSP で未使用、`allpassBuffer_` も未使用。Spatial 側から呼ばれていない。
- `ArtifactSpatialAudioLayer::getAudio` は mono/stereo のみ、時刻評価・seek reset・gain 一重適用は接続済み。room send・tail 契約なし。

## 2. 非目標

- 畳み込み残響（IR ファイル取り込み）は本マイルストーンの範囲外。M-AU-9.6 とは別枠。
- Dolby Atmos / Auro-3D の公式コーデック・認証・ビットストリーム互換は目標にしない（M-AU-9 の方針を継承）。
- GPU 音声処理は範囲外。デバイス同期による遅延要因を増やさない。
- 有償 SDK・ロイヤリティ必須構成は採用しない（2026-09-05 ユーザー方針）。

## 3. 設計

### R1: Room send（最小・先行）

- 共有 `RoomParams { volumeM3, rt60Seconds, wetLevel, preDelayMs }` を導入する。Object 側は `roomSend` のみ持つ。
- Object の send 量 = `roomSend * atten * cone * airGain` で算出する。Direct の VBAP / バイノーラル経路は不変。
- Core 側に固定バッファの共有ステレオ FDN を新設する（`Artifact/ReverbEffect` の FDN を Core へ移植するのではなく、固定長・確保済み版として `Audio.Spatial.Room` に新設）。ホットパス確保ゼロ。
- `SpatialParams` への追加は `roomSend` のみとし、`RoomParams` は composition 共有の1インスタンスとする。Object ごとに部屋を持つ設計にしない。

### R2: 初期反射（image-source lite）

- 部屋寸法から1次反射 4〜6 taps のみ。tap ごとに fractional delay + `1/d` gain + 方向→VBAP / バイノーラルへ分配。
- tap 数は固定（RT 安全性）。delay は block 間補間でクリック防止。
- 反射 tap の方向計算は `SpeakerLayout::calculateSpeakerGains` の再利用を優先し、新規パン経路を作らない。

### R3: 統合・Export

- `tailSamples()` を Spatial + Room 経路に実装し、seek / reset 世代管理と合わせる。
- JSON `spatial.room.*` + Property 編集（`roomSend` のみ Object 側、部屋寸法・RT60 は共有 Room 側）。
- impulse / NaN / soak テストを追加する。

## 4. 受け入れ条件

1. Direct の定位・gain 契約が不変（既存テストが緑のまま）。
2. `roomSend=0` で現行出力と bit 一致（Direct 経路への混入なし）。
3. `roomSend>0` で wet が距離・cone・air に追従する（遠方ほど Direct 減・wet 比増）。
4. seek / mute / source 切替 / sample-rate 変更で tail が残留しない。
5. Export に tail が含まれる（`tailSamples()` 申告どおり）。
6. ホットパスで `new` / `delete` / コンテナ拡張なし（HOT_PATH_RULES 準拠）。

## 5. 実装順

R1 → R2 → R3。R1 の send 量算出と共有 FDN の固定化が最初の実装単位。

## 6. 子 repo への変更予定（明示許可後に実施）

- `ArtifactCore/include/Audio/Spatial/SpatialParams.ixx`: `roomSend` 追加 + sanitize。
- `ArtifactCore/include+src/Audio/Spatial/` 新規: `RoomParams` / 共有 FDN（固定バッファ）。
- `ArtifactCore/src/Audio/Spatial/SpatialRenderer.cppm`: send 量算出 + 共有 FDN への供給。
- `Artifact/src/Layer/ArtifactSpatialAudioLayer.cppm` + `.ixx`: `spatial.roomSend` の JSON / Property / runtime 評価。
- `tests/ArtifactCore/`: room send 契約テスト（`roomSend=0` bit 一致、距離追従、seek 残留なし）。

ビルド・テスト実行はユーザーの明示指示後に行う。本書更新では実行していない。
