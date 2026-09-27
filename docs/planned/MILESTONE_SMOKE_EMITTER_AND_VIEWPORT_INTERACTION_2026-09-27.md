# M-FLUID-SMOKE-1: Smoke / Ink の emitter 制御とビューポートかき混ぜ

**最終更新:** 2026-09-27
**ステータス:** Not Started
**識別子:** M-FLUID-SMOKE-1
**対象:** `Artifact/src/Layer/ArtifactLayerFluidRuntimeState.cppm`、`Artifact/include/Layer/ArtifactLayerFluidRuntimeState.ixx`、`Artifact/src/Tool/ArtifactBrushTool.cppm`、`Artifact/src/Widgets/Render/ArtifactCompositionRenderController.cppm`
**関連:** `docs/planned/MILESTONE_VFX_PARTICLE_FLUID_2026-03-30.md`（SUPERSEDED）、`docs/planned/MILESTONE_PYRO_DOMAIN_LAYER_INTEGRATION_2026-09-27.md`

## 目的

`FluidSolver2D`（Stable Fluids グリッドソルバ）は Core に実装済みで、`FluidSolver2D` を使う smoke / ink 描画も既に動いている。しかし発生源が ** PROCEDURAL な正弦波 1 点に固定** されており、ユーザーが位置・数・速度を制御できず、ビューポートで直接かき混ぜる操作も存在しない。

本 milestone は、既存コアを触らずに **発生源を制御可能にし、ビューポートでの直接操作を追加する**。

## 現状の根拠（2026-09-27、実コード照合）

### コアは存在し、動作している

- `ArtifactCore/include/Physics/FluidSolver2D.ixx` / `ArtifactCore/src/Physics/FluidSolver2D.cppm`
  - `FluidSolver2D` — advect / diffuse / project / 渦拘束 / 浮力、snapshot・restore、`setResolution` リサンプル、非有限値ガード。
  - `ImageProcessing.FluidVisualizer` による CPU 屈折 pass も存在するが、**呼び出し元が 1 つもない孤立コード**（`import ImageProcessing.FluidVisualizer` する箇所も `render()` を呼ぶ箇所も grep でヒットなし）。現状の描画経路は使わない。
- CMake 登録済み: `ArtifactCore/cmake/ArtifactCoreSources.cmake:570,1191`、`ArtifactCoreModuleReferences.cmake:319`。
- テスト済み: `tests/ArtifactCore/PhysicsDeterminismTest.cpp`（`FluidSolver2D` は `:171` / `:191`、同じテスト内の `LiquidSolver2D` が `:214-506`）。CMake 登録は `tests/ArtifactCore/CMakeLists.txt:173`。

### 発生源が固定されている（本来あるべき自由度がない）

`renderLayerSmokeRuntime`（`Artifact/src/Layer/ArtifactLayerFluidRuntimeState.cppm:43-151`）:

- `LayerSmokeRuntimeSettings`（`Artifact/include/Layer/ArtifactLayerFluidRuntimeState.ixx:66-76`）は 9 フィールドのみ: `gridWidth=128` / `gridHeight=128` / `viscosity=0.00001f` / `diffusion=0.00001f` / `buoyancy=0.05f` / `vorticity=0.1f` / `solverIterations=20` / `emitterCount=16` / `emitterSpeed=120.0f`。**emitter の位置・範囲・温度・色は存在しない。**
- 注入は毎ステップ 1 点のみ (`.cppm:79-90`): `centerX = gridWidth/2`、`centerY = max(1, gridHeight-3)` に `addDensity(centerX, centerY, ...)` と `addVelocity(centerX, centerY, sin(phase)*0.85f, -injectVelocity)`。`phase = (fluidLastFrame_ + step) * 0.07f` で正弦変動する **プロシージャルな渦**を注入している。
- **emitterCount / emitterSpeed は particle emitter component から供給されている**（`Artifact/src/Layer/ArtifactAbstractLayerFractureRuntime.cppm:737-738` → `impl_->particleEmitterCount_` / `impl_->particleEmitterSpeed_`）。fluid の設定ではない。`component.fluid.emitterCount` のような Property は存在しない。
- 描画は `getDensity` を 28x28 に間引いた最大 784 粒子を `drawParticles`（`Additive` + `VelocityAligned`）で additive 描画（`.cppm:96-150`）。色は ** 青固定 `r=0.42, g=0.72, b=1.0`**（`.cppm:118-120`）。

