# MILESTONE: RAM Preview Range Policy and Priority - Phase 2 Execution

作成日: 2026-06-24
対象: [MILESTONE_RAM_PREVIEW_RANGE_POLICY_AND_PRIORITY_2026-05-31.md](X:/Dev/ArtifactStudio/docs/planned/MILESTONE_RAM_PREVIEW_RANGE_POLICY_AND_PRIORITY_2026-05-31.md)

## Goal

`RAM preview` の request ordering に `playback direction bias` を反映し、再生中は forward / reverse の向きに先に温める。

Phase 1 で固定した `priority reason` をそのまま使い、Phase 2 では `どの frame を先に作るか` の順序に差を出す。

## Scope

- playback 中の directional side を先行
- pause 中は current frame 近傍を中心に対称化
- reverse playback 時は方向を左右反転
- request ordering の入口を service 側に置く

## Not Yet

- loop wraparound の完全実装
- scrub 方向の細かな追従
- disk cache の再編

## Done For Phase 2

- request ordering が direction aware である
- forward / reverse / pause で順序の違いが読める
- Phase 1 の `priority reason` と混同しない

## Follow-Up

Phase 3 では pause / scrub の warmup 形状をさらに詰める。
