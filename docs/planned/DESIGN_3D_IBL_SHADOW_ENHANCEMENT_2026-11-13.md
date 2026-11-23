# 設計: 3D ビューポート IBL / シャドウマップ強化（Phase 1-b）

**最終更新:** 2026-11-13

**ステータス:** Design Draft（コード変更なし。ビルド・実機確認はユーザー明示指示後）

本設計は `docs/analysis/THREED_LAYER_FEATURE_GAP_DCC_COMPARISON_2026-08-08.md` の
Phase 1 推奨（IBL → シャドウマップ → デフォーマ）を再精査した結果、
**前提を訂正して立て直したものである**。2026-11-13 の実コード精査に基づく。

## 0. 前提の訂正（2026-11-13 実コード照合）

2026-08-08 のギャップ分析は「IBL / シャドウマップ未実装」を前提にしているが、
**現行コードでは split-sum IBL と単一ライトのシャドウマップは既に実装済み**である。
同時に、同分析のファイル参照には誤りがある。

| 分析文書の参照 | 実体（2026-11-13 確認） |
| --- | --- |
| `Artifact/App/shaders/PBR_PS.hlsl` | **存在しない**。3D レイヤー PBR PS は `ArtifactCore/src/Graphics/MeshRenderer.cpp` 内のインライン HLSL（`MeshPSSource`、L1490–3038）。`Artifact/App/shaders/` は paint 用ラスタライザ系で別統 |
| `ArtifactCore/src/Transform/Light.cppm` | 実体は `ArtifactCore/src/Transform/Light.cpp`（.cppm ではない）。契約は `ArtifactCore/include/Transform/Light.ixx` |
| `Artifact/src/Render/ArtifactIRenderer.cppm` | 実体は `Artifact/src/Render/ArtifactIRenderer.cpp` + `Artifact/include/Render/ArtifactIRenderer.ixx` |

**既に存在する実装（設計の土台）:**

| 機能 | 根拠 |
| --- | --- |
| split-sum IBL 本体: equirect→cube 変換、specular prefilter（CPU GGX 32サンプル×7mip + sheen 用 cube）、irradiance（CPU 余弦畳み込み 16px）、BRDF LUT（128×64、Eavg 含む multi-scatter 補償） | `ArtifactCore/src/Graphics/MeshRenderer.cpp` L667–1173 |
| PS 側 IBL サンプリング: diffuse（最深 mip）/ specular（roughness→LOD、anisotropy 3タップ近似）/ sheen / transmission | 同 L1767–1988 |
| 環境マップレイヤーと配線: `ArtifactEnvironmentMapLayer`（hdriPath/intensity/rotation/visibleAsBackground）→ コントローラが最初の有効レイヤーを renderer へ設定。同一パス再呼び出しは再 bake なし | `Artifact/include/Layer/ArtifactEnvironmentMapLayer.ixx`、`ArtifactCompositionRenderController.cpp` L42712–42739、`MeshRenderer.cpp` L6003–6008 |
| HDR/EXR 読み込み（OpenImageIO、.exr/.hdr/.dpx/.tif） | `ArtifactCore/src/IO/Image/ImageImporter.cpp` L41–47, L81 |
| シャドウマップ: 1024² D32 単一マップ、depth-only prepass（`MeshShadowVS/PS`）、caster 登録（opaque・opacity テクスチャ無し）、`SampleCmpLevelZero` 単発 or 3×3 PCF、deferred worker とのオーバーラップ | `Artifact/src/Render/ArtifactIRenderer.cpp` L1639–1784, L2989–3020, L5586–5914、`ArtifactCore/src/Graphics/MeshRenderer.cpp` L1328–1488, L1742–1761, L4333–4417 |
| シャドウライト選定: `castsShadows` 有効な Directional / Spot の**最初の 1 ライト**（Point / Area は caster 選定外） | `ArtifactCompositionRenderController.cpp` L42740–42792 |
| ライト契約: `castsShadows()` / `shadowSoftness()` は renderer-neutral 値。GPU へは `MeshRenderer::setSceneLights` が `SceneLightGpu` にパック（shadow 情報は入っておらず、b3 `ShadowParams` 経由で 1 ライト分だけ渡る） | `ArtifactCore/include/Transform/Light.ixx` L97–100、`ArtifactCore/src/Graphics/MeshRenderer.cpp` L1722–1740, L5844–5914 |
| Studio 環境: 環境なし・ライトなし時のカメラ相対 studio rig（key/fill/hemisphere/rim/cavity）、ライトあり時の `0.24*ao` ベースフィル | `MeshRenderer.cpp` L2524–2533, L3008–3036 |

