# ArtifactAbstractLayer 段階分割計画

**最終更新:** 2026-09-22

## 2026-09-22 追加分割

- `Artifact.Layer.MaskMatteState` を追加し、Mask / Matte の所有、変更履歴、`NamedVector` と公開 API の `std::vector` 境界変換を巨大な AbstractLayer 実装 TU から分離した。
- `Artifact.Layer.ThumbnailSupport` を追加し、サムネイル用 `QPainter` 描画を独立 TU へ移した。
- `ArtifactAbstractLayer.cppm` は上記状態への委譲とキャッシュ管理だけを残す。

## 目的

`ArtifactAbstractLayer` の基底責務から物理、コンポーネント実行、プロパティ構築、
シリアライズを段階的に分離する。MSVC の IFC import 時に巨大な標準ライブラリ
テンプレートを再展開させず、変更時の再コンパイル範囲も縮小する。

## Phase 1: Physics.System 依存遮断

- `Artifact.Layer.PhysicsBridge` のinterfaceは `Physics.System`をimportしない。
- Bridge実装だけが `Physics.System`を所有する。
- `ArtifactAbstractLayer.cppm` は個別solver型と小さなLOD値だけを利用する。
- `GpuBoidConstants`レジストリを `std::map` から `NamedVector`へ移行する。
- レイヤー内のliquid checkpointを順序付き `NamedVector`へ移行し、最大256件を
  construction時にreserveする。

2026-09-21時点でソース変更済み。ビルド・実機確認は未実施。

## Phase 2: PhysicsSystemの連想コンテナ移行

`PhysicsSystem`のsolver registryとframe snapshot cacheを、`NamedVector`基盤の
`PhysicsOrderedRegistry`へ移行した。frame snapshotは時系列順序が意味を持つため、
hash mapへ機械置換せずキー順を維持する。

対象はfluid / soft body / cloth / rigid world、composition rigid world、frame snapshot、
optional pyro registry。2026-09-21時点でソース変更済み、ビルド未確認。

各Entryには用途別 `ContainerName` を設定する。ループ順序、frame trim、snapshot restoreの
意味を維持し、各solverの最初のsnapshot capture時にtimelineの最大キャッシュ件数をreserveする。
通常のframe captureでの内側cacheの容量拡張を避ける。

`Artifact.Layer.RuntimeSupport` の `MotionTrailRingBuffer` も `NamedVector` 化した。
モーション・トレイルの長さ変更時だけ容量を確保し、通常frameでは既存の固定容量を巡回する。

衝突アウトラインの公開 API は `NamedVector<QPointF>` に統一した。Shape Layer の生成、
liquid container、soft body、rigid body、clone／overlay の消費側まで追随済みである。
`LiquidSolver2D` の container polygon API と保持状態も `NamedVector` 化し、レイヤーから液体solverまで
同じコンテナで渡す。`Physics2D` の polygon入力は未移行で、変換をそのsolver境界に限定する。
`Physics.Fluid` interface は宣言に不要だった iostream／filesystem／map／regex 等の標準ヘッダを外し、
IFC に露出する標準ライブラリ面を最小化した。

`ArtifactAbstractLayer::getVariants()` も `NamedVector<LayerVariant*>` を返すよう統一した。
Timeline と Undo の利用側は要素走査／件数確認だけなので、標準コンテナへの互換変換を残していない。

Cloner の generator／field／modifier descriptor と transform 名の問い合わせも `NamedVector` 化した。
Inspector、Text Layer、Clone support はすべて値走査だけであり、呼び出し側の追加変換は不要である。
descriptor snapshot の復元用一時配列も同じコンテナへ揃え、復元後の既存 host への移送だけを残した。

`LayerComponentHost` の所有配列、全件取得、phase／scope filtering を `NamedVector` 化した。
基底レイヤーはこの Host API のみを通るため、Component descriptor の公開経路から `std::vector` を除去した。
Component validation の戻り値も `NamedVector` へ揃え、明示型で保持する Inspector state の空値も同じ型にした。

基底レイヤーが所有する cloner transform stack を `NamedVector` 化した。JSON load/save、snapshot restore、
add/remove/duplicate/reorder の編集経路も `NamedVector` の操作へ揃え、hot path の描画側は既存の順序走査のまま維持した。

