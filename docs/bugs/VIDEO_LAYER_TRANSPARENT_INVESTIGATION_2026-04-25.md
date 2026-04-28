# コンポジットエディタ ビデオレイヤー透明化 調査レポート (2026-04-25)

**作成日:** 2026-04-25
**ステータス:** 調査中（ログ挿入済み）
**前回レポート:** `VIDEO_LAYER_NOT_DISPLAYING_HYPOTHESES_2026-03-27.md`

---

## 現象

コンポジットエディタ上でビデオレイヤーが表示されず、透明（または何も描画されない）状態になる。

---

## コードフロー分析

### 描画パイプライン（GPU パス）

```
CompositeRenderController::Impl::drawImpl()
  → accumRTV をクリア (0,0,0,0) + bgColor 描画
  → レイヤーループ:
      → goToFrame() → decodeCurrentFrame() [非同期]
      → layerRTV をクリア (0,0,0,0)
      → drawLayerForCompositionView(layer, renderer, 1.0f, ...)
          → videoLayer->hasCurrentFrameBuffer() ?
              TRUE:  drawSpriteTransformed(frameBuffer, 1.0f) → layerRTV
              FALSE: ↓ QImage フォールバック
                     currentFrameToQImage() → null?
                     decodeFrameToQImage() [同期] → null?
      → blendPipeline->blend(layerSRV, accumSRV, tempUAV, blendMode, opacity)
      → swapAccumAndTemp()
  → accumSRV を viewport に blit (SRC_ALPHA)
```

### フレームバッファ生成チェーン

```
FFmpeg → QImage::Format_RGB888 (3ch, アルファなし)
  → qImageToCvMat → CV_8UC3 BGR
  → toFrameBuffer:
      CV_8UC3 ≠ CV_8UC4 → skip cvtColor block
      → setFromCVMat(CV_8UC3):
          cvtColor(BGR2RGBA) → CV_8UC4 RGBA alpha=255
          convertTo(CV_32F, 1/255) → CV_32FC4 RGBA alpha=1.0

MF → QImage::Format_RGBA8888 (4ch)
  → qImageToCvMat → CV_8UC4 BGRA (via Format_ARGB32 fallback)
  → toFrameBuffer:
      CV_8UC4 → cvtColor(BGRA2RGBA) → CV_8UC4 RGBA
      → setFromCVMat(CV_8UC4):
          convertTo(CV_32F, 1/255) → CV_32FC4 RGBA
```

---

## 発見

### 発見1 (BUG): `drawSpriteTransformed` で ImageF32x4_RGBA の R/B チャンネルスワップ

**場所:** `Artifact\src\Render\PrimitiveRenderer2D.cppm:1132-1136`

```cpp
if (rgba.type() == CV_32FC4) {
    rgba.convertTo(rgba8, CV_8UC4, 255.0);
    cv::cvtColor(rgba8, rgba8, cv::COLOR_BGRA2RGBA); // BUG: 元データは既にRGBA
}
```

内部の `ImageF32x4_RGBA` は RGBA 順だが、`cv::cvtColor(BGRA2RGBA)` を適用しているため R と B が入れ替わる。
**影響:** 色ずれ（赤⇔青）。透明度には影響しない。
**深刻度:** 中（色が正しくない）

### 発見2 (DESIGN): `drawSpriteTransformed` の opacity 引数がデッドコード

**場所:** `ArtifactCore\src\Graphics\Shader\BasicVertexShader.cppm:230`

トランスフォーム用頂点シェーダ `drawSpriteTransformVSSource` が `output.color = float4(1,1,1,1)` とハードコード。
頂点カラー ATTRIB2（opacity 格納先）を読み取らない。
**影響:** 描画時に opacity が適用されない。ただしブレンドパイプラインが別途 opacity を適用するため合成結果には影響しない。
**深刻度:** 低（実害なしだが混乱のもと）

### 発見3: FFmpeg デコードはアルファなし QImage::Format_RGB888

**場所:** `ArtifactCore\src\Media\MediaImageFrameDecoder.cppm:179`

```cpp
QImage img(codecContext_->width, codecContext_->height, QImage::Format_RGB888);
```

`toFrameBuffer` の `cv::COLOR_BGR2RGBA` 変換で alpha=1.0 が付与されるため最終的なフレームバッファは正しい。
**深刻度:** 低

### 発見4: `hasCurrentFrameBuffer_` に同期機構なし

**場所:** `Artifact\src\Layer\ArtifactVideoLayer.cppm:716-723`

バックグラウンドスレッド（`QtConcurrent::run`）が `hasCurrentFrameBuffer_` と `currentFrameBuffer_` を書き込み、メインスレッドが読み取るが、mutex/atomic など同期機構がない。
**影響:** x86 では実質的に問題になりにくいが、稀に stale リードの可能性。
**深刻度:** 低〜中

---

## 仮説

### ★★★ 仮説A: 非同期デコード未完了 → フレームバッファ不在 → 何も描画されない

