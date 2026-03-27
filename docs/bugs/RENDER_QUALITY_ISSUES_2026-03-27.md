# レンダーキュー画質低下の原因 (2026-03-27)

## 症状

ロスの少ないコーデック（ProRes等）でも出力画質が荒い。

---

## 仮説

### ★★★ 1. buildNativeVideoSettings() が品質パラメータを設定しない

**場所:** `ArtifactRenderQueueService.cppm:279-306`

```cpp
static FFmpegEncoderSettings buildNativeVideoSettings(const ArtifactRenderJob& job)
{
    settings.width = job.resolutionWidth;
    settings.height = job.resolutionHeight;
    settings.fps = job.frameRate;
    settings.bitrateKbps = job.bitrate;
    settings.videoCodec = ...;
    settings.container = ...;
    // ❌ preset    = "medium"   (デフォルト)
    // ❌ crf       = 23         (デフォルト)
    // ❌ gopSize   = 10         (デフォルト)
    // ❌ profile   = "high"     (デフォルト)
    // ❌ zerolatency = true     (デフォルト!) ← 最悪
    return settings;
}
```

**影響:**
- `zerolatency = true` → H.264 の `tune=zerolatency` が有効。品質を大幅に犠牲にする。
- `crf = 23` → デフォルト品質。高品質には `crf = 18` 以下が必要。
- `preset = "medium"` → エンコード速度と品質のトレードオフ。高品質には `"slow"` 以上。
- ユーザーが指定した品質設定がエンコーダーに一切渡されない。

### ★★★ 2. PipeFFmpegExeBackend が job の設定を全て無視

**場所:** `ArtifactRenderQueueService.cppm:340-349`

```cpp
QStringList args;
args << "-y"
     << "-f" << "rawvideo"
     << "-pixel_format" << "rgba"
     << "-video_size" << QString("%1x%2").arg(width).arg(height)
     << "-framerate" << QString::number(fps)
     << "-i" << "-"
     << "-c:v" << "libx264"      // ❌ job.codec を無視、libx264 固定
     << "-pix_fmt" << "yuv420p"  // ❌ 常に yuv420p
     << job.outputPath;
     // ❌ -b:v (bitrate) 設定なし
     // ❌ -crf 設定なし
     // ❌ -preset 設定なし
     // ❌ -profile 設定なし
```

**影響:**
- job.codec が "ProRes" でも `libx264` でエンコードされる
- job.bitrate が 50000kbps でも libx264 のデフォルト品質でエンコード
- Native backend が失敗した場合のフォールバックだが、画質が大幅に劣化する

### ★★ 3. MJPEG のピクセルフォーマット不正

**場所:** `FFmpegEncoder.cppm:144-145`

```cpp
} else if (codecId == AV_CODEC_ID_MJPEG || codecId == AV_CODEC_ID_PNG) {
    codecCtx_->pix_fmt = AV_PIX_FMT_YUV420P;  // ❌ MJPEG は YUVJ420P が必要
}
```

MJPEG は full-range YUV (`AV_PIX_FMT_YUVJ420P`) を期待。
limited-range (`AV_PIX_FMT_YUV420P`) が設定されるとコントラスト低下。

### ★ 4. アップロード時の QImage フォーマット変換

**場所:** `ArtifactRenderQueueService.cppm:308-316`

```cpp
static ImageF32x4_RGBA qImageToImageF32x4RGBA(const QImage& source) {
    const QImage rgba = source.convertToFormat(QImage::Format_RGBA8888);
    cv::Mat mat(rgba.height(), rgba.width(), CV_8UC4, ...);
    ImageF32x4_RGBA out;
    out.setFromCVMat(mat);
    return out;
}
```

元の QImage が `Format_ARGB32_Premultiplied` (8bit/channel) なので、
10bit/12bit コーデックでも入力が 8bit のまま。出力の量子化劣化は避けられないが、
8bit コーデック（H.264等）では問題にならない。

---

## レンダリングパイプライン品質確認済み

| パス | 解像度 | ビット深度 | 品質 |
|------|--------|----------|------|
| `renderSingleFrameComposition()` | compSize × outW | 8bit (ARGB32) | ✅ コンポジション解像度でレンダリング → 出力解像度にスケール |
| `renderLayerSurface()` | layerSize | 8bit (ARGB32) | ✅ レイヤーの元解像度でレンダリング |
| `qImageToImageF32x4RGBA()` | 元のまま | 8→f32 変換 | ✅ データ損失なし |
| エンコーダー設定 | outW × outH | コーデック依存 | ❌ 品質パラメータがデフォルト |

