# Milestone: GPU-Driven MDI Render (2026-04-02)

**Status:** Draft
**Goal:** CPU 依存の draw submission を減らし、GPU 側で可視判定・集約・`Multi-Draw Indirect` 相当の描画指示生成を行える render path を作る。

---

## できるか

いける。  
ただし、いきなり「全部 GPU 駆動」に振り切るより、まずは **既存の render path を壊さずに MDI を差し込める形** にするのが安全。

特にこの repo では、`DiligentEngine` / DX12 周辺の低レベル path は前提がずれやすいので、最初に以下を固めるのが重要。

- どの粒度を 1 draw item とするか
- 何を GPU 側で持ち、何を CPU 側で残すか
- fallback ルートをどう維持するか
- backend ごとの差をどこで吸収するか

---

## Scope

- `Artifact/src/Render/*`
- `Artifact/src/Widgets/Render/*`
- `Artifact/include/Render/*`
- `ArtifactCore/include/Graphics/*`
- `ArtifactCore/src/Graphics/*`
- `libs/DiligentEngine/*` は原則触らず、親 repo 側で吸収できるか先に確認

---

## Non-Goals

- いきなり全レンダリングを GPU 駆動に統一すること
- submodule の low-level backend を広範囲に改変すること
- editor UI の全面刷新
- 可視判定や batching の完全自動最適化

---

## Background

現状の render 系は、CPU 側で build した draw 情報を順に流す構成が中心。
MDI を入れると、同じ種類の描画をまとめられるだけでなく、将来的に GPU culling / instance compaction / render queue 再編成へ伸ばしやすくなる。

この milestone は「最終形の GPU renderer」ではなく、**GPU 駆動の描画 submission を成立させる足場** を作る位置づけ。

---

## Phases

### Phase 1: Draw Item Contract

**Goal:** CPU / GPU どちらでも扱える描画単位を定義する。

- draw item の共通 struct を定義する
- material / transform / bounds / visibility を分離する
- instance 化できるものと、個別 draw が必要なものを分ける
- 現行 render queue から draw item へ変換する橋を作る

**Done when:**

- 既存 path を壊さず draw item を収集できる
- 1 frame の render submission を記録できる

---

### Phase 2: GPU Compaction / Culling

**Goal:** GPU 側で描画候補を絞り込めるようにする。

- bounds buffer を GPU に渡す
- visibility / layer / pass 条件で compaction する
- 同種 item を group 化する
- CPU fallback を維持する

**Done when:**

- ある範囲の draw item が GPU 側で間引ける
- CPU と GPU の結果差を diagnostics で追える

---

### Phase 3: MDI Submission Path

**Goal:** まとめられる draw を MDI で送る。

- indirect args buffer を組み立てる
- pass ごとに batch を作る
- backend ごとの制約を吸収する layer を置く
- MDI 非対応 backend は既存 path に落とす

**Done when:**

- 同一メッシュ / 同一 PSO の draw がまとまる
- draw call 数が明確に減る

---

### Phase 4: Render Queue Integration

**Goal:** render queue と MDI path を統合する。

- preview / playback / export で共通化する
- render mode ごとに GPU / CPU を切り替えられるようにする
- queue snapshot を diagnostics に出す
- frame capture しやすい形にする

**Done when:**

- render queue から MDI path へ自然に流れる
- fallback 切り替えが UI / log から追える

---

### Phase 5: Stabilization

**Goal:** 実運用で壊れない形にする。

- backend 差分の検証
- regression test の追加
- GPU/CPU 差分の可視化
- 順次対象を増やす

**Done when:**

- 主要な render path で安定動作する
- どのケースで CPU fallback に落ちたか説明できる

---

## Risks

- GPU 側で作る前提が増えると、debug が難しくなる
- backend 差分が増えると、MDI が実質的に API ごとの別実装になる
- 既存の render queue / composition/controller と責務が重複しやすい

---

## Suggested Order

1. Phase 1
2. Phase 2
3. Phase 3
4. Phase 4
5. Phase 5

---

## Related

- `docs/planned/MILESTONE_RENDER_PATH_DECOMPOSITION_2026-03-31.md`
- `docs/planned/MILESTONE_RENDER_QUEUE_2026-03-22.md`
- `docs/planned/MILESTONE_DILIGENT_LOW_LEVEL_API_2026-04-01.md`
- `docs/planned/MILESTONE_GPU_EFFECT_PARITY_2026-03-27.md`

## Current Status

GPU-driven MDI は構想段階。  
まずは draw item の共通化と fallback を固めてから、MDI submission に進めるのが現実的。
