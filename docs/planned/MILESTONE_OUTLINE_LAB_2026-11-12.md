# Adaptive Silhouette Outline R&D

**最終更新:** 2026-11-12
**ステータス:** Phase 1.5 実行済み（参照モデル、silhouette / alpha detail 分離、線幅実測、幅スイープ）

## 目的

Alpha、depth、normal、color を組み合わせ、レイヤーや 3D サーフェスの輪郭線を安定して生成する手法を検証する。既存の画像エッジ検出や単純な alpha stroke を置き換えるのではなく、輪郭のどこを線として残すべきか、どの経路で取得できるかを再現可能な実験で明らかにする。

対象は静止画と短い連続フレームの 2D 図形、文字、画像、3D / depth-normal fixture とする。最初の段階では ArtifactCore / Artifact 本体を変更せず、独立した Python 参照モデルで品質と限界を評価する。製品実装やエフェクトカタログ追加はこの R&D の完了条件に含めない。

## 現状の確認

- `Artifact/src/Effects/Rasterizer/EdgeEffect.cpp` の `EdgeCPUImpl` は、輝度ベースの 3x3 Sobel、または前後フレーム差分でエッジ強度を作る。出力 RGB はエッジ強度で、alpha は常に 1 に設定される。GPU 実装はない。
- `Artifact/src/Effects/Rasterizer/StrokeEffect.cpp` と `Artifact/include/Effects/Rasterizer/StrokeEffect.ixx` は、alpha 境界の内側／外側へ指定色のストロークを追加する。輪郭線の細部、内部稜線、depth 不連続、normal 変化を統合するモデルではない。
- `Artifact/src/Effects/Rasterizer/EdgeEffect.cpp` の `RimLightCPUImpl` は alpha 境界から方向性の inner rim を作る。輪郭線の検出ではなく、光の当たりを模した加算処理である。
- `ArtifactCore` には `ShapePath`、`GlyphAtlas`、Mesh / `generateRenderData()`、Render Queue の Depth / Normal / Velocity AOV がある。これらは輪郭情報の入力候補だが、本 R&D 開始時点では入力契約、座標系、深度 encoding、法線の符号、alpha の関連を製品 capture で確認していない。
- 既存の Edge / Stroke / Rim Light と Depth / Normal AOV の runtime 接続、GPU parity、知覚品質、時間安定性は未確認である。これらを確認せずに輪郭生成の品質を断定しない。
- `docs/technical/IMAGE_EFFECT_RND_EVALUATION_RULES.md` の評価計画を満たすまで、数値やサンプル画像を品質達成の根拠にしない。

## 参照モデル

次の順に構成要素を分離する。各段の出力を保存し、段を飛ばした比較を残す。

```text
source RGBA
  -> alpha boundary matte
  -> depth discontinuity matte
  -> normal discontinuity matte
  -> optional color / material edge
  -> combined feature confidence
  -> thresholded or soft line matte
  -> variable-width line raster
  -> composite over source and background
```

初期モデルは 2D alpha と単純な depth map のみを対象とする。normal、color、時間安定化は Phase 2 以降へ回す。alpha は straight と premultiplied を別条件として扱い、透明画素の RGB を推測で補正しない。輪郭線の太さは画素数だけでなく、入力解像度と表示倍率を記録する。

## 評価素材

### A. 制御 2D 図形

- 重なる円、矩形、星、曲線、細い線、角、尖部
- 半透明、完全透明、alpha=0 の RGB、straight / premultiplied
- アンチエイリアス境界、1px 以下の細部
- 透明背景を黒・灰・白へ合成した結果

### B. 3D / depth-normal

- 立方体、球、シリンダー、平面の組み合わせ
- 手前と奥の重なり、稜線、曲面、法線急変部
- Depth / Normal AOV の有無、深度 encoding、法線の座標系を記録
- カメラ距離・投影・アンチエイリアスを変えた条件

### C. 実画像・連続フレーム

- 髪、植物、布、文字、建物など、細線・重なり・テクスチャを含む素材
- 数フレームの移動・回転・occlusion
- 圧縮ノイズ、Depth / Normal が使える場合はその値
- 外部素材は出典、ライセンス、バージョン、SHA-256 を記録する

## 評価項目

| 項目 | 確認内容 |
|---|---|
| 効果の正しさ | alpha 境界、depth 不連続、normal 変化のそれぞれが意図した線として出るか。中立・無効設定で source が保たれるか |
| 画質と細部 | 1px / 2px / 4px の線幅、角、細線、重なり、アンチエイリアス境界での途切れ・過剰線・段差 |
| 色と alpha | 透明背景、半透明、straight / premultiplied、HDR、黒・灰・白背景への合成 |
| 時間安定性 | 連続フレームでの flicker、shimmer、線の太さ・位置の揺れ、occlusion の破綻 |
| 経路と解像度 | Python 参照モデル、Artifact CPU、GPU、Depth / Normal AOV の差。preview / final / Render Queue を区別 |
| 性能 | 画像サイズ、線幅、フレーム数に対する処理時間とメモリ。ホットパスの水性確保を前提にしない |

