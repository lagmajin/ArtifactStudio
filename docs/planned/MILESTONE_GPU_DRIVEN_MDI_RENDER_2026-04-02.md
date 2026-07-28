# Milestone: GPU-Driven MDI Render (2026-04-02)

**ステータス:** In Progress

**Goal:** CPU 依存の draw submission を減らし、GPU 側で可視判定・集約・`Multi-Draw Indirect` 相当の描画指示生成を行える render path を作る。

**上位導入ゲート:** `docs/planned/MILESTONE_INTERACTIVE_RENDER_PERFORMANCE_2026-07-27.md` の `RP-8 GPU-Driven Submission`

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

平面 1 枚や少数の通常 2D layer では、indirect args 生成、compute dispatch、resource barrier の固定費が利益を上回る可能性がある。初期対象は clone、particle、instanced mesh など、同一 pipeline の draw item が大量に存在し、CPU submission が実測上の bottleneck になっている workload とする。

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
- visible count、overflow count、culled count を GPU counter として記録する
- args / compacted item buffer の capacity を超えない bounded write にする

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
- UAV write から indirect argument read への resource state transition を明示する
- 少数 item では既存 direct path を選ぶ break-even threshold を計測する

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
- 透明 layer、matte、adjustment layer を越えた並べ替えは visual correctness を壊す
- item 数が少ない scene では GPU compaction と indirect submission の固定費が逆効果になる
- args buffer overflow や frame slot の早期再利用は GPU fault / stale draw の原因になる

---

## Measurement Contract

- CPU draw item build time
- CPU submission time
- GPU culling / compaction time
- indirect args generation time
- candidate / visible / culled / submitted item count
- direct / indirect draw count
- batch break reason
- args buffer capacity / high-water mark / overflow count
- CPU fallback count / reason
- input-to-visible latency

GPU-driven path は、対象 scene で CPU preparation / submission を削減し、GPU frame time と入力遅延を悪化させない場合だけ既定有効候補とする。

---

## Suggested Order

1. Phase 1
2. Phase 2
3. Phase 3
4. Phase 4
5. Phase 5

---

## Related

- `docs/planned/MILESTONE_INTERACTIVE_RENDER_PERFORMANCE_2026-07-27.md`
- `docs/planned/MILESTONE_RENDER_PATH_DECOMPOSITION_2026-03-31.md`
- `docs/planned/MILESTONE_RENDER_QUEUE_2026-03-22.md`
- `docs/planned/MILESTONE_DILIGENT_LOW_LEVEL_API_2026-04-01.md`
- `docs/planned/MILESTONE_GPU_EFFECT_PARITY_2026-03-27.md`

## Current Status

2026-07-27 に Clone / Instanced Mesh と Particle を最初の対象として submission foundation を導入した。

- Clone の現在の描画本流は `MeshRenderer` ではなく `SolidRectXformPkt` であったため、変形矩形を最大 512 item の単一 vertex/index batch へ統合した。
- Clone batch は 64 item 以上かつ backend 対応時に `DrawIndexedIndirect` を使用する。
- `MeshRenderer` は 64 instance 以上で indexed / non-indexed indirect draw を選択する。
- `ParticleRenderer` は 64 particle 以上で `DrawIndirect` を選択する。
- capability 不足、buffer 作成失敗、少数 item では既存 direct draw を維持する。
- indirect args buffer は persistent resource とし、draw ごとの buffer 作成を行わない。

第一段のindirect argsはCPUがactive countから更新するfoundationとして導入した。その後のGPU visibility対応は以下の更新で継続する。

### 2026-07-27 GPU visibility / compaction update

`MeshRenderer` と `ParticleRenderer` に compute-based visibility path を追加した。

- input structured buffer は変更せず、GPU-visible output bufferへcompactする。
- compute shader がvisible itemごとにIndirect Argsのinstance countをatomic incrementする。
- output capacityはinput upload capacityと同じ上限を使い、buffer外書き込みを防ぐ。
- compute dispatch後は同じpersistent args bufferを`DrawIndirect` / `DrawIndexedIndirect`へ渡す。
- Particleは描画順を変えても意味が保たれるAdditive blendだけを対象とする。
- Meshはtransparent passを除外し、opaque instanceだけを対象とする。
- Mesh boundsはupload済みgeometryからlocal bounding radiusを計算し、instance scaleを含むconservative sphereとして判定する。
- shader compilation、PSO、SRB、UAV、Indirect capabilityのいずれかが成立しない場合は既存pathへ戻る。

残作業:

- GPU counterの非同期diagnostics readback。
- CPU referenceとのvisibility parity validation mode。
- Cloneの現在の`SolidRectXformPkt`経路に対するGPU-side packet compaction。現在のCloneはGPU batch / indirect submissionまでで、visibility判定は未統合。
- backend別のUAV / indirect args buffer mode検証。
