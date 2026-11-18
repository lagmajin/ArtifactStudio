# 質感分離（マテリアル分離）R&D — 画像評価計画

**最終更新:** 2026-11-18
**ステータス:** In Progress（RD0: 決定論的 fixture 11 件と baseline 30 出力、RD1: 候補 5 種（A 自己二層分解 / B 周波数分離 / C 強調型）と記述的測定まで実施。**gate（合否閾値）は未設定**、人手レビュー・順位付け・製品経路は未実施）

## 0. 位置づけ

`docs/technical/IMAGE_EFFECT_RND_EVALUATION_RULES.md` に従い、画像品質を扱う R&D の開始前に登録する評価計画である。実装・製品採用・既定値変更・UI 追加を承認しない。ビルド、CMake、製品テスト、製品 UI／runtime capture はユーザーの明示指示まで行わない。

候補は `docs/planned/MILESTONE_NEW_IMAGE_RND_IDEAS_2026-11-16.md` の **18. 素材の質感分離 — 大まかな色と細かな質感を分け、それぞれを独立して調整する**（規模目安: 中）。着手前に重複監査を行い、結果を §7 に記録する。

---

## 1. 目的と対象

### 改善したい見た目

静止画を次の2層に分離し、それぞれを独立に調整できる状態を.offline で検証する。

- **base（色・トーンマップ）:** 低周波の広い色味と明度。緩やかな色味・階調を主に持つ層。
- **detail（質感・テクスチャ）:** 細かな質感とディテール。ミクロコントラスト・粒状感・微細な明暗の名前を持つ層。

「分離が正しい」ことの物理的正解は主張しない。評価は (a) 再合成の可逆性、(b) 中立設定での入力再現、(c) 記述的な応答統計と目視の相対比較であり、実作品や特定アルゴリズムとの一致を合格条件にしない。

### 対象素材・操作・経路

- **素材:** 決定論的 fixture（opaque／alpha 付き）。実写写真は現時点で**未取得**。
- **操作:** オフライン Python / NumPy / OpenCV の reference model。学習モデルは使わない。パラメータは `base_gain` / `detail_gain`（モデル C では `coarse_gain` / `fine_gain`）、分解半径・sigma、seed。
- **経路:** オフライン fixture のみ。Artifact の Composition、Render Queue、preview／final、GPU（D3D12 / Vulkan）は対象外・未検証。

### 対象外（未検証として明記）

- 学習型 texture / material 分解（ONNX / DirectML）
- 製品 effect 登録、Inspector プロパティ、既定値、Undo、project 保存／再読込
- 実アプリの画像到達、CPU／GPU parity、backend 差、性能 gate
- HDR（1.0 超）と OCIO 色空間（現段階は SDR / sRGB のみ）
- 素材 ID の割り当てやテクスチャ合成（材質ごとのアセット分離は本ラボの対象外）

---

## 2. 比較対象

| 種別 | 内容 | 正解として扱える根拠 |
|---|---|---|
| baseline | 無処理入力 | 現行出力の比較基準。本テーマ専用の既存 effect は現時点で存在しない（§7） |
| control 1 | 低域のみ（premultiplied 空間での gaussian blur、sigma 2.0） | 「质感分離 Without Separation」の対照。単一 Vincent 収差の実測ベース |
| control 2 | 高域のみ（入力 − 低域） | 質感のみを単独表示した対照 |
| 合成 ground truth | 既知の低域／高域パラメータで生成した fixture と既知領域 | 応答の sanity check にのみ使う。美的品質の合格証明には使わない |
| 実作品の目標画像 | 未取得 | 実作品を物理的正解としない。取得までは blind review を相対比較としてのみ扱う |
| 承認済み目標画像 | なし | — |

---

## 3. 評価素材

### 3.1 決定論的 fixture

seed `20261118`、解像度 512×320 と 1536×1024 の2段。生成手順・hash を `captures/rd0/fixtures_manifest.json` に保存し、`generate_fixtures.py --check` で再検証できる。

| fixture | 目的 | 失敗しやすい例 |
|---|---|---|
| 8 色パッチ + 灰色ランプ | 低域の色・階調の保存、中間調の連続性 | 中間調が階段化・反転する、色が飽和する |
| 細線チャート（1〜4px、複数角度） | 高域の保持、細線と低域の両立 | 細線が消える、二重線（halo） |
| Procedural 微細テクスチャ（多オクターブ value noise） | 質感の抽出と局所コントラスト保持 | 平坦部にもノイズが出る、texture が過度に強調される |
| 平坦部 + ハイライト | 平坦部の texture 混入、縁の halo、HDR スパ克的丸め | 平坦部の被视为 texture、境界の抜け |
| 半透明 alpha 境界（hidden RGB 含む） | alpha 境界での descriptor 契約、フリンジ | 黒／白フリンジ、hidden RGB の漏れ、NaN |
| 連続フレーム 8 枚（0.5px 平行移動） | 時間安定性（flicker / shimmer） | 質感が流れる、輝度が揺れる |

