**ステータス:** Partial camera foundation / DOF render path not implemented

# M-CAM: Camera Enhancement - Depth of Field / Lens Blur 設計マイルストーン（本格的移植版）

カメラレイヤーのレンズ機能強化。被写界深度（DOF）とレンズボケ（bokeh）を実描画に繋げる。
方針: **Wicked Engine 由来の tile-based DOF シェーダー資産を Diligent 向けに忠実移植する（本格的）**。

## 核心的制約（設計の前提）

1. **深度バッファは今 `BIND_DEPTH_STENCIL` のみでサンプル不可。**
   - `ArtifactIRenderer.cppm:1283,1989`（`m_layerDepthTex`）や `createOffscreenDepthTexture`（`:3208`）は全て `BindFlags = BIND_DEPTH_STENCIL` のみ。D32_FLOAT を直接サンプルする SRV は作られていない。
   - 3D 描画（`MeshRenderer`, `PrimitiveRenderer3D`）は depth write を有効にしているので GPU 上に深度は live にあるが、**DOF コンピュートが読むには `R32_FLOAT` へコピー/リゾルブした `BIND_SHADER_RESOURCE | BIND_UNORDERED_ACCESS` テクスチャが要る**。これが最初の前提作業。
2. **シェーダーは bindless (`spaceN`) + indirect dispatch + push-constant を仮定。**
   - `globals.hlsli` の `texture_lineardepth = bindless_textures_float[GetCamera().texture_lineardepth_index]` 等。
   - `depthoffield_neighborhoodMaxCOCCS.hlsl` は `InterlockedAdd` で `PostprocessTileStatistics` を書き、3 種の tile list を indirect dispatch で実行する。
   - `PostProcess` 定数は push-constant パス（`PUSHCONSTANT(postprocess, PostProcess)` → Vulkan `vk::push_constant` / 其他 `ConstantBuffer<...> : register(b999)`）。
   - したがって Diligent 側で「bindless 配列 or 静的 SRV 化」「`DispatchComputeIndirect`」「push-constant range」を揃えないと動かない。

## アーキテクチャ決定

- **DOF は「3D シーン描画時のパス」に置く（2D 最終エフェクトではない）。**
  - 本質入力は「カメラのフォーカス面からのシーン深度」。2D 合成のあと（最終 LUT のように）に置くと深度がなくて成立しない。
  - 3D コンテンツは `renderOneFrameImpl` で active `ArtifactCameraLayer` を解決 → `set3DCameraMatrices` へ push される。DOF パスはこの 3D シーンの深度/カラーを消費する。2D のみのコンポジションでは DOF は意味を持たない。
- **`ArtifactCameraLayer` は「意図」のみ持つ。** `depthOfField_` / `aperture_` / `focusDistance_` / `blurAmount_` を CoC uniform に写す小さな `DOFParameters` プロデューサだけ追加。描画ロジックは持たない。
- **実行は Diligent backend が持つ。** 不足の dispatch 層を新規追加。

## 依存シェーダー資産一覧（忠実移植対象）

ヘッダ（移植必須 5 点）: `globals.hlsli`, `ShaderInterop.h`, `ShaderInterop_Postprocess.h`, `ShaderInterop_Renderer.h`, `depthOfFieldHF.hlsli`。
定数: `POSTPROCESS_BLOCKSIZE=8`, `DEPTHOFFIELD_TILESIZE=32`, `dof_cocscale=params0.x`, `dof_maxcoc=params0.y`, `PostProcess{resolution, resolution_rcp, params0, params1}`, `PostprocessTileStatistics{IndirectDispatchArgs x3}`。