liquid spill particle の実行時配列と checkpoint 内の保存配列、`LiquidSolver2D::applySpillInteractions()` の
入力を `NamedVector` 化した。frame 更新時の cull は erase-remove idiom ではなく `removeIf()` を使う。
`takeEscapedParticles()` の戻り値も `NamedVector` に揃え、solver から layer-local spill 状態へ渡る経路で
標準コンテナを露出しないようにした。

その後、`LiquidSnapshot2D` と `LiquidSurfaceSnapshot2D` の DTO 所有配列、および
`buildSurfaceSnapshot()` の入力を `NamedVector` に移した。solver の永続 particle storage、snapshot 作成、
restore、escaped partition も `NamedVector` に統一した。さらに `FluidSolver2D` の連続数値 grid と
`FluidSnapshot2D` の保存状態も命名済み `NamedVector<float>` に揃え、interface から `std::vector` を除いた。
Core の determinism test も同じ API を用いるよう更新済みである。
surface extraction 内の有限値確認を通過した sample 作業配列も `NamedVector` とし、入力から
surface DTO まで同じコンテナ表現を保つ。

fluid preview と component emitter particle の layer-local 配列、component runtime snapshot 内の粒子配列を
`NamedVector` 化した。renderer の `ParticleRenderData` は広い描画境界なので、描画直前の明示コピー先として維持する。

### 標準コンテナの段階的な脱却基準

- layer / component / physics bridge の所有配列と問い合わせ結果は `NamedVector` を正とし、標準コンテナを
  公開境界へ新規に広げない。
- renderer、外部ライブラリ、または既存 solver の狭い互換境界だけは明示変換を許可する。変換理由と位置を
  境界側のコメントで残す。
- Fluid の grid は連続メモリ状態として `NamedVector<float>` を使用する。局所的な空間探索・surface extraction の
  `std::map` / `std::vector` は solver 実装に閉じ、公開 DTO や長期所有状態へ新規に露出させない。
- effect / modifier / mask / matte / property group の既存 `std::vector` API は override と多数の呼び出し元を
  持つ。個別に全 call site を確認してから移行し、互換目的の二重 API は増やさない。

## Phase 3: Component Runtime State

fracture、fluid、particle、crowd、collision、joint、cloner、layoutの状態を
`LayerComponentRuntimeState`へ移す。基底レイヤーは明示所有する生ポインタだけを保持し、
生成・破棄はコールドパスで行う。

## Phase 4: Property Router / Serialization

- `setLayerPropertyValue()` を prefix 単位のadapterへ分割する。
- component property group構築を専用presentationへ移す。
- `toJson()` / `fromJson()` / runtime snapshotをserialization bridgeへ移す。
- 保存形式と既存の公開APIは移行中も維持する。

## Phase 5: 実装単位の分割

内部状態への直接アクセスが減った後で、timing、transform/bounds、mask/matte、effect、
serializationを別実装単位へ移す。巨大な `Impl` 定義を複数unitへ共有するためだけの
private module partitionは導入しない。

2026-09-21時点で、`GuideDefinition` / `GuideBinding` / `GuideSet` の JSON、検索、優先度整列を
`ArtifactAbstractLayerGuides.cppm` へ移した。この unit は `Impl` を共有せず、
`Artifact.Layer.Abstract` の公開型のみを利用する。同ファイルは `ArtifactSources.cmake` に source-only
module implementation として登録済みである。

`LayerBounds` と `ArtifactAbstractLayer::contentBounds()` の effect ROI 集約、bounds 問い合わせ、概要文字列も
`ArtifactAbstractLayerBounds.cppm` へ移した。`visualLocalBounds()` は cloner runtime state を直接読むため、
この段階では本体に残している。

Modulation router の JSON serialize / restore helper は `ArtifactAbstractLayerModulation.cppm` へ移した。
JSON 復元の一時 source 配列は `NamedVector` とし、`ModulationRouter::restoreSources()` が要求する
既存の `std::vector` への変換だけをその境界に限定している。

Composition field sample、composition size、responsive layout 制約、parent auto-layout offset は
`ArtifactAbstractLayerLayout.cppm` へ移した。これらは public layer / composition / property API のみを
利用し、`Impl` 共有を導入していない。

