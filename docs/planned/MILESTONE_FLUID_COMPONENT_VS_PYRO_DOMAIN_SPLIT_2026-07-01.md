# Fluid Component vs Pyro Domain Split (2026-07-01)

**最終更新:** 2026-08-30

## 現行コード監査 (2026-08-15)

`FluidSolver2D` は `PhysicsSystem` の layer／component 寄りの軽量 preview 経路にあり、LOD による解像度・反復数制御も確認できます。`PyroSimulation` は `ArtifactCore` の独立 domain として、3D field、fixed timestep、source／collider、checkpoint、seek、`PyroFrameSnapshot` を持ちます。この責務分離は現行コードと整合します。

ただし、Pyro の GPU backend は enum／契約上の存在に留まり、通常の layer emitter／collider、renderer extraction、cache／bake／queue render への統合は確認できません。Fluid の particle／clone influence への本番接続と、両経路の runtime parity も未検証です。

`fluid` と `pyro` を同じ「流体」という言葉でまとめず、
今の `LayerComponentPhase` / generator / field / modifier / simulation contract に沿って
役割を分離するための方針。

## Decision

### 1. `fluid` は layer component に入れる

`artifact.component.fluid` は、レイヤーや instance 群に対して使う
軽量・局所・2D 寄りの流体シミュレーション component とする。

想定責務:

- 2D grid ベースの velocity / density preview
- particle / clone / layer distortion への influence source
- editor 中の軽量 preview fallback
- small-domain smoke / ink / vortex / dissolve の土台

この系統は、既存 `FluidSolver2D` と相性が良い。

### 2. `pyro` は独立 simulation domain にする

`pyro` は `artifact.component.pyro` に即押し込まず、
まず `ArtifactCore` 側の independent volume simulation として扱う。

想定責務:

- voxel-based gas simulation
- density / temperature / fuel / velocity の持続状態
- deterministic fixed timestep
- frame cache / bake / snapshot
- renderer へ immutable volume snapshot を渡す

これは `ArtifactCore/docs/MILESTONE_PYRO_VOLUME_SIMULATION_CORE_2026-06-27.md`
の責務と一致する。

## Why They Must Be Separate

### Fluid component

- 小さく、軽く、interactive でよい
- layer や instance set に近い
- preview fallback と相性が良い
- crowd / particle / clone modifier の influence に再利用しやすい

### Pyro domain

- 重く、状態を持ち、composition/world owner が必要
- renderer / cache / bake と強く結びつく
- particle に埋め込むより volume snapshot の方が自然
- deterministic replay が重要

このため、`fluid = component-local or layer-facing`,
`pyro = composition simulation world-facing` を基本ルールにする。

## Canonical Placement

### `artifact.component.fluid`

- phase: `Dynamics`
- scope: `Composition`
- owner: layer descriptor
- runtime:
  - immediate preview evaluator
  - 将来的には composition simulation session に参加可能

### `pyro`

- phase: layer component ではなく、まず simulation domain
- scope: composition/world
- owner:
  - `PyroSimulation`
  - `PyroDomain`
  - `PyroFrameSnapshot`
- layer 側は将来的に
  - emitter source
  - collider source
  - render consumer
  のどれかとして接続する

## Integration Contract

### Fluid -> layer/component path

`fluid` は次の経路に接続してよい。

- particle emitter の velocity source
- clone modifier / field の influence source
- distortion / advection effect の preview source
- small smoke / ink preview

### Pyro -> simulation path

`pyro` は次の経路に接続する。

- layer / generator / particle を emitter source として受ける
- collider component / soft-body / fracture result を collider source として受ける
- render extraction で volume snapshot を renderer に渡す
- cache / bake / queue render が正規 consumer になる

## Guardrails

- `pyro` を `FluidSolver2D` の単純拡張として扱わない
- `fluid` に fuel / temperature / voxel cache / bake 責務を押し込まない
- `pyro` を clone/layout/crowd と同じ lightweight preview pathに混ぜない
- `fluid` と `pyro` の両方が `Dynamics` に見えても、owner を混同しない

