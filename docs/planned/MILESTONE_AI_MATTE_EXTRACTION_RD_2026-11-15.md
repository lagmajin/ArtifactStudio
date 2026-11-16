# AI マット抽出（静止画レイヤー化）R&D

**最終更新:** 2026-11-15
**ステータス:** In Progress（R0 / rd0、R1 / rd1・rd3・rd4、R2 の併用経路 / rd2・後処理 / rd5 を実行済み。解像度依存・spill・実写人物 fixture は未実施）
**マイルストン ID:** M-IMG-MATTE-RD-1
**ラボ:** `experiments/MatteLab/`

## 目的

静止画 1 枚から前景の alpha matte を取り出す品質を、再現できる fixture と数値で評価する。髪・獣毛・細線・半透明（ガラス・煙）・ソフトシャドウのような、GrabCut 系の二値マスクが破綻しやすい境界を主対象に、classical baseline と学習型マットモデルの差を切り分ける。

本 R&D は方式選定の材料を出すものであり、製品実装・既定値・UI・Roto Brush の動作変更を承認しない。

## なぜ今か

- 開発優先方針（`AGENTS.md`）で静止画レイヤーの完成度が上位。`M-IMG-LAYER-1`（単一画像レイヤー化, Not Started）が「前景候補をマスク付きレイヤーへ変換する」段階で止まっている。
- 現行の Roto Brush は `ArtifactCore::OpenCVRotoBrushEngine` の GrabCut 初期セグメンテーション + optical flow 伝播で、出力は実質二値マスクである。境界の軟らかさ・半透明は表現できない。
- `ArtifactCore/src/AI/OnnxImageSegmenter.cpp` に ONNX + DirectML のセグメンテーション推論基盤（マスクのバイリニアサンプリングと sigmoid）が既にあり、新規ネイティブ依存なしで学習型マットを検証できる。
- ラボ側にはマット品質を測る fixture と指標が無い。先に測れる状態を作る。

## 現行実装の確認済み範囲

- `ArtifactCore::OpenCVRotoBrushEngine` は `cv::grabCut` を前景 bounds 付きで実行し、`propagateToNextFrame` で optical flow によりマスクを次フレームへ伝播する。`currentMask()` はマスクのポインタを返す。ヘッダコメントにも「GrabCutによる初期セグメンテーションと、OpticalFlowによるフレーム間のマスク伝播」と明記されている。
- `ArtifactCompositionRenderController` は `RotoBrush` ツールからこのエンジンを呼び、`propagateRotoBrushToCurrentFrame()` / `undoSelectedPaintStroke()` を持つ。
- `ArtifactCore::OnnxImageSegmenter` は `ARTIFACT_HAS_ONNX_DML_BACKEND` 有効時に ONNX Runtime + DirectML で推論し、入力画像のバイリニアサンプリング、sigmoid、マスクのバイリニア再サンプリングを行う。
- これらはコード上の確認であり、実素材でのマット品質・半透明・髪の再現性は未検証である。

## 対象 / 非対象

| 区分 | 内容 |
|---|---|
| 対象 | 静止画 1 枚、8-bit sRGB、GT alpha が既知の合成 fixture、classical baseline（GrabCut rect / trimap oracle / chroma key / Otsu）、学習型マット候補、境界・半透明・細線・ソフトシャドウ、複数背景合成、runtime |
| 対象外 | 動画・フレーム間伝播、trimap の自動生成 UI、Roto Brush の操作 UX、製品 GPU 経路、OCIO / HDR、解像度プリセット、DirectML 推論、VRAM 計測、実素材の主観品質 |

対象外は未検証として記録し、合否の根拠にしない。

## 進め方

| 段階 | 内容 | 状態 |
|---|---|---|
| R0 | fixture 生成、classical baseline 計測、ゲート確定、証跡整備（rd0） | 実行済み（結果は `captures/rd0/`） |
| R1 | 学習モデル（RMBG / MODNet / BiRefNet 系）の比較（rd1 / rd3 / rd4） | 一部実行（U2Net `captures/rd1/`、MODNet `captures/rd3/`、BiRefNet 512 `captures/rd4/`。RMBG 系はライセンス上見送り） |
| R2 | 実写合成・解像度依存・境界帯・color spill・性能（rd2 / rd5） | 一部実行（併用経路 rd2、guided refinement rd5。解像度依存・spill・実写拡張は未実施） |
| R3 | 採用／不採用判断と後続実装の受入条件（rd4） | 未着手 |

