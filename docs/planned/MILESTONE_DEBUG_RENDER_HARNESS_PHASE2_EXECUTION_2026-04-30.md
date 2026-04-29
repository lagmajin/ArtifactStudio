# Debug Render Harness - Phase 2 Execution

**Date**: 2026-04-30  
**Source**: [`MILESTONE_DEBUG_RENDER_HARNESS_2026-04-30.md`](./MILESTONE_DEBUG_RENDER_HARNESS_2026-04-30.md)  
**Parent Phase 1**: [`MILESTONE_DEBUG_RENDER_HARNESS_PHASE1_EXECUTION_2026-04-30.md`](./MILESTONE_DEBUG_RENDER_HARNESS_PHASE1_EXECUTION_2026-04-30.md)

---

## Phase 2 Goal

最小シーンを実際に回せるようにする。

この段階では本番 composition の複雑さを捨てて、`particle-only` / `video-only` を最優先で可視化する。  
`blend-only` と `overlay-only` は、表示 contract を揃えるための補助プリセットとして扱う。

---

## Scope

### In

- particle-only scene
- video-only scene
- blend-only scene
- overlay-only scene
- no-RTV / no-frame / decode-fail の見え方整理

### Out

- 自動保存 bundle
- 長期 regression gate
- UI を別 window に完全分離する作業
- backend の全面置き換え

---

## Tasks

### 1. Particle-Only Scene

- 背景が暗い / 明るい両方で粒子が見えることを確認できる構成にする
- alive count / draw call / skipped reason を表示する
- no RTV のときは明示的に failed ではなく `skipped` として扱う
- 詳細契約: [`../technical/DEBUG_RENDER_HARNESS_SCENE_PRESET_CONTRACT_2026-04-30.md`](../technical/DEBUG_RENDER_HARNESS_SCENE_PRESET_CONTRACT_2026-04-30.md)

### 2. Video-Only Scene

- 既知の短い MP4 または生成 fixture を使う
- frame 0 と mid-frame を区別して見られるようにする
- `decode pending` / `frame ready` / `decode failed` の状態を表示する

### 3. Blend-Only Scene

- 2 つの入力が重なったときの visible / transparent を確認する
- blend mode と opacity の影響を読みやすくする
- output が空のときは `transparent output` として記録する

### 4. Overlay-Only Scene

- grid / anchor / basic guidance の見え方を確認する
- scene 自体が壊れていても overlay の表示 contract が残るようにする

### 5. Scene Switch Contract

- preset を切り替えたときに前 scene の state を引きずらない
- harness state reset を明示する
- `FrameDebugSnapshot` の snapshot 時点と scene state を合わせる

---

## Expected Signals

### Particle

- `ParticleRenderer` draw / skip state
- RTV state
- viewport
- particle count

### Video

- open result
- decode state
- last error
- target frame / source frame
- buffer readiness

### Blend

- input count
- opacity
- blend mode
- output visibility

### Overlay

- grid / anchor / guidance visibility
- reset after preset switch

---

## Done Criteria

- particle-only scene で visible / skipped の差が読める
- video-only scene で decode state が読める
- blend-only scene で transparent output の有無が読める
- preset switch 後も古い状態が残らない
