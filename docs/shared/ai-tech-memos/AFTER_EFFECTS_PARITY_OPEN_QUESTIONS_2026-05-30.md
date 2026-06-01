# After Effects Parity Open Questions

> 2026-05-30

## Purpose

AE parity 調査を続けるときに、まだ答えが揃っていない論点だけを切り出すメモ。

## Open Questions

### Stability / Preview

- RAM preview の `requested` と `ready` をどういう state contract に固定するか
- cache hit を final image readiness とどう結び付けるか
- playback / scrub / diagnostics が同じ真実を読むための責務境界をどこに置くか

### Compositing

- track matte の評価順をどこで保証するか
- alpha / premultiplied alpha の境界をどの layer / renderer contract に寄せるか
- blend mode coverage の不足を先に埋めるか、例外時の reason 表示を先に整えるか

### Editor UX

- `Fill` は viewport を埋める cover なのか、全体を収める fit なのか
- `100%` は logical pixel 基準にするのか、physical pixel 基準にするのか
- high DPI 環境での zoom/pan 計算をどの層で正規化するか
- resize debounce と initial fit の競合をどのタイミングで解消するか

### Workflow

- keyframe interpolation / graph editor を timeline のどの UI surface に統合するか
- text animator は effect と同じ編集文法に寄せるのか、別 panel のままにするのか
- precompose / adjustment / motion blur を先に workflow で整えるか、機能単位で埋めるか

### Ecosystem

- motion graphics の量産機として `.mogrt` 相当を目指すか
- plugin SDK / OFX / AEX compatibility の優先順位をどこに置くか
- Python API coverage を production use に足る範囲まで広げるか

## Reference

- [After Effects Parity Comparison Notes](AFTER_EFFECTS_PARITY_COMPARISON_2026-05-30.md)
- [After Effects Parity Checklist](AFTER_EFFECTS_PARITY_CHECKLIST_2026-05-30.md)
- [After Effects Parity Handoff](AFTER_EFFECTS_PARITY_HANDOFF_2026-05-30.md)
- [Composition Editor Zoom / Fill Misposition Bug Report](X:/Dev/ArtifactStudio/docs/bugs/BUG_COMPOSITION_EDITOR_ZOOM_FILL_100_PERCENT_MISPOSITION_2026-05-30.md)

## Next Step

次の AI は、このメモのうち `Editor UX` と `Stability / Preview` から読むのが効率的。
- ただし全体像は [After Effects Parity Master Summary](AFTER_EFFECTS_PARITY_MASTER_SUMMARY_2026-05-30.md) から入ると迷いにくい。
