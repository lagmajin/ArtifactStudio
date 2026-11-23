# Diligent プレビュー ボトルネック メモ

**最終更新:** 2026-08-27
**作成者:** Muse Spark (codebase-walk + cpp-diligent-gpu-studio Investigative)
**対象:** `ArtifactCompositionRenderController` / `DiligentImmediateSubmitter` / `GPUTextureCacheManager` / `ArtifactIRenderer` 経由のDiligentプレビュー

## 結論

ハードウェア `nsys` / `Nsight Graphics` 計測は未取得。`docs/analysis/PERFORMANCE_ASYNC_GPU_OPTIMIZATION_2026-08-06` / `RENDER_PERF_HOTPATH_2026-07-08` / `PERF_COMPOSITION_EDITOR_DILIGENT_2026-06-16` の静的トレースと `Insight.md` の監査に基づく限り、**N層 `2N+1` フルスクリーンGPUパスが最大のボトルネック**。以下5件がDiligent境界で確定。

## ボトルネック一覧

| # | 事象 | ファイル:行 | 推定コスト | Diligent/D3D12/Vulkan |
|---|------|-----------|------------|---------------------|
| 1 | **N層 `2N+1` フルスクリーン**: 通常blend以外で `convertLayerToFloat(RGBA8→R8G8B8A8_32F) + blendLayers + Flush()` を層ごとに実行 | `Artifact/src/Widgets/Render/ArtifactCompositionRenderController.cppm:7601-7724`, `Artifact/src/Render/ArtifactIRenderer.cppm:2970 flush()` | 1080pで `32400` threadGroups/層 (N×) | `RESOURCE_STATE_TRANSITION` で両バックエンド共通。`Flush()`が2N回。Vulkanは `FinishFrame()` のheap回収も絡む |
| 2 | **Adjustment層 同期readback** | `Artifact/src/Render/ArtifactCompositionViewDrawing.cppm:1240`, `Artifact/src/Render/ArtifactIRenderer.cppm:1889 CopyTexture→Flush→Fence Wait→Map` | **2-8ms stall/層** | `USAGE_STAGING` + `EnqueueSignal/Wait` は共通 |
| 3 | **Sprite 1枚ごと `CreateTexture IMMUTABLE`** | `Artifact/src/Render/PrimitiveRenderer2D.cppm:832`, `Artifact/src/Render/GPUTextureCacheManager.cppm:850`, `DiligentUploadCoordinator.cppm:170 processPending(8,64MiB)` | 4Kで **64MB/フレーム** QImage往復。超過は1フレームmiss | D3D12は `ScheduleTextureUpdate`、Vulkanは `CreateTextureFromVulkanImage` ゼロコピーfast-pathありで差分 |
| 4 | **MotionPath `>1000ms`** | `Artifact/src/Widgets/Render/ArtifactCompositionRenderController.cppm:36819 getGlobalTransformAt()×300` | 単独でフレームを潰す | CPU側。`showMotionPathOverlay_`でガード済み |
| 5 | **`dynamic_cast` 30回/層** | `Artifact/src/Render/ArtifactCompositionViewDrawing.cppm:610` | **54K回/秒 @30層×60fps** | CPU |

その他: `buildLayerSurfaceCacheKey` 27フィールド無条件生成(B1)、`batchSolidRect`無効(`DiligentImmediateSubmitter.cppm:710 kSolidRectBatchValidated=false`)、budget超過時の1フレームmiss、重複 `setCanvasSize/Zoom/Pan` N回。

## Active backend

- `Artifact/src/Render/DiligentDeviceManager.cppm:859` `ARTIFACT_RENDER_BACKEND=auto` → **D3D12優先、Vulkanフォールバック**のpeer構成
- `D3D12Agility` / `vkDevice/vkQueue` は `DiligentDeviceManager.ixx:134` で露出。`1` は両バックエンドで同一バリア、`3` はVulkanで差が出る

## Lifetime / Sync 影響

- `1` は `DiligentImmediateSubmitter::submit()` のdeferredCtxライフサイクルと `m_readbackRing[2]`/`m_asyncReadbackRing[3]` に波及
- `Flush` を削ると compute→blend のRTV-UAVハザードが再発（`unbindColorTargetsForCompute` の `SetRenderTargets(0,nullptr)` バリアに依存）

## 検証ギャップ

- `IQuery(QUERY_TYPE_TIMESTAMP)` と `FrameDebugSnapshot(renderGpuFrameMs, RenderCostStats{psoSwitches,srbCommits,bufferUpdates})` は `2026-06-16` で配線済みだが、**ベースライン4 compositionsの計測未実行**
- `present` が常時VSync（`ArtifactIRenderer.cppm:3115 sc->Present()` 引数なし）のため、**present-pacedかshader-boundかの切り分けに uncapped/offscreenベンチが必要**
- ビルド検証は `meshoptimizer` 環境問題で保留中（`out/build/x64-Debug` classic vcpkgで新規portの `find_package` が通らず、インストール後に消失する現象あり。X:側はmanifest modeで正常）

## 次の1手

1. `FrameDebugSnapshot` + `nsys stats --report vulkan_api_sum,osrt_sum,nvtx_sum` の2レーンで 30層Mixed compositionの `2N+1` コストを数値化
2. その後に `1` の最小変更を設計（例: Normal-only fast-path拡張 or ping-pong削減）。いずれも `TRANSITION` バリアと `FinishFrame()` への影響をD3D12/Vulkan両laneで再確認

ビルド許可が得られ次第、計測から再開する。

## 関連文書

- `docs/analysis/PERFORMANCE_ASYNC_GPU_OPTIMIZATION_2026-08-06.md` (High A-X)
- `docs/analysis/RENDER_PERF_HOTPATH_INVESTIGATION_2026-07-08.md` (B1-B5)
- `docs/analysis/PERF_COMPOSITION_EDITOR_DILIGENT_INVESTIGATION_2026-06-16.md` (N+1)
- `docs/analysis/GPU_OFFLOAD_TARGETS_2026-08-08.md`
- `Insight.md` 2026-08-21 GPU blend / 2026-08-24 glyph PSO / 2026-06-16 GPU timing配線
- `references/artifact-diligent.md` (Diligent/D3D12/Vulkan peer policy)
