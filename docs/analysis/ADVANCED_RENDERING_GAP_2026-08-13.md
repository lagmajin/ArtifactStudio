# 先進レンダリングシステム 導入ギャップ調査

**最終更新:** 2026-08-13

「先進的なレンダリングシステム」を導入する上で、ArtifactStudio に何が不足しているかをソースコード（`.ixx` / `.cppm` / `.hlsl`）を一次情報として検証した。

## 結論（要約）

不足の実態は「**部品が無い**」より「**シェーダ資産は大量にあるが C++ 実行経路に配線されていない**」「**完成済みインフラが休眠している**」が中心。レイヤーコンポーネントの整合性検証（`LAYER_COMPONENT_PIPELINE_INTEGRITY_2026-08-13.md`）と同じパターンがレンダリングでも顕著。

既に配線済みの土台は意外と揃っている（DX12/Vulkan デュアル、GPU ブレンド 35 種、FXAA/MSAA、コンピュート MB、SSGI 一部、GPU 駆動 MDI、bindless submitter、PSO キャッシュ、OCIO、動的解像度）。不足しているのは「実行バックボーンの統合」と「先進シェーダ群の実配線」。

---

## 1. レイトレーシング — 機能検出スモークのみ、実ジオメトリ非対応

**判定: 骨組みのみ（ウォームアップ + 機能検出）。実レイトレ未配線。**

### 実装済み
- `ArtifactCore/include/Graphics/RayTracingManager.ixx` + `src/Graphics/RayTracingManager.cppm`（316 行）。
- `ArtifactIRenderer.cppm:1504-1518` で起動時に `createRayTracingManager()` → `initialize()` → `buildTLAS()` → `ensurePipelineAndSBT()` → `traceUnitQuad(1,1)` を実行し、`rayTracingSupported` を判定。
- DXR 相当の機能検出: `props.Features.RayTracing`、`RAY_TRACING_CAP_FLAG_STANDALONE_SHADERS`、MaxRecursionDepth / MaxRayGenThreads / MaxInstancesPerTLAS / MaxPrimitivesPerBLAS 等を `RayTracingCapabilities` に取得。

### 未実装（実用に必要な部分が全部欠落）
- **BLAS に実ジオメトリを詰めない**: `createOrUpdateBLAS()` は `blasBuildCount++` だけの no-op（`RayTracingManager.cppm:106-111`）。
- **TLAS インスタンスを収集しない**: `buildTLAS()` は `tlasBuildCount++` + フラグ立てだけ（`:113-120`）。コメント「Future: collect all layer transforms and update TLAS instances」。
- **BLAS 作成自体が Unit Quad のみ**: `createUnitQuadBLAS()`（`:265-282`）は頂点 4 点・2 三角形の固定 Quad。実レイヤー/メッシュの BLAS 構築経路なし。
- **出力が固定色**: `RTWarmup_RayGen` が `float4(0.2, 0.45, 0.9, 1.0)` を書くだけ（`:31-36`）。closest-hit / any-hit シェーダが無い（miss は空）。
- **実レイヤーからの呼び出しなし**: `traceUnitQuad` は初期化時の 1×1 スモークのみ。合成フレーム内で反射/シャドウ/AO に使う経路がない。
- `NVPRO_RAYTRACING_PROGRESS_2026-07-20.md` に `rayTracingMinimalPayload / Miss / ClosestHit` のシェーダ契約があるが、これは別系統の shader 契約のみで、上記 Manager とも未接続。

### 必要
- BLAS: レイヤー/メッシュの頂点・インデックスバッファを `RTGeometryData` → `createOrUpdateBLAS` で実体化。
- TLAS: 各レイヤーの transform を instance として収集し `buildTLAS` で実構築。
- PSO/SBT: closest-hit / any-hit / miss を実シェーダ（`raytraceCS_rtapi` / `rtreflectionLIB` 等）で構成。
- 合成ループへの接続: 反射・ソフトシャドウ・RT-AO のいずれかに `traceUnitQuad` 相当を接続。

---

