# M-DCC-1: DCCツール連携完全強化マスタープラン

**マイルストーンID**: M-DCC-1-2026-06-28  
**優先度**: P0 (Critical)  
**カテゴリ**: Interoperability / DCC Integration / Export/Import  
**作成日**: 2026-06-28  
**状態**: 計画中
**最終更新:** 2026-08-15
**現行判定:** 画像／動画／JSON／SVG／Lottie等の入出力基盤は広いが、DCCの完全な往復変換とPSD／USDの構造保持は未完了

---

## 🎯 **目標**

**ArtifactStudioをDCCツール (Digital Content Creation Tools) との連携で業界最高レベルに押し上げる**

Nowアプリケーションの制作ワークフローで、ArtifactStudioを中心ツールとして使用できるように、主要なDCCツールとのインポート/エクスポート機能を完全に実装します。

---

## 📊 **現状分析 (2026-06-28 現在)**

### **🟢 完全に実装済み (95%+)**

#### 画像フォーマット (Image Formats)
| 形式 | 状況 | 実装場所 | 注釈 |
|---|---|---|---|
| **PNG** | ✅ 完全 | `ImageImporter/Exporter.cppm` (OIIO) | RGBA,ロスレス |
| **JPEG** | ✅ 完全 | `ImageImporter/Exporter.cppm` (OIIO) | 圧縮 |
| **TIFF** | ✅ 完全 | `ImageImporter/Exporter.cppm` (OIIO) | 高品質,マルチチャンネル |
| **BMP** | ✅ 完全 | `ImageImporter/Exporter.cppm` (OIIO) | 基本フォーマット |
| **EXR** | ✅ 完全 | `ImageImporter/Exporter.cppm` (OIIO) | 32bit浮動小数点, HDR |
| **WebP** | ✅ 完全 | `ImageImporter/Exporter.cppm` (OIIO) | Web最適化, アニメーション可能 |
| **GIF** | ✅ 完全 | `FFmpegEncoder.cppm` / `FFmpegVideoDecoder.cppm` | アニメーションGIF, 静止画像とも |
| **APNG** | ✅ 完全 | `FFmpegEncoder.cppm` | アニメーションPNG |

#### 動画フォーマット (Video Formats)
| 形式 | 状況 | 実装場所 | コーデック | 注釈 |
|---|---|---|---|---|
| **MP4** | ✅ 完全 | `FFmpegVideoDecoder.cppm`, `FFmpegEncoder.cppm` | H.264, H.265, ProRes |  |
| **MOV** | ✅ 完全 | `FFmpegVideoDecoder.cppm`, `FFmpegEncoder.cppm` | ProRes, H.264 | QuickTime |
| **AVI** | ✅ 完全 | `FFmpegVideoDecoder.cppm`, `FFmpegEncoder.cppm` | 各種 |  |
| **WebM** | ✅ 完全 | `FFmpegVideoDecoder.cppm`, `FFmpegEncoder.cppm` | VP9, VP8 |  |
| **MKV** | ✅ 完全 | `FFmpegVideoDecoder.cppm`, `FFmpegEncoder.cppm` | 各種 |  |
| **Image Sequence** | ✅ 完全 | `FFmpegEncoder.cppm` | PNG, JPEG, TIFF | フレーム別 |

#### 3Dフォーマット (3D Formats)
| 形式 | 状況 | 実装場所 | 注釈 |
|---|---|---|---|
| **GLTF/GLB** | ✅ 完全 | DiligentEngine | PBRマテリアル, Web標準 |
| **USD** | ⚠️ 部分的 | DiligentEngine (サンプル) | ビューア実装済み |

#### ベクター/シェイプ (Vector/Shape)
| 形式 | 状況 | 実装場所 | 注釈 |
|---|---|---|---|
| **SVG** | ✅ 完全 | `ShapeLayer.cppm` | `toSvg()`, `fromSvg()` |
| **JSON** | ✅ 完全 | `ShapePath.cppm`, `ShapeGroup.cppm` | シリアライゼーション |

#### オーディオフォーマット (Audio Formats)
| 形式 | 状況 | 実装場所 | 注釈 |
|---|---|---|---|
| **WAV** | ✅ 完全 | `FFmpegAudioEncoder.cppm` | PCM,ロスレス |
| **MP3** | ✅ 完全 | `FFmpegAudioEncoder.cppm` | MPEG Audio |
| **AAC** | ✅ 完全 | `FFmpegAudioEncoder.cppm` (行192-193) | 既に実装済み |
| **FLAC** | ✅ 完全 | `FFmpegAudioEncoder.cppm` (行196) | 既に実装済み |
| **OGG/Vorbis** | ✅ 完全 | `FFmpegAudioEncoder.cppm` (行197) | 既に実装済み |
| **Opus** | ✅ 完全 | `FFmpegAudioEncoder.cppm` (行195) | 既に実装済み |

#### プロジェクトフォーマット (Project Formats)
| 形式 | 状況 | 実装場所 | 注釈 |
|---|---|---|---|
| **JSON** | ✅ 完全 | `ArtifactProjectExporter/Importer.cppm` | プロジェクト全体 |

---

### **🔍 深掘りソースコード分析結果 (2026-06-28 追加分析)**

#### **1. UI列挙型 vs 実際の実装ギャップ**

**問題**: 列挙型 (`ImageFormat`, `VideoCodec`, `AudioCodec`) には含まれていないが、コーデックレベルでサポートされているフォーマットが多数存在します。

| 列挙型 | 実際のサポート | 状況 |
|---|---|---|
| `ImageFormat` (PNG, JPEG, TIFF, EXR, BMP) | + WebP, GIF, APNG | ⚠️ UIに未露出 |
| `VideoCodec` (H264, H265, VP8, ProRes) | + VP9, GIF, APNG, WebP | ⚠️ UIに未露出 |
| `AudioCodec` (AAC, MP3, OPUS) | + FLAC, Vorbis | ⚠️ UIに未露出 |

**推奨**: UI列挙型を拡張して全てのサポートフォーマットを露出させる。

#### **2. FFmpeg経由でサポートされるフォーマット**

