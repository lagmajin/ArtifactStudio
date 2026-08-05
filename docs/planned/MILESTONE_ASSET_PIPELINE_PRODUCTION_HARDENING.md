# MILESTONE: Asset Pipeline Production Hardening

**日付**: 2026-08-04
**現状**: AssetDatabase + AssetManager + AssetBrowser UI は機能的。しかし AssetConverter は空スタブ、VectorImport はデータ構造のみ、プロキシシステムは不在、ImageAssetFile 等の具象クラス不在。
**目標**: 型付きアセットクラス、フォーマット正規化パイプライン、プロキシ生成、バッチ変換、ベクターインポートの実装。

## 現状サマリ

| コンポーネント | ファイル | 行数 | 状態 |
|---------------|---------|------|------|
| AssetDatabase | `ArtifactCore/src/Asset/AssetDatabase.cppm` | 181 | ✅ 機能 |
| AssetManager | `ArtifactCore/src/Asset/AssetManager.cppm` | 385 | ✅ 機能 |
| AssetImporter | `ArtifactCore/src/Asset/AssetImporter.cppm` | 68 | ✅ 基本機能（単一ファイル） |
| AbstractAssetFile | `ArtifactCore/src/Asset/AbstractAssetFile.cppm` | 237 | ✅ PIMPL 基底クラス |
| DataAssetFile | `ArtifactCore/include/Asset/DataAssetFile.ixx` | 100 | ✅ CSV のみ |
| AssetSequence | `ArtifactCore/include/Asset/AssetSequence.ixx` | 250 | ✅ フレームトークン検出 |
| AssetBrowser | `Artifact/src/Widgets/Asset/ArtifactAssetBrowser.cppm` | 3,500 | ✅ 全機能 |
| **AssetConverter** | `ArtifactCore/include/Asset/AssetConverter.ixx` | **10** | ❌ **空スタブ** |
| **AssetMetaFile** | `ArtifactCore/include/Asset/AssetMetaFile.ixx` | **10** | ❌ **空スタブ** |
| **ImageAsset** | `ArtifactCore/include/Asset/ImageAsset.ixx` | **12** | ❌ **空スタブ** |
| **VectorImport** | `ArtifactCore/include/Asset/VectorImport.ixx` | 132 | ❌ **型定義のみ・パーサなし** |
| AssetImportSetting | `ArtifactCore/include/Asset/AssetImportSetting.ixx` | 8 | ❌ **空スタブ** |

---

## Phase 1: 具象アセットクラス

### 1.1 ImageAssetFile

`AbstractAssetFile` を継承する具象クラスを作成。画像アセットのメタデータを型付きで保持する。

```cpp
// ImageAsset.ixx（現行の空スタブを置き換え）
class ImageAssetFile : public AbstractAssetFile {
public:
    struct Meta {
        int width = 0;
        int height = 0;
        int channelCount = 0;
        int bitDepth = 0;
        QString colorSpace;       // "sRGB", "ACEScg", "Linear"
        QString compressionType;  // "None", "Zip", "PIZ", "DWAA"
        int64_t rawByteSize = 0;
    };

    const Meta& imageMeta() const;
    bool generateProxy(int maxWidth, int maxHeight, const QString& outputPath);
    QImage thumbnail(int size) const;      // OIIO 経由でキャッシュ付き
};
```

実装ステップ:
1. `ArtifactCore/include/Asset/ImageAsset.ixx` を空から埋める
2. `ArtifactCore/src/Asset/ImageAsset.cppm` を作成
3. `AssetImporter::importFile()` 内で `ImageAssetFile` を生成し `AssetDatabase` に登録
4. メタデータ読み取り: OIIO の `ImageInput::open()` → `ImageSpec` から width/height/channels/format/compression を取得
5. `generateProxy()`: OIIO で指定サイズにリサイズし JPEG 出力。プロキシは `project.proxies/` に保存