### 3.2 実写素材

- 人物の肌・髪、金属・木目などの**質感を持つ CC0 実写は現時点で未取得**。取得までは実写での品質判定を未検証とする。
- いかなる素材も無条件に正しいとは扱わない。

---

## 4. 画像条件

| 項目 | 固定／記録する条件 |
|---|---|
| 入力 | linear float（`ImageF32x4_RGBA` 相当）。fixture は straight alpha の linear float で生成し、PNG は sRGB OETF で encode |
| 設定 | `base_gain` / `detail_gain`（C は `coarse_gain` / `fine_gain`）を 0 / 1（中立）/ 強調。分解半径・sigma 固定。seed 固定 |
| 解像度 | 512×320 と 1536×1024 |
| 色空間・転送関数 | sRGB 8-bit と linear。表示変換は sRGB OETF 固定 |
| alpha | straight と premultiplied の両方を明示し、入出力契約をテストする |
| フレーム | 0〜7（時間安定性評価のみ） |
| 出力形式 | 目視用 PNG 8-bit と測定用 float32 `.npy` |
| 表示 | 差分画像の倍率・正規化を記録し、表示画像と測定元データを区別する |

**alpha 契約:** 水彩 R&D と鉛筆画 R&D の知見を引き継ぐ。近傍演算（blur / guided filter / 差分）は **premultiplied RGB と alpha を同じカーネル・同じグリッドで同時に処理**し、最後 unpremultiply は一度だけ行う。**unpremultiply の除数には層分解された alpha（recompose した層に対応するもの）を使う**。premul だけを層分解し alpha を未分解の入力で割ると近透明領域で除算が破綻する（RD1 の当初実装で straight RGB 最大値が 6〜8 まで跳ねた実績あり）。unpremultiply 時の除算は alpha ≤ 1e-3 の領域で行わず、RGB を 0 とする（hidden RGB を捨てる契約）。結果は 16.0 でクランプして無限大を防ぐ。descriptor 差が 0 でも絶対値が健全とは限らないため、可視領域と近透明領域の両方で RGB の最大値を記録する。

---

## 5. 評価項目と合否

### 5.0 閾値の確定手順（開始前の登録）

RD0 で baseline（無処理＋2 control）の full 画像と固定 ROI crop を取得した直後、**候補の最終比較より前に**、実装者とは別のレビュー者が各項目の許容差を数値で事前登録する。

- 対象: §5.1〜5.6 の合格条件欄と §5.7 の効果固有項目
- 方法: baseline と control の full／crop を同じ表示条件で見て、領域ごとに許容できる変化量を決定
- 記録: 決定日時、根拠、判定者、変更前後をこの文書へ追記
- ルール: 未記入項目は未評価とし、R&D 合格に含めない。評価後の変更は理由・前後・承認者・影響を記録し、必要な比較をやり直す
- 他 R&D の共通閾値を流用しない。PSNR / SSIM の固定値を単独の合格基準にしない
- **本文書は gate を設定しない。** §5.1〜5.7 の数値は記述的 baseline であり、品質合格・候補順位を意味しない

### 5.1 効果の正しさ

- **領域:** 全体、8 色パッチ、灰色ランプ、細線 ROI、テクスチャ ROI
- **手順:** (a) 全 gain = 1.0（中立）で入力との差分を測る。(b) `detail_gain` を 0 → 1 → 強調で、質感の振幅が単調に変化するか。(c) `base_gain` を動かしたとき色味・階調が独立性を持って変化するか
- **判定者:** 実装者＋別レビュー者
- **合格条件:** 中立設定で可視画素の float 差が §5.0 の許容内（8-bit 差 0 相当）。応答が単調
- **重大な欠陥:** 中立設定で画像が変わる、detail gain 増で応答が反転する、base 調整が detail 層まで dragging される

### 5.2 画質と細部

- **領域:** 全体、細線 ROI、平坦部 ROI、高コントラスト境界 ROI、テクスチャ ROI
- **手順:** baseline／control／候補の full と 1:1・拡大 crop を blind review。細線の連続性、平坦部の残留ノイズ、境界の halo を記録
- **判定者:** 別レビュー者（候補名を伏せて判定）
- **合格条件:** 細線が残存し、平坦部で texture が湧かない。許容差は §5.0 で固定
- **重大な欠陥:** 細線の消失、可視 halo、平坦部の砂状化

### 5.3 色と alpha

