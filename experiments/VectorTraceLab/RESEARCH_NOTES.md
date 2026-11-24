# VectorTraceLab Research Notes

**最終更新:** 2026-11-20

R&D 実行中の所見・失敗・仮説を記録する。

## 2026-11-20 — 起案

- 評価計画を `EVALUATION_PLAN.md` へ事前登録した（品質達成の根拠には使わない）。
- 重複監査の結果、直接の先行仕様 `MILESTONE_AUTO_TRACE_2026-06-02.md` と、
  実質的な tracer `MaskPath::fromAlphaMask()` が既存と判明。本ラボはその実装ではなく品質評価を担当する。

## 2026-11-20 — R0 / rd0

- 7 fixture（5 本の 512×320 + 256 / 1024 の解像度違い）を seed 20261120 で生成。
  再生成時の identity 差は全件 0.0。
- 制御 `alpha_threshold_trace`（既存 `fromAlphaMask` の geometry mirror）と
  baseline `posterize_components`（k-means + connected components + RETR_CCOMP + approxPolyDP）を計測。
- 発見した失敗（いずれも数値のみで視覚品質を断定しない）:
  1. **入れ子トポロジー**: `logo_badge` は ring の穴の中に star がある。RETR_CCOMP の 1 段の穴処理では
     star の island が失われ、blue 領域の IoU は 0.915 でも境界距離が 47 px に達した。
     GT を「ベクタ領域のカバレッジ」ではなく「前面が所有する可視領域」に直す必要もあり、修正後 IoU 0.819 / chamfer 16.5 px。
     rd1 は多段 hierarchy（RETR_TREE）または領域ラベルからの再構成を候補にする。
  2. **細線**: `thin_strokes` は recall 0.898 でゲート通過だが IoU 0.278。2 px 線が approxPolyDP eps 1.5 で潰れる。
     rd1 は eps の細線別スケール或いは線的形状の保持を検討。
  3. **半透明の色**: `alpha_logo` の glass (alpha 0.5) は k-means が合成後の色を返し ΔE 21.8。
     色面分解では alpha を分離して固有色を推定する必要がある（未検証の仮説）。
  4. **エッジ位相**: トレース結果を SS=4 で再ラスタライズすると元の AA エッジと 1 px ずれる。
     最大誤差は 200 code 超になるが、±1 px 許容 p95 は 35〜70 code。gate は後者を採用。
- ゲートを rd0 baseline から確定（`EVALUATION_PLAN.md` §5、`captures/rd0/gates.json`）。
  いずれの方式も全ゲートを通過していない。rd1 で候補比較を行う。
- 未実施: 人手視覚判定、実ロゴ fixture、Bezier 近似、形状／GPU 経路、製品コード変更、ビルド・テスト。
