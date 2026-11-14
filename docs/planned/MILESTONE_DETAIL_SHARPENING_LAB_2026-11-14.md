# Detail Sharpening R&D Lab

**最終更新:** 2026-11-14
**ステータス:** rd1 / rd2 / rd3b 実行済み（luma ガード候補がソフトエッジと CC0 実写 2 枚で事前登録ゲートを通過。適応しきい値は k=3 で部分達成）

## 目的

ディテール強調（unsharp mask / local contrast）を、輪郭の先鋭化と副作用（ハロー、オーバーシュート、色フリンジ、ノイズ増幅、ハイライトクリップ、透明縁、時間ちらつき）のトレードオフとして再現可能に評価する。既存の `Sharpen` エフェクトを置き換えることや、既定値を決めることは目的にしない。R&D の完了は製品実装の承認を意味しない。

評価は `docs/technical/IMAGE_EFFECT_RND_EVALUATION_RULES.md` に従い、実行前に [`experiments/DetailSharpeningLab/EVALUATION_PLAN.md`](../../experiments/DetailSharpeningLab/EVALUATION_PLAN.md) へ計画と閾値を登録した。rd2 の追加素材・追加モデルも実行前に同書 §9 へ記録した。

## 現状の確認（開始時点）

- `Artifact/src/Effects/Sharpen/SharpenEffect.cpp` の CPU 実装は、RGB 3ch を split して `GaussianBlur`、`color + (color - blurred) * amount` を **チャンネルごと** に適用する。
- `Threshold > 0` は `abs(color - blurred) * amount > threshold` のハード二値マスクで原画へ戻す。ソフトニーもノイズ相対化も無い。
- 負値は 0 クランプ。CPU に上限クランプは無く、GPU HLSL は `saturate` する。
- alpha は変更しないが、RGB ディテールは alpha 加重されない。premultiplied 契約は未確認。
- ハロー制御、ハイライト保護、色差を避ける luma 処理、時間安定化は無い。
- 同ファイルの `MagicSharpEffect` は多重スケール係数と edge/shadow protection を持つが、本 R&D の比較対象外。
- 製品キャプチャ、GPU パリティ、Viewer 経路、実写での品質は未確認。

## 評価計画の要点

- 目的: 先鋭化と副作用の分離。対象は 2D 画像のディテール強調。対象外は 3D、時間補間、AI 超解像。
- 比較: 入力 fixture を参照、`product_usm_mirror` を制御、`luma_usm_soft` / `halo_limited` / `alpha_weighted_limited` / `multiscale_limited` / `detail_capped` を候補とする。いずれも製品キャプチャではない。
- 素材: 合成 chart（ハードステップ、1px 細線、色エッジ、ノイズ 2 水準、ランプ、0.985 ハイライト、文字風バー）、straight-alpha 縁、8 フレーム移動グラティング、rd2 の σ=1.2px ソフトエッジ。seed `20261114`。
- 条件: scene-linear float32、Rec.709 luma、960×540（解像度ケース 1920、時間ケース 480×270）。review PNG は Reinhard+sRGB で測定には使わない。
- 合否: 事前登録した halo ≤1%、overshoot ≤0.004 linear、rise 改善 ≥15%、fine energy ≥1.10、新規クリップ ≤0.1%（ハイライト ROI ≤1%）、chroma ≤ミラーの 50%、noise std 比 ≤1.25、alpha 最大差 0、合成誤差 ≤2 code、時間 std 比 ≤1.50、解像度ゲイン差 ±25%。

## フェーズ

### Phase 1 — rd1（ハードエッジ基線）: 完了

- `experiments/DetailSharpeningLab/detail_sharpen_lab.py` を実装。fixture 生成、4 モデル、指標、alpha 合成、パラメータ掃引、接触画像、manifest を出力。
- 結果: 1px ハードエッジでは、極値クランプはハローを 0 にするが rise 改善も 0.2% まで消える。USM は rise 29〜31% と引き換えに halo 1.2〜3.7%、overshoot 0.061、色フリンジ 6.28 code、新規クリップ 0.28%、ハイライト 4.1〜4.4% を出す。
- 合格候補なし。原因は入力ヘッドルーム不足と alpha 未分離。

### Phase 2 — rd2（ソフトエッジ / alpha 分離）: 完了

