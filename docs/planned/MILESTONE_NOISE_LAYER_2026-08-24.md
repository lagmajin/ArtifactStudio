# MILESTONE: 独立ノイズレイヤー（ArtifactNoiseLayer）

**最終更新:** 2026-08-24

**ステータス:** In Progress（コア実装完了、UI/統合フェーズ未着手）

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

## 未着手

- 作成 UI（メニュー / ダイアログ。`CreatePlaneLayerDialog` パターン参照）
- AI / Workspace Automation / Python API への `addNoiseLayer` 系露出
- プリセット（Marble / Clouds / Cellular / Fabric / Terrain / Metal + NoiseImageGenerator 移植分）
- offset / rotation 等のキーフレーム駆動（流れるノイズ）
- GPU compute 正規経路（`ProceduralTextureComputePipeline` 接続、CPU は現状フォールバック兼初期経路）
- タイムライン / アセット系アイコン・表示名の整備

## 対象ファイル

- `Artifact/include/Layer/ArtifactNoiseLayer.ixx` / `Artifact/src/Layer/ArtifactNoiseLayer.cppm`
- `Artifact/include/Layer/ArtifactAbstractLayer.ixx`（enum）
- `Artifact/src/Layer/ArtifactLayerFactory.cppm`
- `Artifact/cmake/ArtifactSources.cmake`