詳細は `experiments/MatteLab/EVALUATION_PLAN.md` を正とする。

## ゲート概要

- SAD / MSE / Gradient / Connectivity（Composition-1k 系の正規化）で baseline を上回る
- 境界帯（α=0.5 近傍）の MAE が基準以下
- 半透明領域（0.05<α<0.95）の MAE が基準以下
- 複数背景合成の誤差が `grabcut_rect` 比で悪化しない（band mean +1.0 code 以内、band max 1.25 倍以内。rd0 で改訂、承認済み）
- 二値性: GT 部分 α 画素（1/255<α<254/255）のうち ±0.1 以内で回収できる画素率が 0.50 以上（二値マスクはここで落ちる。rd0 baseline は 0.07〜0.19）
- runtime は記述のみ（製品予算を主張しない）

## R0（rd0）実測結果

8 fixture × 4 baseline（`grabcut_rect` / `grabcut_gt_trimap` / `chroma_key` / `otsu_luma`）。全数値は `experiments/MatteLab/captures/rd0/metrics.json`。

- 二値マスク系は部分 α を返さず（部分 α 率 0.000）、境界帯 MAE は `hair_strands` 0.672、`soft_shadow` 0.459、`glass_panel` 0.623。GT trimap oracle でも 0.091 / 0.130 / 0.331 で、事前登録ゲート 0.05 に届かない。
- 半透明 MAE は baseline で 0.29〜0.64。α の連続値を出力する手法が無いことの直接的な証拠。
- GrabCut 矩形初期化は細線主体で破綻する（`hair_strands` 予測 α 平均 0.480 vs GT 0.043、SAD 118.38）。塊状の `disc_aa` / `fur_edge` では SAD 0.28 / 1.16 と良好で、失敗は素材依存。
- chroma key は `green_screen` でのみ強い（SAD 4.17、最良の非 oracle。oracle 3.46）。テクスチャ背景では空マスク相当になり、適用範囲外であることを示す。この確認のため `green_screen` fixture を rd0 で追加した。
- コストは GrabCut が 512×512 で 1.3〜7.6 s、640×640 で 9.1 s。chroma / Otsu は 1.3〜13 ms。GrabCut は RNG を呼び出しごとに固定し、再実行で指標が一致する（timing のみ変動）。
- ゲートの不備 3 点（軟らかさ比の分母が 0、合成誤差に絶対値なし、SAD 40% が二値/軟らかい差を含む）は 2026-11-15 のユーザー承認で改訂済み。前後値は `gates.json` の `gate_revision`。SAD はゲートを維持し、境界帯 MAE と併記して判断する。

## R1（rd1）実測結果

8 fixture × 5 候補。学習型は U2Net（rembg 配布 `u2net.onnx`、SHA-256 `8d10d2f3…b491`、320×320 固定入力、CPU 1 スレッド）。ゲート判定は `captures/rd1/gates.json` の `verdicts.u2net`。

