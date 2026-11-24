# マイナー形式のインポート／エクスポート監査

**最終更新:** 2026-11-20

対象: 画像・ベクター・文書・字幕・LUT・フォント・3D のうち、主要経路（PNG/JPEG/EXR/MP4/WAV/OBJ/FBX/glTF）以外の形式。
方法: 現行ソースの静的読解のみ。ビルド、テスト、実ファイル読み込み、書き出しの runtime 確認は未実施。

---

## 結論

「新しいデコーダを増やす」より先に、**既に Core が読める／書ける形式を UI と経路から到達可能にする**方が費用対効果が高い。今回の監査では、マイナー形式の不足は次の 3 系統に集中していた。

1. **取り込みの関門が実経路に無い。** `FileTypeDetector`（分類）と `AssetImporter::isSupported`（許可）が別管理で、しかも後者はアプリの取り込み経路から呼ばれていない。主要経路は拡張子を見ずに任意ファイルをコピーして Footage 化する。
2. **画像の読み込みが QImageReader と OIIO の二重経路。** 単体画像は OIIO で EXR/DPX/PSD などを読めるが、連番・レイヤー作成・サイズ取得・サムネイルの一部は QImageReader 固定で、同じ形式が経路によって読めたり読めなかったりする。
3. **出力形式の「クレーム」と writer の分岐が一致しない。** Render Output の選択肢は実際の writer より狭く、逆に未知の形式名は PNG へ無言フォールバックする。さらに 16/10-bit を選んでも、レンダーキューは `QImage`（8-bit）経由のため実精度は 8-bit のまま。

Core 側には既に DPX の Cineon メタデータ、OIIO 汎用 writer、PSD レイヤー取り込み、SRT/WebVTT、LUT、OTIO、Lottie 書き出しがある。したがって主な作業は**新規 codec の追加ではなく、能力表の一元化と配線**である。

---

## 1. 取り込み側の所見

### 1.1 分類と許可が別リストで、許可ゲートが実経路に無い ★1

- 分類: `ArtifactCore/src/File/FileTypeDetector.cpp:26-93` の `suffix ==` 連鎖。画像 31-42、動画 45-50、音声 52-57、フォント 59-62、テキスト 64-70、文書 72-76、アーカイブ 78-81、3D 83-90。
- 許可: `ArtifactCore/src/Asset/AssetImporter.cpp:113-134` の独立した `QStringList`。
- `AssetImporter::importFile()` の呼び出しはテストのみ（`tests/ArtifactCore/AssetImporterContractTest.cpp`、`Artifact/src/Test/ArtifactTestAIToolBridge.cpp:461-466`）。アプリの取り込みは `ArtifactProjectService::importAssetsFromPathsAsync`（`Artifact/src/Service/ArtifactProjectService.cpp:2729-2840`）で、`isSupported` を通らず、存在するファイルを無条件にコピーして `addAssetFromPath` する。
- `ArtifactProject::Impl::addAssetFromPath` は `registerProjectAsset` の戻り値をそのまま `assetId` に入れる（`Artifact/src/Project/ArtifactProject.cpp:371-373`）。`registerProjectAsset` は `AssetType::Unknown` のとき未登録のまま返す（`Artifact/src/Project/ArtifactProjectAssetRegistrationSupport.cpp:14-25`）。結果として `.zip` や未知拡張子は「Asset ID の無い FootageItem」になる。

含意:
- `.srt` / `.csv` / `.ttf` / `.zip` なども Project へは入る（Data 扱いまたは未登録）。「読めない形式を拒否する」挙動は現状存在しない。
- `isSupported` を広げても実経路の挙動は変わらない。逆に、テストだけが許可リストを固定している（`blend` を閉じている: `ArtifactTestAIToolBridge.cpp:466`）。

### 1.2 連番は QImageReader、単体は OIIO ★2

