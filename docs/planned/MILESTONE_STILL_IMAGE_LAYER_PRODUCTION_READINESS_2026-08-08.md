# MILESTONE: 静止画レイヤー Production Readiness

**最終更新:** 2026-08-08

**ステータス:** In Progress

**識別子:** M-IMG-1

## 目的

`ArtifactImageLayer` を、画像を表示できる段階から、実制作で安心して使える静止画レイヤーへ引き上げる。読み込み、表示品質、変換、マスク、合成、保存／再読込、プレビュー安定性を一つの受入経路として閉じ、静止画を後続の連番画像・画像処理・3D合成の基準素材にする。

## 背景と責務

- `ArtifactImageLayer` には OIIO 読み込み、`ImageF32x4_RGBA` フレームバッファ、入力色解釈、ソース識別、JSON 保存、GPU 描画へ接続する基盤がある。
- 静止画像の既存計画は、OIIO 移行、Static Layer GPU Cache、Source Reframe、Professional Media、Asset System など個別の責務に分かれている。
- 本マイルストーンはそれらを再実装せず、「静止画一枚を読み込み、編集し、保存し、再度開き、同じ結果を得る」制作フローの完成責任を持つ。
- 連番画像のフレーム選択・欠番・再生は `MILESTONE_IMAGE_SEQUENCE_WORKFLOW_COMPLETION_2026-07-27.md` の責務とする。

## 到達目標

1. 一般的な静止画と高ビット深度素材を、失敗理由を失わず読み込める。
2. レイヤー変換、crop／reframe、opacity、mask、blend、effect の結果が preview と最終出力で一致する。
3. source path、asset identity、色解釈、PSD subimage、fit policy を保存／再読込後も復元できる。
4. 同一ソースの再利用、source 更新、relink、missing source の各状態を安全に扱える。
5. GPU fast path が使えない場合も、明示的な理由と安全なフォールバックを持つ。

## 対象範囲

- PNG、JPEG、TIFF、WebP、EXR／HDR など、現行 OIIO 経路が扱う静止画。
- `ArtifactImageLayer` の source load、metadata、working-space 解釈、current frame buffer。
- position／scale／rotation／anchor／opacity、fit、crop／source reframe。
- mask、track matte、blend mode、effect stack、composition scale を通る合成。
- project save／reload、asset relink、source file 更新、missing source 表示。
- Composition View、preview、Render Queue の静止画結果とキャッシュ無効化。

## 非対象

- 動画読み込み、動画デコード、音声同期。
- 連番画像固有の frame rate、欠番、範囲、再生ポリシー。
- 新しい画像編集アプリ相当の paint／retouch 機能。
- DiligentEngine／DX12 backend の広範な変更。
- `ReactiveEvents` の変更。

## 実装フェーズ

### Phase 0 — ベースラインと受入素材の固定

- 8-bit sRGB、alpha 付き画像、16-bit、float／HDR、向き・pixel aspect・ICC等の metadata を含む代表素材を固定する。
- import → layer create → edit → save → reload → preview → render の基準手順を定義する。
- decode failure、unsupported format、missing source、corrupt file の期待表示と診断情報を定義する。

### Phase 1 — 読み込みとソース同一性

- OIIO 読み込み結果から dimensions、channels、alpha、orientation、color metadata を欠落なく保持する。
- shared source identity と localized identity の使い分け、source version 更新、relink の契約を固定する。
- 非同期 prefetch 中の表示、キャンセル、失敗、同期フォールバックで stale buffer を表示しない。

### Phase 2 — 編集と保存／再読込

- transform、fit、crop／reframe、input interpretation、PSD subimage の保存項目を棚卸しする。
- source path と asset ID の優先順位を固定し、project 移動後の相対パス／relink を確認する。
- undo／redo と保存／再読込で pixel result、bounds、anchor、layer duration が変わらないことを確認する。

### Phase 3 — 合成品質と経路一致

- alpha の straight／premultiplied 境界と linear working space への変換位置を明示する。
- mask、matte、blend、effect、opacity、composition scale の代表組み合わせを preview と Render Queue で比較する。
- Qt の `QPainter`／CompositionMode を新しい合成経路として追加せず、GPU または専用 CPU 合成を正規経路にする。

### Phase 4 — キャッシュと更新安定性

- source version、interpretation、crop、effect、mask、resolution を cache key／invalidation に反映する。
- 同一画像を複数レイヤーで共有する場合の GPU texture 再利用と、ローカル化後の分離を確認する。
- source file の上書き、relink、device 再初期化、composition resize 後に古い画像が残らないことを確認する。

