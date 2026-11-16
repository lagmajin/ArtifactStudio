# 新規画像R&D 優先候補ポートフォリオ

**最終更新:** 2026-11-16
**ステータス:** 初期一巡完了（指定9候補の既存実装監査、4テーマのoffline reference比較、5テーマの既存研究接続票を用意。品質 gate・製品受入は未実施）

## 目的

ユーザーが後から製品投資先を選ぶため、候補1–5、10–11、13–14の初期実装／実験資産を一通り揃える。ここでいう初期実装は、再現可能な評価の入口、既存処理への接続根拠、fixtureまたはreference modelを作ること。製品機能の完了・推奨順位・品質合格を意味しない。

画像評価の開始条件は [`../technical/IMAGE_EFFECT_RND_EVALUATION_RULES.md`](../technical/IMAGE_EFFECT_RND_EVALUATION_RULES.md)。ビルド、CMake、製品テスト、製品UI/runtime captureは現行プロジェクト規則に従い、明示指示がない限り実行しない。

## 指定された着手順と現状

|順|案|既存根拠／初期資産|不足している初期成果|
|---:|---|---|---|
|1|不要物除去|Core `WireRemove`、Telea/NS、CPU/GPU effectあり。ObjectRemovalLabに4 truth fixtureとbaseline metrics|候補別出力・visual review・gate|
|2|画像自動修復|OpenCV inpaint基盤あり。共通suiteにscratch/dust truth、median 3/7/13 reference比較。median13のマスク内MAE 0.0207だがマスク外変更あり|傷種類の拡張、実写fixture、境界副作用を扱う候補検討|
|3|モアレ除去|共通suiteにperiodic interferenceと3低域reference比較。合成入力ではMAE 0.00685〜0.01241|実写moire、細部保持評価、artifact視覚判定|
|4|色収差補正|Core `ChromaSpread` とRetroImageLab studies。共通suiteの4px radial RGB shift fixtureあり|共通suite内のcorrector reference未作成。Core実装との式/境界/alpha対比|
|5|レンズ歪曲補正|Core `makeOpticsCompensation`。known radial coefficientのbarrel/pincushion fixture・inverse referenceあり|grid-line誤差を直接測る指標、Core mapping一致と端crop評価|
|6|画像間色合わせ|Core `AutoColorMatcher` 3方式。共通suite source/reference chartとchannel mean/std referenceあり。chart上の誤差低下を記録|Lab/Reinhard・HistogramのCore比較、肌/白保護ROI、実写評価|
|7|画像から配色抽出|Core `SmartPaletteAnalyzer` K-means、RetroImageLab alpha-weighted median-cut研究。known color-area fixtureあり|fixture上でCore/既存候補出力を並べる集約比較|
|8|ディザリング|RetroImageLab Bayer/Floyd–Steinberg、domain/phase/temporal studiesあり|選定用の短い結果要約、必要時に製品式source audit|
|9|ドット絵化|RetroImageLab pixelation/palette/alpha resize studiesあり|既存結果を組み合わせた候補比較票・代表preview集約|

順位は作業順であり、実装採用の優先度ではない。先行案の結果待ちで後続案を止めず、各テーマの初期準備を並行してよい。

## 共通fixture suite

[`../../experiments/ImageCorrectionSuite/README.md`](../../experiments/ImageCorrectionSuite/README.md) と `captures/rd0/` は、seed 20261116・512×320のlinear RGB fixtureを生成する。入力/正解チャートはrepair damage、周期moire、chromatic channel shift、barrel/pincushion、色合わせsource/reference、palette area chartを含む。manifestにはgenerator hashとfixture hashを保存する。

`reference_metrics.json` は候補2・3・5・10のoffline参照出力を記録する。候補4はfixtureのみ。既存RetroImageLabに候補4・7・8・9を評価した先行実験があり、新規ラボでは重複実装せず導線を用意した。共通suiteは最終アルゴリズムや製品式のmirrorではない。品質gate、実写、人手視覚判定、製品runtime、性能は未実施。

## 初期実装の受入

- 9案それぞれに既存コード／研究資産と未検証点が記載される。**満たした。**
- 4案は共通suiteでoffline referenceを比較し、残る5案は既存研究または共通fixtureへの接続を用意した。**初期入口は揃ったが、候補比較の深さは均一でない。**
- fixture条件とhashをmanifestへ記録し、再実行したfixture hashの一致を確認する。
- 既存の研究資産を再利用し、同じ機能を別ラボに重複実装しない。**満たした。**
- 目視品質と数値gateは候補を比べる前に決める。gate未設定なら結果は未判定のままにする。
