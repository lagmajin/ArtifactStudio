# MILESTONE: 独立ノイズレイヤー（ArtifactNoiseLayer）

**最終更新:** 2026-09-01

**ステータス:** In Progress（コア実装・詳細設定付き作成導線・プリセット選択 UI・Automation API・GPU compute 経路・数値プロパティのキーフレーム評価を実装済み。runtime parity とタイムライン表示整備は未完了）

**識別子:** M-NOISE-LAYER

> **経緯:** `MILESTONE_SOLID_LAYER_NOISE_FILL_2026-08-18.md`（M-SOLID-NOISE）の「平面 Fill 拡張」方式を 2026-08-24 に方針変更し、独立レイヤー方式へ移行。平面側実装は revert 済み。`resolveLayerSourceOverride()` 経由の生成アーキテクチャはソースコンポーネント化（Insight.md 2026-08-24 参照）と整合する。

## 目的

`ProceduralTextureGenerator`（ArtifactCore）をソースとして手続きノイズ画像を生成する独立レイヤーを実装する。ノイズ生成アルゴリズムは新規実装せず、既存資産の露出に責務を限定する。

## 実装済み（2026-08-24）

1. `LayerType::Noise = 30` 追加
2. `Artifact.Layers.Noise` モジュール（`include/Layer/ArtifactNoiseLayer.ixx` / `src/Layer/ArtifactNoiseLayer.cppm`）
3. `resolveLayerSourceOverride()` override による `ProceduralTextureGenerator::generate(settings, ImageF32x4_RGBA&)` 直生成（QImage 経由ゼロ）。パラメータ署名キャッシュ、グレースケール→colorA/B カラーマッピング付き
4. draw(): ソース override 経由でバッファオーバーロードの `drawSpriteTransformed` + cloner effect + fracture overlay。単一色 fast path の概念は無く常にテクスチャ経路
5. シリアライズ: `type=30` + `noiseWidth/Height` + `noise` オブジェクト（kind は安定文字列 `"perlin"` / `"simplex"` / `"fbm"` / `"voronoiDistance"` / `"voronoiCell"` / `"voronoiEdge"` / `"white"` / `"value"` / `"gradientLinear"` / `"gradientRadial"`、未知 kind は Perlin フォールバック）
6. プロパティ露出: レイヤー固有グループ `Noise`（`noise.*` パス、kind/seed/scale/offset/rotation/amplitude/octaves/lacunarity/gain/colorMapping/colorA/colorB）
7. ファクトリ `case LayerType::Noise` 登録、`cmake/ArtifactSources.cmake` 明示登録
8. Project View / Composition Editor の New メニューから `Noise Layer` を作成可能
9. Workspace Automation に `createNoiseLayer(compositionId, name, width, height, seed)` を追加
10. `createNoiseLayer` / `addNoiseLayer` の作成時 `kind` 指定（Perlin / Simplex / FBM / Voronoi / White / Value / Gradient）と既存プリセット指定（Marble / Clouds / Cellular / Fabric / Terrain / Metal）
11. 詳細設定付き作成 UI（2026-08-25）: `CreateNoiseLayerDialog`（`include/Widgets/Dialog/CreateNoiseLayerDialog.ixx` / `src/Widgets/Dialog/CreateNoiseLayerDialog.cppm`）。名前 / 種別 / シード / サイズを指定可能。Project View の `Noise Layer...` と Composition Editor の `New Noise Layer...` から使用。初期サイズはコンポジションサイズ
12. Project View / Composition Editor の New メニューから既存 procedural preset（Marble / Clouds / Cellular / Fabric / Terrain / Metal）を直接選択して Noise Layer を作成可能

## 未着手

- アセット系アイコン・表示名の整備

## Update 2026-09-01 — Timeline identity

タイムラインのレイヤー種別判定で `ArtifactNoiseLayer` をSolidから分離し、Noise専用アイコンを表示するようにした。アセットブラウザ側のアイコン・表示名、実機での表示確認は未実施。

複製・再構成時のruntime型推定にもNoise専用分岐を追加し、NoiseレイヤーがSolidとして復元される経路を避けた。

## Update 2026-09-01 — Animated procedural settings

`noise.seed`、`noise.scaleX/Y`、`noise.offsetX/Y`、`noise.rotation`、`noise.amplitude`、`noise.octaves`、`noise.lacunarity`、`noise.gain` をアニメーション可能なPropertyとして登録し、既存のPropertyキーフレームとAnimationLayerStackを現在フレームで評価してからCPU/GPU生成へ渡すようにした。これによりOffset／Rotationを使った流れるノイズの基礎経路を追加した。ビルド・runtime parity・キーフレームUIからの実機確認は未実施。

## Update 2026-08-30 — Python workspace API exposure

`ArtifactPythonHookManager` の既存 `artifact.workspace` 登録へ `createNoiseLayer` と `addNoiseLayer` を追加した。Workspace Automation と同じ composition ID、name、width、height、seed、kind 引数を JSON 結果付きで渡し、composition ID 省略時は `current`、サイズ省略時は composition サイズ、kind 省略時は `perlin` を使う。Project View / Composition Editor の New メニューには procedural preset 6 種が既に存在する。さらに、Workspace Automation が受け取った kind／preset を実レイヤー設定へ反映していなかったため、factory と同じ設定生成を追加した。Python engine 実行と noise layer の runtime／round-trip は未検証である。

## GPU compute 経路の現状（2026-08-30 静的確認）

- `ArtifactNoiseLayer::draw()` は color mapping 無効時に
  `ProceduralTextureComputePipeline` で RGBA16F texture を生成し、既存
  `ArtifactIRenderer::drawSpriteTransformed()` の texture-view 経路へ直接渡す。
- レイヤー設定の署名と Diligent device を保持し、設定変更時のみ再生成する。
  device が切り替わった場合は pipeline／context／texture を破棄して再初期化する。
- pipeline 初期化・texture 作成・dispatch のいずれかが失敗した場合、または
  color mapping 有効時は、既存 `ImageF32x4_RGBA` の CPU 生成・描画経路へ戻る。
- D3D12／Vulkan の実機出力一致、device reset 後の再生成、GPU texture cache との
  統合は未検証であり、完了扱いにはしない。

## 対象ファイル

- `Artifact/include/Layer/ArtifactNoiseLayer.ixx` / `Artifact/src/Layer/ArtifactNoiseLayer.cppm`
- `Artifact/include/Widgets/Dialog/CreateNoiseLayerDialog.ixx` / `Artifact/src/Widgets/Dialog/CreateNoiseLayerDialog.cppm`
- `Artifact/src/Widgets/ArtifactProjectManagerWidget.cppm` / `Artifact/src/Widgets/Render/ArtifactCompositionEditor.cppm`
- `Artifact/include/Layer/ArtifactAbstractLayer.ixx`（enum）
- `Artifact/src/Layer/ArtifactLayerFactory.cppm`
- `Artifact/cmake/ArtifactSources.cmake`
