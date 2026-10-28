# M-PYRO-1: Pyro Domain のレイヤー統合（燃焼・火・煙の emitting 経路）

**最終更新:** 2026-10-28
**ステータス:** In Progress（resolution-scaled effect profile、爆発向けradial velocity source、Diligent GPU volume raymarch、任意の時間変化ディテールと実験的な煙の自己遮蔽を追加。FireAndSmoke transferと5 profileをD3D12/Vulkanで検証。細部の品質、Composition描画への接続、preview/render queue parityは未完了）
**識別子:** M-PYRO-1
**対象:** `ArtifactCore/src/Simulation/PyroSimulation.cpp`、`Artifact/src/Layer/ArtifactAbstractLayer*.cppm`、`Artifact/include/Layer/ArtifactLayerFluidRuntimeState.ixx`
**関連:** `docs/planned/MILESTONE_FLUID_COMPONENT_VS_PYRO_DOMAIN_SPLIT_2026-07-01.md`、`docs/planned/MILESTONE_SURFACE_FX_SYSTEM_2026-07-22.md`

## 目的

`ArtifactCore` に既に存在する `PyroSimulation`（density / temperature / fuel / pressure / velocity を扱う domain-owned シミュレータ）を、実際にレイヤーから发出して描画できる状態にする。

現状 `pyro` は **Core 側の契約と CPUReference 実装だけがコンパイルされている状態で、アプリ側（`Artifact/`）から一切参照されていない**。本 milestone は「enum が存在する」を「Inspector で設定でき、ビューポートで燃え、書き出しできる」に変える。

## 現状の根拠（2026-09-27、実コード照合）

## R&D 更新（2026-10-27）

- `PyroEffectKind` と `PyroEffectProfile` を `Core.Simulation.Pyro` に追加。共通の `PyroSimulation` に対して Smoke / Fire / Explosion / Steam / Fog の開始設定・source値・放出時間を返す `makePyroEffectProfile(kind, resolution)` を提供する。`PyroSourceState::velocity` はsolverのbacktraceと同じgrid cells/secondであることを明記し、source速度と爆発radial velocityをdomain解像度に比例させる。
- Explosion は短いsource duration、高い初期density/temperature/fuel、広いsource extentとradial velocityを持つ。`PyroSourceState::radialVelocity` は圧力投影後にsource中心から外向きの速度を加え、divergence fieldを再計算する。shock frontや圧縮性ガス解法は未実装。
- Smoke / Fire / Steam / Fog のfuel・temperature・buoyancy・source extentを分けた。これらは開始値であり、専用renderer / transfer function / Inspector導線は未実装。
- `tests/ArtifactCore/PhysicsDeterminismTest.cpp` にeffect profile設定、grid resolution scaling、radial sourceのfocused testsを追加。radial injectionは無効時との比較、投影後の正のradial velocity/divergence、同一条件の再現性を検証。直近の全実行では40 testsがpass。
- `tests/Artifact/SmokeRuntimeStandalone/SmokeRuntimeStandaloneTest.cpp::RendersSmokeFireExplosionSteamAndFogProfilesToDistinctVolumes` は同じ40³ domain・放出条件・カメラで5 profileを動かし、各profile専用のCPU `TransferFunction` で画像化する。既存生成済みmodule-interface / implementation / test projectsを直接buildし、focused test 1/1 pass。captureは `build/smoke-runtime-rnd/pyro-effect-profiles-20261027-40grid-centered/`。
- 同じlookdev testに固定seed 3D smooth noiseによるtest-only density modulationとdomain warpを追加。元のvolume massへ再正規化し、同じCPU rendererでbase/detail画像を比較する。40³ fieldでbase/detail massは一致し、各effectのimage RMS差はsmoke 10.11、fire 3.96、explosion 4.50、steam 14.56、fog 1.56（8-bit channel値）。focused test 1/1 pass。captureは `build/smoke-runtime-rnd/pyro-effect-profiles-20261027-detail-rnd/`。
- domain warpは低解像度volumeの輪郭を変え、単純なdensity乗算（detail前段のRMS差0.16〜0.38）より明確な画像差を作る。ただし単一フレームの静的warpであり、時間一貫性・視認上のbillow品質・production適用性は未検証。高周波velocity transportを置き換えるものではない。
- warp/noise phaseをabsolute timeから評価し、30fps相当の隣接field/renderと同時刻replayを追加した。massは全effectで一致し、同じ時刻のdensity配列は完全一致。隣接画像RMS差はSmoke 0.547、Fire 0.525、Explosion 0.449、Steam 0.614、Fog 0.184（8-bit）。静的warpの比較に対し時間変化が小さく連続であることは確認したが、solver velocityに沿った移流ではなく、実時間の一貫性を証明しない。focused test 1/1 pass、captureは `build/smoke-runtime-rnd/pyro-effect-profiles-20261027-temporal-rnd/`。
- `TransferFunctionKind::Fog` / `TransferFunction::makeFog()` をCPU volume reference rendererに追加。既存enum値は維持し、FogはSmokeより低吸収・低発光の青灰色カーブを使う。同一field比較でFogのvisible pixelsは旧Smoke transferの805から298へ減り、画像hashも異なる。lookdev renderer/testは1/1 pass、captureは `build/smoke-runtime-rnd/pyro-effect-profiles-20261027-fog-transfer/`。Core Debug build成功、`PyroEffectProfileTest` は2/2 pass。これはreference CPU経路で、本体GPU rendererへ接続したものではない。
- Diligent用の `GpuVolumeRenderer` を既存 `Render.VolumeRenderer` moduleへ追加。3D density / temperature fieldを再利用可能なDiligent textureへアップロードし、compute raymarchでRGBA16F出力を作る。Smoke / Fire / Explosion / Steam / Fogのtransfer分岐を持つ。dimension変更時のみtextureを作り直す。Core Debug MSBuildとArtifact側GPU contract testのDebug buildは成功。`PyroGpuVolumeRendererProducesDistinctTransferLooks` はVulkan / RTX 4070 TiとD3D12で各1/1 passし、5枚すべての8-bit出力がbackend間でbyte-identicalだった。captureは `build/smoke-runtime-rnd/pyro-gpu-volume-20261027/` と `build/smoke-runtime-rnd/pyro-gpu-volume-20261027-d3d12/`。
- `PyroGpuVolumeRendererRendersSimulationFields` を追加し、PyroSimulationを24³ domainで48 fixed steps進めた実密度・温度を同GPU raymarcherへ渡して5 profileを描画した。Vulkan / RTX 4070 TiとD3D12で各1/1 pass（約2.65s / 2.79s）。すべてのprofile間で画像が異なり、5枚すべてbackend間でbyte-identical。画像は `build/smoke-runtime-rnd/pyro-sim-gpu-20261027-vulkan-48f/` と `build/smoke-runtime-rnd/pyro-sim-gpu-20261027-d3d12-48f/`。これは実solver fieldからGPU rendererまでのtest経路を検証するが、composition統合やDCC級の画質を意味しない。
- 目視では48 stepでも煙・炎はまだ塊状で、爆発も滑らかな雲に留まる。低解像度solverの移流・浮力・燃焼だけでは細いbillowや細かな火炎構造を作れないことが再確認された。wavelet detail、時間一貫した密度shaping、volume lighting/shadowのlookdevが次の研究対象。
- `GpuVolumeRenderSettings` に既定無効の `detailStrength / detailScale / detailTimeSeconds / detailSeed` を追加。compute shaderで整数hash由来の滑らかな3D value noiseをdensity・temperatureへ適用し、seedと絶対時刻から決まるディテールをGPU上で評価する。testは実PyroSimulation fieldでdetail off/onを比較し、同一時刻の再レンダー一致と時刻変更後の画像変化を確認。Vulkan / RTX 4070 TiとD3D12で各1/1 pass、5 profileすべての画像はbackend間でbyte-identical。captureは `build/smoke-runtime-rnd/pyro-sim-detail-20261027-vulkan/` と `build/smoke-runtime-rnd/pyro-sim-detail-20261027-d3d12/`。
- GPU volume rendererへoptional reaction textureを追加し、reaction fieldがあるFire transferでは燃焼率で発光色をgateする。fieldなしの既存callerは従来temperature-based fireを維持する。実PyroSimulationのreaction付き Fire とreactionなし経路が異なることを確認し、focused testはD3D12 / Vulkanで各1/1 pass。captureは `build/smoke-runtime-rnd/pyro-reaction-gate-20261027-d3d12/` と `build/smoke-runtime-rnd/pyro-reaction-gate-20261027-vulkan/`。画面上は小さな発光塊に留まり、炎の形状品質はまだ不足。
- 2026-10-27の最新 focused rerun は既存GPU contract executableを用いてD3D12 5.04秒、Vulkan 3.96秒で各1/1 pass。sourceからのtarget rebuildは `ArtifactCore/src/Physics/FluidSolver2D.cpp` の未宣言 `constraintPreviousPositions_` によるC2065/C2660で先行停止し、新executableの生成には至っていない。この FluidSolver エラーはPyro変更外の既存working tree差分であり、ここでは修正しない。
- このノイズはraymarch時のdensity shapingであり、simulation velocityを変えず、velocityでノイズ座標を移流もしない。目視では丸いvolumeの輪郭に小さな濃淡が加わる程度で、shreddingやwavelet turbulenceの代用ではない。shader計算コストの計測も未実施。高品質化ではrender-only detailとvelocity/temperature由来のsolver shapingを分けて比較する。
- GPU contract testのfieldは単純な球状密度・温度の合成入力であり、Pyro solver fieldの画質検証ではない。視覚上は滑らかな球に近く、煙のbillow、炎の揺らぎ、爆発shock frontは未実装。テストで `Diligent::Box` の範囲順序誤り（Min/Maxを各軸交互に渡す必要がある）を検出・修正した。
- このGPU経路は現状R&D用のfield rendererで、レイヤー統合・composition描画への出力接続・CPU fieldの毎フレームtexture upload性能・D3D12/Vulkan両backendの実行結果は未検証。現段階でC4D/Houdini相当の画質とは判断しない。
- 確認値（capture時点）: density/temp/fuel/reaction massとvisible pixelsはSmoke `76.46/2.97/0/0; 1021`、Fire `150.50/302.83/85.92/105.19; 678`、Explosion `169.35/274.19/187.15/59.31; 1629`、Steam `48.69/22.00/0/0; 425`、Fog `637.99/0/0/0; 805`。固定cameraで全5画像が非空かつpixel hashが相異なる。
- 目視ではFireは細い黄色い火炎、Explosionは広い橙色cloud、Smoke / Steam / Fogは柔らかなvolumeになるが、煙や蒸気はまだ丸い形でbillowの高周波detailが足りない。Smoke vorticity 0.18→0.6ではdensity mass 76.46→73.15に減った一方、画像上の形状改善はほぼ見られず、渦度単独では詳細化しないと判断。波レット等の時間一貫した高周波detailとproduction GPU renderer接続は未完了。
- production 2D Fluid smoke runtime、3D Pyro solver、volume lookdev fixtureは異なる経路であり、相互adapterとComposition renderer統合は未検証。

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

### 現時点で残る統合ギャップ（2026-10-27 実コード再確認）

