# 提案メモ: ファイル書き出し（レンダーキュー）効率化 — 2026-07-28

**作成日:** 2026-07-28
**ステータス:** 提案（未実装・未承認）
**関連:** `docs/analysis/REPORT_TBB_WORK_STEALING_CANDIDATES_2026-07-28.md` §2.2-(5)
**対象:** `Artifact/src/Render/ArtifactRenderQueueService.cppm`、`ArtifactCore/src/Image/FFmpegEncoder.cppm`

---

## 現状の構造（調査済み）

- **レンダー側は並列済み**: MFR（Multi-Frame Rendering）で worker 並列（`useMfr` 判定 L4640-4642）。ただし
  - `maxInFlightFrames_ = 4` 固定（L2677）
  - GPU バックエンド時・コンポーネントシミュレーション使用時は 1 worker
  - outputBuffer 上限: 8 フレーム / 512MB（L2680-2681）
- **consumer（書き出し側）は完全シリアル**（L4941-5073）: フレーム順に「QImage RGBA 変換 → プレビュー縮小 → encoder.addFrame / ImageExporter 書き込み」

## 提案（効果順）

### 1. FFmpeg エンコーダの thread_count 設定【最有力・低リスク】

- `FFmpegEncoder.cppm` の codec context 初期化（L261-288 付近）に `thread_count` / `thread_type` の設定が**一切ない**。libavcodec の既定は 1 スレッドのため、x264/x265 エンコードがシングルスレッドで走っている可能性が高い
- 対応: `codecCtx_->thread_count = 0`（自動）+ `thread_type = FF_THREAD_FRAME | FF_THREAD_SLICE`
- 併せて `buildNativeVideoSettings()`（RenderQueueService L1487-1500）が h264/h265 で preset **"slow"** をハードコードしている点を見直す（1 スレッド × slow は最悪の組合せ。preset のジョブ設定露出 or "medium" 化）
- **未検証:** 実行時に thread_count=1 で動いているかはログ/ベンチで要確認

### 2. プレビュー縮小の間引き【最小工数】

- consumer が毎フレーム 320×180 の `SmoothTransformation` 縮小を実行（L4966-4968）
- 対応: 時間ベース間引き（100〜200ms に 1 回）。書き出し結果に影響なし

### 3. consumer の parallel_pipeline 化【効果大・設計必要】

- `tbb::parallel_pipeline` で 3 段構成:
  1. `serial_in_order`: outputBuffer からフレーム取得
  2. `parallel`: RGBA 変換・プレビュー縮小・EXR zip 圧縮（1 フレームでも重い）
  3. `serial_in_order`: `encoder.addFrame`（FFmpeg はフレーム順序必須）/ 進捗通知
- 連番出力（EXR/PNG）はフレーム間順序制約がないため、書き込み自体を parallel 段に置ける
- 注意: 既存の worker/outputBuffer/cv 機構と二重化しない。置換か「consumer 内の変換のみ task_group 先行実行」かの設計判断が必要

### 4. sws_scale の変換スレッド化

- swscale コンテキストもスレッド指定なし（1 スレッド）。4K float→YUV 変換はフレームあたり数十 ms 級
- 対応: sws context の `threads` オプション設定、または RGBA8 化を `Parallel::For` で事前実施

### 5. maxInFlightFrames_ の可変化

- CPU レンダー時の in-flight 上限 4 固定を HW 並列数ベースに引き上げ。メモリ制御（512MB/8f）は既存のバッファ制限が機能するため安全

## 実施順の推奨

**1（thread_count）→ 2（プレビュー間引き）→ 3（pipeline 化）→ 4 → 5**

## 注意点

- 1・4 は ArtifactCore 側の変更（子リポ編集はユーザー承認必須）
- GPU バックエンド 1 worker 制約は Diligent デバイスコンテキストの制約と思われるため触らない（AGENTS の DX12 慎重方針）
- ビルド・ベンチ実行はユーザー指示待ち

## 更新履歴

- 2026-07-28: 初版（チャット提案の記録化）
