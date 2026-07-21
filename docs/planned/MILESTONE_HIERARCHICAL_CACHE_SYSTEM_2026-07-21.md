# Milestone: Hierarchical Cache System

**Date:** 2026-07-21 (コード実態調査に基づき全面更新)  
**ステータス:** In Progress

## Goal

既存の

1. `Composition Editor Cache`（Layer Surface + Static GPU）
2. `GPUTextureCacheManager`
3. `RAM Preview Controller`
4. `Disk Cache`（baseline 実装済み、manifest は継続）

をバラバラの最適化として増やすのではなく、
**RAM とディスクが連携する階層 cache system** として再整理する。

最終目標は、

- 編集中は軽く反応する
- RAM preview は確保済みフレームを保証再生できる
- RAM を超えた結果は disk に退避できる
- restart 後も再利用できる
- stale cache を誤表示しない

状態へ持っていくこと。

---

## Current Reality (2026-07-21 コード実態調査)

### 実装済みキャッシュ基盤（コード上の実体）

#### A. Layer Surface Cache
- **場所**: `Artifact/src/Render/ArtifactCompositionViewDrawing.cppm:56-64`
- **構造体**: `LayerSurfaceCacheEntry`（ownerId, cacheSignature, processedSurface(QImage), processedBuffer(ImageF32x4_RGBA*), gpuTextureHandle, frameNumber）
- **保持**: `CompositionRenderController` Impl `surfaceCache_`（`QHash<QString, LayerSurfaceCacheEntry>`）
- **キー生成**: `buildLayerSurfaceCacheKey()`（同 `.cppm:~234-406`）— レイヤー型ごとに属性を QString 連結。Text は 27 フィールド。
- **対象**: `layerUsesSurfaceUploadForCompositionView()` が true のレイヤー（Image, Svg, Video, Text, Solid2D, SolidImage + rasterizer/mask あり）

#### B. Static Layer GPU Cache（プロセス寿命）
- **場所**: `Artifact/src/Render/ArtifactCompositionViewDrawing.cppm:66-75, 102-106`
- **構造体**: `StaticLayerGpuCacheEntry`（ownerId, cacheSignature, processedSurface, processedBuffer, gpuTextureHandle, lastFrameNumber, byteSize）
- **保持**: グローバル static `QHash<QString, StaticLayerGpuCacheEntry>`
- **対象**: `layerUsesStaticLayerGpuCacheForCompositionView()` が true のレイヤー（Video/Particle/FormParticle/CompositionLayer は除外）

#### C. GPU Texture Cache Manager
- **場所**: `Artifact/include/Render/GPUTextureCacheManager.ixx` + `src/Render/GPUTextureCacheManager.cppm`
- **API**: `acquireOrCreate(QImage)`, `acquireOrCreate(ImageF32x4_RGBA)`, `acquireOrCreate(GpuVideoFrame)` — 3 入力型
- **ハンドル**: `GPUTextureCacheHandle{id, generation}`, `GPUTextureBindingRecord`
- **機能**: budget (512 MiB default) / maxEntries (256) / generation-based invalidation / owner 単位 invalidate / stats
- **バックエンド**: Diligent Engine, Vulkan native image ラップ対応（`CreateTextureFromVulkanImage`）
- **既知の問題**: (G-1) resize/upload 競合でステージングバッファリーク, (G-2) `reserve()` が mutex 外

#### D. Frame Cache（レンダリング済みフレーム全体）
- **場所**: `Artifact/include/Render/ArtifactFrameCache.ixx` + `ArtifactFrameCache.cppm`
- **クラス**: `FrameCache`（QObject）+ `ProgressiveRenderer` + `RenderPerformanceMonitor`
- **機能**: LRU/LFU/FIFO ポリシー、メモリバジェット、generation-based invalidation、prefetch、signals
- **エントリ**: `FrameCacheEntry`（QImage + float pixels + width/height + frame + generation）
- **既知の問題**: eviction O(N) スキャン

#### E. RAM Preview（active path 実装済み）
- **active owner**: `ArtifactPlaybackService` — frame state、priority build queue、RAM LRU、timeline/playback の readiness source を一元管理
- **状態**: `requested / ready / failed / inRam / onDisk / imageAvailable`。`ready` は RAM 内の final image が存在する場合だけ成立する。
- **診断**: hit rate は composition 全体ではなく、現在の preview range 内で要求済みの frame を分母にする。
- **legacy implementation**: `ArtifactRamPreviewController` は独立した初版 controller として残るが、active render path の readiness source ではない。新機能はここへ追加しない。