- **合格 0 / 8 fixture。** 不合格内訳は境界帯 MAE 8/8、半透明 MAE 8/8、SAD 40% 改善 3/8、gradient 2/8、connectivity 3/8、合成誤差 2/8、部分 α 回収 2/2（`hair_strands` / `fur_edge` に適用）。
- **SAD 改善の大半は「空マスク」による見かけの改善。** 予測 α 平均は `hair_strands` 0.015（GT 0.043）、`thin_wire` 0.011（GT 0.078）、`photo_composite` 0.013（GT 0.346）。前景を取り逃して 0 に寄った結果で、境界帯 MAE は 0.10〜0.38 に留まる。部分 α 回収率も 0.29 / 0.21 でゲート 0.50 に届かない。
- **機能した箇所:** `soft_shadow`（予測平均 0.163 対 GT 0.199、灰色背景の合成誤差 mean 27.6 → 8.4）、`green_screen`（4.83）、`disc_aa` / `fur_edge`（予測平均が GT にほぼ一致）。
- **GrabCut が既に良い画では悪化する。** `disc_aa` は SAD 0.28 → 0.94、`fur_edge` は 1.16 → 1.26。saliency モデルは塊状被写体の上積みにならない。
- **半透明は改善しない。** `glass_panel` の半透明 MAE 0.607、予測平均 0.272（GT 0.105）。不透明寄りの二値分離であることが変わらない。
- コストは 0.92〜0.98 s / 枚で GrabCut（1.2〜9.1 s）より速く、再推論で α が完全一致（決定論）。
- 320×320 固定入力のため 512×512 fixture では境界が resample でぼける。境界帯 MAE の下限が入力解像度に律速されている可能性は未検証。

**rd2 の候補（未実施）:** (a) matte 専用モデル（MODNet / BiRefNet / RMBG 系）との比較、(b) 入力解像度を上げた U2Net 変種、(c) saliency 出力を trimap として GrabCut に渡す併用経路。

## R2（rd2）実測結果（併用経路のみ）

`--derive-trimap` で追加した `u2net_trimap_grabcut`（二値）と `u2net_trimap_soft`（未知帯のみモデル α）を 8 fixture で測定。全数値は `captures/rd2/metrics.json`、`trimap_stats` に確実前景率と未知帯率を記録。

- **モデルが前景を出している画では GrabCut 併用が退行を戻す。** `disc_aa` は SAD 0.94 → 0.34、境界帯 MAE 0.079 → 0.029（ゲート 0.05 を満たす唯一の例）。`fur_edge` は soft 版で SAD 1.26 → 0.92、境界帯 0.092 → 0.068。
- **モデルが空マスクを返す画では併用経路も空のまま。** 確実前景率は `hair_strands` 0.000、`thin_wire` 0.0001、`photo_composite` 0.003 で、GrabCut に前景シードが無い。SAD は U2Net 単体とほぼ同じで改善ではない（退化の継承）。
- **未知帯にモデル α を残す効果はほぼ無い。** U2Net 出力は飽和しており未知帯は全画素の 0.8〜1.9%。soft 版と二値版の指標は `disc_aa` 以外ほぼ一致した。
- ゲート合格は 3 候補とも 0/8。半透明 MAE は 8/8 不合格（`glass_panel` 0.567）で、trimap を改善しても連続 α を出す機構が無い。
- 実行時間は 1.0〜1.3 s / 枚（U2Net 単体 0.85〜0.88 s）。
- **未実施:** 実写 fixture の拡張、解像度依存、color spill の分離測定、前景面積ガードの実装と検証。

## R1 追加モデル（rd3）実測結果: MODNet

`captures/rd3/`（u2net と modnet を同一実行で比較）。MODNet は 512×512 動的入力、0.30〜0.33 s / 枚、Apache-2.0 タグの HF 変換版（`MODELS.md`）。

- **`disc_aa` で事前登録ゲートのうち境界帯 MAE（0.017 ≤ 0.05）と半透明 MAE（0.075 ≤ 0.08）を、このラボで初めて満たした。** 部分 α 回収も 0.693（0.50 以上）。SAD 0.28 → 0.21（27% 改善）で SAD ゲートのみ未達、connectivity も 0.022 → 0.050 で未達。
- **人物ドメインに閉じている。** `hair_strands` / `soft_shadow` の予測 α 平均は 0.000、`green_screen` 0.076。`fur_edge`（SAD 1.16 → 6.90）と `green_screen`（19.15 → 38.45）は GrabCut より悪化する。
- 半透明板 `glass_panel` は半透明 MAE 0.614 で、依然として不可。
- ゲート合格は u2net 0/8、modnet 0/8。単一モデルでは合格 fixture が出ない主因は SAD 40% 改善、髪・獣毛の部分 α 回収、半透明の 3 点。
- **未実施:** 人物・実写 fixture（MODNet 本来のドメイン）、BiRefNet / RMBG 系、解像度依存、spill の分離測定。

