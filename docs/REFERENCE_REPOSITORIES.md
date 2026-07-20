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
