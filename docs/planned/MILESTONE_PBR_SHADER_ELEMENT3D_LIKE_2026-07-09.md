> **SUPERSEDED** — 2026-08-04: 統合先 [MILESTONE_3D_MATERIAL_SYSTEM_2026-03-31.md](MILESTONE_3D_MATERIAL_SYSTEM_2026-03-31.md)

# MILESTONE: Element 3D-like PBR Shader

> 2026-07-09 作成

## 目的

After Effects の **Element 3D** プラグインに近い、AE 内で 3D オブジェクトを
リアルタイムに PBR レンダリングするシェーダー基盤を Artifact に導入する。
金属 / 非金属、粗さ、環境反射（IBL）を含む metallic-roughness ワークフローを
既存の 3D レイヤー・マテリアル導線に接続し、「実物のような」見た目を実用速度で得る。

## 背景

- `PBRMaterialEffect`（`Artifact/include/Effects/Render/PBRMaterialEffect.ixx`）は
  albedo / metallic / roughness のプロパティだけを持ち、実際の GPU シェーダーへ
  接続されていないモック状態。
- `ArtifactCore/include/Render/Material.ixx` の `Material` はレイトレース用
  （Lambertian / Metal / Dielectric）であり、リアルタイム PBR とは別系統。
- `libs/DiligentEngine/DiligentFX/PBR` に `GLTF_PBR_Renderer` が存在し、
  Khronos GLTF 準拠の BRDF、metallic-roughness ワークフロー、IBL（irradiance cube +
  prefiltered env + BRDF LUT）をランタイム生成する。これを流用するのが最短。
- `Artifact3DModelLayer` / `Artifact3DModelViewer` / `PrimitiveRenderer3D` が
  すでに 3D メッシュ・ビューアの導線を持つため、ここに PBR パスを挿入する。

## ターゲット像（Element 3D 相当）

- metallic-roughness ベースの PBR マテリアル（Albedo / Metallic / Roughness / Normal / AO / Emissive）
- 環境マップによるイメージベースライティング（IBL）：反射・拡散が物理的に妥当
- シーンの複数ライト（directional / point / spot）との統合
- テクスチャマップ（アルベド / 法線 / 粗さ / 金属 / AO）の適用
- 3D レイヤーからマテリアルを差し替えられる編集導線
- 既存の solid / wireframe 表示と破綻なく共存

## 非ゴール（このマイルストーンの範囲外）

- MaterialX グラフの完全実装（後段 `MILESTONE_MATERIALX_DOCUMENT_EXCHANGE` に委譲）
- リアルタイムシャドウマップ（後段の shadow マイルストーンに委譲）
- C4D 等の専用インポータ（OBJ / GLTF の既存アセット導線のみ）
- 高度なクリアコート / シェアシックネス / アニソトロピー拡張
- DiligentEngine そのものの低位 backend 再設計

## 現状とギャップ

| 項目 | 現状 | ギャップ |
|---|---|---|
| PBR プロパティ保持 | `PBRMaterialEffect` モック | GPU へ渡していない |
| 実シェーダー | なし（solid / unlit 寄り） | `GLTF_PBR_Renderer` へ接続必要 |
| IBL 環境光 | なし | prefiltered cube / BRDF LUT 生成・管理 |
| マテリアル型 | レイトレ用のみ | リアルタイム用 `PBRMaterial` が必要 |
| テクスチャマップ | 未接続 | SRV バインド導線 |
| ライト統合 | primitive ライトのみ | PBR uniform への複数ライト投入 |

## 設計原則

1. **DiligentEngine の `GLTF_PBR_Renderer` を再利用**し、自前 BRDF 実装は最小に抑える。
2. `PBRMaterialEffect` のプロパティ契約を壊さず、裏側の実装だけ PBR 本実装へ置換する。
3. レイトレ用 `Material`（RayTrace 名前空間）とリアルタイム用 `PBRMaterial` を混ぜない。
4. 既存の solid / wireframe 表示と PBR 表示を同じ 3D surface 上で切り替え可能にする。
5. 環境マップはアセットとして扱い、既存アセットパイプラインへ乗せる。

## Scope（想定する変更ファイル）

- `Artifact/include/Effects/Render/PBRMaterialEffect.ixx`
- `Artifact/src/Layer/Artifact3DModelLayer.cppm`
- `Artifact/src/Render/PrimitiveRenderer3D.cppm` + `Artifact/include/Render/PrimitiveRenderer3D.ixx`
- `Artifact/src/Widgets/Render/Artifact3DModelViewer.cppm`
- `Artifact/src/Widgets/Render/ArtifactCompositionRenderController.cppm`
- `ArtifactCore/include/Material/PBRMaterial.ixx`（新規想定）
- `libs/DiligentEngine/DiligentFX/PBR`（再利用・依存のみ、原則変更しない）

## Phases

### Phase 1: PBR Material Model

実シェーダー用のマテリアルデータ構造をリアルタイム系として切り出す。

- `ArtifactCore/include/Material/PBRMaterial.ixx` を新設（Albedo / Metallic / Roughness /
  NormalMap / AOMap / Emissive / EnvIntensity を保持）
- `PBRMaterialEffect` のプロパティをこの構造へ接続
- レイトレ `Material` と名前空間・責務を分離

**Done when:**

- マテリアルの PBR パラメータが 1 構造にまとまる
- 既存の `PBRMaterialEffect` UI プロパティが壊れない

### Phase 2: GLTF_PBR_Renderer 接続

既存 3D レイヤーの描画パスに DiligentEngine の PBR レンダラを挿入する。

- `GLTF_PBR_Renderer` インスタンスの作成・ライフサイクル管理
- RTV / DSV フォーマットと既存レンダーコントローラの統合
- solid / wireframe と PBR の表示切替導線
- `PBRMaterialEffect` のパラメータを PBR uniform へ転送

