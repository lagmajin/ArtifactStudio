# MILESTONE: 画像エフェクト 次世代ギャップ実装

**最終更新:** 2026-08-18

**ステータス:** Planned

**識別子:** M-FX-AI

## 目的

汎用画像フィルタ（ぼかし・色調・歪み・グリッチ・時間系）は既に充足しているため、残る高次元・セマンティック系の穴を埋める。画像エフェクトを「画素演算の網羅」から「素材の意味を理解して加工する」段階へ引き上げる。

対象は下記の7項目とする。いずれも既存の OpenCV / ONNX / FaceDetection / RotoBrush 基盤を再利用し、新規アルゴリズムの一から開発は避ける。

## 現行資産監査（2026-08-18 ソース確認）

| 項目 | 現状 | 根拠 |
|---|---|---|
| 顔検出 | ✅ 実装済み | `ArtifactCore/src/ImageProcessing/OpenCV/FaceDetectionEngine.cppm` — Haar Cascade + OpenCV DNN（TensorFlow/Caffe）両対応 |
| ロトブラシ | ✅ 実装済み | `OpenCVRotoBrushEngine.cppm` — grabCut（前景/背景ストローク）+ Farneback optical flow によるフレーム伝播 |
| ONNX 画像推論 | ❌ 未接続 | `OnnxDmlLocalAgent.cppm` / `AIChatWidget.cppm` — LLM チャット用途のみ。画像のセグメンテーション・スタイル転送には未使用 |
| 超解像 | ❌ 実体なし | `AIAnalysisDescriptions.cppm` に Anime4K / upscale の説明文字列のみ |
| コンテンツアウェア | ❌ 実体なし | `inpaint` / `ContentAware` の実装ファイル未検出（説明文書のみ） |
| ニューラルスタイル転送 | ❌ 実体なし | ONNX 推論は LLM のみ、画像スタイル変換経路なし |
| Guided Filter | ❌ 未確認 | `StructureTensor.cppm` / `AnisotropicFlowBlur.cppm` は存在するが、Guided Filter / Rolling Guidance は未検出 |
| 自動レベル補正 | 🟡 部分 | `AutoContrastCV.cppm` / `AutoGammaCV.cppm` はあるが、ヒストグラム自動ストレッチの専用 UI なし |
| 高度スピル抑制 | 🟡 部分 | `IBKKeyer.cppm` に despill / edge softness はあるが、Refine Edge 相当は未実装 |

## 対象と優先順位

### P0 — コンテンツアウェアフィル（inpaint）

指定領域を周辺画素から自然に埋める。既存の `OpenCV` 基盤（grabCut / morphology 等を既に使用）に `cv::inpaint` を接続するだけで成立する。

- 既存資産: `OpenCVRotoBrushEngine`、`cv::Mat` 変換（`CvUtils`）
- 不足: inpaint エフェクトの型・プロパティ・マスク連携・GPU/CPU 経路
- フェーズ:
  1. `ArtifactInpaintEffect`（`ArtifactAbstractEffect` 派生）を作成し、`cv::inpaint`（Telea / NS）を呼ぶ
  2. マスク入力をレイヤーマスク / RotoBrush マスクと連携
  3. プロパティ（radius、method、mask feather）と JSON 保存
- 完了条件: 欠損・不要物を指定して自然に埋められ、preview と Render Queue が一致する

### P0 — AIセマンティックセグメンテーションマスク

空・人物・髪・建物などを自動認識して部分マスクを生成。Adobe の Subject/背景分離に相当。既存の ONNX DML 実行基盤を画像推論へ拡張する。

- 既存資産: `OnnxDmlLocalAgent`（ONNX Runtime / DML）、`FaceDetectionEngine`（DNN 推論）、`OpenCVRotoBrushEngine`（マスク生成）
- 不足: セグメンテーションモデルの読み込み・推論・マスク変換・エフェクト化
- フェーズ:
  1. 画像セグメンテーションモデル（ONNX、例: 人物/背景分離）のロード基盤を確立
  2. 推論結果（確率マップ）を `ImageF32x4_RGBA` マスクへ変換
  3. `ArtifactSemanticMaskEffect` として effect stack に接続
  4. マスクの feather / 反転 / 合成を既存マスク契約に載せる
- 完了条件: 画像から被写体マスクを自動生成し、レイヤーマスク / キーイング / エフェクト制限に使える

### P1 — コンテンツアウェア塗りつぶし UI / 導線

inpaint をエフェクトとしてだけでなく、ビューポート上でブラシ選択 → 塗りつぶしの操作導線にする。既存のマスク・ブラシ描画を流用。

- 既存資産: `MaskPath` / `RotoMask` / ブラシ描画基盤、P0 の inpaint エフェクト
- 不足: ビューポート選択 → inpaint 呼び出しの操作契約、Undo、プレビュー
- フェーズ:
  1. 選択領域を inpaint マスクへ変換する操作ツール
  2. プレビュー（低解像度）+ 確定（フル解像度）の2段階
  3. Undo / 保存復元
- 完了条件: ビューポート上で不要物を囲んで自然に消せる

### P1 — ニューラルスタイル転送

