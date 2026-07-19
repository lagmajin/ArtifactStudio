# TangentFlow ベクトル場エフェクト基盤 (2026-07-19)

日付：2026-07-19
目標：エッジ方向ベクトル場をGPUで計算し、FlowBlur/FlowPaintの基盤とする

参照元: OpenToonz `stdfx/iwa_tangentflowfx.cpp` (BSD 3-Clause)
ライセンス: `docs/THIRD_PARTY_NOTICES.md` 記載済

---

## Goal

- 入力画像からエッジの接線方向ベクトル場をGPUでリアルタイム計算
- 5x5 Sobelフィルタによる勾配検出 → 接線ベクトル場正規化
- 出力を中間テクスチャとして後段のFlowBlur/FlowPaintで利用可能に

---

## Definition of Done

- [ ] 5x5 SobelフィルタがGPUシェーダーで動作
- [ ] 空領域（勾配ゼロ）の補完が動作（Fast Sweepingの簡易版）
- [ ] 反復平滑化（kernel radius指定）でベクトル場をなめらかに
- [ ] 出力: R=方向X, G=方向Y, B=勾配強度 のテクスチャ
- [ ] 方向アライメント（基準角度にベクトルを揃える）
- [ ] Inspectorから全パラメータ編集可能

---

## Architecture

```
入力画像 → 輝度抽出 (0.3R+0.59G+0.11B)
        → 5x5 Sobel X, Y
        → 90度回転で接線ベクトル化
        → 正規化 + 空領域マーク
        → 反復平滑化 (解像度に応じて2-5回)
        → 方向アライメント（オプション）
        → 出力テクスチャ (R=X方向, G=Y方向, B=強度)
```

## OpenToonz → GPU 変換

| OpenToonz (CPU) | ArtifactStudio (GPU) |
|---|---|
| `for(y) for(x)` の5x5 Sobel | フラグメントシェーダーで9tap conv |
| QThreadPoolでマルチスレッド | Compute Shader で並列 |
| Fast Sweeping Method (逐次) | Jump Flooding Algorithm (GPU向き) |
| 反復 smoothing (CPU double buffer) | Ping-pong テクスチャ |
| `double2` バッファ | `RG16F` テクスチャ |

---

## Work Packages

### 1. Sobelベクトル場計算シェーダー
- 輝度抽出 + 5x5 Sobel X/Y カーネル
- カーネル定数はシェーダーにハードコード

### 2. 空領域補完
- Jump Flooding Algorithm で未計算領域を補完
- 可変回数パス（log2(max(width,height)) 回）

### 3. 反復平滑化
- 近傍ベクトルの加重平均
- Ping-pong テクスチャで2-5回反復
- kernelRadius パラメータ

### 4. 方向アライメント
- 基準角度にベクトル方向を揃える（内積<0なら反転）

### 5. Inspector統合
- 全パラメータをPropertyシステムで露出
- プレビュー表示（ベクトル場の可視化）

---

## Suggested Order

1. Sobelベクトル場計算（最小実装）
2. 平滑化
3. 空領域補完
4. 方向アライメント
5. Inspector統合
