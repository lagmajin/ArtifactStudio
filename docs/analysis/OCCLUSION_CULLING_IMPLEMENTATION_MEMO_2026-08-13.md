# オクルージョンカリング 実装メモ

**最終更新:** 2026-08-13

ArtifactStudio に遮蔽（オクルージョン）カリングを実装するための調査メモ。既存の深度バッファ・AOV・GPU フラスタムカリング基盤を一次情報として確認し、何を再利用し何を新設すべきかを整理した。

## 現状のカリング（既に存在）

| 層 | 方式 | 実装箇所 | 状態 |
|---|---|---|---|
| CPU 2D レイヤー | レイヤー AABB × renderROI 交差で画面外スキップ | `ArtifactCompositionRenderController.cppm:33030-33096` | 実装済み |
| CPU 2D レイヤー | LOD 微小スキップ（Low 8px / Medium 2px 未満、テキスト除外） | `:32174`、`:33036` | 実装済み |
| CPU 可視判定 | `isLayerEffectivelyVisible()` が親階層を遡り `isVisible()` 確認 | `:5029-5051` | 実装済み |
| GPU 3D メッシュ | view/proj 行列で clip 空間判定 + `InterlockedAdd` compaction | `MeshRenderer.cppm:37-80`（`MeshCullCSSource`） | 実装済み（非透明・64 インスタンス以上・indirect draw 対応時） |
| 遮蔽カリング | なし | — | **未実装** |

結論: フラスタムカリング・可視フラグ・微小スキップはあるが、**遮蔽（ある物体の後ろに隠れた物体を描画しない）カリングは存在しない**。

---

## 再利用できる既存資産（調査確認済み）

### 1. 深度バッファ（既に存在）
- `ArtifactIRenderer::createOffscreenDepthTexture()`（`.ixx:461`）、`createOffscreenMultisampleDepthTexture()`（`:465`）、`liveDepthShaderResourceView()`（`:174`）。
- `pushRenderTarget(colorView, depthView)` / `clearDepthRenderTarget()` / `setOverrideDSV()` が配線済み。
- 3D 描画は `preserveSceneDepth` により連続シーンビンで共有深度アタッチメントを維持（`ArtifactCompositionRenderController.cppm:11201-11205`）。→ オクルーダの深度が既に GPU 上にある。

### 2. AOV / G-buffer 相当（既に存在）
`ArtifactRenderLayerPipeline`（`.ixx:28-104`）に以下が実装済み:
- `depth` / `normal` / `albedo` / `velocity` / `emission` / `objectId` / `materialId`
- `validForScreenSpace()`（depth + normal + albedo）と `validForTemporal()`（+ velocity）。
- `createTextures()` で RGBA16F/RGBA32F の複数ターゲットを確保（`.cppm:412-500`）。

### 3. GPU カリングパターン（既に存在）
- `MeshRenderer` の `MeshCullCSSource`（`:37-80`）が「StructuredBuffer input → compute cull → RWStructuredBuffer output + IndirectArgs」の流れを実装済み。
- `MeshCullConstants`（`:28-35`）に view/proj 行列を渡す仕組みも既存。
- → オクルージョン判定をここに **1 段階追加**する形が最短。

### 4. 深度ピラミッド / Hi-Z（未実装・新設必要）
- grep の結果、`DepthPyramid` は `GIResources.ixx:76` の GI plan enum と `RenderPipelineFoundation.ixx:98` の文字列化のみで、**実行実体は存在しない**。
- 深度の mip 化 / 最小深度（`InterlockedMin`）ピラミッド生成は新規実装が必要。

---

## 実装方針（推奨: Hi-Z GPU オクルージョンカリング）

### 段階 1: 深度ピラミッド（Hi-Z）生成パスを新設
1. 深度テクスチャ（既存 `depth` AOV またはシーンビン深度）を UAV としてバインド。
2. コンピュートシェーダで mip 0 → mip N を **2×2 最小深度**でダウンサンプル（`InterlockedMin` または `min(min(min(d0,d1),d2),d3)`）。
   - 逆 Z かどうかを既存 depth の慣例に合わせる（`MinDepth=0 / MaxDepth=1`、Diligent DX12 標準は逆 Z でない通常 0..1。要確認し min か max かを統一）。
