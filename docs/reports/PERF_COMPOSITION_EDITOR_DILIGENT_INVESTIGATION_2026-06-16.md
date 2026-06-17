# Composition Editor (Diligent) Performance Investigation

**Date**: 2026-06-16
**Author**: CommandCode (dev/commandcode-2026-06-16 worktree)
**Scope**: Composition Editor viewport 描画と per-layer 合成経路 (Diligent バックエンド) の計測可能性と改善ポイントの整理
**Status**: Investigation only — コード変更なし

---

## 1. 結論サマリ

| # | 結論 |
|---|------|
| 1 | **計測フックは既に揃っている**。`frameDebugSnapshot()` / `frameCostStats()` / `lastFrameGpuTimeMs()` / `Profiler::frameHistory(N)` / `GPUTextureCacheManager::stats()` が公開済み。新規 instrumentation 追加は不要 |
| 2 | **最有望ボトルネックは per-layer N+1 経路** (`CompositionRenderController.cppm:7601-7724`)。1 レイヤーごとに `setOverrideRTV → clear → draw → flush → unbind → convertLayerToFloat → blendLayers → swapAccumAndTemp` を N 回実行 |
| 3 | **次点は per-layer constant buffer 再設定**。`setCanvasSize/Zoom/Pan` をレイヤーごとに毎回呼ぶ (L7601-7620) が、フレーム内で同じ値のはず |
| 4 | **dynamic_cast 連鎖 (B1) も CPU ホットパス**。30+ casts/layer/frame、合成 30L×60fps で 54K casts/sec の試算あり |
| 5 | **`beginFrameGpuProfiling()` が定義のみで未呼び出し**。`lastFrameGpuTimeMs()` の値が更新されているか未検証 |

## 2. 推奨アクション

### 2.1 フェーズ 1 — 計測 (リスク最小・成果確実)

worktree 内に小さな harness を作り、以下を取得:

| 計測対象 | API | 比較軸 |
|---|---|---|
| 1 フレームの draw call / PSO 切替 / buffer 更新 | `frameCostStats()` | layer 数 1 / 5 / 10 / 20 |
| CPU pass 別 ms | `Profiler::frameHistory(N)` | idle / gizmo 操作中 / playback |
| GPU frame time | `lastFrameGpuTimeMs()` | 同上 |
| Texture cache 効率 | `GPUTextureCacheManager::stats()` | cache hit rate |
| Constant buffer 更新回数 | `bufferUpdates` カウンタ | フレーム内期待値 1 に対し現状 |

合成 composition (Solid/Image/Text 混在) を 4 種類用意し、各 30 秒流す。
**変更ゼロでデータ獲得 → 改善 ROI の根拠に直結**。

2026-06-16 追記:

- `App Debugger > Export` と `Debug Render Harness > Save Report` に `Perf Baseline` 要約を追加した。
- 出力対象は既存 `FrameDebugSnapshot` の `renderLastFrameMs` / `renderAverageFrameMs` / `renderGpuFrameMs` / `renderCost` / layer counts。
- `renderGpuFrameMs == 0` かつ draw call がある場合は `gpuTimer: not-updating` と読めるため、`beginFrameGpuProfiling()` 接続確認の入口になる。
- `ArtifactIRenderer::beginFrameGpuProfiling()` / `endFrameGpuProfiling()` を公開し、`CompositionRenderController::renderOneFrameImpl()` の既存 frame cost guard から呼ぶようにした。次回 baseline で `renderGpuFrameMs` が更新されるか確認する。
- `ArtifactIRenderer::setCanvasSize()` / `setZoom()` / `setPan()` は同値再設定をスキップするようにした。呼び出し順は維持したまま、フレーム内の重複 viewport state 更新を減らす狙い。

### 2.2 フェーズ 2 — 最小改善 (効果確実・低リスク)

優先度順:

| 優先 | 改善 | 影響 | リスク |
|---|---|---|---|
| ★1 | `setCanvasSize/Zoom/Pan` をフレーム内 1 回に集約 (L7601-7620) | `bufferUpdates` が N+1 → 1 に激減 | 極小 (setter の冪等性確認のみ) |
| ★2 | dynamic_cast 連鎖を `switch (layer->layerType())` に置換 (`CompositionViewDrawing.cppm:610-794`) | CPU ホットパス 30%〜50% 削減可能性 (要計測) | 小 (派生追加時のレビュー必要) |
| ★3 | `interactivePreviewDownsampleFloor_` 4 → 2 に下げる (`CompositionRenderController.cppm:1612-1613, 4124-4125`) | 編集中の体感品質向上 / GPU コスト増 (要トレードオフ計測) | 小 (UI 設定で切替可能にすべき) |
| ○4 | `beginFrameGpuProfiling()` を `renderOneFrameImpl()` から呼ぶよう接続 (L6784-6790 周辺) | GPU 計測が機能するようになる | 極小 (値取得経路の確認のみ) |

### 2.3 フェーズ 3 — 大型改善 (深手・要 PSO 検討)

| 改善 | 影響 | リスク |
|---|---|---|
| N+1 per-layer 経路の batch 化: 複数レイヤーを 1 dispatch に統合 | N+1 → 1、GPU パイプラインストール大幅減 | 中〜大 (PSR 整合性、layer 並びと compositing order の整合) |
| `convertLayerToFloat` を layer 単位でなく batch に統合 | dispatch overhead 削減 | 中 |
| Readback 非同期経路の徹底 (`readbackToImageAsync`) | RAM preview 安定化 | 中 (worker thread の枯渇管理要確認) |

---

## 3. 調査で押さえた構造

### 3.1 widget 階層

| widget | 役割 | 描画 |
|---|---|---|
| `ArtifactCompositionEditor` (QWidget) | UI shell (toolbar, gizmo, context menu) | paint しない |
| `CompositionViewport` (QWidget+WA_PaintOnScreen) | inline native paint window | **Diligent (native) で描かれる** |
| `ArtifactCompositionRenderWidget` (QWidget+tbb::task_group) | 単独 viewport (新経路) | **Diligent (native) で描かれる** |
| `CompositionRenderController` | 上記 2 つの共有 controller | 直接描かない (renderer 呼び出し) |

### 3.2 2 種類の「Diligent レイヤー」

- **① コンポジション内の各レイヤー** (Solid/Image/Video/Text/3D): `ArtifactIRenderer::drawSprite / drawSpriteTransformed / drawRectLocal` 経由
- **② Viewport の SwapChain present**: `WA_PaintOnScreen` で Qt paint をバイパスし、Diligent が host HWND に present

両者は「① レイヤー → ② SwapChain present」の前後に接続。

### 3.3 1 フレームで N レイヤー描画時に走っている処理

| 操作 | N との関係 | 場所 |
|---|---|---|
| `setOverrideRTV(layerRTV)` | N | `CompositionRenderController.cppm:7601, 7626` |
| `clear()` (layerRTV) | N | L7603, 7621, 7628 |
| `flush()` (graphics→compute 境界) | N | L7644 |
| `unbindColorTargetsForCompute()` (barrier) | N | L7651 |
| `convertLayerToFloat(...)` (compute: RGBA8→RGBA32F) | N | L7655 |
| `blendLayers(...)` (compute dispatch) | N | L7671 |
| `swapAccumAndTemp()` (UAV→SRV 遷移) | N | L7718 |
| `drawLayerForCompositionView` 内の sprite 描画 | N | L7637 |
| `setCanvasSize/Zoom/Pan` | N (重複設定) | L7601-7620 |

背景 seed 描画でも 1 回 (a)-(h) 追加 (L7484-7514)。**N レイヤーで N+1 回の layer→float convert + blend dispatch + barrier**。
1920×1080 で 240×135 = **32,400 thread groups/blend**。