- 単体画像: `Artifact/src/Layer/ArtifactImageLayer.cpp:597` 以降の `loadImageViaOIIO`。EXR/DPX/HDR/PSD などを OIIO plugin 経由で読む。
- 連番: `ArtifactCore/src/Media/ImageSequenceSource.cpp:149-156` の `readSequenceFrame()` が `QImageReader`。`isSupportedImageFile()` も `QImageReader::supportedImageFormats()`（158-175）で、`open` / `openFramePaths` / `tryFrameAt`（650-）の前提になっている。
- 一方、連番の自動グループ化は拡張子分類だけを見る。`ArtifactProjectService.cpp:279-283` の `isImageImportCandidate` は `detectByExtension == Image` で、EXR/DPX/PSD/KTX2/AVIF も対象になる（285-330）。

含意:
- EXR/DPX/HDR/PSD/KTX2 などを複数枚落とすと「画像シーケンス」として登録されるが、フレームデコードは QImageReader なので失敗し得る。失敗時は `ArtifactImageLayer.cpp:1319-1327` でプレースホルダへ落ちる。
- 解像度ガードも QImageReader 依存（`ArtifactProjectService.cpp:346-359`）。マイナー形式では「Image size unavailable」警告になり、互換性チェックが効かない。

### 1.3 レイヤー作成 UI が QImageReader でマイナー画像を拒否 ★3

- Composition Editor の「New Image Layer...」はフィルタが PNG/JPEG/BMP/GIF/WebP/TIFF のみで、さらに `QImageReader::canRead()` で検証する（`Artifact/src/Widgets/Render/ArtifactCompositionEditor.cpp:4630-4662`）。EXR/DPX/HDR/PSD/KTX2 はここでは選べず、選んでも拒否される。
- タイムラインへのドロップは `FileTypeDetector::detectByExtension` だけで受理する（`ArtifactLayerPanelWidget.cpp:1073-1090`、`ArtifactTimelineTrackPainterView.cpp:374-394`）。同じ EXR が「ドロップでは入る / メニューでは入らない」状態。

### 1.4 UI フィルタ文字列が 5 箇所以上に重複し、翻訳キーは ja/en のみ ★4

同じ拡張子リストがインライン文字列で重複している。

- `Artifact/src/Widgets/Dialog/ArtifactImportAssetsDialog.cpp:677`
- `Artifact/src/Widgets/Timeline/ArtifactLayerPanelWidget.cpp:5240`（画像）、`5277`（動画）
- `Artifact/src/Widgets/Timeline/ArtifactTimelineTrackPainterView.cpp:11835-11846`（`timeline.audio_files` / `image_files` / `video_files` を使用）
- `Artifact/src/Widgets/Render/ArtifactCompositionEditor.cpp:2583`（メディア）
- `Artifact/src/Project/ArtifactProject.cpp:1655`（メディア）
- `Artifact/src/Widgets/Render/ArtifactRenderQueueManagerWidget.cpp:1478`（出力パス）
- `Artifact/src/Widgets/PropertyEditor/ArtifactPropertyEditorShared.cpp:433`、`Artifact/src/Widgets/Menu/ArtifactLayerMenu.cpp:3260`（3D）

`timeline.image_files` 等は `Artifact/translations/ja.json` / `en.json` にしか無く、`zh` / `zh-TW` / `ko` / `fr` / `de` / `es` / `pt` / `ru` / `ar` では英語フォールバックになる。上記のうち翻訳キーを使っているのは Timeline だけで、他は日本語・英語以前に翻訳対象外の直書きである。

### 1.5 3D マイナー形式のクレームが実装と一致しない ★5

`MeshImporter::loadFromFile` の分岐は `ArtifactCore/src/Geometry/MeshImporter.cpp:3695-3784`。

- 実装あり: `fbx` / `obj` / `gltf` / `glb` / `pmd` / `stl` / `ply` / `las` / `usda`。
- 条件付き: `usd`（ASCII は USDA、それ以外は `ARTIFACT_WITH_OPENUSD`）、`usdc` / `usdz`（同マクロ）。
- 未実装: `dae` / `abc` / `blend` / `pmx`。
- 分類だけ存在: `pmx`（`FileTypeDetector.cpp:87`）、`blend` / `dae` / `abc`。
- 許可リストに無い: `pmx` / `dae` / `abc` / `blend`（`AssetImporter.cpp:128-129`）。
- ダイアログが提示: `dae` / `abc`（`ArtifactLayerMenu.cpp:3260`、`ArtifactPropertyEditorShared.cpp:433`）。`AssetBrowser` は `abc` / `blend` のアイコンまで持つ（`ArtifactAssetBrowser.cpp:1449,1455`）。

