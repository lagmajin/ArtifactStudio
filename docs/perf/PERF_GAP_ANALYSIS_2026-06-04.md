# ArtifactStudio 性能ギャップ分析レポート (2026-06-04)

> **方法論**: ソースコード調査のみ。ドキュメント参照なし。
> **調査範囲**: `Artifact/src/`, `ArtifactCore/src/`, `ArtifactCore/include/` の全 48 モジュール。

---

## 概要

AfterEffects ライクな動画合成ソフトとして、**GPU コンポジットパスが未完成** な点が最も深刻なボトルネック。Diligent/D3D12 のインフラは整っているが、フレームごとに `QImage` / `QPainter` を経由した CPU フォールバックに頼る経路が残っており、GPU の恩恵を活かし切れていない。

---

## 🔴 カテゴリ A: クリティカル（即時対応推奨）

### A1. GPU コンポジションパイプラインの未完成

**影響度: 重大 — レンダリングパスの大部分が CPU フォールバック**

| ファイル | 行 | 問題 |
|---------|-----|------|
| `Render/Software/ArtifactSoftwareImageCompositor.cppm` | 全体 | **QImage/QPainter ベースのソフトウェアフォールバック**。実体はあるが GPU ネイティブではなく、変換・ブレンドのホットパスとして重い |
| `ArtifactLayerPreviewPipeline.cpp` | L8-15 | `LayerPreviewPipeline::Impl` が **空クラス**、一切のパイプライン処理なし |
| `OffscreenRenderer2D::Impl::blendLayer()` | L296-306 | **空実装** — 複数ブレンドモードのディスパッチが行われない |
| `OffscreenRenderer2D::Impl::renderFrame()` | L313-316 | **空実装** — フレームレンダリングがスキップされる |
| `OffscreenRenderer2D::Impl::drawImage()` | L323-331 | **空実装** — 画像描画が行われない |
| `OffscreenRenderer2D::Impl::drawSolidRect()` | L318-321 | **空実装** — 矩形描画が行われない |
| `OffscreenRenderer2D::Impl::resize()` | L278-281 | **空実装** — リサイズ時にテクスチャ再作成なし |
| `RenderPipeline::renderComposition()` | L197-216 | **何も描画せず RTV をクリアするだけ**。引数 layers/currentFrame は void キャストで黙殺 |
| `CompositionRenderer::DrawCompositionRect()` | L31-37 | 単一矩形の drawRectLocal 委譲のみ。レイヤーコンポジションロジックなし |

### A2. CPU-GPU readback の毎フレーム転送

**ファイル**: `ArtifactIRenderer.cppm`
**行**: L878-1020, L1117-1261

**問題**:
- 毎フレーム `CopyTexture → Fence Wait → Map → ピクセル変換ループ` の同期転送
- HDR パスでは **ピクセル単位で `std::pow()`（ガンマ補正）を実行**
- フルHD の readback 1回あたり **約 5-15ms** のオーバーヘッド
- `Adjustment Layer`（`ArtifactCompositionViewDrawing.cppm` L889-897）では **毎フレーム readbackToImage() が呼ばれる**

**改善案**: HDR readback のガンマ補正をテーブルルックアップ化。Adjustment Layer は accumSRV を直接利用。

### A3. 調整レイヤー（Adjustment Layer）の致命的な readback

**ファイル**: `ArtifactCompositionViewDrawing.cppm`
**行**: L889-897

```cpp
if (layer->isAdjustmentLayer()) {
    QImage background = renderer->readbackToImage();  // ← 毎フレーム GPU→CPU 転送！
    if (!background.isNull()) {
        applySurfaceAndDraw(background, localRect, true);
    }
    return;
}
```

**問題**: 調整レイヤーがあるだけで、**パイプライン全体が GPU→CPU→GPU の往復** になる。

### 補足

ソフトウェア合成経路そのものは存在し、`ArtifactSoftwareImageCompositor.cppm` では `QImage` / `QPainter` を使った QPainter fallback と CPU 側ブレンドが実装されている。  
ただし、この経路は「ある」ことと「性能的に十分である」ことは別で、現状のボトルネックは **GPU ネイティブな合成経路が未完成なまま、CPU 変換と readback を跨ぐ構造が残っている** 点にある。

---

## 🟠 カテゴリ B: 中程度（次フェーズ対応）

### B1. `dynamic_cast` チェーンの多発

| ファイル | 関数 | 回数 |
|---------|------|------|
| `ArtifactCompositionViewDrawing.cppm` | `drawLayerForCompositionView` | 10 回 |
| `ArtifactCompositionViewDrawing.cppm` | `layerUsesSurfaceUploadForCompositionView` | 6 回 |
| `ArtifactCompositionRenderController.cppm` | `buildLayerSurfaceCacheKey` | 6 回 |
| `ArtifactCompositionRenderController.cppm` | `layerNeedsFrameSyncForCompositionView` | 4 回 |
| `ArtifactCompositionRenderController.cppm` | `matteResolverLambda` | 4 回 |