3. 深度ピラミッド用の texture + mip UAV 群を `ArtifactRenderLayerPipeline` に追加。

### 段階 2: オクルーダ（遮蔽物）のバウンディング描画
- 既存の `isOpaqueSolid3DCard()`（`ArtifactCompositionRenderController.cppm:1289`）と opaque 判定を流用し、**不透明レイヤーだけ**をオクルーダ候補として扱う。
- 不透明レイヤーのバウンディングボックスを GPU へアップロードし、depth-only でラスタライズして深度ピラミッドに書き込む（または既存シーンビン深度をそのまま利用）。

### 段階 3: オクルージョンテストの統合
1. 各レイヤーの AABB（2D）またはバウンディングスフィア（3D）を clip 空間へ投影。
2. スクリーン空間のバウンディング矩形を求め、深度ピラミッドの適切な mip レベルを選択（矩形サイズから mip を決定）。
3. その mip の深度値と、投影した近接面深度（バウンディングの最近接 Z）を比較。
   - `nearestZ > hiZDepth` なら完全に遮蔽 → スキップ。
   - 部分遮蔽は conservative に「描画する」（誤カリングを避ける）。
4. 既存の `MeshCullCSSource` にこのテストを追加するか、別の `OcclusionCullCS` として分離。

### 段階 4: CPU 側との統合
- 2D レイヤーは CPU の AABB×ROI 判定と併用し、3D シーンに限って GPU オクルージョンカリングを有効化。
- 透明レイヤー・マスク・マット・調整レイヤーは**絶対にオクルージョン対象にしない**（視覚的正しさを壊す）。

---

## 制約・注意点（AGENTS.md 遵守）

- **新規 signal/slot 禁止**。オクルージョンカリングは既存の render loop / compute dispatch 経路で完結させる。
- **`QImage` 新規採用禁止**。深度ピラミッドは GPU テクスチャ + UAV のみ。
- **Diligent 低レベル（PSO/ラスタライザ）の変更は最小化**。深度ピラミッド生成は既存の `ComputeExecutor`（`MeshRenderer` が使用）パターンを踏襲。
- **C++20 modules 循環参照に注意**。新規 compute シェーダは HLSL として `Artifact/shaders/` に置き、C++ からは既存 `ArtifactRenderLayerPipeline` / `MeshRenderer` の枠で参照。
- **ビルド/CMake はユーザー依頼時のみ**実行。

## 実装の優先順位

1. **深度ピラミッド生成パス**（新設、小〜中）
2. **不透明レイヤーのオクルーダ抽出**（既存 `isOpaqueSolid3DCard` 流用、小）
3. **Hi-Z テストの `MeshCullCSSource` への統合**（中）
4. **2D/3D の切り分けと透明レイヤー除外**（小）
5. **診断カウンタ**（culled by occlusion 数）追加（小）

## 期待効果と適用対象

- **効果が大きい対象**: 3D シーンで不透明メッシュが大量にある場合（クローン/パーティクル/インスタンス化メッシュ）。既存の GPU フラスタムカリングでは視錐台内だが前面オブジェクトに隠れるインスタンスを削減できる。
- **効果が薄い対象**: 2D コンポジション（レイヤーが基本透明合成、遮蔽関係が定まらない）。2D では既存の ROI + LOD スキップで十分。

## 主要ファイル

| ファイル | 役割 |
|---|---|
| `ArtifactCore/src/Graphics/MeshRenderer.cppm:37-80` | GPU フラスタムカリング（拡張対象） |
| `Artifact/include/Render/ArtifactRenderLayerPipeline.ixx:28-104` | 深度/AOV ターゲット（深度ピラミッド追加対象） |
| `Artifact/src/Widgets/Render/ArtifactCompositionRenderController.cppm:1289` | `isOpaqueSolid3DCard`（オクルーダ抽出） |
| `Artifact/src/Widgets/Render/ArtifactCompositionRenderController.cppm:11201-11205` | シーンビン深度保持（オクルーダ深度源） |
| `ArtifactCore/include/Graphics/GIResources.ixx:76` | `DepthPyramid`（enum のみ、実行実体なし） |
