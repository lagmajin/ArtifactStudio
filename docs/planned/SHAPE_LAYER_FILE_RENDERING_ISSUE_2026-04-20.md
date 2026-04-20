# ShapeLayer File Rendering Issue

## 概要
- ファイルレンダリング経路で `ArtifactShapeLayer` の描画が不安定、または未対応に見える。
- 画面表示系では `layer->draw(renderer)` まで到達するが、書き出し系では明示的な ShapeLayer 分岐が薄い。

## 現状の観測
- `ArtifactRenderQueueService` の書き出し経路は `Image / Svg / Video / Text / Solid` を主に扱っている。
- `ArtifactShapeLayer` は `toQImage()` と `draw(ArtifactIRenderer*)` を持つ。
- ただし書き出し側は `getThumbnail()` フォールバックに依存している部分がある。

## 影響
- ShapeLayer がファイル書き出しで抜ける、または一部条件で欠落する可能性がある。
- 既存の Composition / Preview 表示と、ファイルレンダリングの結果が一致しない恐れがある。

## 対処方針
1. `ArtifactRenderQueueService::renderLayerSurface()` に ShapeLayer の明示分岐を追加する。
2. `ArtifactShapeLayer::toQImage()` を書き出し用の主要経路として保証する。
3. `localBounds()` と culling 条件が書き出し側で誤って落とさないか確認する。
4. 必要なら `getThumbnail()` 依存を減らし、ShapeLayer のレンダリング責務を明示化する。

## 関連ファイル
- [`Artifact/src/Render/ArtifactRenderQueueService.cppm`](../../Artifact/src/Render/ArtifactRenderQueueService.cppm)
- [`Artifact/src/Layer/ArtifactShapeLayer.cppm`](../../Artifact/src/Layer/ArtifactShapeLayer.cppm)
- [`Artifact/src/Render/ArtifactCompositionViewDrawing.cppm`](../../Artifact/src/Render/ArtifactCompositionViewDrawing.cppm)

