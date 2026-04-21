# M-IR-8 Phase 2 Execution

`ImmediateContext` 直叩きを減らすために、どこへ façade API を増やし、どこから剥がすかを実装票レベルで切ったメモ。

## Ticket 1: Composition Controller De-direct

### Target

- `Artifact/src/Widgets/Render/ArtifactCompositionRenderController.cppm`
- `Artifact/include/Render/ArtifactIRenderer.ixx`
- `Artifact/src/Render/ArtifactIRenderer.cppm`

### Current Problem

controller 側で:

- `renderer->immediateContext()` を取る
- `SetRenderTargets(0, nullptr, nullptr, ...)`
- `SetViewports(...)`

を直接実行している。

### Planned API

- `ArtifactIRenderer::unbindColorTargetsForCompute()`
- `ArtifactIRenderer::setViewportRect(float width, float height)`
- `ArtifactIRenderer::restoreHostViewport(float width, float height)`
- `ArtifactIRenderer::setOverrideRTV(...)` は維持しつつ、bind/unbind の細部を renderer 内へ寄せる

### Done When

- controller 側から `IDeviceContext` 型が消える
- GPU blend path でも renderer façade だけで流れる

## Ticket 2: Particle Draw Boundary Cleanup

### Target

- `Artifact/src/Render/ArtifactIRenderer.cppm`
- `ArtifactCore/src/Graphics/ParticleRenderer.cppm`
- `Artifact/src/Layer/ArtifactParticleLayer.cppm`

### Current Problem

`drawParticles()` は:

- pending command buffer flush
- RTV 再バインド
- matrix upload
- particle buffer update
- direct draw

を 1 か所でまとめて抱えている。

### Planned Split

- `ParticlePassContext` 相当の内部 helper を導入
- `drawParticles()` から
  - target setup
  - matrix setup
  - buffer update / prepare / draw
  を段階分離する

### Note

particle 非表示の主因は「billboard API 単独故障」より、

- RTV 未再バインド
- view/proj 行列
- PSO / SRB 準備

の複合である可能性が高い。既存 bug note と現コードはこの理解で整合している。

### Done When

- `drawParticles()` が renderer internal helper 経由で読める
- particle draw の前提条件がログなしでも追える

## Ticket 3: PrimitiveRenderer2D Clear Boundary

### Target

- `Artifact/src/Render/PrimitiveRenderer2D.cppm`
- `Artifact/src/Render/ArtifactIRenderer.cppm`

### Current Problem

- `PrimitiveRenderer2D::clear(IDeviceContext* ctx, ...)`
- `currentRTV()` の知識

が low-level context 前提になっている。

### Planned Change

- clear 系は renderer internal helper に移す
- `PrimitiveRenderer2D` は packet 生成と target summary に寄せる

### Done When

- clear path の主体が `ArtifactIRenderer::Impl` になる
- 2D primitive 側で context を直接受ける必要が減る

## Ticket 4: 3D Primitive Separate Workstream

### Target

- `Artifact/src/Render/PrimitiveRenderer3D.cppm`

### Current Problem

`PrimitiveRenderer3D` は:

- `ctx_` を保持
- buffer map
- PSO bind
- SRB commit
- draw

をまとめて持っている。

### Strategy

ここは Phase 2 で一気に剥がさず、

- renderer-internal のまま維持
- separate submitter 化を別 workstream にする

### Done When

- M-IR-8 では「新規直叩きの増殖を止める」
- 3D path は後続 phase へ回す

## Ticket 5: Legacy Shell Freeze

### Target

- `Artifact/src/Widgets/Render/ArtifactDiligentEngineRenderWindow.cpp`
- `Artifact/src/Render/ArtifactOffscreenRenderer.cppm`
- `Artifact/src/Render/ArtifactRenderLayerPipeline.cppm`

### Strategy

- 新機能をこれらへ足さない
- 既存 shell / bridge として扱う
- 可能なら `ArtifactIRenderer` へ責務を戻す

## Thread Churn Note

コンポ初期化だけで大量の `code 0` thread exit が見える件は、少なくとも次の候補が重なっている。

- `ArtifactCore/src/Thread/ThreadHelper.cppm`
  - `sharedBackgroundThreadPool()`
- `Artifact/src/Layer/ArtifactVideoLayer.cppm`
  - `openFuture_`, `decodeFuture_`, `publishVideoLayerModifiedAsync`
- `Artifact/src/Layer/ArtifactImageLayer.cppm`
  - prefetch future
- `Artifact/src/Layer/ArtifactSvgLayer.cppm`
  - prefetch future
- `Artifact/src/Render/ArtifactRenderScheduler.cppm`
  - dedicated `QThreadPool`
- `Artifact/src/Playback/ArtifactPlaybackEngine.cppm`
  - worker `QThread`
- `Artifact/src/Widgets/Render/ArtifactCompositionRenderController.cppm`
  - background jobs / lazy pipeline init

これは crash ではないが、startup 時の worker churn として観測価値が高い。

### Follow-up

- `M-DIAG-3 Lightweight Tracer / Frame Timeline`
- `Thread Trace / Lock Trace`

と接続して、startup burst を frame/thread lane で見えるようにするのが自然。