### マウス / ビューポート操作は存在しない

- `ToolType`（`Artifact/include/Tool/ArtifactToolManager.ixx:12-43`）30 値に流体関連のものは 1 つも 無い（`Selection` … `RigWeight`）。
- `addDensity` / `addVelocity` の非テスト呼び出しは 2 つのみ: プロシージャル emitter（上記）と `ArtifactCore/include/Particle/ParticleSystem.ixx:422-429`（audio reactivity、別 system）。マウス・ギズモ・timeline からの注入は無い。
- `ArtifactCore/src/AI/FluidMaskingDescriptions.cppm` は**AI 向けの記述メタデータ**であり、実行経路ではない。かつ実際と食い違っている（`addTemperature` というメソッドは存在しない、`addVelocity` の引数型が `QVector2D` と書かれているが実体は `(int,int,float,float)`、`FluidColorMode` / `dissipation` という型・パラメータは存在しない）。これは本 milestone の対象外だが、別途訂正が要る。

### fluid Property の UI 抜け（副作用）

`component.fluid.*` の setter は 27 個ある（`Artifact/src/Layer/ArtifactAbstractLayerPhysicsRouting.cppm:514-762`）が、`getComponentPropertyGroups` に登録されているのは `component.fluid.enabled` **1 個だけ**（`Artifact/src/Layer/ArtifactAbstractLayerPropertyGroups.cppm:1098`）。smoke の表示は固定色・固定 emitter なので現状致命的ではないが、本 milestone で emitter 設定を追加する際もこの抜けを回避する。

## 範囲

1. emitter の位置・範囲・数を Property として公開する
2. emitter の色（スモーク色）を Property として公開する（固定青の撤廃）
3. ビューポートでのマウス／ドラッグによる流体のかき混ぜ（stirring）を追加する
4. 上記の Property・.tool を既存 Artist ワークフローへ接続する

## 非対象