## 2. RenderGraph — 実行バックボーン未接続（診断専用）

**判定: アルゴリズム完成済み、実行駆動は未実装。診断テレメトリのみ稼働。**

### 実装済み
- `ArtifactCore/include/Graphics/RenderGraph.ixx` に `addPass` / `addResource` / `compile()`（Kahn トポロジカルソート）/ `compileDiagnosticSnapshot()` が自己完結で実装済み。
- `ArtifactCompositionRenderController.cppm:19766-19838` の `frameDebugSnapshot()` 内で**診断専用**の `RenderGraph` を構築し、`addPass`（LayerRaster / LayerBlend / FinalPostProcess / PointwiseEffects / ScreenSpaceGI）→ `compileDiagnosticSnapshot()` → `FramePipelineViewWidget` へ投入。診断表示は稼働中。

### 未実装（実行バックボーンとしての部分）
- **`compile()` で得た実行順序で実際のドローを発行していない**。実描画は `renderOneFrameImpl`（`ArtifactCompositionRenderController.cppm:25943` 付近）の**即時モード手書きシーケンス**。
- `GIRenderGraphAdapter::append()` / `PointwiseRenderGraphAdapter::append()`（`ArtifactCore/include/Graphics/RenderPipelineFoundation.ixx:29/107`）は**呼び出し元ゼロ**の孤児。診断グラフはインライン構築でアダプタを経由しない。
- `GIFrameContext`（`Graphics.GIResources`）と `PointwiseFusionGraph`（`Graphics.PointwiseFusion`）を populate する live コードがない。
- 結果、**自動リソースライフタイム・自動バリア・トポロジカル順の保証・循環検出**といった RenderGraph の本質的価値が、実行側で一切使われていない。

### 必要
- 戦略 A（実行バックボーン化）: `renderOneFrameImpl` の即時シーケンスを pass 構築 → `compile()` → 順次実行へ段階移行。
- まず GI/Pointwise の live エントリ（`dispatchScreenSpaceGlobalIllumination` / `applyPointwise`）を pass から呼ぶ形へ。
- マルチパスが増えるほど、自動バリア・リソース再利用・スケジューリングの重要性が上がるため、レイトレや GI を本格化する前にこの一本化が前提になる。

---

## 3. VXGI / DDGI / RT 反射 — シェーダ資産は大量、C++ 参照ゼロ

**判定: シェーダ資産のみ存在し、実行経路に完全未配線。**

### シェーダ資産（`Artifact/shaders/` と `Artifact/App/shaders/` に同一一式が重複）

| 系統 | ファイル |
|---|---|
| DDGI | `ddgi_raytraceCS(_rtapi)`, `ddgi_updateCS(_depth)`, `ddgi_rayallocationCS`, `ddgi_indirectprepareCS`, `ddgi_debugPS/VS`, `ShaderInterop_DDGI.h` |
| VXGI | `vxgi_resolve_diffuseCS`, `vxgi_resolve_specularCS`, `vxgi_temporalCS`, `vxgi_offsetprevCS`, `vxgi_sdf_jumpfloodCS`, `voxelGS/PS/VS`, `objectGS/PS/VS_voxelizer`, `voxelConeTracingHF.hlsli`, `ShaderInterop_VXGI.h` |
| RT 反射 | `rtreflectionCS`, `rtreflectionLIB`, `ssr_raytraceCS(_cheap/_earlyexit)`, `ssr_tileMaxRoughness_verticalCS` |
| RT 拡散 / AO / シャドウ | `rtdiffuseCS(_spatial/_temporal/_upsample)`, `rtaoCS(_denoise_*)`, `rtshadowCS(_denoise_*)`, `raytraceCS(_rtapi)`, `raytrace_debugbvhPS`, `raytrace_screenVS`, `renderlightmapPS_rtapi`, `surfel_raytraceCS(_rtapi)` |