### 3.4 過去 perf 文書の整理

#### `docs/perf/PERF_GAP_ANALYSIS_2026-06-04.md` (最重要)

| カテゴリ | ボトルネック | 推定影響 |
|---|---|---|
| 🔴 A1 | GPU パイプライン未完成、QImage/QPainter fallback 多 | 最大 |
| 🔴 A2 | readback ガンマ補正 | 5-15 ms |
| 🔴 A3 | Adjustment Layer readback | 5-20 ms |
| 🟠 B1 | dynamic_cast 連鎖 30+/layer/frame | 54K casts/sec @ 30L×60fps |
| 🟠 B2 | QImage convertToFormat | per-call |
| 🟠 B3 | フレーム精度シーク O(N) | — |
| 🟠 B4 | renderOneFrame スケジューラ | 16-48 ms 遅延 |
| 🟠 B5 | 再生 QImage 配送 | — |
| 🟠 B6 | OpenCV チャンネル分解 | — |
| 🟢 C1 | FrameCache O(N) evict | — |
| 🟢 C2 | スタートアップ直列 | — |
| 🟢 C3 | GPU テクスチャ再アップロード | — |
| 🟢 C4 | キャッシュキー文字列連結 | — |

#### `docs/perf/COMPOSITE_EDITOR_UI_UPDATE_MECHANISM_2026-04-25.md`

- スケジューリング設計の問題 (CPU 側) と断定
- ギズモドラッグの三重スロットル (Editor 940/1122 + Controller 3684 + renderOneFrame 3878) — 旧コード、現状は timer 化で解消方向
- 操作中 `interactivePreviewDownsampleFloor_ = 4` の品質ジャンプ
- `panBy()` の毎フレーム `invalidateBaseComposite()` (L2282)

#### `docs/perf/STRUCTURAL_PERF_ISSUES_2026-04-25.md`

- **G**: dynamic_cast 連鎖 30+/layer/frame → `switch (layer->layerType())` 置換提案
- **H**: 調整レイヤー毎フレーム GPU readback
- **I**: 毎 draw call `QImage::convertToFormat`
- **J**: LayerChangedEvent 購読の重処理 (ギズモ中 1 mouseMove で最大 4 回再描画)

#### `docs/codereviews/ARTIFACT_COMPOSITION_RENDER_CONTROLLER_2026-05-11.md`

- **P0-1**: メモリ管理 → `unique_ptr<Impl>` 化
- **P0-2**: `QImage` in hot path (`buildRasterizedSurfaceBuffer`) → `ImageF32x4_RGBA` 化 (AGENTS.md 方針とも一致)
- **Medium-5**: Cache invalidation 不整合
- **Medium-6**: Event bus coupling

#### `docs/technical/GPU_RENDER_OPTIMIZATION_TECHNIQUES_AE_STYLE.md`

AE 風 20 テクニックのうち、コンポジションレイヤーループで未適用:
- ❌ 1 レイヤー 1 描画コマンド (現状 N 描画 + N ブレンド dispatch)
- ❌ 描画毎の定数バッファ再作成
- ❌ 定数バッファ リングアロケーション
- ❌ フレーム間頂点バッファキャッシュ
- ❌ 部分ダーティ領域更新
- ❌ 不透明レイヤー早期 Z リジェクト
- ❌ バッチ併合

### 3.5 bugs から読み取れる perf 影響