- `Artifact/` には `builtin.pyro` descriptor、`component.pyro.*` のProperty・保存復元・Inspector filteringがある（`Artifact/include/Layer/ArtifactLayerComponentSystem.ixx`、`Artifact/src/Layer/ArtifactAbstractLayer*.cpp`、`Artifact/src/Widgets/ArtifactInspectorWidget.cpp`）。一方、`Artifact/` に `PyroSimulation` の参照・生成・field描画 consumer はまだ見つからず、これらのPropertyが実際のsimulation/renderへ流れる接続は未完了。
- `PyroBackendKind::GPUCompute` は `PyroSimulation` のstep backendとしてdispatchされない。CMake macroはCPUReferenceのPhysicsSystem registry / per-frame stepを有効化するだけでGPU solverを意味しない。
- `PyroFieldChannel::Color` / `PyroFieldMask::Color` は宣言のみ。`PyroFieldSet` にcolor field、`PyroFieldSnapshot` にcolor viewは無い。
- `PhysicsSystem` には `createPyroSimulation` / `getPyroSimulation` / unregister / 毎フレームstepのregistryがあり、`ArtifactCore/CMakeLists.txt` は `ARTIFACT_ENABLE_PYRO=1` を定義している。これはsimulation lifecycleのCore側接続を示すが、Artifact layer componentからのemitter/collider更新やrendererへのfield引き渡しを示すものではない。
- テストは存在する。Core determinism/profile tests、`SmokeRuntimeStandalone` のCPU lookdev tests、Artifact GPU contract testsがあり、今回の `PyroGpuVolumeRendererRendersSimulationFields` は24³/48-stepの実solver fieldをGPUで描画する。UI・Composition・preview/render queue integration testsは未完了。

### 既存ドキュメントとの差異

`MILESTONE_FLUID_COMPONENT_VS_PYRO_DOMAIN_SPLIT_2026-07-01.md:9` は2026-07-01時点の基準記録である。現在はCPUReferenceのPhysicsSystem lifecycleとArtifact側のcomponent設定面、および独立したGPU volume lookdev rendererを持つ。ただし、`GPUCompute` solver backend、component→Pyro emitter/collider routing、Composition renderer extraction、cache/bake/preview/render queue統合は未完了。

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
- production Composition への3Dボリューム描画接続と、OpenVDBからの初期場構築（Core GPU volume rendererはR&D実装済み）
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
- [x] `createPyroSimulation` / `getPyroSimulation` / 毎フレーム `step()` が実際にコンパイル対象になる — **CMake macro配線済み。Artifact GPU contract targetのDebug MSBuildでArtifactCore/PhysicsSystemを含む依存buildが成功。Compositionからの実動作接続は未確認**
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
- Artifact GPU contract targetのDebug MSBuildと、実Pyro fieldを使うfocused GPU testはVulkan / D3D12で成功。Inspector UIでの編集・保存往復は実機確認していない。

受入:

- [x] `component.pyro.*` の全 Property が Inspector から編集でき、値を再起動後も保持する — **Property / persistence経路を実装。関連Debug target buildは成功したが、実UIでの編集・再起動後復元は未確認**
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
- [x] effect profileの煙・炎・爆発・蒸気・霧初期値に対するfocused契約テストがgreen（1/1）
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
- この milestone は当初 CPUReference で統合境界を固める計画だったが、2026-10-27 時点では Diligent GPU volume raymarch を独立 R&D で追加済み。Composition / render queue の本番接続と preview parity は引き続き未完了。

## 2026-10-27: Pyro 渦度 scratch の再利用

- `applyVorticityConfinement()` が毎 fixed step に作っていた velocity snapshot vector を `PyroSimulation` 所有の事前サイズ済み scratch に変更。domain 設定時に確保し、既存 density/reaction の数値経路は変更しない。
- Debug GPU contract target を MSBuild し、実 Pyro solver field を使う `PyroGpuVolumeRendererRendersSimulationFields` を RTX 4070 Ti の D3D12 と Vulkan で各 1/1 pass。
- この確認は module/link と solver→renderer経路の回帰確認であり、scratch allocation 計測や DCC 品質を検証したものではない。次段では時間一貫性を持つ velocity/detail shaping と燃焼場の見た目を研究する。

## 2026-10-27: 炎と煙の同一ボリューム描画

- GPU VolumeRendererへ `FireAndSmoke` transferを追加し、同じ密度・温度・反応率fieldから炎の発光と煙の散乱・吸収を一度のraymarchで評価する。反応率は炎の発光だけをgateし、煙の存在と濃度依存の吸収は維持する。
- CPU VolumeRendererにも同じtransferの項目を追加。既存Fireの公開 `TransferFunctionSample::emission` を維持し、混合モードでは煙を `emission`、炎を独立した `fireEmission` で表現してreaction/intensityを炎だけへ適用する。
- GPU contract testに実PyroSimulation Fire fieldからmixed transferを描画し、同一fieldのFire-onlyおよびSmoke画像と異なる画像になるassertionを追加した。さらにreactionを全ゼロにした同一density/temperature fieldでもalpha>8の画素が残ることを検査する。Debug build後、RTX 4070 TiのD3D12/Vulkanで各1/1 pass（2026-10-28）。

## 2026-10-28: 解像度と画面内位置の比較

- 既存GPU fixtureは24³ / 96px、CPU fixtureは40³ / 96px。元のGPU fixtureはsource中心の高さ0.16に対してExplosion sourceの高さ方向extentが0.45であり、発生範囲がdomain外へはみ出していた。GPU fixtureのsourceをdomain中心へ移して固定し、出力画素数とsimulation gridを別々に指定できるようにした。
- RTX 4070 Ti / D3D12で同じ384×384出力・48 fixed stepsの24³と48³を比較し、各focused testは1/1 pass（3.92秒 / 9.90秒。CPU simulationと複数readbackを含むDebug test全体時間）。48³ではalpha>8の煙bboxは `(134,127)-(249,248)`、爆発は `(53,45)-(330,328)`（inclusive）で画像端から離れた。7画像のD3D12/Vulkan出力はRGBA byte一致し、Vulkanも1/1 pass（10.29秒）。capture: `build/smoke-runtime-rnd/pyro-centered-24-384-20261028/`、`pyro-centered-48-384-20261028/`、`pyro-centered-48-384-20261028-vulkan/`。
- 目視では48³は24³より輪郭が滑らかだが、煙と炎はほぼ円形で、爆発も柔らかい球状である。解像度増加だけではbillow、燃焼の舌、照明・自己遮蔽を実現できず、DCC品質には達していない。計測は単一時刻の独立R&Dであり、実アプリのpreviewや再生性能を示さない。
- CPU lookdev fixtureにもdomain全体を収める固定カメラ、256px既定出力と40〜96³の選択、画像上下端の検査を追加した。独立projectの既存生成設定にDiligent include pathがなく `VolumeRenderer.ixx` の依存scanで停止したため、変更後CPU fixtureは未コンパイル・未実行。CMake生成は行っていない。

## 2026-10-28: GPU座標変形の比較実験（採用せず）

- 48³ solver fieldと384px出力に対し、実験的にdensity / temperature / reactionのsample座標を同じ3D value noiseで最大domain幅の9%だけ変形した。既存の濃度変調あり、同じ時刻・seedで再描画した場合の一致と1/30秒後の画像変化をD3D12/Vulkanで検査し、各1/1 pass。3枚の煙画像はbackend間でRGBA byte一致。無効時の画像は以前の48³ captureとbyte一致。
- 通常の煙bboxは `(134,127)-(249,248)`、変形時は `(127,126)-(268,254)`（alpha>8、inclusive）。画像全体のRGBA RMS差は `16.60 / 16.60 / 16.60 / 21.55`、隣接時刻は `0.72 / 0.72 / 0.72 / 0.64`（8-bit）。ただし静止fieldに時間変化するnoiseを適用した比較で、実simulation再生の時間一貫性を示すものではない。
- 目視では輪郭が変わる反面、煙の下側に不自然な筋が現れた。品質向上の根拠とならないため実験の本体実装・APIは撤去し、画像のみ `build/smoke-runtime-rnd/pyro-warp-20261028-d3d12/` と `pyro-warp-20261028-vulkan/` に残した。撤去後のDebug buildとD3D12 focused testは成功し、7画像が実験前の基準とRGBA byte一致した。GPU時間は未計測。次はvelocityで運ばれるdetailやvolume lightingと、複数時刻での視覚的な破綻を評価する。

## 2026-10-28: GPU煙の自己遮蔽R&D

