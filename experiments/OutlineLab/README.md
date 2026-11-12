# Outline Lab

**最終更新:** 2026-11-12
**ステータス:** Phase 1.5 実行済み（参照モデル、silhouette / alpha detail分離、線幅実測、幅スイープ、幅校正、目標幅の逆引き）

ArtifactStudio本体から独立した、輪郭線生成の研究用Pythonラボです。Alpha、depth、normal、colorを組み合わせた輪郭抽出と、線の太さ・安定性・透明縁の扱いを比較します。ここで得た結果をCoreやArtifactへ反映するかは別途判断します。

## 目的

既存の単純な画像エッジ検出やalpha strokeを置き換えるのではなく、次の要素を分けて評価します。

- Alpha境界: レイヤーの外輪郭
- Depth不連続: 手前と奥の重なり
- Normal変化: 面の折れ、稜線
- Color / material edge: 内部の色や材質の変化
- 時間安定性: 連続フレームでのちらつき

最初の対象は2D alphaと単純なdepth mapです。3D normal、color edge、Artifact接続はPhase 2以降とします。

## セットアップ

Python 3.10以降を使います。このディレクトリで次を実行します。

```powershell
py -m venv .venv
.\.venv\Scripts\Activate.ps1
python -m pip install -r requirements.txt
```

## 実行

生成fixtureと参照モデルを実行します。生成物は`assets/generated/`、レビュー画像と測定JSONは`captures/`に保存されます。

```powershell
python generate_fixtures.py
python run_outline_experiment.py --width 2 --output captures/outline-rd1
python compare_outline.py --report captures/outline-rd1/outline-report.json
```

`--alpha-detail-weight`を既定の0.0から変えると、silhouetteにalpha detailチャンネルを混ぜた比較記録を別ディレクトリへ出力できます。runnerは`alpha_detail_weight`に依らず`detail_variant`（重み1.0）を常に併記するため、1回の実行でsilhouetteとdetailを比較できます。

```powershell
python run_outline_experiment.py --width 2 --alpha-detail-weight 1 --output captures/outline-rd1-detail
python sweep_line_width.py
python calibrate_width.py
python resolve_line_width.py
```

`--help`で各スクリプトの条件を確認します。画像条件、seed、alpha association、解像度、線幅を固定して比較します。

`sweep_line_width.py`は`width`と`softness`を変えたときの実測幅の表（`captures/width-sweep/width-sweep.md`と`.json`）を書き出します。`calibrate_width.py`はその表から要求幅→実測幅の校正係数を求め、スイープに含まれない幅（hold-out）で検証します。`resolve_line_width.py`は校正モデルを逆引きし、目標の実測幅に対して要求幅を選び、選んだ設定を実測して誤差を記録します（`captures/width-sweep/width-resolve.md`と`.json`）。幅の校正は、要求値を実測値と同一視せず、この対応表を経由して行います。

### 参照モデルのパラメータ

- `--width`: 線のコア半径は`width / 2`。外側フリンジは`softness`で別に広がる。
- `--softness`: 外側フリンジの不透明度。0でも背景は0のままで、画面全体を持ち上げない。
- `--alpha-detail-weight`: alphaの連続変化（0.5を跨がない半透明の変化）を線に混ぜる重み。既定0.0で、既定はsilhouette（alpha 0.5交差）のみ。silhouette近傍は`detail_suppression_px`で抑制し、同じ縁を二重に数えない。
- `depth_threshold`: depth勾配の絶対しきい値（fixtureの文書化された単位 / pixel）。画像ごとのpercentile正規化はせず、緩やかなランプを全面輪郭に増幅しない。
- `alpha_threshold`: alpha境界の2値化しきい値。silhouette confidenceはalphaのみから作り、RGBは使わない。

## ディレクトリ

