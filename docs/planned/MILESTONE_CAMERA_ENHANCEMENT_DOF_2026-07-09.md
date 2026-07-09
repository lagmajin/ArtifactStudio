**ステータス:** Not Started

# M-CAM-2: Camera Enhancement - Depth of Field / Lens Blur 設計マイルストーン

カメラレイヤーのレンズ機能強化。被写界深度（DOF）とレンズボケ（bokeh）を実描画に繋げる。
現状のカメラ DOF プロパティは **全てメタデータ（描画に繋がっていない）** だが、CoC ベースの DOF シェーダーパイプラインは資産として存在する。したがって「作る」より「既存シェーダーを Diligent 側から dispatch する」のが主眼。

## 現状（重要: 誤解を避ける）

- `Artifact/include/Layer/ArtifactCameraLayer.ixx` + `Artifact/src/Layer/ArtifactCameraLayer.cppm` の DOF 状態は**データモデルのみで完全に round-trip している**:
  - `depthOfField_` (bool, デフォルト false) — property "Camera Options/Depth of Field", JSON `cameraDepthOfField`, ガイズモ円環, オーバーレイ "DOF On/Off"
  - `aperture_` (float 4.0, "Aperture / f-stop")
  - `focusDistance_` (float 1000, "Focus Distance")
  - `blurAmount_` (float 0–100, デフォルト 100, "Blur Amount")
  - `motionBlur_` (bool false, ツールチップ: "Enable camera motion blur metadata" — 明示的にメタデータ)
- **上記いずれも描画パスで blur を算出するコードは皆無**。grep で `focusDistance()` / `aperture()` / `->depthOfField()` の消費者は、自アクセサ・セッター・JSON・作成メニュー・オーバーレイテキストのみ。
- `ArtifactCore/src/Preview/PreviewQuality.cppm` の `setEnableDepthOfField()` / `isDepthOfFieldEnabled()`（デフォルト true）も **誰も読んでいない inert なトグル**。
- 関連する「本物の」レンズエフェクトは別系統: `LensDistortionEffect`（樽型/糸巻き型歪み, 2D ラスタエフェクト）、`ChromaticAberrationEffect`、`FisheyeEffect` — いずれもカメラ DOF ではなく per-layer 2D。

## 資産として既に存在するもの（再利用できる）

- **DOF シェーダーパイプライン**（Wicked Engine 由来, orphaned 状態）:
  - `Artifact/shaders/depthOfFieldHF.hlsli` — CoC 演算: `get_coc(linear_depth)` using `dof_cocscale`/`dof_maxcoc` (`ShaderInterop_Postprocess.h` `params0.x/.y`)、Jimenez 2014 tile-based DOF
  - `depthoffield_prepassCS.hlsl` — `texture_lineardepth` から CoC + 前/背景重み
  - `depthoffield_tileMaxCOC_horizontalCS.hlsl` / `_verticalCS.hlsl` — tile 最小深度 / 最大 CoC リダクション
  - `depthoffield_neighborhoodMaxCOCCS.hlsl` — 近傍 min/max CoC
  - `depthoffield_mainCS.hlsl` (+ `_cheap`, `_earlyexit`) — 散乱/畳み込みブラー（前/背景分離）
  - `depthoffield_postfilterCS.hlsl`, `depthoffield_upsampleCS.hlsl`
  - `lineardepthCS.hlsl` — 深度の線形化（CoC 用）
- **シェーダー定数契約**: `ShaderInterop_Postprocess.h` (`struct PostProcess { resolution; params0; params1; }`)、`globals.hlsli` の `texture_depth` / `texture_lineardepth` bindless バインド。`ShaderInterop_Renderer.h` のカメラ定数バッファに `texture_depth_index` / `texture_depth_index_prev` / `texture_lineardepth_index` が存在。
- **実 DOF 光線数学**: `ArtifactCore/include/Render/Camera.ixx` の `RayTrace::Camera::getRayDOF()` / `setDOF()` — だが消費者は `VolumeRenderer` のみでカメラレイヤーとは無関係。

## 足りないもの（実装すべき核心）

1. **Diligent 側の dispatch 層が不在**。Wicked Engine の `wi::renderer::Postprocess()` 相当のオーケストレーション（線形深度化→DOF チェーン→合成）がリポジトリに存在しない。Artifact の実 renderer は Diligent ベースで、現状走っている GPU ポストは `ArtifactFinalPostProcess.cppm` の 3D LUT のみ。
2. **カメラレイヤーの DOF 状態 → DOF uniform への接続**。`aperture_` / `focusDistance_` / `depthOfField_` を `dof_cocscale` / `dof_maxcoc` に変換して渡すコードがない。
3. **深度バッファの live DOF 入力化**。深度は AOV 出力/読み出し（`readbackDepthToImage()` 等）で存在するが、リアルタイム DOF 合成には未消費。