### 1.2 VectorAssetFile

ベクターアセット用の具象クラス。

```cpp
class VectorAssetFile : public AbstractAssetFile {
public:
    enum class Kind { Svg, Pdf, Eps, Ai, Affinity };
    
    struct Meta {
        Kind kind;
        std::vector<QString> pageNames;
        QSizeF boundingRect;
    };

    const Meta& vectorMeta() const;
    QImage rasterizeAt(float width, float height, float dpi = 72.0f) const;
};
```

実際のパースは Phase 3 で実装する。ここではクラス定義とメタデータだけ。

### 1.3 既存 Layer クラスからのメタデータ抽出

`ArtifactImageLayer`、`ArtifactVideoLayer`、`ArtifactAudioLayer` がロード時に抽出しているメタデータ（解像度、フレームレート、サンプルレート等）を `AbstractAssetFile::setMeta()` 経由で AssetDatabase に書き戻す。

**変更対象**: `Artifact/src/Layer/ArtifactImageLayer.cppm`、`ArtifactVideoLayer.cppm`、`ArtifactAudioLayer.cppm` に `load()` 成功時に `AssetDatabase::instance()->findByPath(path)->setMeta(key, value)` の呼び出しを追加。

### 1.4 完了条件

- [ ] `ImageAssetFile` が画像を開き、メタデータ（width/height/channels/bitDepth）を読み取れる
- [ ] `VectorAssetFile` が SVG/PDF/EPS/AI/Affinity を検出し、`Kind` を設定できる
- [ ] AssetBrowser が `ImageAssetFile` / `VectorAssetFile` のメタデータを表示する（ツールチップ、情報パネル）

---

## Phase 2: AssetConverter — フォーマット正規化パイプライン

`AssetConverter.ixx` を空スタブから実用的な変換パイプラインにする。

### 2.1 コア設計

```cpp
class ArtifactAssetConverter : public QObject {
public:
    struct ConversionJob {
        QString sourcePath;
        QString outputPath;
        AssetType sourceType;
        QString targetFormat;      // "png", "exr", "jpeg"
        QSize maxResolution;
        int jpegQuality = 85;
        bool normalizeColorSpace = true;
    };

    struct ConversionResult {
        bool success;
        QString outputPath;
        QString errorMessage;
        int64_t outputSizeBytes;
        double durationMs;
    };

    // 単一ファイル変換
    ConversionResult convert(const ConversionJob& job);

    // バッチ変換（進捗シグナル付き）
    void convertBatch(const std::vector<ConversionJob>& jobs);
    
    // プロキシ生成（高解像度→低解像度）
    ConversionResult generateProxy(const QString& sourcePath,
                                    int maxWidth, int maxHeight,
                                    const QString& proxyDir);

signals:
    void batchProgress(int current, int total);
    void conversionFinished(ConversionResult result);
};
```

### 2.2 画像変換の実装

OIIO を使用した変換パイプライン:

```cpp
ConversionResult ArtifactAssetConverter::convert(const ConversionJob& job) {
    // 1. OIIO ImageInput でソースを開く
    auto in = OIIO::ImageInput::open(job.sourcePath.toStdString());
    
    // 2. ImageSpec からフォーマット情報を取得
    const auto& spec = in->spec();
    
    // 3. 必要に応じてカラースペース正規化
    OIIO::ImageSpec outSpec = spec;
    if (job.normalizeColorSpace) {
        // OCIO 経由でカラースペース変換
    }
    
    // 4. リサイズ（maxResolution 指定時）
    if (job.maxResolution.isValid()) {
        float scale = std::min(
            (float)job.maxResolution.width() / spec.width,
            (float)job.maxResolution.height() / spec.height
        );
        if (scale < 1.0f) {
            outSpec.width = spec.width * scale;
            outSpec.height = spec.height * scale;
        }
    }
    
    // 5. OIIO ImageOutput で出力
    auto out = OIIO::ImageOutput::create(job.outputPath.toStdString());
    out->open(job.outputPath.toStdString(), outSpec);
    out->copy_image(in.get());
    out->close();
}
```