| Pass | ファイル | 入力 (t/SRV) | 出力 (u/UAV) | 備考 |
|---|---|---|---|---|
| 1 | `tileMaxCOC_horizontalCS` | bindless `texture_lineardepth` | u0 `float2`, u1 `float` | 8x8 |
| 2 | `tileMaxCOC_verticalCS` | t0 horiz `float2`, t1 horiz `float` | u0 `float2`, u1 `float` | 8x8 |
| 3 | `neighborhoodMaxCOCCS` | t0 `float2`, t1 `float` | u0 `RWStructuredBuffer<PostprocessTileStatistics>`, u1-3 tile lists, u4 `float2` neighborhood | `InterlockedAdd` で indirect 引数生成 |
| 4 | `prepassCS` (+earlyexit) | t0 color `float4`, t1 neighborhood `float2`, t2 tiles, bindless lineardepth | u0 `float3`(coc,bg,fg), u1 `float3` prefilter | |
| 5 | `mainCS` (+cheap,+earlyexit) | t0 neighborhood, t1 presort, t2 prefilter, t3/t4/t5 tiles(by variant) | u0 `float3` main, u1 `unorm float` alpha | 64 threads, Jimenez ring scatter |
| 6 | `postfilterCS` | t0 main `float3`, t1 alpha `float` | u0 `float3`, u1 alpha | median |
| 7 | `upsampleCS` | t0 full-res `float4`, t1 postfilter, t2 alpha, t3 neighborhood, bindless lineardepth | u0 full-res `float4` | 8x8 |

- `depthOfFieldHF.hlsli` は self-contained（ring offset `disc[80]`, `ringSampleCount[5]`, `get_coc`, `SampleAlpha` 等）。`Camera` 構造体から `aperture_size` / `focal_length` / `z_far` / `z_near` / `texture_lineardepth_index` を消費。
- `mainCS` の `t3/t4/t5`（earlyexit/cheap/expensive）と `prepassCS` の `t2` は variant でレジスタが分かれる。単一 PRS で全 variant をカバーするよう維持すること。

## Diligent 側で閉じる前提作業（P0）

1. **深度 SRV の導入。** 3D 描画後に D32_FLOAT 深度を `R32_FLOAT`（`BIND_SHADER_RESOURCE | BIND_UNORDERED_ACCESS`）へ `CopyTexture`/リゾルブ。既存 `pushRenderTarget(color, depth)` / `popRenderTarget()` / `clearDepthRenderTarget()` を再利用。`activeDepthView()` の解決順（override DSV → `m_layerDepthTex` → swap-chain DSV）に注意。
2. **bindless 解消。** 全 `texture_*` マクロと `tiles` StructuredBuffer 読みを固定 SRV/UAV スロットへ書き換え。または Diligent の dynamic descriptor set / 配列 SRV で bindless 配列を再現（忠実だがリスク大）。**推奨: 静的 SRV 化。**
3. **push-constant range。** `PostProcess` サイズの push-constant range を確保（`dof_cocscale`/`dof_maxcoc`/`resolution`/`resolution_rcp` をセット）。`CBSLOT_PUSHCONSTANT`（b999）相当でも可。
4. **indirect dispatch。** `neighborhoodMaxCOCCS` が書く `PostprocessTileStatistics` から `DispatchComputeIndirect` で pass 4/5 を 3 variant ずつ実行。tile list バッファは分類→各 variant dispatch 間で生存させる。
5. **sampler 作成。** `sampler_linear_clamp`(s100) / `sampler_point_clamp`(s103) に合わせた immutable sampler。
6. **1 PSO + PRS で 7 エントリをカバー。** variant レジスタ分割（t3/t4/t5, t2）を PRS に維持。

## 実装フェーズ（薄いところから）

### P0: 深度読み出し基盤（必須前提）
- `ArtifactIRenderer` に `R32_FLOAT` 深度コピー用テクスチャ + SRV を追加。3D シーン描画パスのあとに `CopyTexture` で転送。
- 完了条件: DOF パスが GPU 深度を SRV で読めること（AOV readback ではなく live）。

### P1: prepass + main 最小チェーン（本格の一部, 動く DOF）
- `depthoffield_prepassCS` + `depthoffield_mainCS`（cheap のみ）を Diligent `ComputeExecutor` 経由で dispatch。
- ヘッダ 5 点を Diligent 向けに移植（bindless→静的 SRV, push-constant 化）。
- 完了条件: フォーカス面外がブラーになること（tile/neighborhood 簡略でも可）。