### Phase 5 — 制作受入

- 小画像、巨大画像、透明画像、グレースケール、CMYK等の非標準素材を含む受入表を実行する。
- scrub、連続 transform、mask 編集、保存直後の再読込、Render Queue 出力でクラッシュや表示停止がないことを確認する。
- 失敗時に source、decode、color、upload、cache、composite のどの段階かを診断できるようにする。

## 実装制約

- 新規の `QImage` は Qt API／入出力互換境界に限定し、描画・合成・転送の主経路へ増やさない。
- `QPainter`／Qt CompositionMode による新規合成は行わない。
- QtCSS、`QColorDialog`、新規シグナル／スロットを追加しない。
- `.ixx` の変更は公開契約に不可欠な場合だけとし、実装側で閉じられる変更を優先する。
- Diligent／DX12 の挙動を推測で変更せず、backend 問題と app 側 cache／upload 問題を分離して診断する。

## 完了条件

- [ ] 代表静止画フォーマットの成功／失敗条件と metadata 保持が確認されている。
- [ ] transform、fit、crop、mask、matte、blend、effect の代表ケースが preview と Render Queue で一致する。
- [ ] 保存／再読込後に source identity、色解釈、bounds、編集結果が維持される。
- [ ] source 更新、missing、relink、shared／localized source の状態遷移が安全に動作する。
- [ ] cache invalidation 後に stale texture／stale buffer が残らない。
- [ ] フォールバック条件と失敗段階を診断情報から特定できる。
- [ ] 静止画の受入ケースでクラッシュ、UI停止、project data破損がない。

## 関連文書

- [`Artifact/docs/MILESTONE_OIIO_IMAGE_PIPELINE_MIGRATION_2026-03-30.md`](../../Artifact/docs/MILESTONE_OIIO_IMAGE_PIPELINE_MIGRATION_2026-03-30.md)
- [`Artifact/docs/MILESTONE_STATIC_LAYER_GPU_CACHE_2026-03-26.md`](../../Artifact/docs/MILESTONE_STATIC_LAYER_GPU_CACHE_2026-03-26.md)
- [`Artifact/docs/MILESTONE_ASSET_SYSTEM_2026-03-12.md`](../../Artifact/docs/MILESTONE_ASSET_SYSTEM_2026-03-12.md)
- [`MILESTONE_PROFESSIONAL_MEDIA_MATERIALS_2026-07-16.md`](MILESTONE_PROFESSIONAL_MEDIA_MATERIALS_2026-07-16.md)
- [`MILESTONE_IMAGE_SEQUENCE_WORKFLOW_COMPLETION_2026-07-27.md`](MILESTONE_IMAGE_SEQUENCE_WORKFLOW_COMPLETION_2026-07-27.md)
- [`../done/MILESTONE_LAYER_SOURCE_REFRAME_NLE_2026-06-24.md`](../done/MILESTONE_LAYER_SOURCE_REFRAME_NLE_2026-06-24.md)

## 次の実装単位

Phase 2 としてproject root基準のsource path解決責務をAssetManager／project保存境界で定義し、image・sequence・videoを共通のrelocation／relink契約へ載せる。ビルドやruntime検証は、実行許可を得てから行う。

## 実装進捗

### 2026-08-08 — 入力色解釈別の共有デコードキャッシュ分離

- `ArtifactImageLayer` の共有 `ImageF32x4_RGBA` キャッシュキーへ、入力色空間と transfer function の組を SHA-256 で識別する interpretation key を追加した。
- 解釈指定なしの既存キーは維持し、同一ソースかつ同一解釈のレイヤーだけが変換済みバッファを共有する。
- decode payload の取得、非同期 prefetch 完了時の公開、source identity の再共有、遅延 `QImage` 変換後の公開を同じキー規則へ統一した。
- これにより、同一画像を異なる入力色解釈で配置した際に、一方の working-space 変換結果を他方が再利用する誤共有を防ぐ。
- 静的確認のみ実施。ビルドと runtime 比較は未実施。

### 2026-08-08 — 読み込み失敗状態の統一

- header decode失敗、不正な画像寸法、AssetManagerへのsource登録失敗を、同じ失敗遷移へ統一した。
- 失敗時は以前のsource identityとdecoded cacheを破棄し、要求されたsource pathとplaceholderを対応させる。AssetManager登録失敗時に以前の画像が残るstale表示を防ぐ。
- fallback診断へheader decodeエラー、実寸法、source登録失敗の段階を記録し、単一の「missing image」理由へ潰さないようにした。
- 非同期prefetchのgenerationを進め、失敗前に開始された古い結果が後からplaceholderを上書きしないようにした。
- 静的確認のみ実施。ビルドとruntime失敗ケースの確認は未実施。