**ビデオエンコード (FFmpegEncoder.cppm)**:
- コンテナ: MP4, MOV, AVI, WebM, MKV, GIF (アニメーション), Image Sequence
- コーデック: H.264, H.265, VP9, VP8, ProRes, MJPEG, PNG, GIF, APNG, WebP
- HDR: HDR10 PQ, HLG BT2020
- ハードウェアエンコード: NVENC, AMF, QSV, VAAPI

**オーディオエンコード/デコード**:
- FFmpegAudioEncoder: AAC, MP3, Opus, FLAC, Vorbis
- FFmpegAudioDecoder: FFmpegがサポートする全てのオーディオフォーマット

#### **3. OIIO経由でサポートされるフォーマット**

`ImageImporter/Exporter.cppm` は OpenImageIO (OIIO) を使用しており、OIIO 2.4+では以下がサポート:
- **既に確認**: PNG, JPEG, TIFF, EXR, BMP, WebP
- **OIIO 2.4+でサポート**: HEIF, AVIF (列挙型に未追加)
- **可能性**: OpenEXR 3.0, Deep Image, Multi-part TIFF

#### **4. 32-bit画像処理 (ImageF32x4_RGBA.cppm)**

- OpenCVを使用した32-bit浮動小数点画像処理
- `CV_32FC4`形式でRGBA画像を処理
- HDR画像処理に対応
- 現状: EXRフォーマットで32-bit浮動小数点がサポート

#### **5. PSDDocumentの深掘り分析**

**完全に実装済み (PSDDocument.cppm)**:
- PsdHeader: ファイルヘッダーのパース
- PsdSectionInfo: セクション情報の抽出
- PsdLayerInfo: レイヤー情報 (name, bounds, opacity, blendMode, channels, visible, clipped)
- flattenedPreview(): フラット化されたプレビュー画像
- レイヤー情報の完全パース (行325-327)

**未実装**:
- `extractLayerImages()`: 各レイヤーの個別画像抽出
- `convertToArtifactLayers()`: ArtifactStudioレイヤーへの変換
- チャンネルデータの個別読み込み
- マスクデータの処理
- 調整レイヤーの処理

#### **6. TransitionManagerの現状**

`ArtifactTransition.cppm` (行1006-1015) に登録されているトラジション:
1. CrossDissolve
2. WipeLeft
3. WipeRight
4. SlideLeft
5. SlideRight
6. ZoomIn
7. ZoomOut
8. Glitch
9. PageCurl
10. RippleDissolve

**状況**: 10個のトラジションが登録済み。

---

### **🟡 部分的に実装済み (50-95%)**

#### PSD (Photoshop Document)
```cpp
// ArtifactCore/src/Image/PSDDocument.cppm
✅ **完全にパース可能な機能:**
- PsdHeader (ファイルヘッダー)
- PsdSectionInfo (セクション情報)
- PsdLayerInfo (レイヤー情報) - name, bounds, opacity, blendMode, channels, visible, clipped, etc.
- flattenedPreview() - フラット化されたプレビュー画像

❌ **未実装の機能:**
- extractLayerImages() - 各レイヤーの個別画像抽出
- convertToArtifactLayers() - ArtifactStudioレイヤーへの変換
```

**状況:**
- ✅ **レイヤー情報は完全にパース可能**
- ✅ **フラット化プレビューは取得可能**
- ❌ **レイヤー構造の維持は未実装**

---

### **🔴 完全に未実装 (0%)**

#### 🔍 **ソースコード分析で新たに発見された既存実装**

**修正された状況:**
- ✅ **GIF (アニメーション)** - `FFmpegEncoder.cppm` (行137-138, 234-235, 282-283) で既にサポート
- ✅ **AAC** - `FFmpegAudioEncoder.cppm` (行192-193) で既にサポート  
- ✅ **FLAC** - `FFmpegAudioEncoder.cppm` (行196) で既にサポート
- ✅ **OGG/Vorbis** - `FFmpegAudioEncoder.cppm` (行197) で既にサポート
- ✅ **Opus** - `FFmpegAudioEncoder.cppm` (行195) で既にサポート
- ✅ **APNG** - `FFmpegEncoder.cppm` (行139-140, 236-237, 284-285) で既にサポート
- ✅ **WebP (アニメーション)** - `FFmpegEncoder.cppm` (行141-142) で既にサポート

> **注**: これらのフォーマットはコーデックレベルで既に実装されていますが、UIの列挙型 (`ImageFormat`, `VideoCodec`, `AudioCodec`) に含まれていない場合があります。

#### P0 - 絶対優先 (1-2日で実装可能)
| 形式 | DCCツール | 実装難易度 | 予想効果 | 要望レベル |
|---|---|---|---|---|
| **M4A** | Final Cut | ⭐ (極小) | +5%ユーザー | 🟠 |
| **HEIF** | iOS, macOS | ⭐⭐ | +8%ユーザー | 🟡 |
| **AVIF** | Web, 最新 | ⭐⭐ | +5%ユーザー | 🟡 |

#### P1 - 高優先 (1-2週間で実装)
| 形式 | DCCツール | 実装難易度 | 予想効果 | 要望レベル |
|---|---|---|---|---|
| **PSDレイヤー構造** | Photoshop | ⭐⭐⭐ | +30%ユーザー | 🟠 |
| **Lottieエクスポート** | AE, Figma, Web | ⭐⭐⭐ | +40%ユーザー | 🟡 |
| **AEPインポート** | After Effects | ⭐⭐⭐⭐ | +50%ユーザー | 🔝🟠 |
| **PDF** | Adobe, 全般 | ⭐⭐⭐ | +15%ユーザー | 🟡 |
| **EPS** | Adobe, 全般 | ⭐⭐⭐ | +10%ユーザー | 🟠 |
| **SRT字幕** | 全般 | ⭐ | +10%ユーザー | 🟠 |
| **ASS字幕** | 全般 | ⭐⭐ | +8%ユーザー | 🟠 |

