# Milestone: 環境マップ (Environment Map) (2026-03-28)

**Status:** Not Started
**Goal:** 3D 照明に HDRI 環境マップを使用可能にする。
反射サーフェスの環境映り込み、IBL (Image-Based Lighting) を実現。

---

## 現状

| 機能 | 状態 | 場所 |
|------|------|------|
| 3D ライト (Point, Directional, Spot) | ✅ 実装済み | `ArtifactLightLayer.cppm` |
| 3D カメラ | ✅ 実装済み | `ArtifactCameraLayer.cppm` |
| 3D マテリアル | ⚠️ 基本のみ | — |
| 環境マップ / HDRI | ❌ 未実装 | — |
| IBL (Image-Based Lighting) | ❌ 未実装 | — |
| Skybox 描画 | ❌ 未実装 | — |

---

## Architecture

```
ArtifactEnvironmentMapLayer (新規レイヤータイプ)
  ├── HDRI テクスチャ (.hdr, .exr 読み込み)
  ├── 環境マップキューブマップ/エクイレクタングラー変換
  ├── IBL 分解 (拡散 + 鏡面反射)
  └── Skybox 描画用シェーダー

IBL パイプライン:
  1. HDRI 読み込み → エクイレクタングラー画像
  2. キューブマップ変換 (6面)
  3. 拡散 IBL: ランパート積分 → Irradiance キューブマップ
  4. 鏡面 IBL: Split-Sum 近似 → BRDF LUT + Pre-filtered 環境マップ
  5. シェーダーでサンプリングして照明に適用
```

---

## Milestone 1: HDRI 読み込み & Skybox

**目的:** HDR 画像を読み込んで背景として描画。

### Implementation

1. `ArtifactEnvironmentMapLayer` クラス作成:
   - HDR 画像読み込み (`.hdr` Radiance / `.exr`)
   - エクイレクタングラー → キューブマップ変換
   - シェーダー定数バッファに環境マップパラメータ

2. Skybox シェーダー:
   - キューブマップサンプリング
   - カメラ回転に追従
   - ビューポートの最遠レイヤーとして描画

3. プロパティ:
   - `hdriPath` — ファイルパス
   - `intensity` — 明るさスケール
   - `rotation` — Y 軸回転
   - `visibleAsBackground` — 背景として表示するか

### 見積
| タスク | 見積 |
|--------|------|
| EnvironmentMapLayer クラス作成 | 4h |
| HDR 画像読み込み (.hdr/.exr) | 3h |
| キューブマップ変換シェーダー | 4h |
| Skybox 描画シェーダー | 3h |
| プロパティ UI | 2h |

### Acceptance Criteria
- HDR 画像をドラッグ＆ドロップで背景に設定できる
- カメラ回転に環境が追従する
- Skybox が最遠レイヤーとして正しく描画される

---

## Milestone 2: IBL (Image-Based Lighting)

**目的:** 環境マップから 3D オブジェクトを照らす。

### Implementation

1. Irradiance キューブマップ生成:
   - ランパート BRDF で半球積分
   - 低解像度キューブマップ (32×32) に焼き込み

2. Pre-filtered 環境マップ生成:
   - Roughness に応じた BRDF 積分
   - 複数ミップレベル (5段階)

3. BRDF LUT 生成:
   - Split-Sum 近似用の 2D LUT
   - Specular BRDF の事前計算

4. シェーダー統合:
   - PBR マテリアルの環境照明計算
   - `ambient = sampleIrradiance(N) * albedo`
   - `specular = samplePreFiltered(R, roughness) * BRDF_LUT(NdotV, roughness)`

### 見積
| タスク | 見積 |
|--------|------|
| Irradiance キューブマップ生成 | 4h |
| Pre-filtered 環境マップ生成 | 4h |
| BRDF LUT 生成 | 2h |
| PBR シェーダー統合 | 4h |

### Acceptance Criteria
- 環境マップが 3D オブジェクトを照らす
- Roughness が 0 のミラーが環境を映す
- Roughness が高い表面が拡散反射する

---

## Milestone 3: 環境マップエディタ UI

**目的:** 環境マップのパラメータを Inspector で編集。

### Implementation

1. プロパティグループ:
   - HDRI パス (ファイルブラウザ)
   - Intensity スライダー
   - Rotation スライダー
   - Background Visible チェックボックス
   - IBL 有効/無効

2. プレビュー:
   - Inspector に HDRI サムネイル表示
   - Skybox 有効時のリアルタイムプレビュー

3. プリセット:
   - Studio / Outdoor / Night / Sunset 等のプリセット HDRI
   - 1クリックで切り替え

### 見積
| タスク | 見積 |
|--------|------|
| プロパティグループ UI | 2h |
| サムネイル表示 | 1h |
| プリセット HDRI リスト | 2h |

---

## Milestone 4: パフォーマンス最適化

**目的:** 環境マップのレンダリングを軽くする。

### Implementation

1. キューブマップのキャッシュ:
   - 同じ HDRI の再読み込みをスキップ
   - テクスチャキャッシュマネージャ連携

2. Pre-filtered マップの事前計算:
   - HDRI 読み込み時に事前焼き込み
   - ランタイムでの逐次計算を回避

3. シェーダー最適化:
   - IBL サンプリングを SPH (球面調和関数) で近似 (拡散のみ)
   - 鏡面反射は最低限のサンプリング

### 見積
| タスク | 見積 |
|--------|------|
| キューブマップキャッシュ | 2h |
| Pre-filtered 事前計算 | 3h |
| SPH 近似 | 3h |

---

## Deliverables

| ファイル | 内容 |
|---------|------|
| `Artifact/src/Layer/ArtifactEnvironmentMapLayer.cppm` (新規) | 環境マップレイヤー |
| `Artifact/include/Layer/ArtifactEnvironmentMapLayer.ixx` (新規) | ヘッダー |
| `Artifact/shaders/SkyboxVS.hlsl` (新規) | Skybox 頂点シェーダー |
| `Artifact/shaders/SkyboxPS.hlsl` (新規) | Skybox ピクセルシェーダー |
| `Artifact/shaders/IBL.hlsl` (新規) | IBL サンプリング関数 |
| `ArtifactCore/include/Graphics/IBLGenerator.ixx` (新規) | Irradiance/Pre-filter 生成 |
| `ArtifactCore/src/Graphics/IBLGenerator.cppm` (新規) | 実装 |

---

## Recommended Order

| 順序 | マイルストーン | 見積 | 理由 |
|---|---|---|---|
| 1 | **M1 HDRI + Skybox** | 16h | 視覚的インパクト最大 |
| 2 | **M2 IBL** | 14h | 3D 照明の品質向上 |
| 3 | **M3 UI** | 5h | 操作性向上 |
| 4 | **M4 最適化** | 8h | パフォーマンス |

**総見積: ~43h**

---

## 関連ファイル

| ファイル | 行 | 内容 |
|---|---|---|
| `Artifact/src/Layer/ArtifactLightLayer.cppm` | - | 3D ライトレイヤー (参考) |
| `Artifact/src/Layer/ArtifactCameraLayer.cppm` | - | 3D カメラレイヤー (参考) |
| `ArtifactCore/include/Color/ColorACES.ixx` | - | ACES カラーマネージャ (HDRI の色空間変換に使用) |
| `Artifact/shaders/ShaderInterop_BVH.h` | - | Wicked Engine キューブマップ構造体 (参考) |
