# 参照リポジトリ一覧

> ArtifactStudio の設計・実装の参考とするオープンソースプロジェクト
> ライセンス詳細: `docs/THIRD_PARTY_NOTICES.md`

---

## コンポジット・アニメーションツール

| リポジトリ | ライセンス | 学ぶべきこと |
|---|---|---|
| [OpenToonz](https://github.com/opentoonz/opentoonz) | BSD 3-Clause | **クロマキー** (RGB/HSV Key), **照明** (TargetSpot/Raylit/BodyHighlight/Backlit), **レンズグレア** (34波長分光+FFT), **TangentFlow** (Sobelベクトル場), **Fractal Noise** (AE互換), **ColorFX RubberDeform** (物理ベース変形), **FXキャッシュキー設計** (getAlias再帰的キー生成), ShaderFx 枠組み |
| [Natron](https://github.com/NatronGitHub/Natron) | GPL 2.0 | OFX プラグインホスト実装、ノードベースコンポジットエンジン |
| [Olive](https://github.com/olive-editor/olive) | GPL 3.0 | ノードコンポジット+NLEのハイブリッド設計 |

---

## ゲームエンジン（3D レンダリング技法）

| リポジトリ | ⭐ | 言語 | ライセンス | 学ぶべきこと |
|---|---|---|---|---|
| [SaschaWillems/Vulkan](https://github.com/SaschaWillems/Vulkan) | 12.1k | C++/GLSL | **MIT** | **Vulkan 全技法の教科書**。Shadow Mapping、Cascaded Shadow Maps、PCF、PCSS、SSAO、Bloom、PBR、Deferred、Compute Shader。全サンプルにソース完備 |
| [FidelityFX-SDK](https://github.com/GPUOpen-LibrariesAndSDKs/FidelityFX-SDK) | 1.7k | C++/HLSL | **MIT** | **CACAO** (Compute AO)、**SSSR** (確率的SSR)、**SPD** (1pass 12MIPダウンサンプル)、CAS (適応シャープニング)、FSR (超解像)。DX12+Vulkan対応 |
| [FidelityFX-CACAO](https://github.com/GPUOpen-Effects/FidelityFX-CACAO) | 155 | C++/HLSL | **MIT** | **AMD CACAO**: Intel ASSAO の改良版。Compute Shader による高速適応AO。RDNA最適化。サンプル付き |
| [TheRealMJP/Shadows](https://github.com/TheRealMJP/Shadows) | 1k | C++/HLSL | **MIT** | **シャドウ技法のデパート**。CSM、安定化CSM、PCF各種、VSM、**EVSM**、Moment SM。**解説記事付き** |
| [nvpro-samples](https://github.com/nvpro-samples) (NVIDIA) | — | C++/GLSL | Apache 2.0 | **Vulkanレイトレ** (`vk_raytracing_tutorial_KHR` 1.7k⭐)、glTF PBRレンダラー、Gaussian Splatting。68サンプル |
| [bgfx](https://github.com/bkaradzic/bgfx) | 17.3k | C++/GLSL | **BSD 2-Clause** | HDR Bloom (`09-hdr`)、Reinhard TM、Shadow Maps (`16-shadowmaps`)、GPUドリブン (`37`)、マルチバックエンド |
| [Filament](https://github.com/google/filament) | 20.3k | C++/GLSL | Apache 2.0 | DOF (CoC+リング)、Bloom (最大12段)、SSAO、TAA、ACES TM、PBR |
| [Godot Engine](https://github.com/godotengine/godot) | 114k | C++/GLSL | MIT | GTAO (SSAO+SSIL)、Volumetric Fog、Bokeh DOF、TAA |
| [Wicked Engine](https://github.com/turanszkij/WickedEngine) | 7.1k | C++/HLSL | MIT | **DX12 ネイティブ**。GPUドリブンレンダリング、レイトレ |
| [The Forge](https://github.com/ConfettiFX/The-Forge) | 5.6k | C++/HLSL | Apache 2.0 | DX12/Vulkan/Metal 抽象化。Hi-Z SSR、TAA

---

## パーティクル・流体シミュレーション

| リポジトリ | ⭐ | 言語 | ライセンス | 学ぶべきこと |
|---|---|---|---|---|
| [FluidX3D](https://github.com/ProjectPhysX/FluidX3D) | 5.2k | C++/OpenCL | 非商用無料 | **Lattice Boltzmann法** による流体。全GPU/CPU対応。**世界最速のLBM実装**。ベンチマーク充実。数値流体のGPU最適化パターンの宝庫 |
| [SPlisHSPlasH](https://github.com/InteractiveComputerGraphics/SPlisHSPlasH) | 1.9k | C++ | **MIT** | **SPH流体の決定版**。WCSPH/PCISPH/PBF/IISPH/DFSPH の全圧力ソルバー。粘性、表面張力、渦度、混相流、剛体連成。**GPU近傍探索**。Pythonバインド |
| [Blender-FLIP-Fluids](https://github.com/rlguy/Blender-FLIP-Fluids) | 1.9k | C++ | GPL | プロダクション品質のFLIP液体。Blender addonだがC++コア部分は学習価値あり |
| [PositionBasedDynamics](https://github.com/InteractiveComputerGraphics/PositionBasedDynamics) | 2.2k | C++ | **MIT** | PBD/XPBD/PBF。布・ロープ・剛体・流体の統一制約ソルバー。GPU並列化と抜群の相性 |
| [Fusion](https://github.com/Ninjajie/Fusion) | 472 | C#/HLSL | **MIT** | **Unity Compute Shader による PBD布+PBF流体**。CPU対GPUベンチマーク付き。100k粒子の性能データあり |
| [blub](https://github.com/Wumpf/blub) | 483 | Rust/WebGPU | **MIT** | **APIC流体** (PIC/FLIPの改良)。WebGPU compute shader。球の遠近補正レンダリング。GPU境界ボクセル化 |
| [Ten-Minute-Physics](https://github.com/Habrador/Ten-Minute-Physics-Unity) | 354 | C# | **MIT** | **XPBDで22種の物理チュートリアル**。布、流体(FLIP+Eulerian)、剛体、ジョイント、空間ハッシュ。学習に最適 |
| [GridFluidSim3D](https://github.com/rlguy/GridFluidSim3D) | 828 | C++11 | **Zlib** | PIC/FLIP (Bridson教科書の完全実装)。PCGソルバー。Marching Cubes |
| [incremental_mpm](https://github.com/nialltl/incremental_mpm) | 396 | C#/Unity | **MIT** | MLS-MPM。**解説記事付き**。弾性体+流体を数百行 |
| [Taichi](https://github.com/taichi-dev/taichi) | 28.3k | Python/C++ | Apache 2.0 | MPM/SPH/PIC。**MPM流体88行のPythonコード**が即移植参考に |
| [OpenVDB](https://github.com/AcademySoftwareFoundation/openvdb) | 3.3k | C++ | Apache 2.0 | 疎ボリューム。煙/火/霧。NanoVDBでGPU対応 |
| [OceanFFT](https://github.com/achalpandeyy/OceanFFT) | 136 | C++/GLSL | **MIT** | **Stockham FFT** によるGPU海洋波。Compute Shader実装。Phillipsスペクトル |
| [Wave-Particles-River](https://github.com/ACskyline/Wave-Particles-with-Interactive-Vortices) | 290 | C++/HLSL | **MIT** | **DX12 Wave Particles** + 渦+フローマップ。Naughty DogのUncharted手法の再現実装 |
| [sparkle](https://github.com/tcoppex/sparkle) | 196 | C++14/GLSL | **MIT** | 完全GPUパーティクルエンジン。Bitonic Sort、Curl Noise、3Dベクトル場。SquareEnix Agni's Philosophyに触発 |
| [raylib-gpu-particles](https://github.com/arceryz/raylib-gpu-particles) | 186 | C/GLSL | **MIT** | **完全ドキュメント化されたGPUパーティクル**。Compute Shader、GPU Instancing、ビルボード。学習用に最適 |
| [waves](https://github.com/dli/waves) | 1.1k | WebGL/JS | MIT | FFT海洋波。シェーダー完結 |

---


## カラーマネジメント / 画像I/O / 相互運用

| リポジトリ | ⭐ | 言語 | ライセンス | 学ぶべきこと |
|---|---|---|---|---|
| [OpenColorIO](https://github.com/AcademySoftwareFoundation/OpenColorIO) | — | C++ | BSD 3-Clause | カラーマネジメントの業界標準 |
| [OpenImageIO](https://github.com/AcademySoftwareFoundation/OpenImageIO) | — | C++ | BSD 3-Clause | 画像I/O、タイルキャッシュ、MIPmap |
| [OpenEXR](https://github.com/AcademySoftwareFoundation/openexr) | 1.8k | C++ | BSD 3-Clause | HDR画像フォーマット |
| [OpenImageDenoise](https://github.com/RenderKit/oidn) | 2.1k | C++ | Apache 2.0 | **AIデノイザー**。CPU+CUDA+SYCL+HIP+Metal。レイトレ/RAMプレビューのノイズ除去に |
| [OpenTimelineIO](https://github.com/AcademySoftwareFoundation/OpenTimelineIO) | 1.9k | C++/Python | Apache 2.0 | **タイムライン相互運用の業界標準**。AAF/XML/FCPX互換。ArtifactStudioのタイムライン入出力の設計参考 |
| [MaterialX](https://github.com/AcademySoftwareFoundation/MaterialX) | 2.2k | C++/GLSL | Apache 2.0 | **PBRマテリアル交換標準**。3Dレイヤーのマテリアル定義・インポートの参照 |



## VFX ツール・リファレンス

| リポジトリ | ⭐ | 言語 | ライセンス | 学ぶべきこと |
|---|---|---|---|---|
| [DJV](https://github.com/grizzlypeak3d/DJV) | 1.1k | C++ | BSD系 | プロ向けEXR/DPX画像シーケンスビューア。フレーム正確な比較再生。VFXレビューUIの参考 |
| [EveryRay-Rendering-Engine](https://github.com/steaklive/EveryRay-Rendering-Engine) | 759 | C++ | MIT | DX11/DX12。**ボリュメトリックフォグ/クラウド**、VCT、PBR、パララックス |
| [zeno](https://github.com/zenustech/zeno) | 1.4k | C++ | — | ノードベースのシミュレーション＆レンダリングエンジン。データフロー可視化 |

## 理論・研究実装（アルゴリズム参照）

| リポジトリ | ⭐ | 言語 | ライセンス | 学ぶべきこと |
|---|---|---|---|---|
| [yocto-gl](https://github.com/xelatihy/yocto-gl) | 2.9k | C++17 | **MIT** | **PBRの教科書的実装**。パストレーシング、BSDF全種、画像I/O、シェーディング、形状処理。全コードが小ライブラリ単位で完結。教育用に最適 |
| [SpartanEngine](https://github.com/PanosK92/SpartanEngine) | 3.1k | C++/HLSL | **MIT** | **Bindless GPUドリブンレンダラー**。リアルタイムパストレースGI、HWレイトレ、**ReSTIR GI**、大気散乱、TAA。1人で10年。Vulkan |
| [LuxGI](https://github.com/flwmxd/LuxGI) | 340 | C++/GLSL | — | **DDGI ハイブリッドGI**。レイトレーシング+SDFトレーシングの両対応。Surface Cache。SVGFデノイザー |
| [GLSL-PathTracer](https://github.com/knightcrawler25/GLSL-PathTracer) | 2.1k | C++/GLSL | — | **GPUパストレーサー**。Disney BSDF、BVH、NEE。OpenGL学習用 |
| [lighthouse2](https://github.com/jbikker/lighthouse2) | 870 | C++/CUDA | Apache 2.0 | リアルタイムレイトレーシングフレームワーク。OptiX+SVGF |
| [vk_mini_path_tracer](https://github.com/nvpro-samples/vk_mini_path_tracer) | 1.3k | C++/GLSL | Apache 2.0 | **300行のVulkanパストレーサー**。NVIDIA公式。入門に最適 |
| [LuisaRender](https://github.com/LuisaGroup/LuisaRender) | 610 | C++ | Apache 2.0 | 高パフォーマンスクロスプラットフォームレンダラー。ISP+CUDA+Metal+DX。SIGGRAPH Asia 2022 |
| [SMAA](https://github.com/iryoku/smaa) | 1.1k | HLSL/GLSL | **MIT** | **業界標準のアンチエイリアス**。サブピクセル対応+パターン検出LUT。DX9-11+OpenGL |

## NPR / スタイライズドレンダリング

| リポジトリ | ⭐ | 言語 | ライセンス | 学ぶべきこと |
|---|---|---|---|---|
| [Malt](https://github.com/bnpr/Malt) | 1.1k | Python/GLSL | **MIT** | **NPRレンダリングフレームワーク**。Blender統合。Pythonレンダーパイプライン。GLSL自動ノード生成 |
| [Blender-miHoYo-Shaders](https://github.com/festivities/Blender-miHoYo-Shaders) | 1.1k | GLSL | — | 原神風トゥーンシェーディングのBlender実装。スタイライズドレンダリングの事例研究 |
| [JTRP](https://github.com/JasonMa0012/JTRP) | 2.2k | C#/HLSL | — | Unity HDRP トゥーンシェーディングパイプライン |

## 大気散乱 / 空

| リポジトリ | ⭐ | 言語 | ライセンス | 学ぶべきこと |
|---|---|---|---|---|
| [Brunetons-Atmospheric-Scatter](https://github.com/Scrawk/Brunetons-Atmospheric-Scatter) | 62 | C#/HLSL | MIT | **Brunetonの事前計算大気散乱**のUnity移植。多重散乱の3DテクスチャLUT化。ポストエフェクト応用 |





## 超解像 / アップスケーリング

| リポジトリ | ⭐ | 言語 | ライセンス | 学ぶべきこと |
|---|---|---|---|---|
| [Real-ESRGAN](https://github.com/xinntao/Real-ESRGAN) | 36.2k | Python | **BSD 3-Clause** | **実用的な画像超解像の決定版**。純粋合成データで訓練した実世界向けブラインド超解像。**ncnn Vulkan版でGPUネイティブ動作可能**。一般/アニメ/顔補正の複数モデル |
| [Anime4K](https://github.com/bloc97/Anime4K) | 21.2k | **GLSL** | **MIT** | **リアルタイム超解像**。**純粋シェーダーのみ**でML不要。4K 60fps。プッシュ定数ベース。**ArtifactStudioのプレビューアップスケーラとして即採用可能** |
| [Anime4KCPP](https://github.com/TianZerL/Anime4KCPP) | — | **C++** | MIT | **Anime4KのC++移植**。OpenCV+GPU対応。DLL/Lib形式。**ArtifactStudioへの統合が最も容易** |
| [nunif](https://github.com/nagadomi/nunif) | 3.3k | Python | **MIT** | waifu2x最新版。2D→3D立体変換、ビデオ対応。ncnn Vulkanあり |
| [Real-ESRGAN-ncnn-vulkan](https://github.com/xinntao/Real-ESRGAN-ncnn-vulkan) | — | C++ | BSD 3-Clause | **Real-ESRGANのVulkanポータブル版**。Python不要。Intel/AMD/NVIDIA全GPU対応 |

## OpenCV (画像処理全般 — 再掲)

| リポジトリ | ⭐ | 言語 | ライセンス | 学ぶべきこと |
|---|---|---|---|---|
| [OpenCV](https://github.com/opencv/opencv) | 90k | C++ | Apache 2.0 | `imgproc` (フィルタ/色変換/形態学)、`photo` (インペインティング/デノイズ)、**`superres`** (超解像アルゴリズム)、**`dnn_superres`** (DNNベース超解像)、`cudaarithm` (GPU演算) |
| [libvips](https://github.com/libvips/libvips) | 11.5k | C | LGPL 2.1 | 高速・低メモリ。需要駆動型。リサイズ/合成/ICC/FFT。40+フォーマット。サムネイル生成に最適 |
| [Halide](https://github.com/halide/Halide) | 6.6k | C++ | **MIT** | 画像処理DSL+JITコンパイラ。アルゴリズムとスケジュールの分離。CPU/GPU最適化。Google/Adobe使用 |

## オプティカルフロー / モーション推定

| リポジトリ | ⭐ | 言語 | ライセンス | 学ぶべきこと |
|---|---|---|---|---|
| [FILM](https://github.com/google-research/frame-interpolation) | 3.1k | Python | Apache 2.0 | **フレーム補間**。大規模モーション対応。ECCV 2022。タイムリマップ品質向上に |

## インペインティング

| リポジトリ | ⭐ | 言語 | ライセンス | 学ぶべきこと |
|---|---|---|---|---|
| [resynthesizer](https://github.com/bootchk/resynthesizer) | 1.8k | C | GPL 3.0 | 古典的テクスチャ合成+インペインティング。AI不要のアルゴリズム |
---


## 高品質ブラー / 高速ブラー

| リポジトリ | ⭐ | 言語 | ライセンス | 学ぶべきこと |
|---|---|---|---|---|
| [glsl-fast-gaussian-blur](https://github.com/Experience-Monks/glsl-fast-gaussian-blur) | 746 | **GLSL** | **MIT** | **線形サンプリングの最適化トリック**。2tapで4ピクセル取得。5/9/13tapの3段階。可分離1Dを2pass。**Bloom用ブラーの最小実装** |
| [Unified-Universal-Blur](https://github.com/lukakldiashvili/Unified-Universal-Blur) | 959 | C#/HLSL | MIT | Unity向け **Kawase Blur** 実装。URP/RenderGraph対応。UIブラーとして多用途 |
| [Super-Blur](https://github.com/PavelDoGreat/Super-Blur) | 1.1k | HLSL | MIT | Unity向け高速ガウシアンブラー。UI/画面全体 |
| [Dual Kawase Demo](https://github.com/tryone144/dual-kawase-demo) | — | Rust/GLSL | — | Kawaseブラーのパラメータ検証用デモ。**アルゴリズム学習に最適** |

### ブラーアルゴリズム比較

| 方式 | 品質 | 速度 | 大半径 | 用途 |
|---|---|---|---|---|
| Box Blur | ★☆☆ | ★★★ | ★★☆ | 最低品質・マスク生成 |
| **Separable Gaussian (9tap)** | ★★★ | ★★☆ | ★★☆ | **標準。半径∝タップ数** |
| **Kawase / Dual Kawase** | ★★☆ | ★★★ | ★★★ | **大半径ブラー最速。Bloomに最適** |
| **AMD SPD** | — | ★★★ | — | **12MIP 1pass**。ダウンスケール専用。Bloomの前段に |
| Bilateral Blur | ★★★ | ★☆☆ | ★☆☆ | エッジ保存。SSAOデノイズ用 |

### Kawase Blur の原理
```
半径に依存せず、反復回数で品質を制御:
Pass 1: 元画像 → 1/2ダウンサンプル + 小ブラー
Pass 2: 1/2 → 1/4ダウンサンプル + 小ブラー
Pass 3: 1/4 → 1/8...
... → アップサンプル合成で再構成

各passは固定サイズの小ブラーなので、半径が大きくてもコスト一定。
```

### FidelityFX SPD (Single Pass Downsampler)
> 参照: [GPUOpen-Effects/FidelityFX](https://github.com/GPUOpen-Effects/FidelityFX)
- **12 MIP レベルを1 Compute Shader パスで生成**
- RDNA アーキテクチャ最適化（ウェーブフロント活用）
- **Bloom ピラミッド生成の最速解**
- MIT ライセンス
- HLSL + DX12/Vulkan

## ArtifactStudio 実装ドキュメントマップ

| カテゴリ | 参照ドキュメント |
|---|---|
| OpenToonz 全般 | `docs/OT_IMPLEMENTATION_REFERENCE.md` |
| 3D レンダリング技法 | `docs/GAME_ENGINE_RENDERING_REFERENCE.md` |
| Bloom 詳細 | `docs/IMPL_BLOOM_HDR.md` |
| DOF 詳細 | `docs/IMPL_DOF.md` |
| トーンマッピング 詳細 | `docs/IMPL_TONE_MAPPING.md` |
| SSAO 詳細 | `docs/IMPL_SSAO.md` |
| パーティクル・流体 | `docs/PARTICLE_FLUID_REFERENCE.md` |
| TangentFlow マイルストーン | `docs/MILESTONE_TANGENTFLOW_VECTOR_FIELD.md` |
| ColorFX Deform マイルストーン | `docs/MILESTONE_COLORFX_DEFORM.md` |
| ライセンス一覧 | `docs/THIRD_PARTY_NOTICES.md` |


## 高速・高品質アルゴリズム

### ブラー系

| アルゴリズム | 計算量 | 品質 | 参照実装 |
|---|---|---|---|
| Stack Blur | O(N) 半径非依存 | ★★☆ | Photoshop の採用アルゴリズム。スライディングウィンドウ |
| Separable Gaussian 9tap | O(18N) | ★★★ | [glsl-fast-gaussian-blur](https://github.com/Experience-Monks/glsl-fast-gaussian-blur) (MIT) |
| **Kawase / Dual Kawase** | O(log N) | ★★☆ | [Unified-Universal-Blur](https://github.com/lukakldiashvili/Unified-Universal-Blur) (MIT)。大半径Bloom最速 |
| AMD SPD | 1 pass | — | [FidelityFX](https://github.com/GPUOpen-Effects/FidelityFX) (MIT)。12MIP を単一 Compute Shader |

### エッジ保存フィルタ

| アルゴリズム | 計算量 | 品質 | 参照実装 |
|---|---|---|---|
| **Guided Filter** (He 2013) | O(N) | ★★★★ | [guided-filter](https://github.com/atilimcetin/guided-filter)。Photoshop採用。ボックスフィルタ2回 |
| **Domain Transform** (Gastal 2011) | O(N) | ★★★★ | [domain_transform](https://github.com/suyukun/domain_transform)。1D再帰でさらに高速 |
| Bilateral Filter | O(N·r²) | ★★★ | Bilateral Grid / Permutohedral Lattice で高速化可 |
| **L0 Smoothing** (Xu 2011) | 反復 O(N) | ★★★★★ | 芸術的仕上がり。エッジだけ残してフラット化 |

### 高速アルゴリズム（GPU特化）

| アルゴリズム | 速度 | 用途 |
|---|---|---|
| **Jump Flooding (JFA)** | O(log N) | 距離変換、SDF、ボロノイ図。log₂(N) パス |
| Bitonic Sort | O(n log²n) GPU | パーティクル深度ソート。sparkle も使用 |
| FFT Ocean (Tessendorf) | O(n log n) | 海洋波。OceanFFT がリファレンス |
| Radix Sort | O(n) GPU | パーティクル/透明度ソート。AMD FidelityFX Parallel Sort |



## オーディオ / DAW / DSP

| リポジトリ | ⭐ | 言語 | ライセンス | 学ぶべきこと |
|---|---|---|---|---|
| [LMMS](https://github.com/LMMS/lmms) | 10.1k | C++ | GPLv2 | **クロスプラットフォーム音楽制作**。MIDI、VST、LV2、サンプラー、ミキサー。ミキサーUIの設計参考 |
| [Ardour](https://github.com/Ardour/ardour) | 5.1k | C++ | GPLv2 | **プロフェッショナルDAW**。VST/LV2/AU、JACK、MIDI、**ビデオタイムライン同期**。オーディオエンジン設計の模範 |
| [zrythm](https://github.com/zrythm/zrythm) | 3k | **C++23/Qt6** | AGPLv3 | **Qt6 + C++23 DAW**。ArtifactStudio と同スタック！VST/LV2/CLAP。ミキサーUI、オートメーション |
| [giada](https://github.com/monocasual/giada) | 2.1k | C++23/JUCE | GPLv3 | ハードコアループマシン。VST3。軽量でコードが読みやすい |
| [tracktion_engine](https://github.com/Tracktion/tracktion_engine) | 1.4k | C++/JUCE | GPLv3/商用 | **組み込み可能なDAWエンジン**。マルチトラック編集、MIDI、VST/AU、オートメーション、タイムストレッチ |

### オーディオプラグイン規格 / DSP

| リポジトリ | ⭐ | 言語 | ライセンス | 学ぶべきこと |
|---|---|---|---|---|
| [**CLAP**](https://github.com/free-audio/clap) | 2.3k | C | **MIT** | **新オープンオーディオプラグイン標準**。VSTの代替。**ロイヤリティフリー**。ホスト+プラグイン両方MIT。拡張性高い |
| [JUCE](https://github.com/juce-framework/JUCE) | 8.7k | C++ | GPLv3/Apache 2.0/商用 | **C++オーディオアプリケーションのデファクトスタンダード**。VST/VST3/AU/AAX/LV2全対応。DSPモジュール充実。デュアルライセンス |
| [Vaporizer2](https://github.com/VASTDynamics/Vaporizer2) | 573 | C++ | GPLv3 | ウェーブテーブル/加算/減算合成。VST3/AU/LV2。シンセサイザーのDSPリファレンス |
| [SoundFlow](https://github.com/LSXPrime/SoundFlow) | 495 | C# | — | ハイパフォーマンス .NET オーディオエンジン。マルチトラック、リアルタイムDSP、SIMD |

### ソース分離（音楽→ステム）

| リポジトリ | ⭐ | 言語 | ライセンス | 学ぶべきこと |
|---|---|---|---|---|
| [spleeter](https://github.com/deezer/spleeter) | 28.3k | Python | MIT | **音楽ソース分離**。ボーカル/ドラム/ベース/その他に分離。**動画の音声処理に応用可** |


## 独自エフェクト / 他ツールにない表現

| リポジトリ | ⭐ | 言語 | ライセンス | 何が新しいか |
|---|---|---|---|---|
| [Graphite](https://github.com/GraphiteEditor/Graphite) | 26.6k | Rust | Apache 2.0 | ノードベースプロシージャル2Dエンジン。モーショングラフィックス・VFXコンポジットも計画。**ArtifactStudio と設計思想が最も近い** |
| [WaveFunctionCollapse](https://github.com/mxgmn/WaveFunctionCollapse) | 25.2k | C# | **MIT** | 量子力学のアイデアでテクスチャ生成。1枚の見本→無限のバリエーション |
| [MarkovJunior](https://github.com/mxgmn/MarkovJunior) | 8.1k | C# | **MIT** | パターンマッチング確率的画像生成。153サンプル |
| [FastNoiseLite](https://github.com/Auburn/FastNoiseLite) | 3.5k | C++/GLSL他 | **MIT** | 最速ノイズライブラリ。Simplex/Perlin/Voronoi/Cellular。**エフェクトノイズ基盤** |
| [ntsc-rs](https://github.com/ntsc-rs/ntsc-rs) | 2.4k | Rust | MIT | VHS/CRT 劣化エフェクト。After Effects/OFX プラグイン版あり |
| [Blotter](https://github.com/bradley/Blotter) | 3.1k | JS/GLSL | — | 非日常的なテキスト変形。液体・歪み・グリッチ |
| [fishdraw](https://github.com/LingDong-/fishdraw) | 2.3k | JS | — | プロシージャルな手描き風生成 |
| [morphogenesis-resources](https://github.com/jasonwebb/morphogenesis-resources) | 2.3k | リンク集 | — | デジタル形態形成の論文/コード/作品集 |
| [bauble](https://github.com/ianthehenry/bauble) | 588 | Janet/GLSL | — | SDF 数式→美しい3D形状 |
| [curv](https://github.com/curv3d/curv) | 1.2k | C++ | Apache 2.0 | 数学で3Dアート。SDF+CSG+関数表現 |
| [quadim](https://github.com/eternal-io/quadim) | 141 | Rust | MIT | クアッドツリー画像圧縮/スタイライズ。100FPS |

### After Effects にないエフェクト案

| 効果 | アルゴリズム | 元ネタ |
|---|---|---|
| 例からテクスチャ自動生成 | WaveFunctionCollapse | 1枚→シームレス無限テクスチャ |
| 細胞/有機的パターン | Cellular Automata + FastNoiseLite | 成長・分裂する模様 |
| 手描き風レンダリング | fishdraw プロシージャル描画 | 線画・水彩風 |
| VHS/CRT 劣化 | ntsc-rs | グリッチ・走査線・色にじみ |
| 液体テキスト | Blotter GLSL | テキストが溶ける/歪む |
| 数式→3D形状 | SDF Ray Marching (curv, bauble) | 美しい幾何学形状 |
| クアッドツリー風 | quadim | 適応的分割でピクセルアート風 |
| [demucs](https://github.com/facebookresearch/demucs) | — | Python | MIT | Meta製。高品質ソース分離。spleeterより高精度 |



## 他業界・異分野から取り入れられる技法

### 印刷・出版
| 技法 | 何か | ArtifactStudio 応用 |
|---|---|---|
| AM/FM 網点 (Halftone) | ドットサイズ/密度で濃淡表現 | 画像→網点エフェクト |
| 色分解+スクリーン角度 | CMYK 4版の角度付き網点 | ポスタリゼーション+網点合成 |
| トラッピング | 版境界の重ねによる白ズレ防止 | レイヤー境界の自動補完 |
| ダブルトーン | 2色で濃淡表現 | モノクロ→2色変換 |

### 織物・テキスタイル
| 技法 | 何か | ArtifactStudio 応用 |
|---|---|---|
| ジャカード織り | 糸の上下で絵柄表現 | クロスハッチ風テクスチャ |
| 刺繍シミュレーション | 糸の質感・方向・密度 | 手芸風フィルタ |

### 建築・パラメトリックデザイン
| 技法 | 何か | ArtifactStudio 応用 |
|---|---|---|
| ボロノイファサード | 有機的パターン分割 | ガラス割れ・細胞分裂 |
| 空間充填曲線 | Hilbert, Peano 曲線 | トランジション経路生成 |

### 生物学
| 技法 | 何か | ArtifactStudio 応用 |


## 宇宙 / 星空 / 惑星生成

| リポジトリ | ⭐ | 言語 | ライセンス | 何ができるか |
|---|---|---|---|---|
| [ETEngine](https://github.com/Illation/ETEngine) | 805 | C++14/GLSL | **MIT** | 宇宙シム特化のリアルタイム3Dエンジン。**惑星レンダリング+大気散乱**。Deferred PBR。ECS。**卒論がリアルタイム惑星レンダリング** |
| [OpenSpace](https://github.com/OpenSpace/OpenSpace) | 1.2k | **C++23/Qt6**/Lua | **MIT** | **NASA 公式天体可視化**。Digital Universe 星カタログ。高解像度惑星画像。全天周ドーム対応。**ArtifactStudio と C++23+Qt6 で同スタック** |
| [Planet-Generator (Godot)](https://github.com/Hoimar/Planet-Generator) | 255 | GDScript | MIT | Godot向けプロシージャル惑星生成。**地形LOD**。Addon形式。大気散乱 |
| [planet_heightmap_generation](https://github.com/raguilar011095/planet_heightmap_generation) | 90 | JS/WebGL | MIT | 地殻変動+侵食+気候の**物理ベース惑星生成**。Three.js。地球科学に基づく |
| [Accrete.js](https://github.com/tmanderson/Accrete.js) | 40 | JS | — | **惑星系形成シミュレーション**。Carl SaganのStarGen/Accreteアルゴリズム移植。恒星+惑星+衛星の自動生成 |
| [Solar-Wanderer](https://github.com/hyqzz/Solar-Wanderer) | 664 | JS/Three.js | — | **1:1実寸太陽系**。NASA JPL 天体暦。太陽表面〜10万AUのオールト雲まで |
| [Torben Mogensen Planet Generator](https://github.com/MagicalDrizzle/planet-generator) | — | C | — | **古典的テクスチャベース惑星生成**。多数の移植版あり。標高・気温・降水・バイオーム |

### 恒星・星空背景の生成手法（アルゴリズム）

| 手法 | 概要 | 実装のヒント |
|---|---|---|
| H-R図ベース分布 | ヘルツシュプルング・ラッセル図に従った星の色温度と絶対等級のランダム分布 | テーブル駆動。O/B/A/F/G/K/Mスペクトル型ごとに確率重み付け |
| グレア/大気差 | 明るい星ほど大きく、色収差で赤〜青の滲み | 星の輝度に比例したGaussianスポット+波長別スケール |
| 天の川背景 | 銀河円盤に沿った星密度の高い帯 | 銀経に沿ったsin波密度変調 |
| 星雲パターン | パーリンノイズ+FBMでガス雲 | 複数オクターブのノイズを加算合成。色は赤(Hα)/青(反射) |
| Twinkling（瞬き） | 大気の揺らぎによる星の明滅 | 時間+位置依存のPerlinノイズで輝度変調。明るい星ほど少ない |
| Nebula Cube | 星雲を3Dテクスチャとしてベイク | ノイズボリュームをレイマーチング。OpenSpaceが採用 |

### 惑星レンダリングの構成要素

| 要素 | 手法 | 参照 |
|---|---|---|
| 標高マップ生成 | Simplexノイズ多重 + 地殻変動シミュレーション | Planet-Generator, Torben Mogensen |
| 大気散乱 | Bruneton の事前計算大気散乱 / O'Neil | ETEngine, OpenSpace |
| 海 | 水面反射 + 波 (Gerstner/FFT) | OceanFFT 参照 |
| 雲 | ノイズテクスチャ + 影 | ボリュームレイマーチング |
| 都市光 (夜側) | 人口密度マップ + 光源分布 | NASA Black Marble データ |
| リング (土星型) | テクスチャまたはパーティクル円盤 | — |
| 衛星軌道 | ケプラー軌道要素 | Solar-Wanderer 参照 |

### ArtifactStudio での星空背景ジェネレータ設計案

```
┌─ 星分布生成 ─────────────────────────────┐
│ H-R図テーブル + ランダム位置 + 銀河密度関数 │ → 静止星空テクスチャ（1回生成）
│ + Flare/回折スパイク（明るい星のみ）       │


## Froxel Volumetric Fog

| リポジトリ | ⭐ | 言語 | ライセンス | 何ができるか |
|---|---|---|---|---|
| [diharaw/volumetric-fog](https://github.com/diharaw/volumetric-fog) | 138 | C++/GLSL | **MIT** | **Froxel（フラスタム整列ボクセルグリッド）+ Compute Shader によるボリュメトリックフォグ**。OpenGL 4.5。SIGGRAPH 2014 (Wronski) + Frostbite 論文に基づく決定版リファレンス実装 |
| [EveryRay-Rendering-Engine](https://github.com/steaklive/EveryRay-Rendering-Engine) | 759 | C++/HLSL | MIT | Voxel Cone Tracing + ボリュメトリックフォグ/クラウド。DX11/DX12。PBR + Deferred |

### Froxel の参照論文

| 論文 | 著者 | 概要 |
|---|---|---|
| [Volumetric Fog: Unified, compute shader based solution to atmospheric scattering](https://advances.realtimerendering.com/s2014/) | Bart Wronski (Assassin's Creed IV) | **Froxelの発明論文**。SIGGRAPH 2014。フラスタム→3Dグリッド分割→Compute Shaderで散乱計算 |
| [Physically-based & Unified Volumetric Rendering in Frostbite](https://www.ea.com/frostbite/news/physically-based-unified-volumetric-rendering-in-frostbite) | Sébastien Hillaire (Frostbite) | **Froxelの発展形**。物理ベースの散乱+吸収。Battlefield/FIFAで使用 |
| [The Real-time Volumetric Cloudscapes of Horizon Zero Dawn](http://advances.realtimerendering.com/s2015/) | Andrew Schneider (Guerrilla Games) | ボリュメトリッククラウド。Froxelの発展応用 |


### 改善版・発展形

| リポジトリ | ⭐ | 言語 | ライセンス | Froxelからの進化点 |
|---|---|---|---|---|
| [pezcode/Cluster](https://github.com/pezcode/Cluster) | 471 | C++/HLSL | **MIT** | **Clustered Shading（Froxelの一般化）**。Froxelをライトカリングに応用。Forward/Deferred/Clustered 切替可能。bgfx (DX11/12/Vulkan/GL)。1000+ lights |
| [DaveH355/clustered-shading](https://github.com/DaveH355/clustered-shading) | 168 | C++/GLSL | MIT | **Clustered Shading のチュートリアル**。教育向け。Froxel→Clusteredの学習に最適。OpenGL + Compute Shader |
| [azer89/HelloVulkan](https://github.com/azer89/HelloVulkan) | 116 | C++/GLSL | — | **Vulkan Clustered Forward** + PBR + IBL + Ray Tracing + **Bindless** + GPU-driven。FroxelのモダンVulkan実装 |
| [WickedEngine](https://github.com/turanszkij/WickedEngine) | 7.2k | C++/HLSL | MIT | **DX12 ネイティブ**。ボリュメトリック機能あり。実製品級のコード |
| [nTiled](https://github.com/BeardedPlatypus/nTiled) | 38 | C++/GLSL | — | Tiled/Clustered/**Hashed** Shading の3方式比較実装。修士論文付き。Froxel→Clustered→Hashedの進化を比較 |

### Froxel の進化マップ

```
Froxel Volumetric Fog (Wronski 2014)
    │ フラスタム整列3Dグリッド + 散乱積分
    │ [diharaw/volumetric-fog] ─── 純粋リファレンス実装 (MIT, GLSL)
    │
    ├→ Clustered Shading (Olsson 2012, Persson 2014)
    │   Froxelグリッドを"光の空間分割"に一般化
    │   [pezcode/Cluster] ─── Forward/Deferred切替可能 (MIT, bgfx)
    │   [DaveH355/clustered-shading] ─── チュートリアル (MIT, OpenGL)
    │
    ├→ UE4 Volumetric Fog (Epic 2017)
    │   時間的リプロジェクション + ボリュームシャドウ注入
    │   ※ 実装は非公開。エンジンソースを参照
    │
    ├→ Frostbite Unified Volumetric (Hillaire 2018)
    │   物理ベースの散乱/吸収統合モデル
    │   ※ 実装は非公開。論文+スライドを参照
    │
    └→ Hashed Shading (2018)
       ハッシュ関数でクラスタ割り当て。メモリ効率化
       [nTiled] ─── 3方式比較 + 修士論文
```


```
カメラ視錐台 (Frustum)
    ↓
深度方向にスライス分割 (例: 64スライス)
    ↓
各スライスを XY 方向にタイル分割 (例: 160x90)
    ↓
3D Froxel グリッド = 160 x 90 x 64
    ↓
Compute Shader で各 Froxel に光源注入
    ↓
深度方向に積分 (レイマーチング不要!)
    ↓
画面に合成 (フルスクリーンクワッド)
```

### なぜ Froxel が速いか

- レイマーチングと違い、グリッドあたり **O(1)** の処理
- Compute Shader で完全並列
- ライトの注入も Froxel 単位で効率的
- 半透明オブジェクトとの深度整合性が自然に取れる



## コア基盤（Undo/ECS/NodeGraph/Plugin/Serialization/Job）

### Undo/Redo

| リポジトリ | ⭐ | 言語 | ライセンス | 学ぶべきこと |
|---|---|---|---|---|
| [mikwielgus/undoredo](https://github.com/mikwielgus/undoredo) | 120 | Rust | — | Delta/DiffベースのUndo。Snapshot方式と比較した設計 |
| [implot](https://github.com/epezent/implot) のUndoパターン | — | C++ | MIT | ImPlotのUndo実装がコマンドパターンのクリーンな例 |
| [Qt Undo Framework](https://doc.qt.io/qt-6/qundo.html) | — | C++ | LGPL | **Qt純正**。`QUndoCommand` + `QUndoStack`。ArtifactStudio は既に Qt ベースなので最有力 |

### ノードグラフエディタ

| リポジトリ | ⭐ | 言語 | ライセンス | 学ぶべきこと |
|---|---|---|---|---|
| [imgui-node-editor](https://github.com/thedmd/imgui-node-editor) | 4.5k | C++ | MIT | **最も使われているImGuiノードエディタ**。Blueprint風。UE4インスパイア |
| [ImNodeFlow](https://github.com/Fattorino/ImNodeFlow) | 503 | C++ | MIT | ImGui向け軽量ノードエディタ。シンプルな設計 |
| [qt-mvvm](https://github.com/gpospelov/qt-mvvm) | 431 | C++ | — | **Qt向けMVVM + ノードエディタ**。Property Editor + Node Editor 両方あり。ArtifactStudioに最も親和性高い |
| [rete.js](https://github.com/retejs/rete) | 12.1k | TS | MIT | Web向けだがノードエディタ設計の模範。プラグインアーキテクチャ |

### ECS (Entity Component System)

| リポジトリ | ⭐ | 言語 | ライセンス | 学ぶべきこと |
|---|---|---|---|---|
| [EnTT](https://github.com/skypjack/entt) | 11k | C++17 | MIT | **最速ECSライブラリ**。Sparse Setベース。ヘッダオンリー。**ArtifactCore は entt を既に採用済** |
| [Flecs](https://github.com/SanderMertens/flecs) | 6.7k | C99/C++ | MIT | C99で書かれたECS。クエリDSL。メタデータリフレクション |

### シリアライゼーション

| リポジトリ | ⭐ | 言語 | ライセンス | 学ぶべきこと |
|---|---|---|---|---|
| [cereal](https://github.com/USCiLab/cereal) | 4.7k | C++11 | BSD 3-Clause | **C++シリアライゼーションの決定版**。JSON/XML/Binary対応。ヘッダオンリー |
| [magic_enum](https://github.com/Neargye/magic_enum) | 6.1k | C++17 | MIT | 静的enumリフレクション。enum→文字列/文字列→enum。マクロ不要 |
| [reflect-cpp](https://github.com/getml/reflect-cpp) | — | C++20 | MIT | **C++20の構造体リフレクション**。自動シリアライズ |

### プラグインシステム

| リポジトリ | ⭐ | 言語 | ライセンス | 学ぶべきこと |
|---|---|---|---|---|
| [extism](https://github.com/extism/extism) | 5.7k | Rust/C | BSD 3-Clause | **WebAssemblyベースのプラグインシステム**。全言語対応。サンドボックス。将来性あり |
| [Qt Plugin System](https://doc.qt.io/qt-6/plugins-howto.html) | — | C++ | LGPL | **Qt純正**。`QPluginLoader` + インターフェース。既存インフラ |

### ジョブシステム / タスクグラフ

| リポジトリ | ⭐ | 言語 | ライセンス | 学ぶべきこと |
|---|---|---|---|---|
| [taskflow](https://github.com/taskflow/taskflow) | 11k | C++17 | MIT | **最高峰の並列タスクグラフ**。DAGベース。GPUタスク対応。ヘッダオンリー |
| [Tina](https://github.com/slembcke/Tina) | 298 | C | MIT | 超軽量コルーチン+ジョブシステム。シングルファイル。ARM/RISC-V対応 |
| [marl](https://github.com/google/marl) | 2k | C++ | Apache 2.0 | **Google製**。ファイバーベースのスケジューラ。ゲームエンジン向け |

### アセットパイプライン

| リポジトリ | ⭐ | 言語 | ライセンス | 学ぶべきこと |
|---|---|---|---|---|
| [assimp](https://github.com/assimp/assimp) | 13.1k | C++ | BSD 3-Clause | **40+ 3D形式対応**。アセットインポートのデファクト。パイプライン設計の模範 |
| [rres](https://github.com/raysan5/rres) | 556 | C | MIT | raylib向けリソースパッケージング。シンプルなアセットバンドル形式 |
| [OpenAssetIO](https://github.com/OpenAssetIO/OpenAssetIO) | — | C++ | Apache 2.0 | **VFX向けアセット管理の相互運用標準**。ASWFプロジェクト |

### プロファイリング / トレーシング

| リポジトリ | ⭐ | 言語 | ライセンス | 学ぶべきこと |
|---|---|---|---|---|
| [Tracy](https://github.com/wolfpld/tracy) | 11k | C++ | BSD 3-Clause | **リアルタイムC++プロファイラ**。フレーム単位のGPU/CPUトレース。ゲーム業界の標準 |
| [microprofile](https://github.com/jonasmr/microprofile) | — | C++ | MIT | 軽量プロファイラ。ImGui統合。組み込み容易 |
| [optick](https://github.com/bombomby/optick) | 4.9k | C++ | MIT | **超軽量ゲームプロファイラ**。Frame-based。GPUトレース |

### 設定 / データ駆動

| リポジトリ | ⭐ | 言語 | ライセンス | 学ぶべきこと |
|---|---|---|---|---|
| [tomlplusplus](https://github.com/marzer/tomlplusplus) | 1.8k | C++17 | MIT | **TOMLパーサーの決定版**。ヘッダオンリー。UTF-8完全対応。設定ファイルに最適 |
| [nlohmann/json](https://github.com/nlohmann/json) | 45k | C++11 | MIT | **C++ JSONライブラリの標準**。STLライクな構文 |
| [structopt](https://github.com/p-ranav/structopt) | — | C++17 | MIT | 構造体→CLI引数パーサー自動生成 |
└───────────────────────────────────────────┘
         ↓
┌─ 動的要素 ──────────────────────┐
│ Twinkling (輝度ノイズ変調)       │ → アニメーション用
│ 流れ星 (確率的発生 + 軌跡)       │
│ 星雲オーバーレイ (ノイズ合成)     │
└─────────────────────────────────┘
```

|---|---|---|
| 反応拡散 (Turing Pattern) | 化学物質の拡散で縞・斑点が自己組織化 | 毛皮模様・葉脈・指紋 |
| フィロタキシス | 黄金角 137.5° の葉配列 | ひまわりの種・花パターン |
| L-system | 文法規則で樹木生成 | フラクタル植物・稲妻分岐 |
| 差分成長 | 細胞分裂しながら形形成 | 有機的形状アニメ |
| 群体行動 (Boids) | 鳥/魚の群れ | パーティクルの群れ制御 |
| 粘菌ネットワーク | 最短経路網の自己形成 | ロゴ間の有機的経路 |

### 天文学
| 技法 | 何か | ArtifactStudio 応用 |
|---|---|---|
| 星野レンダリング | H-R図に基づく星の色と分布 | 星空背景生成 |
| 星雲シミュレーション | ガス+パーティクル | ボリュームフィル |
| 重力レンズ | 光の曲がり | 歪みエフェクト |

### 地図学
| 技法 | 何か | ArtifactStudio 応用 |
|---|---|---|
| 陰影起伏図 | 標高から陰影計算 | ハイトマップ→3D陰影 |
| 等高線 | 標高線の生成 | 輝度→等高線パターン |
| 図法投影 | Mercator, Mollweide 他 | テクスチャの極座標変換 |

### 医療画像
| 技法 | 何か | ArtifactStudio 応用 |
|---|---|---|
| ボリュームレンダリング | CT/MRIの3D可視化 | スモーク・霧の表現 |
| 最大値投影 (MIP) | 視線上の最大輝度 | 光線痕跡の可視化 |

### 放送工学
| 技法 | 何か | ArtifactStudio 応用 |
|---|---|---|
| 波形モニター | 輝度の波形表示 | カラーグレーディング診断UI |
| ベクトルスコープ | 色相/彩度の円形表示 | 同上 |

### 音響工学
| 技法 | 何か | ArtifactStudio 応用 |
|---|---|---|
| スペクトログラム | 周波数×時間の可視化 | 音声→画像変換エフェクト |
| 波形表示 | 振幅エンベロープ | タイムラインの波形オーバーレイ |

### 結晶学・材料科学
| 技法 | 何か | ArtifactStudio 応用 |
|---|---|---|
| 結晶成長 | 核生成→樹枝状成長 | 氷結晶・霜パターン |
| 拡散律速凝集 (DLA) | ブラウン運動の粒子集積 | 雪の結晶・珊瑚パターン |

### 計算折り紙
| 技法 | 何か | ArtifactStudio 応用 |
|---|---|---|
| 折り畳みパターン | 紙の折り目で立体化 | トランジションの折り紙風変形 |
| 切り絵 | 接続した1枚のシルエット | シルエット+線画抽出 |

### 錯視・知覚心理学
| 技法 | 何か | ArtifactStudio 応用 |
|---|---|---|
| モアレ縞 | 周期的パターンの干渉 | レイヤー合成の干渉エフェクト |
| 運動残効 | 順応後に静止画が動いて見える | 錯視トランジション |
| ストロボ効果 | 周期的遮断で静止 | コマ撮り風 |

### アルゴリズムアート
| 技法 | 参照 | 何か |
|---|---|---|
| ストレンジアトラクタ | Lorenz, Rössler, Clifford | カオス軌道で粒子モーション |
| フラクタルフレーム | Scott Draves 1992 | 反復関数系で炎・雲 |
| ドメインワーピング | Inigo Quilez | ノイズでノイズを歪める高度な模様 |
| ピクセルソーティング | Kim Asendorf | 輝度/色相でピクセルを並べ替えるグリッチ |
| スリットスキャン | 映画 2001年宇宙の旅 | 時間軸を空間に展開 |
| ドロステ効果 | エッシャー | 画像内に自分自身を無限再帰 |
