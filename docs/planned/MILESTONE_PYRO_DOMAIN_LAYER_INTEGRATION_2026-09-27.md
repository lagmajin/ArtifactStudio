# M-PYRO-1: Pyro Domain のレイヤー統合（燃焼・火・煙の emitting 経路）

**最終更新:** 2026-09-27
**ステータス:** Not Started
**識別子:** M-PYRO-1
**対象:** `ArtifactCore/src/Simulation/PyroSimulation.cppm`、`Artifact/src/Layer/ArtifactAbstractLayer*.cppm`、`Artifact/include/Layer/ArtifactLayerFluidRuntimeState.ixx`
**関連:** `docs/planned/MILESTONE_FLUID_COMPONENT_VS_PYRO_DOMAIN_SPLIT_2026-07-01.md`、`docs/planned/MILESTONE_SURFACE_FX_SYSTEM_2026-07-22.md`

## 目的

`ArtifactCore` に既に存在する `PyroSimulation`（density / temperature / fuel / pressure / velocity を扱う domain-owned シミュレータ）を、実際にレイヤーから发出して描画できる状態にする。

現状 `pyro` は **Core 側の契約と CPUReference 実装だけがコンパイルされている状態で、アプリ側（`Artifact/`）から一切参照されていない**。本 milestone は「enum が存在する」を「Inspector で設定でき、ビューポートで燃え、書き出しできる」に変える。

## 現状の根拠（2026-09-27、実コード照合）

### 存在するもの

- 契約モジュール `ArtifactCore/include/Simulation/PyroSimulation.ixx`
  - `PyroBoundaryMode { Open, Closed }` — `:21`
  - `PyroBackendKind { CPUReference, GPUCompute }` — `:22`
  - `PyroFieldChannel { Density, Temperature, Fuel, Pressure, Divergence, Velocity, Color }` — `:23`
  - `PyroFieldMask { None, Density, Temperature, Fuel, Pressure, Divergence, Velocity, Color, All }` — `:27`
  - `PyroColliderType { Box, Sphere }` — `:172`
  - `PyroFieldSet` / `PyroSimulation` / `PyroFieldSnapshot` / `PyroMemoryEstimate` と checkpoint / snapshot / seek を持つ。
- 実装 `ArtifactCore/src/Simulation/PyroSimulation.cppm`
  - `applyColliders` / `applyCombustion` / `applyVorticityConfinement` / `integrateStep` / `computeDivergence` / `solvePressure` / `projectVelocity` / `storeCheckpoint` / `saveCheckpointSnapshot` / `loadCheckpointSnapshot` を実装済み。
- ビルド登録
  - `ArtifactCore/cmake/ArtifactCoreModuleReferences.cmake:370`
  - `ArtifactCore/CMakeLists.txt:6202-6213`（`ArtifactCoreSimulation` 静的ライブラリ）
- OpenVDB ブリッジ `copyOpenVDBDensityToPyro()` — `ArtifactCore/src/Simulation/OpenVDBVolumeReference.cppm:116`（呼び出し元なし）

### 存在しないもの（実コードでconfirmed）

- **`Artifact/` に `pyro` / `Pyro` / `PYRO` の出現はゼロ。** component descriptor、typeId、Property、JSON、Inspector 面、描画 consumer のいずれも存在しない。
- `PyroBackendKind::GPUCompute`（`:22`）は **実装パスが無く**、`PyroSimulation.cppm:889` の `toString` の `case` ラベルだけが唯一の参照。選択しても CPUReference と同じコードが走る。
- `PyroFieldChannel::Color` / `PyroFieldMask::Color` は宣言のみ。`PyroFieldSet` に `color_` メンバが無く、`PyroFieldSnapshot` に `color` view が無い。`PyroMemoryEstimate::colorBytes` だけが空に存在する。
- `PhysicsSystem` 側の pyro ブリッジ（`createPyroSimulation` / `getPyroSimulation` / 毎フレーム `step()`）は 5 箇所すべて `#ifdef ARTIFACT_ENABLE_PYRO` で囲まれている（`ArtifactCore/src/Physics/PhysicsSystem.cppm:19, 501, 1044, 1065, 1177`）。**`ARTIFACT_ENABLE_PYRO` を定義する CMake 設定はリポジトリに存在しない**（全 `.txt` / `.cmake` に対して grep してヒットなし）。よってこのコードはコンパイルされず、`PyroSimulation` は現状どのホストからも step されない。
- **テストがゼロ。** `tests/` に `pyro` の grep ヒットは 0 件（対比: `FluidSolver2D` は `tests/ArtifactCore/PhysicsDeterminismTest.cpp:176-193` でカバー済み）。

