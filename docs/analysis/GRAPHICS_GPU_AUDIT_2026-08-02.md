# Graphics / GPU / Diligent 詳細監査

**日付**: 2026-08-02
**調査範囲**: ソースコード直接読み込み（~30ヘッダ）

---

## 1. GPUComputeContext — デバイス抽象 🟢 85%

`Graphics/include/GPUComputeContext.ixx`

| 機能 | 状態 | 備考 |
|------|------|------|
| Diligent IRenderDevice ラップ | ✅ | 生ポインタ + RefCntAutoPtr 併用 |
| D3D12 デバイスアクセス | ✅ | `D3D12RenderDevice()` / `D3D12DeviceContext()` |
| Vulkan デバイスアクセス | ✅ | `VKDeviceResources()` |
| ランタイムシェーダーコンパイル | ✅ | `CompileShader(source, type, entryPoint, &shader)` |
| GPU 情報 | ✅ | `GPUInfo` でベンダー/メモリ/機能を取得 |
| DeviceResources 構造体 | ✅ | `{IRenderDevice*, IDeviceContext*}` 生ポインタビュー |

**設計上の注意**: MSVC C1116 回避のため `RefCntAutoPtr.hpp` を意図的にインクルードしていない（コメント明記）。生ポインタでやり取りし、呼び出し側が AddRef/Release 管理。

---

## 2. MeshRenderer — GPU メッシュレンダラー 🟢 90%

`Graphics/include/MeshRenderer.ixx` (159行)

| 機能 | 状態 |
|------|------|
| インスタンシング描画 | ✅ `draw(pContext, instanceCount)` |
| ジオメトリ更新 | ✅ `updateMeshGeometry(positions, normals, uvs, indices)` |
| PBR テクスチャ | ✅ baseColor / metallicRoughness / normal / emission / occlusion / opacity |
| シーンライト | ✅ `setSceneLights(Light)` |
| View / Projection 行列 | ✅ `setViewMatrix` / `setProjectionMatrix` / `setPrevious*` |
| 透過パス | ✅ `setTransparentPass(bool)` |
| PSO キャッシュ | ✅ `setPipelineStateCache(IPipelineStateCache*)` |
| レンダーターゲットフォーマット | ✅ `setRenderTargetFormat(TEXTURE_FORMAT)` |
| フレームコスト統計 | ✅ `setFrameCostStats(RenderCostStats*)` |

**ShaderConstants**: `viewMatrix[16], projMatrix[16], prevViewMatrix[16], prevProjMatrix[16]`。
モーションベクター用に前フレームの行列も送っている。

---

## 3. RenderGraph — リソースグラフ 🟢 80%

`Graphics/include/RenderGraph.ixx`

| 機能 | 状態 |
|------|------|
| リソース管理 | ✅ `addResource(RenderResourceDescriptor)` → `RenderResourceHandle` |
| パス管理 | ✅ `addPass(RenderPassDescriptor)` → `RenderPassHandle` |
| リソース種別 | ✅ Texture / Buffer |
| ライフタイム | ✅ Transient / Persistent / External |
| キュー種別 | ✅ Graphics / Compute / Copy |
| 診断パス状態 | ✅ Scheduled / Disabled / Blocked |
| リソース依存関係 | ✅ reads / writes 配列 |
| トポロジカルソート | 🔴 **未実装**（注釈: 「ユーザーが正しい順で追加したと仮定」） |

**コード内コメント問題**: 成熟度分析で指摘されていた「トポロジカルソート未実装」。依存順が狂うと不正描画。

---

## 4. RenderPipelineFoundation — GI パイプライン 🟡 65%

`Graphics/include/RenderPipelineFoundation.ixx`

| 機能 | 状態 |
|------|------|
| GI リソース | ✅ Depth, Normal, Motion, DirectLighting, IndirectLighting, History |
| GI パス | ✅ Reconstruct, ScreenSpaceGather, DepthPyramid, BilateralDenoise, TemporalResolve, Composite |
| PointwiseFusion アダプタ | ✅ `PointwiseRenderGraphAdapter::append()` |
| 解像度 | ✅ フル解像度（Depth/Motion/DirectLighting）と作業解像度（その他）を分離 |

SSGI（Screen Space Global Illumination）のパイプラインが実装されているが、コードの成熟度はこれから。
`GIRenderGraphAdapter::append()` は `context.plan().empty()` の場合は早期リターン。

---