#### P2 - 中優先 (1ヶ月で実装)
| 形式 | DCCツール | 実装難易度 | 予想効果 | 要望レベル |
|---|---|---|---|---|
| **PMD/PMX** | MMD | ⭐⭐⭐⭐ | +30%ユーザー | 🔝🔴 |
| **VMD** | MMD | ⭐⭐⭐ | +30%ユーザー | 🔝🔴 |
| **FBX** | 全3Dツール | ⭐⭐⭐⭐⭐ | +25%ユーザー | 🟡 |
| **USD完全対応** | Pixar, NVIDIA | ⭐⭐⭐⭐ | +20%ユーザー | 🟡 |
| **TTF/OTF** | フォント | ⭐⭐⭐ | +10%ユーザー | ⚠️ |
| **LUT (3D/CUBE)** | カラータグリング | ⭐⭐⭐ | +8%ユーザー | 🟠 |
| **KEP** | Kdenlive | ⭐⭐⭐⭐ | +10%ユーザー | ⚠️ |

#### P3 - 低優先 (将来的)
| 形式 | DCCツール | 実装難易度 | 予想効果 | 要望レベル |
|---|---|---|---|---|
| **Nukeスクリプト** | Nuke | ⭐⭐⭐⭐⭐ | +15%ユーザー | 🟡 |
| **Blendファイル** | Blender | ⭐⭐⭐⭐⭐ | +10%ユーザー | ⚠️ |
| **PRPROJ** | Premiere | ⭐⭐⭐⭐⭐ | +15%ユーザー | 🟡 |
| **AI (Adobe Illustrator)** | Illustrator | ⭐⭐⭐⭐⭐ | +12%ユーザー | 🟠 |
| **INDD (InDesign)** | InDesign | ⭐⭐⭐⭐⭐ | +8%ユーザー | ⚠️ |

---

## 📋 **DCCツール別連携状況**

| DCCツール | 連携スコア | ✅ 可能 | ❌ 不可能 | 主なギャップ |
|---|---|---|---|---|
| **After Effects** | 45/100 | SVG, JSON, GIF, WebP, APNG | AEP, Lottie | AEPインポート, Lottieエクスポート |
| **Photoshop** | 65/100 | PNG, PSD(フラット), JPEG, TIFF, EXR, BMP, WebP, GIF | PSD(レイヤー) | レイヤー構造の維持 |
| **Premiere Pro** | 65/100 | MP4, MOV, WAV, MP3, AAC, FLAC, OGG, Opus | PRPROJ, M4A | プロジェクトファイル, M4A |
| **Blender** | 70/100 | GLB, EXR, MP4, WebM, MKV | FBX, Alembic | 3D形式の充実 |
| **Nuke** | 55/100 | EXR, TIFF, MP4, MOV | スクリプト, Deep Image | スクリプト互換 |
| **Fusion** | 45/100 | EXR, TIFF, MP4, MOV | コンポジット, スクリプト | プロジェクトファイル |
| **Unity** | 80/100 | GLB, MP4, MOV | USD(部分), FBX | USD完全対応 |
| **Unreal Engine** | 80/100 | GLB, MP4, MOV | USD(部分), FBX | USD完全対応 |
| **Figma** | 65/100 | SVG, PNG, JPEG | Lottie | Lottie互換 |
| **DaVinci Resolve** | 55/100 | MP4, MOV, WAV, MP3, AAC | DRP, EDL | プロジェクトファイル |
| **MMD** | 20/100 | 拡張子登録 | ジオメトリ, モーション | PMD/VMD読み込み |

---

## 🎯 **詳細仕様**

---

## 2026-08-15 現行実装監査

- JSON project、画像／動画／音声の FFmpeg／OIIO 経路、SVG／Shape、Lottie／Unity UXML／RML／Gameface／Noesis 系の export surface は現行コードに存在する。
- Asset／Mesh 側には OBJ／FBX／glTF／Alembic／STL の候補や mesh importer 基盤があるが、形式ごとの完全な scene／material／animation round-trip を示す共通契約は確認できない。
- PSD は header／section／layer metadata／flattened preview の基盤に留まり、個別 layer image、mask、adjustment、Artifact layer 変換は未完了。USD も部分対応扱いを維持する。
- したがって「業界主要DCCとの完全連携」は未達。実ファイル往復、欠落情報レポート、UI列挙と codec 実装の一致、runtime parity は未検証。

## Static audit follow-up (2026-07-25)

現行ソースを再確認した。画像・動画・音声・通常 EXR の入出力は広く存在する一方、DCC プロジェクト／レイヤー構造の相互変換を「完全実装」と扱える状態ではない。

| 領域 | 現行ソースで確認できたこと | 判定 |
|---|---|---|
| 画像・通常 EXR | OIIO の import/export と float / named-channel / AOV 経路がある | 実装済み（deep は別） |
| 動画・音声 | FFmpeg の主要コンテナ・codec 経路がある | 部分〜実装済み（UI露出は別確認） |
| SVG / JSON | shape の SVG と JSON シリアライズ経路がある | 実装済み（範囲限定） |
| PSD | header / layer metadata / flattened preview はあるが、個別レイヤー画像・Artifact layer 変換・mask/adjustment の完全経路は未確認 | 部分実装 |
| GLTF / GLB | asset 判定・3D viewer 側の受け口はあるが、DCC round-trip の完全性は未確認 | 部分実装 |
| USD / FBX / Alembic | DCC 向けの完全 import/export 経路は確認できない | 未完了 |
| AEP / PRPROJ / DRP / Nuke script / Blend | プロジェクトファイルの相互変換実装は確認できない | 未実装 |
| Lottie /字幕 /フォント等 | 本文で候補に挙げた形式の一括実装は確認できない | 未完了 |

**判定**: フォーマット基盤は進展しているが、M-DCC-1 の「主要 DCC との完全連携」には未達。特に PSD レイヤー保持、USD/FBX/Alembic、DCC プロジェクト形式、round-trip 検証が残っている。本文の「95%+」表記は DCC 連携全体の現行証拠とは整合しないため、機能単位の状態表を優先する。

---

### **1. GIFアニメーション対応 (P0)**

#### **背景**
ユーザー調査によると、GIFアニメーションは「誰もが一番最初に試して動かなくてがっかりする」フォーマット。20行程度のコードで実装可能。

