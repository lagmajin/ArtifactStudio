# GPU Offload Targets — Non-Rendering Pipeline (2026-08-08)

**最終更新:** 2026-08-08
**状態:** 調査完了・提案

## 概要

描画パイプライン（LayerBlendPipeline, PrimitiveRenderer3D）と動画エフェクト以外で GPU 化の恩恵がある CPU ホットパスを特定し、既存の Diligent コンピュートシェーダー基盤を活用した移行計画を提案する。

## 前提

- **OpenCL / CUDA 導入不要**。DiligentEngine（DX12）のコンピュートシェーダーがすでに使用可能
- **実装テンプレートあり**: `EdgeBloomEffect`（`Artifact/src/Effects/Glow/EdgeBloomEffect.cppm`）が Diligent コンピュートシェーダーによる画像処理を先行実装済み
- **`Graphics.Compute` モジュール**（`ArtifactCore`）が `GpuContext`, `LUT3DComputer` を含む汎用コンピュート基盤を提供
- 全候補が **Parallel::For** または **OpenCV** による CPU 処理。GPU への移植性が高い

---

## 候補一覧

### 🔴 最優先（コスト対効果が極大）

#### 1. PyroSimulation — 3D グリッド流体シミュレーション

**ファイル**: `ArtifactCore/src/Simulation/PyroSimulation.cppm`
**現状**: 6箇所の `Parallel::For(0, depth, ...)` で幅×高さ×奥行きの 3D グリッドを CPU 処理
**CPU コスト**: 64³ グリッドで1ステップあたり数十ミリ秒。全ステップで数秒
**GPU 化**: 3D グリッドはコンピュートシェーダーとの相性が最も良い問題の一つ。各グリッドセルが独立して計算可能

```
移行対象:
  advection (移流)        → 1 CS
  combustion (燃焼)       → 1 CS
  vorticity confinement   → 1 CS
  divergence (発散)       → 1 CS + reduction
  pressure solver (圧力)  → 複数 CS（ヤコビ反復またはマルチグリッド）
  velocity update         → 1 CS

期待効果: 10〜50x 高速化。64³ がリアルタイムに
```

**変更量**: 中（コンピュートシェーダー 5〜6本 + CPU→GPU データ転送追加）

---

#### 2. ガウスぼかしエフェクト

**ファイル**: `Artifact/src/Effects/Blur/GauusianBlur.cppm`
**現状**: OpenCV `cv::GaussianBlur()` + CPU `Parallel::For`
**GPU 化**: `EdgeBloomEffect` が Diligent コンピュートシェーダーでの分離可能ガウスブラーを先行実装済み。**ほぼ流用可能**
**期待効果**: 4K 画像の radius=50 ブラーで CPU 数百ミリ秒 → GPU 1〜2ミリ秒
**変更量**: 低（テンプレートが存在する）

---

### 🟡 中優先

#### 3. TurbulentDisplaceEffect — ノイズベースのピクセル変位

**ファイル**: `Artifact/src/Effects/TurbulentDisplace/TurbulentDisplaceEffect.cppm`
**現状**: `Parallel::For` + OpenCV。per-pixel のハッシュノイズ＋バイリニアサンプリング
**CPU コスト**: 4K で 2回のフルフレームループ（ノイズ計算 + 変位適用）
**GPU 化**: ハッシュノイズ関数 + `sampleBilinear` をコンピュートシェーダーに移植
**期待効果**: 4K で 10〜20x 高速化

---

#### 4. TimeDisplacementEffect — 時間方向フレームサンプリング

**ファイル**: `Artifact/src/Effects/TimeDisplacement/TimeDisplacementEffect.cppm`
**現状**: 2回の `Parallel::For`。cv::Mat での複数フレーム参照＋バイリニアブレンド
**GPU 化**: 参照フレームを UAV として事前アップロードし、コンピュートシェーダーでピクセル単位サンプリング
**期待効果**: フレームブレンドモードで顕著に高速化

---

#### 5. TiltShift + Distortion — ピクセル単位変位

**ファイル**: `ArtifactCore/src/ImageProcessing/TiltShift.cppm`, `ArtifactCore/src/ImageProcessing/Distortion.cppm`
**現状**: TiltShift は 2-pass box blur（`Parallel::For`）、Distortion は bilinear sample with displacement（`Parallel::For`）
**GPU 化**: 分離可能ブラー（TiltShift）＋ピクセル変位（Distortion）。どちらもコンピュートシェーダーの基本パターン
**期待効果**: 中程度

---

### 🟢 低優先（用途が限定的 or 実装が複雑）

#### 8. AudioAnalyzer + AudioSpectrum — CPU FFT

**ファイル**: `ArtifactCore/src/Audio/AudioAnalyzer.cppm`, `ArtifactCore/src/Audio/AudioSpectrum.cppm`
**現状**: 自前 Radix-2 FFT（`AudioAnalyzer::computeFFT`、Cooley-Tukey）＋ 簡易 DFT（`AudioSpectrum::computeFFT`、O(n²) ループ）。毎フレーム実行
**CPU コスト**: 8192点FFTが毎フレーム。`AudioSpectrum` の簡易DFTは n が増えると致命的に遅い（コメントに「FFTはQtMultimedia::QAudioSpectrum や外部ライブラリを推奨」と明記）
**GPU 化**: FFT は GPU コンピュートの定番。Diligent コンピュートシェーダーで Stockham 反復 FFT。`AudioSpectrum` は O(n log n) 化で複数バンド解析が劇的に高速化
**変更量**: 中（FFT コンピュートシェーダー 1本）

---

#### 9. SoftBodySolver — 布・ゲル物理（Position-Based Dynamics）

