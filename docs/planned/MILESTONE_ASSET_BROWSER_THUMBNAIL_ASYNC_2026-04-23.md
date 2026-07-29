# Milestone: Asset Browser Thumbnail Async Warmup (2026-04-23)

**Status:** Partial（Phase 1〜2 と世代付き cancel／shutdown 経路を実装済み。Phase 3 の可視範囲優先 scheduler と runtime 性能検証 pending）
**Goal:** アセットブラウザのサムネイル生成を UI スレッドから外し、動画ファイルの多いディレクトリでも一覧表示が固まらないようにする。

---

## 現状の課題

`ArtifactAssetBrowser::Impl::applyFilters()` は一覧の全件に対して `generateThumbnail()` を呼んでおり、従来は動画ファイルで `cv::VideoCapture` を同期起動していた。

これにより:

1.  ディレクトリを開いた瞬間に UI スレッドが decode 処理で詰まる
2.  動画が複数あると、サムネイル生成のたびに FFmpeg 系の重い初期化が発生しやすい
3.  画像や音声のサムネイルも、全件同期生成だと一覧の初期表示を押し下げる

---

## 改善方針

### Phase 1: 非同期サムネイル生成
- 画像 / 動画サムネイルをバックグラウンド生成へ移す
- 生成前はデフォルトアイコンを返す
- 完了時に `AssetMenuModel` の `DecorationRole` を更新する

### Phase 2: FFmpeg の軽量化
- 動画サムネイル抽出は OpenCV の `VideoCapture` ではなく、既存の `FFmpegThumbnailExtractor` を使う
- extractor 側は `thread_count = 1` で単一スレッド寄りに維持する

### Phase 3: 表示中アイテム優先
- 可視範囲や近傍アイテムを優先して warmup する
- ディレクトリ全件の先読みは後回しにする

---

## 実装メモ

- `AssetMenuModel::updateItemIconByPath()` を使って、非同期完了時に個別更新する
- `thumbnailGeneration_` を使ってディレクトリ切り替え後の古い結果を無効化する
- 既存の audio waveform 非同期処理も同じ世代管理に寄せる

---

## 期待効果

- フォルダを開いたときの UI フリーズの軽減
- 動画サムネイルの生成負荷をメインスレッドから分離
- 大きいアセットフォルダでも初期表示を先に返せるようになる


## 2026-07-25 実装監査

- `ArtifactAssetBrowser` に QtConcurrent／QFutureWatcher による image／video thumbnail の非同期生成、世代番号による stale result 無効化、mutex 保護のメモリ cache、個別 model 更新経路を確認できる。
- FFmpegThumbnailExtractor の単一スレッド寄り抽出、ディスク thumbnail cache、audio waveform の非同期経路も実装されている。
- キャッシュ再生成時は世代更新に加えて進行中の image／video／audio watcher を disconnect／cancel し、古い job を pending map から破棄する。
- 一方、可視範囲・近傍アイテムを厳密に優先する scheduler、明示的な cancel／shutdown の runtime 検証、全件初期表示での UI 非ブロッキング効果は未確認である。
- よって主要な非同期 warmup は実装済みだが、Phase 3 と実機性能検証を残す In Progress 判定を維持する。