`dae` / `abc` は「選べるが必ず失敗する」、`pmx` は「分類されるが許可されず、UI にも出ない」という逆向きの不一致。

### 1.6 ベクターの「編集可能」フラグが実 backend より広い ★6

`ArtifactCore/include/Asset/VectorImport.ixx:132-149` は `editableReady = previewReady && sourceKind != Unknown` としており、`ai` / `pdf` / `eps` / `afdesign` / `afphoto` / `afpub` も「編集可能試行」になる。実際に構造を読むのは SVG の XML 解析（151 以降）だけで、Illustrator/PDF/EPS/Affinity の編集可能レイヤー変換は確認できない。テストも PDF を `editableReady=true` として固定している（`tests/ArtifactCore/AssetImporterContractTest.cpp:331-365`）。

---

## 2. 書き出し側の所見

### 2.1 未知の形式名が PNG に無言フォールバックする ★7

- Render Output のコンテナ一覧: `Artifact/src/Widgets/Dialog/ArtifactRenderOutputSettingDialog.cpp:1550-1554`（MP4 / MOV / AVI / WebM / MKV / PNG / JPEG / TIFF / BMP / EXR / HTML）。コーデック一覧は 1558-1561。
- プリセットには GIF / APNG / Animated WebP（`ArtifactRenderQueuePresets.cpp:163-194`）、BMP / SVG（246-268）もある。プリセット経由で一覧に無いコンテナが `addItem` される（`ArtifactRenderOutputSettingDialog.cpp:1306-1311`）。
- 連番拡張子の決定は `ArtifactRenderQueueEncoder.cpp:436-450` の `sequenceExtension()`。`exr` / `tiff` / `jpg` / `bmp` / `png` / `svg` 以外は **既定で `png`**。
- `isImageSequenceContainer()`（422-434）と `defaultOutputPathForJob()`（406-419）も同じ固定集合。
- AI API は任意の `format` を受け取り、検証せず job に設定する（`Artifact/include/AI/WorkspaceAutomation.ixx:7970-7973`）。`getSupportedExportFormats()` は 7 形式だけ（8094-8104）。
- File メニューの単発書き出しは、`png` / `jpg` / `exr` 以外の拡張子を **PNG に書き換える**（`ArtifactFileMenu.cpp:75-87`、`924-940`）。ワークエリアも `mp4` / `jpg` / `exr` 以外は PNG（988-996）。ダイアログは「All Files」を出しているため、`.tif` や `.webp` を選ぶと PNG ができる。

含意: `format="DPX"` の job は「DPX」と表示されたまま `.png` 連番を書き得る。失敗ではなく**別形式の成功**になるため、診断も付かない。

### 2.2 Core の writer 能力は既にある（新規 codec 不要） ★8

`ArtifactCore/src/IO/Image/ImageExporter.cpp` は拡張子から OIIO の `ImageOutput::create()` を選ぶ（232-250）。DPX の場合の Cineon / KodakLog メタデータも実装済み（133-140）。bit depth は `resolveWriteType()`（199-216）で EXR の 8-bit 昇格と HALF 維持を扱う。

したがって DPX / HDR / TGA / WebP などの書き出しは、**Core の writer 追加ではなく、UI・プリセット・拡張子マッピングの接続不足**である。`docs/analysis/PROFESSIONAL_VS_AUDIENCE_GAP_2026-10-04.md` の A-9「DPX writer・UI・プリセット全て不在」は、Core writer については現状と合わない（UI / プリセット / 経路は不在で正しい）。

### 2.3 16/10-bit 出力は 8-bit 精度 ★9

- レンダーキューのフレーム出力は `QImage beauty`（`ArtifactRenderQueueService.cpp:2811-2830`）。float readback があっても `toQImage()` で 8-bit 化される（7793-7822）。
- 連番書き出しは `ImageExporter::write(qimg, ...)`（9435）。`QImage` 経路は 8-bit を float へ展開するだけ（`ImageExporter.cpp:269-296`）。
- 例外は AOV の `writeMultiChannel()`（9433-9435）と Deep EXR（9401-9409）。

