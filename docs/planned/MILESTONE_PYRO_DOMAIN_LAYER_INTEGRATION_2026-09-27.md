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

残る留意点:

- `PyroSimulation` には `LIBRARY_DLL_API` が付いていない（`PyroSimulation.ixx:258` の `class PyroSimulation` が生宣言）。ただし `ArtifactCore` 全体が static ライブラリ構成のため、既存の同系統（`PhysicsSystem` 自体も `LIBRARY_DLL_API` 無し、`PhysicsSystem.cppm:161`）と同じ扱いであり、障害ではない。
- `PyroSimulation` は `std::vector` / `std::unordered_map` / `std::filesystem` を内部で使う。これは既存実装であり、本 milestone では再設計しない。
- `checkpointCache_` は `std::unordered_map<uint64_t, PyroFieldSnapshot>` で、`storeCheckpoint()` が条件付きで書き込むため、レイヤー側 checkpoint と二重管理にならないよう Phase 2 で看着他を注意する。

受入:

- [x] `ARTIFACT_ENABLE_PYRO` 定義の追加根拠（CPUReference 経路のみ有効、GPU 実装を暗示しない）をコードコメントに残す — **Phase 0 調査完了。実装は次段階**
- [x] `createPyroSimulation` / `getPyroSimulation` / 毎フレーム `step()` が実際にコンパイル対象になる — **CMake 配線済み。マクロ定義のみで有効化される**
- [x] ビルド／実機確認はユーザー明示指示待ちである旨を本文に明記する — **本項に明記済み**

### Phase 1 — component descriptor と Property

- `artifact.component.pyro` の `makePyroComponentDescriptor()` を `Artifact/include/Layer/ArtifactLayerComponentSystem.ixx` に追加する。`fluid` の `makeFluidComponentDescriptor()`（`:881-893`、phase=`Dynamics` / scope=`Composition` / order=`750`）と同じ形にする。
- `component.pyro.*` の Property を `getComponentPropertyGroups()` に登録する。
  - **注意**: 既存 fluid で同じ抜けがある。`getComponentPropertyGroups` に登録されるのは `component.fluid.enabled` **のみ**（`Artifact/src/Layer/ArtifactAbstractLayerPropertyGroups.cppm:1098`）で、setter（`Artifact/src/Layer/ArtifactAbstractLayerPhysicsRouting.cppm:514-762` に 27 個）・descriptor settings・JSON（`Artifact/src/Layer/ArtifactAbstractLayerPersistence.cppm:456-506`）は揃っているが UI からは到達できない。pyro ではこの抜けを再現しない。
- JSON 保存／復元を追加する。既存の流体と同じ **2 系統**（`components` フラットオブジェクトと `componentGraph` 配列）が並存する構造を踏襲し、値の往復は前者で確実に行う。

受入:

- [ ] `component.pyro.*` の全 Property が Inspector から編集でき、値を再起動後も保持する
- [ ] 未知 enum 値・欠損キーから復元しても既定値で破綻しない

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
