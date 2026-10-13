# Milestone: DX12/Vulkan Ray Tracing Integration

Date: 2026-05-16
**最終更新:** 2026-10-14

## Goal

Bring ray tracing into Artifact through Diligent's cross-backend API without forking
the renderer per backend. DX12 and Vulkan should share the Core contract, the
same acceleration-structure ownership model, and the same app-level diagnostics.

## Current State

- `DiligentDeviceManager` already requests `Features.RayTracing = ENABLED` for
  DX12 and Vulkan, then falls back to a non-RT device if creation fails.
- `ArtifactCore::RayTracingManager` owns the future Core boundary for BLAS/TLAS.
- `ArtifactIRenderer` exposes the Core manager to the app layer.
- The first integration slice records RT capability, backend, Diligent limits,
  unit-quad BLAS creation, and TLAS creation in render diagnostics.

## Implementation Slices

1. Capability and diagnostics contract
   - Core reports device type, feature state, supported flag, BLAS/TLAS creation,
     recursion depth, raygen/thread limits, TLAS/BLAS limits, and AS alignments.
   - App render path summary includes the RT debug state.

2. Real BLAS/TLAS build path
   - First slice builds a shared unit-quad BLAS and one-instance TLAS through
     Diligent using `BIND_RAY_TRACING` geometry, scratch, and instance buffers.
   - Replace the unit-quad placeholder with explicit geometry upload/build data.
   - Keep geometry input in Core buffers, not `QImage` or Qt-owned image memory.
   - Add dirty flags so layer transforms update TLAS without rebuilding BLAS.

3. Ray tracing pipeline state
   - First slice creates a minimal HLSL raygen/miss/closest-hit set, RT PSO,
     SBT, and a 1x1 warmup `TraceRays` dispatch when RT is supported.
   - The warmup shader binds the Core TLAS plus an `RGBA32_FLOAT` UAV output
     texture so it exercises the same resource path that a composition render
     pass will later use.
   - Core exposes the RT output SRV/UAV and `ArtifactIRenderer` exposes the SRV
     as `rayTracingOutputTextureView()` for the future composition merge path.
   - Add a minimal ray generation, miss, and closest-hit shader set.
   - Create SBT records through Diligent, not backend-specific native calls.
   - Output to a float render texture compatible with the composition pipeline.

4. App integration and fallback
   - Add debug/experimental toggles in the app layer.
   - Fall back to raster/composition paths when RT is unsupported, disabled, or
     missing AS resources.
   - Keep diagnostics visible enough to distinguish unsupported GPU/driver,
     disabled backend, AS creation failure, and dispatch failure.

5. Composition use cases
   - Start with shadows/visibility or intersection picking before full GI.
   - Later connect scene lights and material data for reflections/GI.

## Constraints

- Do not split DX12 and Vulkan logic unless Diligent cannot represent a feature.
- Do not add hot-path `QImage` conversions.
- Do not change DiligentEngine itself for this milestone.
- Keep each low-level slice small enough to build and bisect independently.

## Static Audit (2026-07-25)

Capability／diagnosticsの基盤は実装されている。`DiligentDeviceManager` は backend の RayTracing feature を確認し、設定に応じて DX12／Vulkan の device creation を試行する。`RayTracingManager` は Diligent の共通 API で capability、recursion／raygen／AS limits、alignment、unit-quad BLAS、TLAS、warmup RT PSO／SBT／出力 texture を管理し、`ArtifactIRenderer` は初期化・TLAS build・backend／AS状態を診断文字列へ出している。非対応時に manager 初期化をスキップする経路もある。

しかし、現状のBLASは unit quad のplaceholderで、`createOrUpdateBLAS` は layer geometry を利用せず、`buildTLAS` も layer transform の収集・更新を将来課題としている。Composition の実描画やshadow／picking／GIへの接続、debug toggle、dispatch failure と AS creation failure の粒度、DX12／Vulkan両backendでの実機動作は未確認。別系統の `GPURayTracer` も存在するが、今回の `IRayTracingManager`／composition 経路へ統合済みとは断定できない。よって Slice 1 は静的実装済み、Slice 2〜5 はwarmup／placeholder段階と実行検証待ちとして記録する。

確認対象:

- `ArtifactCore/src/Graphics/RayTracingManager.cppm`
- `ArtifactCore/include/Graphics/RayTracingManager.ixx`
- `ArtifactCore/src/Render/GPURayTracer.cppm`
- `Artifact/src/Render/DiligentDeviceManager.cppm`
- `Artifact/src/Render/ArtifactIRenderer.cppm`
- `ArtifactCore/src/Render/Shaders/RayTrace.hlsl`

## 現行コード監査 (2026-08-15)

