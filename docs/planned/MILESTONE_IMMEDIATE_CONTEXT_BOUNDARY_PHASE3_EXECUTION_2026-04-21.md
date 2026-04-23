# M-IR-8 Phase 3 Execution

`ImmediateContext` / `IDeviceContext` の access を縮小し、上位コードから low-level 型を見えにくくするための実行メモ。

## Ticket 1: `immediateContext()` Access Split

### Target

- `Artifact/include/Render/ArtifactIRenderer.ixx`
- `Artifact/src/Render/ArtifactIRenderer.cppm`

### Current Problem

- `ArtifactIRenderer::immediateContext()` が public に見えている
- widget / controller 側が「必要だから取る」入口になっている
- renderer façade を足す前に、ここへ依存が増えやすい

### Planned Change

- `immediateContext()` をそのまま増やさない
- 代わりに用途別 façade を増やす
  - render target bind / unbind
  - viewport sync
  - readback request
  - frame summary / resource summary

### Optional Split

- diagnostics 専用の read-only access を残すなら
  - `backendSummary()`
  - `attachmentSummary()`
  - `resourceSummary()`
  などの summary API に置き換える

### Done When

- 上位コードが `IDeviceContext` を import しなくても進められる
- `immediateContext()` を新規 call site が増やさない

## Ticket 2: Render Target Stack Boundary

### Target

- `Artifact/src/Render/ArtifactIRenderer.cppm`
- `Artifact/src/Widgets/Render/ArtifactCompositionRenderController.cppm`

### Current Problem

- override RTV
- offscreen target push / pop
- compute 前の RTV 解除

が複数箇所に分散している

### Planned Change

- `RenderTargetScope` 相当の renderer internal helper を導入
- 外側は
  - `pushRenderTarget`
  - `popRenderTarget`
  - `unbindColorTargetsForCompute`
  だけで済むようにする

### Done When

- RT bind / restore の責務が `ArtifactIRenderer::Impl` にまとまる
- controller 側の state restore コードが減る

## Ticket 3: Readback Narrowing

### Target

- `Artifact/src/Render/ArtifactIRenderer.cppm`
- `Artifact/src/Render/ArtifactOffscreenRenderer.cppm`

### Current Problem

- readback は low-level texture / fence / map を抱えている
- そのため「少し情報が欲しい」だけでも context 依存が増えやすい

### Planned Change

- readback を
  - final color image
  - depth image
  - attachment snapshot
  - float readback
  に整理
- `Frame Debug` / `Resource Inspector` は readback helper を叩く形に寄せる

### Done When

- debug view 側から staging texture や map の知識が消える

## Ticket 4: Particle Context Encapsulation

### Target

- `Artifact/src/Render/ArtifactIRenderer.cppm`
- `ArtifactCore/src/Graphics/ParticleRenderer.cppm`

### Current Problem

particle path だけ:

- pending submit flush
- RTV bind
- matrix upload
- direct draw

を renderer 側で抱えている

### Planned Change

- `ParticlePassContext` か同等 helper を renderer internal へ導入
- `ParticleRenderer` 自体は
  - initialize
  - updateBuffer
  - prepare
  - draw
  に集中

### Done When

- particle draw 前提条件が 1 helper に閉じる
- billboard path の故障点を局所化できる

## Ticket 5: Legacy API Freeze List

### Freeze Targets

- `ArtifactIRenderer::immediateContext()`
- widget / controller からの `IDeviceContext` 直接利用
- shell 系 window に新しい low-level draw を足すこと

### Rule

- 新しい render feature はまず `ArtifactIRenderer` façade へ追加
- low-level Diligent call を新規に外側へ漏らさない

## Done Criteria

- `IDeviceContext` の public 露出が実質減る
- `Frame Debug` / `Pipeline View` / `Trace` が summary API で必要情報を取れる土台ができる
- particle / blend / preview の特殊 path も façade 越しに整理しやすくなる
