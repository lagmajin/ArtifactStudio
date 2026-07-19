# ColorFX 有機的変形エフェクト基盤 (2026-07-19)

日付：2026-07-19
目標：RubberDeformアルゴリズムをGPUシェーダーで実装し、有機的変形エフェクトの基盤とする

参照元: OpenToonz `colorfx/colorfxutils.h` RubberDeform (BSD 3-Clause)
ライセンス: `docs/THIRD_PARTY_NOTICES.md` 記載済

---

## Goal

- 2D画像に物理ベースの有機的変形（ゴム変形）を適用
- 制御点ベースのエネルギー最小化による自然な歪み
- 布、液体、髪の毛などの表現基盤

---

## Definition of Done

- [ ] 制御点（多角形）による変形領域の定義
- [ ] 内部エネルギー最小化の反復計算がGPUで動作
- [ ] 変形後の制御点位置から画像ワープ
- [ ] 変形強度・反復回数・精細化のパラメータ制御
- [ ] Inspectorから全パラメータ編集可能
- [ ] リアルタイムプレビュー対応

---

## Architecture

```
制御点多角形（初期位置）
        ↓
deformStep(): 内部エネルギー最小化
  - エッジ長の平均化（弾性エネルギー）
  - 角度保存（曲げエネルギー）
        ↓
反復 (n回)
        ↓
refinePoly(): 必要に応じて精細化
        ↓
変形後多角形 → テクスチャワープ → 出力
```

## OpenToonz → GPU 変換

| OpenToonz (CPU) | ArtifactStudio (GPU) |
|---|---|
| `vector<T3DPointD>` 制御点 | StructuredBuffer<Point> |
| `deformStep()` 逐次反復 | Compute Shader 反復 |
| 最終的に `TFlash` でラスタライズ | ワープUVマップとしてテクスチャ出力 |

---

## Work Packages

### 1. 制御点バッファ管理
- StructuredBuffer で読み書き可能な制御点バッファ
- 初期配置（円形、矩形、カスタム）

### 2. deformStep シェーダー
- エッジ長の平均化パス
- 角度保存パス
- ダンピング係数による安定化

### 3. ワープUV生成
- 変形前後の制御点から Thin Plate Spline または MLS (Moving Least Squares) 変形
- UVマップとしてテクスチャ出力

### 4. Inspector統合
- 制御点の視覚的編集（Gizmo）
- 変形強度・反復回数スライダー

---

## Suggested Order

1. 制御点バッファ + 基本 deformStep
2. ワープUV生成 (MLS変形)
3. 精細化 (refinePoly)
4. Inspector + Gizmo統合
