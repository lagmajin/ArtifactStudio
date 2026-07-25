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

---

## Static audit follow-up (2026-07-25)

`ArtifactPlaybackService` と RAM preview build queue の現行実装を照合した。ビルド・実機再生は未実施。

| 条件 | 現状 | 判定 |
|---|---|---|
| playback direction aware ordering | queue の priority と directional ordering、再生方向の状態が存在する。 | ソース上確認済み |
| forward / reverse の順序差 | forward/reverse を考慮する policy helper はあるが、ready range の実際の生成順は未実測。 | 実行確認待ち |
| pause の current-frame 中心 | current frame/near priority と pause 状態は存在する。 | 部分実装／実行確認待ち |
| Phase 1 reason との分離 | priority reason と state/failure reason は別経路。 | ソース上確認済み |
| loop wraparound / scrub 追従 | 本文の Not Yet のまま。 | 未完了 |

### Phase 2 判定

方向を意識した priority/order の基盤は入っているが、forward/reverse/pause の体感差と loop 境界は未検証。Phase 2 は「部分実装／実行確認待ち」とする。
