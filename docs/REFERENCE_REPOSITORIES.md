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
| [EveryRay-Rendering-Engine](https://github.com/steaklive/EveryRay-Rendering-Engine) | 759 | C++ | MIT | DX11/DX12 リアルタイムレンダリングエンジン。**ボリュメトリックフォグ/クラウド**、VCT、PBR、パララックス |
| [zeno](https://github.com/zenustech/zeno) | 1.4k | C++ | — | ノードベースのシミュレーション＆レンダリングエンジン。データフロー可視化 |

---

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