- **領域:** 8 色パッチ、半透明 alpha 境界、透明画素（hidden RGB）、ハイライト領域
- **手順:** straight／premultiplied 入力から復元した straight RGB の最大差を測る。黒／白／中灰の3背景へ合成した縁を比較。クリップ率と非有限値数を測る
- **判定者:** 実装者＋別レビュー者
- **合格条件:** 両 descriptor で復元 RGB 差が §5.0 の許容内（RD1 実測は 0）、近透明領域の RGB 最大値が §5.0 の許容内。3背景でフリンジなし。非有限値 0
- **重大な欠陥:** alpha が小さい領域で RGB が発散する、背景依存の縁色、NaN / inf

### 5.4 時間安定性

- **領域:** 固定領域、移動パッチ周辺
- **手順:** 0.5px 平行移動 8 フレームでフレーム間差を測り、意図的な内容変化と flicker を区別する
- **判定者:** 実装者
- **合格条件:** 動きに起因する変化を flicker と誤判定しない。許容差は §5.0 で固定
- **重大な欠陥:** 激しい flicker / shimmer、質感の流れる

### 5.5 経路と解像度

- **領域:** 512×320 と 1536×1024 の same-content 画像
- **手順:** 同じ seed・設定で2解像度を生成し、detail 振幅と低域の応答を比較する
- **判定者:** 実装者
- **対象外:** 製品 preview／final、D3D12／Vulkan 差（未検証として明記）
- **重大な欠陥:** 低解像度で質感が消える、高解像度でだけ段差が出る

### 5.6 性能

- **領域:** オフライン reference model の実行時間とメモリ
- **手順:** 2解像度で warm-up 後に複数回計測し、中央値と最大値を記録（`tracemalloc` peak は Python heap のみ）
- **判定者:** 実装者
- **合格条件:** オフライン探索が現実的な時間で回ること。製品の 1080p / 4K gate は未実施として記録
- **重大な欠陥:** fixture すら回らないほどの消費

### 5.7 効果固有の項目

| 項目 | 測定 | 領域 | 備考 |
|---|---|---|---|
| detail 層の振幅 | detail 層の abs mean / std / p50 / p95 / p99 / max、および base に対する相対量 | テクスチャ ROI、平坦部 ROI、境界 ROI | 振幅が 0 領域ではほぼ 0、質感領域では有限かつ非ゼロであることを記述する |
| detail→base 再合成の可逆性 | `base + detail` が premultiplied 入力から再構成できること（max abs delta） | 全体 | 浮動小数点丸めのみが許される範囲であることを確認する |
| 片方が 0 のときの契約 | `detail_gain = 0`（base のみ）と `base_gain = 0`（detail のみ）の出力で NaN / inf が出 없고、alpha ≤ 1e-4 で RGB が 0 になること | 全体、alpha 境界 ROI | halo は目視（§5.2）と gradient 差分プローブで併用する |
| 中間調の保存 | 灰色ランプの step mean / std / min / max / 単調 fraction、flat step 率 | 灰色ランプ ROI | 階調の階段化・反転を検出する |
| テクスチャの局所コントラスト保持 | 平坦部 luma std とテクスチャ部 luma std の比（入力 / base 層 / 出力の3点） | テクスチャ fixture の flat_region / texture_region | 平坦部への texture 混入と質感の消失を同時に検出する |

---

## 6. 画像統計による評価

- **画素値分布:** min / max / mean / median / 分位点 / ヒストグラム / クリップ率 / 非有限値数。HDR の 0〜1 丸めをしない
- **参照との差分:** 入力との MAE / RMSE / 最大値 / 高分位点 / 差分画像。実画像に正解がないため実画像との差分は参照差分として適用しない
- **構造と細部:** 勾配統計、局所コントラスト、層ごとの std。SSIM / PSNR は補助指標に留める
- **色:** 輝度・色差分布、チャンネル偏り、8 パッチの sRGB 値。評価空間と表示変換を明記
- **透明度・境界:** alpha 分布、可視／近透明領域の RGB 最大値、>1 チャンネル数、3背景合成後の差
- **ノイズ・時間:** 平坦部の分散、固定条件のフレーム間差
- **効果固有:** §5.7 の detail 振幅・再合成可逆性・中間調保存・局所コントラスト保持
- **領域:** 全体に加え色パッチ／灰色ランプ／平坦部／テクスチャ部／境界／高輝度部。座標・マスク・生成方法・画素数を保存
- 目視レビューと組み合わせ、指標の改善だけで品質向上を断定しない

---

## 7. 重複監査（着手前の確認）

