# Outline Lab Research Notes

**最終更新:** 2026-11-12

## 記録方針

このファイルには、確認できた事実、参照モデルの仮説、未検証事項を分けて記録します。数値やサンプル画像を製品品質の根拠にせず、ArtifactStudio本体の変更理由にもしません。

## 初期の問い

1. Alpha境界、depth不連続、normal変化、color edgeは、それぞれ独立した輪郭情報を与えるか。
2. 単純なalpha strokeと、輪郭の位置・太さを制御するモデルはどこで分かれるか。
3. 透明縁、半透明、straight / premultipliedで、線の色とalphaは破綻しないか。
4. 細線、角、重なり、アンチエイリアス境界で、線は途切れず、不要な線が増えないか。
5. 連続フレームで、線の位置と太さは安定するか。
6. 既存のEdgeEffect、StrokeEffect、RimLightEffect、Depth / Normal AOVとの違いは何か。

## Phase 0: 既存資産の棚卸し

### 確認したコード上の事実

- `Artifact/src/Effects/Rasterizer/EdgeEffect.cpp`の`EdgeCPUImpl`は、輝度ベースの3x3 Sobel、または前後フレーム差分でエッジ強度を作る。出力RGBはエッジ強度で、alphaは常に1に設定される。GPU実装はない。
- `Artifact/src/Effects/Rasterizer/StrokeEffect.cpp`と`Artifact/include/Effects/Rasterizer/StrokeEffect.ixx`は、alpha境界の内側／外側へ指定色のストロークを追加する。depthやnormalを統合する輪郭モデルではない。
- `Artifact/src/Effects/Rasterizer/EdgeEffect.cpp`の`RimLightCPUImpl`は、alpha境界から方向性のinner rimを作る。輪郭線の検出ではなく、光の当たりを模した加算処理である。
- `ArtifactCore`には`ShapePath`、`GlyphAtlas`、Meshの`generateRenderData()`、Render QueueのDepth / Normal / Velocity AOVがある。これらは輪郭情報の入力候補だが、座標系、深度encoding、法線の符号、alpha association、色空間を製品captureで確認していない。

### 未確認事項

- Edge / Stroke / Rim Lightのruntime接続とGPU parity
- Depth / Normal AOVの実際のchannel meaning、深度encoding、法線空間
- 画像レイヤー、テキスト、3Dレイヤー、パーティクル、液体 / 煙の輪郭情報をどの経路で取得できるか
- 輪郭線の表示解像度、レイヤー変換、mask、blend modeとの合成順序
- 透明縁、半透明、premultiplied入力での出力alpha契約
- 時間方向のちらつき、occlusion、フレーム間安定性

## Phase 1: 参照モデル

### 入力

- RGBA float32、straight alphaとpremultiplied alphaを別条件にする
- 単純depth mapは、値を正規化せず、入力の単位と範囲を記録する
- 画像条件、seed、線幅、解像度、color space、背景色をmanifestへ記録する

### 実装上の決定（2026-11-12・コード上の事実）

- `alpha_boundary`はalphaチャンネルのみから境界confidenceを作り、RGBは参照しない。straight / premultipliedの違いがalpha契約に混入しないようにするため。
- `depth_discontinuity`の`depth_threshold`は「depth単位 / pixel」の絶対勾配しきい値。画像ごとのpercentile正規化は、全面を覆う緩やかなランプを全面輪郭へ増幅するため採用しない。
- `soft_line`は`width / 2`をコア半径とし、`softness`は外側フリンジの不透明度だけを変える。画面全体のマットを持ち上げないため、背景は常に0。
- `_shift`は端で複製パディングする。これにより画像端に偽の勾配を作らない。
- `line_width_measured`は現状`false`。Phase 1のrunnerはcoverageとline値のみを記録し、幅の実測はまだ主張しない。

### 処理

```text
source RGBA
  -> alpha boundary matte
  -> depth discontinuity matte
  -> normal discontinuity matte（Phase 2以降）
  -> optional color / material edge（Phase 2以降）
  -> combined feature confidence
  -> thresholded or soft line matte
  -> variable-width line raster
  -> composite over source and background
```

### 初期の測定

- 輪郭位置誤差: 既知の図形・depth段差に対する境界位置
- 線幅分布: 指定幅に対する実測の50% / 90% width
- 不要線率: 平坦領域・意図しない内部境界の検出率
- alpha保持: 入力alphaと出力alphaの差
- フレーム間差: 線位置・太さ・RGBの時間変化
- 領域別RGB / luma差分: 細線、角、透明縁、重なり、平坦部

## Phase 1: 実行結果（2026-11-12）

環境: Python 3.14 / numpy 2.4.3 / Pillow 12.2.0。生成物は`assets/generated/`、レビュー画像と測定JSONは`captures/outline-rd1/`に保存。`width=2`、`softness=0.75`、`include_depth=true`、seed 20261112。