## Near-Term Execution Order

1. `artifact.component.fluid` を layer component として維持
2. `FluidSolver2D` preview bridge を layer-side に追加
3. `fluid` を particle / clone influence sourceへ接続
4. `pyro` は `ArtifactCore` 側の domain / cache / snapshot を先に完成
5. その後、layer は `pyro emitter` / `pyro collider` / `pyro consumer` として接続

## Summary

- `fluid` は component
- `pyro` は domain simulation
- 両者は related だが同一レイヤーに押し込まない
- layer component pipeline と authoritative simulation path の分離を守る

## 2026-07-25 実装監査

- `artifact.component.fluid` と `FluidSolver2D` は layer/component 側に存在し、軽量な 2D density／velocity preview と particle field 経路に分離されている。
- `PyroDomain`／`PyroSimulation` は `ArtifactCore` の独立 simulation module として存在し、density／temperature／fuel／velocity／pressure、fixed timestep、source／collider、checkpoint／snapshot／seek を持つ。
- したがって fluid を pyro の単純拡張にせず、component-local と domain-owned を分ける基本方針はコード構造と一致する。
- 一方、layer の pyro emitter／collider／render consumer 接続、immutable volume snapshot の renderer extraction、cache／bake／queue render の正規統合は未確認である。
- 本文書は方針・配置の監査としては整合しているが、統合実装と runtime parity は未完了とする。

## 2026-08-29 — Liquid Container 初期スライス

- `Physics.Fluid` に、既存の Stable Fluids を変更せず独立した CPU 粒子液体 `LiquidSolver2D` を追加した。
- 正規化された矩形容器の左右・底を壁、上辺を開口として扱い、重力、粒子間距離制約、粘性、表面張力、substeps、非有限値ガードを実装した。
- `artifact.component.fluid` に `mode=0 Smoke/Ink, 1 Liquid Container` と、fill amount、gravity、surface tension、particle spacing、substeps の設定・Property・descriptor・JSON 保存／復元を接続した。
- レイヤー変換の逆変換からローカル重力方向を求めるため、容器を回転しても水は画面下方向へ落ちる。表示は既存 `ParticleRenderer` の Alpha／ScreenAligned preview を使用する。
- random access は初期状態から最大600フレームを決定的に再計算する暫定経路。長尺のcheckpoint／revision invalidation、任意collision path、容器から離れた液体のworld-space所有、高品質な連続水面GPU描画は未実装。
- ビルド／ランタイム検証はリポジトリ方針により未実施。

## 2026-08-30 — Liquid checkpoint／長尺seek

- `LiquidSolver2D` に検証付き `restore(LiquidSnapshot2D)` を追加し、authoritative simulation stateを描画cacheとは独立して復元できるようにした。
- レイヤーはフレーム0と30フレーム間隔のcheckpointを最大256個保持する。random seekでは目標以前の直近checkpointを復元し、残りだけを再計算するため、従来の600フレーム上限を撤去した。
- replay中は各履歴フレームの2D global transformを逆変換し、画面下方向をcontainer-local gravityへ変換する。回転アニメーションを現在フレームの重力だけで再計算しない。
- Fluid/Liquid設定、transform property、composition revision、frame rateの変更でcheckpointを破棄する。親レイヤーやcomposition側の変更もcomposition revision経由で無効化する。
- checkpointはCPUメモリ上の編集preview cacheであり、JSONやproject fileには保存しない。
- ビルド／runtime seek一致確認は未実施。

## 2026-08-30 — 2D Particleとの共通snapshot境界