含意: 16-bit PNG/TIFF、32-bit EXR、10-bit DPX/HDR を UI から選んでも、**コンテナだけが高 bit depth で中身は 8-bit 量子化**になり得る。DPX を「10-bit Log デリバリー」として案内する前に、float readback から `ImageBuf` への経路が必要。

### 2.4 単発書き出しの形式が 3 箇所で PNG/JPEG/EXR 固定

- File > 現在のフレーム: `ArtifactFileMenu.cpp:924-925`
- スクリーンショット書き出し: `ArtifactScreenshotExportDialog.cpp:210`
- Composition Editor の画像保存: `ArtifactCompositionEditor.cpp:11709-11715`
- レンダー出力パスの参照ダイアログ: `ArtifactRenderQueueManagerWidget.cpp:1478`

TIFF / WebP / TGA / DPX はここから選べない。

---

## 3. 既にあるが導線が弱いマイナー形式

| 形式 | 現状 | 根拠 | 不足 |
|---|---|---|---|
| PSD レイヤー取り込み | **実装済み**。File > Import PSD Layers でレイヤー選択 → 1 アセット + subimage 参照 | `ArtifactFileMenu.cpp:644-696`、`ArtifactImportPsdDialog`、`PsdDocument` | ドキュメントが「フラットのみ」のまま。PSD 書き出しは無い |
| SRT / WebVTT / ASS | **実装済み**（ASS は取り込みのみ）。Composition Editor の View メニューから Text レイヤーのキーフレームへ変換、選択 Text レイヤーから SRT/WebVTT 書き出し | `ArtifactCompositionEditor.cpp`（取り込み／書き出し）、`OtioAdapter::importSrt/importWebVtt/importAss/exportSrt/exportWebVtt` | File メニュー・ドラッグ＆ドロップ導線なし、UI 文字列が英語直書き、ASS 書き出しなし |
| LUT | `.cube` / `.csp` / `.3dl` 取り込み、`.cube` / `.3dl` 書き出し | `ArtifactCore/src/Color/ColorLUT.cpp:845-847`、`LUTWriter.cpp:14-63`、`ArtifactColorSciencePanel.cpp:341,1166` | `.lut` はフィルタにあるが parser の対応が不明（未確認） |
| パレット | `.gpl` 取り込み・書き出し | `ArtifactColorSwatchWidget.cpp:178,188` | 他のパレット形式は無し |
| OTIO | 取り込み・書き出し | `ArtifactFileMenu.cpp:698-720,1127-1128`、`ArtifactCore/src/NLE/OTIO.cpp:496,553` | トラック種別・トランジションの未対応警告の検証は未実施 |
| Lottie | 書き出し（JSON） | `ArtifactExportDialog.cpp:22-48`、`ArtifactCore/src/Export/Lottie/LottieExporter.cpp` | 取り込みは `importFromFile()` があるがアプリ導線なし、`.lottie` ZIP 展開なし |
| SVG | Shape レイヤーとして取り込み（QSvgRenderer でラスタライズ）、Shape のみ SVG フレーム書き出し | `ArtifactCompositionEditor.cpp:4591-4629`、`ArtifactRenderQueueService.cpp:9314-9362` | 編集可能パス変換は無し。書き出しは Shape 以外を空キャンバスにする |
| CSV | Project へ Data としてコピーされるだけ | `FileTypeDetector.cpp:68` | キーフレーム・数値タイムライン取り込みなし |
| フォント | 分類・Project 登録は可能 | `FileTypeDetector.cpp:59-62`、`AssetBrowser.cpp:1015-1021` | `.ttc` が Detector に無い、テキストレイヤー自動作成なし |
| Aseprite | アイコン定義のみ | `ArtifactAssetBrowser.cpp:1467-1468` | 読み込み・フレーム展開なし |
| Illustrator / Affinity | 分類と `VectorAssetFile` メタデータのみ | `VectorImport.ixx:79-85` | PDF/AI の実パス抽出なし。`MILESTONE_ILLUSTRATOR_FILE_IMPORT_2026-04-18.md` 参照 |

