# M-IR-8 Phase 1 Execution

`ImmediateContext` / `IDeviceContext` の直叩きを、どこから減らすべきかを file 単位で棚卸しした実行メモ。

## Priority A: 外側から backend に手を入れている箇所

### 1. `Artifact/src/Widgets/Render/ArtifactCompositionRenderController.cppm`

現状:
- `renderer->immediateContext()` を直接取得している
- `ctx->SetRenderTargets(...)`
- `ctx->SetViewports(...)`
- compute blend 実行前の RTV 解除

理由:
- controller は本来 `ArtifactIRenderer` の利用者側であり、backend context を直接持つ境界ではない
- `Frame Debug` / `Pipeline View` / ROI / partial blend の追加でここが膨らむと、renderer boundary が壊れやすい

Phase 2 候補:
- `ArtifactIRenderer` に
  - `beginExternalLayerBlendPass(...)`
  - `endExternalLayerBlendPass(...)`
  - `setViewportRect(...)`
  - `unbindColorTargetsForCompute()`
  などの façade を追加して controller 側の直叩きを減らす

### 2. `Artifact/src/Preview/ArtifactPreviewCompositionPipeline.cppm`

現状:
- `ArtifactIRenderer` 経由が主流だが、particle / special path の影響を受けやすい
- direct context ではないが、boundary audit の対象

理由:
- preview は composition editor と責務が似ていて、直叩きが入りやすい
- `CompositionRenderController` とずれた独自 path が増えると保守不能になる

Phase 2 候補:
- `CompositionRenderController` と同じ renderer entry を使う
- preview 専用 special path を summary API か façade API に寄せる

## Priority B: backend 内部だが façade 化したい箇所

### 3. `Artifact/src/Render/ArtifactIRenderer.cppm`

現状:
- `deviceManager_.immediateContext()` を広く使用
- `clear`, `flush`, `flushAndWait`, `present`
- `readbackToImage`, `readbackDepthToImage`
- `drawParticles`
- offscreen RT bind / restore
- query begin / end

理由:
- ここは backend 内部なので直ちに悪ではない
- ただし API 境界として残すものと、submitter / helper に再配置できるものを分けたい

分解候補:
- `FrameOps`: clear / present / flush / query
- `ReadbackOps`: readback 系
- `TargetStackOps`: push / pop / bind / restore
- `ParticlePassOps`: particle 専用 direct draw path

### 4. `Artifact/src/Render/PrimitiveRenderer2D.cppm`

現状:
- `clear(IDeviceContext* ctx, ...)`
- `setContext(IDeviceContext*, ISwapChain*)`

理由:
- 2D path 自体は command buffer 化されているが、clear と current RTV 取得は low-level 前提

Phase 2 候補:
- `clearCurrentTarget(...)` を `ArtifactIRenderer` 側へ寄せる
- `PrimitiveRenderer2D` は packet 生成と current target summary に寄せる

### 5. `Artifact/src/Render/PrimitiveRenderer3D.cppm`

現状:
- `ctx_` を内部保持
- `MapBuffer`, `SetRenderTargets`, `SetPipelineState`, `CommitShaderResources`

理由:
- gizmo / 3D primitive は submitter 化がまだ弱い
- ここは `ImmediateContext` を一気に消すより、3D submitter 化の separate workstream に切る方が安全

Phase 2 候補:
- まず `PrimitiveRenderer3D` を renderer internal と明示
- その後 `Gizmo3DSubmitter` 的な単位に分ける

## Priority C: low-level window / pipeline shell

### 6. `Artifact/src/Widgets/Render/ArtifactDiligentEngineRenderWindow.cpp`

現状:
- `IDeviceContext` を直接保持
- swapchain, buffer map, draw, wireframe draw を直接実行

理由:
- 旧系 / shell 系の low-level window
- ここを基準に新規実装を増やすべきではない

扱い:
- Phase 1 では「legacy shell」として明示
- 新規機能はここへ追加しない

### 7. `Artifact/src/Render/ArtifactOffscreenRenderer.cppm`

現状:
- `pContext_->SetRenderTargets(...)` を直叩き

理由:
- offscreen capture は low-level backend shell として残る可能性が高い
- ただし façade API 化できる余地はある

扱い:
- `ArtifactIRenderer` headless path と責務を比較して重複を減らす

### 8. `Artifact/src/Render/ArtifactRenderLayerPipeline.cppm`

現状:
- `IDeviceContext* ctx` を引数に取り `SetRenderTargets(...)`

理由:
- compute / layer pipeline 系の low-level bridge
- renderer boundary 外に見えるので、将来的に一段 wrapper を入れたい

## Keep As Backend-Internal

以下は M-IR-8 の対象ではあるが、直ちに「悪い直叩き」とはみなさない。

- `Artifact/src/Render/DiligentImmediateSubmitter.cppm`
  - ここは command buffer 実行者なので `IDeviceContext` 直使用が本務
- `Artifact/src/Render/DiligentDeviceManager.cppm`
  - device / context 取得と共有管理の責務

## Proposed Work Order

1. `CompositionRenderController` の direct context access を façade API に寄せる
2. `ArtifactIRenderer` の target / viewport / compute 橋渡し API を整理する
3. `PrimitiveRenderer2D::clear()` の低レベル依存を renderer internal helper へ寄せる
4. `PrimitiveRenderer3D` は別 workstream として submitter 化を検討する
5. legacy shell (`ArtifactDiligentEngineRenderWindow`) は新規責務追加を止める

## Notes

- `ImmediateContext` を repo 全体から消すのではなく、利用層を backend 内部へ後退させるのが目的
- `DiligentImmediateSubmitter` はむしろ正規の low-level endpoint として残す
- DX12 / Vulkan backend の低レベル path は推測で広く触らず、まず façade を増やして上位から触れない構造にする