**Done when:**

- 3D オブジェクトが PBR で塗られる
- solid / wireframe 切替で破綻しない

### Phase 3: Image-Based Lighting (IBL)

環境マップによる物理的妥当な反射・拡散を導入する。

- 環境マップ（.ktx / .hdr 等）のロード導線
- irradiance cube + prefiltered env + BRDF LUT のランタイム事前計算
- `PrecomputeCubemaps` 呼び出しとキャッシュ管理
- `EnvIntensity` での環境光強度調整

**Done when:**

- 環境マップ下で金属 / 非金属の反射がつく
- 環境光なしでも壊れないフォールバック

### Phase 4: Multi-Light Integration

シーンライトを PBR uniform に投入する。

- directional / point / spot の `LightAttribs` 構築
- 既存ライト導線（`ArtifactCore/src/AI/3DLightingDescriptions.cppm` 等）との接続
- 複数ライトの uniforms 上限と転送

**Done when:**

- 複数ライト下で PBR の陰影が妥当
- ライトなしでも既存挙動を壊さない

### Phase 5: Texture Maps

マテリアルへのテクスチャマップ適用。

- Albedo / Normal / Roughness / Metallic / AO の SRV バインド
- UV / tangent の前提確認と生成
- マップあり・なしの切替

**Done when:**

- テクスチャマップ付き PBR マテリアルが使用可能

## Recommended Order

1. Phase 1 (Material Model)
2. Phase 2 (Renderer 接続)
3. Phase 3 (IBL)
4. Phase 4 (Multi-Light)
5. Phase 5 (Texture Maps)

### Why This Order

- Phase 1 がないと PBR uniform の形が決まらず、Phase 2 以降がブレる。
- Phase 2 で最低限の PBR 表示が出てから IBL / ライトを足す方が検証しやすい。
- IBL とライトは直交するので、どちらを先にしても良いが IBL から入れる。
- テクスチャマップは最後でも、パラメータ契約が固まっていれば差し込みやすい。

## 連携先

- `Artifact/include/Effects/Render/PBRMaterialEffect.ixx`
- `Artifact/src/Layer/Artifact3DModelLayer.cppm`
- `Artifact/src/Widgets/Render/Artifact3DModelViewer.cppm`
- `ArtifactCore/include/Material/PBRMaterial.ixx`（新規）
- `libs/DiligentEngine/DiligentFX/PBR`（GLTF_PBR_Renderer 再利用）
- 関連: `docs/planned/MILESTONE_3D_MATERIAL_SYSTEM_2026-03-31.md`
- 関連: `docs/planned/MILESTONE_3D_VIEWPORT_SOLID_CAMERA_OVERLAY_2026-04-10.md`
- 関連: `docs/planned/MILESTONE_AE_PLUGIN_FEATURES_2026-07-04.md`（Element 3D 行）

## Validation Checklist

- 3D オブジェクトが PBR で物理的に妥当な陰影を持つ
- metallic / roughness を変えると見た目が即座に変わる
- 環境マップ下で反射・拡散がつく
- 複数ライト下で陰影が壊れない
- solid / wireframe 切替と PBR が共存する
- 既存の `PBRMaterialEffect` UI プロパティが後方互換

## Notes

Element 3D は「AE 内 3D レンダリング + OBJ/C4D インポート + PBR マテリアル」が
売り。`MILESTONE_AE_PLUGIN_FEATURES_2026-07-04.md` でも
「DiligentEngine で対応可。3D アセットパイプライン拡張」と位置づけられている。
このマイルストーンはその PBR 部分（シェーダー基盤）に集中し、ジオメトリ編集・
専用インポータは別マイルストーンに委譲する。

---

## Next Execution Slice

Phase 1 から入る。まずは `PBRMaterial` の器を作り、既存 `PBRMaterialEffect` の
プロパティを壊さずに接続する。

### Phase 1A の着手点

1. `ArtifactCore/include/Material/PBRMaterial.ixx` を新設し、PBR パラメータを定義
2. `PBRMaterialEffect` の getter/setter を新構造へ向ける
3. レイトレ用 `Material`（RayTrace 名前空間）と混ぜないよう責務を確認
4. 既存 UI プロパティ（Albedo / Metallic / Roughness）が壊れないことを確認

### Phase 1 完了条件

- PBR パラメータが 1 構造にまとまる
- 既存 `PBRMaterialEffect` のプロパティ導線が後方互換
- solid 表示の既存挙動を壊さない

### Phase 2 の前提

- `GLTF_PBR_Renderer` の `CreateInfo` と既存 RTV/DSV フォーマットを合わせる
- 既存レンダーコントローラへの挿入点を先に特定する
- 表示切替は `MILESTONE_3D_VIEWPORT_SOLID_CAMERA_OVERLAY` の overlay 責務と衝突しない

## 2026-07-25 実装監査

`Material` の metallic／roughness／base color／emission／opacity／normal／AO と各種 texture path、GLTF／OBJ 等からの metallic-roughness・normal・emission・AO texture 抽出、`MeshRenderer` の base color／metallic-roughness texture SRV と PBR 計算、3D model layer／viewer／primitive renderer の既存導線は確認した。一方、`PBRMaterialEffect` としての独立契約、Diligent の `GLTF_PBR_Renderer`／IBL（irradiance・prefiltered environment・BRDF LUT）、複数ライトの PBR uniform 統合、Material 差し替え UI、solid／wireframe との表示切替、実機での見た目・性能一致は確認できない。したがって既存 MeshRenderer に PBR 相当の部分実装はあるが、Element 3D-like milestone の統合完了は未達・runtime 未検証とする。