### 既存ドキュメントとの差異

`MILESTONE_FLUID_COMPONENT_VS_PYRO_DOMAIN_SPLIT_2026-07-01.md:9` は「Pyro の GPU backend は enum／契約上の存在に留まり、通常の layer emitter／collider、renderer extraction、cache／bake／queue render への統合は確認できません」と記載している。これは 2026-09-27 時点で**そのまま正しい**（`GPUCompute` は `toString` のみ、`ARTIFACT_ENABLE_PYRO` は未定義）。本文書はその backend 接続に手を付けず、CPUReference 経路の製品化のみを扱う。

## 範囲

1. `ARTIFACT_ENABLE_PYRO` を有効化し、`PhysicsSystem` の pyro レジストリを実際にコンパイルさせる
2. `artifact.component.pyro` component descriptor と Property・JSON 往復の追加
3. emitter（燃料注入）と collider（`Box` / `Sphere`）のレイヤー設定
4. temperature / fuel に基づく CPU 可視化（粒子化または field → 描画）
5. checkpoint / seek と preview／render queue の parity
6. `Core.Simulation.Pyro` の単体テスト

## 非対象

- `PyroBackendKind::GPUCompute` の実装（Diligent compute への pyro step 移植）
- `PyroFieldChannel::Color` の実装
- 3D ボリュームレンダリング、OpenVDB からの初期場構築
- `FluidVisualizer` の再利用（`ImageProcessing.FluidVisualizer` は現状**孤立**。`import` する箇所も `render()` を呼ぶ箇所も存在しない。CPU `float4*` バッファ API であり Diligent の command buffer に対応しないため、本 milestone では使わない）

## フェーズ

### Phase 0 — 接続可否の確定（2026-09-27、実コード照合済み）

**結論: `ARTIFACT_ENABLE_PYRO` を定義するだけでよい。追加の依存追加や構造変更は不要。**

確認できた事実:

- `PhysicsSystem.cppm` の pyro ブロックは 5 箇所（`:19-21` import、`:501-516` create/get/unregister、`:1044-1048` 毎フレーム `step()`、`:1065-1067` `clear()`、`:1177-1179` レジストリメンバ）。すべて素直な登録・呼び出しのみで、`PhysicsOrderedRegistry` / `SharedPtr` / `makeShared` は同一ファイル内の既存コードと同じ使い方。
- `PyroSimulation` は **既定コンストラクタ `PyroSimulation() = default` を持つ**（`PyroSimulation.ixx:260`）。`createPyroSimulation` が引数なしで `makeShared<PyroSimulation>()` を呼べるので整合する。`step(double)`、snapshot、checkpoint API も公开済み（`:277-291`）。
- **CMake 側の配線は既に完了している。** `ArtifactCoreSimulation` は `ArtifactCoreSimulation STATIC` として定義済み（`ArtifactCore/CMakeLists.txt:6202-6214`）、`include/Simulation/` 配下は `_artifact_dependency_target_dir` が `ArtifactCoreSimulation.dir` に解決される（`:2577-2578`）。`PhysicsSystem.cppm` は既に `Core.Simulation.Pyro.ifc` の `/reference` と `OBJECT_DEPENDS` を指定済み（`:1227`, `:1234`）。
- `ArtifactCoreSimulation` は `Artifact` の `target_link_libraries` に列挙済み（`Artifact/CMakeLists.txt:2727`）。**未リンクのライブラリではない。**

したがって Phase 0 の作業は「`target_compile_definitions` で `ARTIFACT_ENABLE_PYRO` を定義する」の 1 行追加に留まる。

実装（2026-09-27）:

- `ArtifactCore/CMakeLists.txt:4367-4372` に OpenVDB ブロック直後へ追加。`target_compile_definitions(ArtifactCore PUBLIC ARTIFACT_ENABLE_PYRO=1)`。
- コメントで「CPUReference のみ。`PyroBackendKind::GPUCompute` は dispatch path が無いため GPU 加速の約束ではない」ことを明記。
- `PUBLIC` とした理由: `PhysicsSystem.cppm` は `ArtifactCore` 本体の private ソース（`ArtifactCore/CMakeLists.txt:4197-4200` の `${CORE_IMPL}`）だが、`ArtifactCoreSimulation` が `ArtifactCore` へ `PUBLIC` で依存している（`:6210`）ため、定義は `ArtifactCore` に置き、推移的に必要な側へ届く形にした。

残る留意点:

