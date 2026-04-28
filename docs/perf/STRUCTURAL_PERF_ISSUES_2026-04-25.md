# 構造的パフォーマンス課題レポート (2026-04-25)

## 概要

即時修正が困難だが、中長期的に解決すべき構造的パフォーマンス問題を列挙する。

---

## G: レイヤー描画パスの `dynamic_cast` 連鎖 (20+ casts/layer/frame)

### 場所

- `Artifact\src\Render\ArtifactCompositionViewDrawing.cppm:610-794` — `drawLayerForCompositionView`
- `Artifact\src\Render\ArtifactCompositionViewDrawing.cppm:472-491` — `layerUsesSurfaceUploadForCompositionView`
- `Artifact\src\Widgets\Render\ArtifactCompositionRenderController.cppm:1003-1180` — `buildLayerSurfaceCacheKey`
- `Artifact\src\Widgets\Render\ArtifactCompositionRenderController.cppm:352-362` — `layerNeedsFrameSyncForCompositionView`

### 問題

コンポジションの全レイヤーに対し、毎フレーム以下の関数が呼ばれ、それぞれが 6〜8 回の `dynamic_cast` を実行する：

```
1レイヤーあたり:
  drawLayerForCompositionView  → 最大 8 casts
  buildLayerSurfaceCacheKey    → 最大 7 casts
  layerUsesSurfaceUpload*      → 最大 6 casts × 2回呼び出し
  layerNeedsFrameSync*         → 最大 3 casts
  ─────────────────────────────────────
  合計: 最大 30 casts/layer
```

30 レイヤーのコンポジションで 60fps = **54,000 dynamic_cast/秒**。

### 提案

`ArtifactAbstractLayer` に仮想メソッド `LayerType layerType() const` を追加し、各具象レイヤーでオーバーライドする。全 `dynamic_cast` チェーンを `switch (layer->layerType())` に置き換える。

```cpp
// Before (毎回 vtable を歩く)
if (auto* video = dynamic_cast<ArtifactVideoLayer*>(layer)) { ... }
if (auto* text  = dynamic_cast<ArtifactTextLayer*>(layer))  { ... }

// After (enum 分岐: 1 回の仮想関数呼び出し + switch)
switch (layer->layerType()) {
case LayerType::Video: handleVideo(static_cast<ArtifactVideoLayer*>(layer)); break;
case LayerType::Text:  handleText(static_cast<ArtifactTextLayer*>(layer));   break;
}
```

### 影響範囲

- `ArtifactAbstractLayer` 基底クラス: 仮想メソッド追加
- 全具象レイヤークラス (12+): オーバーライド追加
- レイヤー描画パスの全 `dynamic_cast` チェーン: switch に置換

### 見積工数

中（半日〜1日）

---

## H: 調整レイヤーの毎フレーム GPU Readback

### 場所

`Artifact\src\Render\ArtifactCompositionViewDrawing.cppm:796-804`

```cpp
if (layer->isAdjustmentLayer()) {
    QImage background = renderer->readbackToImage();  // GPU → CPU 転送 (パイプラインストール)
    if (!background.isNull()) {
        applySurfaceAndDraw(background, localRect, true);
    }
    return;
}
```

### 問題

調整レイヤーが存在する場合、**毎フレーム** レンダーターゲット全体を GPU から CPU に読み戻す。これは以下を引き起こす：
1. GPU パイプラインストール（CPU が GPU の完了を待つ）
2. ステージングバッファの map/unmap
3. `QImage` の確保とコピー

### 提案

調整レイヤーの背景は前フレームの accumSRV から取得できる。`readbackToImage()` の代わりに：
- GPU パス: 前回の accumSRV をそのまま調整レイヤー入力として使用（RTV に blit）
- CPU パス: キャッシュされた前フレーム結果を再利用

### 影響範囲

- `ArtifactCompositionViewDrawing.cppm:796-804` のロジック変更
- GPU パスでは `readbackToImage()` を accumSRV の blit に置き換え