### 2.3 プロキシ生成パイプライン

インポート時に自動プロキシ生成:

```
AssetImporter::importFile(path)
  → AssetConverter::generateProxy(source, 1920, 1080, proxyDir)
  → proxyPath = proxyDir / hash(sourcePath) + ".jpg"
  → AssetDatabase::setMeta(sourcePath, "proxy", proxyPath)
```

プロキシ設定:
- デフォルト: 1920x1080, JPEG quality 85
- 設定可能: `ApplicationSettingDialog` → `Asset` タブ → `Proxy Resolution`
- プロキシディレクトリ: `{projectDir}/.artifact/proxies/`

### 2.4 バッチ変換キュー

`AssetConverter` を `BackgroundTask` で非同期実行:

```cpp
// AssetBrowser のコンテキストメニューから起動
void ArtifactAssetBrowser::convertSelectionToFormat(const QString& format) {
    std::vector<ConversionJob> jobs;
    for (auto& path : selectedAssetPaths()) {
        jobs.push_back({
            .sourcePath = path,
            .outputPath = changeExtension(path, format),
            .targetFormat = format
        });
    }
    
    auto* task = new BackgroundTypedTask<ConversionResult>(
        [converter, jobs = std::move(jobs)]() {
            return converter->convertBatch(jobs);
        }
    );
    connect(task, &BackgroundTypedTask::progressChanged, this, [](int c, int t) {
        statusBar->showMessage(QString("Converting %1/%2...").arg(c).arg(t));
    });
    backgroundWorker->enqueue(task);
}
```

### 2.5 完了条件

- [ ] EXR → PNG/JPEG 変換が動作
- [ ] 4K → 1080p プロキシ生成が動作
- [ ] インポート時に自動プロキシ生成（設定で無効化可能）
- [ ] バッチ変換の進捗表示 + キャンセル機能
- [ ] 変換失敗時のエラーレポート

---

## Phase 3: Vector Import — SVG/PDF/EPS パース

### 3.1 SVG パース

Qt6::Svg はリンク済みだが Asset モジュールで使用されていない。

```cpp
// VectorImport.cppm（新規実装）
class SvgImporter {
public:
    VectorImportResult import(const QString& svgPath, VectorImportOptions options);

private:
    QSvgRenderer renderer_;
    std::vector<VectorPathGroup> pathGroups_;
    
    void parseElement(QDomElement& element, const QTransform& parentTransform);
    void parsePath(const QString& dAttribute, const QTransform& transform,
                   const VectorStyle& style);
    void parseGroup(QDomElement& element, const QTransform& transform);
};

struct VectorPathGroup {
    QString id;
    QString layerName;
    std::vector<VectorPath> paths;
    VectorStyle style;
    QTransform transform;
    bool visible;
};

struct VectorImportResult {
    bool success;
    QString errorMessage;
    std::vector<VectorPathGroup> rootGroups;
    QRectF boundingRect;
    int elementCount;
    int pathCount;
};
```

既存の `VectorImport.ixx` の `makeVectorImportResult()` を、実際のパースを行う実装に置き換える。

### 3.2 PDF/EPS パース

PDF パースには入出力の SimpleWav/Codec と同じパターンで Poppler または PDFium を利用:

```
# vcpkg.json に追加
"poppler"  // PDF レンダリング
```

`PdfImporter`:
```cpp
class PdfImporter {
public:
    VectorImportResult import(const QString& pdfPath, VectorImportOptions options);
    int pageCount() const;
    QImage renderPage(int pageIndex, float dpi = 72.0f) const;
    std::vector<QString> pageLabels() const;
};
```