`RayTracingManager` は Diligent 共通 API で device capability、BLAS／TLAS、scratch／instance buffer、RT PSO／SBT、output texture、`TraceRays` dispatch を管理している。`ArtifactIRenderer::drawMesh` は不透明・単一インスタンスのメッシュについて、描画用 vertex/index buffer、shading geometry、transform、PBR material を manager へ登録し、変更時に AS を更新する。RT shader は metallic／roughness を中心に specular、IOR、transmission、clearcoat、sheen、emission を扱い、GGX direct lighting と shadow ray を実装している。`RTRayCamera` を渡す camera ray generation も追加され、Vulkan acceptance test でカメラ移動と不正入力拒否を確認した。

ただし、Composition の active camera は `traceCamera` にまだ渡されず、manager の RT output SRV も Composition の render target に merge されない。したがって Artifact 起動時の 1x1 warmup dispatch を除いて製品フレームはまだ ray trace されない。RT manager は MeshRenderer から既存の prefiltered environment cubemap、dedicated cosine-convolved irradiance cubemap、split-sum BRDF LUT、有効状態、intensity、rotationを受け取る。closest-hitはprefilter mipとBRDF LUTで鏡面IBL、irradiance cubeで拡散IBLを評価し、薄面transmissionは引き続きprefiltered cubemapを使う。Vulkan acceptance test でirradiance cubeを入れ替えた拡散色差と、BRDF LUTをゼロにした金属反射差を確認した。RT shader は Composition のシーンライト最大8灯を受け取り、directional／point／spot／ambient を評価し、spotのコーンフェードはforward shaderと同じ線形saturate式にそろえた。Areaは矩形／円盤上の固定4サンプルで方向・距離減衰・BRDF・遮蔽を評価する。Ambient 以外のシーンライトは `castsShadows` と `shadowSoftness` を尊重し、影有効時は softness 0 で1本、正値で固定4本の可視性 ray を使う。Vulkan acceptance test で point light の硬影／softness段階差、Areaの影差、shadow無効時のArea直接照明差を確認した。clearcoatは直接光でベース層のdiffuseとspecularのエネルギーをcoat Fresnel分だけ減衰し、IBL反射はclearcoat roughnessに応じてprefilter mipとBRDF LUTを選ぶ。単独Vulkan acceptanceで直接光のclearcoat差を確認した。MeshRendererがprefilter cubeのTextureData subresourceをmip-majorで並べていたためarray-layer-majorへ修正。MeshRenderer実生成のIBL資産を使ったVulkan acceptanceでclearcoat roughness 0.04／1.0の環境反射色差を確認した。Principled transmission は IOR/Fresnel による屈折方向で environment cubemap をサンプルする薄面近似を追加したが、内部体積・厚み・吸収・複数界面の追跡はしない。ライトが無い場合は従来の仮想ライトに対する16サンプルsoft shadowを使う。Composition frame dispatch／output merge、instanced geometry、透明・opacity texture 付きメッシュの RT 登録、DX12 acceptance、raster fallback の runtime parity は未検証である。現状の判定は **geometry／PBR／camera ray／scene lights とそのライト設定対応影／split-sum environment IBL／thin transmission の基盤は実装済み、Composition dispatch／output merge は pending** とする。

## Update 2026-08-15

- `RayTracingManager` の Diligent 共通 capability、`RTGeometryData` による BLAS 更新、active instance の TLAS、scratch／instance buffer、warmup RT PSO／SBT／`TraceRays` を再確認。
- `ArtifactIRenderer` は geometry change 時の BLAS／TLAS 更新と RT diagnostics を呼び出す経路まで持つため、旧「unit-quad placeholder のみ」という判定は更新。
- ただし layer geometry／transform の composition 収集、shadow／picking／GI の実利用、DX12／Vulkan 実機差、AS／dispatch failure の詳細 diagnostics、raster fallback の runtime 受入は未完了。

## Update 2026-10-13