## ワークストリーム A: DOF メタデータの実描画接続（最小実用）

### M-CAM-A1: カメラ DOF 状態 → uniform 変換

- 目標: `ArtifactCameraLayer` の `aperture_` / `focusDistance_` / `blurAmount_` を CoC パラメータに変換する算出式を定義。
- 対象: `ArtifactCameraLayer.cppm`（`Impl` に `cocScale()` / `maxCoc()` 算出ヘルパ追加）、`ShaderInterop_Postprocess.h` の `params0.x/.y` へのマッピング定義。
- 完了条件:
  - `depthOfField_` = false のときは既存描画と完全一致（パスを通さない）
  - `focusDistance_` / `aperture_` から `focalPlane` と `CoC scale` が決まる AE 互換の直感マッピング
  - `blurAmount_` を `dof_maxcoc` の上限クリップとして使う

### M-CAM-A2: Diligent DOF dispatch の新規追加

- 目標: フレームごとに線形深度化→DOF チェーンを走らせる最小ディスパッチャを追加。
- 対象: `Artifact/src/Render/ArtifactFinalPostProcess.cppm`（LUT パスに直列、または独立パス）、Diligent コンピュート投入経路（`DiligentImmediateSubmitter.cppm` 参照）。
- 完了条件:
  - `depthOfField_` 有効時のみ DOF パスを実行（無効時はスキップ、LUT のみ）
  - `texture_lineardepth_index` + DOF uniform ブロックを `depthoffield_*` チェーンに供給
  - 結果を合成フレームに書き戻す
  - `PreviewQuality::isDepthOfFieldEnabled()` をゲートとして接続

### M-CAM-A3: Inspector / ガイズモ同期

- 目標: 既存 DOF プロパティが実効果を持つことを UI で確認可能にする。
- 対象: `ArtifactCameraLayer::getLayerPropertyGroups()`（既存 "Lens / DOF" グループ）、`ArtifactCompositionRenderOverlay.cppm` の "DOF On/Off"。
- 完了条件:
  - `depthOfField_` トグルで実ブラーが出ることをプレビューで目視確認
  - フォーカス距離 / 絞りの変更が即座にボケ量へ反映

## ワークストリーム B: レンズボケ品質向上（オプション拡張）

### M-CAM-B1: ボケ形状（bokeh shape / blades）

- 目標: 円形だけでなく n 角形ボケ、アナモフィック縦長ボケ等をサポート。
- 対象: `depthoffield_mainCS.hlsl` の散乱カーネル、または新規ブレードマスク生成。
- 完了条件:
  - `apertureBlades_` / `bokehRotation_` プロパティ追加（カメラレイヤー）
  - ボケ形状が被写界深度外の highlight で可視

### M-CAM-B2: モーションブラー（カメラ）

- 目標: `motionBlur_` メタデータを実可動にする。
- 対象: `Artifact/shaders/motionblurCS.hlsl`（資産存在）、`ShaderInterop_Renderer.h` の `velocity` チャネル（`VelocityX/Y` AOV として存在）。
- 完了条件:
  - カメラ移動 / 被写体速度からのブラー合成
  - `PreviewQuality` のゲート統合

## 依存・未確認事項

- **Wicked Engine 統合は dead code**。`ArtifactRenderManagerWidget.ixx` / `.cppm` で `wi::renderer::` は全てコメントアウト。したがって DOF は Wicked 経由ではなく **Diligent コンピュートとして自前 dispatch** する必要がある。
- `depthoffield_*.hlsl` は Wicked Engine の bindless / `wi::` ヘルパに依存している可能性が高い。Diligent 側へ持ってくる際、定数バッファ・テクスチャバインドの差し替えが必要（移殖コストの見積もりが別途必要）。
- 深度バッファは「AOV 出力用」で存在するが、「composite 中の live 入力」として使えるか確認が必要。`ArtifactIRenderer.cppm` の `readbackDepthTo*` 系は CPU 読み出し主眼。GPU 側で深度テクスチャを直接バインドできる構造かを `DiligentImmediateSubmitter` / render target 管理で確認すること。
- 3D レイヤー描画時に `set3DCameraMatrices(*cameraView, *cameraProj)` 経由でカメラが渡る（`ArtifactCompositionRenderController.cppm`）。DOF パスはこの 3D シーンの深度を消費することになる。2D のみのコンポジションでは DOF は意味を持たない（深度なし）。
- `motionBlur_` は現状「metadata」とツールチップで明示されているため、実装時は挙動変更の周知が必要。

## 次のステップ

1. A1 で CoC パラメータ算出式を確定（既存プロパティの再定義なし）
2. A2 で Diligent dispatch を最小実装し、既存 `depthoffield_*.hlsl` をbindless から Diligent バインドへ移殖
3. A3 でプレビュー動作確認
4. 余力があれば B1/B2 をオプション拡張として並行