### B2. QImage コンバートのホットパスでのディープコピー

**ファイル**: `PrimitiveRenderer2D.cppm`
**行**: L712-714, L882-884, L951-953, L1057-1058

### B3. フレーム精度シークの O(N) デコード

**ファイル**: `MediaPlaybackController.cppm`
**行**: L176-332

### B4. renderOneFrame() のレンダリングスケジューラ

**ファイル**: `ArtifactCompositionRenderController.cppm`
**行**: L3622-3649 (renderTickTimer_), L6254-6277 (renderOneFrame)

### B5. 再生ループのスレッド間フレーム配送

**ファイル**: `ArtifactPlaybackEngine.cppm`
**行**: L178-366

### B6. ImageF32x4_RGBA の OpenCV 依存オーバーヘッド

**ファイル**: `ImageF32x4_RGBA.cppm`
**行**: L441-475, L312-320

---

## 🟢 カテゴリ C: 軽度（長期的な改善候補）

### C1. FrameCache の eviction が O(N) スキャン

**ファイル**: `ArtifactFrameCache.cppm`
**行**: L101-168

### C2. スタートアップの直列初期化

**ファイル**: `ArtifactIRenderer.cppm`
**行**: L710-786

### C3. GPUTextureCacheManager のテクスチャ再アップロード

**ファイル**: `PrimitiveRenderer2D.cppm`
**行**: L50-91, L718-733

### C4. マテリアル/テキストレイヤーのキャッシュキー合成コスト

**ファイル**: `ArtifactCompositionViewDrawing.cppm`
**行**: L221-259

---

## 📊 優先度マトリクス

| ID | 課題 | 深刻度 | 性能影響 | 修正難易度 | 優先度 |
|----|------|--------|---------|-----------|--------|
| A1 | GPU コンポジット未完成 | 🔴 重大 | 〜5〜50x | 大 | **最優先** |
| A2 | readback ガンマ補正 + 転送 | 🔴 重大 | 5-15ms/フレーム | 小 | **最優先** |
| A3 | Adjustment Layer readback | 🔴 重大 | 5-20ms/フレーム | 中 | **高** |
| B1 | dynamic_cast チェーン | 🟠 中 | 0.2-1ms/フレーム | 中 | **高** |
| B2 | QImage convertToFormat | 🟠 中 | 0.5-3ms/フレーム | 中 | **高** |
| B3 | フレーム精度シーク O(N) | 🟠 中 | 50-500ms/seek | 大 | 中 |
| B4 | レンダリングスケジューラ | 🟠 中 | 16-48ms 遅延 | 小 | **高** |
| B5 | 再生スレッド QImage 配送 | 🟠 中 | 1-5ms/フレーム | 中 | 中 |
| B6 | OpenCV チャンネル分解 | 🟠 中 | 0.5-2ms/呼出 | 小 | 中 |
| C1 | FrameCache O(N) evict | 🟢 軽 | <0.1ms | 小 | 低 |
| C2 | スタートアップ直列 | 🟢 軽 | 1-3秒 | 中 | 低 |
| C3 | GPU テクスチャ再アップロード | 🟢 軽 | 0.1-1ms | 小 | 低 |
| C4 | キャッシュキー文字列連結 | 🟢 軽 | <0.1ms | 小 | 低 |

---

## 📁 関連ファイル一覧（新規発見 + 既存レポートとの差分）

### 新規発見のクリティカルパス
| ファイル | 行 | 問題 ID | 内容 |
|---------|-----|--------|------|
| `Artifact\src\Render\ArtifactRenderLayerPipeline.cppm` | 197-216 | A1 | `renderComposition()` が layers/currentFrame を黙殺 |
| `Artifact\src\Render\ArtifactOffscreenRenderer2D.cppm` | 296-334 | A1 | blendLayer/renderFrame/drawImage が空 |
| `Artifact\src\Preview\ArtifactLayerPreviewPipeline.cpp` | 8-15 | A1 | Impl が空クラス |
| `Artifact\src\Render\ArtifactSoftwareImageCompositor.cppm` | 全行 | A1 | ファイル自体が空 |
| `ArtifactCore\src\Media\MediaPlaybackController.cppm` | 176-332 | B3 | フレーム精度シーク O(N) seek |
| `Artifact\src\Playback\ArtifactPlaybackEngine.cppm` | 178-366 | B5 | 再生ループ + QImage 配送 |
| `ArtifactCore\src\Image\ImageF32x4_RGBA.cppm` | 441-475, 312-320 | B6 | OpenCV split/merge チャンネル分解 |
| `Artifact\src\Render\ArtifactFrameCache.cppm` | 101-168 | C1 | O(N) eviction |

### 既存レポートとの差分
- `STRUCTURAL_PERF_ISSUES_2026-04-25.md` に **A1, A2, B3-6, C1-4 を追加**
- `COMPOSITE_EDITOR_UI_UPDATE_MECHANISM_2026-04-25.md` は **B4 の詳細をカバー**（問題1-5は未修正状態）
