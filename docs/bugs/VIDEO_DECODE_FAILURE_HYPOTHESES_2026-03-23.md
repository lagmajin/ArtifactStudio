# FFmpeg フレームデコード失敗の仮説 (2026-03-23)

## パイプライン概要

```
VideoLayer::goToFrame()
  → decodeCurrentFrame()
    → playbackController_->getVideoFrameAtFrameDirect(targetFrame)
      → seek(targetMs)
      → getNextVideoFrame() loop (max 300)
        → mediaReader_->getNextPacket(Video)  // TBB concurrent_queue
        → videoDecoder_->decodeFrame(pkt)
          → avcodec_send_packet()
          → avcodec_receive_frame()
          → sws_scale() → QImage (Format_RGB888)
```

## 仮説（優先度順）

### ★ 仮説1: デコーダー初期化失敗が無視される

**場所:** `MediaPlaybackController.cppm:417-419`

```cpp
if (!impl_->videoDecoder_->initialize(params)) {
    qWarning() << "Failed to initialize video decoder";
}
// ← ここで return false しない！openMedia() は true を返す
```

`openMedia()` がデコーダー初期化失敗時に `false` を返さず、成功扱いになる。
その後の全フレームデコードが失敗し続ける。

原因の候補:
- `avcodec_find_decoder()` が null（対応コーデックが見つからない）
- `avcodec_open2()` が失敗（HW デコーダーのみ、SW 無効等）
- `sws_getContext()` が null（ピクセルフォーマット変換不可）

### ★ 仮説2: getNextVideoFrame() の無限ループ

**場所:** `MediaPlaybackController.cppm:596-625`

```cpp
while (true) {
    QImage img = impl_->videoDecoder_->decodeFrame(pkt);
    av_packet_free(&pkt);
    if (!img.isNull()) {
        return img;
    }
    // ← 無条件でループ継続、脱出条件なし
}
```

デコーダーが継続的にフレームを返せない場合（B-frames、corrupted stream）、外側の `while(true)` が無限ループ。UI フリーズの原因。

### 仮説3: リーダースレッドのタイムアウト

**場所:** `MediaPlaybackController.cppm:607-614`

```cpp
for (int attempt = 0; attempt < 100 && !pkt; ++attempt) {
    pkt = impl_->mediaReader_->getNextPacket(StreamType::Video);
    if (!pkt) std::this_thread::sleep_for(std::chrono::milliseconds(2));
}
if (!pkt) {
    impl_->notifyEndOfMedia();
    return QImage();
}
```

100回 × 2ms = 200ms でタイムアウト。
seek 後にリーダーが再起動するまでに 200ms 以上かかるファイル
（ネットワークドライブ、大容量ファイル、断片化したファイル）で発生。

### 仮説4: MediaSource::seek() の失敗が無視される

**場所:** `MediaPlaybackController.cppm:496`

```cpp
impl_->mediaSource_->seek(timestampMs);  // ← 戻り値チェックなし！
impl_->rebuildReader();
impl_->videoDecoder_->flush();
```

`av_seek_frame()` が失敗すると、リーダーはシーク前の位置から読み続ける。
デコーダーに渡される PTS が `targetPts` と一致しないため、
`getVideoFrameAtFrame()` の 300 ループが全て使い切られ null を返す。

### 仮説5: loadFromPath() で isLoaded_ = true が早すぎる

**場所:** `ArtifactVideoLayer.cppm:267 vs 275`

```cpp
impl_->isLoaded_ = true;           // ← フレーム0デコードの前
const QImage firstFrame = impl_->playbackController_->getVideoFrameAtFrameDirect(0);
if (!firstFrame.isNull()) { ... }  // ← 失敗しても isLoaded_ = true のまま
```

フレーム 0 のデコードに失敗しても `isLoaded_ = true` のまま。
以降の `decodeCurrentFrame()` が毎回デコードを試みて失敗する。

### 仮説6: スレッド競合

**場所:** `MediaReader.cppm` / `MediaSource.cppm`

- `isRunning_` が mutex なしで読み書きされる
- `MediaSource::close()` が `readLoop()` 実行中に `AVFormatContext*` を無効化する可能性
- `seek()` が `stop()` → `seek()` → `rebuildReader()` を mutex なしで実行

メインスレッドとバックグラウンドスレッドが同じ `AVFormatContext*` に
同時にアクセスするとクラッシュまたは null フレーム。

### 仮説7: B-frames 対応不足

**場所:** `MediaImageFrameDecoder.cppm:51-79`

`decodeFrame()` で `avcodec_receive_frame()` が `AVERROR(EAGAIN)` を返す
（B-frames のためデコーダーがまだフレームを出力できない）と、
単に null を返す。

`getNextVideoFrame()` の外側ループは次のパケットを読むが、
B-frames の数だけパケットが必要。300 ループ以内に到達しない長い GOP で発生。

## 確認方法

コンソールログで以下が出力されていれば仮説が確定:
- `Failed to initialize video decoder` → 仮説1
- `getNextVideoFrame: TIMEOUT after 100 attempts` → 仮説3
- `DECODE FAILED for frame` → 仮説2, 4, 5, 7
- UI フリーズ（ログ出力停止） → 仮説2

## 関連ファイル

| ファイル | 行 | 内容 |
|----------|----|------|
| `ArtifactCore/src/Media/MediaPlaybackController.cppm` | 384-430 | openMedia |
| `ArtifactCore/src/Media/MediaPlaybackController.cppm` | 486-504 | seek |
| `ArtifactCore/src/Media/MediaPlaybackController.cppm` | 596-625 | getNextVideoFrame |
| `ArtifactCore/src/Media/MediaPlaybackController.cppm` | 642-694 | getVideoFrameAtFrame |
| `ArtifactCore/src/Media/MediaPlaybackController.cppm` | 112-210 | decodeVideoFrameDirectAtFrame |
| `ArtifactCore/src/Media/MediaSource.cppm` | 66-92 | open |
| `ArtifactCore/src/Media/MediaSource.cppm` | 94-116 | seek |
| `ArtifactCore/src/Media/MediaReader.cppm` | 83-88 | start |
| `ArtifactCore/src/Media/MediaReader.cppm` | 98-103 | stop |
| `ArtifactCore/src/Media/MediaReader.cppm` | 115-149 | readLoop |
| `ArtifactCore/src/Media/MediaImageFrameDecoder.cppm` | 28-49 | initialize |
| `ArtifactCore/src/Media/MediaImageFrameDecoder.cppm` | 51-79 | decodeFrame |
| `Artifact/src/Layer/ArtifactVideoLayer.cppm` | 232-294 | loadFromPath |
| `Artifact/src/Layer/ArtifactVideoLayer.cppm` | 372-415 | decodeCurrentFrame |