---

## 4. 対応マトリクス

凡例: ○ 実装/到達可能、△ 条件付き・限定、× 無し、— 対象外。
「判定」は `FileTypeDetector::detectByExtension`、「許可」は `AssetImporter::isSupported`、「UI」は主要な取り込みダイアログのいずれかに載っているか（詳細は §1.4）。読込・書出は実際の分岐。

### 4.1 画像

| 形式 | 判定 | 許可 | UI | 読込 | 書出 | メモ |
|---|---|---|---|---|---|---|
| png / jpg / bmp | ○ | ○ | ○ | ○ | ○ | 基準 |
| gif | ○ | ○ | ○ | ○ | △ | アニメーション書き出しのみ（`gif_animation`） |
| tga | ○ | ○ | △ | ○ | × | Composition Editor の新規画像からは不可 |
| webp | ○ | ○ | ○ | ○ | △ | Animated WebP のみ。単体連番は不可 |
| tif / tiff | ○ | ○ | ○ | ○ | ○ | 16-bit 指定は 8-bit 精度（§2.3） |
| exr | ○ | ○ | ○ | ○ | ○ | 単体は OIIO、連番は QImageReader で不可の可能性 |
| dpx | ○ | ○ | ○ | ○ | × | Core writer はあり。UI/プリセット/拡張子マップが無い |
| hdr | ○ | ○ | △ | ○ | × | Project フィルタ・新規画像に無い |
| psd / psb | ○ | ○ | ○ | ○ | × | 専用レイヤー取り込みあり。連番グループ化は危険 |
| svg | △ | ○ | ○ | ○ | △ | 拡張子は Image、`detect()` は magic で Text。Shape のみ SVG 出力 |
| ico / dds / ktx | ○ | ○ | △ | △ | × | 実デコードは OIIO plugin 次第 |
| ktx2 | ○ | ○ | × | △ | × | 許可済みだがどのフィルタにも無い |
| avif / heic / heif / jxl / jp2 / j2k | ○ | ○ | × | △ | × | 同上。OIIO plugin 次第 |
| ppm / pgm / pbm / pam / pfm | ○ | ○ | × | △ | × | 同上 |

### 4.2 3D

| 形式 | 判定 | 許可 | UI | 読込 | 書出 | メモ |
|---|---|---|---|---|---|---|
| obj / fbx / gltf / glb | ○ | ○ | ○ | ○ | × | — |
| stl / ply / las | ○ | ○ | △ | ○ | × | Asset Browser の 3D フィルタに las 以外の差あり |
| pmd | ○ | ○ | ○ | ○ | × | ジオメトリのみ |
| pmx | ○ | × | × | × | × | 分類だけ存在 |
| usd / usda | ○ | ○ | ○ | ○ | △ | `exportSourceUsd()` は呼び出し元未確認 |
| usdc / usdz | ○ | ○ | △ | △ | × | `ARTIFACT_WITH_OPENUSD` 次第 |
| dae / abc | ○ | × | ○ | × | × | ダイアログが提示するが必ず失敗 |
| blend | ○ | × | × | × | × | テストが閉じている（`ArtifactTestAIToolBridge.cpp:466`） |

### 4.3 音声・テキスト・データ

| 形式 | 判定 | 許可 | UI | 読込 | 書出 | メモ |
|---|---|---|---|---|---|---|
| wav / mp3 / flac / aac / ogg / opus / m4a / aiff / wma | ○ | ○ | ○ | ○ | △ | 書き出しは WAV / MP3 のみ |
| ac3 / dts | ○ | × | × | △ | × | FFmpeg は内容で開くため実読込は可能な見込み（未確認） |
| mxf | × | ○ | ○ | △ | × | ダイアログと許可にはあるが Detector に無い |
| srt / vtt | × | × | △ | ○ | ○ | magic で Text。専用メニューのみ |
| ass | ○（Text） | × | △ | ○ | × | `OtioAdapter::importAss` で取り込み可。書き出しは SRT/VTT のみ |
| csv | ○ | × | × | × | × | Data コピーのみ |
| ttf / otf / woff / woff2 / eot | ○ | × | × | △ | × | Project へは Data として入る |
| ttc | × | × | × | △ | × | AssetBrowser のフォント判定だけ対応 |
| json / lottie | ○ | ○ | △ | △ | △ | Lottie は書き出しのみ導線あり |