#### **実装方法**
**方法A: FFmpegを使用 (推奨)**
```cpp
// FFmpegEncoder.cppm に追加
FFmpegEncoderSettings settings;
settings.container = "gif";
settings.videoCodec = "gif";
settings.framerate = 24; // or user specified
settings.width = image.width();
settings.height = image.height();
```

**方法B: ImageMagickを使用**
```cpp
// 外部ツールを使用
// GIFアニメーション用の専用エンコーダー
```

#### **使用例**
```cpp
FFmpegEncoder encoder;
encoder.open("output.gif", gifSettings);
for (const auto& frame : frames) {
    encoder.writeFrame(frame);
}
encoder.close();
```

#### **完了基準**
- [ ] GIFアニメーションのインポート
- [ ] GIFアニメーションのエクスポート
- [ ] フレームレートの指定
- [ ] ループ設定
- [ ] 画質設定

---

### **2. PSDレイヤー構造インポート (P0)**

#### **背景**
`PsdDocument` クラスは既に **レイヤー情報を完全にパース可能** で、以下の情報が取得できる:
- レイヤー名
- バウンディングボックス
- 不透明度
- ブレンドモード
- 可視性
- チャンネル情報

**不足しているのはレイヤー別の画像抽出のみ。**

#### **実装方法**
```cpp
// PSDDocument.cppm に追加

/// 各レイヤーの画像を抽出
std::vector<RawImage> PsdDocument::extractLayerImages() const
{
    std::vector<RawImage> layerImages;
    layerImages.reserve(layers_.size());
    
    for (const auto& layer : layers_) {
        RawImage layerImage = extractSingleLayerImage(layer);
        layerImages.push_back(layerImage);
    }
    return layerImages;
}

/// 単一レイヤーの画像を抽出
RawImage PsdDocument::extractSingleLayerImage(const PsdLayerInfo& layer) const
{
    // レイヤーの画像データをファイルから抽出
    // Channel Data, Mask Data, Adjustment Layersを考慮
    // 合成済みの画像を返す
}

/// ArtifactStudioレイヤーに変換
std::vector<std::shared_ptr<ArtifactAbstractLayer>> 
PsdDocument::convertToArtifactLayers() const
{
    std::vector<std::shared_ptr<ArtifactAbstractLayer>> layers;
    auto layerImages = extractLayerImages();
    
    for (size_t i = 0; i < layers_.size(); ++i) {
        auto artifactLayer = createArtifactLayerFromPsdLayer(
            layers_[i], layerImages[i]);
        layers.push_back(artifactLayer);
    }
    
    return layers;
}
```

#### **新しいAPI**
```cpp
// ImageImporter.ixx に追加
namespace ArtifactCore {

class PsdImportOptions {
public:
    bool importAsFlattened = false;  // フラット化して1レイヤーでインポート
    bool importVisibleOnly = true;   // 可視レイヤーのみ
    bool preserveLayerOrder = true;  // レイヤー順を維持
};

class ImageImporter {
public:
    // 既存
    bool open(const QString& filePath);
    RawImage readImage();
    
    // 新規
    bool openAsPsd(const QString& filePath, const PsdImportOptions& options);
    std::vector<std::shared_ptr<ArtifactAbstractLayer>> readPsdLayers();
};

} // namespace ArtifactCore
```

#### **完了基準**
- [ ] 各レイヤーの画像を個別に抽出
- [ ] レイヤー情報をArtifactStudioレイヤーに変換
- [ ] ブレンドモードを適切にマッピング
- [ ] 不透明度を適切に適用
- [ ] レイヤーの可視性を維持
- [ ] レイヤー順序を維持

---

### **3. 一般的なオーディオ形式サポート (P0)**

#### **背景**
FFmpegは既に導入されており、追加のコーデックは数行の設定で対応可能。

#### **実装方法**
```cpp
// FFmpegEncoder.cppm に追加
// オーディオフォーマットの自動判定
QString determineAudioContainer(const QString& filePath)
{
    QFileInfo info(filePath);
    QString suffix = info.suffix().toLower();
    
    if (suffix == "m4a" || suffix == "aac") {
        return "mp4"; // or "adts" for raw AAC
    } else if (suffix == "flac") {
        return "flac";
    } else if (suffix == "ogg" || suffix == "oga") {
        return "ogg";
    }
    return "wav"; // default
}

QString determineAudioCodec(const QString& container)
{
    if (container == "mp4" || container == "adts") {
        return "aac";
    } else if (container == "flac") {
        return "flac";
    } else if (container == "ogg" || container == "oga") {
        return "libvorbis"; // or "opus" if available
    }
    return "pcm_s16le"; // default for WAV
}
```

#### **新しいAPI**
```cpp
// AudioEncoderSettings.ixx に追加
class AudioEncoderSettings {
public:
    QString container;  // "mp4", "wav", "flac", "ogg", "m4a"
    QString codec;      // "aac", "pcm_s16le", "flac", "libvorbis"
    int bitrate = 192000; // 192kbps
    int sampleRate = 48000; // 48kHz
    int channels = 2; // Stereo
};
```

#### **完了基準**
- [ ] AACエンコード/デコード
- [ ] FLACエンコード/デコード
- [ ] OGG/Vorbisエンコード/デコード
- [ ] M4Aエンコード/デコード
- [ ] 自動フォーマット判定

---

### **4. Lottieエクスポート (P1)**

#### **背景**
LottieはAdobe After EffectsのアニメーションをWebにエクスポートするためのJSON形式。Webサイトでのベクターアニメーションのデファクトスタンダード。

**現状:**
- ❌ 完全未実装
- ✅ SVGは完全サポート (基盤あり)
- ✅ JSONシリアライゼーションは完全 (基盤あり)

#### **実装方法 (フェーズ別)**

**Phase 1: SVG + CSS keyframes (1-2日)**
```cpp
// LottieExporter.cppm (Phase 1)
class LottieExporter {
public:
    QString exportAsSvgAnimation(const ArtifactComposition& composition)
    {
        // SVG + CSS keyframes 方式
        // 各レイヤーをSVG要素に
        // アニメーションをCSS keyframesに
        return svgContent;
    }
};
```

