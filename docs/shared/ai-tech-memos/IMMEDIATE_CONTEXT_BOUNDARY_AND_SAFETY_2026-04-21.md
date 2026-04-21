# ImmediateContext Boundary / Safety Notes

> 2026-04-21

## Summary

- `ImmediateContext` を repo 全体から消すのではなく、backend 内部に隔離する方針
- 上位層は `ArtifactIRenderer` / `RenderCommandBuffer` / `DiligentImmediateSubmitter` を通す
- 低レベル access は `present / flush / readback / swapchain` に閉じ込める

## Evidence

- `ArtifactIRenderer` はまだ `IDeviceContext` を内部保持している
- `CompositionRenderController` / overlay / preview の直叩きが境界整理の対象
- `Frame Debug` / `Pipeline View` / `Trace` は renderer summary API で読めるように寄せる

## Risks

- `DiligentEngine` / DX12 backend は読み違えやすい
- overlay / gizmo / preview を急にまとめすぎると描画崩れしやすい
- build tree の module cache や vcpkg baseline 変更が重なると原因追跡が難しくなる

## Next Steps

- 上位からの direct context access を棚卸しする
- safe sequence に沿って trace -> façade API -> controller 置換の順で進める
- particle path は別の小さい bugfix workstream で扱う
