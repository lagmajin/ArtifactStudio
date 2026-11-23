# AI 深度マップ R&D マイルストン (2026-11-16)

**最終更新:** 2026-11-16

## 目的

静止画・動画から単眼深度マップを作る経路を、既知深度の合成シーンで評価し、下流用途（DoF の焦点面、視差変位、フォグ、フラッシュ距離減衰、深度メッシュ）へ渡せる品質かを切り分ける。本マイルストンは **R&D の評価基盤と初期結果** までであり、製品実装の承認ではない。

## 成果物

| パス | 内容 |
|---|---|
| `experiments/DepthMapLab/EVALUATION_PLAN.md` | 事前登録済みの画像評価計画 |
| `experiments/DepthMapLab/MODELS.md` | モデルの出所・固定 hash・前処理 preset |
| `experiments/DepthMapLab/depth_map_lab.py` | fixture 生成、baseline、ONNX 推論、指標、ゲート、動画時間評価 |
| `experiments/DepthMapLab/README.md` | 実行方法、rd0 / rd1 の結果、制約 |
| `experiments/DepthMapLab/captures/rd0`, `rd1` | 測定 JSON、ゲート、contact sheet、マップ |

## 段階

| 段階 | 内容 | 状態 |
|---|---|---|
| rd0 | 合成 4 fixture と古典 baseline、ゲート確定 | 完了 |
| rd1 | Depth Anything V2 Small int8 の比較、動画 1 本の時間安定性、実写プレビュー | 完了 |
| rd2 | 正規化 4 種 × 後処理 3 種の比較、動画の時間指標 | 完了 |
| rd3 | 2 本目のモデル比較、採用 / 不採用、後続実装の受入条件 | 未着手 |

## 初期結果の要約

- 古典 baseline は事前登録ゲートを全項目で落ちる。輝度 prior は GT と逆相関（Spearman −0.785）、上下 prior は順序 0.682 でも輪郭 F1 0。
- Depth Anything V2 Small int8 は順序 0.952、輪郭 F1 0.830、前景欠落 0.2%。δ1 0.770 と動画 flow warp 後 0.0533 の 2 項目が未達。
- δ1 の未達は、近景が画面の一部しか占めない `scene_prior_trap`（δ1 0.459）に集中する。順序は 0.947 で正しく、percentile 正規化と近景の占有率の問題として切り分けた。
- 遮蔽境界（`scene_occlusion`）は輪郭位置が合っても境界帯 MAE 0.183 で、手前物体の縁に深度のにじみが出る。
- 動画の静止領域は 0.0061 と安定し、残差は移動輪郭に集中する。フレーム単位 / クリップ共通正規化の差は小さく、正規化は主因ではない。

### rd2 の追加結果

- δ1 の未達は percentile の切り詰めが原因だった。`p1_99` で `scene_prior_trap` の δ1 が 0.459 → 0.813、平均 0.770 → 0.859 となり、静止画の全ゲートを通る。`p5_95` は 0.757 へ悪化する。
- `median_iqr` は時間安定性が最良（flow warp 0.0446）だが、SI-RMSE 0.359 / δ1 0.644 / 境界帯 0.156 で静止画のゲートを落ちる。percentile の外れ値除去が実質的な役割を持つ。
- guided filter は境界帯 MAE を 0.112 → 0.111 とわずかに下げるだけなのに、輪郭 F1 を 0.833 → 0.737 へ下げる。遮蔽境界の改善仮説は棄却。一方で時間差分は 0.0521 → 0.0486 とゲート内に入る。静止画の既定には採らない。
- 採用候補は `p1_99` / 後処理なし。rd3 の比較契約もこれに固定する。

## 受入条件（rd3 までに満たすこと）

- 合成シーンの全必須指標が測定済みで、未達項目が明示されている
- 動画でカット・露出変化を含む区間の時間安定性が記録されている
- 少なくとも 2 本のモデルを同一 preset 契約・同一ゲートで比較している
- 近景占有率が小さい構図での δ1 改善案が、数値と境界 crop の両方で確認されている
- 製品統合の提案をする場合、`ArtifactCore` の `DepthMap`（near 値の解釈）と `DepthMeshGenerator` の契約、GPU 経路の未検証範囲を明記する

## 境界

- 製品コード（Artifact / ArtifactCore / ArtifactWidgets / libs）は変更しない
- GPU、Composition Viewer、Render Queue、DCC 連携は対象外
- メートル深度の絶対精度、実写の真値は対象外
- モデル重みはリポジトリへ含めず、固定 hash と取得手順のみ記録する

## 関連

- [`docs/technical/IMAGE_EFFECT_RND_EVALUATION_RULES.md`](../technical/IMAGE_EFFECT_RND_EVALUATION_RULES.md)
- [`experiments/DepthMapLab/README.md`](../../experiments/DepthMapLab/README.md)
- `ArtifactCore/include/Image/DepthMap.ixx`
- `ArtifactCore/include/Geometry/DepthMeshGenerator.ixx`