Fracture field 力・wind・floor / composition bounds 衝突と、runtime fragment dataset 構築は
`ArtifactAbstractLayerFractureSupport.cppm` へ移した。ここも public state と property API のみを使い、
`drawFractureOverlay()` が所有する runtime state 更新は本体に残している。

SoftBody / Cloth3D の solver 有無問い合わせと deformation mesh 生成は
`ArtifactAbstractLayerPhysicsSupport.cppm` へ移した。physics registry は既存の
`Artifact.Layer.PhysicsBridge` を経由し、基底 layer から `Physics.System` を再導入していない。

LayerVariant の内部所有配列も `NamedVector<std::unique_ptr<LayerVariant>>` に移した。抽出は
move 後に `removeAt()`、挿入は index ベースの `insert()` を使い、既存の active variant 補正を維持する。

Layer matte reference の内部所有配列も `NamedVector` 化した。既存の `std::vector` 公開 API は保持し、
`matteReferences()` / `setMatteReferences()` の境界だけで明示変換する。

Layer mask の内部配列も `NamedVector` 化した。削除と並べ替えは iterator erase ではなく
`removeAt()` / index `insert()` を使い、mask revision の更新条件を維持する。

Effect stack の内部所有配列も `NamedVector` 化した。重複 ID 判定の順序を維持しつつ、削除は
`removeIf()` に置換した。既存の `getEffects()` 公開 API は境界で `std::vector` へ明示変換する。
effect ID の一意化 helper は既存の `Artifact.Layer.Abstract.Utilities` へ移し、同じ module にある
`slugifyEffectId()` と責務を集約した。
modifier ID の一意化も同じ Utilities へ移し、effect / modifier の stack 固有 helper を基底実装から除いた。
Animation layer bake の frame/value sample 作業配列も `NamedVector` 化した。renderer / Physics2D の既存
`std::vector` 境界は変換点として維持する。
Fracture runtime motion state の `FractureState::shards` と layer accessor も `NamedVector` 化した。
geometry/export 側の `FractureResult::shards` は別の既存 API として維持する。

## 完了条件

- `ArtifactAbstractLayer.cppm` が `Physics.System` をimportしない。
- `ArtifactAbstractLayer.cppm` を3,500行以下へ縮小する。
- 公開interfaceから物理実装型と不要な再exportを除く。
- `PhysicsSystem` の所有レジストリに直接の `std::map` を残さない。
- startup、project load、physics enable/disable、scrub、save/reload、shutdownを確認する。

## 次の分割順序（2026-09-22 更新）

静的な行数確認では、主 implementation unit は 10,576 行まで縮小している。C1001 の発生位置を
module の大きな実装単位から遠ざけるため、次は依存が一方向で、`Impl` の private state を公開しない
次の順序を採る。

1. `ArtifactAbstractLayerFractureRuntime.cppm` — `drawFractureOverlay()`、fracture impact/reset と
   その render-only helper を移す（およそ 900 行）。既存 `ArtifactAbstractLayerFractureSupport.cppm` は
   field/collision と evaluation DTO の補助に限定する。
2. `ArtifactAbstractLayerTransform.cppm` — local/global 2D/3D transform、frame sample、matrix 変換を
   移す（およそ 700 行）。composition 参照は既存 public API 経由に留める。
3. `ArtifactAbstractLayerPersistence.cppm` — layer JSON と animation-layer snapshot の save/restore を
   移す。JSON schema を変更せず、`Impl` へのアクセスは公開 member definition の内部だけに閉じる。
4. `ArtifactAbstractLayerComponents.cppm` — component descriptor/runtime snapshot と cloner descriptor
   の操作を移す。Property Editor の公開経路と混ぜない。
5. `ArtifactAbstractLayerPropertyRouting.cppm` — property group 構築と値 routing をさらに責務別に
   分ける。これは最大の塊なので、先に各 public member の定義が一箇所だけであることを静的確認してから
   小分けにする。

### `Impl` 可視性の制約

通常の companion unit が `import Artifact.Layer.Abstract;` しただけでは、主 implementation unit にのみ
定義されている `ArtifactAbstractLayer::Impl` は不完全型である。従って `impl_` を直接参照する member
definition（time remap、source/bounds、JSON、component runtime など）を、そのまま companion unit へ移しては
ならない。この方法は module compile で失敗する。

