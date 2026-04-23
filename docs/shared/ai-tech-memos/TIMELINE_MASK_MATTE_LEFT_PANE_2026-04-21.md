# Timeline Left Pane: Mask / Matte Display

> 2026-04-21

## Summary

- 左ペインの visible row に `Masks` / `Track Mattes` の display-only stack を追加した
- `Layer` の子として見出し行を作り、その下に個別の mask / matte row を並べる
- 編集や選択は増やさず、表示だけに限定している

## Evidence

- `ArtifactLayerPanelWidget` の `TimelineRowKind` に `MaskStack / Mask / MatteStack / Matte` を追加
- `ArtifactAbstractLayer` はすでに `maskCount()` / `hasMasks()` / `matteReferences()` を持っている
- mask row には path 数と `Add / Sub / Int / Diff` を表示
- matte row には `Alpha / Luma / Blend / Opacity / Inverted` を表示

## Risks

- row kind が増えたので、将来の selection / drag / filter ロジックで分岐漏れが起きやすい
- `visibleTimelineRowDescriptors()` の consumer は kind 増加を前提にしないといけない

## Next Steps

- 必要なら row 左端に小さい badge を足す
- filter / search で mask / matte をどう扱うかを決める
- 右ペインとの行揃え確認を続ける