- `PyroSimulation` には `LIBRARY_DLL_API` が付いていない（`PyroSimulation.ixx:258` の `class PyroSimulation` が生宣言）。ただし `ArtifactCore` 全体が static ライブラリ構成のため、既存の同系統（`PhysicsSystem` 自体も `LIBRARY_DLL_API` 無し、`PhysicsSystem.cppm:161`）と同じ扱いであり、障害ではない。
- `PyroSimulation` は `std::vector` / `std::unordered_map` / `std::filesystem` を内部で使う。これは既存実装であり、本 milestone では再設計しない。
- `checkpointCache_` は `std::unordered_map<uint64_t, PyroFieldSnapshot>` で、`storeCheckpoint()` が条件付きで書き込むため、レイヤー側 checkpoint と二重管理にならないよう Phase 2 で看着他を注意する。

受入:

- [x] `ARTIFACT_ENABLE_PYRO` 定義の追加根拠（CPUReference 経路のみ有効、GPU 実装を暗示しない）をコードコメントに残す — **2026-09-27 実装済み**
- [x] `createPyroSimulation` / `getPyroSimulation` / 毎フレーム `step()` が実際にコンパイル対象になる — **CMake 配線済み。マクロ定義で有効化される。ビルドは未実施**
- [x] ビルド／実機確認はユーザー明示指示待ちである旨を本文に明記する — **本項に明記済み**

### Phase 1 — component descriptor と Property（2026-09-27 実装済み）

実装した内容:

- `Artifact/include/Layer/ArtifactLayerComponentSystem.ixx:895-910` に `makePyroComponentDescriptor(bool)` を追加。`builtin.pyro` / `artifact.component.pyro` / version 1 / phase=`Dynamics` / scope=`Composition` / order=`760`。fluid（order 750）の直後に置き、phase と scope を揃えた（`PhysicsSystem` はレイヤーごとに1インスタンスを登録するため、clip や生成コピー単位ではなく composition スコープが正しい）。
- `Artifact/src/Layer/ArtifactAbstractLayerImpl.cppm:298-324` に pyro 設定フィールド 24 個を追加。既定値は `ArtifactCore/include/Simulation/PyroSimulation.ixx:150-160` の `PyroSimulationSettings` と、domain の既定（`PyroSimulation.ixx:67-79`）に揃えた。**既定の `PyroSimulation` は不活性なボリュームなので、`component.pyro.enabled` を明示的に true にした時だけこの component が効く前提で値を選んだ。**
- `Artifact/src/Layer/ArtifactAbstractLayer.cppm:404-457` で descriptor の settings へ 24 項目を書き込み `componentHost_.upsert()`。`:476` で `pyroComponentEnabled_ = boolFromHost("artifact.component.pyro")` を追加。
- `Artifact/src/Layer/ArtifactAbstractLayerPhysicsRouting.cppm:763-934` に `component.pyro.*` の setter を 24 個追加。範囲チェックを既存 fluid setter と同じ `std::clamp` 形式で適用。`boundaryMode` は 0/1 の2値なので float で持つ実装にした（Property 側で int に変換）。
- `Artifact/src/Layer/ArtifactAbstractLayerPropertyGroups.cppm:1101-1131` に `PropertyGroup("Pyro")` を追加し、24 項目をすべて登録。**これは既存 fluid で発生している欠陥（setter と descriptor と JSON は揃っているが `getComponentPropertyGroups` には `component.fluid.enabled` しか登録されていない）を pyro では再現しないための措置。**
- `Artifact/src/Widgets/ArtifactPropertyWidgetShared.cppm:215` に `component.pyro.enabled` を `isComponentActivationProperty` の有効化プロパティに追加。
- `Artifact/src/Widgets/ArtifactInspectorWidget.cppm:2090-2092` に `componentInspectorFilter("Pyro")` を追加。
- `Artifact/src/Layer/ArtifactAbstractLayerPersistence.cppm` の保存（`:507-541`）と復元（`:1259-1361`）に 24 項目を追加。復元は各項目に既定値と `std::clamp` があり、未知値・欠損キーでも既定で破綻しない。`components` キー自体が無い場合の無効化リセット（`:1019`）にも `pyroComponentEnabled_ = false` を追加。

未実施（この Phase のスコープ外）:

- **Inspector のコンポーネントボタン（`pyroComponentButton`）は新設していない。** `fluidComponentButton` は生成・ヘッダ設定・配置・状態更新・追加メニュー・toggle で 12 箇所配線されており（`ArtifactInspectorWidget.cppm:1377, 2244, 2252, 2402-2413, 5382, 5408, 5413, 5441-5462, 5487, 5535, 5830-5842, 5869-5871, 5896, 5911`）、同型のボタン新設は別単位の作業になるため Phase 2 の入口で判断する。現状 pyro の設定は Inspector の「Components」グループ内の `Pyro` グループ（`getComponentPropertyGroups`）から編集でき、`component.pyro.enabled` で有効化できるが、コンポーネント一覧のボタンや追加メニューには pyro が現れない。
- ビルド／実機確認は未実施。

