# After Effects Parity Handoff

> 2026-05-30

## Purpose

次の AI が After Effects parity の調査や整理をすぐ再開できるようにするための入口メモ。

## Read First

1. [AFTER_EFFECTS_PARITY_MASTER_SUMMARY_2026-05-30.md](AFTER_EFFECTS_PARITY_MASTER_SUMMARY_2026-05-30.md)
2. [AFTER_EFFECTS_MISSING_FEATURES_CURRENT_2026-05-28.md](X:/Dev/ArtifactStudio/docs/analysis/AFTER_EFFECTS_MISSING_FEATURES_CURRENT_2026-05-28.md)
3. [MILESTONE_AFTER_EFFECTS_PARITY_GAP_2026-05-28.md](X:/Dev/ArtifactStudio/docs/planned/MILESTONE_AFTER_EFFECTS_PARITY_GAP_2026-05-28.md)
4. [AFTER_EFFECTS_PARITY_COMPARISON_2026-05-30.md](AFTER_EFFECTS_PARITY_COMPARISON_2026-05-30.md)
5. [AFTER_EFFECTS_PARITY_CHECKLIST_2026-05-30.md](AFTER_EFFECTS_PARITY_CHECKLIST_2026-05-30.md)
6. [AFTER_EFFECTS_PARITY_DOCUMENT_MAP_2026-05-30.md](AFTER_EFFECTS_PARITY_DOCUMENT_MAP_2026-05-30.md)

## Current Working View

- P0 は preview / cache / playback stability、track matte / alpha correctness、blend coverage
- P1 は keyframe interpolation / graph editor、text animator UX、motion blur、adjustment layer、parent propagation
- P2 は markers、shape operators、precompose workflow、layer styles、time remap / frame blend、expression completeness
- P3 は effects expansion、OCIO / ACES、3D camera tracker、plugin SDK / AEX compatibility、mogrt-like templates、Python API coverage

## Practical Order

1. stability
2. compositing correctness
3. graph editor / interpolation
4. text animator integration
5. precompose / parent / motion blur
6. long-tail interop

## Notes

- `Fill` / `100%` の viewport 問題は AE parity とは別で、UI contract と座標系の問題として切り分ける
- 外部AI の要約は参考値として使い、優先順位は現行 parity gap 文書を基準にする
- 迷ったら checklist に戻る
- 入口を見失ったら document map に戻る
- まず迷ったら master summary に戻る