この制約により、上記の次段階は次のいずれかを満たす塊だけを対象にする。

- `impl_` に触れず公開 layer API だけで完結する helper / algorithm を companion unit へ移す。
- `Impl` の責務を独立した所有クラスへ段階的に抽出し、主 unit はその concrete definition とライフサイクルだけを
  所有する。この抽出は public interface や private partition を安易に広げず、先に保存・undo・runtime state の
  境界を確認する。

`ArtifactAbstractLayerBasics.cppm`、`ArtifactAbstractLayerGeometryCache.cppm`、
`ArtifactAbstractLayerTimeMapping.cppm` の試行はこの制約に抵触するため登録せず撤回した。主 unit の既存挙動を
維持し、以後の分割候補には `Impl` 完全型の可視性を静的確認項目として追加する。

`ArtifactLayerTimelineSupport.cppm` はこの制約を満たす最初の分離 unit として追加した。composition transform
field の適用、effective frame rate、timeline frame/time の helper はいずれも公開 layer / composition API のみを
使う。主 unit は external linkage の明示宣言だけを持ち、匿名 namespace 内の宣言を跨がない。

`ArtifactLayerMaskPropertySupport.cppm` には mask/path の animatable property 評価を移した。評価時刻は
Timeline Support の external helper を使い、mask prefix・property API・mask DTO だけを import する。`Impl`、
renderer、JSON 保存状態には依存しない。

`ArtifactLayerTransformSupport.cppm` には public transform API だけで完結する 4x4 snapshot、parent 合成、
Qt-to-Diligent matrix 変換を移した。2.5D lens や authored transform state には触れないため `Impl` 可視性を
必要としない。

同 support unit に 2.5D render pass の投影、motion blur、depth-of-field sample 生成も移した。主 member は
`Impl` の設定値を値引数として渡すだけにし、helper は public layer API と値スナップショットだけを使う。
frame 指定の global 2D transform 合成も同 support unit へ移し、主 member は layout component の2フラグだけを
渡す。parent auto-layout offset の既存 helper は Layout unit が所有し、Transform support は外部宣言だけを持つ。

`ArtifactLayerDefaultsSupport.cppm` は base layer の既定挙動（draw LOD delegation、class/source override、
layer-kind と audio/video の false default）を所有する。いずれも `Impl` を読まず、派生 layer の override 契約を
変えない。

`ArtifactAbstractLayerPropertyPresentation.cppm` には、`Impl` を参照しない 3D transform と Mask の
property-group 構築を移した。`ArtifactAbstractLayerRuntimeSnapshot.cppm` には component runtime snapshot の
JSON serialize / deserialize を移し、保存形式と上限検証を維持した。これにより主 implementation unit から
約 400 行と、MSVC が同時に解析していた Qt JSON / property 構築処理を分離した。

`ArtifactAbstractLayerEffectPersistence.cppm` には effect の JSON 適用と editable property の復元を移した。
この経路は `Impl` に触れず、effect の生成、enabled/pipeline stage、keyframe/expression/envelope の既存形式を
そのまま維持する。

主 unit から使用のなくなった `<array>`、`<compare>`、`<random>` も除去した。残る標準ヘッダは同 unit の
直接使用だけを対象とする。

`LayerEvaluationState` の instances / intents / contacts を含む実行時 channel 全体を `NamedVector` 化した。
composition simulation の対応する entry も同じコンテナへ揃え、`cloneRenderInstancesForSimulation()` が返す既存の
render-boundary `std::vector<CloneRenderInstance>` だけを entry 作成時に `fromStdVector()` 変換する。fragment 側の
`CloneRenderInstance` 出力は引き続き `ArtifactCloneEffectSupport` でだけ `toStdVector()` を行う。
simulation layer-entry の作業配列も `NamedVector` とし、この pass 内で標準 vector を新規に保持しない。
Component dependency cycle validation の visiting / visited worklist も `NamedVector<QString>` に移した。
cycle 検出の順序は維持し、末尾除去だけ `popBack()` を使用する。
validation auto-fix の未使用 `toRemove` は除去し、disable 対象の重複除去も `NamedVector` と既存探索へ統一した。

