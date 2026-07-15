# Text Workstream Index

**Date**: 2026-04-30

Text 系の次フェーズをまとめた索引。

> **Canonical milestone:** [`MILESTONE_TEXT_LAYER_GPU_EDIT_ANIMATION_2026-07-16.md`](./MILESTONE_TEXT_LAYER_GPU_EDIT_ANIMATION_2026-07-16.md)  
> 2026-07-16 以降、GPU描画・inline edit・Source Text・Text Animator・多言語対応の全体判断は統合マイルストーンを正規入口とする。この文書は旧workstreamの索引として残す。

---

## 1. Text Animator Next Gen

- Plan: [`MILESTONE_TEXT_ANIMATOR_NEXT_GEN_2026-04-18.md`](./MILESTONE_TEXT_ANIMATOR_NEXT_GEN_2026-04-18.md)
- Execution: 親文書へ統合済み
- Focus: UI / selector / preset / timeline
- Note: execution / phase docs are slices of the same parent milestone, not separate workstreams.

## 2. GPU Text Rendering / Japanese Shaping

- Plan: [`ArtifactCore/docs/MILESTONE_GPU_TEXT_RENDERING_JA_2026-04-01.md`](../../ArtifactCore/docs/MILESTONE_GPU_TEXT_RENDERING_JA_2026-04-01.md)
- Execution: 親文書へ統合済み
- Focus: font fallback / shaping / atlas / backend parity

## 3. Complex Script / Vertical Writing

- Plan: [`MILESTONE_TEXT_ANIMATOR_COMPLEX_SCRIPT_VERTICAL_2026-06-12.md`](./MILESTONE_TEXT_ANIMATOR_COMPLEX_SCRIPT_VERTICAL_2026-06-12.md)
- Execution: 親文書へ統合済み
- Focus: grapheme / glyph cluster / RTL / vertical writing / selector semantics
- Note: this is a semantics slice that depends on the shaping backend, not a duplicate of `Text Animator Next Gen`.

## 4. Text Shaping Backend / HarfBuzz

- Plan: [`ArtifactCore/docs/MILESTONE_TEXT_SHAPING_BACKEND_HARFBUZZ_2026-06-12.md`](../../ArtifactCore/docs/MILESTONE_TEXT_SHAPING_BACKEND_HARFBUZZ_2026-06-12.md)
- Execution: 親文書へ統合済み
- Focus: Qt fallback adapter / HarfBuzz backend / logical-visual mapping / shaping contract

## 5. Text Layout Contract

- Plan: [`ArtifactCore/docs/MILESTONE_TEXT_LAYOUT_CONTRACT_2026-06-12.md`](../../ArtifactCore/docs/MILESTONE_TEXT_LAYOUT_CONTRACT_2026-06-12.md)
- Focus: cluster spans / bidi runs / ruby / tate-chu-yoko / kinsoku / punctuation policy

---

## Recommended Order

1. `Text Animator Next Gen`
2. `GPU Text Rendering / Japanese Shaping`
3. `Complex Script / Vertical Writing`
4. `Text Shaping Backend / HarfBuzz`
5. `Text Layout Contract`

理由:
- `Text Animator` は既存 integration の続きを詰めやすい
- `GPU Text Rendering` はその見た目品質と backend 安定性を支える
- `Complex Script / Vertical Writing` は text semantics を壊さずに多言語化する土台になる
- `HarfBuzz` は complex script を支える shaping の正規経路を作りやすい
- `Text Layout Contract` はそれらを跨ぐ共通意味論を固定しやすい
