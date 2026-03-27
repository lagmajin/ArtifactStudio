# Disk Cache System Milestone

`RAM Preview Cache` と `Composition Editor Cache` だけでは、
セッションをまたいだ再利用と長尺・高解像度コンポジションの負荷吸収が足りない。

この milestone では、`preview / editor / render queue` が共有できる
永続ディスクキャッシュの骨格を定義する。

## Goal

- 再起動後でも再利用できる frame / intermediate cache を持つ
- `RAM cache` を超えるサイズの preview 結果を安全に退避できる
- stale な結果を誤表示しない cache key / invalidation を固定する
- `Composition Viewer` / `RAM Preview` / `Render Queue` で cache key を共有できるようにする

## Scope

- cache key / manifest / budget / eviction の定義
- frame cache の disk 永続化
- intermediate cache の拡張余地
- UI / diagnostics / clear action

## Non-Goals

- いきなり全 layer type / 全 effect を対象にすること
- backend 固有の最適化だけで完結させること
- render queue の最終出力をそのまま disk cache と同一視すること

## Background

現状は、

- `Composition Editor Cache`
  - editor 内の再描画削減
- `RAM Preview Cache`
  - 一時的な frame cache

が別々に存在するが、永続化された preview cache はまだない。

そのため、

- アプリ再起動で cache が消える
- 長尺や高解像度で RAM 容量が先に限界に来る
- 一度生成した重い中間結果を翌セッションで活かせない

という問題が残る。

## Proposed Model

- `DiskCacheKey`
  - composition id
  - frame
  - preview quality
  - backend
  - layer/effect state hash
  - render policy hash
- `DiskCacheEntry`
  - payload path
  - format
  - byte size
  - created at / last used at
  - stale reason
- `DiskCacheDomain`
  - `PreviewFrame`
  - `CompositionResult`
  - `LayerSurface`
  - `Intermediate`
- `DiskCacheBudget`
  - max bytes
  - max entry count
  - eviction mode (`LRU`, `Age`, `Domain-aware`)

## Phases

### Phase 1: Cache Key / Manifest

- `DiskCacheKey` を formalize
- stale 判定に必要な hash 入力を固定
- manifest 形式を決める

完了条件:

- 同じ frame が再利用可能かどうかを deterministic に判定できる

### Phase 2: Preview Frame Disk Cache

- preview frame を disk に保存・読込できるようにする
- `RAM Preview Cache` miss 時の fallback として使えるようにする

完了条件:

- 再起動後でも preview frame が再利用できる

### Phase 3: Budget / Eviction

- cache folder 容量上限
- entry 削除ポリシー
- startup 時の orphan cleanup

完了条件:

- 容量が増え続けない

### Phase 4: Diagnostics / UI

- cache size
- hit / miss
- stale reason
- clear action

完了条件:

- ユーザーが cache 状態を把握して制御できる

### Phase 5: Intermediate Cache

- layer surface
- composition intermediate
- precompose result

を disk cache 対象に拡張する

完了条件:

- 重い comp で editor / preview の再利用範囲が広がる

### Phase 6: Render Queue Integration

- render queue と preview 系 cache key の整合
- 同一設定の再レンダリング削減

完了条件:

- preview と export が別々に無駄仕事しにくい

## Recommended Order

1. `Phase 1: Cache Key / Manifest`
2. `Phase 2: Preview Frame Disk Cache`
3. `Phase 3: Budget / Eviction`
4. `Phase 4: Diagnostics / UI`
5. `Phase 5: Intermediate Cache`
6. `Phase 6: Render Queue Integration`

## Dependencies

- `Artifact/docs/MILESTONE_COMPOSITION_EDITOR_CACHE_SYSTEM_2026-03-26.md`
- `docs/planned/MILESTONE_RAM_PREVIEW_CACHE_2026-03-26.md`

## Notes

最初にやるべきは `cache key` の固定であって、保存形式の最適化ではない。
ここが曖昧なまま disk cache を入れると、古い結果の誤再利用で挙動が壊れる。

したがって、`Disk Cache` は `RAM cache の大きい版` ではなく、
`正しい invalidation を持つ永続 cache layer` として設計する必要がある。