---

## 5. 実装スライス案

費用対効果の順。S1〜S4 は新規デコーダを必要としない。

| # | スライス | 内容 | 主な変更先 | リスク |
|---|---|---|---|---|
| S1 | 能力表の一元化 | `FileTypeDetector` に「認識 / 取り込み可 / デコード可 / 連番可 / 書き出し可」を持たせ、`AssetImporter::isSupported` と各ダイアログのフィルタをそこから生成する。新規モジュールは作らず既存 `File.TypeDetector` を拡張する | `FileTypeDetector.ixx/.cpp`、`AssetImporter.cpp`、各ダイアログ | 中（Core 公開 API と .ixx 再コンパイル） |
| S2 | 取り込みゲートと連番の整合 | `importAssetsFromPathsAsync` で Unknown / 未デコード形式を警告または除外。連番グループ化は「連番デコード可」の形式に限定。PSD/EXR などは単体登録へ | `ArtifactProjectService.cpp`、`ImageSequenceSource` | 中（既存の取り込み挙動が変わる） |
| S3 | レイヤー作成と UI の統一 | 「New Image Layer」の `QImageReader::canRead()` を Core の能力判定（OIIO open 試行または形式表）へ置換。フィルタを能力表から生成。`ttc` と `ktx2/avif/...` の扱いを決める | `ArtifactCompositionEditor.cpp`、`ArtifactLayerPanelWidget.cpp`、`ArtifactTimelineTrackPainterView.cpp`、`ArtifactImportAssetsDialog.cpp` ほか | 低〜中 |
| S4 | 出力形式の接続 | `sequenceExtension` / `isImageSequenceContainer` / `defaultOutputPathForJob` に DPX/HDR/TGA/WebP を追加。不明形式は PNG に落とさずエラー。Render Output・プリセット・AI API の形式一覧を同じ表から生成 | `ArtifactRenderQueueEncoder.cpp`、`ArtifactRenderOutputSettingDialog.cpp`、`ArtifactRenderQueuePresets.cpp`、`WorkspaceAutomation.ixx` | 中 |
| S5 | 高 bit depth の実精度 | レンダーキューの float readback を保持し、`ImageExporter::write(ImageBuf)` 経路へ渡す。または UI の bit depth 表記を「コンテナのみ」と明示 | `ArtifactRenderQueueService.cpp`、`ImageExporter` | 高（メモリ・ホットパス・色管理） |
| S6 | 3D マイナー形式の整理 | `pmx` の実装または分類削除。`dae`/`abc` をダイアログから外すか backend を追加。`isSupported` とテストを同時更新 | `MeshImporter.cpp`、`AssetImporter.cpp`、3D フィルタ | 中 |
| S7 | 周辺ファイル形式 | `.ass` 追加、字幕の File メニュー／ドロップ導線、CSV キーフレーム取り込み、フォント→テキストレイヤー、`.lottie` ZIP 展開 | 複数 | 高（新規 parser を含む） |

推奨する最初のまとまりは **S1 + S2 + S4**。形式を増やさずに「選択できるのに読めない／書けるのに選べない／選んだのと違う形式が出る」を解消できる。

---

## 6. 既存ドキュメントとの差分