利用箇所のなかった property-group 判定 wrapper 6件は、既存 Utilities の正規 helper を残して主 unit から
削除した。公開 API ではなく translation-unit 内の未使用 forwarding だけを対象にしている。

private module partition は `Impl` を共有するための回避策として使わない。各 companion unit は主 interface を
import し、既存の public member definition として private state に触れる方式を維持する。

### State object 抽出の設計境界

`Impl` を別 unit で共有しない代わりに、次の state object を通常 module として独立させる。主 `Impl` は各 object
を値として所有し、抽出後の helper は object への明示参照と public layer API だけを受け取る。

| State object | 現在の field cluster | 最初に移す処理 | 依存上の注意 |
| --- | --- | --- | --- |
| `LayerFluidRuntimeState` | liquid solver、checkpoint、spill、surface snapshot、preview particle、fluid frame | invalidation、checkpoint restore/store、surface snapshot build | solver は Core API のまま保持し、layer/composition collision は callback 引数にする |
| `LayerFragmentRuntimeState` | fracture state/result、fragment render 設定、motion trail、particle emitter runtime | fragment evaluation、render DTO extraction、transient reset | renderer DTO への `std::vector` 変換は render boundary に限定する |
| `LayerComponentAuthoringState` | cloner/layout/collision/joint/crowd の descriptor 値と `LayerComponentHost` | descriptor sync、host→bool sync | JSON/property routing の key と既存デフォルトを変更しない |
| `LayerTransformRuntimeState` | motion dynamics channel、transform cache、bounding box cache | dynamics update、cache key evaluation | parent layer と composition field は public API/callback で渡し、cache invalidation rule を保存する |

この順序なら、state object を導入した直後に companion unit の helper が完全型へ正規にアクセスできる。最初の対象は
`LayerFluidRuntimeState` とする。独立 solver の所有権と invalidation が既に `Impl` 内の小さなまとまりで、
JSON schema や public property API を変えずに段階移行できるためである。

#### Fluid state 抽出の受け入れ条件

- `Impl` から smoke solver、liquid solver、checkpoint、spill、surface snapshot、preview particle、frame/revision
  marker を丸ごと移す。設定値（grid size、viscosity、inflow、色など）と JSON/property routing の所有者は当面
  `Impl` に残す。
- `invalidateLiquidSimulation()` / `invalidateLiquidSurface()` は state object の member にする。既存の property
  更新箇所は同じ時点で state object の invalidation を呼ぶ。
- `drawFractureOverlay()` の liquid/smoke branch は、state object と設定値 snapshot を引数に取る helper へ分離する。
  composition collision 解決、layer bounds、renderer submit は public layer API または callback に限定する。
- checkpoint の最大256件 reserve、random seek の replay、composition revision / FPS 変更時の reset、spill cull は
  現在の順序と上限を保持する。
- 抽出後、`ArtifactAbstractLayer.cppm` に `liquidSolver_` / `fluidSolver_` / `liquidCheckpoints_` 等の直接 field を
  残さない。ビルド許可後に liquid random seek、property変更、save/reload、shutdown を確認する。

2026-09-22に `Artifact.Layer.FluidRuntimeState` を追加し、smoke / liquid solver、checkpoint、spill、
surface snapshot、preview particle、frame / revision marker と invalidation を同 state object へ移した。
`ArtifactAbstractLayer::Impl` は state object を値として所有し、設定値と JSON / property routing は従来どおり
`Impl` に残している。主 implementation unit から該当 runtime field の直接所有は除去済み。ビルドと実機確認は未実施。

## 検証状況

2026-09-22時点で、workspace 直下の `build/CMakeCache.txt` は Visual Studio 2022 / Debug を示すが、
`CMAKE_HOME_DIRECTORY` は現在の workspace ではなく `X:/Dev/ArtifactStudio` を指している。X: source と
J: source の root CMake、Artifact CMake、`ArtifactAbstractLayer.cppm` の SHA-256 はすべて不一致であり、
cache の最終更新も 2026-08-08 である。`build_j_vs` / `build_j_vs18` には CMake cache がない。
このため `build` は別 worktree 用として利用・再構成しない。ユーザーが明示的に許可した後、指定 build
directory で `Artifact` target を build し、まず module scan / IFC import、次に link、最後に startup・
project load・physics・save/reload・shutdown の順で確認する。
