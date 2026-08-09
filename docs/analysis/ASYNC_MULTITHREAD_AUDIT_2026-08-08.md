# Multi-Threading & Async Optimization Opportunities (2026-08-08)

**最終更新:** 2026-08-08
**状態:** 調査完了・提案

## 概要

ArtifactStudio の既存マルチスレッド/非同期基盤の評価と、ブロッキング呼び出し・同期ボトルネックの改善提案。

## 既存の優れた非同期基盤 ✅

| 基盤 | 技術 | 用途 |
|------|------|------|
| `AsyncAssetReadScheduler` | QThreadPool + DirectStorage | 非同期ファイル読み込み。Ticketベースの投機的キャッシュ。DecompressionDAG と連携 |
| `AsyncImageWriterManager` | boost::asio thread_pool | 非同期画像書き込み（OIIO経由） |
| `asio_async_file_writer` | boost::asio io_context | 汎用非同期ファイル書き込み |
| `SharedBackgroundThreadPool` | QThreadPool（最大4スレッド） | `QtConcurrent::run` の共通プール |
| `PreciseTicker` | QThread + high-precision timer | 固定レートのティック（renderTickDriver_） |
| `ArtifactPlaybackEngine` | 専用 QThread + 条件変数 | 再生ワーカースレッド |
| Render Farm (MFR) | `FarmWorkerMain` / マスター/ワーカー | 分散レンダリング。チェックポイント復旧付き |
| `ImageLayer::prefetchFuture_` | `QtConcurrent::run` | 画像の先読みプリフェッチ |
| `PreviewDiskWriter` | `std::thread` + 条件変数 | プレビューキャッシュのディスク永続化 |

---

## 問題点と改善提案

### 🔴 問題1: AssetReadSchedule の `waitForResult` がメインスレッドをブロック

**ファイル**: `Artifact/src/Layer/ArtifactImageLayer.cppm`
**箇所**: `loadImagePairViaAsyncReader()` (Line 697-738)

```cpp
// Line 721-722: キャッシュ参照でブロッキング
scheduler.waitForResult(cacheTicket, cacheRead);  // ★メインスレッドがここで停止

// Line 735-736: 本読み込みでもブロッキング  
scheduler.waitForResult(ticket, readResult);      // ★同じく停止
```

**問題**: 非同期スケジューラを使っているのに、結果を同期的に待っている。DirectStorage が高速でも、メインスレッドが I/O 待ちで停止する。初回読み込みやキャッシュミス時に顕著。

**改善策**: 
- `AsyncAssetReadScheduler` に `enqueueWithCallback(request, callback)` を追加し、完了時にメインスレッドへコールバックを投げる
- ImageLayer の `draw()` が画像読み込み完了を待つ必要がある場合、**プレースホルダー（低解像度サムネイル／単色）を先に描画**し、読み込み完了後に再描画をトリガーする
- `prefetchFuture_` はすでに存在するが、初回描画時に future 完了をブロッキング待機している（Line 2219）。この待機をなくし、prefetch 未完了時は前のフレームの画像を使うフォールバックにする

**効果**: 画像読み込み中のビューポートフリーズ解消
**変更量**: 中

---

### 🔴 問題2: FFmpeg エンコードが `QProcess::waitForFinished()` でメインスレッドをブロック

**ファイル**: `Artifact/src/Render/ArtifactRenderQueueService.cppm`
**箇所**: FFmpeg bridge のプロセス起動と終了待ち (Line 1907, 1978-1983)

```cpp
process_->start(ffmpegPath, args);
if (!process_->waitForStarted()) { ... }          // ★ブロッキング
// ... stdin にフレームデータを書き込み ...
process_->closeWriteChannel();
process_->waitForFinished(30000);                 // ★最大30秒ブロッキング
```

**問題**: レンダーキュー実行中、各ジョブのエンコード完了をメインスレッドで 30 秒ブロッキング待機。UI が完全にフリーズする。

**改善策**:
- `QProcess` の `finished` シグナルを使い、ジョブごとに非同期完了ハンドラを登録
- または RenderQueueService のジョブ実行を専用ワーカースレッドに移動
- 既存の `FarmWorkerMain` のパターン（メッセージパッシング）を RenderQueueService にも適用

**効果**: レンダーキュー実行中の UI 応答性回復
**変更量**: 中

---

### 🟡 問題3: バッチレンダリングが全フレーム逐次処理

**ファイル**: `Artifact/src/Render/ArtifactBatchRenderer.cppm`, `ArtifactOffscreenCompositionRenderer.cppm`
**箇所**: `renderFrame()` + `saveFrame()` が同期的

```cpp
// 各フレーム: render → capture → encode → save。すべて逐次
for each frame:
    renderer->renderFrame(position, composition);  // 同期
    auto image = renderer->captureImage();          // 同期
    encodeAndWrite(image);                          // 同期
```

**問題**: 全フレームが直列。マルチコア CPU を活かせていない。

**改善策**:
- フレーム単位のパイプライン化: フレーム N+1 のレンダリングとフレーム N のエンコードを別スレッドで並列実行
- または複数の Diligent デバイスを作成し、複数フレームを同時レンダリング（GPUメモリが許せば）
- 最低でも `OffscreenCompositionRenderer` のレンダリングを `QtConcurrent::run` で非同期化し、エンコード・ファイル書き込みも非同期化

