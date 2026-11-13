# Source Crop / Pan 数値検証計画

**最終更新:** 2026-11-13
**ステータス:** In Progress

## 目的

NLE風 Crop / Pan の画像内ズームとクロップについて、表示上の印象だけでなく、既知のsource pixel座標・寸法に対する数値応答を確認する。対象は静止画レイヤーのSourceCropからComposition Previewへ至る経路。動画、キーフレーム、rotation、alpha品質、PreviewとRender Queueの経路差は後続項目とする。

## 実装から確認した契約

- `ArtifactSourceCrop::effectiveCropRect()` は `cropRect` をsource boundsへ収め、anchorを基準に幅・高さを `zoom` で除し、その後 `pan` を加えてsource boundsと交差する。
- `cropRect` と `pan` はsource pixel座標。`zoom=1` はcropRectそのまま、`zoom=2` は範囲の縦横が半分。
- `sourceToOutputTransform()` は有効なcrop中心をoutput中心へ写す。`preserveAspect=true` は縦横の小さい倍率を共通採用する。
- 静止画のComposition draw pathは別途 `sourceCropDrawLayout()` とGPU texture samplingを通る。モデル数式だけではアプリの最終画素が正しいことを証明しない。

## 評価計画

1. **比較基準:** `tests/fixtures/source_crop/source_crop_coordinate_grid.expected.json` の座標・範囲を解析上の期待値とする。色付き非対称象限、16pxグリッド、座標ラベルを含むPNGをfixture generatorから生成する。現在のアプリ出力をbaselineとして保存し、数式期待値とは別物として記録する。
2. **条件:** 512×384 source、同じ縦横比の384×288 composition、静止フレーム、標準sRGB PNG。layer fitを固定し、opacity=1、rotation=0、anchor=(0.5,0.5)、preserveAspect=true、他のeffect/maskなし。GPU/backend、Artifact revision、表示倍率、出力形式を記録する。
3. **シナリオ:** neutral、zoom=2 center、zoom=2 + pan=(32,24)、zoom=2 + pan=(-32,-24)、explicit crop=(64,48,256,192)。各々有効化前後も比較。
4. **主判定:** 出力で対応するsource landmarkを追跡し、source-to-outputのX/Y倍率・offsetを最小二乗で推定する。期待矩形との差はsource pixel単位で記録。幾何契約はsource座標の最大絶対誤差 ≤1 px、同じaspect設定のoutput boundsは1 output px以内とする。PNG rasterの境界量子化を許容するため画素完全一致は要求しない。
5. **副判定:** Preview capture上のグリッド位置と四象限を確認する。PNG画像座標からMAE/RMSEを出す場合は、同じ composition/output、color transform、alpha合成条件を揃える。色変換差を幾何誤差と混ぜない。
6. **時間・性能・色alpha:** 静止画・固定値で時間安定性は対象外（理由: 時間変化なし）。RGBA edge/transparent outside、preview/render parity、処理時間・GPU memoryは本sliceでは未検証として残し、完了扱いにしない。
7. **実行経路:** Artifactの実Preview captureをシナリオごとに保存し、可能なら同一条件のRender Queue画像も取得。Preview/Render比較は既存静止画受入基準に従い、幾何判定と色差判定を別々に記録する。

## 合否と証跡

幾何数値判定の全シナリオが許容値以内で、zoomとpanの符号・量が期待方向に一致した場合のみ、この範囲の数値動作をPassとする。アプリの実出力未取得、Preview/Render parity未測定、画像上でランドマークを識別できない場合は未検証。画像、capture、測定CSV/JSON、環境情報を同じ証跡フォルダに保存する。

## 実行結果と次の段階

2026-11-13: focused test target `ArtifactStillImageLayerRoundTripTest` を Debug でビルド。前提依存 `ArtifactAppRuntime` に既存未コミットエラーがあったため、ユーザー承認の範囲で Artifact 子リポジトリの2箇所を修正した。`ArtifactCompositionEditor.cpp` はラムダ変数を値型推論し、`QWidgetAction` の型を使用。`AppMain.cpp` は `ArtifactCompositionEditor` を `Artifact::` 修飾した。既存ソース中で欠落していた CompositionEditor imports と capture handler block も復元し、テストをリンク可能にした。編集前にあった Artifact の未コミット差分は保持。ビルドログ: `temp/source-crop-focused-build.log`。PATH上で選ばれる `rc.exe` はmanifest処理に失敗するため、Windows SDK 10.0.26100.0 x64 `rc.exe` をPATH先頭に置くとリンク成功した。