EPS パース:
- EPS は Ghostscript または ImageMagick 経由で SVG に変換し、SVG パーサに流す
- または単純にラスタライズのみ行う

### 3.3 AI/Affinity ファイル

- Illustrator (`.ai`): PDF 互換サブセットを持つ。PDF としてパース試行 → 失敗時エラー
- Affinity (`.afdesign`, `.afphoto`, `.afpub`): プロプライエタリ形式。パーサは未実装。ファイルタイプ検出のみ

### 3.4 完了条件

- [ ] SVG ファイルをインポートし、パスグループとして構造化できる
- [ ] PDF ファイルからページ数・テキスト・パスの抽出ができる
- [ ] EPS → ラスタライズが動作（最低限）
- [ ] インポート結果が Vector Layer に変換可能な形式で出力される
- [ ] インポートエラー時のエラーメッセージが明確

---

## Phase 4: AssetMetaFile — サイドカーファイル形式

### 4.1 フォーマット設計

JSON ベースのメタデータサイドカー:

```json
{
  "version": 1,
  "asset": {
    "sourcePath": "textures/brick_wall_4k.exr",
    "uuid": "550e8400-e29b-41d4-a716-446655440000",
    "type": "Image",
    "importedAt": "2026-08-04T12:00:00Z",
    "lastModified": "2026-08-04T12:00:00Z"
  },
  "image": {
    "width": 4096,
    "height": 4096,
    "channels": 4,
    "bitDepth": 32,
    "colorSpace": "ACEScg"
  },
  "proxies": {
    "2K": "proxies/brick_wall_4k_2K.jpg",
    "1K": "proxies/brick_wall_4k_1K.jpg",
    "thumb": "proxies/brick_wall_4k_thumb.jpg"
  },
  "tags": ["brick", "wall", "texture", "pbr"],
  "custom": {}
}
```

### 4.2 実装

`AssetMetaFile.ixx` + `AssetMetaFile.cppm` を実装:

```cpp
class ArtifactAssetMetaFile {
public:
    static ArtifactAssetMetaFile load(const QString& assetPath);
    static ArtifactAssetMetaFile fromJson(const QByteArray& json);
    static QString metaPathFor(const QString& assetPath);  // "image.exr" → "image.exr.assetmeta"

    bool save() const;
    QJsonObject toJson() const;
    
    // 基本メタデータ
    QString sourcePath() const;
    QUuid uuid() const;
    AssetType type() const;
    QDateTime importedAt() const;
    
    // プロキシ管理
    QStringList proxyResolutions() const;
    QString proxyPath(const QString& resolution) const;
    void addProxy(const QString& resolution, const QString& path);
    
    // タグ
    QStringList tags() const;
    void addTag(const QString& tag);
    void removeTag(const QString& tag);
    
    // カスタムデータ
    QVariant customValue(const QString& key) const;
    void setCustomValue(const QString& key, const QVariant& value);

private:
    QJsonObject data_;
    QString assetPath_;
};
```

### 4.3 インポート時の自動生成

`AssetImporter::importFile()` で自動的にメタファイルを生成:

```cpp
// AssetImporter::importFile()
auto metaFile = ArtifactAssetMetaFile::create(assetPath);
metaFile.setType(detectedType);
metaFile.setImportedAt(QDateTime::currentDateTime());

// 画像の場合、OIIO でメタデータを読み取り
if (detectedType == AssetType::Image) {
    auto in = OIIO::ImageInput::open(assetPath);
    metaFile.setCustomValue("width", in->spec().width);
    metaFile.setCustomValue("height", in->spec().height);
    // ...
}
metaFile.save();
```

### 4.4 完了条件

- [ ] 画像インポート時に `.assetmeta` ファイルが自動生成される
- [ ] プロキシ生成時にメタファイルが更新される
- [ ] AssetBrowser がメタファイルからメタデータを読み取り表示する
- [ ] タグの追加/削除が永続化される

---