**Phase 2: Lottie Schema部分互換 (1-2週間)**
```cpp
// LottieExporter.cppm (Phase 2)
class LottieExporter {
public:
    QJsonObject exportAsLottie(const ArtifactComposition& composition)
    {
        QJsonObject lottieJson;
        
        // Lottie Schema v5.x 形式
        lottieJson["v"] = "5.12.2";
        lottieJson["fr"] = composition.frameRate();
        lottieJson["ip"] = 0;
        lottieJson["op"] = composition.durationFrames();
        lottieJson["w"] = composition.width();
        lottieJson["h"] = composition.height();
        
        // Layers
        QJsonArray layers;
        for (const auto& layer : composition.layers()) {
            QJsonObject layerJson = convertLayerToLottie(layer);
            layers.append(layerJson);
        }
        lottieJson["layers"] = layers;
        
        // Marks (optional)
        if (!composition.markers().empty()) {
            lottieJson["markers"] = convertMarkersToLottie(composition.markers());
        }
        
        // Metadata
        lottieJson["nm"] = composition.name();
        
        return lottieJson;
    }
    
private:
    QJsonObject convertLayerToLottie(const ArtifactAbstractLayerPtr& layer)
    {
        QJsonObject layerJson;
        layerJson["ty"] = convertLayerType(layer->type());
        layerJson["nm"] = layer->name();
        layerJson["ip"] = layer->inFrame();
        layerJson["op"] = layer->outFrame();
        layerJson["st"] = layer->startTime();
        
        // Transform
        layerJson["ks"] = convertTransformToLottie(layer->transform());
        
        // Shape Layer
        if (auto shapeLayer = std::dynamic_pointer_cast<ArtifactShapeLayer>(layer)) {
            layerJson["shapes"] = convertShapeToLottie(shapeLayer);
        }
        
        return layerJson;
    }
};
```

**Phase 3: Lottie完全互換 (1-2ヶ月)**
- マスク
- トラックマット
- ブレンドモード
- エフェクト
- テキストアニメーター

#### **Lottie Schemaマッピング**

| ArtifactStudio | Lottie | 状況 |
|---|---|---|
| Composition | `v`, `fr`, `ip`, `op`, `w`, `h` | ✅ Phase 1 |
| Layer (Position) | `ks.o` | ✅ Phase 1 |
| Layer (Scale) | `ks.s` | ✅ Phase 1 |
| Layer (Rotation) | `ks.r` | ✅ Phase 1 |
| Layer (Opacity) | `ks.o.a` | ✅ Phase 1 |
| Shape (Rect) | `ty: 'rc'` | ✅ Phase 1 |
| Shape (Ellipse) | `ty: 'el'` | ✅ Phase 1 |
| Shape (Path) | `ty: 'sh'` | ✅ Phase 1 |
| Shape (Group) | `ty: 'gr'` | ✅ Phase 1 |
| Mask | `masksProperties` | ❌ Phase 3 |
| Track Matte | `tt` | ❌ Phase 3 |
| Blend Mode | `bm` | ❌ Phase 3 |
| Effects | `ef` | ❌ Phase 3 |
| Text Animator | `t` | ❌ Phase 3 |

#### **完了基準**
**Phase 1:**
- [ ] SVG + CSS keyframesエクスポート
- [ ] 基本的な形状の変換
- [ ] 基本的なアニメーションの変換

**Phase 2:**
- [ ] Lottie Schema v5.x互換
- [ ] レイヤートランスフォームの変換
- [ ] 形状レイヤーの変換
- [ ] マーカーの変換

**Phase 3:**
- [ ] マスクの変換
- [ ] トラックマットの変換
- [ ] ブレンドモードの変換
- [ ] エフェクトの変換
- [ ] テキストアニメーションの変換

---

### **5. AEPインポート (P1)**

#### **背景**
After Effectsのプロジェクトファイル (.aep) はバイナリーフォーマット。直接の読み込みは困難なため、中間形式を経由するのが現実的。

#### **実装方法**

**Phase 1: Lottie経由 (推奨)**
```cpp
// AepImporter.cppm
class AepImporter {
public:
    bool importFromLottie(const QString& lottieFilePath, ArtifactComposition& composition)
    {
        // Lottie JSONを読み込み
        QJsonObject lottieJson = loadJsonFile(lottieFilePath);
        
        // LottieをArtifactStudioコンポジションに変換
        return convertLottieToComposition(lottieJson, composition);
    }
    
    bool importFromAep(const QString& aepFilePath, ArtifactComposition& composition)
    {
        // Phase 1: 外部ツール (aep2lottie) を使用
        // Phase 2: 独自のAEPパーサーを実装
        // Phase 3: 完全なAEPパーサー
        
        QString lottiePath = convertAepToLottie(aepFilePath);
        if (!lottiePath.isEmpty()) {
            return importFromLottie(lottiePath, composition);
        }
        return false;
    }
};
```

**Phase 2: 独自AEPパーサー**
- AEPファイルのバイナリフォーマットを解析
- レイヤー構造の抽出
- トランスフォームキーの抽出
- エフェクトの抽出

**Phase 3: 完全AEPパーサー**
- 全てのAE機能に対応
- 表現の完全な再現

#### **完了基準**
**Phase 1:**
- [ ] Lottie経由のAEPインポート
- [ ] 外部ツールとの連携

**Phase 2:**
- [ ] AEPファイルの基本的なパース
- [ ] レイヤー構造の抽出
- [ ] トランスフォームの変換

**Phase 3:**
- [ ] エフェクトの変換
- [ ] マスクの変換
- [ ] トラックマットの変換
- [ ] 表現の変換

---

### **6. MMD形式サポート (P1)**

#### **背景**
MikuMikuDance (MMD) は日本で非常に人気のある3Dアニメーションソフト。PMD/PMX (モデル), VMD (モーション) 形式が標準。

**要望:** "一番欲しがられている。これだけでユーザーが3倍になる"

#### **実装方法**

