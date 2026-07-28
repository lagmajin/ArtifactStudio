# 調査メモ: アップスケール技術の導入可否 — 2026-07-28

**作成日:** 2026-07-28
**ステータス:** 調査メモ（実装未着手）
**関連:** `docs/planned/PROPOSAL_RENDER_EXPORT_EFFICIENCY_2026-07-28.md`（書き出しパイプラインへの組込先）

---

## 1. 結論（要約)

**導入可能。しかも下地がすでに 3 つ揃っている。**

| 下地 | 状態 |
|---|---|
| Anime4K 風エッジ復元 CS シェーダー | **アセットは存在するが未配線**（コードから参照ゼロ） |
| ONNX Runtime + DirectML | vcpkg 依存として導入済み・初期化コード実在 |
| プレビュー動的解像度（render scale 70-100%） | `ArtifactIRenderer::setUpscaleConfig` で稼働中 |

推奨は 2 段階: **Phase 1 = シェーダー系空間アップスケーラの配線（低リスク）**、**Phase 2 = ONNX ML 超解像の書き出し時オプション（高品質・オフライン）**。時間軸系（DLSS/FSR2/XeSS）は 2D コンポジタに不向きなため非推奨。

## 2. 既存資産の詳細（確認済みの事実）

### 2.1 anime4k_edge_upscaleCS.hlsl（未配線）

- 場所: `Artifact/shaders/anime4k_edge_upscaleCS.hlsl`（64 行、自作実装。Anime4K 由来コピーではない）
- 内容: luma 勾配で方向を判定し、エッジに沿って補間するエッジ指向再構成。`edgeStrength` / `lineBlend` の cbuffer 付き
- **grep 結果: `.cppm/.ixx/.cpp` からの参照 0 件** → シェーダーアセットのみでパイプライン未接続（未検証: ファイル名規約による自動ロードの可能性は低いが完全には否定していない）

### 2.2 ONNX Runtime + DirectML（導入済み）

- `vcpkg.json` に `onnxruntime` (features: `directml`)、overlay port あり（`vcpkg-overlays/ports/onnxruntime/`）
- `ArtifactCore/src/AI/OnnxDmlLocalAgent.cppm`（`Core.AI.OnnxDmlAgent`）に `Ort::Env` + DML provider の初期化実装が既にある（`ARTIFACT_HAS_ONNX_DML_BACKEND` ガード）
- → ML 超解像モデル（ONNX）を動かす実行基盤は**新規依存ゼロ**で使える

### 2.3 プレビュー動的解像度

- `ArtifactIRenderer::setUpscaleConfig(enable, sharpness)`: 内部 RT を 70〜100% に縮小してレンダー（`m_upscaleScale`、L2319-2323 で RT サイズ決定）
- 現状の拡大表示は品質パス無し（=単純スケール）。ここが 2.1 のシェーダーの本来の接続先だったと推測される（**推測**）

### 2.4 その他

- OpenCV (opencv4) 導入済み。`AntiAliasing.ixx` で cv::resize 使用実績
- GPU 合成は Diligent (DX12)。CS ベースのポストパスは既存多数（`Artifact/shaders/` に 400+ シェーダー）

## 3. 技術候補の比較

| 候補 | 種別 | ライセンス | 適合性 |
|---|---|---|---|
| **自作 Anime4K 風 CS**（既存） | 空間・GPU | 自前 | ◎ 配線するだけ。プレビュー・書き出し両用 |
| **AMD FSR 1.0**（EASU+RCAS） | 空間・GPU | MIT | ◎ 単一 CS×2 パス、HLSL 移植容易、実写系にも強い |
| **NVIDIA Image Scaling (NIS)** | 空間・GPU | MIT | ○ FSR1 と同枠。どちらか片方で十分 |
| **Real-ESRGAN (ONNX)** | ML・DML | BSD-3（モデル重みは要個別確認） | ◎ 書き出し時の高品質アップスケールに最適。リアルタイム不可 |
| OpenCV dnn_superres (FSRCNN 等) | ML・CPU/OpenCL | BSD | △ ONNX+DML 基盤があるため冗長 |
| FSR 2/3, DLSS, XeSS | 時間軸・GPU | 各種 | ✕ モーションベクタ+深度+ジッタ前提。2D コンポジタの静止画/レイヤー合成と相性が悪く、DLSS はベンダーロック |

## 4. 導入提案

### Phase 1: シェーダー系空間アップスケール（低リスク・すぐ着手可）

1. **プレビュー配線**: `setUpscaleConfig` の拡大表示パスに `anime4k_edge_upscaleCS` を接続（本来の想定接続先と推測される場所）。sharpness を `edgeStrength` にマップ
2. **書き出しアップスケール**: レンダーキューに「出力解像度 > コンポ解像度」時のアップスケールパスを追加。まず FSR1 風 EASU+RCAS を CS で実装（アニメ調は Anime4K 風、実写調は FSR1 風の 2 モード）
3. 実装先: シェーダーは `Artifact/shaders/`、パスは既存ポストエフェクト CS の流儀に従う。Diligent 低レベルは触らない

### Phase 2: ML 超解像（書き出し時オプション）

1. `OnnxDmlLocalAgent` の初期化パターンを流用し、`Core.AI` 配下に超解像用セッション（Real-ESRGAN x2/x4 ONNX）を追加
2. 書き出しパイプラインの **parallel 段**（PROPOSAL_RENDER_EXPORT_EFFICIENCY §3 の pipeline 化と統合）に組込み。`ImageF32x4_RGBA` → NCHW float 変換は明示関数で（QImage 経由禁止・AGENTS 準拠）
3. VRAM 対策: タイル分割推論（128px オーバーラップ付き 512px タイル等）が必須
4. モデル配布: 実行ファイル同梱かダウンロード式かの判断が必要（重み数十 MB）。**モデル重みのライセンス再配布条件は導入前に要確認**

### 非推奨

- 時間軸アップスケーラ（FSR2+/DLSS/XeSS）: モーションベクタ基盤の整備コストが大きく、静止画・シェイプ中心の現行優先方針（2026-07-27）と合わない

## 5. リスク・未検証事項

- anime4k CS の実際のロード経路（参照ゼロの確認は grep ベース。ビルド生成物経由の間接参照は未確認）
- `ARTIFACT_HAS_ONNX_DML_BACKEND` が現行ビルド構成で有効かどうか未確認
- Real-ESRGAN モデル重みの再配布ライセンス
- DML 推論と Diligent DX12 の GPU 同時使用時の VRAM 圧迫（書き出し時はプレビュー負荷が低いので実害は小さい見込み。**推測**）

## 更新履歴

- 2026-07-28: 初版
