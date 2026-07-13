# Rasterizer Effect / Mask Pipeline Order Issue

**作成日:** 2026-07-02
**状態:** 実装修正済み（実動確認待ち）

---

## 概要

`buildRasterizedSurfaceBuffer()` における Rasterizer エフェクト（DropShadow 等、`EffectPipelineStage::Rasterizer`）とマスクの適用順序が逆になっており、マスクで隠した領域からも影がはみ出して表示される可能性がある。

## 現在の処理順序（`ArtifactCompositionRenderController.cppm`）

```
1. Layer base surface 生成
2. → Rasterizer エフェクト適用（DropShadow 等） ← マスク前のアルファから影生成
3. → マスク適用
```

## 問題点

- DropShadow がマスクされる前のレイヤー全体のアルファチャンネルから影を生成する
- その後マスクでクリップされるため、マスクで除外した領域にも影が残る
- 例: マスクで半分隠した円形レイヤーに DropShadow を適用すると、隠した側にも影が付く

## 期待される順序（AE 準拠）

```
1. Layer base surface 生成
2. → マスク適用（先にクリップ）
3. → Rasterizer エフェクト適用（マスク後のアルファからのみ影生成）
```

## 修正対象ファイル

- `Artifact/src/Widgets/Render/ArtifactCompositionRenderController.cppm` — `buildRasterizedSurfaceBuffer()` の 1362-1406 行目付近

## 修正方針

`hasRasterizerEffect` と `hasMasks` の適用順序を入れ替える:

```cpp
// 現在:
if (hasRasterizerEffect) { ... }  // 先: ラスタライザー
if (hasMasks) { ... }             // 後: マスク

// 修正後:
if (hasMasks) { ... }             // 先: マスクでクリップ
if (hasRasterizerEffect) { ... }  // 後: マスク後のアルファからラスタライザー
```

## 注意点

- 一部の Rasterizer エフェクトはマスク前の元画像への依存がある可能性があるため、各エフェクトの `applyConfigured()` がマスク後の入力でも正しく動作するか確認が必要
- GPU パス（`layerUsesGpuTextureCacheForCompositionView`）でも同様の順序問題がないか要確認
- surface cache key（`buildLayerSurfaceCacheKey`）に影響を与える可能性あり（マスク → ラスタライザーの順では cache signature 変更が必要かも）

## 2026-07-13 implementation update

- `ArtifactCompositionViewDrawing.cppm`の`QImage`入力と`ImageF32x4_RGBA`入力の両方を`mask -> Rasterizer effect`順へ統一した。
- `ArtifactCompositionRenderController.cppm`側の同等処理も同じ順序であることを確認した。
- build / visual fixtureはプロジェクト指示により未実行。Drop Shadow等の実動確認後に状態を「確認済み」へ更新する。