**Phase 1: 基本的なジオメトリ読み込み**
```cpp
// MMDImporter.cppm
class MMDImporter {
public:
    bool importPmd(const QString& filePath, Artifact3DModel& model)
    {
        // PMDファイルの読み込み
        // 頂点データの抽出
        // 面データの抽出
        // テクスチャの読み込み
        // マテリアルの設定
        
        return true;
    }
    
    bool importVmd(const QString& filePath, Artifact3DAnimation& animation)
    {
        // VMDファイルの読み込み
        // ボーンアニメーションの抽出
        // モーフアニメーションの抽出
        // カメラアニメーションの抽出
        
        return true;
    }
};
```

**Phase 2: 高度な機能**
- 物理演算 (PMX拡張)
- 表情 (ファシアルモーフ)
- IK (逆運動学)
- physics (物理演算)

#### **完了基準**
**Phase 1:**
- [ ] PMDファイルのジオメトリ読み込み
- [ ] VMDファイルのアニメーション読み込み
- [ ] 基本的な3Dモデルの表示

**Phase 2:**
- [ ] PMXファイルのサポート
- [ ] 物理演算のサポート
- [ ] 表情のサポート

---

### **7. FBX/USD完全対応 (P2)**

#### **背景**
FBXは3Dデータ交換の業界標準。USDはPixarが開発した次世代3Dフォーマット。

**現状:**
- ✅ GLB/GLTF: 完全対応
- ⚠️ USD: DiligentEngineにサンプルあり (ビューア)
- ❌ FBX: 未実装

#### **実装方法**

**USD完全対応**
```cpp
// USDImporter.cppm
class USDImporter {
public:
    bool importUsd(const QString& filePath, Artifact3DScene& scene)
    {
        // USDファイルの読み込み
        // 3Dモデルの抽出
        // マテリアルの抽出
        // アニメーションの抽出
        // ライトの抽出
        // カメラの抽出
        
        return true;
    }
    
    bool exportUsd(const QString& filePath, const Artifact3DScene& scene)
    {
        // 3DシーンをUSD形式でエクスポート
        
        return true;
    }
};
```

**FBX完全対応**
```cpp
// FBXImporter.cppm
class FBXImporter {
public:
    bool importFbx(const QString& filePath, Artifact3DScene& scene)
    {
        // FBX SDK or Assimp を使用
        // 3Dモデルの抽出
        // アニメーションの抽出
        
        return true;
    }
};
```

#### **完了基準**
- [ ] USDファイルのインポート
- [ ] USDファイルのエクスポート
- [ ] FBXファイルのインポート
- [ ] FBXファイルのエクスポート
- [ ] マテリアルの変換
- [ ] アニメーションの変換

---

## 🏗️ **実装計画 (3フェーズ)**

---

### **📌 Phase 1: 即効性 (1-2日) - +25%ユーザー**
**目標: すぐに使えて、大きな効果**

#### **タスク一覧**

| No | タスク | 予定時間 | 優先度 | 担当ファイル | 効果 |
|---|---|---|---|---|---|
| 1-1 | GIFアニメーションエクスポート | 1時間 | ⭐⭐⭐⭐⭐ | `FFmpegEncoder.cppm` | +10%ユーザー |
| 1-2 | GIFアニメーションインポート | 1時間 | ⭐⭐⭐⭐⭐ | `FFmpegVideoDecoder.cppm` | +10%ユーザー |
| 1-3 | AACオーディオサポート | 30分 | ⭐⭐⭐⭐ | `FFmpegEncoder.cppm` | +5%ユーザー |
| 1-4 | FLACオーディオサポート | 30分 | ⭐⭐⭐⭐ | `FFmpegEncoder.cppm` | +5%ユーザー |
| 1-5 | OGGオーディオサポート | 30分 | ⭐⭐⭐⭐ | `FFmpegEncoder.cppm` | +3%ユーザー |
| 1-6 | M4Aオーディオサポート | 30分 | ⭐⭐⭐⭐ | `FFmpegEncoder.cppm` | +5%ユーザー |
| 1-7 | 日本語UIラベル追加 | 1時間 | ⭐⭐⭐ | 各種UIファイル | - |
| 1-8 | 単体テスト | 1時間 | ⭐⭐⭐⭐⭐ | `tests/` | - |

#### **完了基準**
- [ ] GIFアニメーションの完全サポート
- [ ] 一般的なオーディオ形式の完全サポート
- [ ] 全ての機能が正常に動作
- [ ] UIに新しいフォーマットが表示
- [ ] 既存機能との互換性が100%維持

---

### **📌 Phase 2: 生産性向上 (1-2週間) - +50%ユーザー**
**目標: 作業効率が2倍になる**

#### **タスク一覧**

| No | タスク | 予定時間 | 優先度 | 担当ファイル | 効果 |
|---|---|---|---|---|---|
| 2-1 | PSDレイヤー画像抽出 | 2日 | ⭐⭐⭐⭐⭐ | `PSDDocument.cppm` | +20%ユーザー |
| 2-2 | PSD→ArtifactStudioレイヤー変換 | 2日 | ⭐⭐⭐⭐⭐ | `PSDDocument.cppm` | +10%ユーザー |
| 2-3 | Lottie Phase 1: SVG + CSS keyframes | 2日 | ⭐⭐⭐⭐ | 新規 `LottieExporter.cppm` | +20%ユーザー |
| 2-4 | Lottie Phase 2: 基本Schema | 5日 | ⭐⭐⭐⭐ | `LottieExporter.cppm` | +20%ユーザー |
| 2-5 | AEP Phase 1: Lottie経由 | 2日 | ⭐⭐⭐⭐⭐ | 新規 `AepImporter.cppm` | +15%ユーザー |
| 2-6 | UI統合 (新しいフォーマット) | 1日 | ⭐⭐⭐⭐ | 各種UIファイル | - |
| 2-7 | 包括的なテスト | 2日 | ⭐⭐⭐⭐⭐ | `tests/` | - |
| 2-8 | ドキュメント更新 | 1日 | ⭐⭐ | `docs/` | - |

#### **完了基準**
- [ ] PSDレイヤー構造の完全インポート
- [ ] Lottieエクスポートの基本機能
- [ ] AEPインポートの基本機能
- [ ] 全ての新機能がUIで利用可能
- [ ] 既存機能との互換性が100%維持