### C++ 側の状況
- `Artifact/src` / `Artifact/include` / `ArtifactCore/src` / `ArtifactCore/include` を grep した結果、`ddgi` / `vxgi` / `voxelConeTracing` / `rtreflection` / `rtdiffuse` / `raytracing` の**実行参照は `ArtifactIRenderer.cppm/.ixx` 内の設定・デバッグ状態・モード enum のみ**。
- 実際に動作している GI は **SSGI（`RenderPipeline::dispatchScreenSpaceGlobalIllumination`、`ArtifactRenderLayerPipeline.cppm:660`、呼び出し `ArtifactCompositionRenderController.cppm:32650`）** だけ。
- `ArtifactIRenderer.ixx:87` に `GlobalIlluminationMode::DDGI` が定義され、`ddgiRaysPerProbe` / `ddgiProbeUpdateBudget` の設定と品質プリセット（`:4129-4167`）はあるが、**DDGI を実際にディスパッチするパスが存在しない**。`rayTracingSupported` が false なら SSGI へフォールバックする設計（`:4201-4217`）。

### 必要
- `ddgi_*` / `vxgi_*` / `rtreflection` / `rtao` / `rtshadow` の各シェーダをコンパイルし、対応する PSO を `ShaderManager` または RenderGraph pass に登録。
- BLAS/TLAS（§1）が完成して初めて `*_rtapi` 系が意味を持つ。非 RT フォールバックは `ssr_*` / `voxel*` を使う形。
- 現状は SSGI 一本なので、DDGI/VXGI のいずれかを 2 本目として接続するのが現実的な第一歩。

---

## 4. 仮想テクスチャ / スパース — シェーダのみ、配線なし

**判定: シェーダ資産のみ、C++ 実行経路なし。**

### シェーダ資産
- `virtualTextureTileRequestsCS`, `virtualTextureTileAllocateCS`, `virtualTextureResidencyUpdateCS`（`Artifact/shaders/` と `Artifact/App/shaders/` に重複）。
- `terrainVirtualTextureUpdateCS(_surfacemap/_normalmap/_emissivemap)`。

### C++ 側の状況
- `Artifact/src` / `include` / `ArtifactCore` の grep で、`virtualTexture` / `SparseTexture` / `samplerFeedback` の実行参照は**ゼロ**（シェーダファイル名以外）。
- Diligent 側の sparse/tiled resource や sampler feedback を扱うコードもアプリ側に無い。

### 必要
- 巨大テクスチャ / UDIM / 地形 / 高解像度フッテージのストリーミングをやる場合に、タイルページテーブル・リクエスト → アロケート → レジデンシ更新の CPU 側マネージャを新設し、上記シェーダを接続。
- 現状の静止画/連番/シェイプ優先方針（AGENTS.md）では優先度は低い。3D 地形・UDIM・巨大アセットを本格化する段階で検討。

---

## 5. 空間アップスケール — 動的解像度はあるが品質パス未接続

**判定: 動的解像度のみ実装。FSR1 相当のシェーダはあるが未配線、アップスケール品質パスなし。**

### 実装済み
- `ArtifactIRenderer::setUpscaleConfig(bool, float)`（`ArtifactIRenderer.cppm:4354-4360`）。内部 RT を render scale 70〜100% に縮小（`upscaleRenderScale()`、`:598`、`:2462-2466`）。
- 拡大表示は**品質パスなしの単純スケール**（`REPORT_UPSCALE_TECH_FEASIBILITY_2026-07-28.md` の分析と一致）。

### シェーダ資産
- `fsr_upscalingCS.hlsl`、`fsr_sharpenCS.hlsl`（`Artifact/App/shaders/` にのみ存在）。
- `anime4k_edge_upscaleCS.hlsl`（`Artifact/shaders/`。ただしソース参照ゼロ）。

### C++ 側の状況
- grep で `fsr` / `FSR` / `anime4k` の C++ 実行参照は**ゼロ**（`upcalse`/`Upscale` は `setUpscaleConfig` のみ）。
- `fsr_upscalingCS` / `fsr_sharpenCS` をコンパイル・接続するパスが無い。