## R1 追加モデル（rd4）実測結果: BiRefNet 512

`captures/rd4/`（u2net / modnet / birefnet を同一実行）。BiRefNet は `BiRefNet_512x512-ONNX`（swin-large、940 MB、MIT、512×512 固定）、0.63〜0.70 s / 枚。

- **3 モデル中もっとも広く効く。** SAD は 7/8 で `grabcut_rect` 比 40% 以上改善（唯一の例外は `photo_composite` の 28%）。`disc_aa` SAD 0.13、`fur_edge` 0.68、`thin_wire` 7.21、`hair_strands` 9.69。
- **境界帯 MAE ゲート 0.05 を 2 fixture で満たす。** `disc_aa` 0.011、`fur_edge` 0.0496。他は 0.082〜0.514。
- **軟らかさは改善するが目標未達。** 部分 α 回収は `disc_aa` 0.546、`fur_edge` 0.358、`hair_strands` 0.345。髪・獣毛の 0.50 は満たさない。
- **半透明は全モデル不可。** `glass_panel` 0.552、`photo_composite` 0.379（ゲート 0.08）。GT trimap を与えた GrabCut oracle でも `glass_panel` 0.331 であり、この fixture とゲート値の組み合わせは現状の手法では到達しにくい。
- 3 モデルとも fixture 合格は 0/8。BiRefNet は SAD と境界帯の 2 ゲートを半数の fixture で満たすが、半透明と部分 α 回収で落ちる。
- **未実施:** 1024 入力版、fp16 版、実写人物、半透明専用モデル、解像度依存。

## R1 追加モデル（rd5）実測結果: guided filter refinement

`--refine` で `<model>_refined`（guided filter、guide=入力画像、radius 2、eps 1e-4）を 3 モデル分追加した（`captures/rd5/`）。

- **低解像度モデルのぼけは refinement で回復する。** U2Net（320 入力）の `disc_aa` は SAD 0.94 → 0.34、境界帯 MAE 0.079 → 0.027。raw の劣化が予測ミスではなく拡大ぼけだったことを示す。
- **高解像度モデルでは一様に効かない。** BiRefNet は `hair_strands`（SAD 9.69 → 7.46、部分 α 回収 0.345 → 0.464）、`thin_wire`（7.21 → 5.11）、`green_screen`（3.11 → 2.24）で改善するが、`disc_aa`（0.13 → 0.19）と `fur_edge`（0.68 → 0.78）で既に正しい境界を壊す。SAD ゲート不合格は 1 → 3 に増加。
- **半透明は不変。** `glass_panel` は raw / refined とも境界帯 0.514。
- eps（1e-6 / 1e-4 / 1e-2）は 0〜1 スケールで差が出ず、radius は 2 が最良（8 では劣化）。
- ゲート合格は 6 候補すべて 0/8。refinement は合格 fixture を増やさないが、raw より良い方向の fixture は U2Net 5/8、BiRefNet 3/8。
- **未実施:** raw / refined を選ぶ分岐、エッジ活動量による選択基準。

## リスクと未確認事項

- 合成 fixture は GT が厳密だが、実写の髪・ガラス・煙の分布を代表しない。
- 学習モデルは U2Net 1 本のみ。RMBG / MODNet / BiRefNet は未入手で、「学習型一般」の結論ではない。
- U2Net の重みライセンス条項（商用可否・再配布条件）は配布元 README の記載に留まり、法的確認をしていない（`experiments/MatteLab/MODELS.md`）。
- 実素材の GT matte は人件費が大きく、本 R&D では未整備。
- GrabCut は入力サイズと反復回数で結果が変わる。設定を固定して記録するが、製品設定と同一ではない。

## 関連資料

- `experiments/MatteLab/README.md`
- `experiments/MatteLab/EVALUATION_PLAN.md`
- `experiments/MatteLab/MODELS.md`
- `docs/planned/MILESTONE_SINGLE_IMAGE_LAYERIZATION_2026-08-17.md`
- `docs/technical/IMAGE_EFFECT_RND_EVALUATION_RULES.md`