| Bug | 修正後の perf 効果 / 残課題 |
|---|---|
| `BUG_FIX_COMPOSITION_RENDER_OVERLAY_OVERDRAW_2026-04-09` | 不要な `drawSolidRect` 上書き削除済 |
| `BUG_PLANE_LAYER_GPU_BLEND_ON_2026-05-16` | `LayerFloat (RGBA32F)` 追加済、ただし `convertLayerToFloat` コストは N 回のまま |
| `BUG_PARTICLE_LAYER_INVISIBLE_2026-06-14` | 初期化失敗時 `QPainter` fallback 経由の CPU コストが懸念 |
| `BUG_LAYER_OPACITY_NOT_APPLIED_2026-04-18` | opacity=0 で continue する早期スキップは対応済 (L7596-7600) |
| `BUG_RENDER_SCHEDULER_THREAD_FLOOD_2026-04-18` | スタートアップ時 0.5-4 秒フリーズ、composition 作成時の thread pool 生成遅延で対処方針 |

### 3.6 Diligent 特有の罠 (`DILIGENT_ENGINE_TRAPS_2026-06-14.md`)

1. **STATIC vs DYNAMIC shader variable**: `setBuffer()` が後から効かない罠 → `LayerBlendPipeline` の SRB 設定がどちらを使っているか `psoSwitches` で観測すべき
2. **UAV 書き込み後の readback**: `CopyTextureAttribs(RESOURCE_STATE_TRANSITION_MODE_TRANSITION)` 明示 → `swapAccumAndTemp` の barrier 整合性に直結
3. **readback 同期**: `Flush()` だけでなく `WaitForIdle()`、`MapTextureSubresource` の Stride 確認
4. **リソース名**: `m_layerRT = "LayerRenderTarget"` のみ、accum/temp の名前付与推奨
5. **迷ったら既存パターンに合わせる**: `Compute.cppm` / `BlurEffect.cppm` / `GPURayTracer.cppm` / `ArtifactIRenderer.cppm`

---

## 4. 計測フック一覧 (再掲)

| API | 場所 | 内容 |
|---|---|---|
| `ArtifactIRenderer::frameCostStats()` | `Artifact/include/Render/ArtifactIRenderer.ixx:96` | `{ drawCalls, indexedDrawCalls, psoSwitches, srbCommits, bufferUpdates }` |
| `ArtifactIRenderer::lastFrameGpuTimeMs()` | `.ixx:97` | GPU frame time (ms) |
| `ArtifactIRenderer::presentAttemptCount/success/failure/skipped` | `.ixx:91-95` | present 統計 |
| `CompositionRenderController::lastFrameTimeMs() / averageFrameTimeMs()` | `CompositionRenderController.cppm:5406-5412` | 直近 120 frame 移動平均 |
| `CompositionRenderController::frameDebugSnapshot()` | L4993 付近 | 全統計スナップショット |
| `ArtifactCore::Profiler::instance().frameHistory(N)` | `PerformanceProfiler.ixx:191-200` | 過去 N frame の CPU scope 統計 |
| `GPUTextureCacheManager::stats()` | `CompositionRenderController.cppm:5206` | entries / bytes / hits / misses |
| `RenderDoc capture` | **無し** | コード上にフックなし。docs/bugs/CRITICAL_RENDER_MEDIA_SMOKE に言及のみ |
| `Tracy / perfetto` | **無し** | `TraceRecorder` クラスは `ArtifactCore/include/Diagnostics/Trace.ixx` にあり、`recordFrameDebugSnapshot` 経路のみ |
| `Vulkan validation` | 起動時 log のみ | 詳細は `COMPOSITION_EDITOR_PERF_AND_COMPUTE_HYPOTHESES_2026-03-24` |

---

## 5. 過去の修正履歴 (要温故知新)

`docs/bugs/COMPOSITION_EDITOR_PERFORMANCE_2026-03-26.md` の 10 件中、多くが既修正:

| # | 旧問題 | 現状の対策 |
|---|---|---|
| 1 | `QImage::cacheKey()` ベース cache 毎フレームミス | `computeImageContentKey` で content hash 化済 (`PrimitiveRenderer2D.cppm:30-72`) |
| 2 | Surface cache key も `cacheKey()` 依存 | `buildLayerSurfaceCacheKey` で layerID ベース化 (`CompositionViewDrawing.cppm:150-194`) |
| 3 | `drawCircle` 過剰描画 (128 GPU cycles) | packet 化済 |
| 4 | GPU readback ストール | `readbackToImageAsync` で worker thread 化 |
| 5 | シグナルストーム/二重描画 | `renderScheduled_` guard + `renderTickTimer_` 化 |
| 6 | `renderOneFrame` 過剰呼び出し | dirty flag + QTimer 駆動化 |
| 7-8 | `drawRectOutline` / `drawCrosshair` 個別描画 | packet 化済 |
| 9 | ビデオ QImage 毎フレーム alloc | 改善履歴あり |
| 10 | OpenCV CPU ホットパス | 部分的改善 |

**しかし #1〜#10 の「1 レイヤー 1 描画 + barrier + convert + blend dispatch」構造は概ねそのまま残存**。

---

## 6. 次のステップ提案

1. **本書の合意を取る** — 計測フェーズ 1 に進むか、別方向 (A/C タスク) を優先するか
2. 合意後、worktree 内に `tools/perf_dump/` (仮) を作成
3. 合成 composition 4 種 (Solid 10 / Image 5 + Text 5 / Video 1 / 混在 20) で 30 秒ずつ流す
4. 結果を `docs/reports/PERF_COMPOSITION_EDITOR_DILIGENT_BASELINE_2026-06-16.md` (仮) として記録
5. 改善対象 (★1〜★3) の優先度を実データで確定

## 7. 関連ファイル参照

| 役割 | パス |
|---|---|
| 描画ループ | `Artifact/src/Widgets/Render/ArtifactCompositionRenderController.cppm` (特に 7601-7724) |
| レイヤー描画 dispatch | `Artifact/src/Render/ArtifactCompositionViewDrawing.cppm` (104, 610-794) |
| 低レイヤ API | `Artifact/src/Render/ArtifactIRenderer.cppm` (1486-1520, 1583-1688, 2129-2150, 225-235) |
| Sprite cache | `Artifact/src/Render/PrimitiveRenderer2D.cppm` (30-72, 1000-1095, 165-180) |
| Layer pipeline | `Artifact/src/Render/ArtifactRenderLayerPipeline.cppm` |
| Blend shader | `ArtifactCore/src/Render/LayerBlendComputeShader.ixx` (18 blend modes) |
| 描画統計 | `Artifact/include/Frame/FrameDebug.ixx:32-39` |
| Profiler | `ArtifactCore/include/Utils/PerformanceProfiler.ixx:158-200, 512-525` |
| Channel order 罠 | `docs/technical/IMAGE_FORMAT_CONVENTIONS.md`, `docs/technical/RGB_BGR_CHANNEL_ORDER_REFERENCE.md` |
| 罠メモ | `docs/shared/ai-tech-memos/DILIGENT_ENGINE_TRAPS_2026-06-14.md` |
| 過去 perf 解析 | `docs/perf/PERF_GAP_ANALYSIS_2026-06-04.md`, `docs/perf/COMPOSITE_EDITOR_UI_UPDATE_MECHANISM_2026-04-25.md`, `docs/perf/STRUCTURAL_PERF_ISSUES_2026-04-25.md` |
| codereview | `docs/codereviews/ARTIFACT_COMPOSITION_RENDER_CONTROLLER_2026-05-11.md` |

---

## 8. `QImage` hot path 分類

`QImage` hit 38 は、全部が等価ではない。今回の切り分けでは次の 3 群に分けるのが妥当。

### 8.1 Render Loop Core

- `Artifact/src/Widgets/Render/ArtifactCompositionRenderController.cppm`
- `Artifact/src/Layer/ArtifactVideoLayer.cppm`
- `Artifact/src/Layer/ArtifactTextLayer.cppm`
- `Artifact/src/Layer/ArtifactSvgLayer.cppm`
- `Artifact/src/Layer/ArtifactSDFLayer.cppm`