**ファイル**: `ArtifactCore/src/Physics/SoftBodySolver.cppm`
**現状**: Verlet 積分 + 距離拘束。全質点ループ + 拘束反復 5〜20回。32×32 グリッド布で数千回の拘束評価
**GPU 化**: PBD 距離拘束はコンピュートシェーダーと非常に相性が良い。全質点・全拘束が独立処理可能
**期待効果**: 10〜30x。64×64 グリッド布がリアルタイムに
**変更量**: 中（拘束 + 積分 CS 2〜3本）

---

#### 10. NccTracker — 正規化相互相関トラッキング

**ファイル**: `ArtifactCore/src/Track/NccTracker.cppm`
**現状**: `cv::matchTemplate()` でテンプレートマッチング。サーチ領域 × テンプレート領域の全画素 NCC 計算。フレームごとに実行
**GPU 化**: NCC は 2D 畳み込みとして実装可能。FFT 経由の相関なら超高効率
**期待効果**: 10〜50x。リアルタイムトラッキング
**変更量**: 中（NCC CS 1本）

---

#### 11. RotoMask — ベジェマスクのフィルとフェザー

**ファイル**: `ArtifactCore/src/Mask/RotoMask.cppm`
**現状**: `Parallel::For(0, height, ...)` で全ピクセル inside/outside 判定（Line 548）+ 2箇所の `Parallel::For(0, N, ...)` でフェザー（Line 617, 624）。4K で 800万ピクセル
**GPU 化**: サブディビジョン → ラスタライズ → フェザーブラー を GPU で。編集時のマスクプレビューがリアルタイム化
**変更量**: 中

---

#### 6. AutoColorMatch — 色空間変換

**ファイル**: `ArtifactCore/src/Color/AutoColorMatch.cppm`
**現状**: `Parallel::For` で RGB ↔ Lab 変換＋3D ヒストグラム解析＋マッチング。数学的に複雑
**GPU 化**: 可能だが、用途がレンダリング結果の事後調整に限られる。優先度低

---

#### 7. Video::Stabilizer — 特徴点マッチング

**ファイル**: `ArtifactCore/src/Video/Stabilizer.cppm`
**現状**: 自前実装の特徴点検出＋対応点マッチング＋アフィン推定。OpenCV 不使用
**GPU 化価値**: 特徴点マッチングは GPU 化の恩恵が大きいが、実装が複雑。ORB/SIFT 相当の再実装になる

---

## 実装優先順位

| Phase | 対象 | コスト | 期待効果 | 備考 |
|-------|------|--------|---------|------|
| 1 | ガウスぼかし | 低 | 10〜50x | EdgeBloomEffect テンプレートあり |
| 2 | PyroSimulation | 中 | 10〜50x | 3D グリッド CS。リアルタイム化 |
| 3 | FFT (AudioAnalyzer/Spectrum) | 中 | 5〜50x | AudioSpectrum は O(n²)→O(n log n) |
| 4 | SoftBodySolver | 中 | 10〜30x | 布物理リアルタイム化 |
| 5 | NccTracker | 中 | 10〜50x | トラッキング高速化 |
| 6 | RotoMask fill + feather | 中 | 5〜20x | マスク編集の応答性 |
| 7 | TurbulentDisplace | 中 | 10〜20x | |
| 8 | TimeDisplacement | 中 | 5〜10x | |
| 9 | TiltShift + Distortion | 低 | 5〜10x | |

## 変更対象ファイル一覧

| ファイル | Phase | 新規追加 |
|----------|-------|---------|
| `Artifact/src/Effects/Blur/GauusianBlur.cppm` | 1 | 改変 |
| `Artifact/App/shaders/gaussian_blur_cs.hlsl` | 1 | 新規 |
| `ArtifactCore/src/Simulation/PyroSimulation.cppm` | 2 | 改変 |
| `Artifact/App/shaders/pyro_*.hlsl`（5本） | 2 | 新規 |
| `ArtifactCore/src/Audio/AudioAnalyzer.cppm` | 3 | 改変 |
| `ArtifactCore/src/Audio/AudioSpectrum.cppm` | 3 | 改変 |
| `Artifact/App/shaders/fft_cs.hlsl` | 3 | 新規 |
| `ArtifactCore/src/Physics/SoftBodySolver.cppm` | 4 | 改変 |
| `Artifact/App/shaders/softbody_*.hlsl`（2本） | 4 | 新規 |
| `ArtifactCore/src/Track/NccTracker.cppm` | 5 | 改変 |
| `Artifact/App/shaders/ncc_tracker_cs.hlsl` | 5 | 新規 |
| `ArtifactCore/src/Mask/RotoMask.cppm` | 6 | 改変 |
| `Artifact/App/shaders/rotomask_fill_cs.hlsl` | 6 | 新規 |
| `Artifact/src/Effects/TurbulentDisplace/TurbulentDisplaceEffect.cppm` | 7 | 改変 |
| `Artifact/App/shaders/turbulent_displace_cs.hlsl` | 7 | 新規 |
| `Artifact/src/Effects/TimeDisplacement/TimeDisplacementEffect.cppm` | 8 | 改変 |
| `Artifact/App/shaders/time_displace_cs.hlsl` | 8 | 新規 |
| `ArtifactCore/src/ImageProcessing/TiltShift.cppm` | 9 | 改変 |
| `ArtifactCore/src/ImageProcessing/Distortion.cppm` | 9 | 改変 |
| `Artifact/App/shaders/tiltshift_cs.hlsl` | 9 | 新規 |
| `Artifact/App/shaders/distortion_cs.hlsl` | 9 | 新規 |
