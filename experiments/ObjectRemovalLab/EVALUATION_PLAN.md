# Object Removal Lab 評価計画

**最終更新:** 2026-11-16
**状態:** rd0 fixture 計画を事前登録。品質 gate は未確定。

## 1. 目的と範囲

既存の Wire / Object Remover の静止画除去方式が、異なる構造の背後をどの程度復元できるか測る。対象は平坦背景、周期模様、細線・エッジ近傍、決定論的な自然風テクスチャ上の小さな物体・線状物体。動画伝播、製品 UI、AIモデル、製品既定値変更は対象外。

## 2. 比較対象

- 入力画像: 合成した物体を含む画像。
- 正解: 物体合成前の同じ背景。
- 評価マスク: 合成対象の既知領域。誤差を対象内部と境界帯に分ける。
- 現行 baseline: 除去を適用しない入力画像。隠れた画素の誤差が大きくなることを sanity check する。
- 候補: 現行 C++ と対応する概念上の SideAverage / CloneOffset / Blur / Telea / NS。Python mirror は製品結果と区別する。

## 3. 素材と条件

fixture generator は seed `20261116`、512×320 px、linear RGB float32 で少なくとも4ケースを生成する: (a) なだらかな水平/垂直色勾配上の小矩形、(b) 交差する細線の近傍の円形マーク、(c) 高周波周期格子上の塗りつぶし矩形、(d) 滑らかな deterministic noise 上の斜線。各ケースに clean plate、corrupted input、binary mask を保存し、SHA-256 を manifest に記録する。PNG は8-bit sRGB の目視用で、数値比較用の float 配列を別に保持する。

実写素材は rd0 に含めない。実写を追加する rd1 では取得元・ライセンス・hash を記録し、同じ幾何 mask と匿名化条件を保存する。Alpha は本段階では全面不透明に固定し、HDR / alpha 挙動は C++ 経路の契約を調べる独立段階に回す。

## 4. 指標と gate

rd0 は素材・マスク・baseline の sanity check のみを行い、candidate ranking や合否を出さない。rd0 後、人手の画像レビューと誤差分布を確認してから、候補比較前に gate を固定する。

記録する候補指標: マスク内 RGB MAE / RMSE / p95 / max、境界帯 MAE、マスク外の最大差（変更漏れ）、誤差画像。境界帯はマスクを3 px膨張した領域から元maskを除いたものと定義する。これらは全て同一 linear RGB 値域で計算し、平均値だけで品質を判断しない。構造を跨ぐ誤再構成を目視で確認する。

目視レビューは同一表示変換・同倍率で clean/input/result と full/crop を見る。方式名は隠して比較し、テクスチャ連続性、線の接続、境界のぼけ・複製模様、色ずれを記録する。判定者、環境、日付は結果に残す。現段階で人手レビュー・閾値は未実施／未確定。

## 5. 必須項目の状態

| 項目 | 状態 |
|---|---|
| 効果の正しさ | rd0 fixture のマスク領域と既知正解で測定準備。実装出力未取得 |
| 画質と細部 | 全体・crop の比較を予定。人手レビュー未実施 |
| 色と alpha | linear RGB fixture。alpha/HDR は明示的に対象外（製品契約としては未検証） |
| 時間安定性 | 静止画のみのため本段階は対象外。動画伝播は別段階 |
| 経路と解像度 | 512×320 synthetic CPU reference fixture。製品 preview/final、GPU parity、解像度依存は未検証 |
| 性能 | rd0 では判定しない。製品性能予算・計測条件は未設定 |

## 6. 段階

- rd0: deterministic fixtures、hash、clean/input/mask sanity manifest。
- rd1: 候補方式の同一 fixture 比較。gate を先に固定し、Python reference と C++ capture の証拠を分離。
- rd2: 利用条件を記録した実写・複雑な構造への拡張と独立目視レビュー。
- rd3: 必要なら製品 preview/final、GPU/CPU、性能と運用上の採否を検討。ビルド／runtime 検証は別途明示指示後に行う。

\n