---

### **📌 Phase 3: 完全DCC統合 (1ヶ月) - +75%ユーザー**
**目標: DCCツールとの完全な統合**

#### **タスク一覧**

| No | タスク | 予定時間 | 優先度 | 担当ファイル | 効果 |
|---|---|---|---|---|---|
| 3-1 | Lottie Phase 3: 完全Schema | 2週間 | ⭐⭐⭐⭐ | `LottieExporter.cppm` | +20%ユーザー |
| 3-2 | AEP Phase 2: 独自パーサー | 2週間 | ⭐⭐⭐⭐⭐ | `AepImporter.cppm` | +20%ユーザー |
| 3-3 | MMD Phase 1: PMD/PMX | 1週間 | ⭐⭐⭐⭐ | 新規 `MMDImporter.cppm` | +15%ユーザー |
| 3-4 | MMD Phase 2: VMD | 1週間 | ⭐⭐⭐ | `MMDImporter.cppm` | +15%ユーザー |
| 3-5 | FBXインポート/エクスポート | 2週間 | ⭐⭐⭐⭐ | 新規 `FBXImporter/Exporter.cppm` | +15%ユーザー |
| 3-6 | USD完全対応 | 2週間 | ⭐⭐⭐⭐ | 新規 `USDImporter/Exporter.cppm` | +10%ユーザー |
| 3-7 | 高度なUI統合 | 3日 | ⭐⭐⭐ | 各種UIファイル | - |
| 3-8 | パフォーマンス最適化 | 3日 | ⭐⭐⭐⭐ | 各種実装ファイル | - |
| 3-9 | 包括的なテスト | 2日 | ⭐⭐⭐⭐⭐ | `tests/` | - |

#### **完了基準**
- [ ] 全主要DCCツールとの完全統合
- [ ] 全ての機能が正常に動作
- [ ] 既存機能との互換性が100%維持
- [ ] 60fps以上のパフォーマンス
- [ ] 利用者向けドキュメント完備

---

## 📅 **スケジュール**

|フェーズ|期間|タスク数|完了時の状態|累積ユーザー増|
|---|---|---|---|---|
| **Phase 1** | 1-2日 | 8 | 基本的なフォーマット完全サポート | +25% |
| **Phase 2** | 1-2週間 | 8 | 生産性向上、主要DCCツール連携 | +50% |
| **Phase 3** | 1ヶ月 | 9 | 完全DCCツール統合 | +75% |
| **合計** | **1-1.5ヶ月** | **25** | **全DCCツール完全統合** | **+100%** |

---

## 📁 **ファイル変更一覧**

### **Phase 1 (1-2日)**
```
M ArtifactCore/src/Image/FFmpegEncoder.cppm
- GIFアニメーションエクスポート機能を追加
- 一般的なオーディオフォーマット(AAC, FLAC, OGG, M4A)を追加

M ArtifactCore/src/Codec/FFmpegVideoDecoder.cppm
- GIFアニメーションインポート機能を追加

M Artifact/src/Widgets/...
- 新しいフォーマットをUIに追加
```

### **Phase 2 (1-2週間)**
```
M ArtifactCore/src/Image/PSDDocument.cppm
- extractLayerImages()を追加
- convertToArtifactLayers()を追加

A ArtifactCore/src/IO/LottieExporter.cppm
- Lottie Phase 1 & 2を実装

A ArtifactCore/src/IO/AepImporter.cppm
- AEP Phase 1を実装

A ArtifactCore/include/IO/LottieExporter.ixx
A ArtifactCore/include/IO/AepImporter.ixx
```

### **Phase 3 (1ヶ月)**
```
A ArtifactCore/src/IO/LottieExporter.cppm (拡張)
- Lottie Phase 3を実装

A ArtifactCore/src/IO/AepImporter.cppm (拡張)
- AEP Phase 2を実装

A ArtifactCore/src/IO/MMDImporter.cppm
- MMD Phase 1 & 2を実装

A ArtifactCore/src/IO/FBXImporter.cppm
A ArtifactCore/src/IO/FBXExporter.cppm
- FBXインポート/エクスポートを実装

A ArtifactCore/src/IO/USDImporter.cppm
A ArtifactCore/src/IO/USDExporter.cppm
- USD完全対応を実装
```

---

## 🎯 **完了基準 (Acceptance Criteria)**

### **必須 (Must Have) - 全フェーズ**
- [ ] 全ての新しいフォーマットが正常にインポートできる
- [ ] 全ての新しいフォーマットが正常にエクスポートできる
- [ ] 全ての新しいフォーマットがTransitionManagerに登録
- [ ] 日本語表示名が完備
- [ ] UIで全ての新しいフォーマットが選択可能
- [ ] 既存機能との互換性が100%維持
- [ ] 単体テストが全て通過

### **望ましい (Should Have)**
- [ ] 全ての新しいフォーマットのパラメータがUIで調整可能
- [ ] 60fps以上のパフォーマンス
- [ ] GPU加速に対応
- [ ] 利用者向けドキュメント
- [ ] チュートリアル

### **将来的 (Could Have)**
- [ ] 3Dシーンの高度な機能
- [ ] アニメーションの高度な機能
- [ ] カスタムフォーマットのサポート
- [ ] プラグインシステム

---

## 📊 **依存関係**

### **依存するコンポーネント**
```
ArtifactCore:
├── IO/Image/
│   ├── ImageImporter/Exporter
│   ├── PSDDocument
│   └── FFmpegEncoder
├── IO/
│   └── (新規) LottieExporter, AepImporter, MMDImporter, FBXImporter, USDImporter
├── Codec/
│   └── FFmpegVideoDecoder, FFmpegAudioEncoder
├── Shape/
│   └── ShapeLayer (SVG基盤)
└── Serialization/
    └── JSON基盤

Artifact:
├── Project/
│   └── ArtifactProjectExporter/Importer
├── Service/
│   └── ArtifactProjectService
└── Widgets/
    └── 各種UI

第三者ライブラリ:
├── OpenImageIO (OIIO) - 画像フォーマット
├── FFmpeg - 動画/音声フォーマット
├── stb_image - フォールバック画像ローダー
├── (将来的) Assimp - 3Dフォーマット
└── (将来的) USD SDK - USDフォーマット
```