**最も可能性が高い。**

1. コンポジットビュー初回描画時、`goToFrame()` → `decodeCurrentFrame()` が非同期デコードを開始
2. 同じ render tick 内で `drawLayerForCompositionView()` が呼ばれる
3. `hasCurrentFrameBuffer()` はまだ false（非同期デコード未完了）
4. QImage フォールバック:
   - `currentFrameToQImage()` → null（初回デコード前）
   - `decodeFrameToQImage()` → `playbackController_->getVideoFrameAtFrameDirect()` を同期呼び出し
5. 同期呼び出しが成功すれば描画されるが、以下の理由で失敗する可能性:
   - FFmpeg: `directDecodeMutex_` が非同期デコードと競合
   - FFmpeg: シーク失敗、デコーダー状態不正
   - MF: エクストラクタ閉鎖、フレーム範囲外
   - `lastSyncFallbackSucceeded_ = false` のキャッシュで再試行抑制（line 824-827）

**検証: ログ挿入済み（`[VideoLayerT]` タグ）**

### ★★ 仮説B: 可視性ゲートによるスキップ

| チェック | 場所 (行) | 条件 |
|---------|----------|------|
| `isLayerEffectivelyVisible()` | Controller:530-541 | 親チェーン非表示 |
| `hasSoloLayer && !isSolo()` | Controller:4471 | Solo モード |
| `!isActiveAt(currentFrame)` | Controller:4473 | inPoint/outPoint 範囲外 |
| LOD スキップ | Controller:4491-4496 | 画面 2-8px 未満 |
| `opacity <= 0.0f` | Controller:4519 | 不透明度ゼロ |

**検証: ログ挿入済み（`[LayerSkip]` タグ）**

### ★ 仮説C: ブレンドシェーダが全透過出力

フレームバッファの全ピクセル alpha=0 の場合、HLSL ブレンドシェーダの `srcA <= 0.0001` ガードで dst がそのまま出力される。
通常の動画では発生しないが、特殊な透過動画やエンコード破損時に可能性あり。

---

## デバッグログ挿入箇所

| ファイル | 行 | タグ | 内容 |
|---------|-----|------|------|
| `ArtifactCompositionRenderController.cppm` | 1099 | `[VideoLayerT]` | drawLayerForCompositionView 入口: hasFrameBuffer / isEmpty / QImage null |
| `ArtifactVideoLayer.cppm` | 665 | `[VideoLayerT]` | decodeCurrentFrame 入口: isLoaded / opening / sourceFrame |
| `ArtifactCompositionRenderController.cppm` | 4519 | `[LayerSkip]` | opacity <= 0 でスキップ時の詳細 |

---

## 修正候補（後続タスク）

### 修正1: 同期フォールバックの強化
`decodeFrameToQImage()` が null を返した場合、一定回数リトライするか、より積極的に非同期完了を待つ。

### 修正2: R/B スワップ修正
`PrimitiveRenderer2D.cppm:1136` の `cv::cvtColor(rgba8, rgba8, cv::COLOR_BGRA2RGBA)` を削除するか、`COLOR_RGBA2BGRA` に変更する（データが RGBA であることを前提に修正）。

### 修正3: 同期プリミティブ追加
`hasCurrentFrameBuffer_` と `currentFrameBuffer_` の読み書きを mutex または atomic で保護。

---

## 関連ファイル

| ファイル | 行 | 内容 |
|---------|-----|------|
| `Artifact/src/Layer/ArtifactVideoLayer.cppm` | 252-292 | Impl（hasCurrentFrameBuffer_ など） |
| `Artifact/src/Layer/ArtifactVideoLayer.cppm` | 662-747 | decodeCurrentFrame |
| `Artifact/src/Layer/ArtifactVideoLayer.cppm` | 797-852 | decodeFrameToQImage |
| `Artifact/src/Layer/ArtifactVideoLayer.cppm` | 1078-1116 | draw() |
| `Artifact/src/Widgets/Render/ArtifactCompositionRenderController.cppm` | 1099-1141 | VideoLayer GPU 描画 |
| `Artifact/src/Widgets/Render/ArtifactCompositionRenderController.cppm` | 4468-4561 | レイヤーブレンドループ |
| `Artifact/src/Render/PrimitiveRenderer2D.cppm` | 1108-1170 | drawSpriteTransformed(ImageF32x4_RGBA) |
| `ArtifactCore/src/Media/MediaImageFrameDecoder.cppm` | 158-187 | receiveFrame (Format_RGB888) |
| `ArtifactCore/src/Graphics/Shader/BasicVertexShader.cppm` | 204-239 | drawSpriteTransformVSSource |
| `ArtifactCore/src/Image/ImageF32x4_RGBA.cppm` | 297-343 | setFromCVMat |
| `ArtifactCore/include/Graphics/Shader/Compute/LayerBlendComputeShader.ixx` | 45-85 | ブレンド HLSL |

---

**文書終了**