#### F. Asset Instance Shared Payload Cache（新規実装）
- **場所**: `ArtifactCore/include/Utils/AssetManager.ixx`
- **API**: `acquireSource/releaseSource/localizeSource/sourceId/useCount/sourceVersion/invalidateSource/decodedPayload/publishDecodedPayload`
- **キー**: `asset UUID + source version + representation`
- **所有**: weak ownership（Layer が payload 寿命を所有）
- **状態**: Asset Instance Sharing (2026-06-16) の一部として実装済み。Image/Video/Audio layer が source lease に接続済み。未加工 Image の GPU cache key を `asset UUID + version` に切替済み。

#### G. Video Layer Local Frame Cache
- **場所**: `ArtifactVideoLayer` Impl 内 `frameCache_` — decode 済み `ImageF32x4_RGBA` の per-layer LRU
- **用途**: 非同期デコード結果の保持、prefetch（`put/get/contains`）

#### H. 差分レンダリング
- **`RenderKeyState`**: `CompositionRenderController` Impl 内のパック構造体。frame/zoom/pan/downsample/flags/invalidation serial を `operator==` 比較。
- **`lastRenderKeyState_`**: 前回 key。一致すれば full composite スキップ。
- **`invalidateBaseComposite()`**: serial インクリメント → 次フレームで再合成。
- **`invalidateLayerSurfaceCache(layer)`**: 特定レイヤーの surface cache を hash lookup + erase。

#### I. Motion Path Cache
- **`MotionPathCacheEntry`**: モーションパス点列キャッシュ。`overlaySerial_` 変更時に invalidate。

#### J. Composition Disk Preview Cache（baseline 実装済み）
- **場所**: `Artifact/src/Service/ArtifactPlaybackService.cppm`
- **内容**: final preview frame の PNG 永続化、RAM への近傍 hydrate、非同期 writer、composition namespace
- **安全性**: disk-write generation により invalidation 後の書戻しを防止。`Clear Cache` は RAM/disk と保留 writer を同時に無効化する。
- **key**: composition settings + `preview-frame-v2` + quality/render-path contract hash。contract 未確定時の hydrate は禁止する。
- **budget**: active contract namespace あたり 512 MiB。保存完了後に最終更新時刻が古い frame PNG から eviction し、state を `onDisk=false` に戻す。
- **残課題**: layer/effect state hash、manifest、namespace 横断 budget/orphan cleanup、restart 時に contract を先行解決する経路。

---

### 既知の問題点（コード実装から）

| ID | 問題 | 場所 | 深刻度 |
|----|------|------|--------|
| B1 | `buildLayerSurfaceCacheKey()` 毎フレーム無条件構築 | `ArtifactCompositionViewDrawing.cppm:~724` | 重大 |
| G-1 | GPUTextureCacheManager: resize/upload 競合でステージングバッファリーク | `GPUTextureCacheManager.cppm:277` | 中 |
| G-2 | GPUTextureCacheManager: `reserve()` が mutex 外 | `GPUTextureCacheManager.cppm:355` | 中 |
| C1 | FrameCache eviction O(N) スキャン | `ArtifactFrameCache.cppm` | 軽 |
| -- | Surface cache key に opacity 未反映（stale surface の可能性） | `buildLayerSurfaceCacheKey()` | 中 |
| -- | composition changed → `surfaceCache_.clear()` + `gpuTextureCacheManager_->clear()` 全件クリア | `CompositionRenderController` | 中 |
| -- | panBy() が毎回 `invalidateBaseComposite()` → 全レイヤー再描画 | `CompositionRenderController` | 中 |
| 解決 | stale disk write が invalidation 後に復活 | `ArtifactPlaybackService` | disk-write generation + queue purge で解決 |
| 解決 | layer content edit 後の final preview stale | `CompositionRenderController` → `ArtifactPlaybackService` | active layer-change path から RAM/disk を無効化 |

---

## Design Direction (計画)

cache を 4 層として定義する。

### Layer 0: Source / Decode Cache（部分的に実装済み）

- **実装**: Asset Manager の decoded payload cache（`decodedPayload/publishDecodedPayload`）
- video decode frame（Video layer `frameCache_`）
- image source decode（Asset Instance Sharing）
- audio waveform/thumbnail decode

責務: source 読み込みや decode の再利用。final composition readiness は保証しない。

### Layer 1: Layer / Surface Cache（実装済み）

- **実装**: `surfaceCache_` + `staticLayerGpuCache()` + `GPUTextureCacheManager`
- processed surface cache / layer-local effect/mask/matte result / GPU texture cache

責務: layer 単位の再利用。final frame readiness の直接判定には使わない。

### Layer 2: Composition RAM Cache（実装済み）

- **実装**: `ArtifactPlaybackService` の frame state + build queue + RAM LRU
- authoritative RAM preview frame cache / composition-scoped frame readiness

責務: `frame ready` の真偽を決める唯一の層。timeline cache bar / playback guaranteed mode / diagnostics の基準。

**状態**: active path の authoritative readiness はこの層が持つ。layer cache hit だけでは `ready` にしない。

