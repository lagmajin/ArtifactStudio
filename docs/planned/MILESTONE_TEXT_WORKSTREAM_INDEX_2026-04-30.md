# Text Workstream Index

**Date**: 2026-04-30

Text 系の次フェーズをまとめた索引。

---

## 1. Text Animator Next Gen

- Plan: [`MILESTONE_TEXT_ANIMATOR_NEXT_GEN_2026-04-18.md`](./MILESTONE_TEXT_ANIMATOR_NEXT_GEN_2026-04-18.md)
- Execution: [`MILESTONE_TEXT_ANIMATOR_NEXT_GEN_EXECUTION_2026-04-30.md`](./MILESTONE_TEXT_ANIMATOR_NEXT_GEN_EXECUTION_2026-04-30.md)
- Phase 1: [`MILESTONE_TEXT_ANIMATOR_NEXT_GEN_PHASE1_EXECUTION_2026-04-30.md`](./MILESTONE_TEXT_ANIMATOR_NEXT_GEN_PHASE1_EXECUTION_2026-04-30.md)
- Focus: UI / selector / preset / timeline

## 2. GPU Text Rendering / Japanese Shaping

- Plan: [`ArtifactCore/docs/MILESTONE_GPU_TEXT_RENDERING_JA_2026-04-01.md`](../../ArtifactCore/docs/MILESTONE_GPU_TEXT_RENDERING_JA_2026-04-01.md)
- Execution: [`ArtifactCore/docs/MILESTONE_GPU_TEXT_RENDERING_JA_EXECUTION_2026-04-30.md`](../../ArtifactCore/docs/MILESTONE_GPU_TEXT_RENDERING_JA_EXECUTION_2026-04-30.md)
- Focus: font fallback / shaping / atlas / backend parity

---

## Recommended Order

1. `Text Animator Next Gen`
2. `GPU Text Rendering / Japanese Shaping`

理由:
- `Text Animator` は既存 integration の続きを詰めやすい
- `GPU Text Rendering` はその見た目品質と backend 安定性を支える