よって本設計のスコープは「導入」ではなく **Phase 1-b: 既存 split-sum IBL と単一
シャドウマップの強化**に再定義する。デフォーマ/モディファイアは本設計スコープ外
（Phase 2、別設計）。

## 1. 残っているギャップと設計

### A: IBL 強化

**A1: bake の GPU 化**

- 現状の bake は `setEnvironmentMap` 呼び出し時の**同期 CPU 実行**（cold path、
  レイヤー変更時のみ）。これ自体はホットパス規約上正しい。ただし 64px prefilter
  / 16px irradiance / 128×64 LUT は品質上限が低く、高解像度化を CPU で行うと
  レイヤー変更時の応答が破綻する。
- 第一候補: リポジトリ内の未接続シェーダ
  `Artifact/shaders/filterEnvMapCS.hlsl`（D_GGX / ImportanceSampleGGX 実装済み、
  push constants で roughness filter）と `irradianceEnvMapCS.hlsl` /
  `irradianceEnvMapCB.hlsl` を移植の種として使う。現行コードからは参照されていない
  （`docs/planned/MILESTONE_ENVIRONMENT_MAP_2026-03-28.md` の計画残骸）。
  実装する CS は `ArtifactIRenderer::Impl` の自己完結 executor に置く
  （P1-5 露出 compute の先行例: `docs/planned/MILESTONE_VIEWPORT_DCC_PARITY_2026-09-22.md` L108）。
- 代替案: in-tree の DiligentFX `PBR_Renderer.hpp` にある
  `CreateIrradianceCube` / `CreatePrefilteredEnvMap` / `PrecomputeBRDF` を利用。
  ただし**アプリは DiligentCore interface のみ include し DiligentFX は未リンク**
  （`ArtifactCore/CMakeLists.txt` L4709–4713）。新規リンク対象になるため
  採用時はビルド影響の確認が必須。第一候補（種シェーダ移植）を優先する。
- bake は cold path 維持。フレーム中に bake dispatch を入れない。

**A2: 品質向上**

- irradiance 16px → 64px（高周波環境での漏れ軽減。GPU 化と同時に実施）。
- prefilter / LUT 解像度は GPU 化後に段階調整（例: 256px prefilter、512² LUT）。
- ambient ベースフィル `0.24*ao` と IBL の加算比率は現行のまま維持
  （見た目変化を起こさない。変更する場合は別途承認を取る）。

**A3: スコープ外（将来候補として記録のみ）**

- `LightType::Environment` の追加（ライト経由 IBL）。現行規約
  「Core.Light に GPU texture ownership を置かない」（`MeshRenderer.cpp` GOBO
  コメント L81–83）と整合させるには path/intensity/rotation の値のみ Core に置き
  cube 資源は MeshRenderer 側に留める。既存 `ArtifactEnvironmentMapLayer` 方式が
  成立しているため、追加の設計判断が必要なままで着手しない。
- 反射プローブ / 複数環境（`MaterialRange` ごとの env バインド拡張）。
- SSGI/DDGI（`ArtifactIRenderer.ixx` L107–144 に設定配管のみの可能性、未検証）。

### B: シャドウマップ強化

**B1: CSM（Directional カスケード分割）**

- 現状の制約: 1024² 単一マップ、Directional は固定 ±2048 ortho
  （`ArtifactCompositionRenderController.cpp` L42744–42788）で遠景の解像度不足。
