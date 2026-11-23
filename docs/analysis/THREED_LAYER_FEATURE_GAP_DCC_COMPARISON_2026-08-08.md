# 3D Layer Feature Gap Analysis — DCC Comparison (2026-08-08)

**最終更新:** 2026-08-08
**状態:** 分析完了

## 概要

ArtifactStudio の 3D Model Layer (`Artifact3DLayer`) の現在の実装状況を、C4D / Maya / 3ds Max / Blender / Element 3D の5 DCC と比較し、不足機能を特定する。

ArtifactStudio のポジショニング（モーショングラフィックス＋コンポジション）を前提に、3D モデリングツールとしての全機能ではなく、**3D レイヤーに必要な実用的機能**に絞って評価する。

## ArtifactStudio 3D レイヤー現状

### 実装済み ✅

| 機能 | 詳細 | ファイル |
|------|------|---------|
| 基本ジオメトリ | Plane, Cube, Sphere, Cylinder, Cone | `Artifact3DModelLayer.cppm` |
| 外部モデル読み込み | glTF + MeshImporter (position/normal/uv/polygons) | 同上 Line 151-234 |
| PBR マテリアル | BaseColor, Metallic, Roughness, Opacity, AO | `Material.ixx` |
| PBR テクスチャ | 6種 (baseColor, metallicRoughness, normal, emission, occlusion, opacity) | 同上 + auto-detect |
| エミッシブ | emissionColor + emissionStrength + emissionTexture | `Material.ixx:48-52` |
| マテリアルプリセット | makeDefault, makeMetal, makePlastic, makeGlass, makeEmissive | `Material.ixx:91-96` |
| MaterialX | ドキュメント埋め込み | `Material.ixx:87-88` |
| レンダーモード | Wireframe, Solid + WireOverlay | `Artifact3DModelLayer:93,106` |
| Transform | 3D position/rotation/scale/anchor（全キーフレーム対応） | `draw()` Line 811-829 |
| シーンライト | Directional, Point, Spot, Ambient, Area (5種) | `Light.ixx` |
| ライトレイヤー | per-layer light filtering | `lightAppliesToLayer()` |
| カメラレイヤー | 3D カメラ制御 | `CompositionRenderController` |
| ビューポート3D操作 | Orbit/Pan/Zoom（`viewportOrientationNavigator_`） | 同上 |
| 3D ギズモ | 体積型 Move/Rotate/Scale ハンドル | `Artifact3DGizmo` |
| 選択アウトライン | 3D ライン描画でエッジハイライト | `drawSelectionOutline()` |
| 破壊オーバーレイ | `drawFractureOverlay()` 全レイヤー共通 | Line 884 |
| ソフト3Dパス | `PrimitiveRenderer3D` でのフォールバック描画 | `ArtifactIRenderer` |
| 3D ライン描画 | `draw3DLine()` | 同上 |
| 手続き型3D | Terrain, PathTube | `ArtifactProcedural3DLayer` |
| GPU レンダリング | DiligentEngine DX12 バックエンド | `drawMesh()` |
| LOD | DetailLevel::Low 時のテクスチャ省略 | Line 787-789 |

### 未実装 ❌

| 機能 | C4D | Maya | 3ds Max | Blender | Element3D | 優先度 |
|------|-----|------|---------|---------|-----------|--------|
| **IBL / 環境マップ** | ✅ | ✅ | ✅ | ✅ | ✅ | 🔴 |
| **シャドウマッピング** | ✅ | ✅ | ✅ | ✅ | ✅ | 🔴 |
| **デフォーマ/モディファイア** | ✅ | ✅ | ✅ | ✅ | ❌ | 🔴 |
| **パーティクルインスタンス** | ✅ | ✅ | ✅ | ✅ | ✅ | 🔴 |
| **アニメーション再生** | ✅ | ✅ | ✅ | ✅ | ✅ | 🔴 |
| **法線マップベイク** | ✅ | ✅ | ✅ | ✅ | ❌ | 🟡 |
| **テッセレーション/サブディビジョン** | ✅ | ✅ | ✅ | ✅ | ❌ | 🟡 |
| **頂点カラー** | ✅ | ✅ | ✅ | ✅ | ❌ | 🟡 |
| **テクスチャトランスフォーム** | ✅ | ✅ | ✅ | ✅ | ✅ | 🟡 |
| **反射プローブ** | ✅ | ✅ | ✅ | ✅ | ✅ | 🟡 |
| **AO レンダリング (SSAO)** | ✅ | ✅ | ✅ | ✅ | ✅ | 🟡 |
| **ボリューム/フォグ** | ✅ | ✅ | ✅ | ✅ | ❌ | 🟢 |
| **UV 編集** | ✅ | ✅ | ✅ | ✅ | ❌ | 🟢 |
| **頂点/辺/面 選択** | ✅ | ✅ | ✅ | ✅ | ❌ | 🟢 |
| **リギング/スキニング** | ✅ | ✅ | ✅ | ✅ | ❌ | 🟢（AE的文脈では不要） |
| **物理/コリジョン** | ✅ | ✅ | ✅ | ✅ | ✅ | 🟢（別マイルストーン） |
| **ブーリアン演算** | ✅ | ✅ | ✅ | ✅ | ❌ | 🟢 |
| **LOD 自動生成** | ✅ | ✅ | ✅ | ✅ | ❌ | 🟢 |
| **メッシュデシメーション** | ✅ | ✅ | ✅ | ✅ | ❌ | 🟢 |

## DCC 別比較

### Element 3D (最も直接の競合)

AE プラグイン。3D オブジェクトの読み込み、PBR マテリアル、パーティクルインスタンス、アニメーションテクスチャが主機能。