MSVC Debug STL assertionの切り分け: `setFromQImage` を使う基本的なRGB入力テストも同じ `xtree(192)` iterator assertionで停止。Crop画素ケースは、一時PNGへ保存して `loadFromPath` する通常の静止画ソース経路へ変更し、デコード完了を待つことで実画素判定が可能になった。これは検証対象に近い経路であり、in-memory QImage setterの別問題を避ける。判定ケースにはsource imageごとに一時ディレクトリを作り、project rootを分離した。

数値・画素テスト実行 (`temp/source-crop-focused-tests.log`):
- PASS `SourceCropZoomAndPanDefineEffectiveSourceRect`: crop=(2,2,8,6), zoom=2, pan=(1,-0.5) → effective source rect=(5,3,4,3)、式の期待値と一致。
- PASS `ActiveCropLeavesPixelsOutsideCropTransparent` と `ActiveCropSelectsTheRequestedSourcePixels`: cropサイズ設定後に位置を指定した通常のPNG入力で、選択画素と透明領域が期待通り。
- PASS `ActiveCropPanAndZoomSelectExpectedPixelsInCpuOutput`: 8×8の非対称座標色画像、crop=(2,2,4,4)、zoom=2、pan=(1,1) → effective rect=(4,4,2,2)。出力は同じsource座標の2×2領域だけ不透明でsource色と完全一致し、他画素はalpha=0。
- PASS `SourceCropNumericFixtureScenariosMatchSourcePixels`: 生成した512×384 coordinate-grid PNGとexpected JSONを直接読み込み。neutral、center zoom=2、pan=(+32,+24)、pan=(-32,-24)、explicit cropの全5シナリオでmanifest矩形と`sourceCropDrawLayout().sourcePixelRect`が一致。各シナリオで全196,608画素を検査し、矩形内RGBAはsourceと完全一致、外側alphaは0。テストログ: `temp/source-crop-numeric-fixture-test.log`。
- PASS `CropPanClampsAndIgnoresNonFiniteUpdates`、`CropPropertiesSanitizeNonFiniteAndDegenerateValues`。
- PASS `CropRectangleClampsToSourceEdges` after the controller-order fix: controller request=(3,2,2,2) on 4×3 source resolves to source rect=(2,1,2,2), and source pixels / transparent boundary are correct.
- PASS `ActiveCropSurvivesJsonRestoreAndStillSelectsSourcePixels`: fixture is composed in dimensions→position order, then saved/restored; crop state and selected pixels match.

境界FAILの根本原因はテストだけの順序ではなかった。`CompositionRenderController::setSelectedCropRect()` が実際に enabled→x→y→width→height と設定し、画像レイヤーsetterが各変更後に `clampToSource()` を実行していたため、右下境界付近で指定位置を失っていた。ユーザー依頼を受け、controllerの適用順とUndo登録失敗時のrollback順を両方とも enabled→width→height→x→y に変更した。Undo commandはbefore/afterの`SourceCrop` snapshotを`restoreSourceCropSnapshot()`で原子的に戻すため、command自体は変更不要。回帰テスト`CropRectangleClampsToSourceEdges`は実controllerを通して要求rect=(3,2,2,2)を適用し、有効rect=(2,1,2,2)と画素境界を確認する。JSON復元test fixtureも寸法→位置順でcropを組み立てる。

追加で実行した既存ケース: `ActiveCropAndPanZoomDoNotChangeLocalBounds` はPASS。`PixelAspectChangesDisplayWidthUnlessFitToLayer` と `SourceCropRotationKeepsAnchorPivotFixed` はFAILしたが、今回の中心対象であるsource crop/panの画素一致とは別のpixel-aspect / rotation契約であり、別途調査が必要。CPU `toQImage()` の結果であってComposition GPU PreviewやRender Queueの出力ではないため、それらは未検証。

**今回のCrop/Pan画素合否:** focused suite 10/10 PASS。8×8座標色画像と512×384生成gridの5シナリオで、crop内のsource pixelsはCPU互換出力でpixel-exact、外側は透明。CompositionRenderControllerの右下境界要求もrect=(2,1,2,2)に正しく収まる。Undo commandはsnapshotベースで、更新順修正に伴うcommand内部の変更は不要。実Composition GPU Preview capture、Render Queue比較、GPU/backend差、alpha edgeの別背景合成、性能は未検証。

次の段階: 実Composition Preview/Render Queueへ512×384 fixtureを通してsource landmarkを測定する。Inspectorの個別数値編集順も確認し、GPU/backend間の経路差、alpha edge、性能は別途評価する。