- 通常の2D Particleは既存 `Artifact.Generator.Particle` を正規経路とする。既に emitter単位の固定step、固定seed、フレーム再計算、`ParticleRenderData` captureを持つため、Liquid用に別Particle Systemを重複追加しない。
- Liquidはauthoritativeな `LiquidSnapshot2D` を取得し、Artifact側の明示変換 `makeLiquid2DRenderData()` で共通 `Graphics.ParticleData::ParticleRenderData` へ変換して既存Diligent ParticleRendererへ渡す。
- `ArtifactParticleLayer` のSimulation面に Deterministic、Random Seed、Fixed Time Step、Max Substeps、Self Collision、Collision Radius／Responseを露出した。
- 上記simulation設定は emitter JSONへ保存／復元し、旧JSONでは既存defaultを維持する。
- 3D Particle経路、3D billboard、depth pipelineには変更を加えていない。
- ビルド／ランタイム検証はリポジトリ方針により未実施。

## 2026-08-30 — Liquid spill composition-space移行

- `LiquidSolver2D::takeEscapedParticles()` を追加し、上辺を粒子半径以上越えた粒子をcontainer-local solverから決定的な順序で抽出する。
- layer側は流出フレームのglobal 2D transformでpositionとvelocityをcomposition-spaceへ変換する。以後はworld-down gravityで更新し、容器の移動・回転・scaleへ追従させない。
- 流出粒子は通常Particleと同じ `ParticleRenderData` に追加し、既存Alpha／ScreenAligned Diligent描画へまとめて送る。
- layer checkpointはcontainer `LiquidSnapshot2D` とcomposition-space spill状態を一組で保存／復元する。random seek後も流出位置を再現する。
- 非有限値または安全範囲外へ出たspillを除去し、最大100,000粒子で上限を設ける。
- 現段階のspillは自由落下のみで、他レイヤー／床とのcomposition-space collision、連続水面化、屈折は未実装。
- ビルド／runtime検証は未実施。

## 2026-08-30 — Spillと既存2D Collisionの接続

- composition内で `component.collision.enabled` のレイヤーを既存順序で列挙し、流出粒子を各フレームのcollider local-spaceへ逆変換して判定する。
- Auto Bounds／Box、Circle、Polygonに対応する。Boxは最近傍面、Circleは中心放射方向、Polygonはeven-odd内外判定と最近傍辺で接触点・法線を求める。
- 接触時は粒子半径分を押し戻し、restitution 0.05と接線速度保持0.88で水向けの低反発応答を行う。
- 通常再生とcheckpoint replayは同じ `advanceLiquidFrame()` を通るため、衝突後のspill状態もcheckpointへ保存される。
- fluid owner自身のCollisionは除外し、他レイヤーの既存Collision設定だけをconsumerとして使用する。新しいsignal、global physics world、第三者コードは追加しない。
- 現段階は離散point-contactであり、高速粒子のcontinuous collision、開いたcontainer path、粒子間の流出後液体制約は未実装。
- ビルド／runtime検証は未実施。

## 2026-08-30 — Spill continuous collision

- 各spillに前フレームのcomposition-space位置を保持し、現在位置までの線分をcollider local-spaceへ変換して離散接触の前後を補うsweep判定を追加した。
- Boxは粒子半径で拡張した矩形への最初の進入、Circleは粒子半径で拡張した円との最初の交点、Polygonは最初に横切る辺を採用する。
- sweep接触も既存の押し戻し、低反発、接線速度保持へ統合し、通常再生とcheckpoint replayで同じ判定を使う。複数collider接触後は補正位置を次のsweep開始点に更新する。
- Polygonの辺に平行な近接移動は終点の半径contactで補うが、完全なswept-circle対polygonではない。流出後の粒子間液体制約は引き続き未実装。
- ビルド／runtime検証はリポジトリ方針により未実施。

## 2026-08-30 — Collision Polygonを開放容器として利用

- Liquidを持つ同一レイヤーでCollisionを有効化し、ShapeをPolygonにすると、既存 `collisionOutlineLocalPoints()` の輪郭をlayer bounds基準で正規化して `LiquidSolver2D` へ渡す。
- Polygonの最上部にある辺を開口として自動選択する。同じ高さの候補が複数ある場合は長い辺を優先し、それ以外の辺を粒子半径付きの容器壁として扱う。
- reset時はPolygon内部にある粒子だけを初期充填する。simulation中は最近傍の閉じた辺から内向き法線を求めて押し戻し、開口を越えた粒子だけを既存composition-space spillへ移管する。
- Polygonが無効、頂点不足、Collision無効、またはShapeがPolygon以外の場合は従来の上辺が開いた矩形容器へ戻る。新しいsignal、path所有、第三者コードは追加しない。
- 開口辺の手動指定、複数開口、自己交差Polygon、完全なswept-circle wall collisionは未対応。ビルド／runtime検証はリポジトリ方針により未実施。