代表例:

- `surface = processed.toQImage()`
- `QImage surface(surfaceSize, ...)`
- `const QImage img = imageLayer->toQImage()`
- `const QImage svgImage = svgLayer->toQImage()`
- `const QImage textImage = textLayer->toQImage()`
- `auto matteResolverLambda = ... -> QImage`

ここは `QImage` を減らす本丸。

#### 8.1.1 すぐ削る候補

- `ArtifactCompositionRenderController.cppm` の `QImage surface(surfaceSize, ...)`
- `ArtifactCompositionRenderController.cppm` の `processed.toQImage()`
- `ArtifactCompositionRenderController.cppm` の `surface = surface.convertToFormat(...)`
- `ArtifactCompositionRenderController.cppm` の `matteResolverLambda` 内の `toQImage()` 連鎖

理由:

- frame loop の中で確実に走る
- 変換とメモリ確保が重なる
- boundary ではなく再合成中継なので削減効果が大きい

### 8.2 Render-adjacent Capture

- `Artifact/src/Widgets/Render/ArtifactCompositionEditor.cppm`

代表例:

- `captureCompositionScreenshot()` の `readbackToImage()`
- `saveScreenshotImage()` の `convertToFormat()`
- `selectedLayerDebugImage()` の `toQImage()` 呼び出し

ここは render loop 本体ではないが、`QImage` の往復が明確なので次点で削減余地がある。

#### 8.2.1 当面残す候補

- `captureCompositionScreenshot()` の `readbackToImage()`
- `saveScreenshotImage()` の `convertToFormat()`
- `selectedLayerDebugImage()` の `toQImage()` 呼び出し

理由:

- screenshot / debug は UI 境界としての意味が強い
- まずは render core を軽くしたほうが効果が大きい
- ここを削るなら export / debug 仕様を一緒に整理したほうが安全

### 8.3 Debug / Inspector / Test Boundary

- `Artifact/src/Widgets/Render/ArtifactSoftwareRenderInspectors.cppm`
- `Artifact/src/Widgets/Render/ArtifactSoftwareRenderTestWidget.cppm`
- `Artifact/src/Widgets/Render/ArtifactLayerCompositeTestWidget.cppm`
- `Artifact/src/Widgets/ArtifactProjectManagerWidget.cppm`

ここは `QImage` が多くても、preview / inspector / test / asset browsing の boundary なので、まずは hot path と分離して扱う。

#### 8.3.1 残す前提の候補

- `ArtifactSoftwareRenderInspectors.cppm`
- `ArtifactSoftwareRenderTestWidget.cppm`
- `ArtifactLayerCompositeTestWidget.cppm`
- `ArtifactProjectManagerWidget.cppm`

理由:

- これらは比較・検証・プレビュー用途で、hot path の主犯ではない
- まずは boundary を明示したまま性能問題の大半を潰せる

### 8.4 Boundary-only candidates

- `Artifact/src/Widgets/Render/ArtifactCompositionEditor.cppm` の screenshot save / readback
- `Artifact/src/Layer/ArtifactVideoLayer.cppm` の decode / compatibility wrappers

ここは `QImage` を完全排除するより、明示的な変換境界として残すほうが現実的。

### 8.5 ざっくり優先順位

1. `ArtifactCompositionRenderController.cppm` の `QImage` 連鎖
2. `ArtifactVideoLayer.cppm` / `ArtifactTextLayer.cppm` / `ArtifactSvgLayer.cppm`
3. `ArtifactCompositionEditor.cppm` の capture / readback
4. `SoftwareRenderInspectors` / `SoftwareRenderTest` / `ProjectManager` の boundary `QImage`

---

**更新履歴**

- 2026-06-16: 初版作成 (CommandCode)。コード変更なし、計測計画と改善ポイント整理のみ
- 2026-06-17: `QImage` hit を hot path / UI-debug-export / boundary に分割