### 再現手順

```powershell
python generate_fixtures.py
python run_outline_experiment.py --mode alpha-depth --width 2 --output captures/outline-rd1
python compare_outline.py --report captures/outline-rd1/outline-report.json
```

### 確認できた事実

- fixtureは3件。`alpha-shapes`（straight / premultiplied）はalpha境界のみ、`depth-planes`はalpha全不透明でdepth不連続のみが線に寄与する。
- `alpha-shapes`のstraightとpremultipliedは`line_sha256`が一致し、alphaのみから作る境界はalpha associationに依存しない。
- `alpha-shapes`のline coverageは0.082、`depth-planes`は0.033。depthのみのfixtureでも意図した矩形境界が検出された。
- `softness`を0→1にすると`line_mean`は0.0878→0.1225（alpha-shapes）へ増えるが、`line`の最小値は常に0で、背景は持ち上がらない。
- 0.15振幅の緩やかなdepthランプでは全面輪郭が出ず、絶対勾配しきい値が意図通り働いた。
- 幅1と2は同じcoverage（0.1127→修正後0.082）。`ceil(width/2)`による整数半径のため、1pxと2pxを区別できない。

### Phase 1時点の限界（Phase 1.5で一部解消）

- 線幅はPhase 1.5で実測を追加した（`outline_metrics.py`、`line_width_measured: true`）。
- alpha境界のみのため、alpha=1同士が重なる内部境界（例: 四角と円の重なり）は検出されない。normal / depth / colorの追加根拠になるが、Phase 1では未評価。
- 整数半径によりサブピクセル幅・非整数幅を表現できない。variable-width rasterは未実装。
- 時間安定性、normal、color、HDR、occlusion、Artifact CPU / GPU、preview / final、Render Queueとの差は未評価。
- 参照モデルは画像処理のみで、Artifactの色記述子・alpha規約・AOV encodingとの一致は未検証。

### 次の判断材料

- 幅の実測（距離変換ベース）を先に入れるか、normal / color入力のPhase 2を先に進めるか。
- 内部境界の欠落をdepth / normalで補えるかを、製品AOVの実データで確認する必要がある。

## Phase 1.5: 線幅実測と alpha gradient の比較（2026-11-12）

`outline_metrics.py`を追加し、truth境界の法線方向にlineをサンプリングして線幅を実測した。法線は境界リングの局所共分散から求め、距離変換の勾配は使わない（1pxリングでは距離場の勾配が0になるため）。幅は中心オフセットを含む連続区間だけを数え、近接する別の線を合算しない。

### 測定定義

- `width_50`: 局所ピークの50%以上が続く法線方向の長さ。`width_90`: 90%以上。
- `line_to_truth`: 検出線（line>=0.5）からtruth境界までの距離。`truth_to_line`: その逆方向。
- `unwanted_line_rate`: `width_px + 2`より遠い検出線画素の割合。
- truthはalpha fixtureでは`alpha>=0.5`の境界リング、depth fixtureでは矩形ジオメトリから作る。検出器の出力からtruthを作らない。

### 幅の実測値（p50 px、`width=2`、softness 0.75、silhouetteのみ）

| fixture | width_50 | width_90 |
|---|---|---|
| alpha-shapes | 6.50 | 3.75 |
| depth-planes | 5.75 | 3.75 |

要求幅2pxに対して実測は約3倍。原因は、検出応答が境界の両側に2〜3px広がること、max filterがさらに1px外側へ膨らむこと、幅の刻みが0.25pxであること。`width`1と2は同じ値になり、4では増える（整数半径`ceil(width/2)`の量子化）。**要求幅を実測幅として扱ってはならない。**

### silhouette と alpha detail の分離

Phase 1.5 の途中で、検出器を2チャンネルに分けた。

- `silhouette`: `alpha >= alpha_threshold` の2値交差のみ。被覆境界。
- `alpha_detail`: alpha勾配。silhouette近傍は`detail_suppression_px`で抑制する。

runnerは`primary`（`alpha_detail_weight`の設定値、既定0.0）と`detail_variant`（同1.0）を常に併記する。

| 指標（alpha-shapes, width=2） | silhouetteのみ | detail併用 |
|---|---|---|
| line coverage | 0.0512 | 0.0675 |
| width_50 p50 | 6.50 px | 6.50 px |
| line_to_truth p90 | 2.83 px | 60.08 px |
| unwanted_line_rate | 0.000 | 0.239 |

detail併用では、alphaが1.0→0.7→1.0のように変化するが0.5を跨がない半透明の重なりでも線を出す。その結果、円の内部に矩形状の不要線が現れ、truth境界から最大60px離れた画素が24%を占めた。silhouetteのみではこれが消え、alpha 0.5を跨がない細線（alpha 0.47）も検出しない。

