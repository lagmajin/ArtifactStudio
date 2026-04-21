# Composition Editor Figma-like Overlay

> 2026-04-21

## Summary

- コンポジットエディタに Figma っぽい操作補助を入れる方針を整理した
- 重点は `Smart Guides`, `Selection Overlay`, `Useful HUD`, `Probe`
- overlay はできるだけ本体描画経路へ統合し、別 widget の増殖を避ける

## Evidence

- `CompositionRenderController` が描画本体の中心
- `TransformGizmo` が移動 / 拡縮 / 回転の操作入口になっている
- 既存の `FrameDebug` / `Trace` と情報を共有できる

## Risks

- overlay と parent widget の二重管理は更新ズレの原因になりやすい
- snap を強制にすると操作感が変わりすぎる
- HUD を増やしすぎると逆に読みにくくなる

## Next Steps

- Phase 1: snap / smart guides
- Phase 2: selection bounds / handles / rotation HUD
- Phase 3: context HUD / pixel probe
