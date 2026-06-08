# Export Matrix & Batch Rendering (統合マイルストーン)

> 2026-06-08 作成 (統合元: `EXPORT_MATRIX_2026-06-07`, `BATCH_RENDERING_2026-03-28`)

## 目的

書き出しを「選ぶ」「並べる」「確認する」「流す」の一連の流れとして統合する。

- 複数コンポジションを一括でレンダーキューに追加できる
- 出力プリセットが用途ベースで直感的に選べる
- テンプレートとして保存・再利用できる
- Alpha の状態が明確で、事前に問題を検出できる
- 書き出し前に最終結果をプレビュー確認できる

## 背景と現状

| 機能 | 状態 |
|------|------|
| レンダーキューへの個別ジョブ追加 | ✅ 完成 |
| 一括ジョブ追加 | ❌ 未実装 |
| プロジェクト全コンポジション追加 | ❌ 未実装 |
| 用途ベースプリセット名 | ❌ 未実装 (現状は技術名のみ) |
| Alpha 状態の明示 | ❌ 未実装 |
| Alpha Edge Check / Preflight | ❌ 未実装 |
| バッチテンプレート保存/読込 | ❌ 未実装 |
| レンダーキューの永続化 | ✅ 完成 |

## 設計方針

- **一括追加**: `ArtifactBatchRenderer` サービス (新規) — 既存の `ArtifactRenderQueueService` をラップして一括操作を提供
- **用途プリセット**: 既存 `ArtifactRenderFormatPresetManager` の表示名を用途ベースに拡張。内部の技術名は維持
- **バッチテンプレート**: JSON で保存する `BatchTemplate` 構造体。用途プリセット + 出力先 + ファイル名パターンを保持
- **Alpha Contract**: `opaque / straight / premultiplied / alpha-only` の4状態を `ArtifactRenderJob` とエンコーダー設定で共有
- **Preflight**: 既存の `preflightRenderQueueAt()` を拡張して alpha edge check を追加

## Phases

### Phase 1: 一括ジョブ追加

BatchRendering M1 をそのまま採用。実装は軽い。

- `ArtifactBatchRenderer::addAllCompositions()` — 全コンポジションをキューに追加
- `ArtifactBatchRenderer::addCompositions(ids)` — 選択コンポのみ追加
- ファイル名パターン: `%compName%`, `%date%`, `%time%`, `%frame%`
- UI: レンダーキューメニューに "Add All Compositions" / "Add Selected"

### Phase 2: 用途ベースプリセット

ExportMatrix Phase 1 を採用。既存プリセット名を用途名にマッピングし直す。

- 用途名へのマッピング (内部IDは変えない):
  - `背景透過動画(WebM/VP9)` ← `webm_vp9`
  - `編集ソフト用(ProRes + アルファ)` ← `prores_4444_mov`
  - `編集ソフト用(ProRes 422)` ← `prores_422_mov`
  - `静止画連番(PNG + 透明)` ← `png_sequence`
  - `高画質配布(H.264 MP4)` ← `h264_mp4_high`
  - `標準配布(H.264 MP4)` ← `h264_mp4_standard`
  - `Web配布(WebM VP9)` ← `webm_vp9` (透過なし版)
  - `高効率(H.265 MP4)` ← `h265_mp4`
  - `GIFアニメーション` ← `gif_animation`
  - `連番(OpenEXR)` ← `exr_sequence`

- 用途カテゴリ: `透過あり` / `編集向け` / `配布向け` / `Web向け` / `連番`
- UI 上の表示順を用途カテゴリでグループ化

### Phase 3: バッチテンプレート

BatchRendering M2 と ExportMatrix の用途プリセットを組み合わせる。

- `BatchTemplate` 構造体:
  - `name` — テンプレート名
  - `outputDirectory` — 出力先ディレクトリ
  - `fileNamePattern` — ファイル名パターン (`%compName%_%date%`)
  - `presetId` — 用途プリセットID
  - `resolution` / `fps` — 上書き設定 (空ならコンポジション既定)
  - `frameRange` — "comp" (コンポ既定) または指定範囲

- 標準テンプレートプリセット:
  - `YouTube 1080p` → H.264, 1920x1080, 30fps, CRF18
  - `YouTube 4K` → H.264, 3840x2160, 30fps, CRF18
  - `ProRes 422 HQ` → ProRes, コンポ解像度
  - `透過PNG連番` → PNG, コンポ解像度
  - `Web配布(VP9)` → VP9, 1920x1080

- JSON 保存/読込: `{user_config}/batch_templates/` ディレクトリ

### Phase 4: Alpha State Contract

ExportMatrix Phase 2 を採用。

- `AlphaState` 列挙体: `opaque / straight / premultiplied / alphaOnly`
- `ArtifactRenderJob` に `alphaState` フィールド追加
- `FFmpegEncoderSettings` に `alphaState` フィールド追加
- エンコーダー出力時に alpha state に応じた pix_fmt を自動選択
- 入出力で alpha state が異なる場合は変換方針を明示

### Phase 5: Preflight & Preview

ExportMatrix Phase 3+4 を採用。

- Alpha Edge Check: 境界ピクセルの検査 (白/黒フリンジ検出)
- Straight/Premultiplied 比較プレビュー
- 書き出し前サマリーに alpha 状態を表示
- 推奨設定ガイダンス

### Phase 6: UI Integration

ExportMatrix + BatchRendering UI を統合。

- レンダーキューメニュー: "Add All", "Add Selected", "Apply Template"
- 出力設定ダイアログ: 用途カテゴリタブ + 詳細タブ
- バッチテンプレート管理ダイアログ
- プリフライト結果の表示改善

## 実装順 (推奨)

| 順 | Phase | 内容 | 規模 |
|----|-------|------|------|
| 1 | Phase 1 | 一括ジョブ追加 | 小 (新規サービス ~200行) |
| 2 | Phase 2 | 用途ベースプリセット | 小 (表示名マッピング) |
| 3 | Phase 3 | バッチテンプレート | 中 (~400行) |
| 4 | Phase 4 | Alpha Contract | 中 (~300行) |
| 5 | Phase 5 | Preflight & Preview | 大 (~600行) |
| 6 | Phase 6 | UI Integration | 中 (~400行) |

## 関連

- `docs/planned/MILESTONE_RENDER_QUEUE_ENCODING_2026-04-01.md`
- `docs/planned/MILESTONE_UNIFIED_AUDIO_VIDEO_RENDER_OUTPUT_2026-03-28.md`
- `docs/planned/MILESTONE_AE_PARITY_BACKLOG_2026-04-29.md`
- `Artifact/src/Render/ArtifactRenderQueueService.cppm`
- `Artifact/src/Render/ArtifactRenderQueuePresets.cppm`
- `ArtifactCore/include/Video/FFMpegEncoder.ixx`