### 見積工数

中（数時間）

---

## I: 毎 draw コールの `QImage::convertToFormat` （ディープコピー）

### 場所

- `Artifact\src\Render\PrimitiveRenderer2D.cppm:700, 868, 935, 1039-1040`
- `Artifact\src\Render\GPUTextureCacheManager.cppm:133-135`

```cpp
const QImage rgba = image.convertToFormat(QImage::Format_RGBA8888);
```

### 問題

すべての `drawSprite*` 呼び出しで `QImage::convertToFormat` が走る。入力が既に `Format_RGBA8888` でもディープコピー（確保 + memcpy）が発生する。

AGENTS.md で「QImage はホットパス禁止」と明記されているが、このコードはまさにホットパスにある。

### 提案

1. AGENTS.md に準拠し、`ImageF32x4_RGBA` ベースの描画に統一する
2. どうしても QImage が必要な場合は、`GPUTextureCacheManager` のようにフォーマットチェックを先に行い、既に `RGBA8888` ならコピーを回避する

```cpp
// PrimitiveRenderer2D 内
const QImage& rgba = (image.format() == QImage::Format_RGBA8888)
    ? image : image.convertToFormat(QImage::Format_RGBA8888);
```

### 影響範囲

- `PrimitiveRenderer2D.cppm` の全 `convertToFormat` 箇所
- 各レイヤーの `toQImage()` を `currentFrameBuffer()` に段階的に移行

### 見積工数

大（段階的移行、1〜2日）

---

## J: `LayerChangedEvent` 購読者の処理チェーン最適化

### 場所

`Artifact\src\Widgets\Render\ArtifactCompositionRenderController.cppm:1896-1953`

### 問題

`LayerChangedEvent` の購読処理が重い。特にギズモドラッグ中のスライダー値変更で、毎フレーム以下が同期的に実行される：

```cpp
// Modified イベントの場合:
invalidateLayerSurfaceCache(layer);          // hash lookup + erase
changeDetector_.markLayerChanged(...);        // hash insert
invalidateBaseComposite();                    // bool フラグ
syncSelectedLayerOverlayState(...);           // オーバーレイ再計算
renderDebounceTimer_->start(33);             // タイマー再設定

// Created/Removed イベントの場合:
surfaceCache_.clear();                       // キャッシュ全クリア
gpuTextureCacheManager_->clear();            // GPU キャッシュ全クリア
applyCompositionState(comp);                 // レイヤーツリー完全再構築
```

### 提案

1. ギズモドラッグ中は `syncSelectedLayerOverlayState` をスキップ（`gizmoDragActive_` フラグ活用）
2. Created/Removed イベントのキャッシュ全クリアを、影響レイヤーのみの選択的無効化に変更
3. `markLayerChanged` の hash insert を軽量なフラグセットに置き換え

### 影響範囲

- `ArtifactCompositionRenderController.cppm:1896-1953`
- キャッシュ無効化ロジック

### 見積工数

中（半日）

---

## 関連ファイル一覧

| # | ファイル | 主な変更内容 |
|---|---------|------------|
| G | `ArtifactAbstractLayer.ixx` + 全具象レイヤー | `virtual LayerType type() const` 追加 |
| G | `ArtifactCompositionViewDrawing.cppm` | `dynamic_cast` → `switch(layerType)` |
| G | `ArtifactCompositionRenderController.cppm` | `dynamic_cast` → `switch(layerType)` |
| H | `ArtifactCompositionViewDrawing.cppm:796-804` | `readbackToImage` → accumSRV blit |
| I | `PrimitiveRenderer2D.cppm` 各所 | `convertToFormat` 回避 / `ImageF32x4_RGBA` 優先 |
| J | `ArtifactCompositionRenderController.cppm:1896-1953` | 選択的キャッシュ無効化、ギズモ中スキップ |

---

**文書終了**