受入:

- [x] `component.pyro.*` の全 Property が Inspector から編集でき、値を再起動後も保持する — **コード変更のみ。ビルド未実施のため実 UI での確認は未了**
- [x] 未知 enum 値・欠損キーから復元しても既定値で破綻しない — **復元側に既定値と `std::clamp` を実装済み。実データでの確認は未了**

### Phase 2 — emitter と collider

- 温度・燃料の注入点（emitter）をレイヤーのローカル矩形から導出し、`PyroSimulation` の source として渡す。
- collider を `PyroColliderType` の 2 種（`Box` / `Sphere`）で受け取り、既存 Collider 系の setting を再利用する。
- **新規 signal／slot は追加しない。** 既存 event 経路と service を再利用する。
- smoke／liquid モードは `artifact.component.fluid` の `mode` が持つ既存排他構造（`Artifact/src/Layer/ArtifactAbstractLayerFractureRuntime.cppm:242-246` で fluid mode 1 なら smoke solver を reset している）と競合しないよう、pyro は独立した mode 値として扱うか、fluid の mode 拡張として 2 を追加するかを Phase 0 の結果を見て確定する。

受入:

- [ ] emitter 位置と collider 形状が composition 変換（移動・回転・スケール）へ正しく追従する
- [ ] random access（seek／逆再生）で同じフレームの同じ状態へ到達する

### Phase 3 — 可視化

- `temperature` と `fuel` の場を既存 Diligent 描画へ流す。候補は `ArtifactIRenderer::drawParticles`（`Artifact/include/Render/ArtifactIRenderer.ixx:346`、既存 smoke が `Additive` + `VelocityAligned` で使用）と、`drawGradientRectTransformed`（`:422`）による温度ランプ。
- `PyroFieldChannel::Color` を実装する場合は、Color channel の field を実装してから描画する（宣言済みの値を無視して描かない）。
- 既存の preview／render queue の両方で同じ field 値で同じ見た目を出す。

受入:

- [ ] preview とレンダーキュー出力の field 値・見た目が一致する（parity）
- [ ] 複数レイヤーと合成（Standard / Additive / Screen）スタイルへ正しく挿入される

### Phase 4 — テスト

- `Core.Simulation.Pyro` の単体テストを追加する（`tests/ArtifactCore/`、既存の `PhysicsDeterminismTest.cpp` に倣う名前・登録方式）。
- 決定性（同じ初期状態から同じ結果は同じ）、checkpoint／seek の一致、collider 応答の回帰を固定する。

受入:

- [ ] 決定性テストと seek テストが green
- [ ] 既存 `ArtifactCorePhysicsDeterminismTest` に回帰がない

## 完了条件

- Inspector から pyro component を有効化し、ビューポートで燃焼が見える
- 保存・再読込・Undo で状態が壊れない
- preview／レンダーキュー parity が確認できる
- `Core.Simulation.Pyro` にテストがある
- 上記いずれもビルド・実機確認済みである

## 制約・リスク

- `ARTIFACT_ENABLE_PYRO` を有効化すると `PhysicsSystem.cppm` の 5 ブロックが新たにコンパイル対象になる。モジュール境界や型の不一致によるビルド破壊の可能性があり、Phase 0 で切り分ける。
- `PyroSimulation` は checkpoint を内部に持つ。レイヤーの checkpoint（`LiquidLayerCheckpoint` と同様の管理）と二重管理にならないよう、状態を二重に持たない。
- GPU／Diligent 経路の追加は本 milestone の対象外。CPUReference のみ。

## 実装上の注意

- `#include` は global module fragment（`module;` と `module X;` の間）に置く。purview へ追加しない。
- 独自コンテナ（`NamedVector` / `ArtifactArray` / `HashMap`）を優先し、`std::vector` / `std::map` 等の新規採用を避ける。
- ホットパス（毎フレームの `step()`）で大きな非ゼロ確保を発生させない。field の working buffer は事前確保済みを再利用する。
- 新規 public signal／slot を追加しない。
- ビルド・テスト・CMake はユーザーの明示指示があるまで実行しない。

## 関連

- `docs/planned/MILESTONE_FLUID_COMPONENT_VS_PYRO_DOMAIN_SPLIT_2026-07-01.md`
- `docs/planned/MILESTONE_SURFACE_FX_SYSTEM_2026-07-22.md`
- `docs/planned/MILESTONE_SMOKE_EMITTER_AND_VIEWPORT_INTERACTION_2026-09-27.md`
- `docs/technical/HOT_PATH_RULES.md`
