# Matte / Blend Compositing Evaluation Order

**作成日:** 2026-07-03
**対象:** `ArtifactCompositionRenderController.cppm` (GPU path), `ArtifactCompositionViewDrawing.cppm` (CPU path)

---

## 1. レイヤー合成の全体フロー

```
Layer List (下→上の順)
  │
  ▼
1. Layer visibility / active-at-time check
  │
  ▼
2. Layer-local transform (getGlobalTransform)
  │
  ▼
3. Layer surface build (rasterizer effects + masks)
  │
  ▼
4. Track Matte application
  │
  ▼
5. Blend into accumulation buffer
  │
  ▼
6. Next layer
```

---

## 2. 各レイヤーの処理順（GPU パス）

### 2.1 レイヤー描画 (`drawGpuLayerToIntermediate`)

```
入力: layerRTV, accumSRV (現在の累積合成結果)

1. Adjustment Layer 検出
   └─ isAdjustmentLayer() → true:
        accumSRV の内容を layerRTV にコピー（背景キャプチャ）
        インタラクティブモード: ここでスキップ（readback回避）
   └─ false: layerRTV をクリア

2. drawLayerForCompositionView() を呼び出し
   ├─ レイヤー自身の描画 (layer->draw(renderer))
   ├─ ラスタライザエフェクト適用 (buildRasterizedSurfaceBuffer)
   ├─ マスク適用 (applyLayerMatteToSurface)
   └─ surfaceCache により同一フレームはキャッシュ
```

### 2.2 トラックマット適用 (`prepareGpuLayerForBlend`)

```
1. convertLayerToFloat(): sRGB→リニア変換
2. マット参照の評価:
   ├─ MatteType::Alpha:
   │    matte = matteSource.a
   │    結果 = layer.rgb * matte  (layerをマットでマスク)
   ├─ MatteType::Luma:
   │    matte = dot(matteSource.rgb, (0.299, 0.587, 0.114))
   │    結果 = layer.rgb * matte
   ├─ MatteType::InverseAlpha:
   │    matte = 1.0 - matteSource.a
   │    結果 = layer.rgb * matte
   └─ MatteType::InverseLuma:
        matte = 1.0 - luma(matteSource)
        結果 = layer.rgb * matte

3. 複数マット参照のスタック:
   └─ 直列適用: matte1 → matte2 → ... (stackMode=0 デフォルト)
```

### 2.3 ブレンド (`blendGpuLayerIntoAccum`)

```
1. blendLayers(blendPipeline, layerSRV, accumSRV, tempUAV, blendMode, opacity)
   ├─ 34種のBlendModeに対応（HLSL compute shader）
   ├─ opacity を乗算: src.a *= opacity
   └─ 失敗時: Normal でリトライ → さらに失敗時: 直接スプライト描画

2. swapAccumAndTemp(): accum ← temp, temp ← accum(旧)
```

---

## 3. 各レイヤーの処理順（CPU パス）

### 3.1 レイヤーサーフェス構築 (`drawLayerForCompositionView`)

```
1. localBounds() で描画範囲を取得
2. getGlobalTransform() で親子transformを乗算
3. レイヤー自身をQImageに描画 (layer->draw)
4. ラスタライザエフェクト適用 (buildRasterizedSurfaceBuffer)
   └─ Adjustment Layer: full-frame モード（ROI縮小禁止）
5. マスク適用 (applyLayerMatteToSurface)
6. drawWithClonerEffect → drawSpriteTransformed で GPU へ送信
```

### 3.2 CPU ソフトウェア合成 (`ArtifactSoftwareImageCompositor`)

```
各レイヤーの QImage を ColorBlendMode.cppm の CPU 実装で合成:
1. 下から順にレイヤーをピック
2. 各ピクセルで BlendMode に応じた関数を適用
3. opacity を乗算
4. 次のレイヤーへ
```

---

## 4. 評価順の重要な注意点

| # | ルール | 説明 |
|:--:|--------|------|
| 1 | **下→上** | レイヤーリストのインデックス0（最下層）から順に合成 |
| 2 | **マット → ブレンド** | トラックマットはブレンドの前に適用される |
| 3 | **Adjustment Layer** | 現在の accum 全体にエフェクトを適用。Adjustment Layer 同士のスタックは上から順 |
| 4 | **Stencil/Silhouette** | ブレンドモードの Special カテゴリ。src の alpha/luma で dst をマスク |
| 5 | **Dissolve** | 確率的ピクセル選択。フレームごとにランダムシード変化 |
| 6 | **opacity** | ブレンド前に src.a に乗算。opacity=0 のレイヤーはスキップ |

---

## 5. ブレンドモードカテゴリ

| カテゴリ | モード | 効果 |
|---------|--------|------|
| G0 Compositing | Normal, Dissolve, Dancing Dissolve | 標準アルファ合成 |
| G1 Stencil | Stencil Alpha/Luma, Silhouette Alpha/Luma | マスク系特殊合成 |
| G2 Darken/Lighten | Multiply, Screen, Darken, Lighten, ColorDodge, ColorBurn, LinearBurn, LinearDodge, Classic系 | 輝度操作 |
| G3 Contrast | Overlay, HardLight, SoftLight, VividLight, LinearLight, PinLight, HardMix | コントラスト操作 |
| G4 Inversion | Add, Subtract, Difference, Exclusion, Divide, ClassicDifference | 色差・加減算 |
| G5 HSL | Hue, Saturation, Color, Luminosity | 色相・彩度・輝度置換 |

---

## 6. 関連コード位置

| 処理 | ファイル | 行 |
|------|---------|:--:|
| GPU layer draw | `ArtifactCompositionRenderController.cppm` | 3728-3766 |
| GPU matte apply | `ArtifactCompositionRenderController.cppm` | 3916-3924 |
| GPU blend dispatch | `ArtifactCompositionRenderController.cppm` | 3939-3974 |
| CPU rasterizer+mask | `ArtifactCompositionViewDrawing.cppm` | 350-417 |
| HLSL shader registry | `LayerBlendComputeShader.ixx` | 646-681 |
| BlendMode enum | `LayerBlend.ixx` | 12-47 |
| BlendModeInfo table | `BlendModeInfo.ixx` | 42-77 |