## 2026-08-30 — Spill凝集・近傍粘性

- composition-spaceへ移管したspill状態をCore公開型 `LiquidSpillParticle2D` に統一し、近傍相互作用を `LiquidSolver2D::applySpillInteractions()` のsolver責務へ置いた。Artifact側は固定frame stepで呼び出し、描画状態は所有しない。
- 粒子最大サイズに基づくuniform gridへ決定的なindex順で登録し、周囲9セルだけを探索する。全粒子総当たりを避けつつ、同一入力・frame・設定では同じpair順を維持する。
- `Surface Tension` を弱い近距離凝集、`Viscosity` を時間刻み依存の速度均しへ対応させ、過密時には常時の短距離分離を加える。pairごとの速度差分は一旦蓄積して同時適用し、1 frameの最大補正量を粒子サイズ基準で制限する。
- 通常再生とcheckpoint replayは同じ `advanceLiquidFrame()` を通り、相互作用後のspill stateも既存checkpointへ保存される。設定・JSON項目、新しいsignal、第三者コードは追加しない。
- これは粒子表現向けの弱い凝集であり、非圧縮性のworld-space液体solveではない。ビルド／runtime負荷・見た目の検証はリポジトリ方針により未実施。

## 2026-08-30 — Density surface snapshot描画

- Coreに `LiquidSurfaceSample2D`／`LiquidSurfaceSnapshot2D` と `LiquidSolver2D::buildSurfaceSnapshot()` を追加し、container粒子とcomposition-space spillを同じ座標系のimmutable surface snapshotへ抽出する。
- 粒子kernelのdensityを疎な固定間隔nodeへ加算し、thresholdを横切るcellを中心分割した三角形として補間する。単一の巨大bounds gridを使わないため、遠くへ落ちた水滴があっても容器内水面の解像度を潰さない。
- 4,096 sampleを上限とする決定的stride LOD、最大65,536 surface cell、最大100,000 triangleを設ける。LOD時はspacingとkernel半径をstride平方根で拡大して面の被覆を維持する。
- Artifactはframe単位でCPU surface snapshotをcacheし、既存Diligent `PrimitiveRenderer2D::drawSolidTriangleLocal()` のcommand bufferへ面を先に投入する。その後、縮小・低alphaの既存ParticleRendererをdetailとして重ねる。
- D3D12／Vulkan固有コード、PSO、GPU resource、同期、QImage、QPainter、signal、JSON設定は変更しない。cacheはLiquid設定またはcomposition revision invalidationで破棄する。
- 現段階は単色alpha surfaceであり、厚み、法線、屈折、泡、境界AA、GPU density computeは未実装。ビルド／両backendのruntime visual・frame-time検証はリポジトリ方針により未実施。

## 2026-08-30 — Density contour highlight

- density thresholdを横切る各sub-triangleの2交点を `LiquidSurfaceSegment2D` としてsurface snapshotへ同時収録する。面とは別のdensity計算やsimulation stateを持たない。
- Artifactはsurface triangleの後、最大20,000本のcontour segmentを既存 `drawThickLineLocal()` で淡い水色の縁として投入し、その上へparticle detailを重ねる。
- triangleとcontourは同じframe cacheに属し、Liquid invalidation、composition revision、frame変更で一緒に更新・破棄する。GPU resource／同期／PSOは追加しないためD3D12とVulkanで共通のDiligent command経路を使う。
- 線幅は現段階では固定preview値で、曲率ベースの泡、厚み変化、内外別色、MSAA実機品質は未対応。ビルド／runtime visual・packet cost検証は未実施。