→ **レンダリング自体は最高品質。問題はエンコーダー設定。**

---

## 対処状況

2026-03-27 時点で、以下をコードへ反映済み。

- `ArtifactRenderQueueService.cppm`
  - `buildNativeVideoSettings()` に codec ごとの高品質デフォルトを追加
  - `native` backend の `preset/crf/gopSize/profile/zerolatency` を job 依存に調整
  - `PipeFFmpegExeBackend` で codec を固定せず、job の codec と pix_fmt を反映
  - ProRes 422 / 4444 を `codecProfile` で分離し、4444 時は alpha 対応の pixel format を選択
- `FFmpegEncoder.cppm`
  - MJPEG を `AV_PIX_FMT_YUVJ420P` に変更
  - PNG を `AV_PIX_FMT_RGBA` に変更し、alpha を保持
  - ProRes 4444 のときは `AV_PIX_FMT_YUVA444P10LE` を選択

残タスクがあれば、pipe backend の出力結果を別 clone で確認してから微調整する。

---

## 対策案

### 対策1: buildNativeVideoSettings() に品質パラメータを反映 (最重要)

```cpp
static FFmpegEncoderSettings buildNativeVideoSettings(const ArtifactRenderJob& job) {
    FFmpegEncoderSettings settings;
    settings.width = std::max(1, job.resolutionWidth);
    settings.height = std::max(1, job.resolutionHeight);
    settings.fps = job.frameRate > 0.0 ? job.frameRate : 30.0;
    settings.bitrateKbps = std::max(1, job.bitrate);

    // コーデック
    const QString codec = job.codec.trimmed().toLower();
    settings.videoCodec = mapCodec(codec);

    // 品質設定
    if (codec.contains("prores")) {
        settings.preset = "slow";      // ProRes 用
        settings.profile = "hq";       // HQ プロファイル
        settings.zerolatency = false;  // ProRes は zerolatency 不要
    } else if (codec.contains("h265") || codec.contains("hevc")) {
        settings.crf = 18;             // 高品質
        settings.preset = "slow";
        settings.zerolatency = false;
    } else {
        settings.crf = 18;             // H.264 高品質
        settings.preset = "slow";
        settings.zerolatency = false;  // ← 重要: false に変更
    }

    settings.container = deriveContainerFromJob(job);
    return settings;
}
```

### 対策2: PipeFFmpegExeBackend に job の設定を反映

```cpp
// 現在の固定設定を以下に置き換え:
args << "-c:v" << mapCodecName(job.codec)  // job.codec に応じたコーデック
     << "-b:v" << QString("%1k").arg(job.bitrate)  // bitrate
     << "-preset" << "slow"                      // preset
     << "-crf" << "18"                            // CRF
     << "-pix_fmt" << mapPixelFormat(job.codec);  // コーデックに応じたピクセルフォーマット
```

### 対策3: MJPEG のピクセルフォーマット修正

```cpp
} else if (codecId == AV_CODEC_ID_MJPEG) {
    codecCtx_->pix_fmt = AV_PIX_FMT_YUVJ420P;  // ← full-range YUV
```

---

## 推奨対応順

| 順序 | 対策 | 効果 | 見積 |
|---|---|---|---|
| 1 | buildNativeVideoSettings の品質パラメータ反映 | 最大 | 1h |
| 2 | zerolatency を false に変更 | 即効性大 | 10min |
| 3 | PipeFFmpegExeBackend の設定反映 | 中 | 2h |
| 4 | MJPEG ピクセルフォーマット修正 | 小 | 15min |

## 関連ファイル

| ファイル | 行 | 内容 |
|---|---|---|
| `Artifact/src/Render/ArtifactRenderQueueService.cppm` | 279-306 | buildNativeVideoSettings — 品質パラメータ未設定 |
| `Artifact/src/Render/ArtifactRenderQueueService.cppm` | 340-349 | PipeFFmpegExeBackend::open — 設定無視 |
| `Artifact/src/Render/ArtifactRenderQueueService.cppm` | 308-316 | qImageToImageF32x4RGBA |
| `Artifact/src/Render/ArtifactRenderQueueService.cppm` | 1170-1218 | renderSingleFrameComposition |
| `ArtifactCore/src/Image/FFmpegEncoder.cppm` | 128-180 | FFmpegEncoder::open — コーデック設定 |
| `ArtifactCore/include/Video/FFMpegEncoder.ixx` | 17-32 | FFmpegEncoderSettings 構造体 |