- 深度ターゲット: `m_shadowMapTex`（`ArtifactIRenderer.cpp` L1699–1719、
  `kShadowMapResolution=1024` L1224）を `Texture2DArray`（例: 4 cascade × 1024）に
  置換。DSV/SRV 生成と caster ループを cascade 数分へ拡張。
  `renderShadowMapFrameInline`（L1666–1784）の順序
  （shadow pass → `bindActiveRenderTargets` 復帰）と
  `shadowMapPassWorker_` による deferred FXAA オーバーラップは現行設計を維持し、
  その中へ pass を積む形にする。
- ライト VP: コントローラの単一 ortho 計算をカスケード分割へ差し替え。
  in-tree の DiligentFX `ShadowMapManager.hpp` に
  `DistributeCascades` / `CascadeTransforms` / `SnapCascades` が実装済みだが
  （未リンク、A1 と同じリンク判断）、**数式は自前実装可能**なため参照実装として
  使い、リンクしない方針を既定とする。
- シェーダ ABI: b3 `ShadowParams`（`float4x4 LightViewProjection` + `float4
  ShadowSettings`、`MeshRenderer.cpp` L1737–1740）を
  `float4x4 LightViewProjections[NCASCADE]` + cascade パラメータへ拡張。
  VS の `Out.ShadowPosition = mul(worldPos, LightViewProjection)`（L1289）は
  複数行列化。カスケード選択は **PS 側でカメラ視空間深度から行う方式**
  （VS が ShadowPosition を渡す現行設計との整合が良い。推測含む、実装時に検証）。
- PS の shadow サンプリング: `sampleShadowVisibility`
  （L1742–1761、`SampleCmpLevelZero` / 3×3 PCF）を Texture2DArray 対応版へ拡張し、
  **PCF spread を cascade texel サイズ基準に正規化**（現行はマップ全体 texel 基準）。

**B2: Omni（Point ライト）**

- コントローラ L42741–42743 のコメント通り 6 faces 必要で未対応。
- 追加要素: cubemap face DSV（6 面 or 2×3 atlas）、per-face VP、PS での face 選択 +
  depth cube サンプリング（DX では cube depth が array 化を要する可能性。推測）。
- ABI 影響が B1 より大きいため、**CSM の後の段**に置く。実装着手前に
  深度 cube の形式選択（cube array vs 6×2D array atlas）を含む詳細設計を更新する。

**B3: 複数シャドウライト**

- 現状 `ShadowSettings.z` は単一 lightIndex。複数化する場合は
  `SceneLightGpu`（6×float4、`MeshRenderer.cpp` L5844–5914）の未使用チャンネルへ
  shadow slice index を追加するか、b3 側で slice マッピング配列を持つ。
- `SceneLight` / `SceneLightGpu` / `ShadowParams` は **CPU–HLSL 手動 ABI**
  （MaterialState は static_assert で MeshRenderer.ixx L381 に整合固定）。
  フィールド追加時は PS 側 struct との整合と static_assert の保守を必須とする。
- 1 シャドウライトでなく複数化した場合、フレーム内 pass 数 = ライト数 ×
  cascade 数となり GPU コストが跳ねるため、**上限（例: 2 ライト）を初期実装で
  設ける**ことを推奨。

## 2. 実装段階（提案）

| Step | 内容 | 主な変更対象 | 受入 |
| --- | --- | --- | --- |
| S1 | IBL bake の GPU 化（種シェーダ移植）+ irradiance 64px | `Artifact/src/Render/ArtifactIRenderer.cpp`（executor 追加）、`ArtifactCore/src/Graphics/MeshRenderer.cpp`（bake 呼び出し差し替え）、`Artifact/shaders/*` 種 | 同一パス・同一設定で bake 結果が CPU 版と同等以上。`setEnvironmentMap` が cold path のまま |
| S2 | CSM 実装（4 cascade） | `ArtifactIRenderer.cpp`（shadow pass 配列化）、`ArtifactCompositionRenderController.cpp`（カスケード VP）、`MeshRenderer.cpp`（ShadowParams ABI + PS） | 遠景の影の解像度向上。フレーム時間の劣化が診断（`frameDebugSnapshot` / `shadowMapDebugState`）で把握可能。D3D12 / Vulkan 同一表示 |
| S3 | PCF 正規化 + softness 調整 | `MeshRenderer.cpp` PS | cascade 境界で影の濃淡段差が出ない |
| S4 | Omni（Point）シャドウ | 同上 + 形式選択設計の更新 | 点ライトの影が表示される |
| S5 | 複数シャドウライト（上限付き） | `SceneLightGpu` ABI + cbuffer | 上限内で複数ライトの影。ABI static_assert 整合 |
| S6 | 受入確認（ビルド・実機） | — | ユーザー明示指示後に実施 |