### 2026-08-08 — 非同期decode結果の取込統一

- `QFutureWatcher` callbackと`toQImage()`の完了済みfuture取込を、単一の`adoptPrefetchResult()`へ統合した。
- どちらの完了タイミングでも入力色解釈、解釈別共有cache公開、寸法更新を同じ順序で行い、一方だけ未変換bufferを一時表示する差を解消した。
- full decode失敗時はcacheを空のままにせず、placeholderの`QImage`と`ImageF32x4_RGBA`を確定して同期再読込への移行を防ぐ。
- async decode失敗をFallbackTrackerとwarningへ記録し、同じfuture結果の二重取込・診断重複を`prefetchDone_`で抑止した。
- 静的確認のみ実施。ビルド、破損素材、読み込み中source差し替えのruntime確認は未実施。

### 2026-08-08 — 読み込み開始時の同期全画素decode撤去

- `loadFromPath()`の事前検査を`ImageBuf::read(..., UINT8)`から`ImageInput::open()`と`ImageSpec`取得へ変更した。
- header検査のために全画素を8-bitへ同期decodeし、その直後に非同期float decodeを重ねていた二重処理を撤去した。
- header open失敗はOIIOのglobal errorを保持して既存のplaceholder失敗遷移へ渡す。
- 画素decode、orientation反映、float buffer生成、derived cache生成は引き続き非同期reader側だけが担当する。
- 静的確認のみ実施。巨大画像と低速ストレージでのUI応答時間はruntime未確認。

### 2026-08-08 — sourceなしJSONの再読込状態修正

- `fromJsonProperties()`で`image.sourcePath`が空の場合、以前のsource identity、source path、寸法、image状態を明示的にclearする。
- 既存レイヤーインスタンスへsourceなしJSONを適用した際に、適用前の画像が後から再表示されるstale restoreを防ぐ。
- source pathなしでは保存済みlocalized IDを再取得しない。pixel sourceを持たないidentityだけが復元される不整合を避ける。
- 静的確認のみ実施。sourceあり／なしのJSON往復と既存インスタンス再利用はruntime未確認。

### 2026-08-08 — source registry消失時の自己回復

- `refreshSourceVersionIfNeeded()`でversion `0`を正常な未変更状態として扱わず、保持しているsource pathからAssetManager登録を回復する。
- registry reset等でsource IDが無効になっても、回復したIDとversionへ更新し、旧cacheを破棄してprefetchを再開する。
- 再登録できない場合は古いdecoded bufferを保持せず、generationを進めてplaceholder bufferと明示診断へ遷移する。
- `cachedSourceVersion_ == 0`から有効versionへ戻る場合もcache refresh対象にし、古い表示を再利用しない。
- 静的確認のみ実施。registry reset、relink、source削除のruntime状態遷移は未確認。

### 2026-08-08 — source更新中のstale prefetch拒否

- `PrefetchResult`へ要求開始時のAssetManager source versionを保持する。
- 結果採用時に現versionと一致しない場合はdecoded結果を公開せず、cacheを破棄して最新versionのprefetchを再発行する。
- source registry消失と競合した場合は結果を破棄し、次のversion refreshでregistry回復経路へ渡す。
- background threadの直接`QImage`返却でもgenerationとsource versionを検証し、更新前のframeを返さない。
- 静的確認のみ実施。decode中のsource上書きと連続invalidateはruntime未確認。

### 2026-08-08 — 共有cache公開失敗時のlocal buffer保持

- `publishImagePayloadOrKeep()`を追加し、AssetManagerへのdecoded payload公開がversion競合等で拒否されても、正常なlocal `ImageF32x4_RGBA`を保持する。
- prefetch結果採用、localized sourceからshared sourceへの再接続、遅延`QImage`変換の3経路を同じ規則へ統一した。
- cache共有の失敗をレイヤー表示の空bufferへ波及させず、次のsource version refreshまでlocal表示を継続できるようにした。
- 静的確認のみ実施。publish直前invalidateのruntime競合は未確認。

### 2026-08-08 — 未導入OCIO色空間の保存値保持