```text
OutlineLab/
├── README.md
├── RESEARCH_NOTES.md
├── fixture_manifest.json
├── generate_fixtures.py
├── outline_reference.py
├── outline_metrics.py
├── sweep_line_width.py
├── calibrate_width.py
├── resolve_line_width.py
├── compare_outline.py
├── run_outline_experiment.py
├── requirements.txt
├── assets/
│   ├── sources/
│   └── generated/
├── captures/
└── reports/
```

`assets/sources/`には外部素材を置かず、出典・ライセンス・SHA-256を`fixture_manifest.json`へ記録します。外部素材を使う場合は別途出典と利用条件を記載します。

`assets/generated/`は`.gitignore`対象で、`generate_fixtures.py`で再生成します。古い`.depth.npy`は再生成時に自動削除されませんが、runnerは`index.json`の`has_depth`だけを見るため参照されません。`captures/`内の古い画像もrunnerは削除しないため、出力名を変えた場合は該当ディレクトリを削除してから再実行します。`captures/`はレビュー画像と測定JSONの記録として残します。

## 評価の考え方

数値指標だけで品質を判断しません。全体画像、等倍、拡大crop、黒・灰・白背景への合成を残し、次の領域を分けます。

- 細線・角・尖部
- 透明縁・半透明
- 重なり・depth境界
- 内部の色・材質エッジ
- 連続フレームの時間差

測定には、輪郭位置誤差、線幅分布、不要線率、alpha保持、フレーム間差、領域別RGB / luma差分を使います。指標の定義、値域、許容値、ツール、入力hashをJSONへ残します。

## 初期の合否

- 指定したalpha / depth条件で、意図した境界が検出される。
- 細線、角、重なり、透明縁で極端な途切れや過剰線がない。
- 透明背景、半透明、straight / premultiplied、複数背景で縁が破綻しない。
- 連続フレームで線位置と太さが大きく揺れない。
- 既存のEdge / Stroke / Rim Lightとの役割差を説明できる。
- 未検証の経路と限界を記録できる。

### 実測済みの指標と既知の値（`width=2`, `softness=0.75`, `alpha_detail_weight=0`）

| fixture | width_50 p50 | width_90 p50 | line_to_truth p90 | unwanted |
|---|---|---|---|---|
| alpha-shapes（silhouette） | 6.50 px | 3.75 px | 2.83 px | 0.000 |
| alpha-shapes（+detail 1.0） | 6.50 px | 3.75 px | 60.08 px | 0.239 |
| depth-planes（silhouette） | 5.75 px | 3.75 px | 3.00 px | 0.000 |

- 要求幅2pxに対して実測は約3倍。核心原因は検出応答が境界の両側に広がること、max filterが外側へ1px膨らむこと、整数半径`ceil(width/2)`による量子化。**要求幅を実測幅として扱わない。**
- `width_50` / `width_90` は境界法線方向の連続区間から測る。法線はリングの局所共分散、幅の刻みは0.25px。
- alpha detailを混ぜると、alpha 1.0→0.7→1.0のように0.5を跨がない半透明の重なりに不要線が出る（`unwanted_line_rate` 0.000 → 0.239）。既定で無効なのはこのため。
- 幅スイープ（`captures/width-sweep/width-sweep.md`）: `ceil(width/2)`で量子化されるため、要求0.5 / 1 / 2は同じ実測になる。要求値と実測値の対応は単純な比例ではなく、段階的に増える。
- `unwanted_line_rate`の許容距離は`width_px + 2`で、要求幅が小さいほど厳しくなる。要求幅0.5ではフリンジ自体が許容距離を超えて0.106になるため、小さい要求幅の値は不要線の増加ではなく指標のスケールとして読むこと。
- 幅の校正（`calibrate_width.py`）: `width_50 ≒ slope × effective_radius + intercept`。スイープ行の残差は最大0.08px、hold-outは最大0.52px。ただし係数は参照モデル専用で、他ラスタライザへ流用しない。