## 2026-08-30 — Velocity／exposure foam detail

- `LiquidSurfaceSample2D` にcomposition-space velocityとfoam biasを追加し、Core surface抽出時に高速かつ局所densityが低いsampleだけを `LiquidFoamPoint2D` として決定的に抽出する。
- container内部粒子はfoam bias 0.45、流出後spillは1.0とし、密集した静水は除外しながら、開口付近の跳ねと孤立した高速飛沫を優先する。最大4,096点、size／alphaは速度超過量と露出度から制限付きで求める。
- container粒子のvelocityはlayer transformの平行移動を除いたvectorとしてcomposition-spaceへ変換する。回転・scaleした容器でもfoam速度判定をworld表示と揃える。
- 通常Liquid detailを最大95,000粒子へ描画LODし、foamを加えても既存ParticleRendererの100,000頂点上限を超えない。authoritative container／spill stateとcheckpointは削減しない。
- foamは既存ParticleRendererへ淡い白青色のdetail粒子として追加する。別emitter、乱数、GPU resource、PSO、同期、signal、JSON設定は追加しない。
- density／速度閾値は固定preview値であり、寿命を持つ泡層、壁接触泡、曲率、泡テクスチャ、runtime調整UIは未対応。ビルド／D3D12・Vulkan visualと負荷検証は未実施。

## 2026-08-30 — Collision-impact foam memory

- `LiquidSpillParticle2D` に `collisionImpact` を追加し、既存Box／Circle／Polygonの共通collision responseで衝突直前の負の法線速度をimpactとして記録する。複数colliderではそのframeの最大値を保持する。
- impactは固定stepごとに `exp(-6dt)` で減衰し、非有限値・負値を0へ戻す。spill checkpointにstateごと保存されるため、通常再生とseek replayで泡の残り方を一致させる。
- `LiquidSurfaceSample2D` へimpactを渡し、速度foamとimpact foamの強い方を採用する。impact時はdensity exposureへ下限を与え、密集した水が床・壁へ当たった場合も短時間だけ泡を出せる。
- 新規emitterや乱数は使わず、既存surface snapshotの最大4,096 foam pointとParticleRenderer上限を維持する。Diligent resource／PSO／同期、signal、JSON設定は変更しない。
- 容器内部壁のimpact履歴、接触継続時間、泡の移流・寿命、材質別発泡係数は未対応。ビルド／runtime collision foam確認は未実施。

## 2026-08-30 — Container-wall impact foam

- `LiquidParticle2D` にも `collisionImpact` を追加し、矩形容器の左右・底と、開口辺を除くPolygon容器壁で衝突前の法線速度を記録する。
- impactはsolver substepごとに `exp(-6dt)` で減衰し、snapshot／restoreの検証対象へ含める。container checkpointからseek replayしても壁面foam履歴を復元できる。
- container-local impactはlayer boundsの短辺とglobal transformの最小scaleでcomposition-space速度へ変換してsurface foamへ渡す。容器のsize／rotation／scaleが変わってもspill impactと同じ単位で閾値判定する。
- 開口を通過した粒子はimpactをworld velocity scaleで変換して `LiquidSpillParticle2D` へ引き継ぐ。移管frameで壁・lip付近のfoamが不連続に消えない。
- authoritative impactはCore particle／spill stateにのみ保持し、surface cacheはimmutable consumerのままとする。Diligent resource／PSO／同期、signal、JSON設定は変更しない。
- 接触継続時間、壁材質別係数、泡の独立移流・寿命は未対応。ビルド／runtimeで矩形・凹Polygon・回転容器のimpact foam確認は未実施。

## 2026-08-30 — Density thickness／appearance controls