### P2: tile + neighborhood 本格化
- `tileMaxCOC_horizontal/vertical` + `neighborhoodMaxCOCCS`（indirect dispatch 含む）を追加。main の earlyexit/expensive variant も有効化。
- 完了条件: Jimenez の tile-based 分類（earlyexit/cheap/expensive）が全部動く。

### P3: postfilter + upsample
- `postfilterCS`（median）+ `upsampleCS`（half-res→full-res 合成）を追加。
- 完了条件: 最終出力が full-res カラーに戻り、エッジノイズが median で低減。

### P4: カメラパラメータ → CoC uniform（M-CAM-A1 相当）
- `ArtifactCameraLayer` に `DOFParameters` 算出ヘルパ（`aperture_` / `focusDistance_` / `blurAmount_` → `cocScale` / `maxCoc` / focal plane）。`depthOfField_=false` は既存描画と完全一致（パス通さない）。
- `ShaderInterop_Postprocess.h` の `params0.x/.y` へマッピング。
- `PreviewQuality::isDepthOfFieldEnabled()` をゲート接続（現状 inert）。

### P5: Inspector / ガイズモ同期（M-CAM-A3 相当）
- 既存 "Lens / DOF" グループ + オーバーレイ "DOF On/Off" で実効果を目視確認。

### オプション拡張（M-CAM-B 相当）
- **B1 bokeh shape**: `apertureBlades_` / `bokehRotation_` プロパティ追加、`mainCS` の ring カーネルを n 角形/アナモフィックへ。
- **B2 motion blur**: `motionBlur_` メタデータ実可動。`Artifact/shaders/motionblurCS.hlsl` + `VelocityX/Y` AOV を使用。`metadata` と明示されているため周知必須。

## リスク・未確認

- **Wicked Engine 統合は dead code**（`ArtifactRenderManagerWidget` で `wi::renderer::` 全コメントアウト）。DOF は Diligent コンピュートとして自前 dispatch。
- **bindless→静的 SRV 化の労力** が最大リスク。忠実を取るか、main の ring サンプルを固定 SRV に書き換えるかでコストが数倍違う。
- **D32_FLOAT の直接 SRV サンプル** は backend 依存。R32_FLOAT コピー経路の方がポータブル。
- **indirect dispatch** は Diligent で `DispatchComputeIndirect` が使えることを確認（既存コードに間接 dispatch の実績は未確認）。
- 3D シーン描画のあと、正しいタイミングで深度コピー＋DOF チェーンを挟む編成が `ArtifactCompositionRenderController::renderOneFrameImpl` 側の改修を伴う可能性。

## 次のステップ

1. **P0 から始める**（深度 SRV なしでは何も動かない）。
2. P1 で prepass+main(cheap) を最小動作させ、そこから P2/P3 で本格化。
3. P4/P5 でカメラパラメータと UI を接続。
4. 余力で B1/B2。

## 2026-07-25 実装監査

- `ArtifactCameraLayer` には depthOfField／aperture／focusDistance 等の意図設定があり、作成ダイアログと camera overlay の DOF 表示も確認できる。
- 一方、DOF の必須前提である D32 深度からサンプル可能な R32 深度 SRV への live コピー、tile／neighborhood／prepass／main／postfilter／upsample の compute chain、indirect dispatch は実装を確認できない。
- Wicked Engine shader 資産の Diligent 向け移植、CoC uniform の実描画接続、DOF 有効時の実効果も未検証である。
- したがって本マイルストーンは、カメラ設定・表示の先行実装はあるが、DOF/Lens Blur 本体は未着手の設計段階と判定する。

## Static audit follow-up (2026-07-29)

現行ソースを再確認した。カメラの DOF 設定、作成 UI、overlay 表示、PreviewQuality の enable flag は存在する。一方、live depth SRV、Diligent compute chain、CoC の実描画接続は確認できない。

### 判定

マイルストーン全体を `Not Started` とするのはカメラ設定基盤を過小評価するため、ステータスを「カメラ基盤の部分実装／DOF render path 未実装」に更新した。DOF 本体は未完了のまま扱う。