- `docs/analysis/DESIRED_IMPORT_FORMATS_2026-04-19.md:29` は PSD を「フラット画像のみ」としているが、File > Import PSD Layers と `PsdDocument` によるレイヤー取り込みは実装済み。更新が必要。
- `docs/analysis/MINOR_IMAGE_3D_FORMAT_IMPORT_INVESTIGATION_2026-08-13.md:28` は USDC/USDZ を「OpenUSD runtime 未接続」としているが、`MeshImporter.cpp:3766-3770` に `ARTIFACT_WITH_OPENUSD` 条件付きの実装がある。STL/PLY/GIF/PSD/PSD レイヤー/Lottie 書き出しも現状に合わせて更新が必要。
- `docs/analysis/PROFESSIONAL_VS_AUDIENCE_GAP_2026-10-04.md:49` の A-9 は「DPX writer 不在」としているが、Core の `ImageExporter` には DPX の Cineon メタデータ処理がある。UI・プリセット・経路が不在という表現に修正すべき。
- `docs/analysis/REPORT_DCC_GAP_UPDATE_2026-08-15.md:33` の字幕は「アプリ UI 未接続」と読めるが、Composition Editor のコンテキストメニューに取り込み・書き出しがある。Caption Panel と NLE Store 永続化が未実装という整理に更新が必要。
- `docs/analysis/REPORT_DCC_GAP_UPDATE_2026-08-15.md:77` の「SRT/WebVTT は Core 入出力のみでアプリ UI 未接続」は上記と同じ理由で古い。

---

## 7. 未確認事項

- ビルド、テスト、実ファイル読み込み、書き出しは未実施。以下の runtime 挙動は静的読解からの推定を含む。
- OIIO の実 plugin 構成（embedded か vcpkg か）により `avif/heic/heif/jxl/jp2/ktx2/dpx/psd/tga` の可否が変わる。`ImageExporter` は拡張子から `ImageOutput::create()` を試すだけで、事前の能力照会をしていない。
- Qt の imageformats plugin 構成により、連番の TIFF/WebP/TGA 可否が変わる。EXR/DPX/HDR/PSD/KTX2 は QImageReader では読めない前提で記述した。
- `ARTIFACT_WITH_OPENUSD` が実際の配布ビルドで定義されるかは未確認（`libs/openusd` の存在は確認した）。
- Windows の WIC / Shell サムネイルが EXR/DPX/PSD を返すかは環境依存で未確認。
- `VectorImport` の `editableReady` が UI でどう表示されるか、PDF/AI の変換が実際に試行されるかは未確認。
- `ImageSequenceSource` の prefetch と `QImage` キャッシュはホットパス規則の対象。OIIO 経路へ置き換える場合は `docs/technical/HOT_PATH_RULES.md` の bounded backpressure とアロケーション規則を先に設計する必要がある。

---

## 参照した主なコード

- `ArtifactCore/src/File/FileTypeDetector.cpp`
- `ArtifactCore/src/Asset/AssetImporter.cpp`
- `ArtifactCore/include/Asset/VectorImport.ixx`
- `ArtifactCore/src/Media/ImageSequenceSource.cpp`
- `ArtifactCore/src/IO/Image/ImageImporter.cpp`
- `ArtifactCore/src/IO/Image/ImageExporter.cpp`
- `ArtifactCore/src/Geometry/MeshImporter.cpp`
- `ArtifactCore/src/Color/ColorLUT.cpp` / `LUTWriter.cpp`
- `ArtifactCore/src/Export/Lottie/LottieExporter.cpp`
- `ArtifactCore/src/NLE/OTIO.cpp`
- `Artifact/src/Service/ArtifactProjectService.cpp`
- `Artifact/src/Project/ArtifactProject.cpp` / `ArtifactProjectAssetRegistrationSupport.cpp`
- `Artifact/src/Render/ArtifactRenderQueueService.cpp` / `ArtifactRenderQueueEncoder.cpp` / `ArtifactRenderQueuePresets.cpp`
- `Artifact/src/Widgets/Dialog/ArtifactRenderOutputSettingDialog.cpp` / `ArtifactImportAssetsDialog.cpp` / `ArtifactScreenshotExportDialog.cpp`
- `Artifact/src/Widgets/Menu/ArtifactFileMenu.cpp` / `ArtifactLayerMenu.cpp`
- `Artifact/src/Widgets/Render/ArtifactCompositionEditor.cpp`
- `Artifact/src/Widgets/Timeline/ArtifactLayerPanelWidget.cpp` / `ArtifactTimelineTrackPainterView.cpp`
- `Artifact/src/Widgets/Asset/ArtifactAssetBrowser.cpp`
- `Artifact/include/AI/WorkspaceAutomation.ixx`
- `Artifact/translations/ja.json` / `en.json`

