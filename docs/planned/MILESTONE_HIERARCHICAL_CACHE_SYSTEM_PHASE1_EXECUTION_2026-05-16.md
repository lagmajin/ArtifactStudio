# Hierarchical Cache System - Phase 1 Execution

**Date**: 2026-05-16

**Source**: [`./MILESTONE_HIERARCHICAL_CACHE_SYSTEM_2026-05-16.md`](./MILESTONE_HIERARCHICAL_CACHE_SYSTEM_2026-05-16.md)

**Status**: Planned first execution slice

---

## Phase 1 Goal

既存 cache を消さずに、

1. cache owner を列挙し
2. authoritative readiness owner を固定し
3. `cached` / `ready` / `stale` の語彙を揃える

ところまでを先に進める。

この段階では、disk persistence や intermediate 永続化には入らない。

---

## Scope

### In

- cache owner audit
- readiness vocabulary 固定
- RAM preview を authoritative composition cache として位置づける
- diagnostics / timeline / playback が同じ frame readiness を読む前提づくり

### Out

- disk cache payload format
- large-scale eviction implementation
- render queue reuse 本実装
- 全 cache path の一斉リファクタ

---

## Current Boundary Note

- `Layer cache hit` と `final frame ready` は分ける
- `GPU texture cache` は Layer 1 の再利用であり、playback readiness の主ではない
- `RAM preview state` は composition-scoped cache の中心へ寄せる
- `Disk cache` はこの Phase ではまだ owner ではなく、将来の persistent fallback layer として扱う

---

## First Files

### Parent-side reading / planning

1. `docs/planned/MILESTONE_HIERARCHICAL_CACHE_SYSTEM_2026-05-16.md`
2. `docs/planned/MILESTONE_RAM_PREVIEW_CACHE_2026-03-26.md`
3. `docs/planned/MILESTONE_DISK_CACHE_SYSTEM_2026-03-26.md`
4. `Artifact/docs/MILESTONE_RAM_PREVIEW_SYSTEM_2026-05-01.md`
5. `Artifact/docs/MILESTONE_COMPOSITION_EDITOR_CACHE_SYSTEM_2026-03-26.md`

### Likely first implementation targets

1. `Artifact/src/Service/ArtifactPlaybackService.cppm`
2. `Artifact/src/Widgets/Render/ArtifactCompositionRenderController.cppm`
3. `Artifact/src/Widgets/Timeline/ArtifactTimelineScrubBar.cppm`
4. `Artifact/src/Widgets/Diagnostics/AppDebuggerWidget.cppm`
5. `Artifact/src/Widgets/Diagnostics/DebugRenderHarnessWidget.cppm`

---

## Cache Owner Audit

Phase 1 で最低限見分けたい owner:

### Layer 0: Source / Decode

- video decode frame cache
- audio waveform / thumbnail cache
- image source decode cache

### Layer 1: Layer / Surface

- processed surface cache
- layer-local mask / matte / effect result
- GPU texture cache

### Layer 2: Composition RAM

- RAM preview bitmap
- composition frame requested/ready/failed state
- playback-facing frame readiness

### Layer 3: Composition Disk

- persisted preview frame
- persisted composition result

この audit の目的は「どこに何があるか」を全部説明することではなく、
**どの層が `ready` を名乗ってよいか** を固定すること。

---

## Vocabulary To Freeze

### `cache hit`

その層の再利用が成立した、という意味だけに使う。
`final frame ready` を含意しない。

### `ready`

現在の composition / frame / quality / policy で、
**final composition frame が playback / preview に使える**
場合にだけ使う。

### `stale`

key / hash / policy のいずれかが現状態と一致しないため、
再利用してはいけない状態。

### `requested`

その frame を RAM preview build queue が要求済みだが、
まだ `ready` でも `failed` でもない状態。

### `failed`

その frame の build が失敗したか、中断扱いではなく失敗理由が確定した状態。

---

## First Move

1. diagnostics に出ている `cache / ready / requested / hit` の文言を列挙する
2. `ArtifactPlaybackService` を authoritative readiness owner 候補として固定する
3. scrub bar / debugger / harness が同じ readiness source を読む前提を明文化する
4. その後に code 側で `CompositionFrameCacheState` 相当の shape を導入する

---

## Tasks

### 1. Owner Inventory

- 現在の cache owner を layer別に列挙する
- `final frame ready` を持ってはいけない owner を明示する
- `RAM preview bitmap` の authoritative 性を確認する

### 2. Shared State Shape

少なくとも次の shape を parent-side で固定する。

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

ここでは field 名の厳密固定よりも、意味の固定を優先する。

### 3. Diagnostic Source Unification

- `TimelineScrubBar`
- `AppDebuggerWidget`
- `DebugRenderHarnessWidget`
- playback summary

が読んでいる cache 文法を寄せる。

### 4. Authority Split

次を明文化する。

1. layer/gpu/decode cache = optimization layer
2. RAM preview = authoritative composition readiness
3. disk cache = persistent fallback

---

## Recommended Order

1. owner inventory
2. shared vocabulary freeze
3. `CompositionFrameCacheState` 相当の shape 定義
4. diagnostics source unification
5. code-side RAM preview controller 入口

---

## Done Criteria

- `cached` と `ready` が混同されない
- timeline / diagnostics / playback が同じ readiness 概念を共有する
- RAM preview controller を authoritative owner として実装し始められる
- disk cache 導入前に stale reuse の前提が整理される

---

## Related Docs

- [Hierarchical Cache System](/x:/Dev/ArtifactStudio/docs/planned/MILESTONE_HIERARCHICAL_CACHE_SYSTEM_2026-05-16.md)
- [RAM Preview System](/x:/Dev/ArtifactStudio/Artifact/docs/MILESTONE_RAM_PREVIEW_SYSTEM_2026-05-01.md)
- [RAM Preview Cache](/x:/Dev/ArtifactStudio/docs/planned/MILESTONE_RAM_PREVIEW_CACHE_2026-03-26.md)
- [Disk Cache System](/x:/Dev/ArtifactStudio/docs/planned/MILESTONE_DISK_CACHE_SYSTEM_2026-03-26.md)