- `setInputInterpretation()`で現在のOCIO configに存在しない入力色空間を空文字へ破棄せず、正規化した指定値を保持する。
- 別環境や一時的にconfigが未導入の状態でprojectを開いて保存しても、元の入力色空間指定を失わない。
- 利用可能な色空間は従来どおりcase-insensitiveにcanonical名へ揃え、利用不能時はwarningとFallbackTrackerへ記録する。
- 色空間とtransfer functionが実質同値ならcache再読込、dirty、changed通知を発生させない。
- 静的確認のみ実施。OCIO configなし→導入→再読込のruntime往復は未確認。

### 2026-08-08 — 空source pathの正式clear操作

- `loadFromPath("")`をdecode失敗ではなくsource clearとして扱う。
- source identity、sequence状態、prefetch generation、decoded cache、寸法、`hasImage_`を一括resetし、JSONのsourceなし復元状態と一致させる。
- 実際に状態が変わった場合だけSource dirtyとchanged通知を発行し、既に空の状態への再適用はno-opにする。
- Propertyからsource pathを消した際に256px placeholder画像へ変化する不整合を解消した。
- 静的確認のみ実施。Property編集とundo／redoによるsource clear／restoreはruntime未確認。

### 2026-08-08 — Property経由source変更のdirty／missing受理

- `setLayerPropertyValue()`のsource path変更時だけSource dirtyを明示し、factory生成やJSON復元の`loadFromPath()`はdirtyにしない責務を維持する。
- decodeできないpathでも要求pathとplaceholder状態への遷移が完了していれば、Property値として受理してprojectへ保存できるようにした。
- missing sourceをUI側で変更拒否扱いにして表示値だけ巻き戻し、内部pathと食い違う状態を防ぐ。
- 静的確認のみ実施。Property編集、project dirty表示、missing source保存／再読込はruntime未確認。

### 2026-08-08 — UI thread同期decode fallback撤去

- `toQImage()`でprefetch futureが存在しない例外状態に、メインスレッドから`loadImageViaOIIO()`を同期実行する経路を撤去した。
- prefetch実行中は従来どおりloading placeholder、prefetch未生成時は診断付きの確定placeholderへ遷移する。
- cacheなしの最終fallbackもplaceholderの`QImage`／`ImageF32x4_RGBA`を一度だけcacheし、毎frameのplaceholder生成と診断重複を防ぐ。
- background thread側の同期fallbackはUIを停止しない互換境界として維持する。
- 静的確認のみ実施。prefetch scheduler停止時のUI応答と復旧操作はruntime未確認。

### 2026-08-08 — GPU texture cacheへの入力色解釈反映

- 共有GPU texture keyへsource versionに加えて入力色空間と伝達関数を含め、同一assetを異なる入力解釈で使うレイヤー間の誤共有を防止した。
- 2D composition描画、Render Controller、3D image cardの各GPU cache keyを同じ識別規則へ揃えた。
- layer surface cacheにもsource versionと入力色解釈を反映し、解釈変更後に旧surfaceが残る経路を閉じた。
- 静的確認のみ実施。GPU cache hit／invalidationと2D・3Dの表示一致はruntime未確認。

### 2026-08-08 — JSON復元と入力色解釈編集の状態遷移分離

- 入力色空間／伝達関数の正規化と未導入色空間の診断を、dirty化やdecode再開を伴わない内部更新へ分離した。
- `fromJsonProperties()`は復元前の旧sourceを再decodeせず、serialized sourceに対して一度だけprefetchを開始する。
- project復元中の入力色解釈設定ではユーザー編集用のdirty化と`changed()`通知を発生させず、通常のProperty編集では従来どおりsource invalidationを行う。
- 静的確認のみ実施。異なるsource／色解釈を持つJSONの既存instanceへの適用とproject dirty状態はruntime未確認。

### 2026-08-08 — Phase 0受入マトリクス固定

- [`docs/analysis/STILL_IMAGE_LAYER_ACCEPTANCE_MATRIX_2026-08-08.md`](../analysis/STILL_IMAGE_LAYER_ACCEPTANCE_MATRIX_2026-08-08.md)へ、header、decode、working-space、CPU/GPU、保存復元の現行接続点を整理した。
- 8-bit、alpha、16-bit、float/HDR、grayscale、orientation、ICC、CMYK、PSD、巨大／破損／missing素材の期待結果を固定した。
- transform、fit、crop、mask、matte、blend、effect、shared/localized、source overwrite、device resetを操作受入項目として分離した。
- 静的棚卸しから、pixel aspect／source color metadata保持、4ch channel semantics、Qt thumbnail境界の色一致を次の実装ギャップとして記録した。

