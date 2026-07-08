# BUG: アセットブラウザでサムネイルが正常に取得できない（透明化）

> 作成: 2026-07-08 / 状態: 診断済み（未修正）
> 対象: `Artifact/src/Widgets/Asset/ArtifactAssetBrowser.cppm`
> 症状: アセットブラウザで画像のサムネイルが空白（透明）で表示される / またはファイル型アイコンのままになる。

---

## 症状

- アセットブラウザに画像（JPEG/PNG/TIFF 等）を表示しても、サムネイルが透明・空白になる、あるいはファイル型アイコンのままになる。
- 動画・音声は別経路（FFmpeg / 波形）のため影響は軽微。

---

## 根本原因（高確率）

### 1. OIIO デコードパスでアルファ=0 になる（主要因）

`ArtifactAssetBrowser.cppm:565-568`（`loadImageThumbnailViaOIIO` 内）:

```cpp
} else if (channelCount == 2) {
  rgba = OIIO::ImageBufAlgo::channels(oriented, 4, channelOrder);   // channelValues 未指定
} else if (channelCount == 3) {
  rgba = OIIO::ImageBufAlgo::channels(oriented, 4, channelOrder);   // channelValues 未指定
}
```

- `OIIO::ImageBufAlgo::channels(src, 4, channelorder)` の第 4 引数 `channelvalues` を省略すると、存在しない出力チャンネル（この場合アルファ）は **0** で埋められる（OIIO の仕様）。
- 結果、`QImage::Format_RGBA8888` のサムネイルはすべてのピクセル **alpha=0（完全透過）** になる。
- 返却画像は `isNull()` が `false` のため、`ArtifactAssetBrowser.cppm:1596` の `if (!image.isNull()) return image;` で即返却され、**WIC / QImageReader / Windows Shell のフォールバックには回らない**。
- → 最も一般的な 2ch/3ch（RGB）画像のサムネイルが透明になる。

対照:
- 1ch（grayscale）: `channelValues={0,0,0,1}` を渡しているため alpha=1 → 正常。
- 4ch 以上: 元のアルファを維持 → 正常。
- **壊れているのは最も一般的な 2ch/3ch ケースのみ**。

### 2. 失敗キャッシュの永続化（補助要因）

`ArtifactAssetBrowser.cppm:1540`:

```cpp
failedPreviewPaths_.insert(filePath);
```

- 初回デコードが一時的に失敗（ファイルロック、コーデック初期化失敗等）すると、そのパスは `clearThumbnailCache()` するまで `failedPreviewPaths_` に残り、**永遠に placeholder（ファイル型アイコン）のまま**になり再試行されない。

---

## 呼び出し経路（確認済み）

1. `ArtifactAssetBrowser::Impl::generateThumbnail(filePath)`（`cppm:1359`）: キャッシュミス時に `startAsyncPreviewThumbnailGeneration(filePath)` を起動し placeholder を返す。
2. `startAsyncPreviewThumbnailGeneration`（`cppm:1473`）: `QtConcurrent::run` で `loadImageThumbnailViaOIIO` → WIC → `QImageReader` → WIC → Shell の順でデコード。
3. `QFutureWatcher::finished`（`cppm:1500`）: 成功時に `assetModel_->updateItemIconByPath(filePath, icon)` でモデル更新、`dataChanged(Qt::DecorationRole)` を emit。
4. `AssetMenuModel::data()`（`AssetMenuModel.cppm:108`）: `item.icon` を `Qt::DecorationRole` で返す。

※ OIIO が透過画像を返すとステップ 3 は「成功」扱いになるため、`failedPreviewPaths_` には入らず、透過アイコンがそのままモデルに格納される。

---

## 修正案

### 修正 A（推奨）: OIIO の 2ch/3ch 分岐に channelValues を渡す

`ArtifactAssetBrowser.cppm:563-568` を以下のように修正:

```cpp
const std::vector<float> alphaOne = {0.0f, 0.0f, 0.0f, 1.0f};
if (channelCount == 1) {
  rgba = OIIO::ImageBufAlgo::channels(oriented, 4, channelOrder, alphaOne);
} else if (channelCount == 2) {
  rgba = OIIO::ImageBufAlgo::channels(oriented, 4, channelOrder, alphaOne);
} else if (channelCount == 3) {
  rgba = OIIO::ImageBufAlgo::channels(oriented, 4, channelOrder, alphaOne);
} else if (channelCount >= 4) {
  rgba = OIIO::ImageBufAlgo::channels(oriented, 4, channelOrder);  // 元アルファを維持
}
```

### 修正 B（補強）: 失敗キャッシュの再試行を許容

- `failedPreviewPaths_` に入れる前に、後続フォールバック（WIC/Shell）が全て空だった場合のみ失敗とみなす。
- または失敗エントリに有効期限（世代またはタイムスタンプ）を設け、一定時間経過後に再デコードを許可する。

---

## 影響範囲・制約

- 変更ファイル: `Artifact/src/Widgets/Asset/ArtifactAssetBrowser.cppm`（既存行の編集のみ）。
- CRLF 維持: `edit` ツールで該当箇所のみ書き換え（行末を維持）。
- OIIO は直接シンボル参照（dlopen ではない）のため、リンク時点で OIIO は利用可能。本バグは実行時にも確実に発現する。
- `QImage` 新規採用禁止ルールには抵触しない（既存の `loadImageThumbnailViaOIIO` 内でのみ使用）。

---

## 未確認事項

- 実環境で OIIO デコードが透過画像を返すことを runtime で確認済みか（本診断はコード静観に基づく）。
- 修正 A 適用後の PNG（アルファ付き）サムネイルが正しく透過表示されるかの確認が必要。