### 必要
- `setUpscaleConfig` の拡大表示パスに `fsr_upscalingCS`（EASU 相当）+ `fsr_sharpenCS`（RCAS 相当）、または `anime4k_edge_upscaleCS` を接続。
- 書き出し時アップスケール（出力解像度 > コンポ解像度）にも同パスを追加。
- ML 超解像（Real-ESRGAN + ONNX/DML）は `OnnxDmlLocalAgent` 基盤が既にあるため、書き出し時オプションとして追加可能。モデル重みのライセンス要確認（`REPORT_UPSCALE_TECH_FEASIBILITY_2026-07-28.md` 参照）。

---

## 既に配線済みの土台（不足と誤認しやすいが実は存在）

| 機能 | 状態 | 根拠 |
|---|---|---|
| DX12 / Vulkan デュアルバックエンド | 稼働 | `DiligentDeviceManager` |
| GPU ブレンド 35 種 | 稼働 | `LayerBlendPipeline` / `LayerBlendComputeShader` |
| FXAA + MSAA | 稼働 | `applyFastApproximateAntiAliasing`（`ArtifactRenderLayerPipeline.cppm:914`） |
| コンピュートモーションブラー | 稼働 | `ArtifactMotionBlurPass` |
| SSGI（スクリーンスペース） | 稼働 | `dispatchScreenSpaceGlobalIllumination`（`:660`） |
| SSAO | 稼働（opt-in） | `ArtifactRenderLayerPipeline` |
| GPU 駆動 MDI + visibility compaction | 稼働 | `MILESTONE_GPU_DRIVEN_MDI_RENDER_2026-04-02.md` の 2026-07-27 更新 |
| Bindless submitter | 実装済み | `DiligentBindlessSubmitter.cppm` |
| PSO キャッシュ | 実装済み | `ShaderManager.cppm:616` `createPSOCache` |
| OCIO（実ライブラリ）+ GPU LUT | 実装済み | `ArtifactOCIOManager` / `LUT3DComputer` |
| 動的解像度（70〜100%） | 稼働 | `setUpscaleConfig` |

---

## 不足の優先順位

1. **RenderGraph の実行バックボーン化**（§2）— これが無いとマルチパス拡張が手書きシーケンスに埋もれ続ける。全先進機能の前提。
2. **レイトレ BLAS/TLAS/実シェーダ接続**（§1）— 現在は機能検出のみ。反射・ソフトシャドウ・RT-AO の土台。
3. **DDGI または VXGI の実配線**（§3）— シェーダは揃っており、SSGI からの拡張として最も費用対効果が高い。
4. **空間アップスケールの接続**（§5）— 動的解像度が既にあるので、FSR1 相当の配線は低リスクで即効。
5. **仮想テクスチャ**（§4）— 現行方針では優先度低。3D 地形/UDIM 本格化時に。

時間軸アップスケーラ（DLSS/FSR2+/XeSS）は 2D コンポジタの静止画/シェイプ中心方針と相性が悪く、モーションベクタ + 深度 + ジッタ基盤の整備コストが大きいため非推奨（既存判断 `REPORT_UPSCALE_TECH_FEASIBILITY` に同意）。動画/3D を本格化する段階で再検討。

---

## 主要ファイル

| ファイル | 役割 |
|---|---|
| `ArtifactCore/src/Graphics/RayTracingManager.cppm` | RT 機能検出 + ウォームアップ（実ジオメトリ非対応） |
| `ArtifactCore/include/Graphics/RenderGraph.ixx` | RenderGraph データ構造（休眠） |
| `ArtifactCore/include/Graphics/RenderPipelineFoundation.ixx` | GI/Pointwise アダプタ（未接続） |
| `Artifact/src/Render/ArtifactRenderLayerPipeline.cppm` | SSGI / FXAA / pointwise（live） |
| `Artifact/src/Widgets/Render/ArtifactCompositionRenderController.cppm` | 即時モード実行ループ + 診断グラフ |
| `Artifact/shaders/` + `Artifact/App/shaders/` | DDGI/VXGI/RT/FSR/仮想テクスチャ シェーダ資産（大半未配線） |