これは「silhouette（被覆境界）」と「alphaの微細変化」を同じ検出器で扱えないことを示す。どちらを正とするかは用途次第であり、Phase 1では優劣を決めない。既定はsilhouetteのみとし、detailは明示的な比較バリアントとして残す。

> 注: 以前の`alpha_gradient_weight`（勾配と交差を加算する単一チャンネル）による数値（coverage 0.0820/0.0336、unwanted 0.201、line_to_truth p90 50.75px）は旧実装のもので、上の分離後の値に置き換わる。coverageの差の一部は、検出線の判定を`line > 0.5`から`line >= 0.5`へ直したことによる（`alpha`交差の応答がちょうど0.5になる軸平行境界が旧判定で落ちていた）。

### 幅スイープ（要求値 → 実測値）

`sweep_line_width.py`で`width`と`softness`を振り、実測幅の対応表を`captures/width-sweep/`へ保存した（alpha detail無効、seed 20261112）。

| fixture | 要求幅 | softness | width_50 | width_90 | line_to_truth p90 |
|---|---|---|---|---|---|
| alpha-shapes | 0.5 / 1 / 2 | 0.0 | 4.25 | 3.25 | 1.00 |
| alpha-shapes | 0.5 / 1 / 2 | 0.75 | 6.50 | 3.75 | 1.00 |
| alpha-shapes | 3 / 4 | 0.0 | 7.00 | 5.50 | 1.41 |
| alpha-shapes | 3 / 4 | 0.75 | 11.75 | 5.75 | 2.00 |
| alpha-shapes | 6 | 0.0 | 9.50 | 8.25 | 2.00 |
| alpha-shapes | 6 | 0.75 | 16.75 | 8.75 | 3.00 |
| depth-planes | 0.5 / 1 / 2 | 0.0 | 4.25 | 3.25 | 1.00 |
| depth-planes | 0.5 / 1 / 2 | 0.75 | 5.75 | 3.75 | 2.00 |
| depth-planes | 3 / 4 | 0.0 | 6.25 | 5.25 | 2.00 |
| depth-planes | 3 / 4 | 0.75 | 9.75 | 5.75 | 3.00 |
| depth-planes | 6 | 0.0 | 8.25 | 7.25 | 2.00 |
| depth-planes | 6 | 0.75 | 13.75 | 7.75 | 4.00 |

確認できたこと:

- `width`は`ceil(width / 2)`のコア半径へ量子化されるため、要求0.5 / 1 / 2は同一の実測になる。要求3 / 4も同一。要求値と実測値は比例しない。
- `softness=0`の実測はコア半径`r`に対してほぼ`2r + 2.25`で増える（r=1: 4.25、r=2: 6.25〜7.00、r=3: 8.25〜9.50）。これは観測された段階であり、一般式として確定したものではない。
- `softness=0.75`はフリンジが50%帯に入るため`width_50`を大きく増やす（要求6で9.50→16.75）が、`width_90`の増加は小さい（8.25→8.75）。`softness`は「見た目の太さ」より「にじみの裾」に効く。
- `unwanted_line_rate`の許容距離は`width_px + 2`。要求0.5ではフリンジが許容距離を超えるため0.106になる。これは不要線の増加ではなく、要求幅に連動する指標スケールの効果である。
- 幅の校正係数は`calibrate_width.py`で求めた（次節）。

### 幅の校正（2026-11-12 追加）

`calibrate_width.py`は、参照モデル自身が行う膨張を`effective_radius`として再計算し、残りの差を実測に対する線形モデルで表す。曲線の当てはめではなく、`soft_line`の構造を写した上で係数だけを最小二乗で求める。

```text
core_radius      = ceil(width_px / 2)
fringe_radius    = core_radius + max(1, ceil(core_radius * softness))
effective_radius = core_radius                 (softness < 0.5)
                 = fringe_radius               (softness >= 0.5)
width_50        ~= slope * effective_radius + intercept
```

| fixture | class | slope | intercept | 行数 |
|---|---|---|---|---|
| alpha-shapes | hard | 2.650 | 1.625 | 6 |
| alpha-shapes | soft | 2.575 | 1.375 | 6 |
| depth-planes | hard | 2.000 | 2.250 | 6 |
| depth-planes | soft | 2.000 | 1.750 | 6 |

検証結果:

- スイープ行（24件）の残差は最大0.08px、p90 0.07px。
- hold-out（スイープに含まれない`width` 8 / 1.5 / 5 / 0.75と`softness` 0 / 0.4 / 0.9 / 0.6、8件）の残差は最大0.52px。
- `depth-planes`はスロープが整数2.000で、`2 × effective_radius + 定数`にほぼ完全に一致する。`alpha-shapes`はスロープが2.5超で、境界のアンチエイリアス応答の分だけ実測が速く増える。
- `softness` 0.5付近はクラス境界が段差になる。hold-outの`softness` 0.4 / 0.6は±0.5pxの誤差を出した。
- 到達可能な実測幅は量子化され、スイープ範囲では 4.25 / 5.75 / 6.25 / 6.5 / 7.0 / 8.25 / 9.5 / 9.75 / 11.75 / 13.75 / 16.75 px の11種類しかない。要求値を連続に変えても実測は階段状にしか動かない。

この係数は参照モデルと合成fixture固有であり、ArtifactのラスタライザやGPU実装へそのまま流用してはならない。

### 目標幅の逆引き（`resolve_line_width.py`）

校正モデルを逆引きし、目標`width_50`（1 / 2 / 4 / 8px、許容±0.5px、`softness` 0 / 0.5 / 0.75 / 0.9）に対する要求幅を選び、その設定を実測して検証した。候補は校正で使用したeffective radius範囲に制限し、外挿値は除外している。

| 目標 | alpha hard | alpha soft 0.75 | depth hard | depth soft 0.75 |
|---|---|---|---|---|
| 1px | 4.25（不可） | 6.50（不可） | 4.25（不可） | 5.75（不可） |
| 2px | 4.25（不可） | 6.50（不可） | 4.25（不可） | 5.75（不可） |
| 4px | 4.25（達成） | 6.50（不可） | 4.25（達成） | 5.75（不可） |
| 8px | 7.00（不可） | 6.50（不可） | 8.25（達成） | 9.75（不可） |

- 実測幅の下限は約4.25pxで、要求0.25でも下がらない。検出応答の帯＋整数半径の膨張が下限を作っている。
- hardクラスの実測幅の段差は alpha で約2.65px、depth で約2.00px。段の間に落ちる目標は±0.5pxでは到達できない。
- `softness >= 0.5`は下限を5.75〜6.52pxへ上げる。
- `softness`ちょうど0.5は、フリンジ不透明度が50%帯の境界と一致して判定がぎりぎりになる。alpha で予測6.52 / 実測5.25、depth で予測5.75 / 実測5.25。この設定だけモデル誤差が1px超になるため、不安定条件として扱う。
- 到達可能な値（予測）は alpha hard で 4.27 / 6.92 / 9.57、depth hard で 4.25 / 6.25 / 8.25 など、数段しかない。

1px / 2pxの輪郭が必要な場合は、整数半径のmax filter膨張ではなく、距離場ベースの線ラスタライズなどサブピクセル幅を扱える構成が必要になる。本ラボでは未実装であり、Phase 2以降の設計課題として残す。

## Phase 2: 追加条件

- Normal AOV、color / material edge、HDR、複数光源
- 3D primitive、mesh、text、shape layerの重なり
- 連続フレーム、occlusion、カメラ移動
- Artifact CPU / GPU、Depth / Normal AOV、preview / final / Render Queueの差

## 現時点の結論

Phase 1では、alpha境界とdepth不連続を分離したfixtureで参照モデルが動作し、straight / premultipliedでalpha由来の線が一致することを確認した。Phase 1.5では、検出器をsilhouette（alpha 0.5交差）とalpha detail（0.5を跨がない変化）へ分離し、要求幅2pxに対して実測6.50px（alpha）/ 5.75px（depth）になること、alpha detailを混ぜると半透明の重なりに不要線を出すこと（unwanted 0.000 → 0.239）を確認した。幅は`ceil(width/2)`で量子化され、要求0.5 / 1 / 2は同一の実測になる。要求幅→実測幅の校正モデルはスイープ行で最大0.08px、hold-outで最大0.52pxの残差に収まった。逆引きでは実測幅の下限が約4.25px、段差が約2.0〜2.65pxであることを確認した。つまり1〜2pxの輪郭はこの構成では表現できない。

これは限定された事実であり、品質・性能・製品適合の判断ではない。校正係数は参照モデル固有で、用途ごとのsilhouette定義、normal / color、時間安定性、Artifact接続は未評価のままである。

## 次の作業

1. 細い線（1〜2px）を目標にする場合の構成を検討する。整数半径の膨張では下限4.25pxに張り付くため、距離場からの線ラスタライズなど別方式の比較が必要。
2. silhouette と alpha detail を別チャンネルとして扱う方針を決める（現状はsilhouette既定、detailは比較バリアント）。
3. 内部境界（重なり）をdepth / normalで補う条件をPhase 2で設計する。
4. ArtifactのDepth / Normal AOVの実データで、channel meaning・符号・encodingを確認する。
5. 連続フレームfixtureを追加し、時間安定性の指標を定義する。