### 2026-08-08 — OIIO alpha channel semantics尊重

- 1ch、2ch、3ch、4ch以上のRGBA展開を単一のchannel mapping規則へ統一し、OIIO `ImageSpec::alpha_channel`をalphaの正規判定にした。
- 4ch以上でもalphaが宣言されていない素材は第4chを透明度として扱わず、不透明alphaを補完して追加channelを無視する。
- alpha未宣言の多channel素材をFallbackTrackerへ`ambiguous-channel-semantics`として記録し、CMYK／AOV等をsilentにRGBA扱いする経路を診断可能にした。
- channel mapping変更前のderived float cacheを再利用しないようcache format／identityをv2へ更新した。
- 静的確認のみ実施。gray+alpha、ARGB、CMYK、RGB+AOVの実素材decodeはruntime未確認。

### 2026-08-08 — Source image metadataの保存／復元

- OIIO headerからchannel数／名前、alpha位置、orientation、pixel aspect、bit depth、source color space、transfer function、primaries、ICC profile byte数を取得する。
- source metadataを`image.sourceMetadata`としてJSON保存し、project再読込後もsource interpretationの根拠を保持する。
- missing sourceでprojectを開いた場合は保存済みmetadataをlast-known descriptionとして維持し、relink成功時は新しいheader情報で置き換える。
- in-memory `QImage`／`ImageF32x4_RGBA`へ置換した場合も、旧file metadataを残さず対応するRGBA metadataへ更新する。
- ICC profile本体はproject JSONへ複製せず、存在確認に必要なbyte数だけを保持する。
- 静的確認のみ実施。orientation／pixel aspect／ICC素材のJSON往復はruntime未確認。

### 2026-08-08 — Source version更新時のmetadata同期

- 非同期decode結果へpixel bufferと同じsource header metadata snapshotを保持する。
- generation／source version検証を通過した結果だけがmetadataを更新し、stale prefetchが新しいdescriptionを上書きしないようにした。
- source上書き後のprefetch採用時にchannel構成、orientation、pixel aspect、color metadataも新versionへ追従する。
- derived pixel cacheからの読込ではload開始時のheader metadataを維持し、metadataを持たないcache結果で消去しない。
- 静的確認のみ実施。異なるmetadataを持つ同一path上書きのruntime確認は未実施。

### 2026-08-08 — Source color metadataの入力解釈接続

- 明示的なInput Color Space／Transfer Functionが空の場合、source headerの`oiio:ColorSpace`／`TransferFunction`をworking-space変換へ使用する。
- ユーザーの明示overrideは常にsource metadataより優先し、推定値を保存設定へ書き戻さない。
- metadata由来の実効解釈をdecoded float cache keyにも反映し、明示設定なしのsource間で異なる変換結果を誤共有しない。
- 非同期decodeでheader metadataが更新された場合は、新metadataを先に採用してからworking-space変換を適用する。
- 静的確認のみ実施。embedded color space、明示override、未導入色空間のruntime比較は未実施。

### 2026-08-08 — Source色空間からのtransfer fallback補完

- source metadataに独立したTransfer Functionがない場合、sRGB、Rec.709、Rec.2020、PQ、HLG、ACEScc／ACEScctの代表色空間名からtransferを補完する。
- OCIO processorを利用できない環境でも、sRGB等を誤ってLinear decodeするfallback差を抑止する。
- 明示Transfer Functionとsource metadataの独立属性を優先し、名前推定は両方が空の場合だけ使用する。
- 補完後の実効transferをdecoded cache identityへ含め、fallback設定差によるbuffer誤共有を防ぐ。
- 静的確認のみ実施。OCIO有効／無効でのpixel比較はruntime未確認。

### 2026-08-08 — Pixel aspectの表示bounds適用

- source headerのpixel aspectを、`Fit to Layer`が無効な静止画の表示rectへ横方向の倍率として適用した。
- crop／UVは従来どおりsource pixel座標で保持し、表示rectとrotation pivotだけをaspect補正するため既存projectのcrop値を変更しない。
- `localBounds()`も同じdisplay rectを返すため、Composition View、Render Controller、hit-test、mask座標変換、GPU sprite描画が同じ寸法を共有する。
- `Fit to Layer`が有効な場合は既存のlayer寸法を優先し、pixel aspectによる追加stretchを行わない。
- 静的確認のみ実施。非正方pixel素材のpreview／Render Queue比較はruntime未確認。