- `LiquidSurfaceTriangle2D` にdensity threshold超過量から求めた0〜1の `thickness` を追加する。Artifactは薄い境界を明るく透明、密集部を暗く不透明にして、単色一様alphaより水量を読み取りやすくする。
- Fluid componentへ `Surface Opacity`（既定0.64）、`Edge Opacity`（既定0.42）、`Foam Amount`（既定1.0）を追加し、Property、component descriptor、JSON保存／復元へ接続する。旧JSONは既定値を維持する。
- SurfaceとEdgeは描画時に独立適用し、0にしたlaneはcommandを生成しない。両方0なら元のparticle detailを意図的に縮小しない。Foam Amountはcontainer／spill foam biasへ掛け、変更時はsurface snapshotだけを明示破棄する。
- thicknessとappearanceはimmutable render snapshot／consumer設定であり、authoritative liquid／spill stateやcheckpointへ混ぜない。Diligent resource／PSO／同期、QImage、QPainter、signalは追加しない。
- 色は現在の青系固定preview paletteで、ユーザー色、吸収係数、背景屈折、物理的な厚み積分は未対応。ビルド／runtimeでProperty編集、JSON round-trip、D3D12・Vulkan表示確認は未実施。

## 2026-08-30 — Liquid／Foam color controls

- Fluid componentへColor Propertyの `Liquid Color`（既定RGBA 0.10, 0.50, 0.98, 1.0）と `Foam Color`（既定RGBA 0.78, 0.93, 1.0, 1.0）を追加する。
- UI境界は既存Color PropertyのQColor契約を使い、承認済みcolor picker経路へ載せる。内部描画stateはFloatColorで保持し、QColorDialog、新しいsignal／slot、QtCSSは追加しない。
- component descriptorとcomponents JSONではRGBA objectとして保存・復元し、各channelを有限な0〜1へ検証する。旧JSONでは従来の青／白青defaultを維持する。
- Liquid Colorはsurface thickness色、白寄せしたcontour、container／spill detail粒子へ一貫適用する。Foam Colorはfoam detailへ適用し、双方のalphaを既存Surface／Edge opacity、foam alpha、layer opacityへ乗算する。
- 色変更はsimulation／surface geometryを変更しないためauthoritative stateやcheckpointへ混ぜない。Diligent resource／PSO／同期は変更しない。
- linear／display color spaceの明示変換、吸収色、背景屈折は未対応。ビルド／実color picker操作／JSON round-trip／両backend表示確認は未実施。

## 2026-08-30 — Container solver近傍grid

- 容器内粒子の距離制約と近傍粘性を、全粒子総当たりから粒子間隔基準のuniform grid探索へ変更した。
- 各粒子の周囲9セルだけを候補とし、候補indexを昇順へ正規化してから処理するため、固定入力・固定stepでの決定的なpair訪問順を維持する。
- gridは距離制約の各solver iteration開始時に再構築し、補正後の位置は次iterationで反映する。ビルド、runtime負荷、見た目の比較はリポジトリ方針により未実施。

## 2026-08-30 — Opening inflow

- 初期充填量とは独立した `Inflow Rate`、`Inflow Width`、`Inflow Speed` をFluid component、descriptor、JSONへ追加した。既定rateは0で旧プロジェクトの挙動を維持する。
- Core solverは矩形上辺またはCollision Polygonの開口辺から、内側法線をprobeして最大4,096粒子／frameを決定的な横並びで追加する。総粒子数は既存上限100,000を越えない。
- 実際の追加数は開口幅と粒子間隔から求めた1列分を上限とし、既存粒子との近傍占有をuniform gridで検査する。流入口が液体で塞がれているframeは粒子を重ねず、余剰要求を破棄する。
- 小数粒子のcarryをcheckpointへ保存・復元し、固定frame stepとseek replayで同じ流入数を維持する。Diligent resource／PSO／同期、signal、3D経路は変更しない。
- 開口位置の任意オフセット、複数inlet、時間アニメーション専用UI、圧力境界は未対応。ビルド／runtimeで矩形・凹Polygonへの流入とseek一致は未確認。

## 2026-08-30 — Container wall preview