- `ArtifactIRenderer::drawMesh` の RT geometry tracking と PBR material 連携を確認。対象は不透明かつ非 instanced の mesh draw に限られる。
- closest-hit shader に GGX ベースの直接光、soft shadow、Principled 系の material subset がある。環境反射は shader 内の analytic sky で、既存 MeshRenderer IBL とは別経路。
- `RTRayCamera` と `traceCamera()` を追加。既定値は従来のテスト camera を保ち、Vulkan acceptance test で視点変更による hit 範囲変化と不正 camera 拒否を検証した。
- RT shader に既存 prefiltered environment cubemap の反射サンプリングを追加。roughness による mip 選択、environment intensity／rotation、環境無効時の analytic sky fallback を実装し、Vulkan acceptance test で金属面の cubemap 反射を確認した。
- MeshRendererのdedicated irradiance cubeとsplit-sum BRDF LUTをRT managerへ公開・転送し、RT拡散IBLをirradiance cube、鏡面IBLをprefiltered cube＋BRDF LUTで評価。Vulkan acceptanceで拡散cubeの色差とzero-LUT時の金属反射低下を検証し、`16-dedicated-irradiance.png` と `17-brdf-lut-response.png` を保存。
- RT material packetとMeshRendererのPBR setterでNaN／Infを各Material既定値に戻して値域を制限。Vulkan acceptanceで非有限RT materialの正規化と、正規化後の値が既定materialと同一なら再upload不要となることを確認。
- Composition のシーンライト最大8灯をRTへ渡す固定定数バッファを追加。directional／point／spot／ambient を評価し、Ambient の拡散色と Point のピクセル出力差を Vulkan acceptance test で確認。未変更のライト配列は再アップロードしない。
- Ambient 以外のシーンライトに1灯あたり1本の可視性rayを追加。Vulkan acceptance test でpoint lightの遮蔽ピクセル差を検証し、画像 `10-scene-light-shadows.png` を保存。
- `castsShadows` と `shadowSoftness` を RT shader に渡す。影有効時は softness 0 で1本、softness > 0 で固定4本の可視性rayを使い、ray coneの幅をsoftness (0〜2) に比例させる。Vulkan acceptance test でhard／medium／softの画像全体差を検証し、`12-medium-scene-shadow.png` と `13-soft-scene-shadow.png` を保存。追加コストは影有効な非Ambient灯ごとに1または4本。
- Area の幅・高さ・shapeをRT packetへ渡し、矩形／円盤の固定4サンプルごとに方向、距離減衰、面の向き、GGX/Principled subset BRDF、visibilityを評価する。画像 `14-area-light-shadow.png` で面光源影、`15-area-direct-light.png` で影無効時の有限面直接照明をpoint sourceと比較して確認。
- 固定16サンプルsoft shadowはシーンライト未設定時のfallback経路に残る。
- transmission を裏面直接光の加算から、IOR/Fresnelで屈折させた環境cubemap samplingへ変更。ガラス相当materialの環境有無でpixel radiance差をVulkan acceptance testで確認し、`11-transmission-environment.png` を保存。現状は薄面近似で、内部体積・厚み・吸収・複数界面は追跡しない。
- `ArtifactIRenderer::drawMesh` から MeshRenderer の cubemap 設定を RT manager に渡す。ただし製品 Composition の frame path は依然として 1x1 warmup trace のみ。実 frame dispatch、Composition output への merge、Composition camera の伝播、物理ベースの IBL、複数 instance 対応、DX12 実機 acceptance は次段階。

## 2026-10-13 追加: forward/RT split-sum式

MeshRendererのforward specular IBLを `F0*A+B` にし、RT closest-hitと一致させた。forward diffuse IBLもRTと同じ `(1-F0)*(1-metallic)*(1-transmission)` のエネルギー係数を使う。これはシェーダー式の整合であり、raster/RT全画素parityはまだ確認していない。AO、studio fill、direct lighting、出力変換の差を含めた比較は次段階。


## 2026-10-13 追加: Forward PBRの画素検証

MeshRendererをVulkan RGBA32_FLOAT render targetへ描く受入テストを追加した。固定IBL入力で、NdotV=0.5のBRDF LUT式 `F0*A+B`、diffuse energy配分、全透過時のdiffuse除去を画素値で検証する。forwardのspecular=0.5は既定F0=0.04になるようRTと同じく0.5基準で正規化した。GPU上の画素テストは成功。実HDR・トーンマッピング境界を含むraster/RTの広範なparityは未確認。


## 2026-10-13 追加: MeshRenderer shadow fallback depth format

Vulkan画素受入テストで検出した比較サンプラー形式エラーを修正した。`MeshRenderer_LitShadowFallback` を `D32_FLOAT` のdepth-stencil/SRVとして作り、depth=1.0で初期化する。これは全テクセルをlitとして扱う既存fallbackの意味を維持する。forward PBR画素テストとRT受入5件の再実行でvalidation errorが消えたことを確認した。


## 2026-10-13 追加: Radiance HDRをfloatで保持

ImageImporterのread type選択に `.hdr` をfloatとして追加し、UINT8変換で失われていた1.0超の輝度を保持する。受入テストでRGBE Radianceの値4.0/1.0/0.25をImporterとMeshRendererの実IBL経路で確認した。MeshRenderer画素はHDR強度に比例する。初回Raster/RT比較では環境反射が不一致だったが、RTの頂点／法線poolをfloat4 strideに統一して解消した。OpenUSD Stinson Beach HDRIと1 mip・6面個別色cubemapのRaster/RT比較が通過し、Ray Tracing acceptance suiteは5/5成功。

## 2026-10-13 追加: HDRI上のPrincipled material sweepとstudio fill parity