- `FluidVisualizer` の再利用（現状孤立。CPU `float4*` バッファ API で Diligent の command buffer に対応しないため）
- GPU 流体計算
- `FluidSolver2D` のアルゴリズム変更（安定性が確認済みのため触らない）
- pyro（燃焼）("/{docs/planned/MILESTONE_PYRO_DOMAIN_LAYER_INTEGRATION_2026-09-27.md}" で別途扱う）
- 液体（`LiquidSolver2D`）側の emitter 変更（同ファイル内に存在するが本 milestone の対象外）

## フェーズ

### Phase 0 — 既存パスの確認と方針確定（2026-09-27、実コード照合済み）

**結論: 新しい `ToolType` は作らず、既存 `Brush` 系に「fluid 入力モード」を加える。Undo は行わない。**

確定できた事実（実コード照合）:

- **入力の受け渡しは既存の 2 箇所に集約されている。**
  - press: `ArtifactCompositionRenderController.cppm:26452-26509`。ガード `activeTool == Brush || RotoBrush || Eraser`、座標変換 `impl_->renderer_->viewportToCanvas(...)`（`:26490-26492`）、`brushTool->mousePressEvent(selectedLayer, {canvasPos.x, canvasPos.y})`（`:26502-26503`）。
  - move: `:29255-29274`。同じく `viewportToCanvas`（`:29261-29263`）→ `brushTool->mouseMoveEvent(layer, ...)`（`:29269`）。
  - 両経路とも `markRenderDirty()` を呼ぶため、描画再評価の既存経路も揃っている。
- `ArtifactBrushTool` には既に `setRotoInputMode(bool)` / `setEraserMode(bool)` という**モード切り替え API がある**（`.cppm:26500-26501` で使用）。fluid もこれと同じ形(mode flag)で追加でき、座標と press/release lifecycle は再実装不要。
- **`ArtifactBrushTool::mousePressEvent` は既に `dynamic_cast<ArtifactPaintLayer*>` で型を判定し、fluid を通さない**（`ArtifactBrushTool.cppm:27-28`）。fluid 用の鉤を上書きするか、mode flag で `paintLayer` 不要時の判定を通す必要がある。
- **`ArtifactAbstractLayer` に fluid コンポーネントの有効状態を取得する公開 API が無い。** `fluidComponentEnabled_` は `ArtifactAbstractLayer::Impl` の private メンバで、`boolFromHost`（`ArtifactAbstractLayer.cppm:420-430`）が内部で同期するだけで、layer 側から外から読める接口が無い。`findByType`（`ArtifactLayerComponentSystem.ixx:207-208`）は `LayerComponentHost` のメンバで、外から `ArtifactAbstractLayer` の descriptor を直接読めない。
  - したがって Phase 3 で fluid 有効判定をするには、**`ArtifactAbstractLayer` に fluid 有効状態の getter を追加する必要がある**（新規 signal ではなく単純な読み取り専用の関数なので既存方針と矛盾しない）。
- Undo について: `applyStroke` の経路は `recordUndo` / `undoLastStroke` を持ちます（`ArtifactBrushTool.cppm:31, 65, 187`）。fluid の solver バッファは毎フレーム再計算されるため意味がなく、**Phase 3 では Undo に触れない**方針を確定。

方針:

- 新しい `ToolType` 列挙子を追加せず、`Brush` / `RotoBrush` / `Eraser` の variants として `setFluidInputMode(bool)` を `ArtifactBrushTool` に足す。既存の `setRotoInputMode` / `setEraserMode` と対称。
- `ArtifactAbstractLayer` に fluid 有効 + mode を読み取る getter を追加し、ツール側から fluid 対象か判定する。
- fluid 入力モードのときは `applyStroke` を呼ばず、座標と速さだけを内部に保持して、fluid state への注入は `renderLayerSmokeRuntime` 側の毎フレーム step で行う（次 Phase の設計要点）。

受入:

- [x] 実装方針（新規 ToolType か既存拡張か）が文書に残されている — **既存 Brush への mode flag 追加で確定**
- [x] Undo を適用しないことが明示され、既存 Undo 履歴を壊さない方針が文書化されている — **本項に明記済み**

### Phase 1 — emitter の Property 化

- `LayerSmokeRuntimeSettings` に emitter 位置（正規化 x/y または edge 基準）、範囲／半径、数を追加する。
- `component.fluid.emitter*` 系列の setter を `ArtifactAbstractLayerPhysicsRouting.cppm` に追加する。
- **重要**: この Phase で追加する Property は `getComponentPropertyGroups` に **必ず登録する**（既存 fluid で抜けている傾向を避ける）。
- `emitterCount` / `emitterSpeed` が `particleEmitterCount_` / `particleEmitterSpeed_` から供給されている現行の混在を解消する。fluid 側で独立した Property を持つかどうかは Phase 0 の判断による。

受入:

- [ ] emitter の位置・数・速度が Inspector から変更でき、その変更がプレビューに即反映される
- [ ] `component.fluid.*` の Property がすべて Inspector から到達可能である

### Phase 2 — 色 / 外観の Property 化

- スモークの固定青（`r=0.42, g=0.72, b=1.0`, `.cppm:118-120`）を Property に置き換える。
- `FluidColorPicker` 等の既存承認済み picker を使う。**`QColorDialog` は新規使用しない。**
- color は `FloatColor` として扱い、`ParticleVertex` の rgba に渡す。

受入:

- [ ] smoke 色を Inspector から変更でき、プレビューへ反映される
- [ ] 新規 `QColorDialog` を使用していない

### Phase 3 — ビューポートかき混ぜ

- Phase 0 で確定した入力経路から `(layer, canvasPos, press/move/release)` を取得する。
- drag 中の間、`FluidSolver2D::addDensity` / `addVelocity` を押下位置へ注入する。速度はドラッグ方向と速さから決める。
- **注意**: 流体ソルバは毎フレーム `update()` される。**mousedown 時に 1 回だけ注入しても次フレームには消える。** したがって：
  - 注入は **毎フレーム（`renderLayerSmokeRuntime` 内の step ループ内）** で行う。つまり「現在のドラッグ状態」を solver に引き渡す **継続的な注入** が必要。
  - このため、ツール側は「押下中か」「前フレームの位置」「速度」といった **一時状態を保持し**、`renderLayerSmokeRuntime` が一フレームごとにそれを読み取って注入する形とする。Phase 0 の調査で、press/move の入力は既に `ArtifactBrushTool` に集約されていることが分かったため、この一時状態も `ArtifactBrushTool` のメンバに持たせるのが自然である。
  - Phase 0 の調査で **`ArtifactAbstractLayer` に fluid 有効状態の公開 getter が無い**ことが判明した。fluid 対象レイヤーの判定には getter の追加（読み取り専用関数）が必要で、これは新規 signal ではないため既存方針と矛盾しない。
- 既存 `markRenderDirty()` 相当の無効化も必要。press/move ハンドラは既に `markRenderDirty()` を呼んでいるため、追加の無効化は原則不要。

受入:

- [ ] ドラッグ中は継続的に注入され、1 フレームだけの一時注入に louれ ない
- [ ] ドラッグを離すと注入が止まる
- [ ] renderer がない（オフラインレンダー中）でも state が壊れない

### Phase 4 — preview / render queue parity とテスト

- 通常再生と checkpoint 経路が同じ注入を受けることを確認する。
- 注入が deterministic であることをテストで固定する（同じ初期状態・同じドラッグ列 → 同じ結果）。

受入:

- [ ] preview とレンダーキュー出力が一致する
- [ ] 決定性テストが green

## 完了条件

- smoke の emitter 位置・数・速度・色を Inspector から制御できる
- ビューポートでドラッグすると流体がかき混ぜられる
- Undo 履歴が壊れない
- preview / レンダーキュー parity が確認できる
- 上記いずれもビルド・実機確認済みである

## 制約・リスク

- 注入を「毎フレーム drag 状態を読み取る」形にしないと、solver の毎フレーム再計算により効果が消える。ツールとレイヤー間の一時状態の受け渡しが本 milestone の設計上の要点である。
- `LayerFluidRuntimeState` は `Impl` が `std::unique_ptr` で所有している（`Artifact/src/Layer/ArtifactAbstractLayerImpl.cppm:296`）。既存の checkpoint / invalidation 経路を再利用し、平行な状態保持を作らない。
- マウス入力と simulation step は同じスレッドである可能性が高く、re-entrancy に注意する。
- 新規 public signal／slot は追加しない。
- ビルド・テスト・CMake はユーザーの明示指示があるまで実行しない。

## 実装上の注意

- `#include` は global module fragment に置く。purview へ追加しない。
- `NamedVector` 等の独自コンテナを優先する。
- ホットパス（毎フレーム step）での非ゼロ確保を避ける。注入用の working buffer は事前確保済みを使う。
- 色変換は `FloatColor` を経由して暗黙の `QImage` 化を避ける。

## 関連

- `docs/planned/MILESTONE_VFX_PARTICLE_FLUID_2026-03-30.md`（SUPERSEDED、本文書の前身）
- `docs/planned/MILESTONE_PYRO_DOMAIN_LAYER_INTEGRATION_2026-09-27.md`
- `docs/technical/HOT_PATH_RULES.md`
- `docs/WIDGET_MAP.md`