| 機能 | E3D | Artifact | ギャップ |
|------|-----|----------|--------|
| モデル読み込み | OBJ/C4D/3DS | glTF ✅ | glTF はより現代的 |
| PBR マテリアル | ✅ | ✅ | 同等 |
| パーティクル散布 | ✅ グループ複製＋ランダム配置 | ❌ | **最大のギャップ** |
| アニメーション再生 | ✅ OBJ シーケンス | ❌ | |
| 環境反射 | ✅ 簡易マットキャップ＋IBL | ❌ | |
| シャドウ | ✅ 簡易AO | ❌ | |
| グループ/複製 | ✅ | ❌ | |
| UI | AE エフェクトパネル内 | Property Groups ✅ | 同等 |

### C4D (モーショングラフィックスのベンチマーク)

| 機能 | C4D | Artifact | ギャップ |
|------|-----|----------|--------|
| デフォーマ | Bend/Twist/Taper/FFD/Shear/Explosion等 20種以上 | ❌ | **最大のギャップ** |
| MoGraph | Cloner/Fracture/Emitter/Effector | ❌（Clonerコンポーネントは別系統で存在） | |
| ボリュームモデリング | VDB/SDF | ❌（SDFLayer は 2D） | |
| フィールド | フォース/フィールド駆動エフェクト | ❌ | |
| ノードマテリアル | Redshift/Standard ノードベース | MaterialX 対応 ✅ | 近い |
| スカルプト | ✅ | ❌ | スコープ外 |
| レンダラー | Redshift/Physical | Diligent PBR ✅ | 同等ではないが実用的 |

### Blender (無料リファレンス)

| 機能 | Blender | Artifact | ギャップ |
|------|---------|----------|--------|
| ジオメトリノード | ✅ ノードベースプロシージャル | ❌ | |
| モディファイアスタック | ✅ 非破壊チェーン | ❌ | |
| シェーダーエディタ | ✅ ノードベース | ❌（プリセットマテリアルのみ） | |
| アニメーションベイク | ✅ Alembic/USD export | ❌ | |
| 物理シミュレーション | Cloth/SoftBody/Fluid | ❌ | |
| テクスチャペイント | ✅ | ❌ | スコープ外 |
| スカルプト | ✅ | ❌ | スコープ外 |

## 推奨優先順位

ArtifactStudio のポジショニング（モーショングラフィックス＋コンポジション＋AE代替）を前提に、実用インパクトと実装コストでソート:

### 🔴 最優先（Phase 1）

1. **IBL / 環境マップ**
   - Diligent PBR シェーダーに cubemap ベースの IBL を追加
   - 既存の `Light` クラスに `LightType::Environment` 追加
   - HDR 画像（.hdr / .exr）を環境マップとして読み込み
   - 反射・拡散 IBL の両方をサポート（split-sum approximation）
   - **コスト**: 中（シェーダー改変＋cubemap管理）

2. **シャドウマッピング**
   - Directional/Spot/Point ライトごとにシャドウマップ
   - Diligent の深度バッファを再利用（既存の深度ステンシルあり）
   - Cascaded Shadow Maps（Directional） + Omni Shadow Maps（Point）
   - PBR シェーダーにシャドウ計算追加
   - **コスト**: 中〜高（複数シャドウマップ＋シェーダー統合）

3. **デフォーマ（Bend/Twist/Taper）**
   - `Artifact3DLayer` に modifier stack 追加
   - 頂点シェーダーで変形（GPU）、または CPU でのメッシュ変形
   - キーフレーム可能（AE の変形アニメーションに必須）
   - **コスト**: 中

4. **パーティクルインスタンス**
   - 既存の `Cloner` コンポーネントを 3D 空間に拡張
   - グリッド配置／ランダム散布／パス沿い配置
   - インスタンス描画（Diligent instanced draw）
   - **コスト**: 中（Cloner コンポーネントが土台として存在）

### 🟡 中優先（Phase 2）

5. **アニメーション再生（glTF animation / Alembic）**
   - glTF の animation チャンネル + skinning 再生
   - 1フレームずつサンプリング（リアルタイムではない）
   - キーフレームとして Bake
   - **コスト**: 中（MeshImporter 拡張）

6. **テッセレーション / サブディビジョン**
   - PBR シェーダーに Hull/Domain シェーダー追加
   - または CPU での Catmull-Clark 分割
   - 変位マッピングと組み合わせ
   - **コスト**: 中〜高（Hull/Domain は DX12 で複雑。CPU 分割の方が現実的）

7. **SSAO**
   - 深度バッファからスクリーンスペース AO をコンピュート
   - 軽量。1パス追加で済む
   - **コスト**: 低

### 🟢 低優先（Phase 3+）

8. テクスチャトランスフォーム（offset/scale/rotation per texture）
9. 反射プローブ
10. ボリュームレンダリング（既に `VolumeRenderer` が `ArtifactCore` に存在）
11. メッシュデシメーション

## 変更対象ファイル一覧（Phase 1）

| ファイル | 機能 |
|----------|------|
| `Artifact/App/shaders/PBR_PS.hlsl`（改変） | IBL + シャドウ + デフォーマ |
| `ArtifactCore/include/Transform/Light.ixx`（改変） | `LightType::Environment` + shadow settings |
| `ArtifactCore/src/Transform/Light.cppm`（改変） | IBL cubemap 読み込み |
| `Artifact/src/Render/ArtifactIRenderer.cppm`（改変） | `setEnvironmentMap()`, shadow pass 追加 |
| `Artifact/src/Layer/Artifact3DModelLayer.cppm`（改変） | modifier stack 追加 |
| `Artifact/include/Layer/Artifact3DModelLayer.ixx`（改変） | modifier API |
| `Artifact/src/Layer/ArtifactAbstractLayer.cppm`（改変） | Cloner 3D 拡張 |
