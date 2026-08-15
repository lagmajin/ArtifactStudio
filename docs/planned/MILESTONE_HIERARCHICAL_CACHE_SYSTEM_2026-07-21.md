# Milestone: Hierarchical Cache System

**最終更新:** 2026-08-15

## 現行コード監査 (2026-08-15)

階層の主要部は現行コードに存在します。Asset source／decode、layer surface／GPU texture、composition RAM、disk preview の各 cache と、`ArtifactPlaybackService` の composition-level readiness、generation invalidation、disk hydrate／eviction が確認できます。`ArtifactFrameCache`、image sequence／video の local cache、差分 render key も個別に実装されています。

一方、これらを共通の persisted manifest／state hash 契約で横断する上位 cache facade は未確認です。surface key の毎フレーム構築、全件 invalidation、candidate heap の一時 stale 候補、上位 composition cache への state hash／manifest 接続、再起動後の総合再利用は残課題または未検証です。

## Update 2026-08-15

- source／decode、layer surface／GPU texture、composition RAM、disk preview の各 cache と、Playback Service の readiness／generation invalidation／disk hydrate・eviction を再確認。
- Asset shared payload、video local frame cache、差分 render key、motion path cache も個別に存在するが、persisted manifest／state hash を横断する上位 facade は確認できない。
- surface key の毎フレーム構築、全件 invalidation、candidate heap の stale 候補、再起動後の総合再利用、上位 composition cache への manifest 接続は未完了・未検証。

**Date:** 2026-07-21 (コード実態調査に基づき全面更新)  
**ステータス:** In Progress

## Goal

既存の

1. `Composition Editor Cache`（Layer Surface + Static GPU）
2. `GPUTextureCacheManager`
3. `RAM Preview Controller`
4. `Disk Cache`（baseline 実装済み、manifest v1 検証済み）

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
- **再検証**: 2026-07-21 時点の `GPUTextureCacheManager.cppm` に staging buffer / `reserve()` 呼出しは存在せず、旧 G-1/G-2 指摘は現行コードの既知問題としては扱わない。Diligent backend の挙動は実測時に別途検証する。

#### D. Frame Cache（レンダリング済みフレーム全体）
- **場所**: `Artifact/include/Render/ArtifactFrameCache.ixx` + `ArtifactFrameCache.cppm`
- **クラス**: `FrameCache`（QObject）+ `ProgressiveRenderer` + `RenderPerformanceMonitor`
- **機能**: LRU/LFU/FIFO ポリシー、メモリバジェット、generation-based invalidation、prefetch、signals
- **エントリ**: `FrameCacheEntry`（QImage + float pixels + width/height + frame + generation）
- **既知の問題**: FrameCache の candidate heap 再構築は閾値ベースで、短時間に大量アクセスした場合は一時的に stale 候補を保持する。候補取得自体は O(log N)。

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
- **key**: composition settings + serialized composition state + `preview-frame-v3` + quality/render-path contract hash。contract 未確定時の hydrate は禁止する。
- **budget**: active contract namespace あたり 512 MiB。保存完了後に最終更新時刻が古い frame PNG から eviction し、state を `onDisk=false` に戻す。全 namespace 合計は 2 GiB で、8 回の保存ごとに古い frame を掃除し、空の生成済み namespace を削除する。
- **残課題**: 上位 composition cache への state hash / persisted manifest 接続。

---

### 既知の問題点（コード実装から）