- `followup_soft_edge.py` を追加。σ=1.2px のソフトエッジ、threshold 0 アブレーション、amount 掃引、alpha 加重、時間評価を実行。
- 結果: `halo_limited_t0` は USM と同じ rise 改善 15.8% を halo 0、overshoot 0 で達成し、事前登録ゲートを通過。amount 2.0 では 23.9% 対 27.7%。alpha 加重は黒背景合成誤差を 26.06 → 10.68 code に下げ、クランプ併用で 0.00 code。
- 絶対しきい値 0.02 はソフトエッジ応答を完全に停止させることを確認（rise gain 0.0）。
- 時間は high-pass std 比 1.196 だがフレーム差分比 1.583。クランプの時間一様性は未解決。

### Phase 3 — rd3b（適応しきい値 / ロールオフ / luma ガード / 実写）: 完了

- 初回 rd3 で、事前登録したグローバル median ノイズ推定が平坦部主体のプレートで 0 に潰れることを確認（`captures/rd3/manifest_first_attempt.json` に保持）。評価計画 §11 に修正理由を実行前登録し、16×16 ブロック MAD の 10 パーセンタイルによるノイズ床へ変更した。
- 全画素ノイズ σ=0.004 の `chart_960_noisy` を追加。`noise_rel_k1/k2/k3` の平坦ノイズ比は 2.018 / 1.684 / 1.512 で、k=3 でもゲート 1.15 に届かない。
- ハイライトのロールオフはクリーン chart の新規クリップを 4.41% → 0% にし、ソフトエッジのゲイン（15.8%）を落とさない。
- per-channel クランプは実写で luma 極値超過を 3.40%（neon）/ 0.79%（street）残す。luma デルタをクランプする `luma_guard_combo` は両プレートで 0%、新規クリップ 0%、chroma 0.07 / 0.03。
- 同じ amount のミラーに対する detail ROI の高域エネルギーは 1.58× vs 3.07×（neon）、1.34× vs 2.67×（street）。ハロー除去とディテール量のトレードオフが残課題。
- クランプ半径 1〜12px ではソフトエッジのゲインが変わらず、半径は調整軸にならない。

### Phase 4 — 実写の拡張・製品経路（未実施）

rd4 候補は `experiments/DetailSharpeningLab/RESEARCH_NOTES.md` §7 に記録した。k の掃引、ゲイン配分、エッジ適応ミックス、時間一様性、premultiplied 契約、製品 CPU/GPU キャプチャを含む。ビルド・テスト・GPU 実行はユーザーの明示指示を待つ。

## 成果物

- `docs/planned/MILESTONE_DETAIL_SHARPENING_LAB_2026-11-14.md`（本書）
- `experiments/DetailSharpeningLab/EVALUATION_PLAN.md`
- `experiments/DetailSharpeningLab/README.md`
- `experiments/DetailSharpeningLab/RESEARCH_NOTES.md`
- `experiments/DetailSharpeningLab/detail_sharpen_lab.py`
- `experiments/DetailSharpeningLab/followup_soft_edge.py`
- `experiments/DetailSharpeningLab/rd3_adaptive.py`
- `experiments/DetailSharpeningLab/requirements.txt`
- `experiments/DetailSharpeningLab/captures/rd1/`（manifest + sha256、npz、preview、contact、roi-crops、diff）
- `experiments/DetailSharpeningLab/captures/rd2/`（同上）
- `experiments/DetailSharpeningLab/captures/rd3/`（manifest、初回失敗 manifest、指標、preview、ROI crop、CC0 素材の来歴）

## 非対象

- ArtifactCore / Artifact / ArtifactWidgets の変更
- 新エフェクト、シェーダー、UI、ショートカット、プロジェクト形式の追加
- GPU / Diligent / Render Queue の変更
- ビルド、CMake、テスト実行（ユーザーの明示指示を待つ）
- 製品既定値・プリセットの決定

## 完了条件

- rd1 / rd2 / rd3b の全指標、ゲート判定、未検証項目、既知の欠陥、再現手順が manifest と研究ノートに残っていること。
- 未検証経路（GPU、Viewer、Render Queue、実写、HDR、premultiplied、アロケーション、独立レビュー）を明示していること。
- 数値・画像・実行条件・環境が第三者に再現できること。
- 画質達成や製品統合完了と表現しないこと。