- `Container Opacity`（既定0.58）と `Container Width`（既定2.5px）をFluid component、descriptor、JSONへ追加し、液体表面とは独立して容器壁の見え方を調整できるようにした。
- 矩形containerは上辺を除く左右・底、Collision Polygon containerは自動選択された開口辺を除く全辺を描く。polygon設定と同じlocal point列を再利用し、simulation境界と表示境界のずれを避ける。
- 描画は既存Diligent `drawThickLineLocal` commandのみを使い、新しいPSO、resource、shader、同期、QImage、QPainterを追加しない。D3D12／Vulkanで共通のrenderer境界に留める。
- ガラス厚み、屈折、容器fill、材質、背面／前面のdepth分離は未対応。ビルド／両backend visual確認は未実施。

## 2026-08-30 — Spill off-canvas culling

- `Spill Cull Margin`（既定1,024px、0で無効）をFluid component、descriptor、JSONへ追加した。composition矩形を余白分拡張し、その外へ出たspill粒子だけを固定step中に除去する。
- 一律寿命は床やcollider上に溜まった水まで消すため採用しない。画面内と指定余白内の粒子は時間に関係なく保持し、液体の残留挙動を優先する。
- cullは既存の非有限値／絶対world limit検査と同じauthoritative spill更新で行うため、checkpointとseek replayでも同じframeに同じ粒子が除去される。0指定時は従来のworld limitだけを使う。
- composition外に意図的なcolliderを置く制作では余白を広げるか無効化する必要がある。ビルド／長尺runtimeの粒子数推移とseek一致は未確認。

## 2026-08-30 — Liquid Core regression coverage

- 既存 `ArtifactCorePhysicsDeterminismTest` へ、同一入力90stepのbit-exact state一致、snapshot restore後40stepのexact replay、非有限snapshotの拒否と非破壊性を追加した。
- opening inflowは、同じ未更新開口への2回目追加がoccupancy判定で0件になることと、Polygon開口から内側方向へ生成されることを固定した。spill近傍相互作用とsurface triangle／contour／foam抽出の同一入力一致も追加した。
- surface入力はposition／size／velocity／foam／impactの全fieldを有限値検証し、絶対座標1,000万、sample size100万を境界とする。極端なsize外れ値はdensity半径をgrid 8cellへ制限し、疎mapの病的な展開を防ぐ。不正sampleが空snapshotになる回帰ケースも追加した。
- foam抽出の速度早期returnをimpact評価後へ移し、粒子速度が閾値未満まで落ちたframeでも衝突履歴が残っていれば泡を生成できるよう修正した。静止＋impact sampleの回帰ケースで契約を固定する。

## 2026-08-30 — Container surface cohesion

- container solverの `Surface Tension` を、過密粒子の反発stiffnessだけでなく、rest distanceから1.55 spacingまでの近傍を弱く引き寄せるposition correctionへ接続した。
- 引力はrest distanceからの距離とsupport端までのfalloffへ比例し、pair両側へ半分ずつ適用する。support外は補正せず、Surface Tension 0では従来どおり反発範囲だけを探索する。
- 反発補正は従来の符号と強度を維持し、凝集係数は1 iterationあたり最大6%の弱い補正に抑える。少し離れた2粒子がTension 1でのみ接近するCore回帰ケースを追加した。
- これはPBF/SPHの密度制約ではなく、軽量preview liquidの表面凝集である。高粒子数での体積保持、滴形成、runtime見た目は未検証。

## 2026-08-30 — Manual Polygon opening edge

- Fluid componentへ `Opening Edge` を追加し、-1では従来どおり最上辺（同高なら最長辺）を自動選択、0以上ではCollision Polygonの辺indexを手動指定する。範囲外indexは末尾辺へclampする。
- 選択edgeはCore container境界、opening inflow、spill移管、container wall previewで共有し、横口や底口でもsimulationと表示が一致する。矩形containerでは設定を無視して上辺を開口に保つ。
- descriptor、JSON、Propertyへ接続し、変更時はcheckpointを含むliquid simulationを明示無効化する。旧JSONは-1で従来の自動選択を維持する。
- Core回帰ケースで右辺を開口に指定し、inflow速度が左向きになること、右外の粒子だけがescapeし上外の粒子は残ることを固定した。UIでの辺番号overlayは未対応、テスト実行も未実施。

