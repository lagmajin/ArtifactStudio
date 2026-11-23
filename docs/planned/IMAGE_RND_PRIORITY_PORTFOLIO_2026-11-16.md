# 新規画像R&D 優先候補ポートフォリオ

**最終更新:** 2026-11-16
**ステータス:** 初期一巡完了（指定9候補の既存実装監査、offline reference・既存studyの代表実行とhash記録を用意。品質 gate・製品受入は未実施）

## 目的

ユーザーが後から製品投資先を選ぶため、候補1–5、10–11、13–14の初期実装／実験資産を一通り揃える。ここでいう初期実装は、再現可能な評価の入口、既存処理への接続根拠、fixtureまたはreference modelを作ること。製品機能の完了・推奨順位・品質合格を意味しない。

画像評価の開始条件は [`../technical/IMAGE_EFFECT_RND_EVALUATION_RULES.md`](../technical/IMAGE_EFFECT_RND_EVALUATION_RULES.md)。ビルド、CMake、製品テスト、製品UI/runtime captureは現行プロジェクト規則に従い、明示指示がない限り実行しない。

## 指定された着手順と現状

|順|案|既存根拠／初期資産|残る検証課題|
|---:|---|---|---|
|1|不要物除去|Core `WireRemove`、Telea/NS、CPU/GPU effectあり。ObjectRemovalLabに4 truth fixtureとbaseline metrics|候補別出力・visual review・gate|
|2|画像自動修復|OpenCV inpaint基盤あり。共通suiteにscratch/dust truth、median 3/7/13 reference比較。median13のマスク内MAE 0.0207だがマスク外変更あり|傷種類の拡張、実写fixture、境界副作用を扱う候補検討|
|3|モアレ除去|共通suiteにperiodic interferenceと3低域reference比較。合成入力ではMAE 0.00685〜0.01241|実写moire、細部保持評価、artifact視覚判定|
|4|色収差補正|Core `ChromaSpread`、RetroImageLabのCore integer-shift角度解析とCC0 alpha境界比較あり|Core `ChromaSpread` との式対応、radial lens CAの実写/製品経路|
|5|レンズ歪曲補正|Core `makeOpticsCompensation`。known radial coefficientのbarrel/pincushion fixture・inverse referenceあり|grid-line誤差を直接測る指標、Core mapping一致と端crop評価|
|10|画像間色合わせ|Core `AutoColorMatcher` 3方式。共通suite source/reference chartとchannel mean/std referenceあり。chart上の誤差低下を記録|Lab/Reinhard・HistogramのCore比較、肌/白保護ROI、実写評価|
|11|画像から配色を抽出|Core `SmartPaletteAnalyzer` K-means、RetroImageLabのCC0 3素材・4/8/16色・頻度/alpha重み・sRGB/linear median-cut比較を実行|Core実装と研究用median-cutの同条件出力比較|
|13|ディザリング|RetroImageLabのBayer4/8・Floyd–Steinberg、空間/時間位相studyを実行|Artifact製品式とのsource/runtime比較|
|14|ドット絵化|RetroImageLabにpixel-art hair atlas、alpha edge、crop/resize phase比較を実行|既存処理を束ねたend-to-end preset評価。必要性は未確定|

順位は作業順であり、実装採用の優先度ではない。先行案の結果待ちで後続案を止めず、各テーマの初期準備を並行してよい。

## 共通fixture suite

[`../../experiments/ImageCorrectionSuite/README.md`](../../experiments/ImageCorrectionSuite/README.md) と `captures/rd0/` は、seed 20261116・512×320のlinear RGB fixtureを生成する。入力/正解チャートはrepair damage、周期moire、chromatic channel shift、barrel/pincushion、色合わせsource/reference、palette area chartを含む。manifestにはgenerator hashとfixture hashを保存する。

`reference_metrics.json` は候補2・3・5・10のoffline参照出力を記録する。候補4はsuite内でfixtureのみだが、別途RetroImageLabの角度・alpha studyを実行した。RetroImageLabには候補4・11・13・14の既存研究があり、新規ラボでは重複実装せず再利用した。共通suiteは最終アルゴリズムや製品式のmirrorではない。品質gate、実写、人手視覚判定、製品runtime、性能は未実施。

## 既存RetroImageLab研究の選定用抜粋

以下は既存研究記録の短い要約であり、このポートフォリオの新規測定ではない。平均誤差や応答値を好み・視覚品質の勝者として扱わない。

- **4 色収差:** `experiments/RetroImageLab/tools/core_chromatic_angle_study.py` はCore integer-angle shiftのamount 0–50px、角度1°刻み、clamp、hidden RGB、alphaを解析する。`RESEARCH_NOTES.md` の2026-11-11記録には、CC0素材で4方式×2/4/8pxを比較し、silver featherのpartial-alpha fallbackでfringeと見た目が変わる応答を記録。推奨可否は未判定。
- **11 配色抽出:** `tools/adaptive_palette_weighting_study.py` はCC0 clover、silver feather、pixel-art hairの3系統で4/8/16色、frequency/alpha coverage重み、sRGB/linear median-cutを比較。cloverの16色linearでalpha weightingがsource error 0.0234から0.0269へ増えた例がある。これは一素材の指標結果で一般優劣ではない。
- **13 ディザリング:** `RESEARCH_NOTES.md` 実験019は8-levelのno-dither、Bayer 4×4/8×8、Floyd–SteinbergをsRGB/linearで比較。時間studyではscreen-locked BayerとFloyd–Steinbergにtranslation後の模様残差が出る一方、content-locked Bayerは整数移動fixtureで共変となった。合否gate・製品性能は未評価。
- **14 ドット絵化:** `--alpha-pixel-art-crop-study` と `tools/adaptive_palette_resize_phase_study.py` がalpha付きsprite atlasのcrop、縮小拡大、tile位相を比較。edge composite、hidden RGB、複数背景の方法と制限を記録。end-to-end presetと独立目視評価は別課題。

今回の代表実行と生成物SHA-256は [`priority-rd-initial-runs.json`](../../experiments/RetroImageLab/captures/priority-rd-initial-runs.json) に記録した。候補4/11/13/14のstudyを実行し、候補13のtemporal dither runnerでkeyword引数の誤渡しを修正した。修正後、整数translation fixtureでcontent-locked Bayerの整列MAEは0、screen-locked Bayerは約0.0285〜0.0290、Floyd–Steinbergは約0.0345だった。これは座標位相の挙動で、見た目の優劣や製品契約の承認ではない。

## 初期実装の受入

- 9案それぞれに既存コード／研究資産と未検証点が記載される。**満たした。**
- 共通suiteで候補2・3・5・10を比較し、候補4・11・13・14の代表studyを実行、候補1には独立labのbaselineを用意した。**全案に選定前の初期証拠があるが、比較深度は均一でない。**
- fixture条件とhashをmanifestへ記録し、共通suiteの再実行でfixture hash一致を確認。RetroImageLabの生成物hashも一覧化した。
- 既存の研究資産を再利用し、同じ機能を別ラボに重複実装しない。**満たした。**
- 目視品質と数値gateは候補を比べる前に決める。gate未設定なら結果は未判定のままにする。