## 5. PSOCache — PSO キャッシュ 🟢 75%

`Graphics/include/PSOCache.ixx`

| 機能 | 状態 |
|------|------|
| ベンダー別キャッシュ | ✅ `getPSOCacheDirectory(vendor, deviceName)` |
| キャッシュフォルダ自動作成 | ✅ `createIfMissing` |
| 文字列ベース | ✅ UniString キー |

シンプルだが、Diligent の `IPipelineStateCache` と連携して起動時の PSO コンパイルを省略する目的には十分。

---

## 6. GPUTexture — 🟡 30%

`Graphics/include/GPUTexture.ixx` (25行)

| 機能 | 状態 |
|------|------|
| 幅・高さ取得 | ✅ `GetWidth()` / `GetHeight()` |
| ムーブセマンティクス | ✅ |
| テクスチャ作成 | 🔴 メソッド不在（PIMPL 内？） |
| フォーマット指定 | 🔴 不在 |
| Mipmap | 🔴 不在 |

必要最小限。実際のテクスチャ作成は Diligent の `IRenderDevice::CreateTexture()` を直接使っている可能性が高い。

---

## 7. GPUTextureCacheManager — 🟢 85%

`Graphics/include/GPUTextureCacheManager.ixx`（Artifact 層）

`Artifact/include/Render/GPUTextureCacheManager.ixx`

GPUテクスチャキャッシュ。キー（signature + ownerId）でテクスチャを再利用。
`acquireOrCreate` / `isValid` / `bindingRecord` / `clear`。

---

## 8. ParticleRenderer — 🟡 50%

`Graphics/include/ParticleRenderer.ixx`

`ParticleData` + `ParticleCompute`（GPUコンピュートでパーティクル物理を計算）と連携するが、インターフェースが最小限。MeshRenderer に比べて機能が不足。

---

## 9. RayTracingManager — 🟡 40%

`Graphics/include/RayTracingManager.ixx` (77行)

| 機能 | 状態 |
|------|------|
| BLAS 作成/更新 | ✅ `createOrUpdateBLAS(id, geometry)` |
| TLAS 構築 | ✅ `buildTLAS(pContext)` |
| ユニットクワッドトレース | ✅ `traceUnitQuad(pContext, w, h)` |
| パイプライン状態 | ✅ Capability 構造体（19フィールドの状態追跡） |
| 出力 SRV | ✅ `traceOutputSRV()` |
| サポート検出 | ✅ `isSupported()` |

HW レイトレーシング（DXR/Vulkan RT）の基盤はあるが、ユニットクワッドのみ。複雑なシーンへの応用は未完。

---

## 10. Diligent 依存の注意点

D3D12/DX12 backend には「AI にとって読み違えやすいシビアなコード」という警告が AGENTS.md に明記されている。

全 Graphics モジュールが `using namespace Diligent;` で Diligent の型（`IRenderDevice*`, `IDeviceContext*`, `ITextureView*` など）を広範に使っている。

MSVC 14.51 C1116 の回避のため、多数のファイルで `RefCntAutoPtr.hpp` を意図的にインクルードせず生ポインタでやり取り。呼び出し側での所有権管理に注意が必要。

---

## 全体評価

| コンポーネント | スコア | 所見 |
|---------------|--------|------|
| GPUComputeContext | 🟢 85% | Diligent ラップ。D3D12+Vulkan。ランタイムコンパイル |
| MeshRenderer | 🟢 90% | フル PBR 対応。インスタンシング。モーション対応 |
| RenderGraph | 🟢 80% | リソースグラフ。トポロジカルソート未実装 |
| GIRenderPipeline | 🟡 65% | SSGI パイプライン実装中。プロトタイプ段階 |
| PSOCache | 🟢 75% | シンプルで十分 |
| GPUTexture | 🟡 30% | 最小限。実際の作成は Diligent 直 |
| GPUTextureCacheManager | 🟢 85% | キーベースのテクスチャ再利用。よくできている |
| ParticleRenderer | 🟡 50% | インターフェース最小限 |
| RayTracingManager | 🟡 40% | HW RT 基盤あり。ユニットクワッドのみ |
| PointwiseFusion | 🟢 80% | ポイントワイズエフェクト融合。GPU最適化 |

**総合**: 🟡 70% — Diligent Engine 統合がしっかりしている。RenderGraph のトポロジカルソート不在と GL パイプラインの未完成が主な課題。HW レイトレーシング基盤はあるが応用はこれから。