### 幅校正の係数（`captures/width-sweep/width-calibration.md`）

| fixture | softness class | slope | intercept |
|---|---|---|---|
| alpha-shapes | hard (< 0.5) | 2.650 | 1.625 |
| alpha-shapes | soft (>= 0.5) | 2.575 | 1.375 |
| depth-planes | hard | 2.000 | 2.250 |
| depth-planes | soft | 2.000 | 1.750 |

- depth-planesは整数スロープ2.000で、実測は`2 × effective_radius + 定数`にほぼ完全に一致する。
- alpha-shapesはスロープが2.5超で、境界のアンチエイリアス応答により実測がコア半径より速く増える。
- softness 0.5付近はクラス境界が段差になるため、0.4 / 0.6では±0.5pxの誤差が出る（hold-outで確認）。

### 目標幅の逆引き（`resolve_line_width.py`）

校正モデルを逆引きし、目標の`width_50`（1 / 2 / 4 / 8px、許容±0.5px）に対して要求幅を選び、実際に測って検証した。候補は校正済みのeffective radius範囲に限定し、外挿しない。

| 目標 | alpha hard | alpha soft(0.75) | depth hard | depth soft(0.75) |
|---|---|---|---|---|
| 1px | 4.25（不可） | 6.50（不可） | 4.25（不可） | 5.75（不可） |
| 2px | 4.25（不可） | 6.50（不可） | 4.25（不可） | 5.75（不可） |
| 4px | 4.25（達成） | 6.50（不可） | 4.25（達成） | 5.75（不可） |
| 8px | 7.00（不可） | 6.50（不可） | 8.25（達成） | 9.75（不可） |

確認できたこと:

- **実測幅の下限は約4.25px**。要求0.25でも下がらない。1px / 2pxの輪郭は、この検出器＋整数膨張では表現できない。
- hardクラスの実測幅の段は alpha で約2.65px、depth で約2.00px。目標が段の間に落ちると±0.5pxでは到達できない。
- `softness >= 0.5`にすると下限が5.75〜6.52pxへ上がり、細い線はさらに作れなくなる。
- `softness`ちょうど0.5は、フリンジ不透明度が50%帯の境界と一致するため判定がぎりぎりになる。alpha で予測6.52に対し実測5.25、depth で5.75に対し5.25となり、この設定だけモデル誤差が1px超になる。0.5は不安定な条件として扱う。
- 逆引きは単調な校正ではあるが、要求幅を連続に変えても実測は飛び飛びにしか動かない。UIの幅指定を要求値そのままで見せると、値と見た目が対応しない区間ができる。

したがって、細い輪郭線を要件にする場合は、整数半径のmax filter膨張ではなく、距離場や符号付き距離からの線ラスタライズなど、サブピクセル幅を扱える構成へ変える必要がある。これはPhase 2以降の設計課題であり、本ラボでは未実装である。

Artifact の Core、シェーダー、エフェクトカタログ、UI、プロジェクト形式は変更しません。ビルド・テスト・GPU実行はユーザーの明示指示を待ちます。

## 関連資料

- `docs/planned/MILESTONE_OUTLINE_LAB_2026-11-12.md`
- `docs/technical/IMAGE_EFFECT_RND_EVALUATION_RULES.md`
- `docs/analysis/IMAGE_PIPELINE_AUDIT_2026-08-02.md`
- `docs/analysis/ADVANCED_RENDERING_GAP_2026-08-13.md`
- `Artifact/src/Effects/Rasterizer/EdgeEffect.cpp`
- `Artifact/src/Effects/Rasterizer/StrokeEffect.cpp`
- `Artifact/include/Effects/Rasterizer/StrokeEffect.ixx`
- `ArtifactCore/include/Shape/ShapePath.ixx`
- `ArtifactCore/include/Text/GlyphAtlas.ixx`
- `ArtifactCore/src/Geometry/Procedural3DGenerators.cpp`