### **依存されるコンポーネント**
```
- UI: インポート/エクスポートダイアログ
- レンダリング: 画像/動画の表示
- プロジェクト管理: アセットの管理
- タイムライン: フレーム管理
```

---

## 🚨 **リスクと対策**

| リスク | 可能性 | 影響 | 対策 | 補足 |
|---|---|---|---|---|
| **FFmpegのバージョン互換性** | 中 | 高 | FFmpeg 5.x+をターゲット | 現在のバージョンを確認 |
| **PSDフォーマットの複雑さ** | 中 | 高 | 段階的な実装、テストを徹底 | 最新のPSD仕様を調査 |
| **Lottie Schemaの複雑さ** | 高 | 中 | Phase分け、公式ドキュメントを参照 | lottie-webを参考 |
| **AEPフォーマットの閉鎖性** | 高 | 高 | Lottie経由で回避 | After Effects公式APIも参照 |
| **3Dフォーマットの互換性** | 中 | 中 | Assimpの使用を検討 | FBX SDKも選択肢 |
| **パフォーマンス低下** | 中 | 中 | GPU加速、キャッシュ | ベンチマークを実施 |
| **既存機能との競合** | 低 | 高 | 徹底的な互換性テスト | 既存コードを破壊しない |
| **開発工数の超過** | 中 | 中 |優先度を明確化、Phase分け | 定期的に進捗をレビュー |

---

## ⚡ **パフォーマンス考慮**

### **最適化ポイント**

#### **画像処理**
1. **OIIOのキャッシュ**: 同一画像の再読み込みを避ける
2. **PSDレイヤーのレイジーローディング**: 必要なレイヤーのみ読み込む
3. **画像のダウンスケーリング**: プレビュー用に小さいサイズを使用
4. **メモリ管理**: 大きな画像は早めに解放

#### **動画処理**
1. **ハードウェアエンコード**: NVENC, AMF, QSV, VAAPIを活用
2. **並列処理**: マルチスレッドでエンコード
3. **バッファリング**: フレームをバッファリングして安定化
4. **品質設定**: ユーザーが品質/速度を選択可能

#### **3D処理**
1. **ジオメトリの最適化**: 不要な頂点を削除
2. **LOD (Level of Detail)**: 距離に応じた詳細度
3. **インスタンス化**: 同じジオメトリは再利用
4. **GPU加速**: DiligentEngineを活用

---

## 🎯 **成功指標 (KPI)**

| 指標 | 目標値 | 計測方法 | 期限 |
|---|---|---|---|
| **サポートフォーマット数** | 30+ | Inventory count | Phase 1完了 |
| **DCCツール連携スコア** | 80/100 | 平均スコア | Phase 2完了 |
| **ユーザー満足度** | +25% |ユーザー調査 | Phase 1完了 |
| **ユーザー獲得数** | +50% | ダウンロード数 | Phase 2完了 |
| **パフォーマンス** | 60fps+ | ベンチマーク | 全Phase完了 |
| **テストカバレッジ** | 100% | 単体テスト | 全Phase完了 |
| **ドキュメント完備率** | 100% | ドキュメントチェック | 全Phase完了 |

---

## 💡 **将来的な拡張アイデア**

### **次世代機能**
1. **AI支援インポート** - AIで自動的にフォーマットを判定
2. **バッチ処理** - 複数のファイルを一括で処理
3. **フォーマット変換** - 異なるフォーマット間の変換
4. **クラウドストレージ統合** - Google Drive, Dropbox等との直接連携
5. **リアルタイムプレビュー** - インポート/エクスポートのリアルタイムプレビュー

### **高度な機能**
1. **3Dアニメーションの完全サポート** - 全ての3Dアニメーション機能
2. **物理シミュレーション** - 物理演算を含むインポート/エクスポート
3. **スクリプト自動化** - インポート/エクスポートをスクリプトで制御
4. **カスタムフォーマット** - ユーザー定義のフォーマットをサポート
5. **プラグインシステム** - サードパーティ製のインポーター/エクスポーター

---

## 📝 **変更履歴**

| 日付 | 版 | 内容 | 作成者 |
|---|---|---|---|
| 2026-06-28 | 1.0 | 初版作成 - ソースコード分析に基づく最新状況 | Mistral Vibe |
| 2026-06-28 | 1.1 | **深掘り分析追加** - ソースコード詳細調査により、GIF/AAC/FLAC/OGG/Opus/APNG/WebPが既に実装済みであることを確認。新たにHEIF, AVIF, PDF, EPS, SRT, ASS, TTF/OTF, LUT, KEP, M4A, AI, INDDを特定 | Mistral Vibe |

---

## 📌 **次ステップ**

**このマイルストーンを承認**すると:

1. **Phase 1を開始** - HEIF, AVIF, M4Aを1-2日で実装
2. **Phase 2を開始** - PSDレイヤー、Lottie、AEPを1-2週間で実装
3. **Phase 3を開始** - 全DCCツール統合を1ヶ月で実装

**🎯 直ちに実装可能な低労力高効果タスク:**
- UI列挙型 (`ImageFormat`, `VideoCodec`, `AudioCodec`) への既存サポートフォーマット追加
- GIF, WebP, APNG, AAC, FLAC, OGGをUIで選択可能にする
- TransitionManagerへの新規トラジション登録

**承認しますか？** 🚀

---

**参照文書:**
- [DESIRED_IMPORT_FORMATS_2026-04-19.md](../DESIRED_IMPORT_FORMATS_2026-04-19.md)
- [REPORT_JS_ANIMATION_EXPORT_2026-06-16.md](../../analysis/REPORT_JS_ANIMATION_EXPORT_2026-06-16.md)
- [REPORT_LATE_STAGE_AND_DCC_GAP_2026-06-16.md](../../analysis/REPORT_LATE_STAGE_AND_DCC_GAP_2026-06-16.md)