| ID | 問題 | 場所 | 深刻度 |
|----|------|------|--------|
| B1 | `buildLayerSurfaceCacheKey()` 毎フレーム無条件構築 | `ArtifactCompositionViewDrawing.cppm:~724` | 重大 |
| 解決 | GPUTextureCacheManager の staging leak / mutex 外 `reserve()` | `GPUTextureCacheManager.cppm` | 現行ソースに該当実装がなく、旧調査の指摘を解消 |
| 解決 | FrameCache の LRU/LFU/Size 候補選択 O(N) スキャン | `ArtifactFrameCache.cppm` | 2026-07-22: lazy candidate heap 化 |
| 解決 | FrameCache の currentMemoryUsage() 全走査 | `ArtifactFrameCache.cppm` | 2026-07-22: 増分カウンタ化 |
| -- | Surface cache key に opacity 未反映（stale surface の可能性） | `buildLayerSurfaceCacheKey()` | 中 |
| 解決 | Surface cache key に layer opacity を追加 | `ArtifactCompositionViewDrawing.cppm` | 2026-07-22 |
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

責務: RAM miss 時の fallback と再利用。namespace manifest、budget/eviction/orphan cleanup は実装済み。

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
- ✅ serialized composition state hash を disk namespace に含め、layer/effect 編集後の stale reuse を分離
- ✅ composition disk namespace の persisted manifest v1（stateHash を含む）と読み込み検証
- ❌ 上位 composition cache への state hash / persisted manifest 接続

#### 2026-07-22 implementation slice

- [x] `GPUTextureCacheManager` に explicit / owner / budget / device / clear の invalidation reason 契約を追加
- [x] cache stats から invalidation count と直近 reason を取得可能化
- [x] device reset / budget eviction も reason 記録対象に追加
- [x] Composition Frame Debug resource noteへGPU cacheのinvalidation count / last reasonを公開
- [x] last reasonを数値ではなく共通の診断文字列へ変換
- [ ] 上位 composition cache の state hash / persisted manifest 接続

### Phase 4: Disk Preview Frame Cache（部分実装）
- ✅ asynchronous PNG persistence / RAM hydrate / generation-safe invalidation
- ✅ active contract namespace の 512 MiB budget / oldest-frame eviction
- ✅ namespace 横断 2 GiB budget / empty namespace cleanup
- ✅ namespace ごとの persisted manifest v1（frame / file / bytes / contract / stateHash）書き出し
- ✅ restart 時に manifest の schema / namespace / contract / stateHash / file size を検証
- ✅ 現行 composition の preview contract は disk hydrate 前の既存 render-readback 経路で解決
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
- 2026-07-21: disk cache 全体に 2 GiB budget と empty namespace cleanup を追加。
- 2026-07-22: disk namespace の PNG 状態を `manifest.json` schema v1 として writer 後に原子的に保存。
- 2026-07-22: disk hit / hydrate 前に manifest の schema・namespace・composition・contract・file size を検証。未生成・破損・古い manifest は miss 扱い。
- 2026-07-22: manifest のみ残った空 namespace も global budget cleanup で削除するよう修正。
- 2026-07-22: `ArtifactAbstractComposition::toJson()` の hash を disk namespace に追加し、composition 切り替え時は active namespace を先に削除するよう修正。
- 2026-07-22: composition state hash は active namespace 内で再利用し、invalidation 時だけ再計算するよう cache。frame path 反復時の JSON serialize を抑制。
- 2026-07-22: `ArtifactFrameCache` の memory usage を増分カウンタ化し、LRU/LFU/Size eviction を lazy candidate heap 化。stale heap は閾値で再構築。

## 2026-07-25 実装監査

- Layer／GPU／RAM preview／Disk preview の各基盤と、disk manifest v1・state hash・generation-safe invalidation は計画書の記載どおり実装済み。
- `ready` の authoritative owner は `ArtifactPlaybackService` に集約され、layer cache hit と composition frame readiness は分離されている。
- Phase 3 は上位 composition cache への state hash／persisted manifest 接続が未完了である。
- Phase 5（RAM と disk 間の明示的な promotion policy）と Phase 6（intermediate cache／render queue 統合）は未着手であり、本マイルストーン全体は継続中と判定する。
- ビルド・実行時の再生保証や Diligent backend の実測は未確認のため、コード監査だけで runtime 完了とは判定しない。
