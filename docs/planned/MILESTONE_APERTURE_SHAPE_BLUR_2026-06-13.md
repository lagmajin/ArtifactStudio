> **SUPERSEDED** — 2026-08-04: 統合先 [MILESTONE_GLITCH_EFFECTS_PROPOSAL_2026-06-13.md](MILESTONE_GLITCH_EFFECTS_PROPOSAL_2026-06-13.md)

# Milestone: ApertureShapeBlur (カスタム開口レンズボケ / FFT物理ブラー)

> 2026-06-13

## Purpose

`ApertureShapeBlur` は、任意の白黒マスクをレンズの開口形状、つまり PSF
(`Point Spread Function`) として扱う、**カスタム開口レンズボケ**である。

ハート、星、桜の花びら、猫シルエット、さらにはレンズの汚れや指紋のような
不規則パターンまで、そのままボケ形状として使える。

## Why This Exists

- 既存のレンズブラーよりも、ボケ形状の表現幅が広い
- ヴィンテージレンズや汚れたレンズの「味」を再現しやすい
- 玉ボケの形状を作品ごとに変えられる
- FFT ベースにすると、大きいカーネルでも現実的な速度を狙える

## Existing Signals

- `Artifact\src\Layer\ArtifactCameraLayer.cppm` に aperture 設定がある
- `Artifact\App\shaders\fft_512x512_c2c_CS.hlsl` に FFT 系の実装資産がある
- `Artifact\App\shaders\ShaderInterop_FFTGenerator.h` に FFT 用の constant buffer がある
- `Artifact\src\Widgets\ArtifactLooksPresetBrowser.cppm` に bokeh 系の見た目がある
- `ArtifactCore/include/ImageProcessing/Distortion.ixx` に画面変形やサンプリング変換の土台がある

## Core Idea

この effect は次の 3 層で構成する。

1. PSF Authoring
   - 白黒マスクを正規化して開口形状にする
2. Frequency Convolution
   - FFT / IFFT で画像と PSF を畳み込む
3. Optical Character
   - 二線ボケ、縁の明るさ、汚れ由来のざらつきを演出する

## Recommended First Slice

### Phase 1: Custom PSF Input

**目標**: 任意形状のマスクをボケ形状として使えるようにする。

- ハート、星、花びら、記号などの簡単なマスクを読めるようにする
- PSF を正規化して総エネルギーを保つ
- 開口サイズと回転を調整できるようにする

### Phase 2: FFT Convolution Core

**目標**: 大きいブラーを FFT ベースで処理する。

- `cv::dft` または同等の周波数処理経路を使う
- 画像と PSF を周波数空間で掛け合わせる
- `idft` 後に適切に正規化する

### Phase 3: Vintage Lens Modes

**目標**: レンズらしい癖を追加する。

- 二線ボケの縁強調
- レンズ傷やゴミ由来の乱れ
- 玉ボケの内部濃度差や周辺ハイライト

### Phase 4: Camera / Look Integration

**目標**: 実際のカメラ表現やプリセットと結びつける。

- aperture 設定と接続する
- ボケ形状プリセットを選べるようにする
- ラックフォーカスや被写界深度の表現に使う

## In Scope

- 任意マスクを PSF として扱う
- FFT ベースの convolution
- 大きいカーネルの高速化
- 二線ボケやレンズ汚れの表現
- camera / look preset との連携

## Out Of Scope

- 完全な光学レンズの物理厳密再現
- 3D ライトトレーシング級のボケ計算
- 単なる固定 iris のみの実装
- 小さいカーネル専用で FFT の旨味が出ない実装

## Implementation Notes

- PSF は必ず正規化し、明るさが破綻しないようにする
- エッジやアルファの扱いを雑にすると、ボケだけでなく画像全体が汚れる
- 小サイズでは spatial convolution、大サイズでは FFT のような分岐があると実用的
- `ApertureShapeBlur` は「ぼかす」だけでなく「レンズの個性を出す」ことが重要

## Success Criteria

- ハートや星の形で背景が自然にボケる
- 任意の汚れマスクがレンズの個性として見える
- 二線ボケや縁の明るさが視覚的に成立する
- 固定 iris のブラーよりも表現が豊かで、実用速度を目指せる
- カメラ / プリセット UI から扱いやすい

## Likely Touch Points

- [Artifact/src/Layer/ArtifactCameraLayer.cppm](X:/Dev/ArtifactStudio/Artifact/src/Layer/ArtifactCameraLayer.cppm)
- [Artifact/App/shaders/fft_512x512_c2c_CS.hlsl](X:/Dev/ArtifactStudio/Artifact/App/shaders/fft_512x512_c2c_CS.hlsl)
- [Artifact/App/shaders/ShaderInterop_FFTGenerator.h](X:/Dev/ArtifactStudio/Artifact/App/shaders/ShaderInterop_FFTGenerator.h)
- [Artifact/src/Widgets/ArtifactLooksPresetBrowser.cppm](X:/Dev/ArtifactStudio/Artifact/src/Widgets/ArtifactLooksPresetBrowser.cppm)
- [ArtifactCore/include/ImageProcessing/Distortion.ixx](X:/Dev/ArtifactStudio/ArtifactCore/include/ImageProcessing/Distortion.ixx)

## Related

- [docs/planned/MILESTONE_ANISOTROPIC_FLOW_BLUR_2026-06-13.md](X:/Dev/ArtifactStudio/docs/planned/MILESTONE_ANISOTROPIC_FLOW_BLUR_2026-06-13.md)
- [docs/planned/MILESTONE_REACTION_DIFFUSION_BLUR_2026-06-13.md](X:/Dev/ArtifactStudio/docs/planned/MILESTONE_REACTION_DIFFUSION_BLUR_2026-06-13.md)
