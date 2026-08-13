# マイナーな画像・3Dモデル形式のインポート調査レポート

**最終更新:** 2026-08-13

## 結論

現状は、一般的な画像・動画・音声と、OBJ/FBX/glTF/GLBなどの3Dモデルを中心にした構成である。今回、既存のOIIO画像経路に乗せられる追加画像形式を、形式判定とAsset管理の対象へ追加した。マイナー形式を広く追加する前に、既存の形式判定、インポート、Contents Viewer、Asset Browser、レイヤー生成の経路を一つの対応表に揃える必要がある。

追加候補は、単に拡張子を認識するだけで価値が出るものと、専用デコーダ／シーン変換が必要なものを分けるべきである。短期は画像シーケンス・HDR・PSD・Lottieなど制作実務に近い形式、中期は特殊3D形式の「変換前提」対応が妥当である。

## 現行の確認結果

### 画像

- OIIOベースの画像取り込み・メタデータ・float buffer・thumbnail経路がある。
- PSDは現状フラット画像として扱う整理で、レイヤー構造の保持は未対応。
- EXRは需要文書上の未対応候補であり、OpenEXR/OIIO経路の実装範囲を再確認する必要がある。
- GIFは先頭フレームのみという整理で、アニメーションとしての取り込みは未完了。
- SVGはビットマップ化が中心で、編集可能なパス／アニメーションとしてのインポートは未対応。
- Aseprite、Krita、TIFFの多ページ・RAW・DDS/KTX2などの専用取り込みは、今回の静的調査では正規の統合経路を確認できなかった。
- AVIF/HEIC/HEIF/JXL/JP2/PNM/PFM/KTX2は、OIIOがビルドに該当codec/pluginを持つ場合に既存画像readerで扱えるよう、拡張子の分類とAsset管理へ追加した。実ファイルの読み込みはruntime未検証。

### 3Dモデル

- `MeshImporter` は `ufbx` を本線としてOBJ/FBX/glTF/GLBを読み込む。
- OBJには `tinyobjloader` fallbackがある。
- PMD/PMXには限定的なPMD系読み込みコードが存在する。
- USDAはASCIIの限定読み込みがあるが、USDC/USDZはOpenUSD runtime未接続。
- ABC/Alembic、DAE、USD系の一部はUIのファイルフィルタに現れる一方、実際の importer backend が形式ごとに完成しているとは限らない。STLはASCII／binary、PLYはASCIIの静的メッシュ読み込みを追加した。
- 既存の3Dレイヤーは、読み込んだメッシュを基本的に単一 `Artifact3DLayer` として保持するため、シーン階層、スキニング、アニメーション、複数マテリアルの完全保持は別課題である。

## 候補形式の優先順位

| 優先 | 形式 | 価値 | 実装見込み | 方針 |
|---|---|---|---|---|
| P1 | GIFアニメーション | 誰でも使う。現在の先頭フレーム制限を解消 | 小 | 既存フレーム／画像シーケンス経路へ接続 |
| P1 | EXR/HDR | コンポジット・HDRIで実務価値が高い | 小〜中 | OIIO経路と色管理を正規化 |
| P1 | PSDレイヤー | 制作データの再利用価値が高い | 中 | レイヤー名、可視、位置、透明度を限定保持 |
| P1 | SVGパス | UI・モーション用途に有効 | 中 | ビットマップ化と編集可能パスを分離 |
| P2 | Lottie | 2Dモーション素材の交換に有効 | 中 | ThorVG等の採用可否を決める |
| P2 | Aseprite/Krita | 連番・ドット絵・レイヤー素材向け | 中 | フレームとレイヤーを画像シーケンスへ変換 |
| P2 | DAE | 3D素材の受け皿を広げる | 小〜中 | 変換後の三角形メッシュに限定 |
| P2 | Alembic | キャッシュアニメーション用途 | 高 | まず静的ジオメトリ／キャッシュ再生の要否を分ける |
| P3 | USDC/USDZ | USDパイプラインとの接続 | 高 | OpenUSD導入と依存配布を先に評価 |
| P3 | DDS/KTX2 | GPUテクスチャ・ゲーム資産 | 中 | 画像入力とGPU圧縮テクスチャを分離 |
| P3 | RAW各種 | 写真現像用途 | 高 | 専用ライブラリとカラーマネジメントが必要 |

## 実装上の重要な分離

1. **認識**: `FileTypeDetector` が拡張子や内容を分類する。
2. **解釈**: decoder/importerがピクセル、フレーム、メッシュ、属性を読み込む。
3. **アプリ統合**: Asset Browser、Contents Viewer、レイヤー生成、保存／再読込へ渡す。
4. **品質保証**: backend、警告、部分対応、失敗理由をUI／diagnosticsに残す。

現在のコードには、UIのファイルフィルタに形式が先行して追加されている例があるため、「選択できる」ことと「実際に読める」ことを対応表で分ける必要がある。拡張子追加だけで対応済みとは扱わない。

## 推奨する最初の実装スライス

### A. 対応表と失敗状態の整理

- 形式ごとに `recognized / importable / previewable / layerable` を定義する。
- `MeshImporter` と画像読み込み経路で backend 名、部分対応、失敗理由を統一する。
- デフォルトCubeやplaceholderへのフォールバック時に、成功扱いにならないようにする。

### B. 低コスト形式

- GIFをフレーム列として読み込む。
- EXR/HDRをOIIO／既存float pipelineに接続する。
- STL/PLYは静的メッシュ限定で追加する。

### C. 制作データ形式

- PSDは最初から全Photoshop互換を目指さず、ラスタライズ可能な一般レイヤーの名前・可視・位置・不透明度に絞る。
- SVGは既存のラスタライズ経路を維持しつつ、パス保持は別backendとして設計する。

## 見送るべき初期対応

- 拡張子だけの追加。
- Alembic/USDZ/RAWの一括対応宣言。
- 3Dリグ、スキニング、モーフ、ノードマテリアルまで含む完全互換。
- QtのQImage/QPainter合成を本流に戻す実装。画像入力の境界とGPU／float処理の本流を分ける必要がある。

## 推奨ロードマップ

1. 対応表・エラー表現を整える
2. GIF、EXR/HDR、STL/PLYの実ファイル受入れを整える
3. PSDの限定レイヤー取り込みを設計する
4. SVGパスまたはLottieを、採用ライブラリ込みで比較する
5. Alembic/USD/DDS/KTX2/RAWは需要サンプルと依存配布条件を確認してから判断する

## 根拠として確認した主な文書・コード

- `docs/analysis/DESIRED_IMPORT_FORMATS_2026-04-19.md`
- `docs/analysis/TECH_ADOPTION_CANDIDATES_2026-07-27.md`
- `docs/analysis/MILESTONE_OIIO_IMAGE_PIPELINE_MIGRATION_2026-03-30.md`
- `docs/planned/MILESTONE_3D_MODEL_IMPORT_AND_CONTENTS_VIEWER_2026-03-29.md`
- `ArtifactCore/src/Geometry/MeshImporter.cppm`
- `Artifact/src/Layer/Artifact3DModelLayer.cppm`
- `Artifact/src/Widgets/Viewer/ArtifactContentsViewer.cppm`
- `Artifact/src/Widgets/Asset/ArtifactAssetBrowser.cppm`

※ 先ほど作成したMaya前提の調査は解釈違いのため、このレポートへ差し替えた。