- Diligent `GpuVolumeRenderer` に既定無効の `smokeLightingStrength` と `smokeShadowDensity` を追加。煙sampleから固定方向に最大12回densityを読み、Beer-Lambert型の光透過を用いて煙色を変える。`FireAndSmoke` では煙の寄与だけを照らし、炎発光とalphaは変えない。既存textureを再利用する。
- RTX 4070 Ti、48³ field・384px・48 stepのfocused GPU testはD3D12 / Vulkanで各1/1 pass。光のon/offで画像は変わり、alphaは全pixelで一致。off画像は前回基準とRGBA byte一致、両backendの出力8画像もRGBA byte一致。画像全体RGB RMS差は約6.80/255。capture: `build/smoke-runtime-rnd/pyro-light-20261028-d3d12/` と `pyro-light-20261028-vulkan/`。
- 光源側から影側への階調は生まれたが、煙の形状は依然球状。固定方向・固定距離の近似で、scene light、多重散乱、炎の照明、Composition接続は未実装。GPU時間、複数frameの見た目とrender queue parityも未確認。参考: [Pixar Production Volume Rendering](https://graphics.pixar.com/library/ProductionVolumeRendering/paper.pdf)、[NVIDIA Volume Rendering Techniques](https://developer.nvidia.com/gpugems/gpugems/part-vi-beyond-triangles/chapter-39-volume-rendering-techniques)。

## 2026-10-28: 固定矩形と連続煙のフレーミング

- `GpuVolumeRenderSettings` にドメイン正規化座標の中心と幅・高さを追加。既定は全域表示で、範囲外は透明。固定矩形でraymarch位置を変え、simulation field・texture所有・同期は変えない。frameごとの自動追従は行わない。
- 48×96×48 domain、384×768出力、下側の連続emitter、照明有効で24/48/72/73/96 frameを比較。全域表示ではalpha重心Yが651.78→623.66、固定矩形では510.35→447.87へ移り、frame 96のalpha>8 pixelは13,334→65,849。固定矩形は5 frameすべてで画像端にalpha>8 pixelなし。D3D12 / Vulkan testは各1/1 pass。5画像のうち4枚はRGBA byte一致、frame 96の1画素のみRGBが最大1/255異なりalphaは一致。72→73 frameの画像全体RGBA RMS差は約1.23/255。
- 全域の既定設定は既存8画像のうち7枚がbyte一致し、Explosionの赤1画素が1/255異なる。capture: `build/smoke-runtime-rnd/pyro-smoke-sequence-20261028-d3d12/`、`pyro-sequence-framed-20261028-d3d12/`、`pyro-sequence-framed-20261028-vulkan/`。固定矩形は対象時間を事前に知るR&D向け設定であり、長時間の移動やComposition cameraへの接続は未検証。拡大画像でも格子感と塊状の見た目が残るため、simulationの解像度とdetail品質の課題は未解決。

## 2026-10-28: 高解像度時のcheckpointメモリ

- 同一カメラ・384×768・96 frameの煙を48×96×48と64×128×64で比較。64側は輪郭が滑らかになる一方、形は依然として大きな塊であり、高周波の渦は得られなかった。64側の5 frameは端に触れず、D3D12 test 1/1 pass。
- `setCacheInterval(0)` は従来 `1` に変換され、無効化の意図で使うGPU R&D fixtureが毎frame全fieldをcheckpointへコピーしていた。`0` を自動checkpoint無効として扱い、正の間隔は従来どおり保持するよう変更。`PyroZeroCacheIntervalDisablesCheckpoints` は無効時の未保存と間隔1での復元を確認し1/1 pass。
- 同じRTX 4070 Ti / D3D12 Debug test、64×128×64・96 frame・384×768、processを0.1秒間隔でsampleした一回ずつの比較: peak private memoryは2,246.2→530.1 MiB、peak working setは2,305.9→595.2 MiB。全test時間は15.33→15.39秒で、速度改善の根拠にはならない。GPU memory、全allocation、長時間動作は未計測。
- 修正前後の5 captureはRGBA byte一致。修正後のD3D12 / Vulkanでも各1/1 pass、5 captureはRGBA byte一致。capture: `build/smoke-runtime-rnd/pyro-sequence-framed-64-20261028-d3d12/`、`pyro-sequence-cache-off-64-20261028-d3d12/`、`pyro-sequence-cache-off-64-20261028-vulkan/`。正のintervalを使用する場合のcheckpoint増加は残り、solverの移流用fieldコピーも毎step発生する。

## 2026-10-28: 密度の高次移流R&D

- Composition 接続境界を再確認: `PhysicsSystem` に Pyro registry と step はあるが、`Artifact/src` には `createPyroSimulation` / `getPyroSimulation` の呼び出しがなく、既存 Composition draw は 2D Fluid overlay を描く。Pyro component の設定を有効化するだけでは独立 3D renderer には届かない。`GpuVolumeRenderer::outputSRV()` に、再描画で内容が上書きされ resize / destruction で view が無効となる所有契約を追記。実接続には simulation lifecycle、設定・source routing、frame/seek、renderer の device/context と resource lifetime、Composition / queue parity が必要。

- 同一 GPU output texture の再利用: smoke SRV を sprite queue に積んだ後、送信前に同じ volume renderer で fog を書くと、D3D12 / Vulkan の両方で元の smoke 画像を保持しなかった。sprite を `flush()` で送信してから上書きすると smoke とバイト一致し、両backendのテスト pass。Composition 統合時は各未送信 draw が参照する texture の寿命と書き換え順を保証する必要がある。これは単一 immediate context の独立 fixture であり、別 context、非同期実行、実 Composition は未検証。

- GPU texture 2枚の同時利用: 同一 field を smoke と fog transfer でそれぞれ別の `GpuVolumeRenderer` に出力し、2枚の SRV を同じ画面の左右に並べるテストを追加。D3D12 / Vulkan で左右とも非空、test pass。同じ出力 texture を再利用する場合の上書き順、透明同士の重ね合わせ、異なる simulation、炎、実 Composition には未対応・未検証。

- GPU handoff の重なり: 透明面へ opacity 0.5 で smoke SRV を描画し、alpha が source の半分から最大1/255以内であること、続く不透明な赤矩形が中央で煙を覆うことを assert。D3D12 / Vulkan 両方 pass。これは smoke texture と単一矩形の順序で、複数 volume の同一 texture 再利用や fire、Composition 階層の合成順は未検証。

- GPU 合成背景の比較: 96² smoke fixture を不透明な RGB 背景へ重ね、8bit sRGB から復元した近似線形 source-over と比較。alpha > 32 の608画素で RGB 最大差は最大3/255・平均1.03/255、D3D12 / Vulkan 同値。色差は診断出力のみであり、浮動小数点の参照、半透明背景、複数レイヤー順序や実 Composition は未検証。

- GPU texture handoff 色差の切り分け: Pyro 出力は RGBA16F、QImage readback は linear RGB を sRGB 8bit に変換し、描画面は RGBA8 sRGB。透明面への描画結果を `sRGB_encode(sRGB_decode(source) × alpha)` と比較すると、608画素の最大差 3/255・画素RGB最大差の平均 1.01/255、alpha差 0。D3D12 / Vulkan の同一 fixture で同値・test pass。source の8bit丸めを含む近似比較で、合成結果を未乗算RGBと直接比べた前回の色差はそのまま不具合を意味しない。背景色あり・半透明重ね合わせ、浮動小数点の厳密参照、実 Composition は未検証。

- GPU texture handoff の数値観測: 96² smoke fixture、透明背景・等倍描画、D3D12 / Vulkan とも非空テスト pass。raw readback の alpha は全画素一致した一方、source/result の alpha > 32 となる608画素の `QImage::pixelColor` RGB最大差は 59/255、画素ごとのRGB最大差の平均は 29.53/255。source RGB を alpha 乗算した値との比較も最大36・平均22.53/255。色空間・出力面の transfer・readback形式を揃えていないため、現時点で色バグや実アプリでの見た目を断定できない。alpha 以外の parity は未達・要調査。

- GPU texture handoff: 独立 Pyro volume の出力 SRV を、同一 Diligent device の `ArtifactIRenderer::drawOffscreenTexture` に渡し透明な面へ合成する検証を追加。D3D12 / Vulkan の5 profile test がそれぞれ pass、煙の描画が非空で角が透明であることを確認した。合成前の CPU readback は手渡しには使用しないが、テストは検証のため前後に readback を行う。アルファ値／色の厳密一致、非単位変形、複数レイヤーの順序、texture 再利用時の同期、実 Composition / Render Queue 接続は未検証。

- 履歴なし画角の試作: 正密度の重心と各軸の標準偏差から `6×max(σx, σy)` の範囲を計算し、平滑追従なしで適用。3移動 source・48×96×48・384 frame・D3D12 の5画像は端接触なしで test pass。初回検出後の frame 間最大変化は 0.000735、bbox直接方式の 0.0625 より小さく、最後の直接値と表示値は一致。ただし初回は既定画角から 0.276 の急変、frame 24 可視 pixel は 9,062（bbox平滑は13,045）で、常に大きく映るわけではない。密度 ≥0.002 の voxel が一つもないと画角を保持するため空状態では履歴依存が残る。少量の遠方煙が切れる可能性、全 frame の端切れ、実 seek、GPU-only field の読み戻し負荷、炎は未検証。画像 `build/smoke-runtime-rnd/pyro-moment-frame-20261028/`。test-only。

- 自動画角の揺れ計測: 3移動 source、48×96×48、384 frame の density bbox 直接計算では、最初の検出を除き17 frameで画角パラメータが変わり、最大1 frame差は 0.0625。平滑追従は最大 0.01328。単一条件の normalized view parameter 上の比較で、視覚的な揺れや再生・seekの整合を定量化するものではない。D3D12 test pass。ログ `temp/pyro-auto-jitter-test.log`。

- 自動画角の履歴依存確認: 3移動 source の frame 384 の同一 density field を、連続追従済みの画角と、その場の density 範囲から直接求めた画角で D3D12 描画。center X/Y/scale 差は 0.00000113 / 0.004132 / 0.00000721、画像はバイト一致しなかった。test pass。実際のシークではなく、履歴なしの画角再構築を模擬した比較であり、再生・シーク間で同じ構図を保証できないことを示す。二画像: `build/smoke-runtime-rnd/pyro-auto-history-20261028/smoke-384.png` と `smoke-direct-final.png`。

- 自動画角・3発生源: source が半径0.2の範囲で移動する条件を追加し、48×96×48、384×768、D3D12、高次密度移流で384 frame まで実行。24/192/288/336/384 の5画像で端接触なし、test pass。frame 384 の可視 pixel 22,433、重心 Y=388.61、全frameの view parameter 最大変化 0.01328。小さな source を3つ使い各発生量も変更しており、単一 source と同一煙量の比較ではない。画面端以外の場所で各 plume がすべて保持されること、全 frame の端切れ、seek、負荷は未検証。画像: `build/smoke-runtime-rnd/pyro-auto-spread-20261028/`。

- 自動画角の pulse 検証: source を frame 24 で停止し、48×96×48・384×768 の同条件で384 frame まで実行。D3D12 / Vulkan とも pass、24/192/288/336/384 の5画像は両backendでバイト一致。frame 384 の可視 pixel 47,309、重心 Y=359.97、全frameの view parameter 最大変化 0.00634 未満。煙は frame 384 でも残っていたため、完全に消えた後の動作は未検証。画像: `build/smoke-runtime-rnd/pyro-auto-pulse-20261028/` と `pyro-auto-pulse-vulkan-20261028/`。連続画像全体の端切れ、seek と performance の検証も未実施。

- 自動画角の独立試作: 密度 ≥0.002 の外接矩形を毎 frame CPU 走査し、1.5 倍の余白と係数 0.08 の平滑追従で正方 normalized view を更新。48×96×48・D3D12・384×768・高次密度移流の連続発生では、24/192/288/336/384 frame の5画像すべてで煙が画面端に接触せずテスト pass。frame 24 は 52,243 可視 pixel・重心 Y=397.69、frame 384 は 66,990 pixel・Y=350.67。全384 frameでの各 view parameter の最大変更量は 0.00634 未満。ただし端接触を検査したのは上記5 frame のみ、1条件・1回の測定で、密度閾値、急な出現・複数煙源・炎・空フレーム・seek時の履歴依存、GPU readback や CPU 走査負荷は未検証。画角追従は Composition の意図した構図を変えるためテスト専用であり、production に適用していない。画像: `build/smoke-runtime-rnd/pyro-auto-frame-20261028/`。煙自体の詳細は不足。

- 固定ビューの長時間検証: 48×96×48、高次密度移流、384×768、D3D12、連続発生では拡大ビューは frame 192 まで画面端に触れなかったが、288 / 336 / 384 で端接触しテスト失敗。全領域ビューは同フレームで通過する一方、frame 24 の可視面積は 38,269→7,744 pixel と小さく、重心は Y=651.86 / 768 と下寄り。長時間で収まることと初期の見やすさを単一固定矩形では両立しない例であり、既定変更や自動追尾の品質を証明するものではない。画像は `build/smoke-runtime-rnd/pyro-longer-view-20261028/` と `pyro-full-view-long-20261028/`。

- 球形 source を24 frame で止めて96 frame まで進める GPU 検証を追加。64×128×64、高次密度移流、384×768、D3D12 で pass。frame 96 は alpha > 8 が35,522 pixel、重心 Y=460.91 / 768 で、画像はなお滑らかな丸い塊。連続発生との注入量は異なるため画質の定量比較ではない。画像: `build/smoke-runtime-rnd/pyro-pulse-24-20261028/`。

- 横長 smoke source の比較: 球形 extent `(0.16, 0.16, 0.16)` と解析体積を揃えた `(0.24, 0.0711111, 0.24)` を、同じ64×128×64、384×768、D3D12、高次移流、96 frame で描画。テストは pass、frame 96 の alpha > 8 は 44,298 pixel、重心 Y=478.91 / 768。球形の 57,168 pixel より小さく、形は平たい半円状で細部の改善は見られない。格子上の注入量と結果の露出を揃えた比較ではない。出力: `build/smoke-runtime-rnd/pyro-wide-source-20261028/`。

- checkpoint の solver version を 3 とし、v3 専用ファイル名で保存・読み込みする。旧 version 1/2 のファイルは削除せず、新実装では読み込まない。旧形式拒否、旧ファイルの保持、移流方式別の v3 保存／読み戻しを含む4テストが pass。source や collider の変更履歴、その他 settings の変更を識別する仕組みは未実装で、これらが異なる v3 checkpoint の再利用は引き続き安全とは限らない。

- 発生源の非等方範囲: 以前は走査範囲に各 extent を使う一方、減衰は最大 extent の球距離で計算していた。各軸の extent で正規化した楕円距離へ修正し、density / temperature / fuel / source velocity と radial velocity の falloff を揃えた。等方の source は従来式を維持。D3D12 の source 数値テスト、5 profile 描画テスト、96 frame smoke テストの3件が pass し、球形煙の5画像は変更前とバイト一致。既存キャッシュの互換性と長時間動作は未検証。

- 48³、384²、D3D12 の5 profile を再描画して pass。Fog の目視比較では旧画像の角張った矩形に対し、新画像は横長で丸く減衰する。ただし両画像は露出や総量を揃えた画質比較ではない。旧 `pyro-centered-48-384-20261028/pyro-simulation-fog.png`、新 `pyro-ellipsoid-profiles-20261028/pyro-simulation-fog.png`。旧 solver 方式の checkpoint は上記 v3 分離で読み込まない。source 変更履歴などの識別は未実装。

- 移流前 field コピーの作業領域を domain 初期化時に確保して再利用。D3D12 の既定および高次密度移流、Vulkan の高次密度移流の各96フレームテストが通過し、各5キャプチャは変更前とバイト一致。フィールドが外部でリサイズされた場合は次のステップで再確保する。allocator 計測や速度評価は未実施で、見た目は改善していない。

- 上記変更後のピークメモリを追加測定: D3D12、64×128×64、高次密度移流、384×768、96フレーム、cache interval 0 の1回の実行で、100ms間隔の process sampling は private 538.1 MiB、working set 604.6 MiB。テスト pass。変更前の 530.1 MiB は一次移流の測定であり、条件が異なるため増減を比較できない。GPU メモリ、allocation 回数、速度への影響は未計測。

- Vulkan 統合テストで preview 側の `DiligentDeviceManager` を queue 実行前から保持する実験を行ったが、Vulkan device 重複エラーと D3D12 初期化は継続したため、テストコード変更は撤回。queue 側も含めたデバイス生成経路の調査が必要。ログ: `temp/smoke-render-queue-vulkan-shared-20261028.log`。

- 2D Fluid の既存 Composition / Render Queue 統合テストは Vulkan 指定でも pass したが、ログに Vulkan instance / device の同時生成エラーと D3D12 adapter 初期化があり、Vulkan 一貫経路の証拠にはならない。テストは非既定の炎寿命を設定しておらず、寿命設定の修正を直接検証していない。ログ: `temp/smoke-render-queue-test-vulkan-20261028.log`。Vulkan の単一 device 共有または fixture 分離、backend の明示的な検証が必要。

- 2D Fluid の既存 Composition / Render Queue 統合テスト `FluidSmokeUsesSharedStateInCpuAndGpuRenderQueue` を D3D12 で再実行し成功。offscreen Qt plugin のある `build/vcpkg_installed/x64-windows/debug/Qt6/plugins/platforms` を `QT_QPA_PLATFORM_PLUGIN_PATH` に指定した。無指定ではテスト本体に入る前に停止したが、原因の直接デバッグは未実施。このテストは炎寿命の非既定値や 3D Pyro 統合の検証ではない。

- 発生源の非対称性: 独立テストに `ARTIFACT_PYRO_SEQUENCE_VARIED_SOURCES=1` を追加。3 個の小さな発生源を時間変化させ、横方向速度と渦係数 2 を与えた 64×128×64 の D3D12 96 フレームはテスト通過したが、煙は低い位置に横広がりし、細かな煙にはならなかった。発生源の半径、密度・温度、上向き速度、渦係数も同時に変更しているため、各要素の効果は分離できていない。出力: `build/smoke-runtime-rnd/pyro-varied-sources-20261028/`。通常の単一発生源に戻した 5 フレームは、従来の高次移流キャプチャと PNG バイト単位で一致。

- 解像度比較: 独立テストの上限を 96 に拡張し、高次密度移流・96×192×96・384×768 の D3D12 96 フレームを実行。有限・非負、画面端接触なしで通過した。frame 96 の alpha > 8 は 55,563 ピクセル、重心 Y は 444.3 / 768。64×128×64 と同じ出力サイズ・構図で輪郭は滑らかになったが、煙の形状は依然として丸く、解像度増加だけでは不足。テスト時間は 45.2 秒（64 の以前の実行約 16.3 秒、各 1 回で readback を含む参考値）。発生源速度は解像度に応じて設定されるため、両条件は値の完全一致ではない。出力: `build/smoke-runtime-rnd/pyro-high-order-96-20261028/`。メモリの実測と実アプリでの確認は未実施。

- 描画密度の切り分け: GPU 3D テクスチャのサンプラーは linear filtering を使用している。独立テストの `ARTIFACT_PYRO_SEQUENCE_DENSITY_SCALE=0.25` で描画密度を既定の 1/4 にし、高次密度移流・64×128×64・384×768 の D3D12 96 フレームを比較した。テストは通過し、frame 96 の alpha > 8 は 49,235 ピクセルだったが、画像は依然として丸く滑らかで、密度倍率だけでは煙の細部は回復しなかった。出力: `build/smoke-runtime-rnd/pyro-density-quarter-20261028/`。実アプリでの検証は未実施。

- 渦強度の切り分け: 独立テストで `ARTIFACT_PYRO_SEQUENCE_VORTICITY=20`（Smoke 既定 0.18）を指定し、同じ高次密度移流・64×128×64・384×768 の D3D12 シーケンスを実行。96 フレームまで有限・非負、画面端への接触なしで通過したが、フレーム 96 は輪郭が粗く崩れ、単なる係数増加は品質改善にならなかった。出力: `build/smoke-runtime-rnd/pyro-vorticity-20-20261028/`。係数 20 の他の解像度・長時間安定性は未検証。

- 追加切り分け: 独立シーケンステストに `ARTIFACT_PYRO_SEQUENCE_DISSIPATION` を設け、高次密度移流・64×128×64・384×768 で散逸 0 を検証。D3D12 の 96 フレームまで通過し、フレーム 96 の alpha > 8 は 59,241 ピクセル（既定散逸では 57,168）。画像は依然として丸い塊で、散逸を止めるだけでは形状改善にならなかった。既定値の変更根拠にはしない。出力: `build/smoke-runtime-rnd/pyro-no-dissipation-20261028/`。

- 3D一定速度移動の独立prototypeで、既存の一次semi-Lagrangianと、前進・逆移流による補正を元fieldの近傍値に制限する方法を比較。24 step後のRMS誤差は `0.0109489 → 0.0052252`、peakは `0.682788 → 0.900436`。値は `[0,1]` 内。これは理想化した移動の比較であり、煙の品質全体を証明しない。
- `PyroSimulationSettings::highOrderDensityAdvection` を既定falseの実験設定として追加。密度のみを補正し、open boundaryで前後の追跡がdomain外なら一次移流へ戻す。近傍の最小・最大で制限し、作業fieldは `ArtifactCore::Array` を設定／domain変更時に確保して再利用する。lazy初期化または外部field resize後は最初のstepで再確保する可能性がある。温度・燃料・反応・速度は一次移流のまま。
- 実solverの24 step一定速度fixtureではRMS `0.0121406 → 0.00810148`、補正fieldは有限・非負・1以下で同条件再計算が一致。設定切替でmemory checkpointを無効化し、高次方式のdisk checkpointは `.density-high-order.pyroc` として従来ファイルと分ける。無効checkpoint、正interval復元、両方式のdisk分離を含むfocused CPU testsは4/4 pass。
- 64×128×64・96 frame・384×768の連続煙では、高次方式のD3D12 / Vulkan testは各1/1 pass、5画像はbackend間RGBA byte一致。96 frameでalpha>8 pixelは一次63,065 / 高次57,168。72→73 frameの高次画像全体RGBA RMS差は約 `1.38 / 1.38 / 1.38 / 1.48`（8-bit）。既定offの5画像は変更前の基準とbyte一致。capture: `build/smoke-runtime-rnd/pyro-advection-array-64-20261028/` と `pyro-advection-array-64-20261028-vulkan/`。
- 目視では輪郭が変わるが、煙はなお球状で渦は明瞭でない。高次密度と一次温度・燃料の整合、質量保存、closed boundary、長時間・炎・衝突、UI/Composition接続、allocator計測は未検証。追加作業fieldは64×128×64で少なくとも2 MiB、resize履歴によって容量は大きくなり得る。既存のmemory estimateはscratchを含まない。参考: [NVIDIA Real-Time Simulation and Rendering of 3D Fluids](https://developer.nvidia.com/gpugems/gpugems3/part-v-physics-simulation/chapter-30-real-time-simulation-and-rendering-3d-fluids)、[Stanford Visual Simulation of Smoke](https://www.graphics.stanford.edu/papers/smoke/)。

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

## 2026-10-28 — 自動画角の連続フレーム検査

- 独立 `PyroGpuSmokeSequenceRemainsInFrame` に、保存する5画像以外も毎フレーム GPU 描画・readback して端の alpha と重心を検査する opt-in を追加。D3D12、24×48×24、出力192×384、連続 source と矩形追従の条件で frame 24–384 の361フレームすべてが既存の端・重心条件を満たし test pass。最初の23フレーム、より高解像度、移動／停止 source、seek、しきい値以下の薄い煙、実アプリは未検証。
- この方法は frame ごとの GPU readback を伴う test-only の品質検査であり、product hot path や性能の測定ではない。自動画角は構図を変えるため production へ適用していない。ログ `temp/pyro-all-frame-long.log`。

- 連続検査を D3D12 で拡張。24×48×24・192×384 では、3つの移動 source と frame 24 で source を止める条件が、それぞれ frame 24–384 の361フレームで端・重心条件を満たした。さらに移動 source、高次密度移流、48×96×48・384×768 でも同361フレーム pass。これは alpha > 8/255 の端接触と alpha 重心の検査であり、薄い煙の切れ、個々の plume の保持、画質、seek や実アプリの性能は保証しない。停止条件では frame 384 に煙が残るため、完全消失後の挙動も未検証。ログ `temp/pyro-all-frame-moving.log`、`temp/pyro-all-frame-pulse.log`、`temp/pyro-all-frame-moving-high.log`。自動画角は引き続き test-only。

- 端の検査閾値を test-only 環境変数で指定できるようにし、同じ 48×96×48・移動3 source・384×768・高次密度移流の 361 frame を alpha > 0/255 に厳格化して D3D12 再実行、pass。8-bit readback で非ゼロと判定できる画素は端に触れなかった。RGBA16F に存在しても 8-bit 化でゼロになる薄い煙は対象外であり、画像の端を超える不可視 field の有無や seek・画質を保証しない。ログ `temp/pyro-all-frame-thin-alpha.log`。

- 低密度 volume 出力を調査。Smoke transfer は density に比例する直線 RGB と density に基づく alpha を同時に出すため、暗い背景への合成寄与は低密度で概ね density の二乗になる。均一 density 0.00005・照明無効の GPU RGBA16F 出力では straight green 0.0001498、alpha 0.001498 程度。16 / 64 / 256 steps の契約 test は D3D12 / Vulkan で pass。薄い縁が目立ちにくい要因の候補だが、実煙の視覚改善は未検証。次は単位密度あたりの散乱色を独立させた transfer を同じ solver field・背景・露出で比較する。

- 実 3D Pyro field で、Smoke transfer の密度依存 RGB と密度非依存 RGB を試作比較。D3D12、48×96×48、384×768、高次密度移流、同じ field と自動画角の frame 24/48/72/73/96 で各 focused test pass。frame 96 は後者の薄い外縁が強調される一方、格子状の段差が目立ち、煙の内部は平坦なまま。露出合わせ・複数条件・性能・候補の Vulkan 検証はしていない。画質改善と判断できないため試作 shader・公開設定は撤去。画像 `build/smoke-runtime-rnd/pyro-scatter-base-20261028/` と `pyro-scatter-albedo-20261028/`。復元後に Core と test を再buildし、低密度契約 test は D3D12 / Vulkan で各 pass。

- 前項の輪郭評価を訂正: 2枚の PNG は straight RGB と alpha を含み、alpha channel は完全一致。透明部分の RGB をそのまま画像表示した観察では段差を過大評価していた。黒背景へ alpha 合成した比較では目立つ格子輪郭は大幅に緩和され、両者とも依然として細部に乏しい丸い塊に見える。8-bit sRGB 上の簡易合成で画像全体 RGB 平均絶対差 0.733/255、最大 11/255。これは linear 合成や実 Composition の厳密な参照ではなく、候補の優劣を確定しない。候補は撤去済みのまま。比較画像 `build/smoke-runtime-rnd/pyro-scatter-base-black-096.png` と `pyro-scatter-albedo-black-096.png`。

- `PyroGpuSmokeSequenceRemainsInFrame` の capture に黒背景の preview を追加。straight sRGB PNG の各チャンネルを linear に戻して alpha を掛け、sRGB に再符号化する test-only 処理。元の透明 PNG も保持する。48×96×48 / 384×768 の D3D12 test pass、出力 `build/smoke-runtime-rnd/pyro-linear-preview-20261028/`。frame 96 の4点を独立計算と比較し最大差 0/255。8-bit readback 後の近似なので、RGBA16F の本番 composition 全体の正確性を示さない。黒背景画像でも煙は細部の少ない塊で、画質・上端・実アプリへの統合は未完了。

- solver の既存 density dissipation は各 step に固定量を引くため、薄い密度が先にゼロになる。独立 sequence fixture に、source 注入前に密度へ指数 retention を適用する実験を追加。既存 solver 内の dissipation は0とし、速度の減衰は同じ係数で fixture 側から適用する。48×96×48 / 384×768 / D3D12 / 高次密度移流の 96 frame test pass。黒背景 frame 96 の見た目は引き続き滑らかな塊で、明確な billow の改善は確認できなかった。指数減衰と固定量減衰は等量でも同等の残存量を意味せず、撮影画角も密度に追従するため、露出・密度量を揃えた比較ではない。production solver は変更しない。capture `build/smoke-runtime-rnd/pyro-exp-decay-20261028/`、基準 `pyro-linear-preview-20261028/`。

- 3D sequence fixture に XY streamfunction の curl による時変速度加算を opt-in で追加。連続式では発散ゼロ、Z 方向に強度を変え、毎 step 加速度として適用する。D3D12、48×96×48 / 384×768、高次移流、frame 24–96 の捕捉で強度 3 / 20 とも test pass。黒背景 frame 96 は 3 でほぼ滑らかな塊、20 で柱や帯状の不自然な変形になり、自然な巻き込みとは判断できない。離散発散・他 seed・固定画角／露出・長時間・性能は未検証、production solver へ適用しない。capture `build/smoke-runtime-rnd/pyro-curl-force-3-20261028/` と `pyro-curl-force-20-20261028/`。

- 周期渦の代わりに source 近傍の逆回転 Gaussian 渦ペアを test-only で追加。D3D12、48×96×48 / 384×768、高次移流、強度 20、96 frame で test pass。黒背景画像では煙が横に広がった椀状となり、自然な billow は確認できない。位置・強度・長時間・密度質量や離散発散の検証も不足するため採用しない。capture `build/smoke-runtime-rnd/pyro-local-curl-20-20261028/`。

- 文献 [Visual Simulation of Smoke](https://graphics.stanford.edu/papers/smoke/) と [GPU Gems 3: Real-Time Simulation and Rendering of 3D Fluids](https://developer.nvidia.com/gpugems/gpugems3/part-v-physics-simulation/chapter-30-real-time-simulation-and-rendering-3d-fluids) は、粗い格子の semi-Lagrangian 移流による数値拡散と渦の喪失を説明する。現実装の高次移流は密度だけで、速度は一次補間のまま。confinement は既存渦と密度で重み付けされる。実際にこれが今回の画質の主因かは未確定。
- 同じ 48×96×48 の基準 sequence に内部セルの速度 curl RMS 記録を追加。frame 24 / 48 / 72 / 96 は 0.0830 / 0.1590 / 0.2220 / 0.2678、D3D12 focused test pass。全領域平均であり、空間周波数別の損失、実物らしい流れ、数値拡散の因果は判定できない。次は渦を含む既知速度場の移流誤差を基準・高次法で比較し、発散／境界・保存量・コストを併せて評価する。

- 既知の発散ゼロ shear `v=(constant,f(x),0)` を固定速度で30 step 平行移動する独立1次元 prototype を追加。一次補間 / 近傍制限つき補正の velocity RMS 誤差は 0.178913 / 0.138271、curl RMS 誤差は 0.120676 / 0.0978156。両方の誤差改善を assert し focused test pass。これは外部固定速度による受動移流で、3D 自己移流・圧力投影・境界・保存量・性能・実煙の画質を検証しない。production velocity solver は未変更。

- 実 Pyro solver に一様 X 速度と `v_y=f(x)` の発散ゼロ shear を設定し、source・冷却・減衰・浮力・渦閉じ込めなしで30 step、内部60セルと解析平行移動を比較した診断は **失敗**。速度 RMS 1.31754（暫定閾値 0.4）、curl RMS 0.218057。有限 open domain の流入側で領域外速度がゼロとなり、圧力投影も影響しうるため、一次移流そのものの誤差と断定できない。診断テストは `DISABLED_PyroSolverAdvectsDivergenceFreeShear` として残し、通常 suite には組み込まない。大きい domain・境界条件・投影なしの対照で原因を切り分けてから閾値を決める。ログ `temp/pyro-solver-shear.log`。

- 上記診断の対照: 横96セルを固定し、縦×奥行を小 16×8／大 64×32、open／closed を変えた。velocity / curl RMS は小 open 1.31754 / 0.21806（前回失敗）、大 open 0.24697 / 0.15331、小 closed 0.22394 / 0.15105、大 closed 0.22394 / 0.15105（後3条件は暫定閾値 pass）。有限 open 境界と領域サイズの組合せが大きく影響することを示すが、個別の圧力・速度移流の寄与は未分離。open sampler は領域外位置の速度をゼロとして返す。強制一様流は開境界の流入条件と整合しない可能性があり、この fixture 単独で solver 不具合を断定しない。通常 suite では診断を引き続き disabled にする。

- 一様 X 流をゼロにした定常 shear `v=(0,f(x),0)` の30 step 対照を追加。無限領域の解析解は初期場のまま。小 open / 大 open / 小 closed の内部速度 RMS は 0.0398865 / 0.0006511 / 0、curl RMS は 0.0115191 / 0.0001107 / 0。各診断実行は暫定閾値で pass。境界距離と open/closed の影響を示すが、剪断流は Y 方向の領域端を横切り得るため、有限 open domain で解析解が厳密な境界条件を満たすわけではない。圧力と移流の分離は未実施で、診断は disabled のまま。

- 小 open 領域・一様 X 流を含む shear 診断で pressureIterations を 1 / 8 / 32 / 64 と変更。velocity RMS は 0.36455 / 1.31754 / 3.57804 / 4.19286、curl RMS は 0.15887 / 0.21806 / 0.25470 / 0.26009。暫定 velocity 閾値 0.4 は1回のみ pass、残りは fail。反復数に伴う変化から圧力処理は結果に影響するが、この無限領域の解析解と有限 open 境界の不一致があるため、反復増加そのものが不具合とは断定できない。実煙の projection 前後の発散・境界 flux と、境界条件に整合した参照が必要。診断は disabled のまま。ログ `temp/pyro-shear-pressure-*.log`。

- 実 Pyro smoke sequence（48×96×48、384×768、高次密度移流、D3D12）で pressureIterations 1 / 8 / 32 を比較し、投影後 velocity から境界を除く中心差分 divergence RMS を測定。frame 96 は 0.020886 / 0.003606 / 0.001605。各 focused test pass。3画像は黒背景でいずれも滑らかな塊で、反復増加だけの明確な画質向上は見られない。各条件は独立 simulation・自動画角であり同一 field ではない。投影前の発散、境界 flux、長時間や計算時間は未測定。以前の shear 診断と異なり、この実煙条件では反復増加で投影後の測定発散は下がったが、正しい開境界や product quality の証明ではない。画像 `build/smoke-runtime-rnd/pyro-pressure-{1,8,32}-20261028/`。

- [Wavelet Turbulence for Fluid Simulation](https://graphics.stanford.edu/~djames/publication/wavelet-turbulence-for-fluid-simulation/) は低解像度流れの不足する細部を合成し時間一貫性を保つ方法を示す。現 GPU noise は流れに沿う座標を持たない。これを検証するため、同じ合成密度を4 voxel移動し、GPU画像を32 pixel戻して alpha 差を測定する test を追加。detail off / on の平均差は 0.00104 / 2.5679（8-bit alpha、画像有効範囲平均）、D3D12 / Vulkan 一致、各 test pass。同時刻の固定 shader noise と2つの密度場を比較しており、実際の動画の違和感や wavelet 手法の品質・コストを示さない。test は detail 差が大きいままであることを合格条件にはせず、改善を妨げない。

- `GpuVolumeRenderSettings` に正規化 domain 座標の `detailOffsetX/Y/Z` を追加し、GPU noise sample を一様移動できるようにした。既定 0、非有限値は 0。shader 定数追加のみで texture の所有・同期・確保は変更しない。24³ 合成密度を4 voxel平行移動した test で、位置合わせ後 alpha 平均差は固定 noise 2.5679、既知移動量の offset を与えると 0.00166（8-bit）。D3D12 / Vulkan で各 pass。既定 offset 0 の 48×96×48 / 384×768 の5 smoke capture は変更前と RGBA byte 一致、D3D12 test pass。
- この offset は caller が与える一様移動だけを表す。velocity field による座標移流、非剛体変形、texture の歪み・再初期化、checkpoint/seek、Composition への接続、性能と実煙の動画品質は未実装／未検証。Wavelet turbulence 自体の実装ではない。Vulkan で同一 process 内に複数 renderer を連続初期化すると Diligent の複数 instance 警告と別 backend fallback が発生するため、Vulkan の offset 検証は単一 test の別 process で行った。

- 実 Pyro sequence に密度重み平均 velocity を積分して detailOffset に与える test-only 追従を追加。detailTime の独自移動は比較両条件で停止。48×96×48、384×768、D3D12、高次密度移流の固定 noise / 一様追従はいずれも focused test pass。frame 72→73 の画像全体 alpha 平均絶対差は 0.20645 / 0.20509（8-bit）。カメラ追従と実際の密度変化を含み、動き補償されていないため、差が小さいことをちらつき改善の証拠にはできない。全体平均速度は局所の回転・変形を表さず、履歴依存で seek も未検証。production caller へは接続せず、3D 座標場の移流が次の候補。出力 `build/smoke-runtime-rnd/pyro-follow-detail-{0,1}-20261028/`。

- 一様 offset を超える候補として、各 grid cell に material coordinate を保持し `PyroFieldSet::sampleVector` で移流する独立 prototype を追加。既知の剛体回転を8回適用し、内側領域の解析座標との RMS は追従 3.80e-6 cells、固定座標 0.97476 cells。CPU focused test pass。各 step の逆追跡には解析的な回転を使うため、solver velocity からの数値積分精度や境界を証明しない。実煙への接続、GPU texture upload、座標の歪み対策、checkpoint/seek、メモリ・性能、画質は未検証。

- 実 Pyro velocity を使う test-only material coordinate 移流を追加。各 step の後に得た velocity で一次逆追跡し、領域外追跡は identity に戻す。48×96×48・高次密度移流・D3D12 で density >= 0.002 の内側セルの中心差分 Jacobian determinant 最小/最大は frame 24: 0.993/1.003、96: 0.657/1.518、192: -0.033/6.43、384: -0.442/21.97。後半で局所的な反転と大きな伸縮を検出し、この単純方式をそのまま描画へ使えない。有限値と煙の存在を確かめる test 自体は pass だが、座標品質は不合格。solver density の移流が使う同一時刻の速度ではなく step 後の velocity を使う近似で、境界リセットの影響、特異値、複数 seed と seek も未検証。フィールドは fixture でのみ確保・CPU 走査し、product hot path と GPU には接続していない。ログ `temp/pyro-material-real.log`、`temp/pyro-material-real-long.log`。

- test-only material coordinate を60 step ごとに identity へ一括 reset。前回と同じ 48×96×48 の smoke sequence で、frame 192 / 288 / 336 / 384 の density mask 内 Jacobian determinant 最小/最大は 0.979/1.021、0.778/1.187、0.871/1.099、0.939/1.054。比較対象の reset なし frame 384 は -0.442/21.97。5 capture のみの有限性 test は pass だが全 frame の反転を否定できない。reset 時の座標変化 RMS は frame 61 で 2.37 cells、361 で 6.68 cells あり、描画に直接使えば不連続となり得る。実画像や連続性はまだ未検証。単一 field の reset を product に採用せず、位相をずらした複数座標場のブレンド、歪み計測、seek と性能を次に検証する。ログ `temp/pyro-material-reset-60.log`。

- 2つの test-only 座標場を同じ実 Pyro velocity で移流し、60 step 間隔で30 stepずらして交互に reset、sin² 重みで切り替える prototype を追加。座標から求めた単純な周期関数を density mask 内で比較すると、frame 60→61 の RMS 変化は単一場 reset 0.63106、二場 blend 0.02858。frame 61→62 は各 0.03004 / 0.02877。D3D12 smoke sequence test は両条件 pass。これは空間固定セルでの試験信号であり、実 GPU noise の画像、動き補償、全 reset・全 frame、座標場両方の歪み、seek や性能を検証した結果ではない。二場と作業領域は test-only で、production 描画・保存には未接続。ログ `temp/pyro-material-dual-signal-{0,1}.log`。

- 実 solver density に test-only CPU 側の座標由来の周期信号を乗じ、Diligent volume renderer に渡して reset 前後を画像化。48×96×48、384×768、D3D12、黒背景 linear 合成 PNG の画像全体 RGB 平均絶対差は frame 60→61 が単一場 2.401 / 二場 0.151、61→62 が 0.172 / 0.159（8-bit）。双方 focused test pass。煙の実移動・自動画角も含む差で、動画品質・動き補償・複数シーンの保証ではない。画像は両方とも滑らかな塊で、画質改善は未達。CPU 側で一時 density field を作る実験であり GPU の座標場実装・production path ではない。出力 `build/smoke-runtime-rnd/pyro-material-render-{0,1}-20261028/`。

- test-only 二場座標を trilinear 補間し、密度と周期 signal を各軸2倍の 96×192×96 field で評価して GPU volume に送る比較を追加。48×96×48 simulation、384×768、D3D12、reset frame 60/61/62/90/91 の test pass。黒背景画像では元の粗い field を使う場合と比べてもなお滑らかな塊で、目視上の明確な細部改善は確認できない。frame 60→61 / 61→62 の RGB 平均差は 0.146 / 0.155（8-bit、動き補償なし）。単純な周期信号1種・一条件であり、より良い turbulence の可能性を否定しない。CPU 上の密度・温度配列は各 7,077,888 bytes、GPU upload・生成時間と実アプリ性能は未計測。production に未接続。capture `build/smoke-runtime-rnd/pyro-material-upsample-20261028/`。

- 高解像度 test-only material field の周期信号に代え、2スケールの滑らかな value noise を追加。48×96×48 solver、96×192×96 描画用 field、384×768 出力、D3D12 で test pass。frame 60→61 / 61→62 の黒背景 RGB 平均差は 0.1345 / 0.1465（8-bit、動き補償なし）。画像は前の周期信号より均一で、煙の巻き込みや細部の明確な改善は見られなかった。1 seed・1条件、CPU noise とアップロードのコスト未計測、実 GPU noise や wavelet turbulence とは異なる。production 未接続。出力 `build/smoke-runtime-rnd/pyro-material-noise-20261028/`。

- 高解像度 material 実験に、同一 frame の元密度と detail 密度の中央断面を同じ固定倍率で記録する test-only 出力を追加。D3D12 focused test pass。frame 61 の断面における density 差の基準 RMS 比は 0.1474、frame 90 は 0.1619 で、入力 field の変調は非ゼロ。断面は変化する一方で最終黒背景 volume 画像は滑らかな塊のまま。raymarch 中の積分・transfer・光のどれが細部を抑えるかは分離できていない。初回の高倍率 slice は白く飽和したため、最終出力は倍率1に修正。capture `build/smoke-runtime-rnd/pyro-material-slice-final-20261028/`。画像は8-bit診断、実アプリの評価ではない。

- raymarch段数と照明の寄与を切り分けるため、sequence test に段数・lighting の test-only override を追加。同じ実 solver sequence / 高解像度 material field / 384×768 / D3D12 を照明なしで1段と96段レンダーし、focused test は両方 pass。96段は1段の粗い断層サンプルによる濃淡ムラを減らすが、両方とも滑らかな塊で細部は見えず、96段画像も既存の照明あり96段画像と比べて大きな形状差はない。従って照明オフや積分段数増加だけでは元の「低解像度・上部への集中」および生きた細部の問題を解決しない。比較画像 `build/smoke-runtime-rnd/pyro-integral-unlit-{1,96}-20261028/`、ログ `temp/pyro-integral-unlit-{1,96}.log`。段数1は診断用で品質条件ではなく、定量画像差・別カメラ・複数 seed は未評価。test-only override は production 描画に未接続。

- solver 格子そのものを48×96×48から96×192×96へ上げ、同一のdomain/source設定・D3D12・96 frame・512×1024出力・96 ray stepsで比較。両方focused test pass。96格子は輪郭の格子感が少し減ったが、両画像ともディテールのない滑らかな一塊で、96格子側は可視プルームも小さく、上部に細部が増えることはなかった。実行時間は48格子10.7秒、96格子48.7秒（各1回で環境負荷未統制、厳密な性能比ではない）。この品質差だけを理由にsolver格子を倍増する根拠は不足する。画像 `build/smoke-runtime-rnd/pyro-grid{48,96}-512-20261028/smoke-096-black.png`、ログ `temp/pyro-grid{48,96}-512-20261028.log`。単一seed・単一時刻のR&Dで、Production renderer / Composition接続ではない。

- 飽和・内部陰影の仮説を調べるため、48×96×48 solver・同一source/sequence・D3D12・512×1024・96 ray stepsでdensityScale 0.15 / 0.30 / 0.60を比較し、基準1.0も同設定captureと比較。各focused test pass。scaleを下げると全体が薄くなるだけで、球状のsmoothな形と内部detail不足は残る。したがって現象は単純なdensityScale過多だけでは説明できず、scaleを下げる変更は解決策として採用しない。画像 `build/smoke-runtime-rnd/pyro-density-scale-{0p15,0p30,0p60}-512-20261028/`。単一seedの定性的比較であり、別のdensity transfer・lighting・解像度に一般化しない。

- sourceの継続注入が上側の密集に寄与するかを見るため、同一48×96×48 sequenceでframe 24以降にsourceを止め、frame 96で連続注入と比較。D3D12 focused test pass。停止後は総量と見かけの面積が下がるが、球状の大きな塊と細部不足は残り、連続注入だけが原因ではない。単一source停止時刻の試験で、source停止後もdensity/temperatureがどう輸送・減衰するかを含むため、上端境界やsource形状を個別に証明しない。画像 `build/smoke-runtime-rnd/pyro-source-stop-24-512-20261028/`、対照 `build/smoke-runtime-rnd/pyro-grid48-512-20261028/`。

- sequence testにtest-only `detailStrength` / `detailScale` overrideを追加し、既定相当の0.7/12と最大強度の1.0/scale24を同一48×96×48 simulation・512×1024出力で比較。VS 18既存MSBuild targetで再build成功、D3D12 focused test各1/1 pass。frame 24/96とも目視では大形状は同じ滑らかな塊で、最大設定でもproduction相当の細かなbillowは出ない。noise detail単独の強度上げは解決策として不十分で、既定値は変更しない。capture `build/smoke-runtime-rnd/pyro-detail-{0p7,1p0}-512-20261028/`、log `temp/pyro-detail-{0p7,1p0}-20261028.log`。視覚比較のみで、GPU shader時間とbackend差は未測定。

- test-only localized counter-rotating curl forceを0.75 / 2 / 4 grid-cell/s²で比較。実Pyro sequenceの48×96×48、512×1024、96 frames、同一seed/source、D3D12で3条件focused test pass。0.75は基準形状に近く明瞭なbillowを作らず、2 / 4は水平拡散・扁平化を強めるが、視覚的な細部は増えず上方向の柱を弱める。単純な局所循環力の増加は目標から外れるためproductionへ採用しない。画像 `build/smoke-runtime-rnd/pyro-local-curl-{0p75,2p0,4p0}-512-20261028/`、log `temp/pyro-local-curl-{0p75,2p0,4p0}-20261028.log`。速度場統計・発散・計算負荷は未測定。

- test-only curl-noise prototypeを追加。3つのseed分離された3D value-noise potentialを2周波数で事前生成し、その離散curlをdensityのあるセルへ加算してから実Pyro stepの圧力投影に通す。48×96×48 / 512×1024 / 96 frames / D3D12でstrength 1.5、3.0を各focused testしpass。1.5では平滑な塊が少し揺らぐ程度、3.0では帯状の歪み・内部の濃淡斑が出るがcoherentなbillowにはならない。密度mask付きcurlはそのままでは境界で発散し得るため、smooth density envelopeも追加比較したが大きな視覚差はない。安定した品質改善がないので製品には接続しない。画像 `build/smoke-runtime-rnd/pyro-curl-noise-{1p5,3p0}-512-20261028/` と `pyro-curl-noise-smooth-3p0-512-20261028/`、ログ `temp/pyro-curl-noise-{1p5,3p0}-20261028.log`、`temp/pyro-curl-noise-smooth-3p0-20261028.log`。
- このprototypeは[Wavelet Turbulence for Fluid Simulation](https://graphics.stanford.edu/~djames/publication/wavelet-turbulence-for-fluid-simulation/)の実装ではない。同研究の概要にある低解像度場から不足する高周波エネルギーを見積もる処理も、流れと整合する時間コヒーレンスも実装していない。今回の固定potentialを使う力場は乱流の見た目や連番品質を確認する予備実験であり、同論文の品質を示すものではない。

- 高解像度 material density の振幅をtest-onlyで0.8から2.0へ変更し、必要なら各frameの総densityをbase fieldに合わせる正規化を追加。48×96×48 solver、2倍field (96×192×96)、D3D12、384×768、frame 60/61/62/90/91で両条件focused test pass。frame 61中央断面のbase比RMSは0.1474→0.3717、frame 90は0.1619→0.4053へ上がり、画像輪郭は少し不規則になる。mass normalization係数はframe 60〜91で1.023〜1.026であり、総密度を約2.3〜2.6%補正すれば同程度の差を保てる。ただし目視では依然として一つの球状塊で、細かな煙の巻き込みや繊維状billowは作らない。この振幅はtexture-like modulationであり、solverの新しい周波数帯を解いた証拠ではない。画像 `build/smoke-runtime-rnd/pyro-material-strength-{0p8,2p0}-20261028/`、mass-matched `build/smoke-runtime-rnd/pyro-material-strength-2p0-massmatched-20261028/`、log `temp/pyro-material-strength-{0p8,2p0}-20261028.log` / `temp/pyro-material-strength-2p0-massmatched-20261028.log`。productionへは未接続。

- detail fieldと最終raymarch画像の差を直接見るため、strength 0の同一高解像度fieldを別processでbase captureし、strength 2 / mass normalizedとblack-background outputをオフライン比較。最終画像のluminance Laplacian RMSはframe 60/61/62/90/91でbase 0.706/0.702/0.710/0.775/0.775に対し、isotropic detail 0.709/0.710/0.713/0.802/0.804（+0.4〜3.7%）。中央断面では+37〜41%の変調がある一方、最終像に残る高周波成分はごく小さいことを示す。frame 61の画像RMS差は6.115/255。テスト側の複数renderer再利用はDiligent mutable binding assertionを起こしたため破棄し、基準とdetailを個別processで取得した。
- 視線方向のnoise相関長を伸ばすtest-only anisotropic sweep (detail Z scale 0.35 / 0.60、strength 2、同じ2倍高解像度/dual material reset/mass normalization) を実施し、各focused test pass。Z=0.35はluminance Laplacian RMSのbase比をframe 61で+3.9%に上げるが、frame 90ではisotropicと同じ+3.5%に留まり、画像は同心状の帯と分離した塊に歪む。Z=0.60の増加はframe 61 +0.7%。mass normalization factorはZ=0.35で0.90前後、Z=0.60で0.79〜0.81となり、異方性の変更が素のnoise分布を大きく変える。画像 `build/smoke-runtime-rnd/pyro-material-strength-0p0-20261028/`、`pyro-material-zscale-{0p35,0p60}-20261028/`、logs `temp/pyro-material-strength-0p0-20261028.log`、`temp/pyro-material-zscale-{0p35,0p60}-20261028.log`。視点依存の歪みはproductionへ採用しない。

- 論文のwavelet turbulenceを実装したと誤認しない診断として、capture frameだけで速度を段階的に2×2×2平均し、各段で失われる密度重み付き速度分散を測るtest-only dyadic block estimateを追加。48×96×48、D3D12 focused sequence test pass。frame 96ではband 0〜4の分散が0.043/0.637/4.373/3.296/1.184、各段のdensity weightは0.100/0.802/6.416/51.328/410.620。粗いバンド側の変動が強く、細かい帯域を足すだけでは最終像の細部不足を説明しきれない可能性がある。ただしこれは直交wavelet係数でもエネルギースペクトルでもなく、単純な局所平均差の指標。複数seed・時刻・解像度で再計測し、density transportとrendererのどこで視覚detailが失われるかを分けて調べる。source: [Wavelet Turbulence for Fluid Simulation](https://www.cs.cornell.edu/~tedkim/WTURB/wavelet_turbulence.pdf)。log `temp/pyro-dyadic-diagnostic.log`。


## 2026-10-28 smoke billow品質のDCC比較観点

- SideFX公式Pyro Solver (Sparse) / Pyro look development資料を確認。煙のbulk形状を決めた後、広域のTurbulence、小規模な輪郭破壊を行うDisturbance、速度方向を保ったまま乱すShreddingを役割別に組み合わせる。Disturbanceは滑らかなcapを崩す用途、Shreddingは火炎の縦lickを崩す用途として説明される。
- 現行 `PyroSimulationSettings` / `PyroSimulation.cpp` で確認できるshape forceはvorticity confinementであり、上記3種の独立field/force operator、sparse active-tile simulationは確認できない。`PyroFieldSet` は全domainのdense arraysを保持する。したがって高解像度化は計算量・メモリ量をdomain全体で増やし、既存の2D 128格子を直接拡大した比較で約3.6倍のwall timeとなった傾向とも整合する。
- これはHoudiniの挙動をそのまま移植する提案ではなく、煙・炎の形状形成で分けるべきR&D軸を定義するもの。製品適用にはArtifact固有の決定性、frame seek、hot-path allocation、Diligent backend契約に沿った実装・計測が必要。
- 次の比較は、同一source積分量・初期場・frame・camera・exposureで (1) bulk flow only、(2) resolution-scaled large-scale turbulence、(3) density-edge-limited disturbance、(4) fuel/reaction-gated shredding を評価し、density centroid/上端比/mass drift、炎反応領域、連続画像差、CPU/GPU時間を記録する。まずstandalone Pyro renderer内で検証し、品質・負荷・決定性の条件を満たしてからComposition integrationへ進む。
- 参照: [SideFX Pyro Solver (Sparse)](https://www.sidefx.com/docs/houdini/nodes/dop/pyrosolver_sparse.html)、[SideFX Pyro look development](https://www.sidefx.com/docs/houdini/pyro/lookdev.html)、[SideFX Sparse vs dense simulations](https://www.sidefx.com/docs/houdini/pyro/sparsity.html)。

## 2026-10-28: Smoke + Fire shared-field integration probe

- `tests/Artifact/ClonerGpuRenderContractTest.cpp::PyroFireAndSmokeShareOneSimulationVolume` を追加。単一の実 `PyroSimulation` Fire profile から density / temperature / reaction を GPU volume renderer へ渡し、`FireAndSmoke` 出力と同じfieldのSmoke-only出力を比較する。source emissionは48 fixed steps、gridは24³、capture先は `ARTIFACT_PYRO_FIRE_SMOKE_OUTPUT_DIR`。
- このテストは炎と煙が同じPyro fieldから同時に成立し、反応がある場合に混合transferがSmoke-onlyと異なることを確認する独立GPU R&D。Composition layer、seek再現、レンダーqueue統合、性能の証拠ではない。
- ユーザーはこの turn で CMake configure / build / test の実行を明示的に保留した。新テストはソースに追加したが、実行結果なし。次の実行時に現在のtargetをconfigure/buildしてfocused testを回す。
- `tests/Artifact/ClonerGpuRenderContractTest.cpp::PyroSmokeDistributionSweepReportsResolutionAndVerticalSpread` を追加。Smoke profile、24³ open domain、source emission時間、GPU camera/outputを固定し、baseline / buoyancy低下 / source下方配置 / 両方+smoke turbulenceの4条件でdensity mass・Y重心・上下1/3質量比・GPU画像を記録する。capture環境変数は `ARTIFACT_PYRO_DISTRIBUTION_OUTPUT_DIR`。
- 同スイープの出力画像計測にalpha重心、上/中/下alpha比率、alpha閾値以上の画素数を追加。QImageの行0が画像上端である順序に統一し、top/middle/bottomを逆順に報告しないよう修正した。ソースのみで、configure/build/testは未実行。

## 2026-10-28: Existing Pyro lookdev reviewed against user quality concern

- 既存capture `build/smoke-runtime-rnd/pyro-sim-gpu-20261027-d3d12-48f/pyro-simulation-smoke.png` と `.../pyro-simulation-fire.png` はどちらも小さなほぼ球形のvolume。sequence capture `build/smoke-runtime-rnd/pyro-sequence-framed-20261028-d3d12/smoke-096.png` も平滑なcap/columnで、細かいbillowは見えない。これらは既存R&D画像の目視確認で、新しいテスト実行ではない。
- `GpuVolumeRenderer::render` は毎renderでdensity・temperature・reactionの全3D R32F textureを `UpdateTexture` する。密格子の増加はsolver計算・メモリに加え全field upload量も増やすため、Production化では高解像度化だけでなくsparse/adaptive domainかGPU resident simulationを性能比較ゲートに含める。
- このターンはCMake・build・testを実行していない。既存画像の分析は新規実行証拠ではない。画質改善・distribution sweepの効果、本番Composition経路は未検証のまま。
- テストはまだ実行していないため、どの条件が上寄り・見た目を改善するかは未確認。ソース解像度は全条件24³固定であり、高解像度化の評価ではない。次回は同じテストをD3D12/Vulkanで実行し、数値と画像の両方を見てからノブを選ぶ。
- `tests/Artifact/ClonerGpuRenderContractTest.cpp::PyroVolumeRenderResolutionSweepUsesTheSameSimulationField` を追加。24³の実Smoke Pyroを一度だけ進め、同じdensity/temperature/reaction fieldを固定transfer・camera・ray stepで128²/256²/512²に描く。alpha mass、normalized centroid、visible pixels、alpha gradient energyを記録し、output samplingとsolver gridを分離する。capture環境変数 `ARTIFACT_PYRO_RESOLUTION_OUTPUT_DIR`。現時点では未実行。
- solver解像度そのものを分ける `PyroSmokeSolverResolutionSweepUsesMatchedPhysicalSetup` も追加。world bounds・source位置/extent・物理時間0.8秒・出力384²を揃え、24³と48³を比較する。既存pressure sweepでは24³で8反復の発散RMSが約0.054、48³では8反復で約0.192、36反復で約0.067だったため、projection差が解像度差へ混ざらないよう反復数を8/36にする。これは旧R&D測定から選んだ初期値で、今回の各profileでも発散RMSを再計測する。source velocity profileとbuoyancyは格子幅に比例させ、物理空間の速度・加速度を一致させる。格子幅2倍では時間刻みを半分、substep数を2倍にして1 substep当たりのセル移動量を合わせる。上昇要因はbaseline、buoyancyのみ0.5倍、source velocityのみ0.5倍の3条件に分け、形状ディテール候補のcurl turbulence条件を加えた計4条件を各解像度で比較する。raymarchもsolver解像度に比例して48/96 stepsとし、solver格子より粗い一定ray samplingが比較を隠さないようにする。密度massはvoxel volumeで積分し、物理密度重心・上下三分割mass・閾値以上のY範囲・各面境界帯mass、境界を除いた投影後速度の発散RMSと、GPU output alpha分布・可視画素三分割を併記する。これにより密度場の上端集中、Open境界からの流出傾向、pressure反復不足、投影/transfer/raymarch後の画面上の偏りを分けて評価する。solver時間・renderer/readback総時間・field memory estimateも記録する。capture環境変数 `ARTIFACT_PYRO_SOLVER_RESOLUTION_OUTPUT_DIR`。ソースのみで未実行のため、品質・コスト結果は未確定。
- centroid計測の座標向きを統一。Pyro density Yはbottom=0/top=1、QImage rowはtop=0/bottom=1なので、distribution / resolution sweepのGPU alpha重心を反転し、density centroidと同じworld-up normalized Yとして出す。上下shareは引き続き表示画像のtop/middle/bottom順で別記する。
- test sourceの停止条件を修正。`PyroSimulation::setSources` はsource stateを所有配列へコピーするため、ローカル変数の `source.enabled` 変更だけではsolver内のemitterは停止しない。solver-resolution sweep、distribution sweep、同じfieldをrender output解像度だけ変えて描くsweepの各stepで更新済みsourceを再設定し、設定したemission duration後に実際に停止する条件へ直した。これらの該当スイープは未実行なので、以前に記録したcaptureから停止挙動を推定しない。
- 既存capture再確認（新規実行ではない）: 48³単フレーム `build/smoke-runtime-rnd/pyro-resolution-48-384-20261028/pyro-simulation-smoke.png` は小さな滑らかな球状塊、D3D12 96-frame sequenceの終端 `build/smoke-runtime-rnd/pyro-sequence-framed-20261028-d3d12/smoke-096.png` は柱と滑らかなcapで、いずれも細かなbillowがない。コードではSmoke profileが `vorticity=0.18` を設定する一方、`smokeTurbulence` は既定値0のまま。従って単純な解像度増加だけでは形状ディテールを作れない可能性が高い。比較コードは同一source積分量・物理時間・camera・transferのまま、baseline / low-buoyancy / slow-source / solver curl turbulenceの4条件を解像度ごとに測る。turbulenceは格子解像度に比例したstrength/scaleで物理domain上の波長を揃える。GPU detail modulationはEulerianの見た目操作なので、先にsolver速度field由来の時間連続性を評価する。現時点では画像の再閲覧とsource inspectionのみで、追加build/testはしていない。
- 2026-10-28 vertical visibility diagnostic: `SmokeDepthVaryingVolumeRaymarchOnDiligentRenderer` now measures alpha-covered fraction and covered-pixel mean luma separately for top/middle/bottom thirds on its same-field `frontLit` and `frontAmbient` captures. This distinguishes density distribution from render-space visibility and makes an upper cap disappearing into low contrast visible in the R&D log without asserting that ambient fill changes plume shape. Existing PNG capture names are unchanged. Source only; this diagnostic has not been built or run.
- Read-only measurement of the existing `gpu-2d-field-d3d12` and `gpu-2d-field-vulkan` front captures (384×288) found top/middle/bottom alpha coverage of 10.753% / 49.365% / 11.182% for `front-lit`, identical to displayed precision for `front-ambient`. Covered-pixel mean luma was 0.39335 / 0.50867 / 0.43593 for D3D12 front-lit and 0.37794 / 0.49701 / 0.43941 for front-ambient; Vulkan differed by less than 0.00002. In this stored lookdev image, visible volume is concentrated in the middle third and the ambient variant does not expand its footprint or brighten the upper third. This does not establish the distribution of a live Composition render; it indicates that exposure alone is not a demonstrated fix for the lookdev capture. Measured directly from existing PNGs; no test or renderer was run.
- 同じ保存済み出力の24³ / 48³比較: どちらも384²出力だが、24³画像は球状domainを段状に感じるほどsolver sampleが少なく、48³では輪郭が滑らかになる一方で球状の塊のまま。これはscreen output pixelsではなくsolver gridが主な解像度上限であることを示す。計算量の概算では、各軸2倍で密field memoryは8倍、同じphysical durationを保つためsubstepも2倍にする場合はstep当たりvolume workがさらに2倍となり、おおむね16倍に増える（実測benchではない）。従って96³以上への単純なdense拡大はproduction解像度の解決策として未確立であり、sparse/adaptive domainまたはGPU-resident simulationの性能評価が必要。
- 保存済み2D production captureの比較（新規実行ではない）: 128×128同一密度場を768×432で描画した `volume-detail-rnd-20261028/detail-0p0/production-smoke-frame-60-768x432-grid-128x128-outflow-30.png` と `frequency-0p42-strength-0p9/...png` を比較。detail無効は滑らかなドーム、frequency 0.42 / strength 0.9 は外縁に細かな凹凸を見せるが、記録値のdensity centroidYは両方0.687957、upper-third mass shareは両方0.0484345で、ソルバー分布は変化しない。したがってdetail modulationは低解像度の輪郭改善候補で、上側の分布問題の解決ではない。captureは2D production pathの歴史的実験であり、現在追加中の3D Pyro sweepの結果とは混同しない。
- 2D production 768×576 captureのpixel解析では、上1/3/中1/3/下1/3の輝度寄与が約0.306/0.366/0.328と全体量は極端に上偏重ではない一方、目視で上部capは背景に近い暗さで中央の細い柱が目立つ。既存 `drawSmokeVolumeField` はambientFill=0.30固定で `illumination=ambientFill+(1-ambientFill)*lightTransmittance`。一時的に0.55への変更を検討したが、Insightにある同field ambient sweepは主に明るさをremapしplume形状を変えないため、本番変更は保持しなかった。
- Insightの既存2026-10-26 ambient sweepは、同一fieldでambient fill .18/.30/.42/.55に対しmean gray 27.12/32.17/37.17/42.54、高周波指標3.147e-5/4.253e-5/5.563e-5/7.217e-5と変化する一方、目視では主にglobal contrast remapでbroad plume構造は変わらないと報告。今回の本番2.5D shaderへ未検証のambient変更を適用しない判断とした。
- 追加照合: 2D `advanceLayerSmokeRuntime` の上部帯処理は各solver substepでdensityとflame lifetimeを指数減衰するが、FluidSolver2D自体の速度境界は閉じたまま。既存top sponge実験では6/12セルで可視pixel 43,466→35,993/29,092となり、12セルは上端を消す一方で画素面積も大きく減った。render fadeはbillow capを暗く/切り詰める。密度をさらに捨てる策は採らず、本質対策はpressureと整合するopen-top velocity boundaryのtest-only prototype。ただしsolverはArtifactCore child repoであり、明示依頼なしの編集は禁止。

## 2026-10-28: Production integration boundary audit

- Existing ownership: `ArtifactCore::PhysicsSystem` already has a `LayerID -> PyroSimulation` registry and advances registered Pyro simulations in its fixed-step path behind `ARTIFACT_ENABLE_PYRO`. `ArtifactAbstractLayer` has editable Pyro component values, but current Artifact layer code has no create/get/unregister call for that registry. Do not add a second authoritative Pyro simulation to `LayerFluidRuntimeState`; first bridge the existing PhysicsSystem owner and define frame-seek/settings-invalidation behavior against its checkpoint API.
- Existing render boundary: `ArtifactIRenderer::drawSmokeVolumeField` consumes 2D density/flame arrays. The separate `GpuVolumeRenderer` consumes 3D density/temperature/reaction fields and returns a borrowed output SRV, but no layer/composition code currently consumes it. Its output may be overwritten on the next render and becomes invalid on resize/destruction, so an integration must composite it into the active layer target in the same ordered render pass and keep renderer resources device-scoped.
- First vertical slice should be limited to: (1) a LayerID bridge from the Pyro component to PhysicsSystem registry, including unregister and parameter/domain reset; (2) deterministic mapping from composition frame/in-point to Pyro fixed steps with backward/random seek replay and per-layer independence; (3) an Artifact renderer entry point that renders the existing Core field with `GpuVolumeRenderer` and composites its SRV at the layer's normal Z-order; (4) identical field/frame inputs for Composition preview and render queue. Preserve the current 2D smoke path and software fallback until the GPU path has parity evidence.
- No third-party solver code is selected or copied: the current Pyro solver is the simulation donor and SideFX feature roles remain design references. The unresolved production gates are 3D active-domain/resolution cost, GPU upload or residency cost, D3D12/Vulkan output parity, frame-seek determinism, composition ordering, and live viewport image quality. The independent GPU sweep and the vertical-slice integration tests remain unrun under the user's explicit instruction not to configure/build/test yet.
- 2026-10-28 R&D instrumentation: the distribution and solver-resolution sweeps now measure the same output image's alpha-weighted RGB-luma centroid and top/middle/bottom shares, in addition to solver density and output alpha. This distinguishes density mass, visible opacity, and brightness placement when analyzing the upper-heavy appearance. Source-only; no build or test was run.
- A focused GPU R&D target, `ArtifactPyroSmokeLookdevRAndTest`, now isolates the 24³/48³ matched-physical smoke matrix from the much larger Cloner GPU contract executable. It compares baseline, lower buoyancy, lower source, curl shaping, and combined shaping; logs solver density bounds / six boundary-band shares / projected divergence / raw output alpha / alpha-weighted RGB luma distributions plus simulation and render/readback times; and optionally writes sixteen captures to `ARTIFACT_PYRO_LOOKDEV_OUTPUT_DIR` under the current eight-profile-per-grid matrix. It asserts the actual domain resolution and keeps the whole emitter extent inside the domain so a mislabeled grid or clipped-emission mass cannot confound the comparison. This is target/source registration only: CMake regeneration, build, and execution remain deferred under the user's instruction.
- Revisited existing captures, without running tests: the 2D 384×216 smoke retains a thin center column that curls into a broad upper loop; the 48³ Pyro image has a smoother edge but remains a compact sphere. Thus output/grid resolution and a single smooth curl field do not yet create the requested billowy structure. `PyroSimulationSettings` now has opt-in `smokeTurbulenceOctaves` (default 1, maximum 4); each additional smoke band halves the wavelength and amplitude, and bands below four cells are skipped. Flame forcing and the default single-band result remain unchanged. The focused Core test checks octave clamping, changed density/velocity, setting-hash separation, and repeatability.
- Corrected the 2D production R&D matrix after checking coordinate mapping: normalized emitter Y maps directly to increasing grid Y (downward), so `.84` is above `.90`, not lower. The earlier source-position factorial was mislabeled and its directional assertions are not valid evidence of moving the source lower. The CPU and Diligent matrices now hold source Y at `.90` and compare upward emitter velocity `-.75` vs `-.45` against top absorption bands `.30` and `.35`; metrics/captures include solver distribution and rendered coverage/luma. The Diligent capture also counts warm pixels to flag loss of the flame component as lift speed changes. The previous source-position fixture must not be used to infer results. Changes are source-only and still unrun.
- Refined `ArtifactPyroSmokeLookdevRAndTest`: the standard .36-domain wavelength now has a direct one-octave vs two-octave pair at turbulence strength 3 (scaled by grid resolution); the multi-octave cases also test .24 and .48 wavelengths. Strength 3 follows an earlier 40³ study where it changed upper-third distribution while preserving more total density than strength 9; this is only a candidate starting point, not a production recommendation. Baseline, lower-buoyancy, lower-source, and combined controls remain. The fixture writes eight profiles per grid (16 captures total) and logs strength, scale, octave count, rendered alpha-gradient RMS, and alpha high-pass RMS. These frequency metrics can also rise with grain, so they must be judged with captures and temporal continuity. At 24³ the .24 case's added band falls below four cells and is skipped, which explicitly tests the grid cutoff against 48³. No settings defaults are changed; source remains unconfigured, unbuilt, and unrun.
- Added a standalone test-only, mass-normalized smooth multi-scale coordinate warp of the actual 128² production solver density at the 60-frame component profile. The 0/.7/1.0 sweep reports total mass, upper-third share, visible pixels, high-pass energy, and optional captures. It does not alter the runtime renderer or settings; visual and temporal results remain unknown until the test run is authorized.
- Recomputed luma metrics from saved frame-95 GPU captures at 768×432, outflow 30/s: 128² upper/middle/lower thirds `.2361/.3837/.3802`, 82,812 visible pixels, centroid `.5571`; 256×144 `.3885/.3309/.2807`, 80,069 visible pixels, centroid `.4711`. The higher grid gives a more legible broad roll but concentrates substantially more brightness in the upper third. At frame 96 on 128², outflow bands 0/15/30/60 give upper-third shares `.4526/.2652/.2335/.2066`, with visible pixels 138,705/88,882/81,921/77,869. This shows the current absorption control trades upper brightness against overall visible smoke. These calculations analyze saved captures only; they are not fresh test results or live viewport evidence.
- No quality or cost result is known until the explicit execution gate is lifted. The four-cell minimum applies only to added higher-frequency bands; the original base band still runs below that scale for compatibility, so aliasing at a very small base scale remains a possible user-configured condition.


## 2026-10-28: Experimental density-edge smoke disturbance

- `PyroSimulationSettings` now exposes default-off `smokeDisturbance` and `smokeDisturbanceScale`. When enabled, a short-wave curl force is weighted by smoke density and a smooth density-gradient edge gate. It changes velocity only; it does not directly inject or remove density. This is an Artifact implementation inspired by SideFX's documented Disturbance role, not copied code and not a full Houdini-equivalent operator.
- The operation reuses existing density and velocity fields and allocates no per-step scratch array. The enabled path adds density-gradient samples and curl evaluation per cell; CPU cost is unknown until the standalone comparison is run. Settings clamp and checkpoint invalidation include both parameters, and `hashSettings` includes them for deterministic cache identity.
- The standalone smoke lookdev comparison now includes baseline versus edge disturbance, same-settings replay, image captures, density/hash distinction, and parameter clamping. Test source only; no configure, build, or test execution was authorized, so compilation, measured image quality, performance, and cache replay remain unverified.
- Visual recheck of saved D3D12 captures confirms the user's production-quality concern: the 48³, 384×384 Smoke profile after 48 steps is a small, nearly spherical blob (`build/smoke-runtime-rnd/pyro-resolution-48-384-20261028/pyro-simulation-smoke.png`); the 48×96×48 sequence at frame 96 is a compact lower-center column/cap (`build/smoke-runtime-rnd/pyro-smoke-sequence-20261028-d3d12/smoke-096.png`). The matched 24³ vs 48³ captures have similar silhouette and scale, so current evidence does not support resolution alone as the cause. Code inspection shows the 48³ lookdev runs 0.8 seconds total and injects source velocity for the first 0.4 seconds; `PyroSourceState::velocity` is injected into fluid cells, not an animated emitter transform, and the test's emission window overrides the profile's 2-second duration. The 48³ sequence capture after 96 frames is a separate setup, not the same source/time condition. Thus the saved images confirm compact morphology but do not establish upper-boundary accumulation as its cause. Next compare sustained emission and longer evolution at matched physical times, logging density/luma vertical shares, bounds, and measured cost before changing resolution or production defaults. This is read-only capture/source analysis; no build or test was run.
- Extended `ArtifactPyroSmokeLookdevRAndTest` with two sustained-emission controls at both 24³ and 48³: baseline and standard two-octave turbulence, each simulated for 120 frames (2.0 seconds) with emission active throughout, matching the Smoke profile's source duration. Existing eight short-pulse controls remain unchanged. Logs now include simulation/emission frames and physical duration, with the existing density/output-alpha/brightness vertical distributions, bounds, memory, and elapsed-time measures; expected output grows from 16 to 20 PNGs. This directly tests whether the compact look is primarily a short-pulse artifact while preserving a turbulence comparison. Source-only; configure/build/test have not been run, so timing, visual quality, and target timeout remain unverified.
- References: [SideFX Pyro Solver (Sparse)](https://www.sidefx.com/docs/houdini/nodes/dop/pyrosolver_sparse.html) and [SideFX Pyro look development](https://www.sidefx.com/docs/houdini/pyro/pyro_look.html). They describe separate shape controls; they do not establish parity of this implementation.