### 2026-08-08 — Standard CMYK decodeのRGB変換

- OIIO channel nameがC/M/Y/KまたはCyan/Magenta/Yellow/Blackの4ch素材を標準CMYKとして明示検出する。
- CMYKはdecode直後にRGBへ変換してopaque alphaを設定し、Kを透明度やRGB第4成分として誤用しない。
- async decode、background同期fallback、PSD subimage用float decodeのすべてで同じ変換を適用する。
- channel名が不明な多channel素材やalphaを持つCMYK系素材はCMYKへ推測変換せず、既存の診断付きopaque fallbackを維持する。
- 静的確認のみ実施。CMYK TIFF／JPEG、ICC profile付きCMYK、CMYK+alphaの実素材検証は未実施。

### 2026-08-08 — Associated alphaの入力色変換境界

- source metadataへOIIOのUnassociatedAlpha由来のalpha associationを保持する。
- associated alphaを明示する素材だけ、working-space変換の直前にunpremultiplyし、変換後に同じalphaでpremultiplyする。
- alphaがゼロに近い画素はRGBをゼロへ正規化し、除算による非有限値やfringeを防ぐ。
- unassociated alphaが既定のPNG等は従来のstraight color経路を維持する。
- 静的確認のみ実施。premultiplied EXR、straight PNG、半透明edgeのpreview／Render Queue比較はruntime未確認。

### 2026-08-08 — 総画素数上限による巨大画像メモリ境界

- 既存の16K辺長上限に加え、64 Mi pixelsの総画素数上限を定義した。
- header preflight、OIIO decode、derived cache復元、連番frame取込を同一の寸法検証へ統一し、float RGBAとQImageの確保前に巨大素材を拒否する。
- 8K級素材は許容し、16K四方のような必要メモリが大きい素材はplaceholder failureへ遷移する。
- 静的確認のみ実施。上限近傍の素材、メモリ逼迫時、巨大なderived cacheの実行検証は未実行。

### 2026-08-08 — In-memory image入力の総画素数境界

- 編集ツールから渡される`QImage`と`ImageF32x4_RGBA`にも同じ64 Mi pixelsの上限を適用した。
- 上限を超える入力は既存のlayer stateを変更せず、`FallbackTracker`とwarningへ記録して拒否する。
- ファイルdecodeだけを制限して編集結果のdeep copyやQImage化で過大な追加確保が発生する経路を防ぐ。
- 静的確認のみ実施。高解像度の編集結果を生成する各ツールからの拒否表示と回復操作はruntime未確認。

### 2026-08-08 — Decode channel数の安全上限

- image layerの入力channel数を1〜64に制限し、異常な多channel素材をheader検査でplaceholder failureへ遷移させる。
- 通常decode、PSD subimage decode、非同期decodeで同じ判定を使い、経路ごとのresource limit差をなくした。
- 既存のRGBA展開・CMYK判定より前に入力を拒否するため、異常specをchannel変換や追加buffer確保へ渡さない。
- 静的確認のみ実施。多channel EXR／PSDの実素材による許容範囲と診断文言はruntime未確認。

### 2026-08-08 — 連番source path復元の入力境界統一

- JSONの`image.sequencePaths`復元にも、通常の`image.sourcePath`と同じ32,768文字のpath長上限を適用した。
- 保存データ経由だけが無制限のframe pathを`ImageSequenceSource`へ渡す差をなくし、連番の欠損／再リンク状態を既存のplaceholder経路へ揃えた。
- 静的確認のみ実施。極端に長いpathを含む連番projectの復元と診断表示はruntime未確認。

### 2026-08-08 — Float decode helperのsubimage経路統一

- `ImageF32x4_RGBA`固有のdefault load分岐を撤去し、未指定subimageもOIIO subimage 0として共通decodeへ通すようにした。
- CMYK conversion、RGBA channel mapping、寸法／channel数上限がdefault imageとPSD subimageで同じ規則になる。
- 静的確認のみ実施。PSD flattened imageとsubimage 0の実素材比較はruntime未確認。

### 2026-08-08 — 受入マトリクスの静的根拠監査

- grayscale／gray+alpha channel mapping、PSD index保存、failure placeholder、missing source path保持の実装根拠を受入マトリクスへ明記した。
- runtime未実行の行をPass扱いにせず、すべて`実装済・未実行`として実素材比較が必要な状態を維持する。
- ICC profile変換、project相対path契約、Qt表示とfloat bufferの色一致、Render Queue比較は引き続き未解決またはruntime待ち。