- `experiments/DetailSharpeningLab/` はディテール強調（unsharp 系の増幅）を研究済み。本ラボは「増幅」ではなく「**低域と高域の分離**」と「両層の独立調整」を対象とする。ディテール強調の結果は比較対象であり、実装の重複を作らない
- `experiments/RetroImageLab/` はディザ・位相の研究。テクスチャ合成部的ノイズ（screen 固定）の再利用余地はあるが、分離処理とは別軸
- `experiments/WatercolorLab/` の premultiplied 空間処理と descriptor 契約の知見を引き継ぐ（RD3 / RD4 の「descriptor 差 0 でも絶対値の健全性は別問題」という知見）
- `experiments/PencilPrintLab/` の RD9/RD10 の測定方法的欠陥（参照の取り違え）の教訓を引き継ぎ、RD1 の descriptor 比較では「同じ処理を通した straight 入力」と「premultiplied 入力」を直接比較する
- `ArtifactCore/include/ImageProcessing/` には Blur / Sharpen 系が存在するが、質感分離専用の二層分解 API は確認できていない。既存Blur / Sharpen は比較対象として扱い、二層分解を製品コードへ追加しない
- 質感分離専用の既存ラボは確認できず、重複ラボは存在しない（未確定箇所は着手時に再確認）

---

## 8. 比較と判定

- 全体と評価領域の crop を同じ表示条件で目視確認する
- 数値比較は目的に合う指標と領域を選び、単位・閾値・集計方法を記録する。PSNR / SSIM や画素差分は補助指標であり単独で合格を証明しない
- 候補と baseline を同じ入力・条件で比較し、改善と新たな欠陥、性能・メモリへの影響を併記する
- 欠陥ごとに場所、再現条件、影響、重大度、証拠、対応状況を記録する。重大な欠陥や必須項目の未検証は合格扱いにしない
- **本時点（RD1 完了時点）では gate 未設定のため、合格判定・不合格判定・候補の順位付けは行わない**

---

## 9. 証跡と完了条件

入力・参照・出力への参照、取得手順、設定、環境、実行日、画像全体と crop、測定結果、判定者、既知の制限を保存する。数値契約テスト、画像評価、実アプリ経路、性能計測の証明範囲を区別する。

| RD | 内容 | 記録 |
|---|---|---|
| RD0 | 決定論的 fixture と baseline（無処理 + 低域のみ + 高域のみ） | `captures/rd0/`（`metrics_rd0.json` / `manifest_rd0.json` / `review/RD0_TABLES.md` / 空の gate 票） |
| RD1 | 候補 5 種の出力と記述的測定（中立性・descriptor 差・detail 振幅・再合成誤差・中間調・局所コントラスト・絶対値・時間） | `captures/rd1/`（`metrics_rd1.json` / `manifest_rd1.json` / `review/RD1_TABLES.md` / blind シート / 空の採点フォーム） |

完了には必須評価の結果、合否と根拠、未解決事項、採用／不採用判断、後続実装の受入条件を残す。未達や未検証はそのまま明示し、画質達成や製品統合完了と表現しない。**R&D の完了自体は製品実装の承認ではない。**

---

## 10. 実行条件

- 言語／依存: Python 3.14、NumPy 2.4.3、opencv-python-headless 4.13.0、Pillow。製品バイナリは起動しない
- GPU / backend: オフラインのみ。D3D12 / Vulkan は未検証
- 時間／メモリ予算: RD0 の baseline 取得後に実測し、§5.0 で gate を固定
- ビルド、CMake、製品テスト、実アプリ操作、Windows デスクトップ操作は明示指示まで禁止

---

## 11. 実行履歴（RD0〜RD1）

| RD | 内容 | 記録 |
|---|---|---|
| RD0 | 決定論的 fixture 11 件（512×320 と 1536×1024、alpha fixture の premultiplied 版を含む）と baseline 30 出力 | `captures/rd0/` |
| RD1 | 候補 5 種（A 自己二層分解 ×2 / B 周波数分離 / C 強調型 ×2）の出力と記述的測定 | `captures/rd1/` |

### 11.1 RD1 で判定していない事項

- gate 未設定のため、候補の優劣・順位付け・採用判断は行っていない
- blind シートと空の採点フォームは作成済みだが、人手採点は未実施
- 実写素材（質感を持つ実画像）での評価は素材未取得のため未実施
- GPU 実装、実アプリ経路、性能予算、Undo / 保存・再読込はいずれも未検証

---

## 関連資料

- `docs/technical/IMAGE_EFFECT_RND_EVALUATION_RULES.md`
- `docs/planned/MILESTONE_NEW_IMAGE_RND_IDEAS_2026-11-16.md`（候補 18）
- `experiments/DetailSharpeningLab/`、`experiments/WatercolorLab/`、`experiments/PencilPrintLab/`、`experiments/RetroImageLab/`（再利用資産・知見）
