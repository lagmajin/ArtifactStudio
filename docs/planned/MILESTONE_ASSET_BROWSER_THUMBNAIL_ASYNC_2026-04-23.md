# Milestone: Asset Browser Thumbnail Async Warmup (2026-04-23)

**Status:** In Progress
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