Stinson Beach HDRI、同じtriangle/cameraを使い、smooth/rough dielectric、異なるIOR、fully metallicの5材質をMeshRendererとRTへ渡すVulkan acceptance比較を追加した。初回比較で残ったRGB差はlinear base colorの各channelに0.24を掛けた値と一致し、forward PBRにある固定studio ambient fillがRT側に無かったためと確認した。RT出力に同じfillをdiffuse energy／clearcoat適用外で加え、HDR material sweepと方向cubemap parityを含むfocused test、およびRay Tracing acceptance全5テストが成功した。fixture上の材質反応とbackend parityを検証したものであり、Composition最終出力の露出・tone mappingを含む視覚parityは未検証。

## 2026-10-13 追加: 非一様スケール時のPBR法線変換

MeshRendererのforward頂点shaderは法線へinstance位置行列を直接適用していた。非一様スケールでは面法線とPBR反射方向が崩れるため、余因子行列と行列式でinverse-transposeを計算し、RT側と同じnormal transformにそろえた。非一様スケールを加えたHDRI raster/RT pixel比較を含むRay Tracing acceptanceは5/5成功。余因子計算のvertex shader負荷は未計測で、特異変換のnormal fallbackも物理的な解ではない。


## 2026-10-14 追加: RT PBRのtexture mipとEmission Texture

ray-generationからpixel cone spreadをray payloadへ渡し、hit triangleのworld／UV Jacobianとtexture寸法からLODを推定する。RT AO／base color／metallic-roughness／normal textureで、近縮小時にmipを選択する。128×128 checker mapのVulkan acceptanceで複数画素をlinear-light平均値と比較し、個別fixtureと全suiteを通過した。ray-cone footprintは近似で、斜面・seam・非一様scaleをまたぐ精度とmap同時利用時のGPUコストは未測定。

MaterialのEmission Texture pathをRT packet、slot別cache、shared texture descriptor array、closest-hit shaderへ接続した。sRGB viewとlinear-light mipを使い、mapのlinear RGBをEmission Color／Strengthへ乗算する。左右で赤／緑が異なるVulkan acceptanceを追加し、個別fixtureと当時のsuiteを通過した。

以前の試作はgeometryの`RAYTRACING_GEOMETRY_FLAG_OPAQUE`がany-hitを省略させていた。後続実装でMasked alpha geometryだけopaque flagを外し、mode切替時はBLASを再buildするよう修正。Vulkan acceptanceはalpha map黒部で前面をcutoutして青い背面を表示し、白部は前面を維持する。さらにbaseColor alpha 0.7 × scalar opacity 0.7 < cutoff 0.5の組合せで前面を捨てることを確認した。material更新後はTLASを再buildし、hit groupをSBTへ再bindする必要がある。透過Blended材質の順序依存合成とComposition output mergeは引き続き未対応。


## 2026-10-14 追加: Masked alphaとOpacity Texture

RT material packetへbaseColor alpha、layer/material scalar opacity、alpha cutoff/mode、Opacity Texture pathを接続した。Opacity TextureはRGB linear luminanceをalphaへ格納し、共通PBR texture arrayでray-cone mipをsampleする。Masked geometryはBLASのopaque flagを解除し、alpha cutoff未満のhitにIgnoreHit()を使う。Opaque/Masked切替でBLAS build flagsを更新するため、再buildを要求する。

Vulkan acceptance fixtureは前後2面を配置し、Opacity map黒部を通るrayが背面の青emissionへ届き、白部は前面の赤emissionを維持することを確認した。続けてbaseColor alphaとscalar opacityを各0.7にし、積が0.5 cutoff未満となるため白map上の前面も捨てられることを確認した。map fixtureとfactor fixtureを含めRayTracingAcceptanceTest全13件成功、ArtifactCoreRayTracingAcceptanceTest／ArtifactRender build成功。Blended alpha composition、複数material slotごとのopacity texture、Composition ray dispatch/output mergeは未完了。


## 2026-10-14 追加: Masked alpha shadow ray

独立Vulkan fixtureで、camera rayから外れたOpacity Texture付きMasked quadをreceiverとpoint lightの間へ配置した。mapをwhiteにするとquadがshadow rayを遮り、同じreceiver pixelが暗くなる。mapをblackへ差し替えるとIgnoreHit()後にrayが継続し、receiverへのpoint light寄与が戻ることをpixel値とcapture 30-pbr-masked-shadow-ray.pngで確認。単独fixtureと、Masked opacity/factor、既存RT suiteを含む全14 acceptance tests成功。したがってalpha cutoffはprimary visibilityとdirect-light shadow visibilityの両方に適用される。複数material slotのshadow-map差、soft-shadow penumbra品質、DX12 parity、Blended transmissionは引き続き未確認。