数値指標は、輪郭位置誤差、線幅分布、内側・外側の不要線率、alpha 保持、フレーム間差、領域別の RGB / luma 差分、必要に応じて SSIM / PSNR を使う。画像全体だけでなく、細線、角、透明縁、重なり、深度境界の crop を残す。数値の改善だけで視覚品質や製品適合を断定しない。

## 合格条件

- 指定した alpha / depth / normal 条件で、意図した境界が安定して検出される。
- 細線、角、重なり、透明縁で、極端な途切れ・過剰線・破綻がない。
- straight / premultiplied と複数背景への合成で、縁の色・alpha が破綻しない。
- 連続フレームで、輪郭位置と太さが大きく揺れない。揺れる場合は条件と原因を記録する。
- 既存 Edge / Stroke / Rim Light との役割差、参照モデルの限界、未検証経路を明示できる。
- 画像全体・等倍・拡大 crop の目視確認と、再現に必要な入力・設定・環境を保存できる。

## フェーズ

### Phase 0 — 資産と入力契約の棚卸し

Edge / Stroke / Rim Light、ShapePath、GlyphAtlas、Mesh、Depth / Normal AOV、Render Queue、Selection / Mask の出力契約を読み、輪郭入力として使える情報を表にまとめる。座標系、深度 encoding、法線の符号・空間、alpha association、色空間を未確認として記録する。

### Phase 1 — 静的 fixture と参照モデル

`experiments/OutlineLab/` に 2D alpha と単純 depth map の生成 fixture、参照モデル、比較スクリプトを置く。まず alpha boundary のみ、次に depth discontinuity、最後に単純な合成を実装する。Artifact 本体、Core、GPU は変更しない。

2026-11-12 時点の進捗: fixture 生成、参照モデル、runner、比較スクリプトを実行し、`experiments/OutlineLab/RESEARCH_NOTES.md` に結果と限界を記録した。線幅実測（`outline_metrics.py`）を追加し、alpha 検出器を silhouette（alpha 0.5 交差）と alpha detail（0.5 を跨がない変化）へ分離した。既定は silhouette のみで、detail は比較バリアントとして常に併記する。幅スイープ（`sweep_line_width.py`）で要求幅の量子化を確認し、要求2pxに対して実測6.50px（alpha）/ 5.75px（depth）、detail 併用時の不要線率 0.239 / silhouette のみ 0.000 を確認した。校正モデル（`calibrate_width.py`）は `width_50 ~= slope * effective_radius + intercept` で、スイープ行の残差 最大0.08px、hold-out 最大0.52px まで検証した。逆引き（`resolve_line_width.py`）では実測幅の下限が約4.25px、段差が約2.0〜2.65px で、1〜2px の輪郭はこの構成では表現できないことを確認した。normal / color、時間安定性、Artifact 接続は未実施である。

### Phase 2 — 品質・時間評価

細線・角・透明縁・重なり・複数背景・連続フレームを追加し、線幅、位置誤差、不要線、alpha、時間差を測定する。必要に応じて normal / color 条件を追加するが、初期の合否を混ぜない。

### Phase 3 — 製品接続の検討

参照モデルで再現性と限界を確認した後、Core / Artifact の既存 effect 契約へ接続できるかを別途判断する。この段階なしに製品実装や新エフェクト追加を完了と表現しない。

## 非対象

- ArtifactCore / Artifact / ArtifactWidgets の変更
- 新エフェクト、シェーダー、Render Graph、UI、ショートカット、プロジェクト形式の追加
- `QImage` / `QPainter` による新規合成、`QColorDialog`、QtCSS、新規 signal / slot
- GPU / Diligent の render path 変更、Render Queue の実装変更
- 動画デコード、AI モデル、フレーム補間
- ビルド、CMake、テスト実行。実行はユーザーの明示指示を待つ

## 成果物

- `docs/planned/MILESTONE_OUTLINE_LAB_2026-11-12.md`（本書）
- `experiments/OutlineLab/README.md`
- `experiments/OutlineLab/RESEARCH_NOTES.md`
- `experiments/OutlineLab/fixture_manifest.json`
- `experiments/OutlineLab/generate_fixtures.py`
- `experiments/OutlineLab/outline_reference.py`
- `experiments/OutlineLab/outline_metrics.py`
- `experiments/OutlineLab/sweep_line_width.py`
- `experiments/OutlineLab/calibrate_width.py`
- `experiments/OutlineLab/resolve_line_width.py`
- `experiments/OutlineLab/compare_outline.py`
- `experiments/OutlineLab/run_outline_experiment.py`
- `experiments/OutlineLab/requirements.txt`
- 生成 fixture、capture、report は `experiments/OutlineLab/assets/` と `captures/` に保存する。

## 完了条件

Phase 1 の参照モデルと評価結果、既知の限界、Phase 2 の未実施項目、製品接続の判断根拠を残す。数値・画像・実行条件・環境を第三者が再現できる形で記録する。未検証の経路や未達の項目がある場合は、R&D 完了としても製品実装完了とは表現しない。