**効果**: バッチレンダリング時間の大幅短縮（エンコード比率が高ければ特に）
**変更量**: 中〜高（Diligent デバイスのスレッドセーフ）

---

### 🟡 問題4: 画像プリフェッチが完了しないとレイヤー描画が止まる

**ファイル**: `Artifact/src/Layer/ArtifactImageLayer.cppm`
**箇所**: `draw()` 内の画像参照 (Line 2189-2225)

```cpp
if (isMainThread) {
    // prefetchが走っていたらメインスレッドで同期的に完了を待つ
    if (impl_->prefetchFuture_.isRunning())
        return;  // まだならスキップするが、次回もまた待つ
} else {
    // バックグラウンドスレッドならブロッキング待機
    impl_->prefetchFuture_.waitForFinished();  // ★
}
```

**問題**: メインスレッドでは prefetch 未完了時に描画せずに return するのは良いが、次回も同じ結果になり、画像が表示されるまで数フレーム空白が続く。バックグラウンドスレッドではブロッキング。

**改善策**:
- `return` ではなく、**前回成功した画像**（`cachedDisplayImage_`）をフォールバック描画
- または低解像度プロキシ画像を先に描画し、高解像度が読み込まれたら差し替え
- `prefetchFuture_` の完了を `QFutureWatcher::finished` シグナルで受けて `markRenderDirty()` を発行

**効果**: 画像切り替え時の瞬間的な空白をなくす
**変更量**: 低

---

### 🟡 問題5: AudioAnalyzer の FFT がメインスレッドで毎フレーム実行

**ファイル**: `ArtifactCore/src/Audio/AudioAnalyzer.cppm`
**箇所**: `analyze()` (Line 98-140)

```cpp
AnalysisResult AudioAnalyzer::analyze(const AudioSegment& segment) {
    // RMS + Peak 計算 → FFT → スペクトル強度 → バンド強度
    // すべてメインスレッドで同期的に実行
}
```

**問題**: 8192点 FFT + ハミング窓 + マグニチュード計算。60fps で毎フレーム。実は FFT 部分は軽い（Radix-2 は効率的）が、`AudioSpectrum::computeFFT` 側の簡易 DFT の方が O(n²) で重い。

**改善策**:
- AudioAnalyzer の結果をキャッシュ（同一フレームで同じオーディオデータなら再計算しない）
- `AudioSpectrum::computeFFT` を O(n log n) の FFT に置き換え（前項 GPU提案の代わりに CPU 向け改善）
- または FFT 結果を `QtConcurrent::run` で非同期計算し、結果だけを `std::atomic` 共有バッファに書き込む

**効果**: オーディオ波形表示の応答性改善
**変更量**: 低〜中

---

### 🟢 問題6: コンポジションサムネイルが同期的に生成

**ファイル**: `ArtifactPlaybackEngine::renderFrame()` (Line 501)
**箇所**: `generateCompositionThumbnail(composition_, sz)` 

```cpp
QImage renderFrame(const FramePosition& position) {
    if (composition_) {
        QImage preview = generateCompositionThumbnail(composition_, sz);  // 同期
```

**問題**: タイムラインスクラブ時にサムネイル生成がメインスレッドをブロック。大量のレイヤーがあると顕著。

**改善策**:
- サムネイル生成を `SharedBackgroundThreadPool` に投げ、完了シグナルで再描画
- またはプレビューキャッシュからの読み出し（すでに存在）に完全に依存し、サムネイルはキャッシュ構築時のみ生成

**変更量**: 低

---

## 実装優先順位

| Priority | 問題 | コスト | 効果 | リスク |
|----------|------|--------|------|-------|
| 🔴 1 | AssetRead blocking (`waitForResult`) | 中 | UIフリーズ解消 | 中（async callback化） |
| 🔴 2 | FFmpeg `waitForFinished` | 中 | レンダーキューUI応答 | 中（プロセス管理変更） |
| 🟡 3 | Batch render parallel | 中〜高 | エンコード時間半減 | 高（Diligentスレッドセーフ） |
| 🟡 4 | Image prefetch fallback | 低 | 空白フレーム解消 | 低 |
| 🟡 5 | Audio FFT async | 低〜中 | 微改善 | 低 |
| 🟢 6 | Thumbnail async | 低 | タイムライン応答 | 低 |

## 変更対象ファイル一覧

| ファイル | 優先度 |
|----------|--------|
| `Artifact/src/Layer/ArtifactImageLayer.cppm` | 1, 4 |
| `Artifact/src/IO/AsyncAssetReadScheduler.cppm` | 1 |
| `Artifact/src/Render/ArtifactRenderQueueService.cppm` | 2 |
| `Artifact/src/Render/ArtifactBatchRenderer.cppm` | 3 |
| `Artifact/src/Render/ArtifactOffscreenCompositionRenderer.cppm` | 3 |
| `ArtifactCore/src/Audio/AudioAnalyzer.cppm` | 5 |
| `ArtifactCore/src/Audio/AudioSpectrum.cppm` | 5 |
| `Artifact/src/Playback/ArtifactPlaybackEngine.cppm` | 6 |