## 2026-08-30 — Inflow position

- `Inflow Position`（既定0.5）をFluid component、descriptor、JSONへ追加し、開口辺の始点0〜終点1の範囲で流入口中心を移動できるようにした。
- Coreはpositionを0〜1へ制限し、粒子間隔1.5個分の端余白へさらにclampする。width配置、内向き法線、Polygon内判定、occupancy判定は移動後のsource centerを基準にする。
- 各laneは開口segment両端から最低1 spacingを残す範囲へ制限する。positionを端、widthを1にした矩形でも生成粒子が左右壁の外へ出ない回帰ケースを追加した。
- width／speed／positionの非有限入力は生成前に拒否する。0.2と0.8でsource位置が移ること、NaN controlがstateを変更しないことをCore回帰ケースへ追加した。
- 複数inletと辺上のvisual handleは未対応。ビルド／Property操作／矩形・横口Polygon runtime確認は未実施。

## 2026-08-30 — 2D Particle default／hot-path cleanup

- 新規 `ArtifactParticleLayer` の既定を `is3D=false` へ変更し、2D transform／composition-space描画を標準とした。既存JSONで `is3D=true` を保存しているParticle layerは基底復元経路で3D状態を維持する。
- 通常Particle GPU分岐で欠落していた `captureRenderData()` を分岐内へ戻し、未定義 `sourceData` 参照を解消した。既存Diligent ParticleRendererとLOD／transform経路は変更しない。
- 通常Particle／ParticleDebugのdraw、transform、fallbackに残っていた毎frame／particle0の `qInfo` と空particle `qWarning` を除去した。ログI/Oをsimulation／preview hot pathから外し、液体との同時表示時も不要な出力を発生させない。
- fallbackのQImage／QPainter既存経路は変更せず、GPU利用時は従来どおりDiligentを優先する。ビルド／新規2D layer作成／旧3D JSON復元／同時表示負荷は未確認。

## 2026-08-30 — 2D Particle vector transform

- Particle GPU描画のworld-space変換でpositionだけでなく、translationを除いた2D線形transformを `vx/vy` へ適用する。回転・scale・反転を含むレイヤーtransform後もVelocityAligned billboardの向きをposition軌跡と一致させる。
- particle rotationへQTransformのx軸角度をdegreeで加算し、ScreenAligned等のsprite自転もレイヤー回転へ追従させる。非有限transformは既存identity fallback、結果velocity／rotationは既存安全範囲へ制限する。
- invalid stretchの速度補完はsource-local速度ではなく変換後world速度を使う。Diligent ParticleRenderData contract、PSO、resource、同期、backend固有コードは変更しない。
- shear／非一様scaleでのsprite角度はx軸基準であり、完全な楕円変形は行わない。ビルド／90度回転・mirror・VelocityAligned visual確認は未実施。
- 既存test targetへ追記したため新しいCMake target／module登録は追加しない。リポジトリ方針によりbuild／test実行は未実施。

## 2026-08-30 — 2D／3D Particle layer identity split

- 既存`LayerType::Particle`（ID 15）を2D専用として維持し、`LayerType::Particle3D`（ID 31）と`ArtifactParticle3DLayer`を追加した。simulationと既存Diligent ParticleRendererは共有するが、作成・型識別・保存を分離する。
- Layerメニューは「2D パーティクル」と「3D パーティクル」を別項目として生成する。2Dはcomposition-space、3Dはcamera／depth経路を選択し、通常Propertyの`layer.is3D`トグルはParticle系から除外した。
- 旧`Particle` JSONで`is3D=true`だったレイヤーはfactory読込時に3D専用種別へ移行する。既存数値IDは並べ替えず、2Dプロジェクトの互換性を維持する。
- Fluid componentはParticle layerへ統合せず独立を維持し、描画snapshot／ParticleRendererだけを共有する。
- 新しいGPU resource／PSO／shader／同期、backend固有コード、signal／slot、QImage／QPainter経路は追加していない。build／runtime確認は未実施。