## Phase 5: バッチリリンク + 参照トラッカー

### 5.1 AssetReferenceTracker

```cpp
class AssetReferenceTracker {
public:
    struct Reference {
        QUuid sourceId;         // 参照元レイヤー/コンポジション
        QUuid targetId;         // 参照先アセット
        QString targetPath;     // 現在のファイルパス
        QString sourceDescription;  // "Layer 'Background' in Composition 'Main'"
    };

    // プロジェクト全体の参照グラフを構築
    void buildReferenceGraph();
    
    // 参照切れ（missing）のアセットを検出
    std::vector<Reference> findMissingReferences() const;
    
    // 特定アセットへの全参照を検索
    std::vector<Reference> findReferencesTo(const QUuid& assetId) const;
    
    // パス変更の一括適用
    int relinkAll(const QMap<QUuid, QString>& newPaths);

private:
    std::vector<Reference> references_;
};
```

### 5.2 バッチリリンク UI

`ArtifactAssetBrowser` にリリンクダイアログを追加:

1. アセット一覧をフィルター → `Status: Missing`
2. 「Relink All」ボタンでダイアログ表示
3. 各 missing アセットに対して:
   - 元のファイル名を提案
   - 手動ブラウズ
   - ディレクトリ指定で一括検索
   - 複数選択して一括リリンク

### 5.3 完了条件

- [ ] プロジェクト全体の参照グラフが構築できる
- [ ] missing アセットが一覧表示される
- [ ] バッチリリンクで複数アセットを一括修正できる
- [ ] Undo 対応

---

## ファイル一覧

| Phase | ファイル | 変更内容 |
|-------|---------|---------|
| P1 | `ArtifactCore/include/Asset/ImageAsset.ixx` | 空→具象クラス実装 |
| P1 | 新規 `ArtifactCore/src/Asset/ImageAsset.cppm` | メタデータ読み取り、プロキシ生成 |
| P1 | `ArtifactCore/include/Asset/VectorImport.ixx` | VectorAssetFile 追加 |
| P1 | `Artifact/src/Layer/ArtifactImageLayer.cppm` | メタデータ書き戻し追加 |
| P2 | `ArtifactCore/include/Asset/AssetConverter.ixx` | 空→変換パイプライン実装 |
| P2 | 新規 `ArtifactCore/src/Asset/AssetConverter.cppm` | OIIO 変換、プロキシ、バッチ |
| P2 | `ArtifactCore/include/Asset/AssetImportSetting.ixx` | 空→インポート設定実装 |
| P3 | `ArtifactCore/src/Asset/VectorImport.cppm` | 新規: SVG パーサ実装 |
| P3 | 新規 `ArtifactCore/src/Asset/PdfImporter.cppm` | PDF パーサ |
| P4 | `ArtifactCore/include/Asset/AssetMetaFile.ixx` | 空→実装 |
| P4 | `ArtifactCore/src/Asset/AssetMetaFile.cppm` | JSON 読み書き |
| P5 | 新規 `ArtifactCore/include/Asset/AssetReferenceTracker.ixx` | 参照グラフ |
| P5 | `Artifact/src/Widgets/Asset/ArtifactAssetBrowser.cppm` | バッチリリンクダイアログ追加 |

## 優先度・工数

| Phase | 優先度 | 工数 | 理由 |
|-------|--------|------|------|
| P1: 具象アセットクラス | **P0** | 中 | 基盤。全アセットコードが型なし `void*` で運用されている |
| P2: AssetConverter | **P0** | 中 | 空スタブ。サムネイル生成が毎回の再デコードに依存している |
| P4: AssetMetaFile | **P0** | 小 | タグ・プロキシ管理の基盤 |
| P3: Vector Import | **P1** | 大 | SVG/PDF パーサ実装。Qt::Svg 活用で SVG は容易 |
| P5: Batch Relink | **P1** | 中 | 参照グラフ構築が主 |