絵画風・イラスト風のスタイル変換。ONNX 推論基盤を流用し、スタイル画像 + コンテンツ画像からスタイル転送モデルを実行する。

- 既存資産: `OnnxDmlLocalAgent`、`ImageF32x4_RGBA` パイプライン
- 不足: スタイル転送モデルの選定・入出力テンソル変換・GPU 経路
- フェーズ:
  1. スタイル転送 ONNX モデルのロードと前処理（リサイズ・正規化）
  2. 推論実行と後処理（denormalize → `ImageF32x4_RGBA`）
  3. `ArtifactStyleTransferEffect` として effect stack に接続
  4. スタイル強度・プリセット（数種のスタイル画像）
- 完了条件: 代表スタイルで画像を変換でき、スタイル強度を調整できる

### P2 — 超解像 / スーパーサンプリング

低解像度画像の高解像度化。既存の FSR/DLSS マイルストーン（`MILESTONE_UPSCALING_FSR_DLSS_2026-08-05.md`）と連携し、実画像 upscale の実装を完成させる。

- 既存資産: FSR/DLSS マイルストーン、ONNX DML 基盤
- 不足: 実画像 upscale の入出力経路、モデル、品質段階
- フェーズ:
  1. 超解像 ONNX モデル（または FSR）の画像入出力契約を確定
  2. `ArtifactSuperResolutionEffect`（または upscale 操作）を接続
  3. プレビュー品質（低）と最終品質（高）の切り替え
- 完了条件: 低解像度画像を高解像度化し、元画像と比較して品質改善を確認できる

### P2 — 構造保持フィルタ（Guided Filter / Bilateral）

エッジを保持したまま平滑化・ディテール転送を行う。既存の `StructureTensor` / `AnisotropicFlowBlur` を発展させる。

- 既存資産: `StructureTensor.cppm`、`AnisotropicFlowBlur.cppm`
- 不足: Guided Filter（誘導画像 + 入力を分離したフィルタ）、Bilateral Filter、エフェクト化
- フェーズ:
  1. `GuidedFilter`（O(1) 近似、box filter ベース）を Core に実装
  2. `ArtifactGuidedFilterEffect`（誘導画像、半径、正則化）を接続
  3. ディテール抽出・転送（AE の Detail-preserving Upscale に相当）
- 完了条件: エッジを保持した平滑化と、別画像のディテール転送ができる

### P3 — トーンカーブ自動補正 / 高度スピル抑制

- **自動レベル補正**: `AutoContrastCV` / `AutoGammaCV` を統合し、ヒストグラム自動ストレッチ + 専用 UI
- **高度スピル抑制**: IBK の despill を拡張し、Refine Edge（エッジ検出 + マット微調整）相当を追加

## 実装制約

- 新規の `QImage` は Qt API／入出力互換境界に限定し、描画・合成・転送の主経路へ増やさない。
- `QPainter`／Qt CompositionMode による新規合成は行わない。AI/セマンティック系は既存の `ImageF32x4_RGBA` と OpenCV/ONNX 基盤へ寄せる。
- QtCSS、`QColorDialog`、新規シグナル／スロットを追加しない。
- 新規ファイルは原則作成せず、既存の `ImageProcessing` / `Effects` モジュールを拡張する。どうしても分離が必要な場合のみ新規エフェクトファイルを追加する。
- ONNX / OpenCV のモデルファイルはリポジトリに埋め込まず、外部パス指定または軽量プリセットモデルを採用する。
- `.ixx` の変更は公開契約に不可欠な場合だけとし、実装側（`.cppm`）で閉じられる変更を優先する。
- `ReactiveEvents` の変更は行わない。

## 完了条件

- [ ] inpaint（コンテンツアウェアフィル）が指定領域を自然に埋められる。
- [ ] セマンティックマスクが被写体を自動分離し、既存マスク/キーイング契約へ接続される。
- [ ] スタイル転送が代表スタイルで機能し、強度を調整できる。
- [ ] 超解像が実画像を高解像度化できる。
- [ ] Guided Filter がエッジを保持した平滑化・ディテール転送を実現する。
- [ ] 自動レベル補正と高度スピル抑制が実用レベルで動作する。
- [ ] すべての新機能で preview と Render Queue の結果が一致する。
- [ ] GPU 非対応時の安全な CPU フォールバックを持つ。

## 関連文書

- [`MILESTONE_UPSCALING_FSR_DLSS_2026-08-05.md`](MILESTONE_UPSCALING_FSR_DLSS_2026-08-05.md)
- [`MILESTONE_ROTOBRUSH_AI_MASK_2026-08-01.md`](MILESTONE_ROTOBRUSH_AI_MASK_2026-08-01.md)
- [`../analysis/REPORT_DCC_GAP_3D_TEXT_2026-08-18.md`](../analysis/REPORT_DCC_GAP_3D_TEXT_2026-08-18.md)

## 次の実装判断

最初の実装対象は P0 の「コンテンツアウェアフィル（inpaint）」と「AIセマンティックセグメンテーションマスク」とする。inpaint は既存 OpenCV 基盤への接続で費用対効果が最大、セマンティックマスクは ONNX 実行基盤の画像推論への拡張として次の土台になる。ビルドや runtime 検証は、実行許可を得てから行う。