### Layer 3: Composition Disk Cache（baseline 実装済み）

- persisted preview frame（PNG）/ RAM hydrate / async writer / contract namespace

責務: RAM miss 時の fallback と再利用。state manifest、budget/eviction/orphan cleanup は未実装。

---

## Key Rule

**authoritative frame readiness は Layer 2/3 の composition-level cache だけが持つ。**

layer surface cache hit / GPU texture cache hit / decode frame cache hit だけでは `ready` と言わない。
`ready` は **final composition frame が現在の policy / quality / state hash で再利用可能** であることを意味する。

---

## Proposed Contracts

### 1. Shared Cache Key Family

最低限: composition identity, frame, quality preset, backend, render format contract hash, layer/effect state hash, policy hash

### 2. Shared Cache Manifest

```cpp
struct CompositionFrameCacheState {
    bool requested = false;
    bool ready = false;
    bool failed = false;
    bool inRam = false;
    bool onDisk = false;
    QString reason;
};
```

### 3. Explicit Promotion / Demotion

1. source/decode → layer surface
2. layer surface → composition RAM frame
3. RAM frame → disk persisted frame
4. disk frame → RAM rehydrate

---

## Upgrade Plan

### Phase 1: Ownership Cleanup（完了）

✅ cache owner を列挙（本文書で完了）  
✅ active readiness owner を `ArtifactPlaybackService` に固定  
✅ `ready` は final image が RAM に存在すること、と統一

### Phase 2: Composition RAM Preview Controller（完了）

✅ priority build queue / cancellation / directional ordering  
✅ playback / timeline / diagnostics が service state を参照  
✅ content edit / quality / render policy で final preview を無効化

### Phase 3: Shared Cache Key / Manifest（部分実装）
- ✅ composition identity / size / frame rate / range / quality / render path contract
- ❌ layer/effect state hash と persisted manifest

### Phase 4: Disk Preview Frame Cache（部分実装）
- ✅ asynchronous PNG persistence / RAM hydrate / generation-safe invalidation
- ✅ active contract namespace の 512 MiB budget / oldest-frame eviction
- ❌ namespace 横断 budget / orphan cleanup / restart contract bootstrap
### Phase 5: Promotion Policy（未着手）
### Phase 6: Intermediate / Render Queue Integration（未着手）

---

## Implementation Targets（コード確認済みファイル）

- `Artifact/src/Widgets/Render/ArtifactCompositionRenderController.cppm` — surfaceCache_, invalidateLayerSurfaceCache, invalidateBaseComposite, RenderKeyState
- `Artifact/src/Render/ArtifactCompositionViewDrawing.cppm` — LayerSurfaceCacheEntry, StaticLayerGpuCacheEntry, buildLayerSurfaceCacheKey, drawLayerForCompositionView, applySurfaceAndDraw
- `Artifact/include/Render/GPUTextureCacheManager.ixx` + `src/Render/GPUTextureCacheManager.cppm` — GPU テクスチャキャッシュ
- `Artifact/include/Render/ArtifactFrameCache.ixx` + `ArtifactFrameCache.cppm` — FrameCache
- `Artifact/src/Service/ArtifactPlaybackService.cppm` — active RAM preview owner + disk preview cache
- `Artifact/src/Render/ArtifactRamPreviewController.cppm` — legacy initial controller（新規機能追加対象外）
- `ArtifactCore/include/Utils/AssetManager.ixx` — decoded payload cache (source sharing)
- `Artifact/src/Layer/ArtifactVideoLayer.cppm` — per-layer frameCache_
- `Artifact/src/Service/ArtifactPlaybackService.cppm`
- `Artifact/src/Widgets/Timeline/ArtifactTimelineScrubBar.cppm`

---

## Guardrails

1. disk cache を RAM cache の単なる巨大版にしない
2. layer cache hit を final frame ready と混同しない
3. cache key 固定前に保存形式最適化へ走らない
4. cache invalidation を implicit event へ散らさない
5. diagnostics vocabulary を先に揃える

---

## Short Decision

- `Layer cache` は編集コスト削減
- `RAM preview cache` は authoritative playback cache
- `Disk cache` は persistent fallback + reuse layer

既存の `Composition Editor Cache` と `RAM Preview` を中心に、shared key / manifest / disk persistence を重ねる方針で進める。

---

## 更新履歴

- 2026-05-16: 初版作成
- 2026-07-21: コード実態調査に基づき全面更新。実装済み項目・ファイルパス・API・既知の問題を反映。Asset Instance Sharing の decoded payload cache を追加。
- 2026-07-21: RAM/disk final preview の active owner、disk-write generation、quality/render-path contract、content-edit invalidation を実装反映。
- 2026-07-21: RAM preview hit rate を要求済み preview range 基準へ修正。
- 2026-07-21: active disk namespace に 512 MiB budget と oldest-frame eviction を追加。