---

## 8. 実装状況（同日追記）

S1 / S2 / S4 の一部を適用した。**ビルド・テスト・実ファイル確認は未実施**。

### 適用済み

- **能力表の一元化（S1）**: `File.TypeDetector` に `FileCapability`（`Import` / `StillImage` / `ImageSequence` / `ImageExport` / `SequenceExport` / `Model3D` / `VideoImport` / `AudioImport`）と `supports()` / `suffixPatterns()` を追加。分類も同じ表から行うようにした。`AssetImporter::isSupported()` は重複リストを削除してこの表に委譲。
- **分類の不整合を解消**: `mxf` を Video、`eps` / `afdesign` / `afphoto` / `afpub` を Document、`ttc` を Font として分類（旧 `isSupported` の範囲は維持）。
- **連番グループ化（S2）**: `isImageImportCandidate()` を `ImageSequence` 能力で判定。EXR / DPX / HDR / PSD / KTX2 などは連番にグループ化せず単体登録する。
- **UI フィルタの一部を一元化**: Import Assets、New Image Layer、Replace Image Source、Property Editor の画像/3D/動画/音声、3D モデル選択、Relink 3D、Render Output パス参照を能力表から生成。`dae` / `abc` は 3D フィルタから外した。
- **出力（S4 の一部）**: `sequenceExtension()` に dpx / hdr / tga / webp を追加。`deriveContainerFromJob()` は固有形式を `"* Sequence"` より先に判定するように修正（EXR/DPX の黙って PNG 化を解消）。`isImageSequenceContainer()` / `defaultOutputPathForJob()` に dpx / hdr / tga を追加。Render Output に DPX / HDR / TGA Sequence と拡張子マッピングを追加。画像連番ジョブはコーデック検証をスキップするようにした（TIFF/BMP 連番も従来から誤エラーだった）。AI API の形式一覧・既定コーデックを更新。File メニューの単発/ワークエリア書き出しは未知拡張子を PNG に書き換えず、能力表で受理された形式を使う。
- **字幕 ASS 取り込み（S7 の一部）**: `OtioAdapter::importAss()` を追加（`Dialogue:` 行の timing / text を SRT 化して既存 parser へ渡す）。Composition Editor の字幕インポートで `.ass` を受理。`srt` / `vtt` / `ass` を detector の Text に分類。
- **単発スクリーンショット書き出し**: Screenshot Export ダイアログと Composition Editor のクイック保存を能力表駆動に変更。TIFF/BMP/DPX/HDR/TGA/WebP も選択可能（EXR のマルチチャンネルは従来どおり EXR のみ）。
- **マイナー画像のサイズ取得**: `imageSizeForPath()` に OIIO 経由（`ImageAssetFile`）のフォールバックを追加し、EXR/DPX/HDR などの互換性警告を低減（インポート時のコールドパスのみ）。
- **テスト**: `FileTypeDetectorContractTest` に能力表・パターン生成・字幕分類の契約テストを追加（未実行）。

### 未対応 / 意図的に見送り

- **WebP 連番**: `isImageSequenceContainer()` に入れると Animated WebP の経路と既定出力先が壊れるため、`sequenceExtension()` の拡張子解決だけ追加した（`Insight.md` に記録）。
- **高 bit depth の実精度（S5）**: レンダーキューは `QImage` 経由のまま。DPX/HDR を選んでも中身は 8-bit。
- **タイムラインの翻訳済みフィルタ**: `timeline.image_files` 等は ja/en の翻訳文字列に拡張子が埋め込まれており、他 9 言語はフォールバック。能力表からの生成には 11 言語のキー追加が必要。
- **3D backend（S6 の残り）**: `blend` / `dae` / `abc` / `pmx` は分類のみのまま（ゲートを閉じた状態を維持）。
- **S7 の残り**: CSV キーフレーム取り込み、フォント→テキストレイヤー生成、`.lottie` ZIP 展開、ASS 書き出しは未着手。字幕の File メニュー／ドロップ導線も未対応。
- OIIO / Qt plugin の runtime 能力照会は未実装。能力表は「コードパスの有無」のみを表す。