S1 と S2 は独立して着手可能。既存の見た目を変えない順序として
S1 → S2 → S3 → S4 → S5 を推奨する。**どの段階でも既存シングルマップ経路を
フォールバックとして残すか、完全置換かは S2 着手時に決める。**

## 3. リスク

- **パイプライン再構築（B1）**: depth prepass PSO（`NumRenderTargets=0` /
  `DSVFormat=D32_FLOAT`）と `pShadowConstantsBuffer_`（16 floats）の ABI 変更は
  `meshRenderers_`（レイヤー毎に cache key 生成、`ArtifactIRenderer.cpp`
  L1619–1637）の複数 MeshRenderer に波及する。リソース再生成経路
  （`recreateSwapChain`、RT フォーマットごとの PSO cache）との整合確認が必須。
- **Diligent/DX12 シビア領域**: `ArtifactIRenderer` / `MeshRenderer` の変更は
  推測で広く触らず、本設計の参照位置（行番号）を実装前に再確認して
  変更範囲を最小化する。
- **モジュール循環 / 境界**:
  - Core.Light への Diligent 型・GPU 資源の導入は禁止相当
    （`Core.Graphics.MeshRenderer → Core.Light` の一方向依存を維持）。
    `.ixx` への不要 import 追加もしない（AGENTS.md）。
  - `MeshPSSource` は `__ARTIFACT_MATERIAL_GRAPH_*__` マーカー付き文字列
    テンプレート。シェーダ変更は splice との共存を壊しやすい。
- **ホットパス**: 現 shadow pass は `shadowMapPassWorker_` で即時コンテキストを
  占有し deferred FXAA とオーバーラップ。cascade 増加で pass ループが増えるため
  `deferredWorkerShadowMapOverlapCount_` 等の診断を維持する。
  毎フレームの `std::vector<Light>` 生成（コントローラ L42661–42709）を
  悪化させないこと。bake はフレームに入れない（A1）。
- **見た目の回帰**: studio rig / `0.24*ao` ベースフィルと IBL の比率変更は
  見た目変化を伴う。S1 では比率を変更しない。

## 4. 未検証事項

- `filterEnvMapCS.hlsl` / `irradianceEnvMapCS.hlsl` の動作可否・CMake 組み込み状況。
- DiligentFX のビルド・リンク実態（`DILIGENT_BUILD_FX` は ON、アプリ未リンク）。
- PS 側カスケード選択（視空間深度）の VS 設計との整合。
- 深度 cube の D3D12/Vulkan 形式制約（B2 着手前の再確認事項）。
- RT（レイトレーシング）資源によるシャドウ代替の適用範囲
  （現コントローラは RT をライティングパスに未接続）。
- SSGI/DDGI 設定配管の実体。

## 5. 関連文書

- `docs/analysis/THREED_LAYER_FEATURE_GAP_DCC_COMPARISON_2026-08-08.md`
  （Phase 1 推奨の由来。§0 で訂正済み）
- `docs/planned/MILESTONE_3D_VIEWPORT_HARDENING.md`
- `docs/planned/MILESTONE_ENVIRONMENT_MAP_2026-03-28.md`（種シェーダの計画残骸）
- `docs/planned/MILESTONE_VIEWPORT_DCC_PARITY_2026-09-22.md`（P1-5 compute executor の先行例）
- `docs/technical/HOT_PATH_RULES.md`
