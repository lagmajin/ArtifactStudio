# 新規画像R&D 優先候補ポートフォリオ

**最終更新:** 2026-11-16
**ステータス:** In Progress（指定9候補の既存実装を棚卸し。共通fixture suiteとobject removal初期計測を作成。残りの候補別比較は未完了）

## 目的

ユーザーが後から製品投資先を選ぶため、候補1–5、10–11、13–14の初期実装／実験資産を一通り揃える。ここでいう初期実装は、再現可能な評価の入口、既存処理への接続根拠、fixtureまたはreference modelを作ること。製品機能の完了・推奨順位・品質合格を意味しない。

画像評価の開始条件は [`../technical/IMAGE_EFFECT_RND_EVALUATION_RULES.md`](../technical/IMAGE_EFFECT_RND_EVALUATION_RULES.md)。ビルド、CMake、製品テスト、製品UI/runtime captureは現行プロジェクト規則に従い、明示指示がない限り実行しない。

## 指定された着手順と現状

|順|案|既存根拠／初期資産|不足している初期成果|
|---:|---|---|---|
|1|不要物除去|Core `WireRemove`、Telea/NS、CPU/GPU effectあり。`experiments/ObjectRemovalLab/` に4 synthetic fixture、baseline metrics|candidateごとの同条件出力・visual review・gate|
|2|画像自動修復|OpenCV inpaintと物体除去基盤はあるが、傷/埃向け復元評価は未確認。共通suiteにdamage truth fixture|damage専用候補比較、実写fixture|
|3|モアレ除去|専用の製品経路は今回の棚卸しで未確認。共通suiteに周期干渉fixture|周波数候補比較、細部損失分析|
|4|色収差補正|Core `ChromaSpread`、RetroImageLab chromatic studies|レンズ半径由来CAとCoreの式/境界/alpha契約の対比|
|5|レンズ歪曲補正|Core `makeOpticsCompensation`|逆写像referenceと製品式の幾何誤差・端crop比較|
|6|画像間色合わせ|Core `AutoColorMatcher` Reinhard/MeanStddev/Histogram|肌・白保護、空間領域、色domain別の共通比較|
|7|画像から配色抽出|Core `SmartPaletteAnalyzer` K-means、RetroImageLab alpha-weighted median-cut|Core抽出結果のseed再現性、透明重み、palette coverage比較|
|8|ディザリング|RetroImageLab ordered Bayer、Floyd–Steinbergと長期研究記録あり|portfolio用要約と、必要なら製品実装のsource-contract差分|
|9|ドット絵化|RetroImageLab pixelation/palette/alpha resize研究あり|端から端の品質比較票と候補まとめ。新規式は不要の可能性|

順位は作業順であり、実装採用の優先度ではない。先行案の結果待ちで後続案を止めず、各テーマの初期準備を並行してよい。

## 共通fixture suite

[`../../experiments/ImageCorrectionSuite/README.md`](../../experiments/ImageCorrectionSuite/README.md) と `captures/rd0/` は、seed 20261116・512×320のlinear RGB fixtureを生成する。入力/正解チャートはrepair damage、周期moire、chromatic channel shift、barrel/pincushion、色合わせsource/reference、palette area chartを含む。manifestにはgenerator hashとfixture hashを保存する。

rd0はfixture生成だけであり、候補出力、品質指標の比較、gate、実写、人手視覚判定、製品runtime、性能は未実施。共通suiteは最終アルゴリズムの代理や製品式のmirrorではない。

## 初期実装の受入

- 9案それぞれに既存コード／研究資産と未検証点が記載される。
- 各案の比較入力・正解またはbaseline・主要ROIを生成可能にする。正解が定義できない場合はその旨を記録し、応答測定を品質合格と混同しない。
- 各fixtureの条件とhashを記録し、繰り返し生成で一致を確認する。
- 既存の研究資産を再利用し、同じ機能を別ラボに重複実装しない。
- 目視品質と数値gateは候補を比べる前に決める。gate未設定なら結果は未判定のままにする。
