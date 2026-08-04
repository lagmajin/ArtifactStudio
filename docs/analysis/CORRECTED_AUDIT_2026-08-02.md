# 全モジュール 修正監査 2026-08-02

**重要**: このドキュメントは `.cppm` 実装ファイルを実際に読み直した**修正版**です。
以前の監査は古い成熟度分析レポートの情報を信用しすぎ、多くの誤った評価を含んでいました。

---

## 修正された重大な誤評価

| モジュール | 以前の誤評価 | 実態 | 証拠 |
|-----------|------------|------|------|
| **Stabilizer** | 「完全に機能しないスタブ」 | 🟢 **完全実装**: Harris corner (正しい公式)、ブロックマッチング追跡、2D類似変換推定、移動平均平滑化、LiveStabilizer（リアルタイム）、特徴点/モーション可視化 | `ArtifactStabilizer.cppm` L342-344 で `det - 0.04*trace*trace` 正しい公式 |
| **OCIO v2** | 「自前実装。実ライブラリ未統合」 | 🟢 **実OCIOライブラリ統合済み**: `#include <OpenColorIO/OpenColorIO.h>`、`OCIO::Config::CreateFromFile()` で直接 `.ocio` 読込、JSON フォールバックも併用 | `OCIOConfig.cppm` L13, L96 |
| **FarmWorker** | 「レンダリングせず即座に完了報告するスタブ」 | 🟢 **完全実装**: `QProcess` で外部レンダラー起動、OCIO config 伝達、パスマッピング、TLS認証、ケイパビリティ（GPU/VRAM/RAM/プラグイン一覧）、リトライ3回、出力検証 | `FarmWorkerMain.cppm` L222-329 |
| **ワークスペース保存** | 「DCCなのに保存できない」 | 🟢 **完全実装**: `ArtifactWorkspaceManager`、JSON保存/読込、プリセット管理、セッション復元 | `ArtifactWorkspaceManager.cppm` |
| **RotoMask** | 過小評価 | 🟢 **完全実装**: アニメーション対応ベジェパス、Linear/Bezier/Step 補間、キーフレーム毎の頂点/タンジェント/プロパティ、ラスタライズ | `RotoMask.cppm` 200+行 |
| **MFRバグ** | 「renderFrame()戻り値無視」 | 🔵 **現行コードでは再現せず**: Farm master が `request.renderFrame(frame)` の bool を受け、例外・フレームタイムアウト・リトライ・失敗数へ反映 | `ArtifactCore/src/Render/RenderFarmMaster.cppm` のフレーム処理ループ |
| **RenderGraphバグ** | 「トポロジカルソート未実装」 | 🔵 **現行コードでは再現せず**: リソースの read/write から依存辺と入次数を構築し、Kahn 法で有効パスをソート、サイクルを失敗扱い | `ArtifactCore/include/Graphics/RenderGraph.ixx` の `RenderGraph::compile()` |

## 2026-08-03 再検証結果

上記2件を現行ソースで再確認した。したがって、旧 `FINAL_AUDIT_SUMMARY_2026-08-02.md` に残る「MFR 戻り値無視」「RenderGraph トポロジカルソート未実装」は、修正監査の結果と矛盾する旧記述である。コード変更は不要で、監査サマリー側の記述更新のみを行う。

## 2026-08-04 追加再検証

### Track Matte Drag UX

旧監査で「Track Matte Drag UX」を未実装と判定していたが、現行コードでは実装済みだった。

| 領域 | 現行実装 | 証拠 |
|------|----------|------|
| データモデル | `LayerMatteReference` が `sourceLayerId`、`MatteType`、`MatteBlendMode`、`enabled`、`opacity` を保持 | `Artifact/include/Layer/ArtifactLayerMatte.ixx` |
| 保存・読込 | `mattes` 配列を JSON に保存し、旧 `assetId` も `sourceLayerId` に移行 | `Artifact/src/Layer/ArtifactAbstractLayer.cppm` |
| 描画 | `MatteStack` へ変換し、レイヤー描画経路で source layer を解決して適用 | `Artifact/src/Layer/ArtifactAbstractLayer.cppm`、`Artifact/src/Widgets/Render/ArtifactCompositionRenderController.cppm` |
| Undo | マット参照の変更を `ChangeLayerMatteReferencesCommand` で復元 | `Artifact/src/Undo/UndoManager.cppm` |
| Drag UX | Alt-drag で source layer を pick-whip、循環参照を拒否して Track Matte link を作成 | `Artifact/src/Widgets/Timeline/ArtifactLayerPanelWidget.cppm` |

したがって残課題は、データモデルや Drag UX の新規実装ではなく、表示文言・操作発見性・複数マットの編集体験の改善である。旧 `FEATURE_AUDIT_MOTION_DESIGN_2026-06-02.md` の #9 判定は現行コードに対して古い。
