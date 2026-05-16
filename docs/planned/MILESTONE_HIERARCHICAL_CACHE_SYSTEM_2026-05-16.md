# Milestone: Hierarchical Cache System

**Date:** 2026-05-16  
**Status:** Planned

## Goal

既存の

1. `Composition Editor Cache`
2. `GPUTextureCache`
3. `RAM Preview`
4. `Disk Cache`

をバラバラの最適化として増やすのではなく、  
**RAM とディスクが連携する階層 cache system** として再整理する。

最終目標は、

- 編集中は軽く反応する
- RAM preview は確保済みフレームを保証再生できる
- RAM を超えた結果は disk に退避できる
- restart 後も再利用できる
- stale cache を誤表示しない

状態へ持っていくこと。

## Existing Assets To Reuse

すでにある土台:

### 1. Composition Editor Cache

- processed surface cache
- render key based redundant composite suppression
- partial GPU texture cache hardening

参照:

- `Artifact/docs/MILESTONE_COMPOSITION_EDITOR_CACHE_SYSTEM_2026-03-26.md`

### 2. GPU Texture Cache

- `GPUTextureCacheManager`
- static layer 系の upload reuse
- cache invalidation の入口が一部存在

### 3. RAM Preview State

- `ArtifactPlaybackService` の early RAM preview state
- cache bitmap
- timeline scrub bar の可視化
- diagnostics の ready/requested/cached 表示

参照:

- `Artifact/docs/MILESTONE_RAM_PREVIEW_SYSTEM_2026-05-01.md`
- `docs/planned/MILESTONE_RAM_PREVIEW_CACHE_2026-03-26.md`

### 4. Disk Cache Planning

- `DiskCacheKey`
- manifest
- budget/eviction
- preview/editor/render queue 共有の構想

参照:

- `docs/planned/MILESTONE_DISK_CACHE_SYSTEM_2026-03-26.md`

## Core Problem

現状は cache が複数あるが、**ownership と authoritative layer が分かれている**。

典型的なズレ:

1. layer cache hit なのに final frame は ready ではない
2. RAM preview bitmap と render-side cache が silent desync する
3. GPU texture cache はあるが session をまたげない
4. disk cache を入れても RAM preview との責務分担が曖昧なままだと stale reuse が起きる

## Design Direction

cache を 4 層として定義する。

### Layer 0: Source / Decode Cache

- video decode frame
- image source decode
- audio waveform/thumbnail decode

責務:

- source 読み込みや decode の再利用
- final composition readiness は保証しない

### Layer 1: Layer / Surface Cache

- processed surface cache
- layer-local effect/mask/matte result
- GPU texture cache

責務:

- layer 単位の再利用
- final frame readiness の直接判定には使わない

### Layer 2: Composition RAM Cache

- authoritative RAM preview frame cache
- composition-scoped frame readiness

責務:

- `frame ready` の真偽を決める唯一の層
- timeline cache bar / playback guaranteed mode / diagnostics の基準

### Layer 3: Composition Disk Cache

- persisted preview frame
- persisted composition result
- optional persisted intermediate

責務:

- RAM miss 時の fallback
- session 跨ぎ再利用
- budget/eviction/orphan cleanup

## Key Rule

**authoritative frame readiness は Layer 2/3 の composition-level cache だけが持つ。**

つまり、

- layer surface cache hit
- GPU texture cache hit
- decode frame cache hit

だけでは `ready` と言わない。

`ready` は  
**final composition frame が現在の policy / quality / state hash で再利用可能**
であることを意味する。

## Proposed Contracts

### 1. Shared Cache Key Family

まず `preview / editor / render queue` で共有する key family を作る。

最低限:

- composition identity
- frame
- quality preset
- backend
- render format contract hash
- layer/effect state hash
- policy hash

### 2. Shared Cache Manifest

RAM / disk / diagnostics が同じ語彙で読めるようにする。

例:

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

cache 遷移を暗黙にしない。

1. source/decode -> layer surface
2. layer surface -> composition RAM frame
3. RAM frame -> disk persisted frame
4. disk frame -> RAM rehydrate

## Upgrade Plan

### Phase 1: Ownership Cleanup

最初にやること:

1. cache owner を列挙
2. authoritative readiness owner を `RAM preview controller` に固定
3. `cached` という語の意味を統一

完了条件:

- cache bar / diagnostics / playback が同じ readiness を参照する

### Phase 2: Composition RAM Preview Controller

既存 `ArtifactPlaybackService` の early state を土台に、
composition-scoped の controller を明示化する。

責務:

1. build queue
2. frame state table
3. cancellation
4. invalidation
5. playback integration

完了条件:

- RAM preview が layer cache の寄せ集めではなく composition-level cache として振る舞う

### Phase 3: Shared Cache Key / Manifest

disk cache に入る前に、key を固定する。

完了条件:

- RAM / disk / render queue が同一 key family を共有できる

### Phase 4: Disk Preview Frame Cache

まずは final preview frame だけを永続化する。

理由:

- intermediate より invalidation が明快
- RAM preview miss の fallback として効果が大きい

完了条件:

- restart 後に preview frame を rehydrate できる

### Phase 5: Promotion Policy

RAM と disk の連携 policy を決める。

例:

1. ready frame を LRU で RAM 保持
2. RAM pressure 時は disk へ退避済み frame から RAM 解放
3. replay 需要の高い work area は RAM 優先

完了条件:

- RAM overflow 時も cache の意味が壊れない

### Phase 6: Intermediate / Render Queue Integration

最後に、

1. composition result
2. heavy layer surfaces
3. render queue reuse

へ拡張する。

## Recommended First Implementation Targets

### Parent-side planning / diagnostics

- `Artifact/docs/MILESTONE_RAM_PREVIEW_SYSTEM_2026-05-01.md`
- `docs/planned/MILESTONE_DISK_CACHE_SYSTEM_2026-03-26.md`
- `docs/planned/MILESTONE_RAM_PREVIEW_CACHE_2026-03-26.md`

### Artifact implementation

- `Artifact/src/Service/ArtifactPlaybackService.cppm`
- `Artifact/src/Widgets/Render/ArtifactCompositionRenderController.cppm`
- `Artifact/src/Widgets/Timeline/ArtifactTimelineScrubBar.cppm`
- `Artifact/src/Render/GPUTextureCacheManager.cppm`
- cache-related diagnostics surfaces

## First Execution Memo

- `docs/planned/MILESTONE_HIERARCHICAL_CACHE_SYSTEM_PHASE1_EXECUTION_2026-05-16.md`

## Guardrails

1. disk cache を RAM cache の単なる巨大版にしない
2. layer cache hit を final frame ready と混同しない
3. cache key 固定前に保存形式最適化へ走らない
4. cache invalidation を implicit event へ散らさない
5. diagnostics vocabulary を先に揃える

## Short Decision

長期的には、

- `Layer cache` は編集コスト削減
- `RAM preview cache` は authoritative playback cache
- `Disk cache` は persistent fallback + reuse layer

として役割分担するのがよい。

新規に全部作り直すのではなく、既存の `Composition Editor Cache` と `RAM Preview` を中心に、  
その上へ shared key / manifest / disk persistence を重ねる方針で進める。
