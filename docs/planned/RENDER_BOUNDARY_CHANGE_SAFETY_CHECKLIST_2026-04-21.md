# Render Boundary Change Safety Checklist

`ImmediateContext` 境界整理、particle pass 整理、render entry 統合を実装するときに、描画を壊しやすい点を先に確認するための checklist。

## 1. Render Target / Depth Target

- `RTV` を bind したあと、別経路で `SetRenderTargets(0, nullptr, dsv, ...)` していないか
- compute 前の `unbind` 後に、color target を戻し忘れていないか
- `overrideRTV` / offscreen RT / swapchain RTV の戻し先が 1 か所にまとまっているか
- `DSV` が必要な path と不要な path を混ぜていないか

## 2. Viewport / Canvas / Pan / Zoom

- offscreen 描画後に host viewport を復元しているか
- `canvas size` と `viewport size` を取り違えていないか
- `zoom/pan` を screen-space blit 前後で戻しているか
- particle / gizmo / overlay が同じ view/proj 前提で動いているか

## 3. Submit Order

- command buffer に積む draw と direct draw の順序が崩れていないか
- direct draw 前に必要なら pending submit を flush しているか
- `present()` 前に想定外の empty submit / double submit が入っていないか

## 4. Matrix Contract

- `QMatrix4x4` と shader 側 row/column convention を取り違えていないか
- transpose 前提の path を途中で変えていないか
- particle / 3D gizmo / screen blit で別々の行列契約が混ざっていないか

## 5. Particle Specific

- active RTV が本当に有効か
- particle count 0 のとき早期 return しているか
- PSO / SRB / buffer 未初期化で draw していないか
- billboard size が小さすぎて「描かれているが見えない」状態になっていないか

## 6. Readback / Debug

- debug view のために low-level context へ逆戻りしていないか
- attachment / resource / submit 情報を summary API で取れるか
- readback helper が staging texture / map の責務を持ちすぎていないか

## 7. Safe Work Order

1. trace / summary を先に足す
2. façade API を追加する
3. 既存 call site を 1 つずつ置き換える
4. direct context access を最後に閉じる

## 8. Red Flags

- `CompositionRenderController` に新しい `ctx->...` を足す
- widget 側で `IDeviceContext` を import する
- debug 用のつもりで low-level accessor を public に増やす
- particle path だけ別の matrix / target contract を持つ

## Related

- `docs/planned/MILESTONE_IMMEDIATE_CONTEXT_BOUNDARY_2026-04-21.md`
- `Artifact/docs/MILESTONE_PARTICLE_RENDER_PATH_STABILIZATION_2026-04-21.md`
