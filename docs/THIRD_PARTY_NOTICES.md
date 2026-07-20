# Third-Party Notices

ArtifactStudio は以下のオープンソースプロジェクトをアルゴリズム・設計の参考としており、そのライセンスを記載します。

---

## OpenToonz

- **プロジェクト**: OpenToonz (https://github.com/opentoonz/opentoonz)
- **ライセンス**: BSD 3-Clause License
- **Copyright**: Copyright (c) 2016-2026, DWANGO Co., Ltd. and respective contributors
- **参照内容**: クロマキーアルゴリズム、照明エフェクト、レンズグレア分光計算

```
Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:

1. Redistributions of source code must retain the above copyright notice,
   this list of conditions and the following disclaimer.

2. Redistributions in binary form must reproduce the above copyright
   notice, this list of conditions and the following disclaimer in the
   documentation and/or other materials provided with the distribution.

3. Neither the name of the copyright holder nor the names of its
   contributors may be used to endorse or promote products derived from
   this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
POSSIBILITY OF SUCH DAMAGE.


---

## bgfx

- **プロジェクト**: bgfx (https://github.com/bkaradzic/bgfx)
- **ライセンス**: BSD 2-Clause License
- **Copyright**: Copyright 2010-2026 Branimir Karadzic
- **参照内容**: HDR Bloom パイプライン、トーンマッピング (Reinhard)、シェーダー実装パターン

## Filament

- **プロジェクト**: Filament (https://github.com/google/filament)
- **ライセンス**: Apache License 2.0
- **Copyright**: Copyright (C) 2016 The Android Open Source Project
- **参照内容**: ポストプロセスパイプライン設計、DOF (CoCベース)、Bloom (Karis平均 + Kawaseフィルタ)

## Godot Engine

- **プロジェクト**: Godot Engine (https://github.com/godotengine/godot)
- **ライセンス**: MIT License
- **Copyright**: Copyright (c) 2014-present Godot Engine contributors
- **参照内容**: GTAO (SSAO)、SSIL、ボリュメトリックフォグ

## SPlisHSPlasH

- **プロジェクト**: SPlisHSPlasH (https://github.com/InteractiveComputerGraphics/SPlisHSPlasH)
- **ライセンス**: MIT License
- **参照内容**: SPH流体 (WCSPH/PCISPH/PBF/IISPH/DFSPH)、GPU近傍探索

## PositionBasedDynamics

- **プロジェクト**: PositionBasedDynamics (https://github.com/InteractiveComputerGraphics/PositionBasedDynamics)
- **ライセンス**: MIT License
- **参照内容**: PBD/XPBD/PBF 統一制約ソルバー、布・ロープ・剛体・流体シミュレーション

## Fusion

- **プロジェクト**: Fusion (https://github.com/Ninjajie/Fusion)
- **ライセンス**: MIT License
- **参照内容**: Unity Compute Shader による PBD布 + PBF流体の GPU 実装パターン

## blub

- **プロジェクト**: blub (https://github.com/Wumpf/blub)
- **ライセンス**: MIT License
- **参照内容**: APIC流体、WebGPU compute shader、GPU境界ボクセル化

## Ten-Minute-Physics

- **プロジェクト**: Ten-Minute-Physics-Unity (https://github.com/Habrador/Ten-Minute-Physics-Unity)
- **ライセンス**: MIT License
- **参照内容**: XPBD 物理チュートリアル全22種 (布・FLIP流体・Eulerian流体・剛体・空間ハッシュ)

## OceanFFT

- **プロジェクト**: OceanFFT (https://github.com/achalpandeyy/OceanFFT)
- **ライセンス**: MIT License
- **参照内容**: Stockham FFT による GPU 海洋波シミュレーション、Phillips スペクトル

## Wave-Particles-with-Interactive-Vortices

- **プロジェクト**: Wave-Particles-with-Interactive-Vortices (https://github.com/ACskyline/Wave-Particles-with-Interactive-Vortices)
- **ライセンス**: MIT License
- **参照内容**: DX12 Wave Particles + 渦 + フローマップによる川レンダリング (Naughty Dog Uncharted 手法)

## sparkle

- **プロジェクト**: sparkle (https://github.com/tcoppex/sparkle)
- **ライセンス**: MIT License
- **参照内容**: 完全 GPU パーティクルエンジン (Bitonic Sort、Curl Noise、3D ベクトル場)

## raylib-gpu-particles

- **プロジェクト**: raylib-gpu-particles (https://github.com/arceryz/raylib-gpu-particles)
- **ライセンス**: MIT License
- **参照内容**: GPU Compute Shader パーティクル (完全ドキュメント化、Instancing、ビルボード)

## GridFluidSim3D

- **プロジェクト**: GridFluidSim3D (https://github.com/rlguy/GridFluidSim3D)
- **ライセンス**: Zlib License
- **参照内容**: PIC/FLIP グリッドベース流体 (Bridson 教科書の完全実装)

## incremental_mpm

- **プロジェクト**: incremental_mpm (https://github.com/nialltl/incremental_mpm)
- **ライセンス**: MIT License
- **参照内容**: MLS-MPM による弾性体・流体シミュレーション (解説記事付き)

## Taichi

- **プロジェクト**: Taichi (https://github.com/taichi-dev/taichi)
- **ライセンス**: Apache License 2.0
- **参照内容**: MPM/SPH/PIC の GPU 実装リファレンス

## OpenVDB

- **プロジェクト**: OpenVDB (https://github.com/AcademySoftwareFoundation/openvdb)
- **ライセンス**: Apache License 2.0


## AMD FidelityFX SDK

- **プロジェクト**: FidelityFX-SDK (https://github.com/GPUOpen-LibrariesAndSDKs/FidelityFX-SDK)
- **ライセンス**: MIT License
- **Copyright**: Copyright (c) 2020-2026 Advanced Micro Devices, Inc.
- **参照内容**: CACAO (Compute AO), SSSR (確率的SSR), SPD (12MIP ダウンサンプラ), CAS (適応シャープニング)

## SaschaWillems Vulkan Examples

- **プロジェクト**: Vulkan (https://github.com/SaschaWillems/Vulkan)
- **ライセンス**: MIT License
- **参照内容**: Shadow Mapping, Cascaded Shadow Maps, PCF, PCSS, SSAO, Bloom, PBR, Deferred, Compute Shader

## TheRealMJP/Shadows

- **プロジェクト**: Shadows (https://github.com/TheRealMJP/Shadows)
- **ライセンス**: MIT License
- **参照内容**: CSM, 安定化CSM, PCF各種, VSM, EVSM, Moment Shadow Maps (解説記事付き)

## NVIDIA nvpro-samples

- **プロジェクト**: nvpro-samples (https://github.com/nvpro-samples)
- **ライセンス**: Apache License 2.0
- **参照内容**: Vulkan レイトレーシング, glTF PBR レンダラー, Gaussian Splatting


## OpenImageDenoise

- **プロジェクト**: oidn (https://github.com/RenderKit/oidn)
- **ライセンス**: Apache License 2.0
- **Copyright**: Copyright (c) Intel Corporation
- **参照内容**: AIデノイザー (CPU+CUDA+SYCL+HIP+Metal)。レイトレーシング/RAMプレビューのノイズ除去

## OpenTimelineIO

- **プロジェクト**: OpenTimelineIO (https://github.com/AcademySoftwareFoundation/OpenTimelineIO)
- **ライセンス**: Apache License 2.0
- **参照内容**: タイムライン相互運用の業界標準 (AAF/XML/FCPX互換)。タイムライン入出力の設計参考

## MaterialX

- **プロジェクト**: MaterialX (https://github.com/AcademySoftwareFoundation/MaterialX)
- **ライセンス**: Apache License 2.0
- **参照内容**: PBR マテリアル交換標準。3D レイヤーのマテリアル定義・インポートの参照


## OpenCV

- **プロジェクト**: OpenCV (https://github.com/opencv/opencv)
- **ライセンス**: Apache License 2.0
- **参照内容**: 画像フィルタリング、色変換、形態学演算、オプティカルフロー、インペインティング

## libvips

- **プロジェクト**: libvips (https://github.com/libvips/libvips)
- **ライセンス**: LGPL 2.1
- **参照内容**: 高速・低メモリ画像処理。需要駆動型パイプライン。リサイズ/合成/ICC/FFT/40+フォーマット

## Halide

- **プロジェクト**: Halide (https://github.com/halide/Halide)
- **ライセンス**: MIT License
- **参照内容**: 画像処理 DSL + JIT コンパイラ。アルゴリズムとスケジュールの分離。CPU/GPU 最適化

## FILM (Frame Interpolation)

- **プロジェクト**: frame-interpolation (https://github.com/google-research/frame-interpolation)
- **ライセンス**: Apache License 2.0
- **参照内容**: 大規模モーション対応フレーム補間。ECCV 2022。Google製


## Real-ESRGAN

- **プロジェクト**: Real-ESRGAN (https://github.com/xinntao/Real-ESRGAN)
- **ライセンス**: BSD 3-Clause License
- **参照内容**: 実世界向けブラインド超解像。ncnn Vulkan版で Python 不要の GPU 推論

## Anime4K

- **プロジェクト**: Anime4K (https://github.com/bloc97/Anime4K)
- **ライセンス**: MIT License
- **参照内容**: 純粋 GLSL シェーダーによるリアルタイム超解像。ML 不要。4K 60fps

## Anime4KCPP

- **プロジェクト**: Anime4KCPP (https://github.com/TianZerL/Anime4KCPP)
- **ライセンス**: MIT License
- **参照内容**: Anime4K の C++ 移植。OpenCV + GPU 対応。DLL/Lib 形式
- **参照内容**: 疎ボリュームデータ構造、煙・火・霧の表現

---

## ArtifactStudio 実装参照監査（2026-07-20）

以下は現在追加した shader / core contract の設計・アルゴリズム参照元である。現時点の追加ファイルは、各プロジェクトのソースコードを直接コピーしたものではなく、ArtifactStudio 用に独自実装した初期 pass である。

| 参照元 | ライセンス | ArtifactStudio での参照内容 |
|---|---|---|
| [Anime4K](https://github.com/bloc97/Anime4K) | MIT | edge-aware preview upscale の考え方 |
| [glsl-fast-gaussian-blur](https://github.com/Experience-Monks/glsl-fast-gaussian-blur) | MIT | separable Gaussian blur の構成 |
| [Unified-Universal-Blur](https://github.com/lukakldiashvili/Unified-Universal-Blur) | MIT | Dual Kawase blur の構成 |
| [FidelityFX-SDK](https://github.com/GPUOpen-LibrariesAndSDKs/FidelityFX-SDK) | MIT | SPD / CACAO の compute reduction・AO の考え方 |
| [FidelityFX-CACAO](https://github.com/GPUOpen-Effects/FidelityFX-CACAO) | MIT | screen-space AO の品質設計 |
| [TheRealMJP/Shadows](https://github.com/TheRealMJP/Shadows) | MIT | PCF shadow resolve の考え方 |
| [nvpro-samples](https://github.com/nvpro-samples) | Apache License 2.0 | ray tracing payload / miss / closest-hit の最小契約 |
| [Godot Engine](https://github.com/godotengine/godot) | MIT | GTAO / height fog の設計参考 |
| [OpenToonz](https://github.com/opentoonz/opentoonz) | BSD 3-Clause | RGB/HSV keyer の設計参考 |
| [Malt](https://github.com/bnpr/Malt) | MIT | NPR toon lighting の設計参考 |
| [Google FILM](https://github.com/google-research/frame-interpolation) | Apache License 2.0 | motion-aware frame interpolation の設計参考 |
| [FastNoiseLite](https://github.com/Auburn/FastNoiseLite) | MIT | NoiseField / noise family の設計参考 |
| [PositionBasedDynamics](https://github.com/InteractiveComputerGraphics/PositionBasedDynamics) | MIT | SoftBody / constraint solver の設計参考 |
| [OpenTimelineIO](https://github.com/AcademySoftwareFoundation/OpenTimelineIO) | Apache License 2.0 | NLE timeline interchange schema の設計参考 |

### ライセンス上の扱い

- 参照のみのものは、ArtifactStudio のバイナリへ自動的に取り込まれない。
- 今後、外部ソースコード・ヘッダ・モデル・shader を直接取り込む場合は、対象リポジトリの LICENSE と NOTICE、著作権表示、配布条件を個別に確認する。
- [Dual Kawase Demo](https://github.com/tryone144/dual-kawase-demo) はライセンス表記を確認できていないため、現時点では概念参照のみとし、コードは取り込まない。
